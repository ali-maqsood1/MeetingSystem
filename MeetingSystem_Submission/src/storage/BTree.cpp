#include "BTree.h"
#include <iostream>
#include <algorithm>

BTree::BTree(DatabaseEngine *engine)
    : db_engine(engine), root_page_id(0)
{
}

void BTree::initialize()
{
    // Allocate root page
    root_page_id = db_engine->allocate_page();

    // Create empty root node
    BTreeNode root;
    root.is_leaf = true;
    root.num_keys = 0;

    save_node(root_page_id, root);

    std::cout << "B-Tree initialized with root page: " << root_page_id << std::endl;
}

void BTree::load(uint64_t root_id)
{
    root_page_id = root_id;
    std::cout << "B-Tree loaded with root page: " << root_page_id << std::endl;
}

BTreeNode BTree::load_node(uint64_t page_id)
{
    Page page = db_engine->read_page(page_id);
    BTreeNode node;
    node.deserialize(page.data);
    return node;
}

void BTree::save_node(uint64_t page_id, const BTreeNode &node)
{
    Page page;
    page.header.type = node.is_leaf ? BTREE_LEAF : BTREE_INTERNAL;
    node.serialize(page.data);
    db_engine->write_page(page_id, page);
}

int BTree::search_key_position(const BTreeNode &node, uint64_t key)
{
    // Binary search for key position
    int left = 0;
    int right = node.num_keys - 1;

    while (left <= right)
    {
        int mid = left + (right - left) / 2;
        if (node.keys[mid] == key)
        {
            return mid;
        }
        else if (node.keys[mid] < key)
        {
            left = mid + 1;
        }
        else
        {
            right = mid - 1;
        }
    }

    return left; // Insert position
}

RecordLocation BTree::search_internal(uint64_t node_page_id, uint64_t key, bool &found)
{
    BTreeNode node = load_node(node_page_id);

    int pos = search_key_position(node, key);

    if (node.is_leaf)
    {
        if (pos < node.num_keys && node.keys[pos] == key)
        {
            found = true;
            return node.records[pos];
        }

        found = false;
        return RecordLocation();
    }

    // For internal nodes: if key equals separator key, descend to the right child
    if (pos < node.num_keys && node.keys[pos] == key)
    {
        return search_internal(node.children[pos + 1], key, found);
    }

    // Otherwise descend to child[pos]
    return search_internal(node.children[pos], key, found);
}

RecordLocation BTree::search(uint64_t key, bool &found)
{
    if (root_page_id == 0)
    {
        found = false;
        return RecordLocation();
    }

    return search_internal(root_page_id, key, found);
}

