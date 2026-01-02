#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include <boost/asio.hpp>
#include <string>
#include <map>
#include <functional>
#include <memory>
#include <thread>
#include <vector>

using boost::asio::ip::tcp;

// HTTP Request structure
struct HTTPRequest {
    std::string method;
    std::string path;
    std::string version;
    std::map<std::string, std::string> headers;
    std::map<std::string, std::string> query_params;
    std::map<std::string, std::string> path_params;  
    std::string body;
    
    // Parsed from Authorization header
    std::string auth_token;
    
    HTTPRequest() {}
};

// HTTP Response structure
struct HTTPResponse {
    int status_code;
    std::string status_message;
    std::map<std::string, std::string> headers;
    std::string body;
    
    HTTPResponse() : status_code(200), status_message("OK") {
        headers["Content-Type"] = "application/json";
        headers["Server"] = "MeetingSystem/1.0";
    }
    
    void set_json_body(const std::string& json) {
        body = json;
        headers["Content-Type"] = "application/json";
        headers["Content-Length"] = std::to_string(body.length());
    }
    
    void set_status(int code, const std::string& message) {
        status_code = code;
        status_message = message;
    }
    
    std::string to_string() const {
        std::string response = "HTTP/1.1 " + std::to_string(status_code) + " " + status_message + "\r\n";
        
        for (const auto& header : headers) {
            response += header.first + ": " + header.second + "\r\n";
        }
        
        response += "\r\n";
        response += body;
        
        return response;
    }
};

// Route handler function type
using RouteHandler = std::function<void(HTTPRequest&, HTTPResponse&)>;

// HTTP Connection handler
class HTTPConnection : public std::enable_shared_from_this<HTTPConnection> {
private:
    tcp::socket socket;
    boost::asio::streambuf buffer;
    std::function<void(HTTPRequest&, HTTPResponse&)> request_handler;
    
public:
    HTTPConnection(tcp::socket sock, std::function<void(HTTPRequest&, HTTPResponse&)> handler)
        : socket(std::move(sock)), request_handler(handler) {}
    
    void start() {
        read_request();
    }
    
private:
    void read_request();
    void write_response(const HTTPResponse& response);
    HTTPRequest parse_request(const std::string& raw_request);
    std::map<std::string, std::string> parse_query_string(const std::string& query);
};

// Main HTTP Server
class HTTPServer {
private:
    boost::asio::io_context io_context;
    tcp::acceptor acceptor;
    std::map<std::string, std::map<std::string, RouteHandler>> routes; // method -> path -> handler
    std::vector<std::thread> thread_pool;
    int thread_count;
    
public:
    HTTPServer(int port, int threads = 4);
    ~HTTPServer();
    
    // Register routes
    void add_route(const std::string& method, const std::string& path, RouteHandler handler);
    
    // Start server
    void start();
    void stop();
    
private:
    void accept_connections();
    void handle_request(HTTPRequest& req, HTTPResponse& res);
    bool match_route(const std::string& method, const std::string& path, 
                             std::map<std::string, std::string>& path_params, 
                             RouteHandler& handler);
};

#endif // HTTP_SERVER_H



