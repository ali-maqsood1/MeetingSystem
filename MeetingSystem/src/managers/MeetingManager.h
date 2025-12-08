#ifndef MEETING_MANAGER_H
#define MEETING_MANAGER_H

#include "../storage/DatabaseEngine.h"
#include "../storage/BTree.h"
#include "../storage/HashTable.h"
#include "../models/Meeting.h"
#include <string>
#include <vector>
#include <random>
#include <map>

struct MeetingParticipant {
    uint64_t user_id;
    uint64_t joined_at;
    bool is_active;
    
    MeetingParticipant() : user_id(0), joined_at(0), is_active(true) {}
};

class MeetingManager {
private:
    DatabaseEngine* db;
    BTree* meetings_btree;
    HashTable* meeting_code_hash;

    
    std::mutex participants_mutex;
    
public:
    MeetingManager(DatabaseEngine* database, BTree* meetings_tree, HashTable* code_hash)
        : db(database), meetings_btree(meetings_tree), meeting_code_hash(code_hash) {}
    
    // Create new meeting
    bool create_meeting(uint64_t creator_id, const std::string& title, 
                        Meeting& out_meeting, std::string& error);
    
    // Join meeting by code
    bool join_meeting(const std::string& meeting_code, uint64_t user_id,
                      Meeting& out_meeting, std::string& error);
    
    // Get meeting by ID
    bool get_meeting(uint64_t meeting_id, Meeting& out_meeting);
    
    // Get meeting by code
    bool get_meeting_by_code(const std::string& code, Meeting& out_meeting);
    
    // Start meeting
    bool start_meeting(uint64_t meeting_id, std::string& error);
    
    // End meeting
    bool end_meeting(uint64_t meeting_id, std::string& error);
    
    // Get user's meetings
    std::vector<Meeting> get_user_meetings(uint64_t user_id);

    // Add participant when joining meeting
    bool add_participant(uint64_t meeting_id, uint64_t user_id);
    
    // Remove participant when leaving
    bool remove_participant(uint64_t meeting_id, uint64_t user_id);
    
    // Get all participants in meeting
    std::vector<MeetingParticipant> get_participants(uint64_t meeting_id);
    
private:

    std::map<uint64_t, std::vector<MeetingParticipant>> meeting_participants;
    // Generate unique meeting code (ABC-DEF-123 format)
    std::string generate_meeting_code();
    
    // Store meeting in database
    bool store_meeting(const Meeting& meeting);
    
    // Update meeting in database
    bool update_meeting(const Meeting& meeting);
};

#endif 