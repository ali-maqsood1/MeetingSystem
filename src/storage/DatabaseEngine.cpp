#include "DatabaseEngine.h"
#include <iostream>
#include <cstring>

DatabaseEngine::DatabaseEngine(const std::string& filename) 
    : db_filename(filename) {
}

DatabaseEngine::~DatabaseEngine() {
    close();
}

bool DatabaseEngine::initialize() {
    // Create new database file
    db_file.open(db_filename, std::ios::binary | std::ios::out | std::ios::trunc);
    if (!db_file.is_open()) {
        std::cerr << "Failed to create database file: " << db_filename << std::endl;
        return false;
    }
    db_file.close();
    
    // Reopen in read/write mode
    db_file.open(db_filename, std::ios::binary | std::ios::in | std::ios::out);
    if (!db_file.is_open()) {
        std::cerr << "Failed to reopen database file" << std::endl;
        return false;
    }
    
    // Initialize header
    header = DatabaseHeader();
    
    // Write header to page 0
    Page header_page;
    header_page.header.type = FREE_PAGE;  // Header page
    header.serialize(header_page.data);
    header_page.update_checksum();
    
    uint8_t buffer[PAGE_SIZE];
    header_page.serialize(buffer);
    
    db_file.seekp(0);
    db_file.write(reinterpret_cast<char*>(buffer), PAGE_SIZE);
    db_file.flush();
    
    std::cout << "Database initialized: " << db_filename << std::endl;
    return true;
}

bool DatabaseEngine::open() {
    db_file.open(db_filename, std::ios::binary | std::ios::in | std::ios::out);
    if (!db_file.is_open()) {
        std::cerr << "Failed to open database file: " << db_filename << std::endl;
        return false;
    }
    
    // Read header from page 0
    uint8_t buffer[PAGE_SIZE];
    db_file.seekg(0);
    db_file.read(reinterpret_cast<char*>(buffer), PAGE_SIZE);
    
    Page header_page;
    header_page.deserialize(buffer);
    
    if (!header_page.verify_checksum()) {
        std::cerr << "Database header checksum failed!" << std::endl;
        return false;
    }
    
    header.deserialize(header_page.data);
    
    // Verify magic number
    if (strncmp(header.magic, "MTDB", 4) != 0) {
        std::cerr << "Invalid database file format" << std::endl;
        return false;
    }
    
    std::cout << "Database opened: " << db_filename << std::endl;
    std::cout << "Total pages: " << header.total_pages << std::endl;
    return true;
}

void DatabaseEngine::close() {
    if (db_file.is_open()) {
        write_header();  // Save header before closing
        db_file.close();
        std::cout << "Database closed" << std::endl;
    }
}

uint64_t DatabaseEngine::allocate_page() {
    uint64_t page_id;

    // Protect header access and raw file reads with the mutex, but avoid
    // calling other methods that also lock the same mutex while holding it
    // (to prevent deadlocks). We'll do the minimal work under the lock,
    // then persist the header afterwards.
    {
        std::lock_guard<std::mutex> lock(file_mutex);

        // Check free list first
        if (header.free_list_head != 0) {
            page_id = header.free_list_head;

            // Read the free page directly from disk (we hold the mutex so it's safe)
            uint8_t buffer[PAGE_SIZE];
            db_file.seekg(page_id * PAGE_SIZE);
            db_file.read(reinterpret_cast<char*>(buffer), PAGE_SIZE);

            Page free_page;
            free_page.deserialize(buffer);

            header.free_list_head = free_page.header.next_free_page;
        } else {
            // Allocate new page at end
            page_id = header.total_pages;
            header.total_pages++;
        }
    }

    // Persist header (acquires mutex internally)
    write_header();
    return page_id;
}

void DatabaseEngine::free_page(uint64_t page_id) {
    // We'll update the in-memory header under lock, but perform the page write
    // and header persistence outside that lock to avoid nested locking.
    uint64_t prev_head;
    {
        std::lock_guard<std::mutex> lock(file_mutex);
        prev_head = header.free_list_head;
        // Reserve this page as new head in memory
        header.free_list_head = page_id;
    }

    // Prepare free page (no locks needed for constructing the object)
    Page free_page;
    free_page.header.type = FREE_PAGE;
    free_page.header.next_free_page = prev_head;

    // Write the free page to disk (will lock internally)
    write_page(page_id, free_page);

    // Persist updated header (will lock internally)
    write_header();
}

Page DatabaseEngine::read_page(uint64_t page_id) {
    std::lock_guard<std::mutex> lock(file_mutex);
    
    // Check cache first
    auto it = page_cache.find(page_id);
    if (it != page_cache.end()) {
        return it->second;
    }
    
    // Read from disk
    uint8_t buffer[PAGE_SIZE];
    db_file.seekg(page_id * PAGE_SIZE);
    db_file.read(reinterpret_cast<char*>(buffer), PAGE_SIZE);
    
    if (!db_file.good()) {
        std::cerr << "Error reading page " << page_id << std::endl;
        return Page();
    }
    
    Page page;
    page.deserialize(buffer);
    
    // Add to cache
    if (page_cache.size() >= MAX_CACHE_SIZE) {
        // Simple eviction: remove first element (can be improved with LRU)
        page_cache.erase(page_cache.begin());
    }
    page_cache[page_id] = page;
    
    return page;
}

void DatabaseEngine::write_page(uint64_t page_id, const Page& page) {
    std::lock_guard<std::mutex> lock(file_mutex);
    
    // Update checksum before writing
    Page writable_page = page;
    writable_page.update_checksum();
    
    uint8_t buffer[PAGE_SIZE];
    writable_page.serialize(buffer);
    
    db_file.seekp(page_id * PAGE_SIZE);
    db_file.write(reinterpret_cast<char*>(buffer), PAGE_SIZE);
    db_file.flush();
    
    // Update cache
    page_cache[page_id] = writable_page;
}

void DatabaseEngine::write_header() {
    std::lock_guard<std::mutex> lock(file_mutex);
    
    Page header_page;
    header_page.header.type = FREE_PAGE;  // Special type for header
    header.serialize(header_page.data);
    header_page.update_checksum();
    
    uint8_t buffer[PAGE_SIZE];
    header_page.serialize(buffer);
    
    db_file.seekp(0);
    db_file.write(reinterpret_cast<char*>(buffer), PAGE_SIZE);
    db_file.flush();
}

uint64_t DatabaseEngine::get_next_user_id() {
    std::lock_guard<std::mutex> lock(file_mutex);
    return ++header.last_user_id;
}

uint64_t DatabaseEngine::get_next_meeting_id() {
    std::lock_guard<std::mutex> lock(file_mutex);
    return ++header.last_meeting_id;
}

uint64_t DatabaseEngine::get_next_message_id() {
    std::lock_guard<std::mutex> lock(file_mutex);
    return ++header.last_message_id;
}

uint64_t DatabaseEngine::get_next_file_id() {
    std::lock_guard<std::mutex> lock(file_mutex);
    return ++header.last_file_id;
}

uint64_t DatabaseEngine::get_next_whiteboard_id() {
    std::lock_guard<std::mutex> lock(file_mutex);
    return ++header.last_whiteboard_id;
}