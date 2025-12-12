#ifndef BTREE_H
#define BTREE_H

#include "DatabaseEngine.h"
#include <vector>
#include <string>

// B-Tree configuration
const int BTREE_ORDER = 64; // Maximum 63 keys, 64 children
const int MAX_KEYS = BTREE_ORDER - 1;
const int MIN_KEYS = (BTREE_ORDER / 2) - 1;

// Record location in database
struct RecordLocation
{
    uint64_t page_id;
    uint16_t offset;
    uint16_t size;

    RecordLocation() : page_id(0), offset(0), size(0) {}
    RecordLocation(uint64_t pid, uint16_t off, uint16_t sz)
        : page_id(pid), offset(off), size(sz) {}
};

// B-Tree node structure (stored in page data)
struct BTreeNode
{
    bool is_leaf;
    uint16_t num_keys;
    uint64_t parent_page;
    uint64_t next_leaf; // For leaf nodes, links to next leaf

    // Keys and values
    uint64_t keys[MAX_KEYS];

    // For internal nodes: child page IDs
    // For leaf nodes: record locations
    union
    {
        uint64_t children[BTREE_ORDER];   // Internal nodes
        RecordLocation records[MAX_KEYS]; // Leaf nodes
    };

    BTreeNode() : is_leaf(true), num_keys(0), parent_page(0), next_leaf(0)
    {
        for (int i = 0; i < MAX_KEYS; i++)
        {
            keys[i] = 0;
        }
        for (int i = 0; i < BTREE_ORDER; i++)
        {
            children[i] = 0;
        }
    }

    // Serialize node to page data
    void serialize(uint8_t *buffer) const
    {
        size_t offset = 0;

        buffer[offset++] = is_leaf ? 1 : 0;
        memcpy(buffer + offset, &num_keys, sizeof(num_keys));
        offset += sizeof(num_keys);
        memcpy(buffer + offset, &parent_page, sizeof(parent_page));
        offset += sizeof(parent_page);
        memcpy(buffer + offset, &next_leaf, sizeof(next_leaf));
        offset += sizeof(next_leaf);

        // Write keys
        memcpy(buffer + offset, keys, sizeof(keys));
        offset += sizeof(keys);

        // Write children/records based on node type
        if (is_leaf)
        {
            memcpy(buffer + offset, records, sizeof(RecordLocation) * MAX_KEYS);
        }
        else
        {
            memcpy(buffer + offset, children, sizeof(children));
        }
    }

    // Deserialize node from page data
    void deserialize(const uint8_t *buffer)
    {
        size_t offset = 0;

        is_leaf = (buffer[offset++] == 1);
        memcpy(&num_keys, buffer + offset, sizeof(num_keys));
        offset += sizeof(num_keys);
        memcpy(&parent_page, buffer + offset, sizeof(parent_page));
        offset += sizeof(parent_page);
        memcpy(&next_leaf, buffer + offset, sizeof(next_leaf));
        offset += sizeof(next_leaf);

        // Read keys
        memcpy(keys, buffer + offset, sizeof(keys));
        offset += sizeof(keys);

        // Read children/records based on node type
        if (is_leaf)
        {
            memcpy(records, buffer + offset, sizeof(RecordLocation) * MAX_KEYS);
        }
        else
        {
            memcpy(children, buffer + offset, sizeof(children));
        }
    }
};

// B-Tree class
class BTree
{
private:
    DatabaseEngine *db_engine;
    uint64_t root_page_id;

    // Helper functions
    BTreeNode load_node(uint64_t page_id);
    void save_node(uint64_t page_id, const BTreeNode &node);

    int search_key_position(const BTreeNode &node, uint64_t key);
    // Split the child at child_index of the parent located at parent_page_id
    void split_child(uint64_t parent_page_id, int child_index, uint64_t child_page_id);
    void insert_non_full(uint64_t node_page_id, uint64_t key, const RecordLocation &record);

    RecordLocation search_internal(uint64_t node_page_id, uint64_t key, bool &found);

    // Delete helper functions
    void remove_internal(uint64_t node_page_id, uint64_t key);
    void remove_from_leaf(BTreeNode &node, int idx);
    void remove_from_non_leaf(uint64_t node_page_id, int idx);
    void borrow_from_prev(uint64_t node_page_id, int child_idx);
    void borrow_from_next(uint64_t node_page_id, int child_idx);
    void merge(uint64_t node_page_id, int child_idx);
    void fill_child(uint64_t node_page_id, int child_idx);

public:
    BTree(DatabaseEngine *engine);

    // Initialize empty tree
    void initialize();

    // Load existing tree
    void load(uint64_t root_id);

    // Core operations
    bool insert(uint64_t key, const RecordLocation &record);
    RecordLocation search(uint64_t key, bool &found);
    bool remove(uint64_t key);

    // Range query
    std::vector<RecordLocation> range_search(uint64_t start_key, uint64_t end_key);

    // Getters
    uint64_t get_root_page_id() const { return root_page_id; }
};

#endif // BTREE_H