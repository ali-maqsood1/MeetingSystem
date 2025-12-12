#include "BTree.h"
#include <iostream>
#include <algorithm>

BTree::BTree(DatabaseEngine* engine) 
    : db_engine(engine), root_page_id(0) {
}

void BTree::initialize() {
    // Allocate root page
    root_page_id = db_engine->allocate_page();
    
    // Create empty root node
    BTreeNode root;
    root.is_leaf = true;
    root.num_keys = 0;
    
    save_node(root_page_id, root);
    
    std::cout << "B-Tree initialized with root page: " << root_page_id << std::endl;
}

void BTree::load(uint64_t root_id) {
    root_page_id = root_id;
    std::cout << "B-Tree loaded with root page: " << root_page_id << std::endl;
}

BTreeNode BTree::load_node(uint64_t page_id) {
    Page page = db_engine->read_page(page_id);
    BTreeNode node;
    node.deserialize(page.data);
    return node;
}

void BTree::save_node(uint64_t page_id, const BTreeNode& node) {
    Page page;
    page.header.type = node.is_leaf ? BTREE_LEAF : BTREE_INTERNAL;
    node.serialize(page.data);
    db_engine->write_page(page_id, page);
}

int BTree::search_key_position(const BTreeNode& node, uint64_t key) {
    // Binary search for key position
    int left = 0;
    int right = node.num_keys - 1;
    
    while (left <= right) {
        int mid = left + (right - left) / 2;
        if (node.keys[mid] == key) {
            return mid;
        } else if (node.keys[mid] < key) {
            left = mid + 1;
        } else {
            right = mid - 1;
        }
    }
    
    return left;  // Insert position
}

RecordLocation BTree::search_internal(uint64_t node_page_id, uint64_t key, bool& found) {
    BTreeNode node = load_node(node_page_id);

    int pos = search_key_position(node, key);

    if (node.is_leaf) {
        if (pos < node.num_keys && node.keys[pos] == key) {
            found = true;
            return node.records[pos];
        }

        found = false;
        return RecordLocation();
    }

    // For internal nodes: if key equals separator key, descend to the right child
    if (pos < node.num_keys && node.keys[pos] == key) {
        return search_internal(node.children[pos + 1], key, found);
    }

    // Otherwise descend to child[pos]
    return search_internal(node.children[pos], key, found);
}

RecordLocation BTree::search(uint64_t key, bool& found) {
    if (root_page_id == 0) {
        found = false;
        return RecordLocation();
    }
    
    return search_internal(root_page_id, key, found);
}

void BTree::split_child(uint64_t parent_page_id, int child_index, uint64_t child_page_id) {
    // Load parent and child nodes from disk
    BTreeNode parent = load_node(parent_page_id);
    BTreeNode child = load_node(child_page_id);
    
    // Create new node
    uint64_t new_page_id = db_engine->allocate_page();
    BTreeNode new_node;
    new_node.is_leaf = child.is_leaf;
    new_node.parent_page = parent_page_id;
    
    int mid = MAX_KEYS / 2;
    
    // Copy second half of keys to new node
    new_node.num_keys = MAX_KEYS - mid - 1;
    for (int i = 0; i < new_node.num_keys; i++) {
        new_node.keys[i] = child.keys[mid + 1 + i];
    }
    
    if (child.is_leaf) {
        // Copy records
        for (int i = 0; i < new_node.num_keys; i++) {
            new_node.records[i] = child.records[mid + 1 + i];
        }
        // Link leaves
        new_node.next_leaf = child.next_leaf;
        child.next_leaf = new_page_id;
    } else {
        // Copy children
        for (int i = 0; i <= new_node.num_keys; i++) {
            new_node.children[i] = child.children[mid + 1 + i];
        }
    }
    
    // Update child node size
    child.num_keys = mid;
    
    // Insert middle key into parent (shift keys/children to make space)
    for (int i = parent.num_keys; i > child_index; i--) {
        parent.keys[i] = parent.keys[i - 1];
        parent.children[i + 1] = parent.children[i];
    }
    
    parent.keys[child_index] = child.keys[mid];
    parent.children[child_index + 1] = new_page_id;
    parent.num_keys++;
    
    // Save all modified nodes
    save_node(child_page_id, child);
    save_node(new_page_id, new_node);
    save_node(parent_page_id, parent);
}

void BTree::insert_non_full(uint64_t node_page_id, uint64_t key, const RecordLocation& record) {
    BTreeNode node = load_node(node_page_id);
    
    int pos = search_key_position(node, key);
    
    if (node.is_leaf) {
        // Insert into leaf
        for (int i = node.num_keys; i > pos; i--) {
            node.keys[i] = node.keys[i - 1];
            node.records[i] = node.records[i - 1];
        }
        
        node.keys[pos] = key;
        node.records[pos] = record;
        node.num_keys++;
        
        save_node(node_page_id, node);
    } else {
        // Internal node - find child
        uint64_t child_page_id = node.children[pos];
        BTreeNode child = load_node(child_page_id);
        
        if (child.num_keys == MAX_KEYS) {
            // Split child first (pass parent page id)
            split_child(node_page_id, pos, child_page_id);
            
            // Reload node as it was modified
            node = load_node(node_page_id);
            
            if (key > node.keys[pos]) {
                pos++;
            }
        }
        
        insert_non_full(node.children[pos], key, record);
    }
}

bool BTree::insert(uint64_t key, const RecordLocation& record) {
    if (root_page_id == 0) {
        initialize();
    }
    
    BTreeNode root = load_node(root_page_id);
    
    if (root.num_keys == MAX_KEYS) {
        // Root is full, split it
        uint64_t new_root_id = db_engine->allocate_page();
        BTreeNode new_root;
        new_root.is_leaf = false;
        new_root.num_keys = 0;
        new_root.children[0] = root_page_id;
        
        save_node(new_root_id, new_root);
        
    split_child(new_root_id, 0, root_page_id);
        
        root_page_id = new_root_id;
        
        insert_non_full(new_root_id, key, record);
    } else {
        insert_non_full(root_page_id, key, record);
    }
    
    return true;
}

std::vector<RecordLocation> BTree::range_search(uint64_t start_key, uint64_t end_key) {
    std::vector<RecordLocation> results;
    
    if (root_page_id == 0) {
        return results;
    }
    
    // Find start leaf
    uint64_t current_page = root_page_id;
    BTreeNode node = load_node(current_page);
    
    // Traverse to leftmost leaf >= start_key
    while (!node.is_leaf) {
        int pos = search_key_position(node, start_key);
        current_page = node.children[pos];
        node = load_node(current_page);
    }
    
    // Scan leaves
    while (current_page != 0) {
        node = load_node(current_page);
        
        for (int i = 0; i < node.num_keys; i++) {
            if (node.keys[i] >= start_key && node.keys[i] <= end_key) {
                results.push_back(node.records[i]);
            } else if (node.keys[i] > end_key) {
                return results;  // Done
            }
        }
        
        current_page = node.next_leaf;
    }
    
    return results;
}

bool BTree::remove(uint64_t key) {
    // TODO: Implement delete with merge/redistribute
    // This is complex and can be added later
    std::cout << "Delete not yet implemented" << std::endl;
    return false;
}