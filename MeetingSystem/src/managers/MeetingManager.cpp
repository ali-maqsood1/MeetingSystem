#include "MeetingManager.h"
#include <iostream>
#include <ctime>
#include <algorithm>

std::string MeetingManager::generate_meeting_code() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 35);
    
    const char chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    
    std::string code;
    for (int i = 0; i < 11; i++) {
        if (i == 3 || i == 7) {
            code += '-';
        } else {
            code += chars[dis(gen)];
        }
    }
    
    return code; // Format: ABC-DEF-123
}

bool MeetingManager::store_meeting(const Meeting& meeting) {
    // Allocate page for meeting data
    uint64_t data_page_id = db->allocate_page();
    
    // Serialize meeting
    uint8_t buffer[Meeting::serialized_size()];
    meeting.serialize(buffer);
    
    // Write to page
    Page data_page;
    memcpy(data_page.data, buffer, Meeting::serialized_size());
    db->write_page(data_page_id, data_page);
    
    // Create record location
    RecordLocation meeting_loc(data_page_id, 0, Meeting::serialized_size());
    
    // Index in B-Tree by meeting_id
    if (!meetings_btree->insert(meeting.meeting_id, meeting_loc)) {
        std::cerr << "Failed to insert meeting into B-Tree" << std::endl;
        db->free_page(data_page_id);
        return false;
    }
    
    // Index in hash table by meeting_code
    if (!meeting_code_hash->insert(meeting.meeting_code, meeting_loc)) {
        std::cerr << "Failed to insert meeting into hash table" << std::endl;
        return false;
    }
    
    return true;
}

bool MeetingManager::update_meeting(const Meeting& meeting) {
    bool found;
    RecordLocation loc = meetings_btree->search(meeting.meeting_id, found);
    
    if (!found) {
        return false;
    }
    
    // Serialize meeting
    uint8_t buffer[Meeting::serialized_size()];
    meeting.serialize(buffer);
    
    // Read page, update data, write back
    Page page = db->read_page(loc.page_id);
    memcpy(page.data + loc.offset, buffer, Meeting::serialized_size());
    db->write_page(loc.page_id, page);
    
    return true;
}

bool MeetingManager::create_meeting(uint64_t creator_id, const std::string& title, 
                                     Meeting& out_meeting, std::string& error) {
    // Validate
    if (title.empty()) {
        error = "Meeting title is required";
        return false;
    }
    
    if (title.length() >= 128) {
        error = "Meeting title too long";
        return false;
    }
    
    // Create meeting
    Meeting meeting;
    meeting.meeting_id = db->get_next_meeting_id();
    
    // Generate unique code
    std::string code;
    do {
        code = generate_meeting_code();
    } while (get_meeting_by_code(code, out_meeting)); // Ensure uniqueness
    
    strcpy(meeting.meeting_code, code.c_str());
    strcpy(meeting.title, title.c_str());
    meeting.creator_id = creator_id;
    meeting.created_at = std::time(nullptr);
    meeting.started_at = 0;
    meeting.ended_at = 0;
    meeting.is_active = false;
    
    // Store meeting
    if (!store_meeting(meeting)) {
        error = "Failed to store meeting";
        return false;
    }
    
    // Save database header
    db->write_header();
    
    out_meeting = meeting;
    std::cout << "Meeting created: " << meeting.title << " (Code: " << meeting.meeting_code << ")" << std::endl;
    return true;
}

bool MeetingManager::join_meeting(const std::string& meeting_code, uint64_t user_id,
                                   Meeting& out_meeting, std::string& error) {
    if (!get_meeting_by_code(meeting_code, out_meeting)) {
        error = "Meeting not found";
        return false;
    }


    if (out_meeting.creator_id == user_id) {
        std::cout << "Creator " << user_id << " accessed their meeting: " << out_meeting.title << std::endl;
        return true;  // Creator can always access their meeting
    }
    
    
    std::cout << "User " << user_id << " joined meeting: " << out_meeting.title << std::endl;
    return true;
}

bool MeetingManager::get_meeting(uint64_t meeting_id, Meeting& out_meeting) {
    bool found;
    RecordLocation loc = meetings_btree->search(meeting_id, found);

    
    
    if (!found) {
        return false;
    }
    
    Page page = db->read_page(loc.page_id);
    out_meeting.deserialize(page.data + loc.offset);
    
    return true;
}

bool MeetingManager::get_meeting_by_code(const std::string& code, Meeting& out_meeting) {
    bool found;
    RecordLocation loc = meeting_code_hash->search(code, found);
    
    if (!found) {
        return false;
    }
    
    Page page = db->read_page(loc.page_id);
    out_meeting.deserialize(page.data + loc.offset);
    
    return true;
}

bool MeetingManager::start_meeting(uint64_t meeting_id, std::string& error) {
    Meeting meeting;
    if (!get_meeting(meeting_id, meeting)) {
        error = "Meeting not found";
        return false;
    }
    
    if (meeting.is_active) {
        error = "Meeting already started";
        return false;
    }
    
    meeting.is_active = true;
    meeting.started_at = std::time(nullptr);
    
    if (!update_meeting(meeting)) {
        error = "Failed to update meeting";
        return false;
    }
    
    std::cout << "Meeting started: " << meeting.title << std::endl;
    return true;
}

bool MeetingManager::end_meeting(uint64_t meeting_id, std::string& error) {
    Meeting meeting;
    if (!get_meeting(meeting_id, meeting)) {
        error = "Meeting not found";
        return false;
    }
    
    if (!meeting.is_active) {
        error = "Meeting not active";
        return false;
    }
    
    meeting.is_active = false;
    meeting.ended_at = std::time(nullptr);
    
    if (!update_meeting(meeting)) {
        error = "Failed to update meeting";
        return false;
    }
    
    std::cout << "Meeting ended: " << meeting.title << std::endl;
    return true;
}

std::vector<Meeting> MeetingManager::get_user_meetings(uint64_t user_id) {
    std::vector<Meeting> user_meetings;
    
    // For simplicity, we'll scan all meetings
    // In production, you'd want a separate index for creator_id
    // This is acceptable for a DSA project with limited data
    
    // Range search from meeting_id 1 to max
    auto locations = meetings_btree->range_search(1, UINT64_MAX);
    
    for (const auto& loc : locations) {
        Page page = db->read_page(loc.page_id);
        Meeting meeting;
        meeting.deserialize(page.data + loc.offset);
        
        if (meeting.creator_id == user_id) {
            user_meetings.push_back(meeting);
        }
    }
    
    return user_meetings;
}