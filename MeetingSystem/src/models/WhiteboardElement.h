#pragma once
#include <cstdint>
#include <cstring>



// Whiteboard element model
struct WhiteboardElement {
    uint64_t element_id;
    uint64_t meeting_id;
    uint64_t user_id;
    uint8_t element_type;    // 0=line, 1=rect, 2=circle, 3=text
    int16_t x1, y1, x2, y2;  // Coordinates
    uint8_t color_r, color_g, color_b;
    uint16_t stroke_width;
    char text[256];          // For text elements
    uint64_t timestamp;
    
    WhiteboardElement() : element_id(0), meeting_id(0), user_id(0),
                          element_type(0), x1(0), y1(0), x2(0), y2(0),
                          color_r(0), color_g(0), color_b(0), 
                          stroke_width(1), timestamp(0) {
        memset(text, 0, sizeof(text));
    }
    
    void serialize(uint8_t* buffer) const {
        size_t offset = 0;
        memcpy(buffer + offset, &element_id, sizeof(element_id)); offset += sizeof(element_id);
        memcpy(buffer + offset, &meeting_id, sizeof(meeting_id)); offset += sizeof(meeting_id);
        memcpy(buffer + offset, &user_id, sizeof(user_id)); offset += sizeof(user_id);
        buffer[offset++] = element_type;
        memcpy(buffer + offset, &x1, sizeof(x1)); offset += sizeof(x1);
        memcpy(buffer + offset, &y1, sizeof(y1)); offset += sizeof(y1);
        memcpy(buffer + offset, &x2, sizeof(x2)); offset += sizeof(x2);
        memcpy(buffer + offset, &y2, sizeof(y2)); offset += sizeof(y2);
        buffer[offset++] = color_r;
        buffer[offset++] = color_g;
        buffer[offset++] = color_b;
        memcpy(buffer + offset, &stroke_width, sizeof(stroke_width)); offset += sizeof(stroke_width);
        memcpy(buffer + offset, text, sizeof(text)); offset += sizeof(text);
        memcpy(buffer + offset, &timestamp, sizeof(timestamp)); offset += sizeof(timestamp);
    }
    
    void deserialize(const uint8_t* buffer) {
        size_t offset = 0;
        memcpy(&element_id, buffer + offset, sizeof(element_id)); offset += sizeof(element_id);
        memcpy(&meeting_id, buffer + offset, sizeof(meeting_id)); offset += sizeof(meeting_id);
        memcpy(&user_id, buffer + offset, sizeof(user_id)); offset += sizeof(user_id);
        element_type = buffer[offset++];
        memcpy(&x1, buffer + offset, sizeof(x1)); offset += sizeof(x1);
        memcpy(&y1, buffer + offset, sizeof(y1)); offset += sizeof(y1);
        memcpy(&x2, buffer + offset, sizeof(x2)); offset += sizeof(x2);
        memcpy(&y2, buffer + offset, sizeof(y2)); offset += sizeof(y2);
        color_r = buffer[offset++];
        color_g = buffer[offset++];
        color_b = buffer[offset++];
        memcpy(&stroke_width, buffer + offset, sizeof(stroke_width)); offset += sizeof(stroke_width);
        memcpy(text, buffer + offset, sizeof(text)); offset += sizeof(text);
        memcpy(&timestamp, buffer + offset, sizeof(timestamp)); offset += sizeof(timestamp);
    }
    
    static size_t serialized_size() {
        return sizeof(element_id) + sizeof(meeting_id) + sizeof(user_id) + 1 +
               sizeof(x1) + sizeof(y1) + sizeof(x2) + sizeof(y2) + 3 +
               sizeof(stroke_width) + sizeof(text) + sizeof(timestamp);
    }
};
