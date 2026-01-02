#pragma once
#include <cstdint>
#include <cstring>

// Message model (for chat)
struct Message
{
    uint64_t message_id;
    uint64_t meeting_id;
    uint64_t user_id;
    char username[64];
    char content[2048]; // Increased from 512 to allow longer messages
    uint64_t timestamp;

    Message() : message_id(0), meeting_id(0), user_id(0), timestamp(0)
    {
        memset(username, 0, sizeof(username));
        memset(content, 0, sizeof(content));
    }

    void serialize(uint8_t *buffer) const
    {
        size_t offset = 0;
        memcpy(buffer + offset, &message_id, sizeof(message_id));
        offset += sizeof(message_id);
        memcpy(buffer + offset, &meeting_id, sizeof(meeting_id));
        offset += sizeof(meeting_id);
        memcpy(buffer + offset, &user_id, sizeof(user_id));
        offset += sizeof(user_id);
        memcpy(buffer + offset, username, sizeof(username));
        offset += sizeof(username);
        memcpy(buffer + offset, content, sizeof(content));
        offset += sizeof(content);
        memcpy(buffer + offset, &timestamp, sizeof(timestamp));
        offset += sizeof(timestamp);
    }

    void deserialize(const uint8_t *buffer)
    {
        size_t offset = 0;
        memcpy(&message_id, buffer + offset, sizeof(message_id));
        offset += sizeof(message_id);
        memcpy(&meeting_id, buffer + offset, sizeof(meeting_id));
        offset += sizeof(meeting_id);
        memcpy(&user_id, buffer + offset, sizeof(user_id));
        offset += sizeof(user_id);
        memcpy(username, buffer + offset, sizeof(username));
        offset += sizeof(username);
        memcpy(content, buffer + offset, sizeof(content));
        offset += sizeof(content);
        memcpy(&timestamp, buffer + offset, sizeof(timestamp));
        offset += sizeof(timestamp);
    }

    static size_t serialized_size()
    {
        return sizeof(message_id) + sizeof(meeting_id) + sizeof(user_id) +
               sizeof(username) + sizeof(content) + sizeof(timestamp);
    }
};