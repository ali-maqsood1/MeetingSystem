# Meeting System with Custom C++ Storage Engine

## Project Documentation

---

## Table of Contents
1. Project Overview
2. Technology Stack
3. System Architecture
4. Core Components
5. Features Implemented
6. Design Decisions
7. Challenges and Solutions
8. Future Enhancements

---

## 1. Project Overview

### Topic
**Collaborative Meeting System with Custom Database Engine**

### Description
This project implements a fully-functional web-based meeting system powered by a custom-built database engine written in C++. Unlike traditional applications that rely on existing databases (MySQL, PostgreSQL, MongoDB), this system features a ground-up implementation of core data structures including B-Trees and Hash Tables for efficient data storage and retrieval.

### Motivation
The project demonstrates the practical application of Data Structures & Algorithms (DSA) concepts by building a real-world application where:
- **B-Trees** manage hierarchical user and meeting data
- **Hash Tables** provide O(1) login lookups and meeting code validation
- **Custom Storage Engine** handles page-based disk I/O similar to production databases
- **RESTful API** bridges low-level C++ backend with modern web frontend

---

## 2. Technology Stack

### Backend (Core Engine)
- **Language**: C++17
- **Networking**: Boost.Asio for asynchronous HTTP server
- **Build System**: CMake
- **Platform**: Linux (Ubuntu/Arch)

### Frontend
- **HTML5/CSS3**: Responsive UI design
- **Vanilla JavaScript**: No framework dependencies
- **Fetch API**: RESTful communication with backend

### Storage Layer
- **Custom B-Tree Implementation**: O(log n) search, insert, delete
- **Custom Hash Table**: O(1) average-case lookups with chaining
- **Page-based Storage**: 4KB pages mimicking real database systems
- **Single File Database**: `meeting_system.db` (binary format)

---

## 3. System Architecture

### High-Level Architecture
```
┌─────────────────────────────────────────────────────────┐
│                    Web Browser                          │
│  (HTML/CSS/JS - Lobby, Meeting, Whiteboard Interface)  │
└──────────────────────┬──────────────────────────────────┘
                       │ HTTP/JSON
                       ▼
┌─────────────────────────────────────────────────────────┐
│              HTTPServer (Boost.Asio)                    │
│  • Route Handling    • CORS Support                     │
│  • Authentication    • Request Parsing                  │
└──────────────────────┬──────────────────────────────────┘
                       │
                       ▼
┌─────────────────────────────────────────────────────────┐
│                  Manager Layer                          │
│  ┌──────────────┬────────────┬─────────────────────┐  │
│  │ AuthManager  │ MeetingMgr │ ChatManager         │  │
│  │ FileMgr      │ Whiteboard │ ScreenShareManager  │  │
│  └──────────────┴────────────┴─────────────────────┘  │
└──────────────────────┬──────────────────────────────────┘
                       │
                       ▼
┌─────────────────────────────────────────────────────────┐
│              Storage Layer (DSA Core)                   │
│  ┌─────────────────────┬───────────────────────────┐  │
│  │  B-Tree Indexes     │   Hash Table Indexes      │  │
│  │  • Users            │   • Login Cache           │  │
│  │  • Meetings         │   • Meeting Codes         │  │
│  │  • Messages         │   • Session Tokens        │  │
│  │  • Files            │                           │  │
│  │  • Whiteboard       │                           │  │
│  └─────────────────────┴───────────────────────────┘  │
└──────────────────────┬──────────────────────────────────┘
                       │
                       ▼
┌─────────────────────────────────────────────────────────┐
│         DatabaseEngine (Page Manager)                   │
│  • 4KB Page Allocation   • Free Page Management        │
│  • Disk I/O Operations   • Transaction Safety          │
└──────────────────────┬──────────────────────────────────┘
                       │
                       ▼
              meeting_system.db
              (Binary File Storage)
```

