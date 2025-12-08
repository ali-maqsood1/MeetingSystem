#ifndef SCREEN_SHARE_MANAGER_H
#define SCREEN_SHARE_MANAGER_H

#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <memory>
#include <functional>

// Screen frame data
struct ScreenFrame {
    uint64_t meeting_id;
    uint64_t user_id;
    std::string username;
    std::vector<uint8_t> jpeg_data;
    uint64_t timestamp;
    uint32_t width;
    uint32_t height;
    
    ScreenFrame() : meeting_id(0), user_id(0), timestamp(0), width(0), height(0) {}
};

class ScreenShareManager {
private:
    // Active screen shares: meeting_id -> (user_id, latest frame)
    std::map<uint64_t, std::pair<uint64_t, ScreenFrame>> active_shares;
    std::mutex shares_mutex;
    
    // Frame broadcast callback
    std::function<void(uint64_t, const ScreenFrame&)> frame_callback;
    
public:
    ScreenShareManager() {}
    
    // Set callback for broadcasting frames
    void set_frame_callback(std::function<void(uint64_t, const ScreenFrame&)> callback) {
        frame_callback = callback;
    }
    
    // Start screen share
    bool start_screen_share(uint64_t meeting_id, uint64_t user_id, 
                           const std::string& username, std::string& stream_id);
    
    // Stop screen share
    bool stop_screen_share(uint64_t meeting_id, uint64_t user_id);
    
    // Upload frame
    bool upload_frame(uint64_t meeting_id, uint64_t user_id, 
                      const std::vector<uint8_t>& jpeg_data,
                      uint32_t width, uint32_t height);
    
    // Get latest frame
    bool get_latest_frame(uint64_t meeting_id, ScreenFrame& out_frame);
    
    // Check if screen share is active
    bool is_sharing(uint64_t meeting_id, uint64_t& out_user_id);
    
    // Generate stream ID
    std::string generate_stream_id(uint64_t meeting_id, uint64_t user_id);
};

#endif // SCREEN_SHARE_MANAGER_H

