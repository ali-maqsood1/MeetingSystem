#ifndef HASHTABLE_H
#define HASHTABLE_H

#include "DatabaseEngine.h"
#include "BTree.h"
#include <string>
#include <vector>

// Hash table configuration
const uint32_t DEFAULT_BUCKET_COUNT = 256;
// Calculate a safe number of entries that fit in a page. With PAGE_DATA_SIZE
// of 4032 bytes and HashEntry size around 152 bytes, 24 entries per bucket is
// a safe choice to avoid overflowing the page when serializing.
const uint16_t MAX_ENTRIES_PER_BUCKET = 24;

// Hash entry
struct HashEntry {
    uint64_t hash_value;
    char key[128];          // Max key length
    uint16_t key_length;
    uint64_t value_page;    // Page containing actual value
    uint16_t value_offset;
    uint16_t value_size;
    
    HashEntry() : hash_value(0), key_length(0), value_page(0), 
                  value_offset(0), value_size(0) {
        memset(key, 0, sizeof(key));
    }
};

// Hash bucket node (stored in page data)
struct HashBucket {
    uint16_t entry_count;
    uint64_t overflow_page;  // Next bucket page if overflow
    HashEntry entries[MAX_ENTRIES_PER_BUCKET];
    
    HashBucket() : entry_count(0), overflow_page(0) {}
    
    void serialize(uint8_t* buffer) const {
        size_t offset = 0;
        memcpy(buffer + offset, &entry_count, sizeof(entry_count)); 
        offset += sizeof(entry_count);
        memcpy(buffer + offset, &overflow_page, sizeof(overflow_page)); 
        offset += sizeof(overflow_page);
        memcpy(buffer + offset, entries, sizeof(entries));
    }
    
    void deserialize(const uint8_t* buffer) {
        size_t offset = 0;
        memcpy(&entry_count, buffer + offset, sizeof(entry_count)); 
        offset += sizeof(entry_count);
        memcpy(&overflow_page, buffer + offset, sizeof(overflow_page)); 
        offset += sizeof(overflow_page);
        memcpy(entries, buffer + offset, sizeof(entries));
    }
};

// Hash table header (stored in first page)
struct HashTableHeader {
    uint32_t bucket_count;
    uint64_t bucket_pages[DEFAULT_BUCKET_COUNT];
    
    HashTableHeader() : bucket_count(DEFAULT_BUCKET_COUNT) {
        for (uint32_t i = 0; i < DEFAULT_BUCKET_COUNT; i++) {
            bucket_pages[i] = 0;
        }
    }
    
    void serialize(uint8_t* buffer) const {
        size_t offset = 0;
        memcpy(buffer + offset, &bucket_count, sizeof(bucket_count)); 
        offset += sizeof(bucket_count);
        memcpy(buffer + offset, bucket_pages, sizeof(bucket_pages));
    }
    
    void deserialize(const uint8_t* buffer) {
        size_t offset = 0;
        memcpy(&bucket_count, buffer + offset, sizeof(bucket_count)); 
        offset += sizeof(bucket_count);
        memcpy(bucket_pages, buffer + offset, sizeof(bucket_pages));
    }
};

class HashTable {
private:
    DatabaseEngine* db_engine;
    uint64_t header_page_id;
    HashTableHeader header;
    
    // Hash functions
    uint64_t hash_string(const std::string& str);
    uint32_t get_bucket_index(uint64_t hash_value);
    
    // Bucket operations
    HashBucket load_bucket(uint64_t page_id);
    void save_bucket(uint64_t page_id, const HashBucket& bucket);
    
public:
    HashTable(DatabaseEngine* engine);
    
    // Initialize new hash table
    void initialize();
    
    // Load existing hash table
    void load(uint64_t header_page);
    
    // Core operations
    bool insert(const std::string& key, const RecordLocation& record);
    RecordLocation search(const std::string& key, bool& found);
    bool remove(const std::string& key);
    
    // Utility
    std::vector<std::string> get_all_keys();
    
    uint64_t get_header_page_id() const { return header_page_id; }
};

#endif // HASHTABLE_H