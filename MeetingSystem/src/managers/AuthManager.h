#ifndef AUTH_MANAGER_H
#define AUTH_MANAGER_H

#include "../storage/DatabaseEngine.h"
#include "../storage/BTree.h"
#include "../storage/HashTable.h"
#include "../models/User.h"
#include <string>
#include <map>
#include <mutex>
#include <random>
#include <chrono>

class AuthManager {
private:
    DatabaseEngine* db;
    BTree* users_btree;
    HashTable* login_hash;
    
    // Active sessions: token -> (user_id, expiry_time)
    std::map<std::string, std::pair<uint64_t, uint64_t>> sessions;
    std::mutex sessions_mutex;
    
public:
    AuthManager(DatabaseEngine* database, BTree* users_tree, HashTable* login_table)
        : db(database), users_btree(users_tree), login_hash(login_table) {}
    
    // Register new user
    bool register_user(const std::string& email, const std::string& username, 
                       const std::string& password, User& out_user, std::string& error);
    
    // Login user
    bool login(const std::string& email, const std::string& password, 
               std::string& out_token, User& out_user, std::string& error);
    
    // Verify session token
    bool verify_token(const std::string& token, uint64_t& out_user_id);
    
    // Logout
    void logout(const std::string& token);
    
    // Get user by ID
    bool get_user_by_id(uint64_t user_id, User& out_user);
    
    // Get user by email
    bool get_user_by_email(const std::string& email, User& out_user);
    
private:
    // Hash password (simple SHA-256-like hash)
    std::string hash_password(const std::string& password);
    
    // Generate session token
    std::string generate_token();
    
    // Check if token is expired
    bool is_token_expired(uint64_t expiry_time);
    
    // Store user in database
    bool store_user(const User& user);
};

#endif // AUTH_MANAGER_H