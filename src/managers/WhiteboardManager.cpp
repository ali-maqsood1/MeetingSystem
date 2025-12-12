#include "WhiteboardManager.h"
#include <iostream>
#include <ctime>
#include <algorithm>

bool WhiteboardManager::store_element(const WhiteboardElement &element)
{
    // Allocate page for element
    uint64_t data_page_id = db->allocate_page();

    // Serialize element
    uint8_t buffer[WhiteboardElement::serialized_size()];
    element.serialize(buffer);

    // Write to page
    Page data_page;
    memcpy(data_page.data, buffer, WhiteboardElement::serialized_size());
    db->write_page(data_page_id, data_page);

    // Create record location
    RecordLocation element_loc(data_page_id, 0, WhiteboardElement::serialized_size());

    // Index in B-Tree by element_id
    if (!whiteboard_btree->insert(element.element_id, element_loc))
    {
        std::cerr << "Failed to insert whiteboard element into B-Tree" << std::endl;
        db->free_page(data_page_id);
        return false;
    }

    return true;
}

bool WhiteboardManager::draw_element(uint64_t meeting_id, uint64_t user_id,
                                     uint8_t element_type,
                                     int16_t x1, int16_t y1, int16_t x2, int16_t y2,
                                     uint8_t color_r, uint8_t color_g, uint8_t color_b,
                                     uint16_t stroke_width, const std::string &text,
                                     WhiteboardElement &out_element, std::string &error)
{
    // Validate
    if (element_type > 6)
    {
        error = "Invalid element type (0=line, 1=rect, 2=circle, 3=text, 4=triangle, 5=arrow, 6=star)";
        return false;
    }

    if (element_type == 3 && text.length() >= 256)
    {
        error = "Text too long (max 255 characters)";
        return false;
    }

    // Create element
    WhiteboardElement element;
    element.element_id = db->get_next_whiteboard_id();
    element.meeting_id = meeting_id;
    element.user_id = user_id;
    element.element_type = element_type;
    element.x1 = x1;
    element.y1 = y1;
    element.x2 = x2;
    element.y2 = y2;
    element.color_r = color_r;
    element.color_g = color_g;
    element.color_b = color_b;
    element.stroke_width = stroke_width;

    if (!text.empty())
    {
        strcpy(element.text, text.c_str());
    }

    element.timestamp = std::time(nullptr);

    // Store element
    if (!store_element(element))
    {
        error = "Failed to store whiteboard element";
        return false;
    }

    // Save database header
    db->write_header();

    out_element = element;

    const char *type_names[] = {"line", "rectangle", "circle", "text", "triangle", "arrow", "star"};
    std::cout << "Whiteboard element drawn: " << type_names[element_type]
              << " in meeting " << meeting_id << std::endl;

    return true;
}

std::vector<WhiteboardElement> WhiteboardManager::get_meeting_elements(uint64_t meeting_id)
{
    std::vector<WhiteboardElement> elements;

    auto locations = whiteboard_btree->range_search(1, UINT64_MAX);

    for (const auto &loc : locations)
    {
        Page page = db->read_page(loc.page_id);
        WhiteboardElement element;
        element.deserialize(page.data + loc.offset);

        if (element.meeting_id == meeting_id)
        {
            elements.push_back(element);
        }
    }

    // Sort by timestamp
    std::sort(elements.begin(), elements.end(),
              [](const WhiteboardElement &a, const WhiteboardElement &b)
              {
                  return a.timestamp < b.timestamp;
              });

    return elements;
}

std::vector<WhiteboardElement> WhiteboardManager::get_elements_since(uint64_t meeting_id,
                                                                     uint64_t since_timestamp)
{
    std::vector<WhiteboardElement> elements;

    auto locations = whiteboard_btree->range_search(1, UINT64_MAX);

    for (const auto &loc : locations)
    {
        Page page = db->read_page(loc.page_id);
        WhiteboardElement element;
        element.deserialize(page.data + loc.offset);

        if (element.meeting_id == meeting_id && element.timestamp > since_timestamp)
        {
            elements.push_back(element);
        }
    }

    // Sort by timestamp
    std::sort(elements.begin(), elements.end(),
              [](const WhiteboardElement &a, const WhiteboardElement &b)
              {
                  return a.timestamp < b.timestamp;
              });

    return elements;
}

bool WhiteboardManager::clear_whiteboard(uint64_t meeting_id, std::string &error)
{
    // For DSA project, we'll mark all elements as deleted
    // Full implementation would remove from B-Tree

    auto elements = get_meeting_elements(meeting_id);

    for (const auto &element : elements)
    {
        delete_element(element.element_id, error);
    }

    std::cout << "Whiteboard cleared for meeting " << meeting_id
              << " (" << elements.size() << " elements)" << std::endl;

    return true;
}

bool WhiteboardManager::delete_element(uint64_t element_id, std::string &error)
{
    WhiteboardElement element;
    if (!get_element(element_id, element))
    {
        error = "Element not found";
        return false;
    }

    // Mark as deleted by setting element_type to invalid value
    element.element_type = 255; // Deleted marker

    // Update in database
    bool found;
    RecordLocation loc = whiteboard_btree->search(element_id, found);

    if (!found)
    {
        error = "Element not found";
        return false;
    }

    uint8_t buffer[WhiteboardElement::serialized_size()];
    element.serialize(buffer);

    Page page = db->read_page(loc.page_id);
    memcpy(page.data + loc.offset, buffer, WhiteboardElement::serialized_size());
    db->write_page(loc.page_id, page);

    return true;
}

bool WhiteboardManager::get_element(uint64_t element_id, WhiteboardElement &out_element)
{
    bool found;
    RecordLocation loc = whiteboard_btree->search(element_id, found);

    if (!found)
    {
        return false;
    }

    Page page = db->read_page(loc.page_id);
    out_element.deserialize(page.data + loc.offset);

    return true;
}