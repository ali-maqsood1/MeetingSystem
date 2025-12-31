#include "ChatManager.h"
#include <iostream>
#include <ctime>
#include <algorithm>
#include <sstream>
#include <cctype>

// Constructor - start persistence thread
ChatManager::ChatManager(DatabaseEngine *database, BTree *messages_tree, HashTable *search_hash)
    : db(database), messages_btree(messages_tree), chat_search_hash(search_hash),
      shutdown_flag(false)
{
    persistence_thread = std::thread(&ChatManager::persistence_worker, this);
}

// Destructor - clean shutdown
ChatManager::~ChatManager()
{
    shutdown_flag = true;
    queue_cv.notify_one();
    indexing_cv.notify_one();
    message_notify_cv.notify_all();

    if (persistence_thread.joinable())
    {
        persistence_thread.join();
    }
    if (indexing_thread.joinable())
    {
        indexing_thread.join();
    }
}

// Background worker that persists messages
void ChatManager::persistence_worker()
{
    while (!shutdown_flag)
    {
        Message msg;

        // Wait for a message
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            queue_cv.wait(lock, [this]
                          { return !persistence_queue.empty() || shutdown_flag; });

            if (shutdown_flag && persistence_queue.empty())
            {
                break;
            }

            if (persistence_queue.empty())
            {
                continue;
            }

            msg = persistence_queue.front();
            persistence_queue.pop();
        }

        // Persist to disk (outside lock)
        if (!store_message(msg))
        {
            std::cerr << "Failed to persist message " << msg.message_id << std::endl;
        }

        // Write header
        db->write_header();
    }
}

// 🔥 Separate indexing worker - doesn't block persistence
void ChatManager::indexing_worker()
{
    while (!shutdown_flag)
    {
        std::pair<uint64_t, std::string> item;

        // Wait for indexing work
        {
            std::unique_lock<std::mutex> lock(indexing_mutex);
            indexing_cv.wait(lock, [this]
                             { return !indexing_queue.empty() || shutdown_flag; });

            if (shutdown_flag && indexing_queue.empty())
            {
                break;
            }

            if (indexing_queue.empty())
            {
                continue;
            }

            item = indexing_queue.front();
            indexing_queue.pop();
        }

        // Index keywords (outside lock)
        index_message_keywords(item.first, item.second);
    }
}

// Warm cache on startup - preload recent messages
void ChatManager::warm_cache()
{
    std::cout << "Warming message cache..." << std::endl;

    // Load recent messages from database
    auto locations = messages_btree->range_search(1, UINT64_MAX);

    // Track messages per meeting
    std::map<uint64_t, std::vector<Message>> temp_cache;

    for (const auto &loc : locations)
    {
        Page page = db->read_page(loc.page_id);
        Message message;
        message.deserialize(page.data + loc.offset);

        temp_cache[message.meeting_id].push_back(message);
    }

    // Store in cache (keep only last 500 per meeting)
    std::lock_guard<std::mutex> lock(cache_mutex);
    for (auto &[meeting_id, messages] : temp_cache)
    {
        // Sort by timestamp
        std::sort(messages.begin(), messages.end(),
                  [](const Message &a, const Message &b)
                  {
                      return a.timestamp < b.timestamp;
                  });

        // Keep only last 500
        if (messages.size() > 500)
        {
            messages.erase(messages.begin(), messages.end() - 500);
        }

        meeting_messages[meeting_id] = std::move(messages);

        // Build message_by_id index
        for (const auto &msg : meeting_messages[meeting_id])
        {
            message_by_id[msg.message_id] = msg;
        }
    }

    std::cout << "Cache warmed with " << message_by_id.size() << " messages" << std::endl;
}

bool ChatManager::store_message(const Message &message)
{
    // Allocate page for message data
    uint64_t data_page_id = db->allocate_page();

    // Serialize message
    uint8_t buffer[Message::serialized_size()];
    message.serialize(buffer);

    // Write to page
    Page data_page;
    memcpy(data_page.data, buffer, Message::serialized_size());
    db->write_page(data_page_id, data_page);

    // Create record location
    RecordLocation message_loc(data_page_id, 0, Message::serialized_size());

    // Index in B-Tree by message_id
    if (!messages_btree->insert(message.message_id, message_loc))
    {
        std::cerr << "Failed to insert message into B-Tree" << std::endl;
        db->free_page(data_page_id);
        return false;
    }

    return true;
}