void BTree::split_child(uint64_t parent_page_id, int child_index, uint64_t child_page_id)
{
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
    for (int i = 0; i < new_node.num_keys; i++)
    {
        new_node.keys[i] = child.keys[mid + 1 + i];
    }

    if (child.is_leaf)
    {
        // Copy records
        for (int i = 0; i < new_node.num_keys; i++)
        {
            new_node.records[i] = child.records[mid + 1 + i];
        }
        // Link leaves
        new_node.next_leaf = child.next_leaf;
        child.next_leaf = new_page_id;
    }
    else
    {
        // Copy children
        for (int i = 0; i <= new_node.num_keys; i++)
        {
            new_node.children[i] = child.children[mid + 1 + i];
        }
    }

    // Update child node size
    child.num_keys = mid;

    // Insert middle key into parent (shift keys/children to make space)
    for (int i = parent.num_keys; i > child_index; i--)
    {
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

void BTree::insert_non_full(uint64_t node_page_id, uint64_t key, const RecordLocation &record)
{
    BTreeNode node = load_node(node_page_id);

    int pos = search_key_position(node, key);

    if (node.is_leaf)
    {
        // Insert into leaf
        for (int i = node.num_keys; i > pos; i--)
        {
            node.keys[i] = node.keys[i - 1];
            node.records[i] = node.records[i - 1];
        }

        node.keys[pos] = key;
        node.records[pos] = record;
        node.num_keys++;

        save_node(node_page_id, node);
    }
    else
    {
        // Internal node - find child
        uint64_t child_page_id = node.children[pos];
        BTreeNode child = load_node(child_page_id);

        if (child.num_keys == MAX_KEYS)
        {
            // Split child first (pass parent page id)
            split_child(node_page_id, pos, child_page_id);

            // Reload node as it was modified
            node = load_node(node_page_id);

            if (key > node.keys[pos])
            {
                pos++;
            }
        }

        insert_non_full(node.children[pos], key, record);
    }
}

bool BTree::insert(uint64_t key, const RecordLocation &record)
{
    if (root_page_id == 0)
    {
        initialize();
    }

    BTreeNode root = load_node(root_page_id);

    if (root.num_keys == MAX_KEYS)
    {
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
    }
    else
    {
        insert_non_full(root_page_id, key, record);
    }

    return true;
}

std::vector<RecordLocation> BTree::range_search(uint64_t start_key, uint64_t end_key)
{
    std::vector<RecordLocation> results;

    if (root_page_id == 0)
    {
        return results;
    }

    // Find start leaf
    uint64_t current_page = root_page_id;
    BTreeNode node = load_node(current_page);

    // Traverse to leftmost leaf >= start_key
    while (!node.is_leaf)
    {
        int pos = search_key_position(node, start_key);
        current_page = node.children[pos];
        node = load_node(current_page);
    }

    // Scan leaves
    while (current_page != 0)
    {
        node = load_node(current_page);

        for (int i = 0; i < node.num_keys; i++)
        {
            if (node.keys[i] >= start_key && node.keys[i] <= end_key)
            {
                results.push_back(node.records[i]);
            }
            else if (node.keys[i] > end_key)
            {
                return results; // Done
            }
        }

        current_page = node.next_leaf;
    }

    return results;
}

void BTree::borrow_from_prev(uint64_t node_page_id, int child_idx)
{
    BTreeNode node = load_node(node_page_id);
    BTreeNode child = load_node(node.children[child_idx]);
    BTreeNode sibling = load_node(node.children[child_idx - 1]);

    // Move a key from parent to child
    for (int i = child.num_keys; i > 0; i--)
    {
        child.keys[i] = child.keys[i - 1];
    }

    if (!child.is_leaf)
    {
        for (int i = child.num_keys + 1; i > 0; i--)
        {
            child.children[i] = child.children[i - 1];
        }
        child.children[0] = sibling.children[sibling.num_keys];
    }
    else
    {
        for (int i = child.num_keys; i > 0; i--)
        {
            child.records[i] = child.records[i - 1];
        }
        child.records[0] = sibling.records[sibling.num_keys - 1];
    }

    child.keys[0] = node.keys[child_idx - 1];
    node.keys[child_idx - 1] = sibling.keys[sibling.num_keys - 1];

    child.num_keys++;
    sibling.num_keys--;

    save_node(node.children[child_idx - 1], sibling);
    save_node(node.children[child_idx], child);
    save_node(node_page_id, node);
}

void BTree::borrow_from_next(uint64_t node_page_id, int child_idx)
{
    BTreeNode node = load_node(node_page_id);
    BTreeNode child = load_node(node.children[child_idx]);
    BTreeNode sibling = load_node(node.children[child_idx + 1]);

    child.keys[child.num_keys] = node.keys[child_idx];

    if (!child.is_leaf)
    {
        child.children[child.num_keys + 1] = sibling.children[0];
        for (int i = 0; i < sibling.num_keys; i++)
        {
            sibling.children[i] = sibling.children[i + 1];
        }
    }
    else
    {
        child.records[child.num_keys] = sibling.records[0];
        for (int i = 0; i < sibling.num_keys - 1; i++)
        {
            sibling.records[i] = sibling.records[i + 1];
        }
    }

    node.keys[child_idx] = sibling.keys[0];

    for (int i = 0; i < sibling.num_keys - 1; i++)
    {
        sibling.keys[i] = sibling.keys[i + 1];
    }

    child.num_keys++;
    sibling.num_keys--;

    save_node(node.children[child_idx], child);
    save_node(node.children[child_idx + 1], sibling);
    save_node(node_page_id, node);
}

void BTree::merge(uint64_t node_page_id, int child_idx)
{
    BTreeNode node = load_node(node_page_id);
    BTreeNode child = load_node(node.children[child_idx]);
    BTreeNode sibling = load_node(node.children[child_idx + 1]);

    // Pull key from parent and merge with right sibling
    child.keys[child.num_keys] = node.keys[child_idx];

    // Copy keys from sibling
    for (int i = 0; i < sibling.num_keys; i++)
    {
        child.keys[child.num_keys + 1 + i] = sibling.keys[i];
    }

    if (!child.is_leaf)
    {
        for (int i = 0; i <= sibling.num_keys; i++)
        {
            child.children[child.num_keys + 1 + i] = sibling.children[i];
        }
    }
    else
    {
        for (int i = 0; i < sibling.num_keys; i++)
        {
            child.records[child.num_keys + 1 + i] = sibling.records[i];
        }
        child.next_leaf = sibling.next_leaf;
    }

    child.num_keys += sibling.num_keys + 1;

    // Remove key from parent
    for (int i = child_idx; i < node.num_keys - 1; i++)
    {
        node.keys[i] = node.keys[i + 1];
        node.children[i + 1] = node.children[i + 2];
    }
    node.num_keys--;

    save_node(node.children[child_idx], child);
    save_node(node_page_id, node);

    // Free the sibling page
    db_engine->free_page(node.children[child_idx + 1]);
}

void BTree::remove_from_leaf(BTreeNode &node, int idx)
{
    for (int i = idx; i < node.num_keys - 1; i++)
    {
        node.keys[i] = node.keys[i + 1];
        node.records[i] = node.records[i + 1];
    }
    node.num_keys--;
}

void BTree::remove_from_non_leaf(uint64_t node_page_id, int idx)
{
    BTreeNode node = load_node(node_page_id);
    uint64_t key = node.keys[idx];

    BTreeNode left_child = load_node(node.children[idx]);
    BTreeNode right_child = load_node(node.children[idx + 1]);

    int min_keys = MAX_KEYS / 2;

    if (left_child.num_keys > min_keys)
    {
        // Get predecessor
        uint64_t pred_page = node.children[idx];
        BTreeNode pred_node = load_node(pred_page);
        while (!pred_node.is_leaf)
        {
            pred_page = pred_node.children[pred_node.num_keys];
            pred_node = load_node(pred_page);
        }
        uint64_t pred_key = pred_node.keys[pred_node.num_keys - 1];

        remove_internal(node.children[idx], pred_key);
        node = load_node(node_page_id);
        node.keys[idx] = pred_key;
        save_node(node_page_id, node);
    }
    else if (right_child.num_keys > min_keys)
    {
        // Get successor
        uint64_t succ_page = node.children[idx + 1];
        BTreeNode succ_node = load_node(succ_page);
        while (!succ_node.is_leaf)
        {
            succ_page = succ_node.children[0];
            succ_node = load_node(succ_page);
        }
        uint64_t succ_key = succ_node.keys[0];

        remove_internal(node.children[idx + 1], succ_key);
        node = load_node(node_page_id);
        node.keys[idx] = succ_key;
        save_node(node_page_id, node);
    }
    else
    {
        // Merge with right sibling
        merge(node_page_id, idx);
        remove_internal(node.children[idx], key);
    }
}

void BTree::fill_child(uint64_t node_page_id, int child_idx)
{
    BTreeNode node = load_node(node_page_id);

    // Borrow from previous sibling if possible
    if (child_idx != 0)
    {
        BTreeNode prev_sibling = load_node(node.children[child_idx - 1]);
        if (prev_sibling.num_keys > MAX_KEYS / 2)
        {
            borrow_from_prev(node_page_id, child_idx);
            return;
        }
    }

    // Borrow from next sibling if possible
    if (child_idx != node.num_keys)
    {
        BTreeNode next_sibling = load_node(node.children[child_idx + 1]);
        if (next_sibling.num_keys > MAX_KEYS / 2)
        {
            borrow_from_next(node_page_id, child_idx);
            return;
        }
    }

    // Merge with sibling
    if (child_idx != node.num_keys)
    {
        merge(node_page_id, child_idx);
    }
    else
    {
        merge(node_page_id, child_idx - 1);
    }
}

void BTree::remove_internal(uint64_t node_page_id, uint64_t key)
{
    BTreeNode node = load_node(node_page_id);
    int idx = search_key_position(node, key);

    if (node.is_leaf)
    {
        if (idx < node.num_keys && node.keys[idx] == key)
        {
            remove_from_leaf(node, idx);
            save_node(node_page_id, node);
        }
        return;
    }

    bool found_in_node = (idx < node.num_keys && node.keys[idx] == key);

    if (found_in_node)
    {
        remove_from_non_leaf(node_page_id, idx);
        return;
    }

    // Key is in subtree
    BTreeNode child = load_node(node.children[idx]);
    bool is_in_subtree = (idx == node.num_keys || key < node.keys[idx]);

    if (child.num_keys <= MAX_KEYS / 2)
    {
        fill_child(node_page_id, idx);
        node = load_node(node_page_id);

        // After filling, child might have merged, adjust idx
        if (idx > node.num_keys)
        {
            idx = node.num_keys;
        }

        // Check if key is now in current node
        if (idx < node.num_keys && node.keys[idx] == key)
        {
            remove_from_non_leaf(node_page_id, idx);
            return;
        }

        // Recalculate position
        idx = search_key_position(node, key);
    }

    remove_internal(node.children[idx], key);
}

bool BTree::remove(uint64_t key)
{
    if (root_page_id == 0)
    {
        return false;
    }

    remove_internal(root_page_id, key);

    // If root is empty after deletion, make its only child the new root
    BTreeNode root = load_node(root_page_id);
    if (root.num_keys == 0 && !root.is_leaf)
    {
        uint64_t old_root = root_page_id;
        root_page_id = root.children[0];
        db_engine->free_page(old_root);
    }

    return true;
}