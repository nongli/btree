#ifndef BTREE_V1_H
#define BTREE_V1_H

#include <stdio.h>
#include <string.h>
#include <cstdint>
#include <sstream>

// Implementation of a B+ tree. This is probably "atypical" in at least these ways:
//  - Maintains parent pointers
//  - Every level is a doubly-linked list
//  - Every internal node maintains the min/max for the subtree under it.
// These modifications make concurrency pretty much impossible.
class BTreeV1 {
 public:
  BTreeV1() : size_(0) {
    root_ = new Node(true, NULL);
    root_->prev = root_->next = NULL;
    root_->num_values = 0;
  }

  ~BTreeV1() {
    Delete(root_);
  }

  class Iterator {
   public:
    bool AtEnd() { return parent_ == NULL; }

   private:
    friend class BTreeV1;
    Iterator(const BTreeV1* parent = NULL, void* value = NULL)
      : parent_(parent), value_(value ){
     }

    const BTreeV1* parent_;
    void* value_;
  };

  Iterator Find(int64_t key) const {
    //printf("BTREE: Finding %ld\n", key);
    Node* leaf_node = FindLeafNode(root_, key, false);
    if (leaf_node == NULL) return End();
    assert(leaf_node->is_leaf());
    int idx = IndexOfKey(leaf_node, key);
    if (idx == -1) return End();
    return Iterator(this, leaf_node->values[idx].value);
  }

  bool Update(int64_t key, void* value) {
    //printf("BTREE: Update %ld\n", key);
    Node* leaf_node = FindLeafNode(root_, key, false);
    if (leaf_node == NULL) return false;
    assert(leaf_node->is_leaf());
    int idx = IndexOfKey(leaf_node, key);
    if (idx == -1) return false;
    leaf_node->values[idx].value = value;
    return true;
  }

  Iterator Insert(int64_t key, void* value) {
    //printf("BTREE: Inserting %ld\n", key);
    Node* leaf_node = FindLeafNode(root_, key, true);
    if (leaf_node == NULL) return End();
    assert(leaf_node->is_leaf());
    if (!InsertInNode<true>(leaf_node, key, value)) return End();
    VerifyTreeIntegrity();
    ++size_;
    return Iterator(this, value);
  }

  Iterator Upsert(int64_t key, void* value) {
    //printf("BTREE: Upsert %ld\n", key);
    Node* leaf_node = FindLeafNode(root_, key, true);
    if (leaf_node == NULL) return End();
    assert(leaf_node->is_leaf());
    int idx = IndexOfKey(leaf_node, key);
    if (idx == -1) {
      if (!InsertInNode<true>(leaf_node, key, value)) return End();
      ++size_;
    } else {
      leaf_node->values[idx].value = value;
    }
    return Iterator(this, value);
  }

  bool Remove(int64_t key) {
    //printf("BTREE: Removing %ld\n", key);
    Node* leaf_node = FindLeafNode(root_, key, false);
    if (leaf_node == NULL) return false;
    assert(leaf_node->is_leaf());
    if (!RemoveKeyFromNode(leaf_node, key)) return false;
    VerifyTreeIntegrity();
    --size_;
    return true;
  }

  int64_t size() const { return size_; }

  Iterator End() const { return Iterator(); }

  void DebugPrint() const {
    printf("Printing Tree:\n");
    PrintNode(root_, 0);
  }

  void CollectAllKeys(std::vector<int64_t>* keys, bool backwards = false) const {
    keys->clear();
    Node* node = root_;
    // Get the left most/right most child.
    while (node->is_internal()) {
      if (backwards) {
        node = GetChildNode(node, node->num_values - 1);
      } else {
        node = GetChildNode(node, 0);
      }
    }
    while (node != NULL) {
      for (int i = 0; i < node->num_values; ++i) {
        int idx = backwards ? (node->num_values - i - 1) : i;
        keys->push_back(node->values[idx].key);
      }
      if (backwards) {
        node = node->prev;
      } else {
        node = node->next;
      }
    }
  }

