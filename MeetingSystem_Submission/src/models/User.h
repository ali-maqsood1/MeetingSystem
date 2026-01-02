#pragma once
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

// User model
struct User {
    uint64_t user_id;
    char email[128];
    char password_hash[64];  // Store hashed password
    char username[64];
    uint64_t created_at;     // Unix timestamp
    
    User() : user_id(0), created_at(0) {
        memset(email, 0, sizeof(email));
        memset(password_hash, 0, sizeof(password_hash));
        memset(username, 0, sizeof(username));
    }
    
    void serialize(uint8_t* buffer) const {
        size_t offset = 0;
        memcpy(buffer + offset, &user_id, sizeof(user_id)); offset += sizeof(user_id);
        memcpy(buffer + offset, email, sizeof(email)); offset += sizeof(email);
        memcpy(buffer + offset, password_hash, sizeof(password_hash)); offset += sizeof(password_hash);
        memcpy(buffer + offset, username, sizeof(username)); offset += sizeof(username);
        memcpy(buffer + offset, &created_at, sizeof(created_at)); offset += sizeof(created_at);
    }
    
    void deserialize(const uint8_t* buffer) {
        size_t offset = 0;
        memcpy(&user_id, buffer + offset, sizeof(user_id)); offset += sizeof(user_id);
        memcpy(email, buffer + offset, sizeof(email)); offset += sizeof(email);
        memcpy(password_hash, buffer + offset, sizeof(password_hash)); offset += sizeof(password_hash);
        memcpy(username, buffer + offset, sizeof(username)); offset += sizeof(username);
        memcpy(&created_at, buffer + offset, sizeof(created_at)); offset += sizeof(created_at);
    }
    
    static size_t serialized_size() {
        return sizeof(user_id) + sizeof(email) + sizeof(password_hash) + 
               sizeof(username) + sizeof(created_at);
    }
};






