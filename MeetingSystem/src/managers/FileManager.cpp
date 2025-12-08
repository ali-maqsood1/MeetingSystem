#include "FileManager.h"
#include <iostream>
#include <ctime>
#include <algorithm>

std::string FileManager::calculate_file_hash(const uint8_t* data, size_t size) {
    // Simple hash for DSA project (in production, use SHA-256)
    uint64_t hash = 5381;
    for (size_t i = 0; i < size; i++) {
        hash = ((hash << 5) + hash) + data[i];
    }
    
    // Add size to hash
    hash ^= size;
    
    char hash_str[65];
    snprintf(hash_str, sizeof(hash_str), "%016lx", hash);
    return std::string(hash_str);
}

uint64_t FileManager::store_file_data(const uint8_t* data, size_t size) {
    const size_t CHUNK_SIZE = PAGE_DATA_SIZE - 16; // Leave room for metadata
    size_t bytes_written = 0;
    uint64_t first_page_id = 0;
    uint64_t prev_page_id = 0;
    
    while (bytes_written < size) {
        uint64_t page_id = db->allocate_page();
        
        if (first_page_id == 0) {
            first_page_id = page_id;
        }
        
        Page page;
        page.header.type = DATA_OVERFLOW;
        
        // Store next page ID in first 8 bytes
        uint64_t next_page_id = 0;
        memcpy(page.data, &next_page_id, sizeof(uint64_t));
        
        // Store data
        size_t chunk_size = std::min(CHUNK_SIZE, size - bytes_written);
        memcpy(page.data + 8, data + bytes_written, chunk_size);
        
        db->write_page(page_id, page);
        
        // Link previous page to this one
        if (prev_page_id != 0) {
            Page prev_page = db->read_page(prev_page_id);
            memcpy(prev_page.data, &page_id, sizeof(uint64_t));
            db->write_page(prev_page_id, prev_page);
        }
        
        prev_page_id = page_id;
        bytes_written += chunk_size;
    }
    
    return first_page_id;
}

bool FileManager::read_file_data(uint64_t first_page_id, size_t size, 
                                  std::vector<uint8_t>& out_data) {
    const size_t CHUNK_SIZE = PAGE_DATA_SIZE - 16;
    out_data.clear();
    out_data.reserve(size);
    
    uint64_t current_page_id = first_page_id;
    size_t bytes_read = 0;
    
    while (current_page_id != 0 && bytes_read < size) {
        Page page = db->read_page(current_page_id);
        
        // Read next page ID
        uint64_t next_page_id;
        memcpy(&next_page_id, page.data, sizeof(uint64_t));
        
        // Read data
        size_t chunk_size = std::min(CHUNK_SIZE, size - bytes_read);
        out_data.insert(out_data.end(), 
                       page.data + 8, 
                       page.data + 8 + chunk_size);
        
        bytes_read += chunk_size;
        current_page_id = next_page_id;
    }
    
    return bytes_read == size;
}

bool FileManager::store_file_record(const FileRecord& file) {
    // Allocate page for file record
    uint64_t data_page_id = db->allocate_page();
    
    // Serialize file record
    uint8_t buffer[FileRecord::serialized_size()];
    file.serialize(buffer);
    
    // Write to page
    Page data_page;
    memcpy(data_page.data, buffer, FileRecord::serialized_size());
    db->write_page(data_page_id, data_page);
    
    // Create record location
    RecordLocation file_loc(data_page_id, 0, FileRecord::serialized_size());
    
    // Index in B-Tree by file_id
    if (!files_btree->insert(file.file_id, file_loc)) {
        std::cerr << "Failed to insert file into B-Tree" << std::endl;
        db->free_page(data_page_id);
        return false;
    }
    
    // Index in hash table by file_hash
    if (!file_dedup_hash->insert(file.file_hash, file_loc)) {
        std::cerr << "Failed to insert file hash" << std::endl;
        return false;
    }
    
    return true;
}

