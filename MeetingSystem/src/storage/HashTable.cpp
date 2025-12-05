#include "HashTable.h"
#include <iostream>
#include <cstring>

HashTable::HashTable(DatabaseEngine* engine) 
    : db_engine(engine), header_page_id(0) {
}

void HashTable::initialize() {
    // Allocate header page
    header_page_id = db_engine->allocate_page();
    
    // Initialize header
    header = HashTableHeader();
    
    // Allocate bucket pages
    for (uint32_t i = 0; i < header.bucket_count; i++) {
        header.bucket_pages[i] = db_engine->allocate_page();
        
        // Initialize empty bucket
        HashBucket bucket;
        save_bucket(header.bucket_pages[i], bucket);
    }
    
    // Save header
    Page header_page;
    header_page.header.type = HASH_BUCKET;  // Special type
    header.serialize(header_page.data);
    db_engine->write_page(header_page_id, header_page);
    
    std::cout << "Hash table initialized with " << header.bucket_count 
              << " buckets at page " << header_page_id << std::endl;
}

void HashTable::load(uint64_t header_page) {
    header_page_id = header_page;
    
    Page page = db_engine->read_page(header_page_id);
    header.deserialize(page.data);
    
    std::cout << "Hash table loaded from page " << header_page_id << std::endl;
}

uint64_t HashTable::hash_string(const std::string& str) {
    // FNV-1a hash algorithm
    uint64_t hash = 14695981039346656037ULL;
    for (char c : str) {
        hash ^= static_cast<uint64_t>(c);
        hash *= 1099511628211ULL;
    }
    return hash;
}

uint32_t HashTable::get_bucket_index(uint64_t hash_value) {
    return hash_value % header.bucket_count;
}

HashBucket HashTable::load_bucket(uint64_t page_id) {
    Page page = db_engine->read_page(page_id);
    HashBucket bucket;
    bucket.deserialize(page.data);
    return bucket;
}

void HashTable::save_bucket(uint64_t page_id, const HashBucket& bucket) {
    Page page;
    page.header.type = HASH_BUCKET;
    bucket.serialize(page.data);
    db_engine->write_page(page_id, page);
}

bool HashTable::insert(const std::string& key, const RecordLocation& record) {
    if (key.length() >= 128) {
        std::cerr << "Key too long (max 127 chars)" << std::endl;
        return false;
    }
    
    uint64_t hash_value = hash_string(key);
    uint32_t bucket_idx = get_bucket_index(hash_value);
    uint64_t bucket_page = header.bucket_pages[bucket_idx];
    
    // Check if key already exists and find insertion point
    uint64_t current_page = bucket_page;
    while (current_page != 0) {
        HashBucket bucket = load_bucket(current_page);
        
        // Check for existing key
        for (uint16_t i = 0; i < bucket.entry_count; i++) {
            if (bucket.entries[i].hash_value == hash_value &&
                strcmp(bucket.entries[i].key, key.c_str()) == 0) {
                // Update existing entry
                bucket.entries[i].value_page = record.page_id;
                bucket.entries[i].value_offset = record.offset;
                bucket.entries[i].value_size = record.size;
                save_bucket(current_page, bucket);
                return true;
            }
        }
        
        // Check if bucket has space
        if (bucket.entry_count < MAX_ENTRIES_PER_BUCKET) {
            // Insert new entry
            HashEntry& entry = bucket.entries[bucket.entry_count];
            entry.hash_value = hash_value;
            strcpy(entry.key, key.c_str());
            entry.key_length = key.length();
            entry.value_page = record.page_id;
            entry.value_offset = record.offset;
            entry.value_size = record.size;
            bucket.entry_count++;
            
            save_bucket(current_page, bucket);
            return true;
        }
        
        // Check overflow
        if (bucket.overflow_page == 0) {
            // Create overflow page
            uint64_t overflow_page = db_engine->allocate_page();
            bucket.overflow_page = overflow_page;
            save_bucket(current_page, bucket);
            
            // Initialize overflow bucket
            HashBucket overflow_bucket;
            save_bucket(overflow_page, overflow_bucket);
        }
        
        current_page = bucket.overflow_page;
    }
    
    return false;
}

RecordLocation HashTable::search(const std::string& key, bool& found) {
    uint64_t hash_value = hash_string(key);
    uint32_t bucket_idx = get_bucket_index(hash_value);
    uint64_t bucket_page = header.bucket_pages[bucket_idx];
    
    uint64_t current_page = bucket_page;
    while (current_page != 0) {
        HashBucket bucket = load_bucket(current_page);
        
        for (uint16_t i = 0; i < bucket.entry_count; i++) {
            if (bucket.entries[i].hash_value == hash_value &&
                strcmp(bucket.entries[i].key, key.c_str()) == 0) {
                // Found!
                found = true;
                return RecordLocation(
                    bucket.entries[i].value_page,
                    bucket.entries[i].value_offset,
                    bucket.entries[i].value_size
                );
            }
        }
        
        current_page = bucket.overflow_page;
    }
    
    found = false;
    return RecordLocation();
}

bool HashTable::remove(const std::string& key) {
    uint64_t hash_value = hash_string(key);
    uint32_t bucket_idx = get_bucket_index(hash_value);
    uint64_t bucket_page = header.bucket_pages[bucket_idx];
    
    uint64_t current_page = bucket_page;
    while (current_page != 0) {
        HashBucket bucket = load_bucket(current_page);
        
        for (uint16_t i = 0; i < bucket.entry_count; i++) {
            if (bucket.entries[i].hash_value == hash_value &&
                strcmp(bucket.entries[i].key, key.c_str()) == 0) {
                // Found - remove by shifting
                for (uint16_t j = i; j < bucket.entry_count - 1; j++) {
                    bucket.entries[j] = bucket.entries[j + 1];
                }
                bucket.entry_count--;
                
                save_bucket(current_page, bucket);
                return true;
            }
        }
        
        current_page = bucket.overflow_page;
    }
    
    return false;
}

std::vector<std::string> HashTable::get_all_keys() {
    std::vector<std::string> keys;
    
    for (uint32_t i = 0; i < header.bucket_count; i++) {
        uint64_t current_page = header.bucket_pages[i];
        
        while (current_page != 0) {
            HashBucket bucket = load_bucket(current_page);
            
            for (uint16_t j = 0; j < bucket.entry_count; j++) {
                keys.push_back(std::string(bucket.entries[j].key));
            }
            
            current_page = bucket.overflow_page;
        }
    }
    
    return keys;
}