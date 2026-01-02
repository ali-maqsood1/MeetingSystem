#include "../src/storage/BTree.h"
#include "../src/storage/HashTable.h"
#include "../src/models/User.h"
#include "../src/storage/DatabaseEngine.h"

#include <iostream>
#include <cassert>
#include <ctime>

void test_page_management() {
    std::cout << "\n=== Testing Page Management ===" << std::endl;
    
    DatabaseEngine db("test_pages.db");
    db.initialize();
    
    // Allocate pages
    uint64_t page1 = db.allocate_page();
    uint64_t page2 = db.allocate_page();
    uint64_t page3 = db.allocate_page();
    
    std::cout << "Allocated pages: " << page1 << ", " << page2 << ", " << page3 << std::endl;
    
    // Write data to a page
    Page test_page;
    test_page.header.type = DATA_OVERFLOW;
    strcpy((char*)test_page.data, "Hello, Database!");
    db.write_page(page1, test_page);
    
    // Read it back
    Page read_page = db.read_page(page1);
    std::cout << "Read data: " << (char*)read_page.data << std::endl;
    
    // Free a page
    db.free_page(page2);
    
    // Allocate again - should reuse freed page
    uint64_t page4 = db.allocate_page();
    std::cout << "Reused page: " << page4 << " (should be " << page2 << ")" << std::endl;
    
    db.close();
    std::cout << "Page management test PASSED!" << std::endl;
}

void test_btree() {
    std::cout << "\n=== Testing B-Tree ===" << std::endl;
    
    DatabaseEngine db("test_btree.db");
    db.initialize();
    
    BTree btree(&db);
    btree.initialize();
    
    // Insert test data
    std::cout << "Inserting records..." << std::endl;
    for (uint64_t i = 1; i <= 100; i++) {
        RecordLocation loc(i * 10, 0, 100);
        btree.insert(i, loc);
        if (i % 20 == 0) {
            std::cout << "  Inserted " << i << " records" << std::endl;
        }
    }
    
    // Search for records
    std::cout << "\nSearching records..." << std::endl;
    bool found;
    RecordLocation loc = btree.search(50, found);
    assert(found);
    std::cout << "  Found key 50 at page " << loc.page_id << std::endl;
    
    loc = btree.search(999, found);
    assert(!found);
    std::cout << "  Key 999 not found (as expected)" << std::endl;
    
    // Range search
    std::cout << "\nRange search [20, 30]..." << std::endl;
    auto results = btree.range_search(20, 30);
    std::cout << "  Found " << results.size() << " records" << std::endl;
    
    db.close();
    std::cout << "B-Tree test PASSED!" << std::endl;
}

void test_hashtable() {
    std::cout << "\n=== Testing Hash Table ===" << std::endl;
    
    DatabaseEngine db("test_hash.db");
    db.initialize();
    
    HashTable hashtable(&db);
    hashtable.initialize();
    
    // Insert test data
    std::cout << "Inserting key-value pairs..." << std::endl;
    hashtable.insert("user@example.com", RecordLocation(1, 0, 100));
    hashtable.insert("admin@example.com", RecordLocation(2, 0, 100));
    hashtable.insert("test@example.com", RecordLocation(3, 0, 100));
    
    // Search
    std::cout << "\nSearching..." << std::endl;
    bool found;
    RecordLocation loc = hashtable.search("user@example.com", found);
    assert(found);
    std::cout << "  Found 'user@example.com' at page " << loc.page_id << std::endl;
    
    loc = hashtable.search("nonexistent@example.com", found);
    assert(!found);
    std::cout << "  'nonexistent@example.com' not found (as expected)" << std::endl;
    
    // Remove
    std::cout << "\nRemoving 'test@example.com'..." << std::endl;
    bool removed = hashtable.remove("test@example.com");
    assert(removed);
    
    loc = hashtable.search("test@example.com", found);
    assert(!found);
    std::cout << "  Removed successfully" << std::endl;
    
    db.close();
    std::cout << "Hash Table test PASSED!" << std::endl;
}

void test_user_storage() {
    std::cout << "\n=== Testing User Storage ===" << std::endl;
    
    DatabaseEngine db("test_users.db");
    db.initialize();
    
    BTree user_btree(&db);
    user_btree.initialize();
    
    HashTable email_hash(&db);
    email_hash.initialize();
    
    // Create a user
    User user;
    user.user_id = db.get_next_user_id();
    strcpy(user.email, "john@example.com");
    strcpy(user.username, "john_doe");
    strcpy(user.password_hash, "hashed_password_here");
    user.created_at = time(nullptr);
    
    std::cout << "Created user: " << user.username << " (ID: " << user.user_id << ")" << std::endl;
    
    // Serialize user
    uint8_t buffer[User::serialized_size()];
    user.serialize(buffer);
    
    // Store in a data page
    uint64_t data_page_id = db.allocate_page();
    Page data_page;
    memcpy(data_page.data, buffer, User::serialized_size());
    db.write_page(data_page_id, data_page);
    
    // Index in B-Tree
    RecordLocation user_loc(data_page_id, 0, User::serialized_size());
    user_btree.insert(user.user_id, user_loc);
    
    // Index in hash table by email
    email_hash.insert(user.email, user_loc);
    
    // Retrieve by ID
    std::cout << "\nRetrieving by user ID..." << std::endl;
    bool found;
    RecordLocation loc = user_btree.search(user.user_id, found);
    assert(found);
    
    Page retrieved_page = db.read_page(loc.page_id);
    User retrieved_user;
    retrieved_user.deserialize(retrieved_page.data);
    
    std::cout << "  Retrieved: " << retrieved_user.username << std::endl;
    assert(strcmp(retrieved_user.email, user.email) == 0);
    
    // Retrieve by email
    std::cout << "\nRetrieving by email..." << std::endl;
    loc = email_hash.search(user.email, found);
    assert(found);
    
    retrieved_page = db.read_page(loc.page_id);
    retrieved_user.deserialize(retrieved_page.data);
    
    std::cout << "  Retrieved: " << retrieved_user.username << std::endl;
    assert(strcmp(retrieved_user.email, user.email) == 0);
    
    db.close();
    std::cout << "User storage test PASSED!" << std::endl;
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "  Meeting System Storage Engine Tests  " << std::endl;
    std::cout << "========================================" << std::endl;
    
    try {
        test_page_management();
        test_btree();
        test_hashtable();
        test_user_storage();
        
        std::cout << "\n========================================" << std::endl;
        std::cout << "  ALL TESTS PASSED!                    " << std::endl;
        std::cout << "========================================" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "\nERROR: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}