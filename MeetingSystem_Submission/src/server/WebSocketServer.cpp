#include "WebSocketServer.h"
#include <iostream>

void WebSocketSession::start() {
    ws.async_accept([self = shared_from_this()](beast::error_code ec) {
        if (ec) {
            std::cerr << "WebSocket accept error: " << ec.message() << std::endl;
            return;
        }
        
        std::cout << "WebSocket connected: " << self->username 
                  << " to meeting " << self->meeting_id << std::endl;
        
        self->do_read();
    });
}

void WebSocketSession::send(const std::string& message) {
    auto msg = std::make_shared<std::string const>(message);
    
    ws.async_write(
        boost::asio::buffer(*msg),
        [self = shared_from_this(), msg](beast::error_code ec, std::size_t) {
            if (ec) {
                std::cerr << "WebSocket write error: " << ec.message() << std::endl;
            }
        });
}

void WebSocketSession::do_read() {
    ws.async_read(
        buffer,
        [self = shared_from_this()](beast::error_code ec, std::size_t bytes) {
            if (ec) {
                if (ec == websocket::error::closed) {
                    std::cout << "WebSocket closed: " << self->username << std::endl;
                } else {
                    std::cerr << "WebSocket read error: " << ec.message() << std::endl;
                }
                
                if (self->on_disconnect) {
                    self->on_disconnect(self->meeting_id, self->user_id);
                }
                return;
            }
            
            // Process message
            std::string message = beast::buffers_to_string(self->buffer.data());
            
            if (self->on_message) {
                self->on_message(self->meeting_id, message);
            }
            
            self->buffer.consume(self->buffer.size());
            self->do_read();
        });
}

void WebSocketManager::start_accept() {
    acceptor.async_accept([this](beast::error_code ec, tcp::socket socket) {
        if (!ec) {
            // For simplicity, we'll extract meeting_id and user_id from the socket
            // In production, you'd parse the WebSocket handshake URL
            // For now, we'll create a dummy session
            // This should be handled by HTTP server upgrading connections
            std::cout << "WebSocket connection accepted" << std::endl;
        }
        
        start_accept();
    });
}

void WebSocketManager::add_to_room(uint64_t meeting_id, 
                                     std::shared_ptr<WebSocketSession> session) {
    std::lock_guard<std::mutex> lock(rooms_mutex);
    meeting_rooms[meeting_id].insert(session);
    
    std::cout << "Added " << session->get_username() 
              << " to meeting room " << meeting_id 
              << " (total: " << meeting_rooms[meeting_id].size() << ")" << std::endl;
    
    // Set callbacks
    session->set_callbacks(
        [this, meeting_id](uint64_t, const std::string& msg) {
            if (message_handler) {
                // Extract user_id from session (simplified)
                message_handler(meeting_id, 0, msg);
            }
        },
        [this](uint64_t mid, uint64_t uid) {
            remove_from_room(mid, uid);
        }
    );
}

void WebSocketManager::remove_from_room(uint64_t meeting_id, uint64_t user_id) {
    std::lock_guard<std::mutex> lock(rooms_mutex);
    
    auto room_it = meeting_rooms.find(meeting_id);
    if (room_it != meeting_rooms.end()) {
        auto& sessions = room_it->second;
        
        // Find and remove session
        for (auto it = sessions.begin(); it != sessions.end(); ++it) {
            if ((*it)->get_user_id() == user_id) {
                std::cout << "Removed " << (*it)->get_username() 
                          << " from meeting room " << meeting_id << std::endl;
                sessions.erase(it);
                break;
            }
        }
        
        // Remove empty rooms
        if (sessions.empty()) {
            meeting_rooms.erase(room_it);
        }
    }
}

void WebSocketManager::broadcast_to_room(uint64_t meeting_id, 
                                          const std::string& message,
                                          uint64_t exclude_user_id) {
    std::lock_guard<std::mutex> lock(rooms_mutex);
    
    auto room_it = meeting_rooms.find(meeting_id);
    if (room_it != meeting_rooms.end()) {
        for (auto& session : room_it->second) {
            if (session->get_user_id() != exclude_user_id) {
                session->send(message);
            }
        }
    }
}

int WebSocketManager::get_room_size(uint64_t meeting_id) {
    std::lock_guard<std::mutex> lock(rooms_mutex);
    
    auto room_it = meeting_rooms.find(meeting_id);
    if (room_it != meeting_rooms.end()) {
        return room_it->second.size();
    }
    return 0;
}