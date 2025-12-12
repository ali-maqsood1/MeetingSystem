#ifndef DATABASE_ENGINE_H
#define DATABASE_ENGINE_H

#include "Page.h"
#include <fstream>
#include <string>
#include <mutex>
#include <unordered_map>

class DatabaseEngine {
private:
    std::string db_filename;
    std::fstream db_file;
    DatabaseHeader header;
    std::mutex file_mutex;  // Thread-safe file operations
    
    // Page cache (LRU cache can be added later)
    std::unordered_map<uint64_t, Page> page_cache;
    const size_t MAX_CACHE_SIZE = 100;
    
public:
    DatabaseEngine(const std::string& filename);
    ~DatabaseEngine();
    
    // Initialize or open database
    bool initialize();
    bool open();
    void close();
    
    // Page management
    uint64_t allocate_page();
    void free_page(uint64_t page_id);
    Page read_page(uint64_t page_id);
    void write_page(uint64_t page_id, const Page& page);
    
    // Header management
    DatabaseHeader& get_header() { return header; }
    void write_header();
    
    // Auto-increment ID generators
    uint64_t get_next_user_id();
    uint64_t get_next_meeting_id();
    uint64_t get_next_message_id();
    uint64_t get_next_file_id();
    uint64_t get_next_whiteboard_id();
    
    // Utility
    bool is_open() const { return db_file.is_open(); }
    uint64_t get_total_pages() const { return header.total_pages; }
};

#endif // DATABASE_ENGINE_H