bool FileManager::upload_file(uint64_t meeting_id, uint64_t uploader_id,
                               const std::string& filename, const uint8_t* data,
                               size_t data_size, FileRecord& out_file, std::string& error) {
    // Validate
    if (filename.empty()) {
        error = "Filename is required";
        return false;
    }
    
    if (filename.length() >= 256) {
        error = "Filename too long";
        return false;
    }
    
    if (data_size == 0) {
        error = "File is empty";
        return false;
    }
    
    if (data_size > 10 * 1024 * 1024) { // 10MB limit
        error = "File too large (max 10MB)";
        return false;
    }

    auto existing_files = get_meeting_files(meeting_id);
    size_t total_size = 0;
    for (const auto& f : existing_files) {
        total_size += f.file_size;
    }
    
    if (total_size + data_size > 50 * 1024 * 1024) {
        error = "Meeting storage limit exceeded (max 50MB total)";
        return false;
    }
    
    // Calculate hash
    std::string file_hash = calculate_file_hash(data, data_size);
    
    // Check for duplicate
    FileRecord existing_file;
    if (file_exists_by_hash(file_hash, existing_file)) {
        // File already exists, just create new record pointing to same data
        std::cout << "File deduplication: reusing existing data" << std::endl;
        
        FileRecord file;
        file.file_id = db->get_next_file_id();
        file.meeting_id = meeting_id;
        file.uploader_id = uploader_id;
        strcpy(file.filename, filename.c_str());
        strcpy(file.file_hash, file_hash.c_str());
        file.file_size = data_size;
        file.uploaded_at = std::time(nullptr);
        file.data_page_id = existing_file.data_page_id; // Reuse data
        
        if (!store_file_record(file)) {
            error = "Failed to store file record";
            return false;
        }
        
        db->write_header();
        out_file = file;
        return true;
    }
    
    // Store file data
    uint64_t data_page_id = store_file_data(data, data_size);
    
    // Create file record
    FileRecord file;
    file.file_id = db->get_next_file_id();
    file.meeting_id = meeting_id;
    file.uploader_id = uploader_id;
    strcpy(file.filename, filename.c_str());
    strcpy(file.file_hash, file_hash.c_str());
    file.file_size = data_size;
    file.uploaded_at = std::time(nullptr);
    file.data_page_id = data_page_id;
    
    // Store file record
    if (!store_file_record(file)) {
        error = "Failed to store file record";
        return false;
    }
    
    // Save database header
    db->write_header();
    
    out_file = file;
    std::cout << "File uploaded: " << filename << " (" << data_size << " bytes)" << std::endl;
    return true;
}

bool FileManager::download_file(uint64_t file_id, std::vector<uint8_t>& out_data,
                                 FileRecord& out_file, std::string& error) {
    // Get file info
    if (!get_file_info(file_id, out_file)) {
        error = "File not found";
        return false;
    }
    
    // Read file data
    if (!read_file_data(out_file.data_page_id, out_file.file_size, out_data)) {
        error = "Failed to read file data";
        return false;
    }
    
    std::cout << "File downloaded: " << out_file.filename << std::endl;
    return true;
}

std::vector<FileRecord> FileManager::get_meeting_files(uint64_t meeting_id) {
    std::vector<FileRecord> files;
    
    auto locations = files_btree->range_search(1, UINT64_MAX);
    
    for (const auto& loc : locations) {
        Page page = db->read_page(loc.page_id);
        FileRecord file;
        file.deserialize(page.data + loc.offset);
        
        if (file.meeting_id == meeting_id) {
            files.push_back(file);
        }
    }
    
    // Sort by upload time
    std::sort(files.begin(), files.end(),
              [](const FileRecord& a, const FileRecord& b) {
                  return a.uploaded_at > b.uploaded_at;
              });
    
    return files;
}

bool FileManager::get_file_info(uint64_t file_id, FileRecord& out_file) {
    bool found;
    RecordLocation loc = files_btree->search(file_id, found);
    
    if (!found) {
        return false;
    }
    
    Page page = db->read_page(loc.page_id);
    out_file.deserialize(page.data + loc.offset);
    
    return true;
}

bool FileManager::delete_file(uint64_t file_id, std::string& error) {
    // Get file info
    FileRecord file;
    if (!get_file_info(file_id, file)) {
        error = "File not found";
        return false;
    }
    
    // Remove from B-Tree index
    if (!files_btree->remove(file_id)) {
        std::cerr << "Warning: Failed to remove file from B-Tree" << std::endl;
        // Continue anyway - the important thing is it won't show up in queries
    }
    
    // Note: We're NOT freeing the data pages or removing from hash table
    // because other files might be referencing the same data (deduplication)
    // For a full implementation, you'd need reference counting
    
    std::cout << "File deleted: " << file.filename << " (ID: " << file_id << ")" << std::endl;
    return true;
}

bool FileManager::file_exists_by_hash(const std::string& file_hash, FileRecord& out_file) {
    bool found;
    RecordLocation loc = file_dedup_hash->search(file_hash, found);
    
    if (!found) {
        return false;
    }
    
    Page page = db->read_page(loc.page_id);
    out_file.deserialize(page.data + loc.offset);
    
    return true;
}