 private:
  struct Node;

  struct Link {
    int64_t key;
    union {
      // For leaf nodes.
      void* value;

      // For internal nodes.
      Node* node;
    };
  };

  struct Node {
    Node* parent;
    bool is_leaf_;
    int32_t num_values;
    int64_t min_key; // Only used for internal nodes.
    Link values[ORDER];

    // Only used for leaf nodes
    Node* prev;
    Node* next;

    bool is_leaf() const { return is_leaf_; }
    bool is_internal() const { return !is_leaf_; }
    Node(bool is_leaf, Node* parent) : parent(parent), is_leaf_(is_leaf) {}
  };

  void MoveValues(Node* node, int dst_idx, int src_idx, int n) const {
    if (n == 0) return;
    memmove(&node->values[dst_idx], &node->values[src_idx], n * sizeof(Link));
  }

  void CopyValues(Node* dst, int dst_idx, Node* src, int src_idx, int n) const {
    assert(dst != src);
    assert(dst->parent == src->parent);
    memcpy(&dst->values[dst_idx], &src->values[src_idx], n * sizeof(Link));
    if (dst->is_internal()) {
      // Moving values between internal nodes. Need to update the parent pointer for
      // all the nodes from src that were moved.
      for (int i = 0; i < n; ++i) {
        dst->values[i + dst_idx].node->parent = dst;
      }
    }
  }

  // Moves src[src_idx] into dst[dst_idx];
  void MoveNode(Node* dst, int dst_idx, Node* src, int src_idx) const {
    dst->values[dst_idx] = src->values[src_idx];
    if (dst->is_internal()) dst->values[dst_idx].node->parent = dst;
    ++dst->num_values;
    --src->num_values;
    assert(dst->num_values <= ORDER);
    assert(src->num_values >= 1);
  }

  // Returns the largest key in the subtree from node.
  int64_t LargestKey(const Node* node) const {
    assert(node->num_values > 0);
    return node->values[node->num_values - 1].key;
  }

  // Returns the smallest key in the subtree from node.
  int64_t SmallestKey(const Node* node) const {
    assert(node->num_values > 0);
    return node->is_leaf() ? node->values[0].key : node->min_key;
  }

  // Returns the child node for parent. Parent must be an internal node.
  Node* GetChildNode(const Node* parent, int idx) const {
    assert(parent->is_internal());
    assert(idx < parent->num_values);
    return parent->values[idx].node;
  }

  // Returns the index in node->values that contains key. Returns -1 if the key does
  // not exist.
  int IndexOfKey(const Node* node, int64_t key) const {
    for (int i = 0; i < node->num_values; ++i) {
      if (node->values[i].key == key) return i;
      if (node->values[i].key > key) return -1;
    }
    return -1;
  }

  // Updates the separator key in node->parent
  void UpdateParentSeparator(Node* node, int64_t old_key, int64_t new_key) const {
    Node* parent = node->parent;
    assert(parent != NULL);
    while (parent != NULL) {
      int separtor_idx = IndexOfKey(parent, old_key);
      assert(separtor_idx != -1);
      assert(GetChildNode(parent, separtor_idx) == node);
      parent->values[separtor_idx].key = new_key;
      if (separtor_idx == parent->num_values - 1) {
        // Just updated the max value in parent. We need to propagate up.
        node = parent;
        parent = parent->parent;
      } else {
        break;
      }
    }
  }

  // Updates min_key from node to the root with 'key'
  void PropagateMinKey(Node* node, int64_t key) const {
    while (node != NULL) {
      if (key >= node->min_key) break;
      node->min_key = key;
      node = node->parent;
    }
  }

  // node->values[idx] = {key, value}
  void AssignInNode(Node* node, int idx, int64_t key, void* value) const {
    node->values[idx].key = key;
    node->values[idx].value = value;
  }

