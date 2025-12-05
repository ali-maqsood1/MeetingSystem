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
#include <iostream>
#include <memory>
#include <signal.h>

// Global server pointer for signal handling
HTTPServer* g_server = nullptr;

void signal_handler(int signal) {
    std::cout << "\nShutting down server..." << std::endl;
    if (g_server) {
        g_server->stop();
    }
    exit(0);
}

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
        // POST /api/v1/meetings/join
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
            // ✅ BUILD NESTED OBJECT MANUALLY WITHOUT JSON::build()
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
        
        // ✅ SAFE PARSING
        auto [success, meeting_id] = parse_meeting_id(req.path_params, res);
        if (!success) return;  // Error already set in response
        
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
    [&auth_manager, &file_manager](const HTTPRequest& req, HTTPResponse& res) {
        uint64_t user_id;
        if (!auth_manager.verify_token(req.auth_token, user_id)) {
            res.set_status(401, "Unauthorized");
            res.set_json_body(JSON::error("Invalid or expired token"));
            return;
        }
        
        uint64_t meeting_id = std::stoull(req.path_params.at("id"));
        
        auto files = file_manager.get_meeting_files(meeting_id);
        
        std::vector<std::string> file_objects;
        for (const auto& file : files) {
            file_objects.push_back(JSON::build(
                JSON::field("file_id", file.file_id) + "," +
                JSON::field("filename", file.filename) + "," +
                JSON::field("file_size", file.file_size) + "," +
                JSON::field("uploaded_at", file.uploaded_at)
            ));
        }
        
        res.set_json_body(JSON::success(
            JSON::field("files", JSON::array(file_objects))
        ));
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
        
        std::cout << "  Registered " << 17 << " routes" << std::endl;
        
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