#include "server/HTTPServer.h"
#include <iostream>
#include <sstream>
#include <algorithm>

// HTTPConnection implementation
void HTTPConnection::read_request() {
    auto self = shared_from_this();
    boost::asio::async_read_until(socket, buffer, "\r\n\r\n",
        [this, self](boost::system::error_code ec, std::size_t bytes) {
            if (!ec) {
                std::istream stream(&buffer);
                std::string raw_request;
                std::string line;
                
                // Read all data
                while (std::getline(stream, line)) {
                    raw_request += line + "\n";
                }
                
                // Parse request
                HTTPRequest request = parse_request(raw_request);
                
                // Handle request
                HTTPResponse response;
                request_handler(request, response);
                
                // Write response
                write_response(response);
            } else {
                std::cerr << "Error reading request: " << ec.message() << std::endl;
            }
        });
}

void HTTPConnection::write_response(const HTTPResponse& response) {
    auto self = shared_from_this();
    auto response_str = std::make_shared<std::string>(response.to_string());
    
    boost::asio::async_write(socket, boost::asio::buffer(*response_str),
        [this, self, response_str](boost::system::error_code ec, std::size_t) {
            if (ec) {
                std::cerr << "Error writing response: " << ec.message() << std::endl;
            }
            socket.close();
        });
}

HTTPRequest HTTPConnection::parse_request(const std::string& raw_request) {
    HTTPRequest request;
    std::istringstream stream(raw_request);
    std::string line;
    
    // Parse request line
    if (std::getline(stream, line)) {
        std::istringstream line_stream(line);
        std::string path_with_query;
        line_stream >> request.method >> path_with_query >> request.version;
        
        // Split path and query string
        size_t query_pos = path_with_query.find('?');
        if (query_pos != std::string::npos) {
            request.path = path_with_query.substr(0, query_pos);
            std::string query = path_with_query.substr(query_pos + 1);
            request.query_params = parse_query_string(query);
        } else {
            request.path = path_with_query;
        }
    }
    
    // Parse headers
    while (std::getline(stream, line) && line != "\r" && !line.empty()) {
        size_t colon_pos = line.find(':');
        if (colon_pos != std::string::npos) {
            std::string key = line.substr(0, colon_pos);
            std::string value = line.substr(colon_pos + 1);
            
            // Trim whitespace
            value.erase(0, value.find_first_not_of(" \t\r\n"));
            value.erase(value.find_last_not_of(" \t\r\n") + 1);
            
            request.headers[key] = value;
        }
    }
    
    // Parse Authorization header
    auto auth_it = request.headers.find("Authorization");
    if (auth_it != request.headers.end()) {
        std::string auth = auth_it->second;
        if (auth.substr(0, 7) == "Bearer ") {
            request.auth_token = auth.substr(7);
        }
    }
    
    // Read body (if any)
    auto content_length_it = request.headers.find("Content-Length");
    if (content_length_it != request.headers.end()) {
        int content_length = std::stoi(content_length_it->second);
        request.body.resize(content_length);
        stream.read(&request.body[0], content_length);
    }
    
    return request;
}

std::map<std::string, std::string> HTTPConnection::parse_query_string(const std::string& query) {
    std::map<std::string, std::string> params;
    std::istringstream stream(query);
    std::string pair;
    
    while (std::getline(stream, pair, '&')) {
        size_t eq_pos = pair.find('=');
        if (eq_pos != std::string::npos) {
            std::string key = pair.substr(0, eq_pos);
            std::string value = pair.substr(eq_pos + 1);
            params[key] = value;
        }
    }
    
    return params;
}

// HTTPServer implementation
HTTPServer::HTTPServer(int port, int threads) 
    : acceptor(io_context, tcp::endpoint(tcp::v4(), port)),
      thread_count(threads) {
    std::cout << "HTTP Server initializing on port " << port << std::endl;
}

HTTPServer::~HTTPServer() {
    stop();
}

void HTTPServer::add_route(const std::string& method, const std::string& path, RouteHandler handler) {
    routes[method][path] = handler;
    std::cout << "Route registered: " << method << " " << path << std::endl;
}