  // Finds the child in node which can contain key. If insert, then this never returns
  // NULL and returns the leaf node to insert into. If not insert, returns NULL if this
  // key cannot be in any of the children.
  // node must be an internal node.
  Node* FindInInternalNode(Node* node, int64_t key, bool insert) const {
    assert(node->is_internal());
    assert(node->num_values > 0);
    if (key < node->min_key) return (insert ? GetChildNode(node, 0) : NULL);
    for (int i = 0; i < node->num_values; ++i) {
      if (key <= node->values[i].key) return GetChildNode(node, i);
    }
    return (insert ? GetChildNode(node, node->num_values - 1) : NULL);
  }

  // Finds the leaf node in the subtree from node which can contain the key. Returns NULL
  // if the key does not exist and insert is false.
  Node* FindLeafNode(Node* node, int64_t key, bool insert) const {
    while (node->is_internal()) {
      node = FindInInternalNode(node, key, insert);
      if (node == NULL) return NULL;
    }
    return node;
  }

  // Inserts right_sibling to the right of node, maintaining the doubly-linked list.
  void ConnectSiblingNode(Node* node, Node* right_sibling) const {
    right_sibling->next = node->next;
    right_sibling->prev = node;
    if (node->next != NULL) node->next->prev = right_sibling;
    node->next = right_sibling;
  }

  // Removes node from the doubly linked list.
  void RemoveNode(Node* node) {
    node->prev->next = node->next;
    if (node->next != NULL) node->next->prev = node->prev;
  }

  // Splits 'node' to insert a value at *value_idx. Returns the node that the value
  // should be inserted into and adjusts value_idx.
  // For example if the current node contains 7 elements and is full.
  //  1 2 4 5 6 8 9
  // If we are inserting 3 (value_idx == 3), this would return the left node after the
  // split with value_idx == 3. If we are inserting 7 (value_idx == 5), this would
  // return the right node with value_idx = 1
  Node* SplitNodeForInsert(Node* node, int* value_idx) {
    // Values from [0, split_idx] stay in node.
    // Values from [split_idx +1, ORDER] go to the new node.
    int split_idx = ORDER / 2;
    // Take into account where the new value will be inserted into to get a more even
    // split.
    if (*value_idx < split_idx) --split_idx;

    // Make the new node, copy the bottom half of the values and shrink the original node.
    Node* new_node = new Node(node->is_leaf(), node->parent);
    new_node->num_values = ORDER - split_idx - 1;
    node->num_values = split_idx + 1;
    CopyValues(new_node, 0, node, split_idx + 1, new_node->num_values);
    ConnectSiblingNode(node, new_node);

    if (new_node->is_internal()) {
      // This is an internal node. It needs the min key.
      new_node->min_key = SmallestKey(GetChildNode(new_node, 0));
    }

    // Update the parent chain.
    if (node->parent == NULL) {
      // Need a new root.
      Node* root = new Node(false, NULL);
      root->prev = root->next = NULL;
      AssignInNode(root, 0, LargestKey(node), node);
      AssignInNode(root, 1, LargestKey(new_node), new_node);
      root->num_values = 2;
      root->min_key = SmallestKey(node);
      node->parent = new_node->parent = root;
      root_ = root;
    } else {
      int64_t old_separator = LargestKey(new_node);
      UpdateParentSeparator(node, old_separator, LargestKey(node));
      InsertInNode<false>(node->parent, old_separator, new_node);
    }

    if (*value_idx > split_idx) {
      // Inserting into new node, shift the index by split_idx.
      *value_idx -= (split_idx + 1);
      return new_node;
    }
    return node;
  }