std::vector<std::string> ChatManager::extract_keywords(const std::string &text)
{
    std::vector<std::string> keywords;
    std::string current_word;

    for (char c : text)
    {
        if (std::isalnum(c))
        {
            current_word += std::tolower(c);
        }
        else if (!current_word.empty())
        {
            if (current_word.length() >= 3)
            { // Only index words with 3+ chars
                keywords.push_back(current_word);
            }
            current_word.clear();
        }
    }

    if (!current_word.empty() && current_word.length() >= 3)
    {
        keywords.push_back(current_word);
    }

    return keywords;
}

void ChatManager::index_message_keywords(uint64_t message_id, const std::string &content)
{
    auto keywords = extract_keywords(content);

    for (const auto &keyword : keywords)
    {
        // Store message_id in hash table under keyword
        RecordLocation loc(message_id, 0, 0); // Store just the ID
        chat_search_hash->insert(keyword, loc);
    }
}

bool ChatManager::send_message(uint64_t meeting_id, uint64_t user_id,
                               const std::string &username, const std::string &content,
                               Message &out_message, std::string &error)
{
    // Validate
    if (content.empty())
    {
        error = "Message content is required";
        return false;
    }

    if (content.length() >= 2048)
    {
        error = "Message too long (max 2047 characters)";
        return false;
    }

    // Create message
    Message message;
    message.message_id = db->get_next_message_id();
    message.meeting_id = meeting_id;
    message.user_id = user_id;
    strcpy(message.username, username.c_str());
    strcpy(message.content, content.c_str());
    message.timestamp = std::time(nullptr);

    // 🔥 FAST PATH - immediate cache update
    {
        std::lock_guard<std::mutex> lock(cache_mutex);
        meeting_messages[meeting_id].push_back(message);
        message_by_id[message.message_id] = message; // O(1) lookup index
        last_message_timestamp[meeting_id] = message.timestamp;

        // Keep cache size limited (last 500 messages per meeting - 5x increase)
        if (meeting_messages[meeting_id].size() > 500)
        {
            // Remove oldest message from both caches
            uint64_t old_id = meeting_messages[meeting_id].front().message_id;
            meeting_messages[meeting_id].erase(meeting_messages[meeting_id].begin());
            message_by_id.erase(old_id);
        }
    }

    // 🔥 ASYNC persistence - queue for background thread
    {
        std::lock_guard<std::mutex> lock(queue_mutex);
        persistence_queue.push(message);
    }
    queue_cv.notify_one();

    // 🔥 ASYNC indexing - separate queue
    {
        std::lock_guard<std::mutex> lock(indexing_mutex);
        indexing_queue.push({message.message_id, content});
    }
    indexing_cv.notify_one();

    // 🔥 NOTIFY long-polling waiters
    message_notify_cv.notify_all();

    out_message = message;
    std::cout << "Message sent in meeting " << meeting_id << " by " << username << std::endl;
    return true;
}

// 🔥 Long polling: wait for new messages after timestamp
std::vector<Message> ChatManager::wait_for_messages(uint64_t meeting_id, uint64_t since_timestamp, int timeout_seconds)
{
    auto start_time = std::chrono::steady_clock::now();
    auto timeout = std::chrono::seconds(timeout_seconds);

    while (true)
    {
        // Check for new messages
        {
            std::lock_guard<std::mutex> lock(cache_mutex);
            auto it = last_message_timestamp.find(meeting_id);

            // If we have new messages, return them
            if (it != last_message_timestamp.end() && it->second > since_timestamp)
            {
                return get_messages(meeting_id, 50);
            }
        }

        // Check timeout
        auto elapsed = std::chrono::steady_clock::now() - start_time;
        if (elapsed >= timeout)
        {
            return {}; // Empty vector - no new messages within timeout
        }

        // Wait for notification or timeout
        std::unique_lock<std::mutex> lock(notify_mutex);
        message_notify_cv.wait_for(lock, std::chrono::milliseconds(100));
    }
}

