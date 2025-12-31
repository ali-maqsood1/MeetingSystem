#include "AuthManager.h"
#include <iostream>
#include <ctime>
#include <algorithm>

std::string AuthManager::hash_password(const std::string &password)
{
    // Simple hash function for DSA project
    // In production, use bcrypt or argon2
    uint64_t hash = 5381;
    for (char c : password)
    {
        hash = ((hash << 5) + hash) + c;
    }

    // Add salt
    hash ^= 0xDEADBEEF;

    char hash_str[65];
    snprintf(hash_str, sizeof(hash_str), "%016lx", hash);
    return std::string(hash_str);
}

std::string AuthManager::generate_token()
{
    static std::random_device rd;
    static std::mt19937_64 gen(rd());
    static std::uniform_int_distribution<uint64_t> dis;

    uint64_t token1 = dis(gen);
    uint64_t token2 = dis(gen);

    char token_str[128];
    snprintf(token_str, sizeof(token_str), "%016lx%016lx", token1, token2);
    return std::string(token_str);
}

bool AuthManager::is_token_expired(uint64_t expiry_time)
{
    uint64_t current_time = std::time(nullptr);
    return current_time > expiry_time;
}

bool AuthManager::store_user(const User &user)
{
    // Allocate page for user data
    uint64_t data_page_id = db->allocate_page();

    // Serialize user
    uint8_t buffer[User::serialized_size()];
    user.serialize(buffer);

    // Write to page
    Page data_page;
    memcpy(data_page.data, buffer, User::serialized_size());
    db->write_page(data_page_id, data_page);

    // Create record location
    RecordLocation user_loc(data_page_id, 0, User::serialized_size());

    // Index in B-Tree by user_id
    if (!users_btree->insert(user.user_id, user_loc))
    {
        std::cerr << "Failed to insert user into B-Tree" << std::endl;
        db->free_page(data_page_id);
        return false;
    }

    // Index in hash table by email
    if (!login_hash->insert(user.email, user_loc))
    {
        std::cerr << "Failed to insert user into hash table" << std::endl;
        return false;
    }

    return true;
}

bool AuthManager::register_user(const std::string &email, const std::string &username,
                                const std::string &password, User &out_user, std::string &error)
{
    // Validate input
    if (email.empty() || username.empty() || password.empty())
    {
        error = "All fields are required";
        return false;
    }

    if (email.length() >= 128 || username.length() >= 64)
    {
        error = "Email or username too long";
        return false;
    }

    // Check if email already exists
    User existing_user;
    if (get_user_by_email(email, existing_user))
    {
        error = "Email already exists";
        return false;
    }

    // Create new user
    User user;
    user.user_id = db->get_next_user_id();
    strcpy(user.email, email.c_str());
    strcpy(user.username, username.c_str());
    strcpy(user.password_hash, hash_password(password).c_str());
    user.created_at = std::time(nullptr);

    // Store user
    if (!store_user(user))
    {
        error = "Failed to store user";
        return false;
    }

    // Save database header (to persist counter)
    db->write_header();

    out_user = user;
    std::cout << "User registered: " << username << " (ID: " << user.user_id << ")" << std::endl;
    return true;
}

bool AuthManager::login(const std::string &email, const std::string &password,
                        std::string &out_token, User &out_user, std::string &error)
{
    // Get user by email
    User user;
    if (!get_user_by_email(email, user))
    {
        error = "Invalid credentials";
        return false;
    }

    // Verify password
    std::string password_hash = hash_password(password);
    if (strcmp(user.password_hash, password_hash.c_str()) != 0)
    {
        error = "Invalid credentials";
        return false;
    }

    // Generate session token
    std::string token = generate_token();
    uint64_t expiry = std::time(nullptr) + (24 * 60 * 60); // 24 hours

    // Store session (with single session enforcement)
    {
        std::lock_guard<std::mutex> lock(sessions_mutex);

        // Check if user already has an active session
        auto user_session_it = user_sessions.find(user.user_id);
        if (user_session_it != user_sessions.end())
        {
            // Invalidate previous session
            std::string old_token = user_session_it->second;
            sessions.erase(old_token);
            std::cout << "Previous session invalidated for user: " << user.username << std::endl;
        }

        // Store new session
        sessions[token] = {user.user_id, expiry};
        user_sessions[user.user_id] = token;
    }

    out_token = token;
    out_user = user;

    std::cout << "User logged in: " << user.username << std::endl;
    return true;
}

bool AuthManager::verify_token(const std::string &token, uint64_t &out_user_id)
{
    std::lock_guard<std::mutex> lock(sessions_mutex);

    auto it = sessions.find(token);
    if (it == sessions.end())
    {
        return false;
    }

    // Check expiry
    if (is_token_expired(it->second.second))
    {
        sessions.erase(it);
        return false;
    }

    out_user_id = it->second.first;
    return true;
}

void AuthManager::logout(const std::string &token)
{
    std::lock_guard<std::mutex> lock(sessions_mutex);

    // Find and remove from user_sessions mapping
    auto it = sessions.find(token);
    if (it != sessions.end())
    {
        uint64_t user_id = it->second.first;
        user_sessions.erase(user_id);
    }

    sessions.erase(token);
    std::cout << "User logged out" << std::endl;
}

bool AuthManager::get_user_by_id(uint64_t user_id, User &out_user)
{
    bool found;
    RecordLocation loc = users_btree->search(user_id, found);

    if (!found)
    {
        return false;
    }

    Page page = db->read_page(loc.page_id);
    out_user.deserialize(page.data + loc.offset);

    return true;
}

bool AuthManager::get_user_by_email(const std::string &email, User &out_user)
{
    bool found;
    RecordLocation loc = login_hash->search(email, found);

    if (!found)
    {
        return false;
    }

    Page page = db->read_page(loc.page_id);
    out_user.deserialize(page.data + loc.offset);

    return true;
}