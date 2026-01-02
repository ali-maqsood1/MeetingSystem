#pragma once

#include <cstdint>
#include <cstring>

// Meeting model
struct Meeting {
    uint64_t meeting_id;
    char meeting_code[16];   // Unique code
    char title[128];
    uint64_t creator_id;
    uint64_t created_at;
    uint64_t started_at;
    uint64_t ended_at;
    bool is_active;
    
    Meeting() : meeting_id(0), creator_id(0), created_at(0), 
                started_at(0), ended_at(0), is_active(false) {
        memset(meeting_code, 0, sizeof(meeting_code));
        memset(title, 0, sizeof(title));
    }
    
    void serialize(uint8_t* buffer) const {
        size_t offset = 0;
        memcpy(buffer + offset, &meeting_id, sizeof(meeting_id)); offset += sizeof(meeting_id);
        memcpy(buffer + offset, meeting_code, sizeof(meeting_code)); offset += sizeof(meeting_code);
        memcpy(buffer + offset, title, sizeof(title)); offset += sizeof(title);
        memcpy(buffer + offset, &creator_id, sizeof(creator_id)); offset += sizeof(creator_id);
        memcpy(buffer + offset, &created_at, sizeof(created_at)); offset += sizeof(created_at);
        memcpy(buffer + offset, &started_at, sizeof(started_at)); offset += sizeof(started_at);
        memcpy(buffer + offset, &ended_at, sizeof(ended_at)); offset += sizeof(ended_at);
        buffer[offset++] = is_active ? 1 : 0;
    }
    
    void deserialize(const uint8_t* buffer) {
        size_t offset = 0;
        memcpy(&meeting_id, buffer + offset, sizeof(meeting_id)); offset += sizeof(meeting_id);
        memcpy(meeting_code, buffer + offset, sizeof(meeting_code)); offset += sizeof(meeting_code);
        memcpy(title, buffer + offset, sizeof(title)); offset += sizeof(title);
        memcpy(&creator_id, buffer + offset, sizeof(creator_id)); offset += sizeof(creator_id);
        memcpy(&created_at, buffer + offset, sizeof(created_at)); offset += sizeof(created_at);
        memcpy(&started_at, buffer + offset, sizeof(started_at)); offset += sizeof(started_at);
        memcpy(&ended_at, buffer + offset, sizeof(ended_at)); offset += sizeof(ended_at);
        is_active = (buffer[offset++] == 1);
    }
    
    static size_t serialized_size() {
        return sizeof(meeting_id) + sizeof(meeting_code) + sizeof(title) + 
               sizeof(creator_id) + sizeof(created_at) + sizeof(started_at) + 
               sizeof(ended_at) + 1;
    }
};