  // Inserts (key, value) into node, splitting as necessary. Returns true if the
  // insert is successful or false if the key already exists in which case the value
  // is not inserted.
  template<bool IS_VALUE>
  bool InsertInNode(Node* node, int64_t key, void* value) {
    int i = 0;
    for (; i < node->num_values; ++i) {
      if (node->values[i].key < key) continue;
      if (node->values[i].key == key) return false;
      break;
    }

    // Node is full. Split it before inserting.
    if (node->num_values == ORDER) {
      node = SplitNodeForInsert(node, &i);
      assert(node->num_values < ORDER);
      if (node->is_internal()) reinterpret_cast<Node*>(value)->parent = node;
    }

    if (i == node->num_values && node->parent != NULL) {
      // Propagate max up to the root.
      UpdateParentSeparator(node, LargestKey(node), key);
    }

    if (i == 0) {
      // Propagate min up to the root.
      int64_t min_key = IS_VALUE ? key : SmallestKey(reinterpret_cast<Node*>(value));
      if (node->is_internal()) node->min_key = min_key;
      PropagateMinKey(node->parent, min_key);
    }

    MoveValues(node, i + 1, i, node->num_values - i);
    AssignInNode(node, i, key, value);
    ++node->num_values;
    return true;
  }

  // Removes key from the node. If the key does not exist, returns false and
  // nothing is removed.
  bool RemoveKeyFromNode(Node* node, int64_t key) {
    int key_idx = IndexOfKey(node, key);
    if (key_idx == -1) return false;

    MoveValues(node, key_idx, key_idx + 1, node->num_values - key_idx - 1);
    --node->num_values;

    // Propagate min and separators up to the root.
    if (node->parent != NULL) {
      if (key_idx == 0) PropagateMinKey(node->parent, SmallestKey(node));

      // Propagate separators up the root.
      if (key_idx == node->num_values) {
        int64_t new_key = LargestKey(node);
        Node* parent = node->parent;
        while (parent != NULL) {
          int separator_idx = IndexOfKey(parent, key);
          if (separator_idx == -1) break;
          parent->values[separator_idx].key = new_key;
          if (separator_idx != parent->num_values - 1) break;
          parent = parent->parent;
        }
      }
    }

    // Need to rebalance.
    if (node != root_ && node->num_values < ORDER / 2) RebalanceNode(node);

    if (node == root_ && node->is_internal() && node->num_values == 1) {
      // In this case, we've collapsed to the root which now only has 1 child. Replace the
      // root with its child and delete the root.
      root_ = GetChildNode(node, 0);
      root_->parent = NULL;
      delete node;
    }

    return true;
  }

