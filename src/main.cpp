#include "storage/DatabaseEngine.h"
#include "storage/BTree.h"
#include "storage/HashTable.h"
#include "server/HTTPServer.h"
#include "managers/AuthManager.h"
#include "managers/MeetingManager.h"
#include "managers/ChatManager.h"
#include "managers/FileManager.h"
#include "managers/WhiteboardManager.h"
#include "managers/ScreenShareManager.h"
#include "utils/JSON.h"
#include <utils/Hash.h>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <memory>
#include <signal.h>


struct WebRTCSignal {
    std::string type;          // "offer", "answer", "ice-candidate"
    std::string sdp;           // SDP description (for offer/answer)
    std::string candidate;     // ICE candidate (for ice-candidate)
    std::string sdp_mid;       // Media stream ID
    int sdp_m_line_index;      // Media line index
    uint64_t from_user_id;
    uint64_t to_user_id;
    uint64_t timestamp;
    
    WebRTCSignal() : sdp_m_line_index(0), from_user_id(0), to_user_id(0), timestamp(0) {}
};



// Helper function to serialize WebRTCSignal to JSON
std::string serialize_signal(const WebRTCSignal& sig) {
    std::string json = "{";
    json += "\"type\":\"" + sig.type + "\",";
    
    if (!sig.sdp.empty()) {
        json += "\"sdp\":\"" + sig.sdp + "\",";
    }
    
    if (!sig.candidate.empty()) {
        json += "\"candidate\":\"" + sig.candidate + "\",";
        json += "\"sdpMid\":\"" + sig.sdp_mid + "\",";
        json += "\"sdpMLineIndex\":" + std::to_string(sig.sdp_m_line_index) + ",";
    }
    
    json += "\"from\":" + std::to_string(sig.from_user_id) + ",";
    json += "\"to\":" + std::to_string(sig.to_user_id) + ",";
    json += "\"timestamp\":" + std::to_string(sig.timestamp);
    json += "}";
    
    return json;
}

// Global server pointer for signal handling
HTTPServer* g_server = nullptr;

void signal_handler(int signal) {
    std::cout << "\nShutting down server..." << std::endl;
    if (g_server) {
        g_server->stop();
    }
    exit(0);
}

std::map<uint64_t, std::vector<WebRTCSignal>> pending_signals;
std::mutex signals_mutex;


