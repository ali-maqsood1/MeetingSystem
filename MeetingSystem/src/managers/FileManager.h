#ifndef FILE_MANAGER_H
#define FILE_MANAGER_H

#include "../storage/DatabaseEngine.h"
#include "../storage/BTree.h"
#include "../storage/HashTable.h"
#include "../models/File.h"
#include <string>
#include <vector>
#include <cstring>

class FileManager {
private:
    DatabaseEngine* db;
    BTree* files_btree;
    HashTable* file_dedup_hash;
    
public:
    FileManager(DatabaseEngine* database, BTree* files_tree, HashTable* dedup_hash)
        : db(database), files_btree(files_tree), file_dedup_hash(dedup_hash) {}
    
    // Upload file
    bool upload_file(uint64_t meeting_id, uint64_t uploader_id, 
                     const std::string& filename, const uint8_t* data, 
                     size_t data_size, FileRecord& out_file, std::string& error);
    
    // Download file
    bool download_file(uint64_t file_id, std::vector<uint8_t>& out_data, 
                       FileRecord& out_file, std::string& error);
    
    // Get files for meeting
    std::vector<FileRecord> get_meeting_files(uint64_t meeting_id);
    
    // Get file info
    bool get_file_info(uint64_t file_id, FileRecord& out_file);
    
    // Delete file
    bool delete_file(uint64_t file_id, std::string& error);
    
    // Check if file exists by hash (deduplication)
    bool file_exists_by_hash(const std::string& file_hash, FileRecord& out_file);
    
private:
    // Calculate SHA-256-like hash
    std::string calculate_file_hash(const uint8_t* data, size_t size);
    
    // Store file data across multiple pages
    uint64_t store_file_data(const uint8_t* data, size_t size);
    
    // Read file data from pages
    bool read_file_data(uint64_t first_page_id, size_t size, std::vector<uint8_t>& out_data);
    
    // Store file record
    bool store_file_record(const FileRecord& file);
};

#endif // FILE_MANAGER_H