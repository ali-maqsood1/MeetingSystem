#ifndef PAGE_H
#define PAGE_H

#include <cstdint>
#include <cstring>
#include <vector>

const uint32_t PAGE_SIZE = 4096;
const uint32_t PAGE_HEADER_SIZE = 64;
const uint32_t PAGE_DATA_SIZE = PAGE_SIZE - PAGE_HEADER_SIZE;

// Page types
enum PageType : uint8_t {
    FREE_PAGE = 0,
    BTREE_INTERNAL = 1,
    BTREE_LEAF = 2,
    HASH_BUCKET = 3,
    DATA_OVERFLOW = 4
};

// Page header structure (64 bytes)
struct PageHeader {
    PageType type;              // 1 byte
    uint8_t reserved1[7];       // 7 bytes padding
    uint64_t next_free_page;    // 8 bytes - for free list
    uint32_t checksum;          // 4 bytes
    uint8_t reserved2[44];      
    
    PageHeader() : type(FREE_PAGE), next_free_page(0), checksum(0) {
        memset(reserved1, 0, sizeof(reserved1));
        memset(reserved2, 0, sizeof(reserved2));
    }
};


class Page {
public:
    PageHeader header;
    uint8_t data[PAGE_DATA_SIZE];
    
    Page() {
        memset(data, 0, PAGE_DATA_SIZE);
    }
    
    uint32_t calculate_checksum() const {
        uint32_t sum = 0;
        for (size_t i = 0; i < PAGE_DATA_SIZE; i++) {
            sum += data[i];
        }
        return sum;
    }
    
    void update_checksum() {
        header.checksum = calculate_checksum();
    }
    
    bool verify_checksum() const {
        return header.checksum == calculate_checksum();
    }
    
    void serialize(uint8_t* buffer) const {
        memcpy(buffer, &header, PAGE_HEADER_SIZE);
        memcpy(buffer + PAGE_HEADER_SIZE, data, PAGE_DATA_SIZE);
    }
    
    void deserialize(const uint8_t* buffer) {
        memcpy(&header, buffer, PAGE_HEADER_SIZE);
        memcpy(data, buffer + PAGE_HEADER_SIZE, PAGE_DATA_SIZE);
    }
};

struct DatabaseHeader {
    char magic[4];              // "MTDB"
    uint32_t version;           // Database version
    uint32_t page_size;         // PAGE_SIZE
    uint64_t total_pages;       // Total pages in database
    
    // B-Tree root pages
    uint64_t users_btree_root;
    uint64_t meetings_btree_root;
    uint64_t messages_btree_root;
    uint64_t files_btree_root;
    uint64_t whiteboard_btree_root;
    
    // Hash table page locations
    uint64_t login_hash_page;
    uint64_t meeting_code_hash_page;
    uint64_t file_dedup_hash_page;
    uint64_t chat_search_hash_page;
    
    // Free list management
    uint64_t free_list_head;
    
    // Auto-increment counters
    uint64_t last_user_id;
    uint64_t last_meeting_id;
    uint64_t last_message_id;
    uint64_t last_file_id;
    uint64_t last_whiteboard_id;
    
    DatabaseHeader() {
        magic[0] = 'M'; magic[1] = 'T'; 
        magic[2] = 'D'; magic[3] = 'B';
        version = 1;
        page_size = PAGE_SIZE;
        total_pages = 1;  // Start with header page
        
        users_btree_root = 0;
        meetings_btree_root = 0;
        messages_btree_root = 0;
        files_btree_root = 0;
        whiteboard_btree_root = 0;
        
        login_hash_page = 0;
        meeting_code_hash_page = 0;
        file_dedup_hash_page = 0;
        chat_search_hash_page = 0;
        
        free_list_head = 0;
        
        last_user_id = 0;
        last_meeting_id = 0;
        last_message_id = 0;
        last_file_id = 0;
        last_whiteboard_id = 0;
    }
    