  // Rebalances node because it is too small. This can either pull a value from one of
  // its siblings in which case no nodes are deleted. If there are too few values, it
  // is combined with its sibling and a node is deleted.
  void RebalanceNode(Node* node) {
    assert(node->num_values < ORDER / 2);
    if (node->prev != NULL && node->prev->parent == node->parent) {
      int64_t old_separator_key = LargestKey(node->prev);
      if (node->prev->num_values > ORDER / 2) {
        // Rebalance by stealing from my prev sibling. Move node's values over one and
        // take the prev node's last value. Update the parent separators.
        MoveValues(node, 1, 0, node->num_values);
        MoveNode(node, 0, node->prev, node->prev->num_values - 1);
        UpdateParentSeparator(node->prev, old_separator_key, LargestKey(node->prev));
        if (node->is_internal()) PropagateMinKey(node, SmallestKey(GetChildNode(node, 0)));
      } else {
        // Move node into node->prev.
        CopyValues(node->prev, node->prev->num_values, node, 0, node->num_values);
        node->prev->num_values += node->num_values;
        assert(node->prev->num_values <= ORDER);

        // Fix up the side links and delete the removed node.
        // Update parent link to point to new node.
        int separtor_idx = IndexOfKey(node->parent, LargestKey(node));
        assert(separtor_idx != -1);
        assert(GetChildNode(node->parent, separtor_idx) == node);
        node->parent->values[separtor_idx].node = node->prev;

        // Fix up the separators in the parent which can call this recursively.
        RemoveKeyFromNode(node->parent, old_separator_key);

        // Finally fix up the side links and delete the node.
        RemoveNode(node);
        delete node;
      }
    } else if (node->next != NULL && node->next->parent == node->parent) {
      int64_t old_separator_key = LargestKey(node);
      if (node->next->num_values > ORDER / 2) {
        // Rebalance by stealing from node's next sibling. Take next's first value and
        // shift next's values over one. Update parent separators.
        MoveNode(node, node->num_values, node->next, 0);
        MoveValues(node->next, 0, 1, node->next->num_values);
        UpdateParentSeparator(node, old_separator_key, LargestKey(node));
      } else {
        // Move node->next into node.
        CopyValues(node, node->num_values, node->next, 0, node->next->num_values);
        node->num_values += node->next->num_values;
        assert(node->num_values <= ORDER);

        // Update parent link to point to new node.
        int separtor_idx = IndexOfKey(node->parent, LargestKey(node->next));
        assert(separtor_idx != -1);
        assert(GetChildNode(node->parent, separtor_idx) == node->next);
        node->parent->values[separtor_idx].node = node;

        // Fix up the separators in the parent which can call this recursively.
        RemoveKeyFromNode(node->parent, old_separator_key);

        // Finally fix up the side links and delete the node.
        // node <--> node->next <--> [some node]
        // and want to delete node->next. Some node can be NULL.
        Node* node_to_delete = node->next;
        RemoveNode(node_to_delete);
        delete node_to_delete;
      }
    } else {
      printf("Invalid BTree!\n");
      DebugPrint();
      assert(false);
    }
  }

  // Depth first traversal to delete the entire tree. This *cannot* be used to delete a
  // subtree.
  void Delete(Node* node) {
    if (node->is_internal()) {
      for (int i = 0; i < node->num_values; ++i) {
        Delete(GetChildNode(node, i));
      }
    }
    delete node;
  }

  void PrintNode(const Node* node, int level = -1) const {
    std::stringstream ss;
    if (level != -1) {
      ss << level << ": ";
    }
    ss << (node->is_leaf() ? "<" : "[");
    if (node->is_internal()) {
      ss << "(" << node->min_key << ") ";
    }
    for (int i = 0; i < node->num_values; ++i) {
      if (i != 0) ss << " ";
      ss << node->values[i].key;
    }
    ss << (node->is_leaf() ? ">" : "]");
    printf("%s\n", ss.str().c_str());
    if (level != -1 && node->is_internal()) {
      for (int i = 0; i < node->num_values; ++i) {
        PrintNode(GetChildNode(node, i), level + 1);
      }
    }
  }

  void VerifyTreeIntegrity() {
#ifndef NDEBUG
    VerifyTreeIntegrity(root_, NULL);
    Node* node = root_;
    while (true) {
      // Verify doubly linked list
      Node* l = node;
      Node* prev = NULL;
      while (l->next != NULL) {
        assert(l->prev == prev);
        if (prev != NULL) assert(prev->next == l);
        prev = l;
        l = l->next;
      }
      if (node->is_leaf()) break;
      node = node->values[0].node;
    }
#endif
  }

  void VerifyTreeIntegrity(Node* node, Node* parent) {
    if (node != root_) {
      assert(node->num_values >= ORDER / 2);
      assert(node->num_values <= ORDER);
    }
    if (node->is_internal()) {
      assert(node->min_key < node->values[0].key);
      for (int i = 0; i < node->num_values; ++i) {
        Node* child = node->values[i].node;
        if (child->parent != node) {
          PrintNode(child);
        }
        assert(child->parent == node);
        VerifyTreeIntegrity(node->values[i].node, node);
      }
    }
  }

  // Number of values in tree.
  int64_t size_;

  // Root of the tree. Never NULL.
  Node* root_;
};

#endif