std::vector<Message> ChatManager::get_messages(uint64_t meeting_id, int limit,
                                               uint64_t before_timestamp)
{
    std::vector<Message> messages;

    // 🔥 OPTIMIZED: Check cache first (now 500 msgs, should hit most of the time)
    {
        std::lock_guard<std::mutex> lock(cache_mutex);
        if (meeting_messages.find(meeting_id) != meeting_messages.end())
        {
            auto &cached = meeting_messages[meeting_id];
            for (auto it = cached.rbegin(); it != cached.rend() && messages.size() < (size_t)limit; ++it)
            {
                if (it->timestamp < before_timestamp)
                {
                    messages.push_back(*it);
                }
            }

            // If we have enough messages from cache, return immediately
            if (messages.size() >= (size_t)limit || cached.size() < 500)
            {
                std::reverse(messages.begin(), messages.end());
                return messages;
            }
        }
    }

    // Fallback: Load from database (only if cache miss or need older messages)
    auto locations = messages_btree->range_search(1, UINT64_MAX);

    for (auto it = locations.rbegin(); it != locations.rend() && messages.size() < (size_t)limit; ++it)
    {
        Page page = db->read_page(it->page_id);
        Message message;
        message.deserialize(page.data + it->offset);

        if (message.meeting_id == meeting_id && message.timestamp < before_timestamp)
        {
            messages.push_back(message);
        }
    }

    std::reverse(messages.begin(), messages.end());
    return messages;
}

std::vector<Message> ChatManager::search_messages(uint64_t meeting_id, const std::string &query)
{
    std::vector<Message> results;
    std::map<uint64_t, bool> found_ids; // Avoid duplicates

    auto keywords = extract_keywords(query);

    for (const auto &keyword : keywords)
    {
        bool found;
        RecordLocation loc = chat_search_hash->search(keyword, found);

        if (found)
        {
            uint64_t message_id = loc.page_id; // We stored message_id here

            if (found_ids.find(message_id) == found_ids.end())
            {
                Message message;
                if (get_message(message_id, message))
                {
                    if (message.meeting_id == meeting_id)
                    {
                        results.push_back(message);
                        found_ids[message_id] = true;
                    }
                }
            }
        }
    }

    // Sort by timestamp
    std::sort(results.begin(), results.end(),
              [](const Message &a, const Message &b)
              { return a.timestamp < b.timestamp; });

    return results;
}

bool ChatManager::get_message(uint64_t message_id, Message &out_message)
{
    // 🔥 OPTIMIZED: Check cache first (O(1) lookup)
    {
        std::lock_guard<std::mutex> lock(cache_mutex);
        auto it = message_by_id.find(message_id);
        if (it != message_by_id.end())
        {
            out_message = it->second;
            return true;
        }
    }

    // Fallback: Load from database
    bool found;
    RecordLocation loc = messages_btree->search(message_id, found);

    if (!found)
    {
        return false;
    }

    Page page = db->read_page(loc.page_id);
    out_message.deserialize(page.data + loc.offset);

    return true;
}

bool ChatManager::delete_message(uint64_t message_id, std::string &error)
{
    // For DSA project, we'll just mark it as deleted (set content to empty)
    // Full implementation would remove from B-Tree
    Message message;
    if (!get_message(message_id, message))
    {
        error = "Message not found";
        return false;
    }

    strcpy(message.content, "[deleted]");

    // Update in database
    bool found;
    RecordLocation loc = messages_btree->search(message_id, found);

    if (!found)
    {
        error = "Message not found";
        return false;
    }

    uint8_t buffer[Message::serialized_size()];
    message.serialize(buffer);

    Page page = db->read_page(loc.page_id);
    memcpy(page.data + loc.offset, buffer, Message::serialized_size());
    db->write_page(loc.page_id, page);

    return true;
}

int ChatManager::get_message_count(uint64_t meeting_id)
{
    int count = 0;

    auto locations = messages_btree->range_search(1, UINT64_MAX);

    for (const auto &loc : locations)
    {
        Page page = db->read_page(loc.page_id);
        Message message;
        message.deserialize(page.data + loc.offset);

        if (message.meeting_id == meeting_id)
        {
            count++;
        }
    }

    return count;
}

void ChatManager::delete_meeting_messages(uint64_t meeting_id)
{
    std::lock_guard<std::mutex> lock(cache_mutex);

    // Remove from cache
    meeting_messages.erase(meeting_id);

    // Remove individual messages from message_by_id cache
    for (auto it = message_by_id.begin(); it != message_by_id.end();)
    {
        if (it->second.meeting_id == meeting_id)
        {
            it = message_by_id.erase(it);
        }
        else
        {
            ++it;
        }
    }

    // Remove from database
    auto locations = messages_btree->range_search(1, UINT64_MAX);
    for (const auto &loc : locations)
    {
        Page page = db->read_page(loc.page_id);
        Message message;
        message.deserialize(page.data + loc.offset);

        if (message.meeting_id == meeting_id)
        {
            messages_btree->remove(message.message_id);
            db->free_page(loc.page_id);
        }
    }

    std::cout << "🗑️  Deleted all messages for meeting " << meeting_id << std::endl;
}