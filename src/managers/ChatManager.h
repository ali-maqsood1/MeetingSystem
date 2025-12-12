#ifndef CHAT_MANAGER_H
#define CHAT_MANAGER_H

#include "../storage/DatabaseEngine.h"
#include "../storage/BTree.h"
#include "../storage/HashTable.h"
#include "../models/Message.h"
#include <string>
#include <vector>
#include <map>
#include <mutex>

class ChatManager {
private:
    DatabaseEngine* db;
    BTree* messages_btree;
    HashTable* chat_search_hash;
    
    // Message cache for quick access
    std::map<uint64_t, std::vector<Message>> meeting_messages; // meeting_id -> messages
    std::mutex cache_mutex;
    
public:
    ChatManager(DatabaseEngine* database, BTree* messages_tree, HashTable* search_hash)
        : db(database), messages_btree(messages_tree), chat_search_hash(search_hash) {}
    
    // Send message
    bool send_message(uint64_t meeting_id, uint64_t user_id, const std::string& username,
                      const std::string& content, Message& out_message, std::string& error);
    
    // Get messages for a meeting
    std::vector<Message> get_messages(uint64_t meeting_id, int limit = 50, 
                                       uint64_t before_timestamp = UINT64_MAX);
    
    // Search messages by keyword
    std::vector<Message> search_messages(uint64_t meeting_id, const std::string& query);
    
    // Get message by ID
    bool get_message(uint64_t message_id, Message& out_message);
    
    // Delete message
    bool delete_message(uint64_t message_id, std::string& error);
    
    // Get message count for meeting
    int get_message_count(uint64_t meeting_id);
    
private:
    // Store message in database
    bool store_message(const Message& message);
    
    // Index message keywords for search
    void index_message_keywords(uint64_t message_id, const std::string& content);
    
    // Extract keywords from text
    std::vector<std::string> extract_keywords(const std::string& text);
};

#endif // CHAT_MANAGER_H