### Data Flow Example: User Login
1. **Frontend**: User enters credentials → `POST /api/v1/auth/login`
2. **HTTPServer**: Parses JSON request → Calls `AuthManager::login()`
3. **AuthManager**: 
   - Looks up username in Login Hash Table (O(1))
   - Retrieves User record from Users B-Tree (O(log n))
   - Validates SHA-256 password hash
   - Generates session token
4. **Response**: Returns `{ success: true, token: "...", username: "..." }`
5. **Frontend**: Stores token in localStorage → Redirects to lobby

---

## 4. Core Components

### 4.1 Storage Engine (`DatabaseEngine.cpp`)
- **Page-based Architecture**: Fixed 4KB pages (industry standard)
- **Free Page Management**: Linked list of available pages
- **Persistent Storage**: Single `.db` file on disk
- **Thread-safe Operations**: Mutex-protected I/O

**Key Functions:**
```cpp
uint64_t allocate_page();           // Get next free page
void deallocate_page(uint64_t page_id);  // Return page to free list
void read_page(uint64_t page_id, void* data);
void write_page(uint64_t page_id, const void* data);
```

### 4.2 B-Tree Implementation (`BTree.cpp`)
- **Self-balancing**: Maintains O(log n) height
- **Order 100**: Up to 100 keys per node (optimize disk I/O)
- **Supports**: Insert, Search, Delete, Range Queries
- **Use Cases**:
  - `users_tree` (page 1): User ID → User struct
  - `meetings_tree` (page 2): Meeting ID → Meeting struct
  - `messages_tree` (page 517): (Meeting ID, Timestamp) → Message
  - `files_tree` (page 518): File ID → FileMetadata
  - `whiteboard_tree` (page 519): Element ID → WhiteboardElement

**Node Structure:**
```cpp
struct BTreeNode {
    bool is_leaf;
    uint64_t num_keys;
    uint64_t keys[MAX_KEYS];
    uint64_t values[MAX_KEYS];       // Data page IDs
    uint64_t children[MAX_KEYS + 1]; // Child node page IDs
};
```

### 4.3 Hash Table Implementation (`HashTable.cpp`)
- **Chaining**: Linked lists handle collisions
- **Dynamic Capacity**: 256 buckets across multiple pages
- **Hash Function**: Simple modulo (extensible to better hashing)
- **Use Cases**:
  - `login_hash` (page 3): Username → User ID (fast login)
  - `meeting_code_hash` (page 260): Invite Code → Meeting ID
  - `token_hash` (page 520): Session Token → User ID

**Bucket Structure:**
```cpp
struct HashBucket {
    uint64_t count;
    Entry entries[ENTRIES_PER_BUCKET];
    uint64_t overflow_page;  // Chaining for collisions
};
```

### 4.4 HTTP Server (`HTTPServer.cpp`)
- **Async I/O**: Boost.Asio event loop with thread pool (4 threads)
- **Route Registration**: Dynamic path matching (supports `:id` params)
- **CORS Support**: Handles preflight OPTIONS requests
- **Request Parsing**: Manual HTTP 1.1 parser (headers, body, query params)

**Example Route:**
```cpp
server.add_route("POST", "/api/v1/meetings/:id/messages",
    [&](const HTTPRequest& req, HTTPResponse& res) {
        uint64_t meeting_id = std::stoull(req.path_params["id"]);
        // ... handle chat message
    });
```

### 4.5 Manager Layer

#### AuthManager
- User registration with email validation
- SHA-256 password hashing
- Session token generation (random 32-byte hex)
- Token-based authentication for all protected endpoints

#### MeetingManager
- Meeting creation with unique invite codes (format: `XXX-XXX-XXX`)
- Join meeting via invite code
- Track active meetings (is_active flag)
- Creator-based access control

#### ChatManager
- Real-time message storage in B-Tree
- Ordered by (meeting_id, timestamp) composite key
- Supports message history retrieval (last N messages)
- Username + content + timestamp metadata