void HTTPServer::start() {
    std::cout << "Starting HTTP Server..." << std::endl;
    
    // Start accepting connections
    accept_connections();
    
    // Create thread pool
    for (int i = 0; i < thread_count; ++i) {
        thread_pool.emplace_back([this]() {
            io_context.run();
        });
    }
    
    std::cout << "HTTP Server running with " << thread_count << " threads" << std::endl;
    
    // Wait for threads
    for (auto& thread : thread_pool) {
        if (thread.joinable()) {
            thread.join();
        }
    }
}

void HTTPServer::stop() {
    io_context.stop();
    std::cout << "HTTP Server stopped" << std::endl;
}

void HTTPServer::accept_connections() {
    acceptor.async_accept([this](boost::system::error_code ec, tcp::socket socket) {
        if (!ec) {
            auto connection = std::make_shared<HTTPConnection>(
                std::move(socket),
                [this](HTTPRequest& req, HTTPResponse& res) {
                    handle_request(req, res);
                }
            );
            connection->start();
        } else {
            std::cerr << "Accept error: " << ec.message() << std::endl;
        }
        
        // Continue accepting
        accept_connections();
    });
}

void HTTPServer::handle_request(HTTPRequest& req, HTTPResponse& res) {
    std::cout << req.method << " " << req.path << std::endl;
    // Always add CORS headers so browser clients can call this API from other origins.
    // These headers will be present on all responses including errors.
    res.headers["Access-Control-Allow-Origin"] = "*";
    res.headers["Access-Control-Allow-Methods"] = "GET, POST, PUT, DELETE, OPTIONS";
    res.headers["Access-Control-Allow-Headers"] = "Content-Type, Authorization";
    res.headers["Access-Control-Max-Age"] = "3600";

    // Respond to preflight CORS requests immediately
    if (req.method == "OPTIONS") {
        // No body needed for preflight
        res.set_status(204, "No Content");
        res.set_json_body("");
        return;
    }

    std::cout << req.method << " " << req.path << std::endl;
    
    RouteHandler handler;
    std::map<std::string, std::string> path_params;  
    if (match_route(req.method, req.path, path_params, handler)) {
        req.path_params = path_params;  
        handler(req, res);
    } else {
        res.set_status(404, "Not Found");
        res.set_json_body("{\"error\":\"Route not found\"}");
    }
}

bool HTTPServer::match_route(const std::string& method, const std::string& path, 
                             std::map<std::string, std::string>& path_params, 
                             RouteHandler& handler) {
    auto method_routes = routes.find(method);
    if (method_routes == routes.end()) {
        return false;
    }
    
    // First try exact match
    auto exact_match = method_routes->second.find(path);
    if (exact_match != method_routes->second.end()) {
        handler = exact_match->second;
        return true;
    }
    
    // Try pattern matching with path parameters
    for (const auto& route_pair : method_routes->second) {
        const std::string& route_pattern = route_pair.first;
        
        // Split both paths into segments
        std::vector<std::string> pattern_segments, path_segments;
        
        // Simple split by '/'
        std::istringstream pattern_stream(route_pattern);
        std::istringstream path_stream(path);
        std::string segment;
        
        while (std::getline(pattern_stream, segment, '/')) {
            if (!segment.empty()) pattern_segments.push_back(segment);
        }
        
        while (std::getline(path_stream, segment, '/')) {
            if (!segment.empty()) path_segments.push_back(segment);
        }
        
        // Must have same number of segments
        if (pattern_segments.size() != path_segments.size()) {
            continue;
        }
        
        // Match each segment
        bool matches = true;
        path_params.clear();
        
        for (size_t i = 0; i < pattern_segments.size(); i++) {
            if (pattern_segments[i][0] == ':') {
                // This is a path parameter
                std::string param_name = pattern_segments[i].substr(1);
                path_params[param_name] = path_segments[i];
            } else if (pattern_segments[i] != path_segments[i]) {
                // Static segment doesn't match
                matches = false;
                break;
            }
        }
        
        if (matches) {
            handler = route_pair.second;
            return true;
        }
    }
    
    return false;
}