    // Serialize to page data
    void serialize(uint8_t* buffer) const {
        size_t offset = 0;
        memcpy(buffer + offset, magic, 4); offset += 4;
        memcpy(buffer + offset, &version, sizeof(version)); offset += sizeof(version);
        memcpy(buffer + offset, &page_size, sizeof(page_size)); offset += sizeof(page_size);
        memcpy(buffer + offset, &total_pages, sizeof(total_pages)); offset += sizeof(total_pages);
        
        memcpy(buffer + offset, &users_btree_root, sizeof(users_btree_root)); offset += sizeof(users_btree_root);
        memcpy(buffer + offset, &meetings_btree_root, sizeof(meetings_btree_root)); offset += sizeof(meetings_btree_root);
        memcpy(buffer + offset, &messages_btree_root, sizeof(messages_btree_root)); offset += sizeof(messages_btree_root);
        memcpy(buffer + offset, &files_btree_root, sizeof(files_btree_root)); offset += sizeof(files_btree_root);
        memcpy(buffer + offset, &whiteboard_btree_root, sizeof(whiteboard_btree_root)); offset += sizeof(whiteboard_btree_root);
        
        memcpy(buffer + offset, &login_hash_page, sizeof(login_hash_page)); offset += sizeof(login_hash_page);
        memcpy(buffer + offset, &meeting_code_hash_page, sizeof(meeting_code_hash_page)); offset += sizeof(meeting_code_hash_page);
        memcpy(buffer + offset, &file_dedup_hash_page, sizeof(file_dedup_hash_page)); offset += sizeof(file_dedup_hash_page);
        memcpy(buffer + offset, &chat_search_hash_page, sizeof(chat_search_hash_page)); offset += sizeof(chat_search_hash_page);
        
        memcpy(buffer + offset, &free_list_head, sizeof(free_list_head)); offset += sizeof(free_list_head);
        
        memcpy(buffer + offset, &last_user_id, sizeof(last_user_id)); offset += sizeof(last_user_id);
        memcpy(buffer + offset, &last_meeting_id, sizeof(last_meeting_id)); offset += sizeof(last_meeting_id);
        memcpy(buffer + offset, &last_message_id, sizeof(last_message_id)); offset += sizeof(last_message_id);
        memcpy(buffer + offset, &last_file_id, sizeof(last_file_id)); offset += sizeof(last_file_id);
        memcpy(buffer + offset, &last_whiteboard_id, sizeof(last_whiteboard_id)); offset += sizeof(last_whiteboard_id);
    }
    
    // Deserialize from page data
    void deserialize(const uint8_t* buffer) {
        size_t offset = 0;
        memcpy(magic, buffer + offset, 4); offset += 4;
        memcpy(&version, buffer + offset, sizeof(version)); offset += sizeof(version);
        memcpy(&page_size, buffer + offset, sizeof(page_size)); offset += sizeof(page_size);
        memcpy(&total_pages, buffer + offset, sizeof(total_pages)); offset += sizeof(total_pages);
        
        memcpy(&users_btree_root, buffer + offset, sizeof(users_btree_root)); offset += sizeof(users_btree_root);
        memcpy(&meetings_btree_root, buffer + offset, sizeof(meetings_btree_root)); offset += sizeof(meetings_btree_root);
        memcpy(&messages_btree_root, buffer + offset, sizeof(messages_btree_root)); offset += sizeof(messages_btree_root);
        memcpy(&files_btree_root, buffer + offset, sizeof(files_btree_root)); offset += sizeof(files_btree_root);
        memcpy(&whiteboard_btree_root, buffer + offset, sizeof(whiteboard_btree_root)); offset += sizeof(whiteboard_btree_root);
        
        memcpy(&login_hash_page, buffer + offset, sizeof(login_hash_page)); offset += sizeof(login_hash_page);
        memcpy(&meeting_code_hash_page, buffer + offset, sizeof(meeting_code_hash_page)); offset += sizeof(meeting_code_hash_page);
        memcpy(&file_dedup_hash_page, buffer + offset, sizeof(file_dedup_hash_page)); offset += sizeof(file_dedup_hash_page);
        memcpy(&chat_search_hash_page, buffer + offset, sizeof(chat_search_hash_page)); offset += sizeof(chat_search_hash_page);
        
        memcpy(&free_list_head, buffer + offset, sizeof(free_list_head)); offset += sizeof(free_list_head);
        
        memcpy(&last_user_id, buffer + offset, sizeof(last_user_id)); offset += sizeof(last_user_id);
        memcpy(&last_meeting_id, buffer + offset, sizeof(last_meeting_id)); offset += sizeof(last_meeting_id);
        memcpy(&last_message_id, buffer + offset, sizeof(last_message_id)); offset += sizeof(last_message_id);
        memcpy(&last_file_id, buffer + offset, sizeof(last_file_id)); offset += sizeof(last_file_id);
        memcpy(&last_whiteboard_id, buffer + offset, sizeof(last_whiteboard_id)); offset += sizeof(last_whiteboard_id);
    }
};

#endif // PAGE_H