int main(int argc, char* argv[]) {
    std::cout << "========================================" << std::endl;
    std::cout << "  Meeting System Server Starting...    " << std::endl;
    std::cout << "========================================" << std::endl;
    
    // Parse arguments
    int port = 8080;
    if (argc > 1) {
        port = std::atoi(argv[1]);
    }
    
    try {
        // Initialize database
        std::cout << "\n[1/6] Initializing database..." << std::endl;
        DatabaseEngine db("meeting_system.db");
        
        // Check if database exists
        bool db_exists = db.open();
        if (!db_exists) {
            std::cout << "  Creating new database..." << std::endl;
            db.initialize();
        } else {
            std::cout << "  Loaded existing database" << std::endl;
        }
        
        // Initialize B-Trees
        std::cout << "\n[2/6] Initializing B-Trees..." << std::endl;
        BTree users_btree(&db);
        BTree meetings_btree(&db);
        
        if (!db_exists) {
            users_btree.initialize();
            meetings_btree.initialize();
            
            db.get_header().users_btree_root = users_btree.get_root_page_id();
            db.get_header().meetings_btree_root = meetings_btree.get_root_page_id();
            db.write_header();
        } else {
            users_btree.load(db.get_header().users_btree_root);
            meetings_btree.load(db.get_header().meetings_btree_root);
        }
        std::cout << "  Users B-Tree: root page " << users_btree.get_root_page_id() << std::endl;
        std::cout << "  Meetings B-Tree: root page " << meetings_btree.get_root_page_id() << std::endl;
        
        // Initialize Hash Tables
        std::cout << "\n[3/6] Initializing Hash Tables..." << std::endl;
        HashTable login_hash(&db);
        HashTable meeting_code_hash(&db);
        
        if (!db_exists) {
            login_hash.initialize();
            meeting_code_hash.initialize();
            
            db.get_header().login_hash_page = login_hash.get_header_page_id();
            db.get_header().meeting_code_hash_page = meeting_code_hash.get_header_page_id();
            db.write_header();
        } else {
            login_hash.load(db.get_header().login_hash_page);
            meeting_code_hash.load(db.get_header().meeting_code_hash_page);
        }
        std::cout << "  Login Hash Table: page " << login_hash.get_header_page_id() << std::endl;
        std::cout << "  Meeting Code Hash Table: page " << meeting_code_hash.get_header_page_id() << std::endl;
        
        // Initialize more B-Trees and Hash Tables
        std::cout << "\n[3/6] Initializing additional indexes..." << std::endl;
        BTree messages_btree(&db);
        BTree files_btree(&db);
        BTree whiteboard_btree(&db);
        HashTable chat_search_hash(&db);
        HashTable file_dedup_hash(&db);
        
        if (!db_exists) {
            messages_btree.initialize();
            files_btree.initialize();
            whiteboard_btree.initialize();
            chat_search_hash.initialize();
            file_dedup_hash.initialize();
            
            db.get_header().messages_btree_root = messages_btree.get_root_page_id();
            db.get_header().files_btree_root = files_btree.get_root_page_id();
            db.get_header().whiteboard_btree_root = whiteboard_btree.get_root_page_id();
            db.get_header().chat_search_hash_page = chat_search_hash.get_header_page_id();
            db.get_header().file_dedup_hash_page = file_dedup_hash.get_header_page_id();
            db.write_header();
        } else {
            messages_btree.load(db.get_header().messages_btree_root);
            files_btree.load(db.get_header().files_btree_root);
            whiteboard_btree.load(db.get_header().whiteboard_btree_root);
            chat_search_hash.load(db.get_header().chat_search_hash_page);
            file_dedup_hash.load(db.get_header().file_dedup_hash_page);
        }
        std::cout << "  Messages B-Tree: root page " << messages_btree.get_root_page_id() << std::endl;
        std::cout << "  Files B-Tree: root page " << files_btree.get_root_page_id() << std::endl;
        std::cout << "  Whiteboard B-Tree: root page " << whiteboard_btree.get_root_page_id() << std::endl;
        
        // Initialize Managers
        std::cout << "\n[4/6] Initializing Managers..." << std::endl;
        AuthManager auth_manager(&db, &users_btree, &login_hash);
        MeetingManager meeting_manager(&db, &meetings_btree, &meeting_code_hash);
        ChatManager chat_manager(&db, &messages_btree, &chat_search_hash);
        FileManager file_manager(&db, &files_btree, &file_dedup_hash);
        WhiteboardManager whiteboard_manager(&db, &whiteboard_btree);
        ScreenShareManager screen_share_manager;
        std::cout << "  All managers initialized (6 total)" << std::endl;
        
        // Create HTTP Server
        std::cout << "\n[5/6] Setting up HTTP routes..." << std::endl;
        HTTPServer server(port);
        g_server = &server;
        
        // Register signal handler
        signal(SIGINT, signal_handler);
        signal(SIGTERM, signal_handler);
        
        // ============ AUTH ROUTES ============
        
        // POST /api/v1/auth/register
        server.add_route("POST", "/api/v1/auth/register", 
            [&auth_manager](const HTTPRequest& req, HTTPResponse& res) {
                auto data = JSON::parse(req.body);
                
                std::string email = data["email"];
                std::string username = data["username"];
                std::string password = data["password"];
                
                User user;
                std::string error;
                
                if (auth_manager.register_user(email, username, password, user, error)) {
                    res.set_status(201, "Created");
                    res.set_json_body(JSON::success(
                        JSON::field("user_id", user.user_id) + "," +
                        JSON::field("username", user.username) + "," +
                        JSON::field("email", user.email)
                    ));
                } else {
                    res.set_status(400, "Bad Request");
                    res.set_json_body(JSON::error(error));
                }
            });
        
        // POST /api/v1/auth/login
        server.add_route("POST", "/api/v1/auth/login",
            [&auth_manager](const HTTPRequest& req, HTTPResponse& res) {
                auto data = JSON::parse(req.body);
                
                std::string email = data["email"];
                std::string password = data["password"];
                
                User user;
                std::string token, error;
                
                if (auth_manager.login(email, password, token, user, error)) {
                    res.set_json_body(JSON::success(
                        JSON::field("user_id", user.user_id) + "," +
                        JSON::field("username", user.username) + "," +
                        JSON::field("session_token", token) + "," +
                        JSON::field("expires_at", user.created_at + 86400)
                    ));
                } else {
                    res.set_status(401, "Unauthorized");
                    res.set_json_body(JSON::error(error));
                }
            });
        
        // POST /api/v1/auth/logout
        server.add_route("POST", "/api/v1/auth/logout",
            [&auth_manager](const HTTPRequest& req, HTTPResponse& res) {
                if (req.auth_token.empty()) {
                    res.set_status(401, "Unauthorized");
                    res.set_json_body(JSON::error("No token provided"));
                    return;
                }
                
                auth_manager.logout(req.auth_token);
                res.set_json_body(JSON::success(JSON::field("message", "Logged out successfully")));
            });
        
        // GET /api/v1/users/me
        server.add_route("GET", "/api/v1/users/me",
            [&auth_manager](const HTTPRequest& req, HTTPResponse& res) {
                uint64_t user_id;
                if (!auth_manager.verify_token(req.auth_token, user_id)) {
                    res.set_status(401, "Unauthorized");
                    res.set_json_body(JSON::error("Invalid or expired token"));
                    return;
                }
                
                User user;
                if (auth_manager.get_user_by_id(user_id, user)) {
                    std::cout << "DEBUG: user_id=" << user_id 
                    << ", username=" << user.username 
                    << ", email=" << user.email << std::endl;
                    res.set_json_body(JSON::success(
                        JSON::field("user", JSON::build(
                            JSON::field("user_id", user.user_id) + "," +
                            JSON::field("username", user.username) + "," +
                            JSON::field("email", user.email) + "," +
                            JSON::field("created_at", user.created_at)
                        ))
                    ));
                } else {
                    res.set_status(404, "Not Found");
                    res.set_json_body(JSON::error("User not found"));
                }
            });
        
        // ============ MEETING ROUTES ============
        
        // POST /api/v1/meetings/create
        // POST /api/v1/meetings/create
server.add_route("POST", "/api/v1/meetings/create",
    [&auth_manager, &meeting_manager](const HTTPRequest& req, HTTPResponse& res) {
        uint64_t user_id;
        if (!auth_manager.verify_token(req.auth_token, user_id)) {
            res.set_status(401, "Unauthorized");
            res.set_json_body(JSON::error("Invalid or expired token"));
            return;
        }
        
        auto data = JSON::parse(req.body);
        std::string title = data["title"];
        
        Meeting meeting;
        std::string error;
        
        if (meeting_manager.create_meeting(user_id, title, meeting, error)) {
            // ✅ ADD CREATOR AS PARTICIPANT
            meeting_manager.add_participant(meeting.meeting_id, user_id);
            
            res.set_status(201, "Created");
            res.set_json_body(JSON::success(
                JSON::field("meeting", JSON::build(
                    JSON::field("meeting_id", meeting.meeting_id) + "," +
                    JSON::field("meeting_code", meeting.meeting_code) + "," +
                    JSON::field("title", meeting.title) + "," +
                    JSON::field("creator_id", meeting.creator_id) + "," +
                    JSON::field("created_at", meeting.created_at) + "," +
                    JSON::field("is_active", meeting.is_active)
                ))
            ));
        } else {
            res.set_status(400, "Bad Request");
            res.set_json_body(JSON::error(error));
        }
    });
// POST /api/v1/meetings/join
server.add_route("POST", "/api/v1/meetings/join",
    [&auth_manager, &meeting_manager](const HTTPRequest& req, HTTPResponse& res) {
        uint64_t user_id;
        if (!auth_manager.verify_token(req.auth_token, user_id)) {
            res.set_status(401, "Unauthorized");
            res.set_json_body(JSON::error("Invalid or expired token"));
            return;
        }
        
        auto data = JSON::parse(req.body);
        std::string meeting_code = data["meeting_code"];
        
        Meeting meeting;
        std::string error;
        
        if (meeting_manager.join_meeting(meeting_code, user_id, meeting, error)) {
            std::string response = "{\"success\":true,\"meeting\":{";
            response += "\"meeting_id\":" + std::to_string(meeting.meeting_id) + ",";
            response += "\"title\":\"" + std::string(meeting.title) + "\",";
            response += "\"meeting_code\":\"" + std::string(meeting.meeting_code) + "\",";
            response += "\"creator_id\":" + std::to_string(meeting.creator_id) + ",";
            response += "\"is_active\":" + std::string(meeting.is_active ? "true" : "false");
            response += "}}";
            
            res.set_status(200, "OK");
            res.set_json_body(response);
        } else {
            res.set_status(404, "Not Found");
            res.set_json_body(JSON::error(error));
        }
    });
        
        // GET /api/v1/meetings/my-meetings
        server.add_route("GET", "/api/v1/meetings/my-meetings",
            [&auth_manager, &meeting_manager](const HTTPRequest& req, HTTPResponse& res) {
                uint64_t user_id;
                if (!auth_manager.verify_token(req.auth_token, user_id)) {
                    res.set_status(401, "Unauthorized");
                    res.set_json_body(JSON::error("Invalid or expired token"));
                    return;
                }
                
                auto meetings = meeting_manager.get_user_meetings(user_id);
                
                std::vector<std::string> meeting_objects;
                for (const auto& meeting : meetings) {
                    meeting_objects.push_back(JSON::build(
                        JSON::field("meeting_id", meeting.meeting_id) + "," +
                        JSON::field("title", meeting.title) + "," +
                        JSON::field("meeting_code", meeting.meeting_code) + "," +
                        JSON::field("created_at", meeting.created_at) + "," +
                        JSON::field("is_active", meeting.is_active)
                    ));
                }
                
                res.set_json_body(JSON::success(
                    JSON::field("meetings", JSON::array(meeting_objects))
                ));
            });
        
        // ============ CHAT ROUTES ============

// POST /api/v1/meetings/:id/messages
server.add_route("POST", "/api/v1/meetings/:id/messages",
    [&auth_manager, &chat_manager](const HTTPRequest& req, HTTPResponse& res) {
        uint64_t user_id;
        if (!auth_manager.verify_token(req.auth_token, user_id)) {
            res.set_status(401, "Unauthorized");
            res.set_json_body(JSON::error("Invalid or expired token"));
            return;
        }
        
        uint64_t meeting_id = std::stoull(req.path_params.at("id"));
        
        User user;
        auth_manager.get_user_by_id(user_id, user);
        
        auto data = JSON::parse(req.body);
        std::string content = data["content"];
        
        Message message;
        std::string error;
        
        if (chat_manager.send_message(meeting_id, user_id, user.username, content, message, error)) {
            res.set_status(201, "Created");
            res.set_json_body(JSON::success(
                JSON::field("message", JSON::build(
                    JSON::field("message_id", message.message_id) + "," +
                    JSON::field("user_id", message.user_id) + "," +
                    JSON::field("username", message.username) + "," +
                    JSON::field("content", message.content) + "," +
                    JSON::field("timestamp", message.timestamp)
                ))
            ));
        } else {
            res.set_status(400, "Bad Request");
            res.set_json_body(JSON::error(error));
        }
    });

// Helper function to parse meeting ID safely
auto parse_meeting_id = [](const std::map<std::string, std::string>& path_params, HTTPResponse& res) -> std::pair<bool, uint64_t> {
    try {
        uint64_t meeting_id = std::stoull(path_params.at("id"));
        return {true, meeting_id};
    } catch (const std::exception& e) {
        res.set_status(400, "Bad Request");
        res.set_json_body(JSON::error("Invalid meeting ID format"));
        return {false, 0};
    }
};

// Example: GET /api/v1/meetings/:id/messages
server.add_route("GET", "/api/v1/meetings/:id/messages",
    [&auth_manager, &chat_manager, parse_meeting_id](const HTTPRequest& req, HTTPResponse& res) {
        uint64_t user_id;
        if (!auth_manager.verify_token(req.auth_token, user_id)) {
            res.set_status(401, "Unauthorized");
            res.set_json_body(JSON::error("Invalid or expired token"));
            return;
        }
        
        auto [success, meeting_id] = parse_meeting_id(req.path_params, res);
        if (!success) return;  
        
        auto messages = chat_manager.get_messages(meeting_id, 50);
        
        std::vector<std::string> message_objects;
        for (const auto& msg : messages) {
            message_objects.push_back(JSON::build(
                JSON::field("message_id", msg.message_id) + "," +
                JSON::field("username", msg.username) + "," +
                JSON::field("content", msg.content) + "," +
                JSON::field("timestamp", msg.timestamp)
            ));
        }
        
        res.set_json_body(JSON::success(
            JSON::field("messages", JSON::array(message_objects))
        ));
    });

// ============ FILE ROUTES ============

// GET /api/v1/meetings/:id/files
server.add_route("GET", "/api/v1/meetings/:id/files",
    [&auth_manager, &file_manager, parse_meeting_id](const HTTPRequest& req, HTTPResponse& res) {
        uint64_t user_id;
        if (!auth_manager.verify_token(req.auth_token, user_id)) {
            res.set_status(401, "Unauthorized");
            res.set_json_body(JSON::error("Invalid or expired token"));
            return;
        }

        auto [success, meeting_id] = parse_meeting_id(req.path_params, res);
        if (!success) return;

        auto files = file_manager.get_meeting_files(meeting_id);
        std::vector<std::string> file_objects;
        for (const auto& f : files) {
            file_objects.push_back(JSON::build(
                JSON::field("file_id", f.file_id) + "," +
                JSON::field("filename", f.filename) + "," +
                JSON::field("file_size", f.file_size) + "," +
                JSON::field("uploaded_at", f.uploaded_at) + "," +
                JSON::field("uploader_id", f.uploader_id)
            ));
        }

        res.set_json_body(JSON::success(
            JSON::field("files", JSON::array(file_objects))
        ));
    });

// POST /api/v1/meetings/:id/files/upload
server.add_route("POST", "/api/v1/meetings/:id/files/upload",
    [&auth_manager, &file_manager](const HTTPRequest& req, HTTPResponse& res) {
        uint64_t user_id;
        if (!auth_manager.verify_token(req.auth_token, user_id)) {
            res.set_status(401, "Unauthorized");
            res.set_json_body(JSON::error("Invalid or expired token"));
            return;
        }
        
        uint64_t meeting_id = std::stoull(req.path_params.at("id"));
        
        try {
            std::cout << "DEBUG: upload handler received body size=" << req.body.size();
            if (req.headers.find("Content-Type") != req.headers.end()) {
                std::cout << ", Content-Type='" << req.headers.at("Content-Type") << "'";
            }
            if (req.headers.find("Transfer-Encoding") != req.headers.end()) {
                std::cout << ", Transfer-Encoding='" << req.headers.at("Transfer-Encoding") << "'";
            }
            std::cout << std::endl;

            auto make_escaped = [](const std::string& s, size_t maxlen) {
                std::string out;
                for (size_t i = 0; i < s.size() && out.size() < maxlen; ++i) {
                    unsigned char c = s[i];
                    if (c >= 32 && c <= 126) {
                        out += c;
                    } else if (c == '\n') {
                        out += "\\n";
                    } else if (c == '\r') {
                        out += "\\r";
                    } else if (c == '\t') {
                        out += "\\t";
                    } else {
                        out += '.'; // unprintable
                    }
                }
                return out;
            };

            if (!req.body.empty()) {
                std::string escaped = make_escaped(req.body, 200);
                std::cout << "DEBUG: upload preview (escaped)='" << escaped << "'" << std::endl;

                // Print first 64 bytes as hex for precise inspection
                std::ostringstream hexout;
                size_t hex_len = std::min<size_t>(req.body.size(), 64);
                hexout << std::hex << std::setfill('0');
                for (size_t i = 0; i < hex_len; ++i) {
                    hexout << std::setw(2) << (static_cast<unsigned int>(static_cast<unsigned char>(req.body[i]))) << " ";
                }
                std::cout << "DEBUG: upload preview (hex, first " << hex_len << " bytes)='" << hexout.str() << "'" << std::endl;
            }

            auto data = JSON::parse(req.body);

            std::string filename = data["filename"];
            std::string base64_data = data["data"];
            
            if (filename.empty() || base64_data.empty()) {
                res.set_status(400, "Bad Request");
                res.set_json_body(JSON::error("Filename and data are required"));
                return;
            }
            
            // Decode base64
            std::vector<uint8_t> file_data = decode_base64(base64_data);
            if (file_data.empty() && !base64_data.empty()) {
                res.set_status(400, "Bad Request");
                res.set_json_body(JSON::error("Failed to decode base64 data"));
                return;
            }
            
            FileRecord file;
            std::string error;
            
            if (file_manager.upload_file(meeting_id, user_id, filename, 
                                         file_data.data(), file_data.size(), 
                                         file, error)) {
                res.set_status(201, "Created");
                res.set_json_body(JSON::success(
                    JSON::field("file_id", file.file_id) + "," +
                    JSON::field("filename", file.filename) + "," +
                    JSON::field("file_size", file.file_size) + "," +
                    JSON::field("uploaded_at", file.uploaded_at)
                ));
            } else {
                res.set_status(400, "Bad Request");
                res.set_json_body(JSON::error(error));
            }
        } catch (const std::exception& e) {
            res.set_status(400, "Bad Request");
            res.set_json_body(JSON::error(std::string("Invalid request: ") + e.what()));
        }
    });

// GET /api/v1/meetings/:id/files/:file_id/download
server.add_route("GET", "/api/v1/meetings/:id/files/:file_id/download",
    [&auth_manager, &file_manager](const HTTPRequest& req, HTTPResponse& res) {
        uint64_t user_id;
        if (!auth_manager.verify_token(req.auth_token, user_id)) {
            res.set_status(401, "Unauthorized");
            res.set_json_body(JSON::error("Invalid or expired token"));
            return;
        }
        
        try {
            uint64_t file_id = std::stoull(req.path_params.at("file_id"));
            
            std::vector<uint8_t> file_data;
            FileRecord file;
            std::string error;
            
            if (file_manager.download_file(file_id, file_data, file, error)) {
                // Encode to base64 for JSON transport
                std::string base64_data = encode_base64(file_data);
                
                res.set_json_body(JSON::success(
                    JSON::field("filename", file.filename) + "," +
                    JSON::field("data", base64_data) + "," +
                    JSON::field("file_size", file.file_size)
                ));
            } else {
                res.set_status(404, "Not Found");
                res.set_json_body(JSON::error(error));
            }
        } catch (const std::exception& e) {
            res.set_status(400, "Bad Request");
            res.set_json_body(JSON::error("Invalid file ID"));
        }
    });

// DELETE /api/v1/meetings/:id/files/:file_id
server.add_route("DELETE", "/api/v1/meetings/:id/files/:file_id",
    [&auth_manager, &file_manager](const HTTPRequest& req, HTTPResponse& res) {
        uint64_t user_id;
        if (!auth_manager.verify_token(req.auth_token, user_id)) {
            res.set_status(401, "Unauthorized");
            res.set_json_body(JSON::error("Invalid or expired token"));
            return;
        }
        
        try {
            uint64_t file_id = std::stoull(req.path_params.at("file_id"));
            
            std::string error;
            if (file_manager.delete_file(file_id, error)) {
                res.set_json_body(JSON::success(
                    JSON::field("message", "File deleted successfully")
                ));
            } else {
                res.set_status(400, "Bad Request");
                res.set_json_body(JSON::error(error));
            }
        } catch (const std::exception& e) {
            res.set_status(400, "Bad Request");
            res.set_json_body(JSON::error("Invalid file ID"));
        }
    });


// ============ WHITEBOARD ROUTES ============

// POST /api/v1/meetings/:id/whiteboard/draw
server.add_route("POST", "/api/v1/meetings/:id/whiteboard/draw",
    [&auth_manager, &whiteboard_manager](const HTTPRequest& req, HTTPResponse& res) {
        uint64_t user_id;
        if (!auth_manager.verify_token(req.auth_token, user_id)) {
            res.set_status(401, "Unauthorized");
            res.set_json_body(JSON::error("Invalid or expired token"));
            return;
        }
        
        uint64_t meeting_id = std::stoull(req.path_params.at("id"));
        
        try {
            auto data = JSON::parse(req.body);
            
            uint8_t element_type = 0;
            int16_t x1 = 0, y1 = 0, x2 = 0, y2 = 0;
            
            if (data.find("element_type") != data.end() && !data["element_type"].empty()) {
                element_type = std::stoi(data["element_type"]);
            }
            if (data.find("x1") != data.end() && !data["x1"].empty()) {
                x1 = std::stoi(data["x1"]);
            }
            if (data.find("y1") != data.end() && !data["y1"].empty()) {
                y1 = std::stoi(data["y1"]);
            }
            if (data.find("x2") != data.end() && !data["x2"].empty()) {
                x2 = std::stoi(data["x2"]);
            }
            if (data.find("y2") != data.end() && !data["y2"].empty()) {
                y2 = std::stoi(data["y2"]);
            }
            
            WhiteboardElement element;
            std::string error;
            
            if (whiteboard_manager.draw_element(meeting_id, user_id, element_type, x1, y1, x2, y2,
                                                 255, 0, 0, 3, "", element, error)) {
                res.set_status(201, "Created");
                res.set_json_body(JSON::success(
                    JSON::field("element_id", element.element_id) + "," +
                    JSON::field("element_type", (int)element.element_type) + "," +
                    JSON::field("timestamp", element.timestamp)
                ));
            } else {
                res.set_status(400, "Bad Request");
                res.set_json_body(JSON::error(error));
            }
        } catch (const std::exception& e) {
            res.set_status(400, "Bad Request");
            res.set_json_body(JSON::error("Invalid JSON or numeric values"));
        }
    });

// GET /api/v1/meetings/:id/whiteboard/elements
server.add_route("GET", "/api/v1/meetings/:id/whiteboard/elements",
    [&auth_manager, &whiteboard_manager](const HTTPRequest& req, HTTPResponse& res) {
        uint64_t user_id;
        if (!auth_manager.verify_token(req.auth_token, user_id)) {
            res.set_status(401, "Unauthorized");
            res.set_json_body(JSON::error("Invalid or expired token"));
            return;
        }
        
        uint64_t meeting_id = std::stoull(req.path_params.at("id"));
        
        auto elements = whiteboard_manager.get_meeting_elements(meeting_id);
        
        std::vector<std::string> element_objects;
        for (const auto& elem : elements) {
            element_objects.push_back(JSON::build(
                JSON::field("element_id", elem.element_id) + "," +
                JSON::field("element_type", (int)elem.element_type) + "," +
                JSON::field("x1", elem.x1) + "," +
                JSON::field("y1", elem.y1) + "," +
                JSON::field("x2", elem.x2) + "," +
                JSON::field("y2", elem.y2)
            ));
        }
        
        res.set_json_body(JSON::success(
            JSON::field("elements", JSON::array(element_objects))
        ));
    });


    // ============ WEBRTC SIGNALING ROUTES ============

// POST /api/v1/meetings/:id/webrtc/signal
// Send WebRTC signaling message (offer/answer/ICE candidate)
server.add_route("POST", "/api/v1/meetings/:id/webrtc/signal",
    [&auth_manager](const HTTPRequest& req, HTTPResponse& res) {
        uint64_t user_id;
        if (!auth_manager.verify_token(req.auth_token, user_id)) {
            res.set_status(401, "Unauthorized");
            res.set_json_body(JSON::error("Invalid or expired token"));
            return;
        }
        
        try {
            auto data = JSON::parse(req.body);
            
            WebRTCSignal signal;
            signal.type = data["type"];
            signal.from_user_id = user_id;
            signal.timestamp = std::time(nullptr);
            
            // Parse target user
            if (data.find("to") != data.end() && !data["to"].empty()) {
                signal.to_user_id = std::stoull(data["to"]);
            } else {
                res.set_status(400, "Bad Request");
                res.set_json_body(JSON::error("Missing 'to' field"));
                return;
            }
            
            // Parse type-specific fields
            if (signal.type == "offer" || signal.type == "answer") {
                if (data.find("sdp") != data.end()) {
                    signal.sdp = data["sdp"];
                }
            } else if (signal.type == "ice-candidate") {
                if (data.find("candidate") != data.end()) {
                    signal.candidate = data["candidate"];
                }
                if (data.find("sdpMid") != data.end()) {
                    signal.sdp_mid = data["sdpMid"];
                }
                if (data.find("sdpMLineIndex") != data.end()) {
                    signal.sdp_m_line_index = std::stoi(data["sdpMLineIndex"]);
                }
            }
            
            
            {
                std::lock_guard<std::mutex> lock(signals_mutex);
                pending_signals[signal.to_user_id].push_back(signal);
            }
            
            std::cout << "WebRTC signal queued: " << signal.type 
                      << " from user " << signal.from_user_id 
                      << " to user " << signal.to_user_id << std::endl;
            
            res.set_json_body(JSON::success(
                JSON::field("message", "Signal queued")
            ));
            
        } catch (const std::exception& e) {
            res.set_status(400, "Bad Request");
            res.set_json_body(JSON::error(std::string("Invalid signal: ") + e.what()));
        }
    });

// GET /api/v1/meetings/:id/webrtc/signals
// GET /api/v1/meetings/:id/webrtc/signals
server.add_route("GET", "/api/v1/meetings/:id/webrtc/signals",
    [&auth_manager](const HTTPRequest& req, HTTPResponse& res) {
        uint64_t user_id;
        if (!auth_manager.verify_token(req.auth_token, user_id)) {
            res.set_status(401, "Unauthorized");
            res.set_json_body(JSON::error("Invalid or expired token"));
            return;
        }
        
        std::vector<WebRTCSignal> user_signals;
        
        {
            std::lock_guard<std::mutex> lock(signals_mutex);
            auto it = pending_signals.find(user_id);
            if (it != pending_signals.end()) {
                user_signals = it->second;
                pending_signals.erase(it);
            }
        }
        
        // Serialize signals to JSON array
        std::vector<std::string> signal_jsons;
        for (const auto& sig : user_signals) {
            signal_jsons.push_back(serialize_signal(sig));
        }
        
        std::string signals_array = "[";
        for (size_t i = 0; i < signal_jsons.size(); ++i) {
            signals_array += signal_jsons[i];
            if (i < signal_jsons.size() - 1) {
                signals_array += ",";
            }
        }
        signals_array += "]";
        
        // ✅ USE raw_field() FOR RAW JSON
        res.set_json_body(JSON::success(
            JSON::raw_field("signals", signals_array)
        ));
    });

// GET /api/v1/meetings/:id/participants
// Get list of participants for peer discovery
server.add_route("GET", "/api/v1/meetings/:id/participants",
    [&auth_manager, &meeting_manager](const HTTPRequest& req, HTTPResponse& res) {
        uint64_t user_id;
        if (!auth_manager.verify_token(req.auth_token, user_id)) {
            res.set_status(401, "Unauthorized");
            res.set_json_body(JSON::error("Invalid or expired token"));
            return;
        }
        
        uint64_t meeting_id = std::stoull(req.path_params.at("id"));
        
        
        auto participants = meeting_manager.get_participants(meeting_id);
        
        std::vector<std::string> participant_objects;
        for (const auto& participant : participants) {
            User user;
            if (auth_manager.get_user_by_id(participant.user_id, user)) {
                participant_objects.push_back(JSON::build(
                    JSON::field("user_id", user.user_id) + "," +
                    JSON::field("username", user.username) + "," +
                    JSON::field("joined_at", participant.joined_at)
                ));
            }
        }
        
        res.set_json_body(JSON::success(
            JSON::field("participants", JSON::array(participant_objects))
        ));
    });

// ============ SCREEN SHARE ROUTES ============

// POST /api/v1/meetings/:id/screenshare/start
server.add_route("POST", "/api/v1/meetings/:id/screenshare/start",
    [&auth_manager, &screen_share_manager](const HTTPRequest& req, HTTPResponse& res) {
        uint64_t user_id;
        if (!auth_manager.verify_token(req.auth_token, user_id)) {
            res.set_status(401, "Unauthorized");
            res.set_json_body(JSON::error("Invalid or expired token"));
            return;
        }
        
        uint64_t meeting_id = std::stoull(req.path_params.at("id"));
        
        User user;
        auth_manager.get_user_by_id(user_id, user);
        
        std::string stream_id;
        if (screen_share_manager.start_screen_share(meeting_id, user_id, user.username, stream_id)) {
            res.set_json_body(JSON::success(
                JSON::field("stream_id", stream_id) + "," +
                JSON::field("upload_url", "ws://localhost:8081/screenshare/upload/" + stream_id)
            ));
        } else {
            res.set_status(400, "Bad Request");
            res.set_json_body(JSON::error("Failed to start screen share"));
        }
    });

// POST /api/v1/meetings/:id/screenshare/stop
server.add_route("POST", "/api/v1/meetings/:id/screenshare/stop",
    [&auth_manager, &screen_share_manager](const HTTPRequest& req, HTTPResponse& res) {
        uint64_t user_id;
        if (!auth_manager.verify_token(req.auth_token, user_id)) {
            res.set_status(401, "Unauthorized");
            res.set_json_body(JSON::error("Invalid or expired token"));
            return;
        }
        
        uint64_t meeting_id = std::stoull(req.path_params.at("id"));
        
        if (screen_share_manager.stop_screen_share(meeting_id, user_id)) {
            res.set_json_body(JSON::success(
                JSON::field("message", "Screen share stopped")
            ));
        } else {
            res.set_status(400, "Bad Request");
            res.set_json_body(JSON::error("Failed to stop screen share"));
        }
    });

    // Add this route AFTER the existing screenshare routes in main.cpp

// POST /api/v1/meetings/:id/screenshare/frame
// Upload screen share frame (JPEG data)
server.add_route("POST", "/api/v1/meetings/:id/screenshare/frame",
    [&auth_manager, &screen_share_manager](const HTTPRequest& req, HTTPResponse& res) {
        uint64_t user_id;
        if (!auth_manager.verify_token(req.auth_token, user_id)) {
            res.set_status(401, "Unauthorized");
            res.set_json_body(JSON::error("Invalid or expired token"));
            return;
        }
        
        uint64_t meeting_id = std::stoull(req.path_params.at("id"));
        
        try {
            auto data = JSON::parse(req.body);
            
            // Get base64 JPEG data
            std::string base64_jpeg = data["frame"];
            uint32_t width = 0;
            uint32_t height = 0;
            
            if (data.find("width") != data.end() && !data["width"].empty()) {
                width = std::stoul(data["width"]);
            }
            if (data.find("height") != data.end() && !data["height"].empty()) {
                height = std::stoul(data["height"]);
            }
            
            if (base64_jpeg.empty()) {
                res.set_status(400, "Bad Request");
                res.set_json_body(JSON::error("Frame data is required"));
                return;
            }
            
            // Decode base64 to JPEG bytes
            std::vector<uint8_t> jpeg_data = decode_base64(base64_jpeg);
            
            if (jpeg_data.empty()) {
                res.set_status(400, "Bad Request");
                res.set_json_body(JSON::error("Failed to decode frame data"));
                return;
            }
            
            // Upload frame to ScreenShareManager
            if (screen_share_manager.upload_frame(meeting_id, user_id, jpeg_data, width, height)) {
                res.set_json_body(JSON::success(
                    JSON::field("message", "Frame uploaded") + "," +
                    JSON::field("size", static_cast<uint64_t>(jpeg_data.size()))
                ));
            } else {
                res.set_status(400, "Bad Request");
                res.set_json_body(JSON::error("Failed to upload frame. Start screen share first."));
            }
            
        } catch (const std::exception& e) {
            res.set_status(400, "Bad Request");
            res.set_json_body(JSON::error(std::string("Invalid request: ") + e.what()));
        }
    });

// GET /api/v1/meetings/:id/screenshare/frame
// Get latest screen share frame
server.add_route("GET", "/api/v1/meetings/:id/screenshare/frame",
    [&auth_manager, &screen_share_manager](const HTTPRequest& req, HTTPResponse& res) {
        uint64_t user_id;
        if (!auth_manager.verify_token(req.auth_token, user_id)) {
            res.set_status(401, "Unauthorized");
            res.set_json_body(JSON::error("Invalid or expired token"));
            return;
        }
        
        uint64_t meeting_id = std::stoull(req.path_params.at("id"));
        
        ScreenFrame frame;
        if (screen_share_manager.get_latest_frame(meeting_id, frame)) {
            // Encode JPEG to base64 for JSON transport
            std::string base64_jpeg = encode_base64(frame.jpeg_data);
            
            res.set_json_body(JSON::success(
                JSON::field("frame", base64_jpeg) + "," +
                JSON::field("width", static_cast<uint64_t>(frame.width)) + "," +
                JSON::field("height", static_cast<uint64_t>(frame.height)) + "," +
                JSON::field("timestamp", frame.timestamp) + "," +
                JSON::field("user_id", frame.user_id) + "," +
                JSON::field("username", frame.username)
            ));
        } else {
            res.set_status(404, "Not Found");
            res.set_json_body(JSON::error("No active screen share"));
        }
    });

// DELETE /api/v1/meetings/:id/whiteboard/clear
server.add_route("DELETE", "/api/v1/meetings/:id/whiteboard/clear",
    [&auth_manager, &whiteboard_manager](const HTTPRequest& req, HTTPResponse& res) {
        uint64_t user_id;
        if (!auth_manager.verify_token(req.auth_token, user_id)) {
            res.set_status(401, "Unauthorized");
            res.set_json_body(JSON::error("Invalid or expired token"));
            return;
        }
        
        uint64_t meeting_id = std::stoull(req.path_params.at("id"));
        
        std::string error;
        if (whiteboard_manager.clear_whiteboard(meeting_id, error)) {
            res.set_json_body(JSON::success(
                JSON::field("message", "Whiteboard cleared")
            ));
        } else {
            res.set_status(400, "Bad Request");
            res.set_json_body(JSON::error(error));
        }
    });

// POST version for browsers that don't support DELETE
server.add_route("POST", "/api/v1/meetings/:id/whiteboard/clear",
    [&auth_manager, &whiteboard_manager](const HTTPRequest& req, HTTPResponse& res) {
        uint64_t user_id;
        if (!auth_manager.verify_token(req.auth_token, user_id)) {
            res.set_status(401, "Unauthorized");
            res.set_json_body(JSON::error("Invalid or expired token"));
            return;
        }
        
        uint64_t meeting_id = std::stoull(req.path_params.at("id"));
        
        std::string error;
        if (whiteboard_manager.clear_whiteboard(meeting_id, error)) {
            res.set_json_body(JSON::success(
                JSON::field("message", "Whiteboard cleared")
            ));
        } else {
            res.set_status(400, "Bad Request");
            res.set_json_body(JSON::error(error));
        }
    });
        
        // Health check
        server.add_route("GET", "/health",
            [](const HTTPRequest& req, HTTPResponse& res) {
                res.set_json_body("{\"status\":\"ok\",\"service\":\"MeetingSystem\"}");
            });
        
        std::cout << "  Registered " << 20 << " routes" << std::endl;
        
        // Start server
        std::cout << "\n[6/6] Starting HTTP server on port " << port << "..." << std::endl;
        std::cout << "\n========================================" << std::endl;
        std::cout << "  Server is running!                    " << std::endl;
        std::cout << "  Visit: http://localhost:" << port << "/health" << std::endl;
        std::cout << "  Press Ctrl+C to stop                  " << std::endl;
        std::cout << "========================================\n" << std::endl;
        
        server.start();
        
    } catch (const std::exception& e) {
        std::cerr << "FATAL ERROR: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}