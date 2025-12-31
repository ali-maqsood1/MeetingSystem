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
#include <queue>
#include <thread>
#include <condition_variable>
#include <atomic>

class ChatManager
{
private:
    DatabaseEngine *db;
    BTree *messages_btree;
    HashTable *chat_search_hash;

    // Message cache for quick access (increased from 100 to 500 per meeting)
    std::map<uint64_t, std::vector<Message>> meeting_messages; // meeting_id -> messages
    std::map<uint64_t, Message> message_by_id;                 // message_id -> message (O(1) lookup)
    std::mutex cache_mutex;

    // Async persistence
    std::queue<Message> persistence_queue;
    std::mutex queue_mutex;
    std::condition_variable queue_cv;
    std::thread persistence_thread;
    std::atomic<bool> shutdown_flag;

    // Long polling support
    std::condition_variable message_notify_cv;
    std::mutex notify_mutex;
    std::map<uint64_t, uint64_t> last_message_timestamp; // meeting_id -> last timestamp

    // Async indexing (separate from persistence)
    std::queue<std::pair<uint64_t, std::string>> indexing_queue; // (message_id, content)
    std::mutex indexing_mutex;
    std::condition_variable indexing_cv;
    std::thread indexing_thread;

public:
    ChatManager(DatabaseEngine *database, BTree *messages_tree, HashTable *search_hash);
    ~ChatManager();

    // Send message
    bool send_message(uint64_t meeting_id, uint64_t user_id, const std::string &username,
                      const std::string &content, Message &out_message, std::string &error);

    // Get messages for a meeting
    std::vector<Message> get_messages(uint64_t meeting_id, int limit = 50,
                                      uint64_t before_timestamp = UINT64_MAX);

    // 🔥 Long polling: wait for new messages after timestamp
    std::vector<Message> wait_for_messages(uint64_t meeting_id, uint64_t since_timestamp,
                                           int timeout_seconds = 20);

    // Search messages by keyword
    std::vector<Message> search_messages(uint64_t meeting_id, const std::string &query);

    // Get message by ID
    bool get_message(uint64_t message_id, Message &out_message);

    // Delete message
    bool delete_message(uint64_t message_id, std::string &error);

    // Get message count for meeting
    int get_message_count(uint64_t meeting_id);

    // Delete all messages for a meeting (cleanup when deleting meeting)
    void delete_meeting_messages(uint64_t meeting_id);

private:
    // Background persistence worker
    void persistence_worker();

    // Background indexing worker
    void indexing_worker();

    // Warm cache on startup
    void warm_cache();

    // Store message in database
    bool store_message(const Message &message);

    // Index message keywords for search
    void index_message_keywords(uint64_t message_id, const std::string &content);

    // Extract keywords from text
    std::vector<std::string> extract_keywords(const std::string &text);
};

#endif // CHAT_MANAGER_H
