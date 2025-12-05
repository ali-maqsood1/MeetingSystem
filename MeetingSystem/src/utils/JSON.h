#ifndef JSON_H
#define JSON_H

#include <string>
#include <sstream>
#include <map>
#include <vector>

// Simple JSON builder (good enough for DSA project)
class JSON {
public:
    // Build JSON from key-value pairs
    static std::string object(const std::map<std::string, std::string>& data) {
        std::ostringstream json;
        json << "{";
        
        bool first = true;
        for (const auto& pair : data) {
            if (!first) json << ",";
            json << "\"" << pair.first << "\":\"" << escape(pair.second) << "\"";
            first = false;
        }
        
        json << "}";
        return json.str();
    }
    
    // Build JSON with mixed types
    static std::string build(const std::string& content) {
        return "{" + content + "}";
    }
    
    // Add string field
    static std::string field(const std::string& key, const std::string& value) {
        return "\"" + key + "\":\"" + escape(value) + "\"";
    }

    // Const-char* overload to avoid accidental conversion to bool when passing C-strings
    static std::string field(const std::string& key, const char* value) {
        return field(key, std::string(value ? value : ""));
    }

    // Add a raw JSON field (value is already a JSON fragment like {..} or [..])
    static std::string raw_field(const std::string& key, const std::string& raw_json) {
        return "\"" + key + "\":" + raw_json;
    }
    
    // Add numeric field
    static std::string field(const std::string& key, uint64_t value) {
        return "\"" + key + "\":" + std::to_string(value);
    }
    
    static std::string field(const std::string& key, int value) {
        return "\"" + key + "\":" + std::to_string(value);
    }
    
    // Add boolean field
    static std::string field(const std::string& key, bool value) {
        return "\"" + key + "\":" + (value ? "true" : "false");
    }
    
    // Array of objects
    static std::string array(const std::vector<std::string>& items) {
        std::ostringstream json;
        json << "[";
        
        bool first = true;
        for (const auto& item : items) {
            if (!first) json << ",";
            json << item;
            first = false;
        }
        
        json << "]";
        return json.str();
    }
    
    // Parse simple JSON (improved implementation)
    static std::map<std::string, std::string> parse(const std::string& json) {
        std::map<std::string, std::string> result;
        
        size_t pos = 0;
        while (pos < json.length()) {
            // Find key
            size_t key_start = json.find('"', pos);
            if (key_start == std::string::npos) break;
            key_start++;
            
            size_t key_end = json.find('"', key_start);
            if (key_end == std::string::npos) break;
            
            std::string key = json.substr(key_start, key_end - key_start);
            
            // Find colon
            size_t colon_pos = json.find(':', key_end);
            if (colon_pos == std::string::npos) break;
            
            // Skip whitespace after colon
            size_t value_start = colon_pos + 1;
            while (value_start < json.length() && (json[value_start] == ' ' || json[value_start] == '\t')) {
                value_start++;
            }
            
            std::string value;
            
            // Check if value is a string (starts with ")
            if (json[value_start] == '"') {
                value_start++;
                size_t value_end = json.find('"', value_start);
                if (value_end == std::string::npos) break;
                value = json.substr(value_start, value_end - value_start);
                pos = value_end + 1;
            } else {
                // Value is a number or boolean
                size_t value_end = value_start;
                while (value_end < json.length() && 
                       json[value_end] != ',' && 
                       json[value_end] != '}' && 
                       json[value_end] != ']') {
                    value_end++;
                }
                value = json.substr(value_start, value_end - value_start);
                // Trim whitespace
                size_t last = value.find_last_not_of(" \t\r\n");
                if (last != std::string::npos) {
                    value = value.substr(0, last + 1);
                }
                pos = value_end;
            }
            
            result[key] = value;
        }
        
        return result;
    }
    // For building NESTED objects (no outer braces)
    static std::string nested(const std::string& fields) {
        return "{" + fields + "}";
    }
    // Success response
    static std::string success(const std::string& content = "") {
        if (content.empty()) {
            return "{\"success\":true}";
        }
        return "{\"success\":true," + content + "}";
    }
    
    // Error response
    static std::string error(const std::string& message) {
        return "{\"success\":false,\"error\":\"" + escape(message) + "\"}";
    }
    
private:
    static std::string escape(const std::string& str) {
        std::string result;
        for (char c : str) {
            if (c == '"') result += "\\\"";
            else if (c == '\\') result += "\\\\";
            else if (c == '\n') result += "\\n";
            else if (c == '\r') result += "\\r";
            else if (c == '\t') result += "\\t";
            else result += c;
        }
        return result;
    }
};

#endif // JSON_H