#pragma once
#include <cstdint>
#include <cstring>



// File model
struct FileRecord {
    uint64_t file_id;
    uint64_t meeting_id;
    uint64_t uploader_id;
    char filename[256];
    char file_hash[64];      // SHA-256 hash for deduplication
    uint64_t file_size;
    uint64_t uploaded_at;
    uint64_t data_page_id;   // First page of file data
    
    FileRecord() : file_id(0), meeting_id(0), uploader_id(0), 
                   file_size(0), uploaded_at(0), data_page_id(0) {
        memset(filename, 0, sizeof(filename));
        memset(file_hash, 0, sizeof(file_hash));
    }
    
    void serialize(uint8_t* buffer) const {
        size_t offset = 0;
        memcpy(buffer + offset, &file_id, sizeof(file_id)); offset += sizeof(file_id);
        memcpy(buffer + offset, &meeting_id, sizeof(meeting_id)); offset += sizeof(meeting_id);
        memcpy(buffer + offset, &uploader_id, sizeof(uploader_id)); offset += sizeof(uploader_id);
        memcpy(buffer + offset, filename, sizeof(filename)); offset += sizeof(filename);
        memcpy(buffer + offset, file_hash, sizeof(file_hash)); offset += sizeof(file_hash);
        memcpy(buffer + offset, &file_size, sizeof(file_size)); offset += sizeof(file_size);
        memcpy(buffer + offset, &uploaded_at, sizeof(uploaded_at)); offset += sizeof(uploaded_at);
        memcpy(buffer + offset, &data_page_id, sizeof(data_page_id)); offset += sizeof(data_page_id);
    }
    
    void deserialize(const uint8_t* buffer) {
        size_t offset = 0;
        memcpy(&file_id, buffer + offset, sizeof(file_id)); offset += sizeof(file_id);
        memcpy(&meeting_id, buffer + offset, sizeof(meeting_id)); offset += sizeof(meeting_id);
        memcpy(&uploader_id, buffer + offset, sizeof(uploader_id)); offset += sizeof(uploader_id);
        memcpy(filename, buffer + offset, sizeof(filename)); offset += sizeof(filename);
        memcpy(file_hash, buffer + offset, sizeof(file_hash)); offset += sizeof(file_hash);
        memcpy(&file_size, buffer + offset, sizeof(file_size)); offset += sizeof(file_size);
        memcpy(&uploaded_at, buffer + offset, sizeof(uploaded_at)); offset += sizeof(uploaded_at);
        memcpy(&data_page_id, buffer + offset, sizeof(data_page_id)); offset += sizeof(data_page_id);
    }
    
    static size_t serialized_size() {
        return sizeof(file_id) + sizeof(meeting_id) + sizeof(uploader_id) + 
               sizeof(filename) + sizeof(file_hash) + sizeof(file_size) + 
               sizeof(uploaded_at) + sizeof(data_page_id);
    }
};