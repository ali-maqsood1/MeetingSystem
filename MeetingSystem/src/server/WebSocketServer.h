#ifndef WEBSOCKET_SERVER_H
#define WEBSOCKET_SERVER_H

#include <boost/asio.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <string>
#include <map>
#include <set>
#include <memory>
#include <mutex>
#include <thread>

namespace beast = boost::beast;
namespace websocket = beast::websocket;
using tcp = boost::asio::ip::tcp;

// WebSocket session (one per client)
class WebSocketSession : public std::enable_shared_from_this<WebSocketSession> {
private:
    websocket::stream<tcp::socket> ws;
    beast::flat_buffer buffer;
    uint64_t meeting_id;
    uint64_t user_id;
    std::string username;
    
    std::function<void(uint64_t, const std::string&)> on_message;
    std::function<void(uint64_t, uint64_t)> on_disconnect;
    
public:
    WebSocketSession(tcp::socket socket, uint64_t mid, uint64_t uid, const std::string& uname)
        : ws(std::move(socket)), meeting_id(mid), user_id(uid), username(uname) {}
    
    void set_callbacks(
        std::function<void(uint64_t, const std::string&)> msg_callback,
        std::function<void(uint64_t, uint64_t)> disconnect_callback) {
        on_message = msg_callback;
        on_disconnect = disconnect_callback;
    }
    
    void start();
    void send(const std::string& message);
    
    uint64_t get_meeting_id() const { return meeting_id; }
    uint64_t get_user_id() const { return user_id; }
    std::string get_username() const { return username; }
    
private:
    void do_read();
    void do_write(std::shared_ptr<std::string const> const& message);
};

// WebSocket server manager
class WebSocketManager {
private:
    boost::asio::io_context& io_context;
    tcp::acceptor acceptor;
    
    // Active sessions: meeting_id -> set of sessions
    std::map<uint64_t, std::set<std::shared_ptr<WebSocketSession>>> meeting_rooms;
    std::mutex rooms_mutex;
    
    std::function<void(uint64_t, uint64_t, const std::string&)> message_handler;
    
public:
    WebSocketManager(boost::asio::io_context& ioc, int port)
        : io_context(ioc), acceptor(ioc, tcp::endpoint(tcp::v4(), port)) {}
    
    void set_message_handler(std::function<void(uint64_t, uint64_t, const std::string&)> handler) {
        message_handler = handler;
    }
    
    void start_accept();
    
    // Add client to meeting room
    void add_to_room(uint64_t meeting_id, std::shared_ptr<WebSocketSession> session);
    
    // Remove client from meeting room
    void remove_from_room(uint64_t meeting_id, uint64_t user_id);
    
    // Broadcast message to all clients in a meeting room
    void broadcast_to_room(uint64_t meeting_id, const std::string& message, 
                          uint64_t exclude_user_id = 0);
    
    // Get participant count
    int get_room_size(uint64_t meeting_id);
    
private:
    void handle_accept(std::shared_ptr<WebSocketSession> session);
};

#endif // WEBSOCKET_SERVER_H