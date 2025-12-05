#include "ScreenShareManager.h"
#include <iostream>
#include <ctime>
#include <sstream>
#include <iomanip>

std::string ScreenShareManager::generate_stream_id(uint64_t meeting_id, uint64_t user_id) {
    std::ostringstream oss;
    oss << "stream_" << meeting_id << "_" << user_id << "_" << std::time(nullptr);
    return oss.str();
}

bool ScreenShareManager::start_screen_share(uint64_t meeting_id, uint64_t user_id,
                                             const std::string& username, 
                                             std::string& stream_id) {
    std::lock_guard<std::mutex> lock(shares_mutex);
    
    // Check if someone is already sharing in this meeting
    auto it = active_shares.find(meeting_id);
    if (it != active_shares.end()) {
        std::cerr << "Screen share already active in meeting " << meeting_id << std::endl;
        return false;
    }
    
    // Generate stream ID
    stream_id = generate_stream_id(meeting_id, user_id);
    
    // Initialize frame
    ScreenFrame frame;
    frame.meeting_id = meeting_id;
    frame.user_id = user_id;
    frame.username = username;
    frame.timestamp = std::time(nullptr);
    
    active_shares[meeting_id] = {user_id, frame};
    
    std::cout << "Screen share started: " << username 
              << " in meeting " << meeting_id 
              << " (stream: " << stream_id << ")" << std::endl;
    
    return true;
}

bool ScreenShareManager::stop_screen_share(uint64_t meeting_id, uint64_t user_id) {
    std::lock_guard<std::mutex> lock(shares_mutex);
    
    auto it = active_shares.find(meeting_id);
    if (it == active_shares.end()) {
        std::cerr << "No active screen share in meeting " << meeting_id << std::endl;
        return false;
    }
    
    if (it->second.first != user_id) {
        std::cerr << "User " << user_id << " is not sharing in meeting " << meeting_id << std::endl;
        return false;
    }
    
    active_shares.erase(it);
    
    std::cout << "Screen share stopped: user " << user_id 
              << " in meeting " << meeting_id << std::endl;
    
    return true;
}

bool ScreenShareManager::upload_frame(uint64_t meeting_id, uint64_t user_id,
                                       const std::vector<uint8_t>& jpeg_data,
                                       uint32_t width, uint32_t height) {
    std::lock_guard<std::mutex> lock(shares_mutex);
    
    auto it = active_shares.find(meeting_id);
    if (it == active_shares.end() || it->second.first != user_id) {
        std::cerr << "User " << user_id << " is not sharing in meeting " << meeting_id << std::endl;
        return false;
    }
    
    // Update frame
    ScreenFrame& frame = it->second.second;
    frame.jpeg_data = jpeg_data;
    frame.width = width;
    frame.height = height;
    frame.timestamp = std::time(nullptr);
    
    std::cout << "Screen frame uploaded: meeting " << meeting_id 
              << ", size: " << jpeg_data.size() << " bytes, "
              << width << "x" << height << std::endl;
    
    // Broadcast frame
    if (frame_callback) {
        frame_callback(meeting_id, frame);
    }
    
    return true;
}

bool ScreenShareManager::get_latest_frame(uint64_t meeting_id, ScreenFrame& out_frame) {
    std::lock_guard<std::mutex> lock(shares_mutex);
    
    auto it = active_shares.find(meeting_id);
    if (it == active_shares.end()) {
        return false;
    }
    
    out_frame = it->second.second;
    return true;
}

bool ScreenShareManager::is_sharing(uint64_t meeting_id, uint64_t& out_user_id) {
    std::lock_guard<std::mutex> lock(shares_mutex);
    
    auto it = active_shares.find(meeting_id);
    if (it == active_shares.end()) {
        return false;
    }
    
    out_user_id = it->second.first;
    return true;
}