#### WhiteboardManager
- Stores drawing elements (lines, rectangles, circles, text)
- Each element has: type, color, position, size
- Supports collaborative drawing (multiple users)
- Clear whiteboard functionality

---

## 5. Features Implemented

### Authentication & Authorization
✅ User registration with email/password  
✅ Secure login with session tokens  
✅ Token validation on all protected endpoints  
✅ Password hashing (SHA-256)  
✅ Session management (logout clears tokens)

### Meeting Management
✅ Create meetings with custom titles  
✅ Generate unique invite codes (XXX-XXX-XXX format)  
✅ Join meetings via invite code  
✅ List user's created meetings  
✅ Track meeting participants  
✅ Meeting status (active/inactive)

### Real-time Chat
✅ Send messages in meeting rooms  
✅ Retrieve message history  
✅ Display username with each message  
✅ Timestamp-based ordering  
✅ Persistent message storage

### Collaborative Whiteboard
✅ Draw lines, rectangles, circles  
✅ Color picker for elements  
✅ Real-time sync via polling  
✅ Clear whiteboard (all users)  
✅ Persistent element storage

### File Sharing
✅ Upload files to meeting rooms  
✅ Store file metadata (name, size, type)  
✅ List all files in meeting  
✅ Associate files with meetings

### Screen Sharing
✅ Start screen share (placeholder)  
✅ Stop screen share  
✅ Track active screen shares

---

## 6. Design Decisions

### Why Custom Database Instead of MySQL/PostgreSQL?

**Educational Value:**
- Demonstrates deep understanding of data structures (B-Trees, Hash Tables)
- Shows how real databases work under the hood
- Practical application of DSA course concepts

**Performance Control:**
- Optimized for specific use case (small-scale meetings)
- No network overhead (in-process database)
- Predictable performance characteristics

**Simplicity:**
- No external dependencies (just Boost)
- Single binary deployment
- Easy to debug and profile

### Why B-Trees Over Binary Search Trees?

