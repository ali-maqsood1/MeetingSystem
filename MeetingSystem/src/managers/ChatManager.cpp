#include "ChatManager.h"
#include <iostream>
#include <ctime>
#include <algorithm>
#include <sstream>
#include <cctype>

bool ChatManager::store_message(const Message& message) {
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
    if (!messages_btree->insert(message.message_id, message_loc)) {
        std::cerr << "Failed to insert message into B-Tree" << std::endl;
        db->free_page(data_page_id);
        return false;
    }
    
    return true;
}

std::vector<std::string> ChatManager::extract_keywords(const std::string& text) {
    std::vector<std::string> keywords;
    std::string current_word;
    
    for (char c : text) {
        if (std::isalnum(c)) {
            current_word += std::tolower(c);
        } else if (!current_word.empty()) {
            if (current_word.length() >= 3) { // Only index words with 3+ chars
                keywords.push_back(current_word);
            }
            current_word.clear();
        }
    }
    
    if (!current_word.empty() && current_word.length() >= 3) {
        keywords.push_back(current_word);
    }
    
    return keywords;
}

void ChatManager::index_message_keywords(uint64_t message_id, const std::string& content) {
    auto keywords = extract_keywords(content);
    
    for (const auto& keyword : keywords) {
        // Store message_id in hash table under keyword
        RecordLocation loc(message_id, 0, 0); // Store just the ID
        chat_search_hash->insert(keyword, loc);
    }
}

bool ChatManager::send_message(uint64_t meeting_id, uint64_t user_id, 
                                const std::string& username, const std::string& content,
                                Message& out_message, std::string& error) {
    // Validate
    if (content.empty()) {
        error = "Message content is required";
        return false;
    }
    
    if (content.length() >= 512) {
        error = "Message too long (max 511 characters)";
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
    
    // Store message
    if (!store_message(message)) {
        error = "Failed to store message";
        return false;
    }
    
    // Index keywords for search
    index_message_keywords(message.message_id, content);
    
    // Save database header
    db->write_header();
    
    // Add to cache
    {
        std::lock_guard<std::mutex> lock(cache_mutex);
        meeting_messages[meeting_id].push_back(message);
        
        // Keep cache size limited (last 100 messages per meeting)
        if (meeting_messages[meeting_id].size() > 100) {
            meeting_messages[meeting_id].erase(meeting_messages[meeting_id].begin());
        }
    }
    
    out_message = message;
    std::cout << "Message sent in meeting " << meeting_id << " by " << username << std::endl;
    return true;
}

std::vector<Message> ChatManager::get_messages(uint64_t meeting_id, int limit, 
                                                uint64_t before_timestamp) {
    std::vector<Message> messages;
    
    // Check cache first
    {
        std::lock_guard<std::mutex> lock(cache_mutex);
        if (meeting_messages.find(meeting_id) != meeting_messages.end()) {
            auto& cached = meeting_messages[meeting_id];
            for (auto it = cached.rbegin(); it != cached.rend() && messages.size() < limit; ++it) {
                if (it->timestamp < before_timestamp) {
                    messages.push_back(*it);
                }
            }
            if (!messages.empty()) {
                std::reverse(messages.begin(), messages.end());
                return messages;
            }
        }
    }
    
    // Load from database
    auto locations = messages_btree->range_search(1, UINT64_MAX);
    
    for (auto it = locations.rbegin(); it != locations.rend() && messages.size() < limit; ++it) {
        Page page = db->read_page(it->page_id);
        Message message;
        message.deserialize(page.data + it->offset);
        
        if (message.meeting_id == meeting_id && message.timestamp < before_timestamp) {
            messages.push_back(message);
        }
    }
    
    std::reverse(messages.begin(), messages.end());
    return messages;
}

std::vector<Message> ChatManager::search_messages(uint64_t meeting_id, const std::string& query) {
    std::vector<Message> results;
    std::map<uint64_t, bool> found_ids; // Avoid duplicates
    
    auto keywords = extract_keywords(query);
    
    for (const auto& keyword : keywords) {
        bool found;
        RecordLocation loc = chat_search_hash->search(keyword, found);
        
        if (found) {
            uint64_t message_id = loc.page_id; // We stored message_id here
            
            if (found_ids.find(message_id) == found_ids.end()) {
                Message message;
                if (get_message(message_id, message)) {
                    if (message.meeting_id == meeting_id) {
                        results.push_back(message);
                        found_ids[message_id] = true;
                    }
                }
            }
        }
    }
    
    // Sort by timestamp
    std::sort(results.begin(), results.end(), 
              [](const Message& a, const Message& b) { return a.timestamp < b.timestamp; });
    
    return results;
}

bool ChatManager::get_message(uint64_t message_id, Message& out_message) {
    bool found;
    RecordLocation loc = messages_btree->search(message_id, found);
    
    if (!found) {
        return false;
    }
    
    Page page = db->read_page(loc.page_id);
    out_message.deserialize(page.data + loc.offset);
    
    return true;
}

bool ChatManager::delete_message(uint64_t message_id, std::string& error) {
    // For DSA project, we'll just mark it as deleted (set content to empty)
    // Full implementation would remove from B-Tree
    Message message;
    if (!get_message(message_id, message)) {
        error = "Message not found";
        return false;
    }
    
    strcpy(message.content, "[deleted]");
    
    // Update in database
    bool found;
    RecordLocation loc = messages_btree->search(message_id, found);
    
    if (!found) {
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

int ChatManager::get_message_count(uint64_t meeting_id) {
    int count = 0;
    
    auto locations = messages_btree->range_search(1, UINT64_MAX);
    
    for (const auto& loc : locations) {
        Page page = db->read_page(loc.page_id);
        Message message;
        message.deserialize(page.data + loc.offset);
        
        if (message.meeting_id == meeting_id) {
            count++;
        }
    }
    
    return count;
}