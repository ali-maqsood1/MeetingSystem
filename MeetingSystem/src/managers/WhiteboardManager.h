#ifndef WHITEBOARD_MANAGER_H
#define WHITEBOARD_MANAGER_H

#include "../storage/DatabaseEngine.h"
#include "../storage/BTree.h"
#include "../models/WhiteboardElement.h"
#include <string>
#include <vector>

class WhiteboardManager {
private:
    DatabaseEngine* db;
    BTree* whiteboard_btree;
    
public:
    WhiteboardManager(DatabaseEngine* database, BTree* whiteboard_tree)
        : db(database), whiteboard_btree(whiteboard_tree) {}
    
    // Draw element
    bool draw_element(uint64_t meeting_id, uint64_t user_id, uint8_t element_type,
                      int16_t x1, int16_t y1, int16_t x2, int16_t y2,
                      uint8_t color_r, uint8_t color_g, uint8_t color_b,
                      uint16_t stroke_width, const std::string& text,
                      WhiteboardElement& out_element, std::string& error);
    
    // Get all elements for meeting
    std::vector<WhiteboardElement> get_meeting_elements(uint64_t meeting_id);
    
    // Get elements since timestamp (for incremental updates)
    std::vector<WhiteboardElement> get_elements_since(uint64_t meeting_id, uint64_t since_timestamp);
    
    // Clear whiteboard
    bool clear_whiteboard(uint64_t meeting_id, std::string& error);
    
    // Delete specific element
    bool delete_element(uint64_t element_id, std::string& error);
    
    // Get element by ID
    bool get_element(uint64_t element_id, WhiteboardElement& out_element);
    
private:
    // Store whiteboard element
    bool store_element(const WhiteboardElement& element);
};

#endif // WHITEBOARD_MANAGER_H