**Disk I/O Optimization:**
- B-Trees minimize disk reads (fewer levels)
- Each node holds ~100 keys (vs BST's 1-2 keys)
- Better cache locality

**Real-world Relevance:**
- MySQL InnoDB, PostgreSQL, SQLite all use B-Trees
- Industry-standard indexing structure

### Why Hash Tables for Login?

**Constant-time Lookups:**
- Username → User ID in O(1) average case
- Critical for login performance (happens frequently)

**vs B-Tree Login:**
- B-Tree: O(log n) lookups
- Hash Table: O(1) lookups
- Trade-off: Hash tables don't support range queries (not needed for login)

### Why HTTP Server Instead of WebSockets?

**Simplicity:**
- REST API easier to implement and debug
- No persistent connection management
- Works with standard browsers (no special libraries)

**Scalability:**
- Stateless requests scale horizontally
- Can add load balancer later

**Trade-off:**
- Polling instead of push notifications
- Higher latency for real-time updates (5-second intervals)

---

## 7. Challenges and Solutions

### Challenge 1: JSON Parsing Without Libraries
**Problem:** C++ has no built-in JSON support  
**Solution:** Implemented lightweight JSON helper class with:
- Manual string parsing for `{ "key": "value" }`
- Type-safe field builders
- Escaped string handling for special characters

### Challenge 2: Thread-safe Database Access
**Problem:** Multiple HTTP threads accessing same database  
**Solution:**
- Mutex locks in `DatabaseEngine` for page I/O
- Each manager has its own lock for data structure modifications
- Read-heavy operations use shared locks (future optimization)

### Challenge 3: Routing with Dynamic Parameters
**Problem:** Need to extract `:id` from `/api/v1/meetings/:id/messages`  
**Solution:**
- Custom path matching algorithm
- Split path into segments
- Match `:param` placeholders → store in `path_params` map

### Challenge 4: CORS Preflight Requests
**Problem:** Browser sends OPTIONS request before POST  
**Solution:**
- Added CORS headers to all responses:
  ```cpp
  res.headers["Access-Control-Allow-Origin"] = "*";
  res.headers["Access-Control-Allow-Methods"] = "GET, POST, DELETE, OPTIONS";
  res.headers["Access-Control-Allow-Headers"] = "Content-Type, Authorization";
  ```
- Return 200 OK for all OPTIONS requests

### Challenge 5: Double Login Requests
**Problem:** Frontend sending duplicate login requests  
**Solution:**
- Removed duplicate event listeners
- Added `e.preventDefault()` to form submissions
- Disabled submit button during request processing

### Challenge 6: B-Tree Node Splitting
**Problem:** Complex pointer updates during node splits  
**Solution:**
- Implemented iterative split algorithm (no recursion)
- Careful ordering of page writes to maintain consistency
- Tested with edge cases (full nodes, root splits)

---

## 8. Future Enhancements

### Short-term (1-2 weeks)
- [ ] WebSocket support for real-time updates (eliminate polling)
- [ ] Video/audio streaming integration (WebRTC)
- [ ] End-to-end encryption for messages
- [ ] Meeting recording functionality
- [ ] Advanced whiteboard tools (undo/redo, eraser)

### Medium-term (1-2 months)
- [ ] User profiles with avatars
- [ ] Meeting scheduling (date/time picker)
- [ ] Email invitations with calendar integration
- [ ] Persistent WebRTC connections for screen sharing
- [ ] Mobile-responsive UI improvements
- [ ] Dark mode theme

### Long-term (3+ months)
- [ ] Multi-database support (sharding for horizontal scaling)
- [ ] Write-ahead logging (WAL) for crash recovery
- [ ] MVCC (Multi-Version Concurrency Control) for better concurrency
- [ ] Query optimizer for complex range queries
- [ ] Replication for high availability
- [ ] Admin dashboard with analytics

### Advanced Database Features
- [ ] ACID transactions with rollback support
- [ ] Composite indexes for complex queries
- [ ] Full-text search for chat history
- [ ] Background compaction (garbage collection)
- [ ] Snapshot isolation for consistent reads

---

## Conclusion

This project successfully demonstrates the practical application of fundamental data structures in building a real-world application. By implementing a custom storage engine from scratch, the project provides deep insights into:

1. **How databases work internally** (page management, indexing, caching)
2. **Performance trade-offs** (B-Trees vs Hash Tables, polling vs WebSockets)
3. **System design principles** (layered architecture, separation of concerns)
4. **Full-stack development** (backend C++, frontend JavaScript, HTTP protocol)

The Meeting System serves as both a functional collaboration tool and an educational platform for understanding low-level systems programming combined with modern web technologies.

---

## References

### Academic Resources
- "Database System Concepts" by Silberschatz, Korth, Sudarshan
- "Introduction to Algorithms" by Cormen, Leiserson, Rivest, Stein (B-Tree chapter)
- "The Art of Computer Programming, Vol. 3" by Donald Knuth (Hashing)

### Technical Documentation
- Boost.Asio Documentation: https://www.boost.org/doc/libs/1_83_0/doc/html/boost_asio.html
- CMake Documentation: https://cmake.org/documentation/
- HTTP/1.1 RFC 2616: https://www.rfc-editor.org/rfc/rfc2616

### Open Source Inspiration
- SQLite B-Tree Implementation: https://www.sqlite.org/btreemodule.html
- Redis Hash Table Implementation: https://github.com/redis/redis
- Boost Beast HTTP Examples: https://github.com/boostorg/beast

---

**Project Repository:** /home/ali/Documents/DSA/final_project/ver_1  
**Build System:** CMake 3.10+  
**Tested On:** Ubuntu 22.04, Arch Linux  
**Lines of Code:** ~5,000+ (C++), ~2,000+ (JavaScript/HTML/CSS)  
**Development Time:** 4-6 weeks  

**Author:** Ali  
**Course:** Data Structures & Algorithms  
**Date:** December 2024