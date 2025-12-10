# B-Tree Index File Handler - Deletion Algorithm

This document explains the deletion flow in the B-Tree index file implementation and provides detailed explanations of each function.

## Table of Contents
- [Deletion Flow](#deletion-flow)
- [Function Explanations](#function-explanations)
  - [Helper Functions](#helper-functions)
  - [Underflow Handling Functions](#underflow-handling-functions)
  - [Public Functions](#public-functions)

---

## Deletion Flow

The deletion process in a B-Tree follows these steps:

```
┌─────────────────────────────────────────────────────────────────┐
│                    DeleteARecord(key)                            │
└─────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│ Step 1: Search for the record using searchARecordInIndex()      │
│         - Traverse from root to leaf                            │
│         - Build path of parent nodes                            │
│         - Return path with found record at end                  │
└─────────────────────────────────────────────────────────────────┘
                              │
                              ▼
                    ┌─────────────────┐
                    │  Record Found?  │
                    └─────────────────┘
                      │           │
                     No          Yes
                      │           │
                      ▼           ▼
              ┌──────────┐  ┌────────────────────────────────────┐
              │  Throw   │  │ Step 2: Delete key from leaf node  │
              │  Error   │  │         using deleteAtNode()       │
              └──────────┘  └────────────────────────────────────┘
                                          │
                                          ▼
┌─────────────────────────────────────────────────────────────────┐
│ Step 3: If deleted key was MAX of node:                         │
│         - Update all parent entries via updateParentsMax()      │
└─────────────────────────────────────────────────────────────────┘
                              │
                              ▼
                    ┌─────────────────┐
                    │   Underflow?    │
                    │ (keys < minKeys │
                    │  AND not root)  │
                    └─────────────────┘
                      │           │
                     No          Yes
                      │           │
                      ▼           ▼
              ┌──────────┐  ┌────────────────────────────────────┐
              │   Done   │  │ Step 4: handleUnderflow()          │
              └──────────┘  └────────────────────────────────────┘
                                          │
                                          ▼
                           ┌──────────────────────────┐
                           │  Get Sibling Information │
                           │  via getSiblingInfo()    │
                           └──────────────────────────┘
                                          │
                    ┌─────────────────────┼─────────────────────┐
                    │                     │                     │
                    ▼                     ▼                     ▼
        ┌───────────────────┐ ┌───────────────────┐ ┌───────────────────┐
        │ Right sibling has │ │ Left sibling has  │ │ Neither sibling   │
        │ extra keys?       │ │ extra keys?       │ │ has extra keys    │
        └───────────────────┘ └───────────────────┘ └───────────────────┘
                    │                     │                     │
                   Yes                   Yes                    │
                    │                     │                     │
                    ▼                     ▼                     ▼
        ┌───────────────────┐ ┌───────────────────┐ ┌───────────────────┐
        │ borrowFromRight() │ │ borrowFromLeft()  │ │   mergeNodes()    │
        │ - Move first key  │ │ - Move last key   │ │ - Merge with      │
        │   from right      │ │   from left       │ │   sibling         │
        │ - Update parent   │ │ - Update parent   │ │ - Update free list│
        │   max key         │ │   max key         │ │ - Remove parent   │
        └───────────────────┘ └───────────────────┘ │   entry           │
                    │                     │         └───────────────────┘
                    │                     │                     │
                    ▼                     ▼                     ▼
              ┌──────────┐         ┌──────────┐    ┌───────────────────┐
              │   Done   │         │   Done   │    │ Check parent for  │
              └──────────┘         └──────────┘    │ collapse/underflow│
                                                   └───────────────────┘
                                                            │
                                    ┌───────────────────────┼───────────────────────┐
                                    │                       │                       │
                                    ▼                       ▼                       ▼
                        ┌───────────────────┐   ┌───────────────────┐   ┌───────────────────┐
                        │ Parent has only   │   │ Parent has        │   │ Parent is root    │
                        │ 1 child?          │   │ underflow?        │   │ (record 1)?       │
                        └───────────────────┘   └───────────────────┘   └───────────────────┘
                                    │                       │                       │
                                   Yes                     Yes                     Yes
                                    │                       │                       │
                                    ▼                       ▼                       ▼
                        ┌───────────────────┐   ┌───────────────────┐   ┌───────────────────┐
                        │ COLLAPSE:         │   │ RECURSIVE:        │   │ Done (root can    │
                        │ Pull child content│   │ handleUnderflow() │   │ have < minKeys)   │
                        │ up to parent      │   │ on parent         │   └───────────────────┘
                        └───────────────────┘   └───────────────────┘
```

### Key Concepts

1. **minKeys**: The minimum number of keys a node (except root) must have = `floor(m/2)`
2. **Underflow**: When a node has fewer than `minKeys` after deletion
3. **Borrow**: Take a key from a sibling that has more than `minKeys`
4. **Merge**: Combine two siblings when neither can spare a key
5. **Collapse**: When a parent has only one child after merge, pull child's content up

---

## Function Explanations

### Helper Functions

#### `getMinKeys()`
```cpp
int getMinKeys() { return handler->m / 2; }
```
Returns the minimum number of keys a non-root node must contain. Calculated as `floor(m/2)` where `m` is the maximum number of keys per node.

---

#### `countKeysFrom(IndexNode node)`
```cpp
int countKeysFrom(IndexNode node)
```
**Purpose**: Counts the number of valid keys in a record starting from the given node.

**How it works**:
- Iterates through the record from the given position
- Counts nodes until it encounters a key value of `-1` (empty slot)
- Returns the total count

**Parameters**:
- `node`: Starting position in the record

**Returns**: Number of valid keys in the record

---

#### `getMaxKeyNode(IndexNode node)`
```cpp
IndexNode getMaxKeyNode(IndexNode node)
```
**Purpose**: Finds the node containing the maximum key in a record.

**How it works**:
- Iterates through all valid keys in the record
- Keeps track of the last valid node encountered
- The last valid node contains the maximum key (since keys are sorted)

**Parameters**:
- `node`: First node of the record

**Returns**: IndexNode containing the maximum key

---

#### `findKeyNode(IndexNode node, int key)`
```cpp
IndexNode findKeyNode(IndexNode node, int key)
```
**Purpose**: Searches for a specific key within a record.

**How it works**:
- Iterates through the record
- Compares each key with the target
- Returns the node if found, or a node with `key=-1` if not found

**Parameters**:
- `node`: Starting position in the record
- `key`: The key to search for

**Returns**: IndexNode with the key, or node with `key=-1` if not found

---

#### `deleteAtNode(IndexNode deleteNode)`
```cpp
void deleteAtNode(IndexNode deleteNode)
```
**Purpose**: Deletes a key-address pair at the specified position and shifts remaining entries left.

**How it works**:
1. Gets the record boundaries
2. Starting from the delete position, copies each subsequent entry to the previous position
3. Clears the last slot by setting both key and address to `-1`

**Parameters**:
- `deleteNode`: The node position to delete

**Side Effects**: Modifies the index file by shifting entries and clearing the last slot

---

#### `insertAtNode(IndexNode insertNode, int key, int addr)`
```cpp
void insertAtNode(IndexNode insertNode, int key, int addr)
```
**Purpose**: Inserts a new key-address pair at the specified position, shifting existing entries right.

**How it works**:
1. Finds the first empty slot in the record
2. Shifts all entries from the insert position to the end, moving right
3. Writes the new key-address pair at the insert position

**Parameters**:
- `insertNode`: Position where the new entry should be inserted
- `key`: The key to insert
- `addr`: The address (or child pointer) to insert

---

#### `getSiblingInfo(vector<IndexNode> &path, int childNodeIndex)`
```cpp
SiblingInfo getSiblingInfo(vector<IndexNode> &path, int childNodeIndex)
```
**Purpose**: Retrieves information about a node's parent and siblings from the traversal path.

**How it works**:
1. Searches backwards through the path for an entry pointing to `childNodeIndex`
2. When found, identifies:
   - `parentNode`: The entry in the parent that points to this node
   - `leftSibling`: The entry immediately before in the parent (if exists)
   - `rightSibling`: The entry immediately after in the parent (if exists)

**Parameters**:
- `path`: Vector of nodes representing the traversal path from root to current node
- `childNodeIndex`: The record number of the node we need sibling info for

**Returns**: `SiblingInfo` struct containing parent and sibling information

---

### Underflow Handling Functions

#### `borrowFromRight(int leafNode, SiblingInfo &siblings)`
```cpp
void borrowFromRight(int leafNode, SiblingInfo &siblings)
```
**Purpose**: Borrows the first (smallest) key from the right sibling when the current node has underflow.

**How it works**:
1. Gets the first key from the right sibling
2. Deletes that key from the right sibling
3. Appends the borrowed key to the end of the current node
4. Updates the parent's entry for current node with the new maximum key

**Parameters**:
- `leafNode`: Record number of the node with underflow
- `siblings`: Sibling information containing parent and right sibling references

**Visual Example**:
```
Before:                          After:
Parent: [..., 5→A, 8→B, ...]    Parent: [..., 6→A, 8→B, ...]
Node A: [3, 5]  (underflow)     Node A: [3, 5, 6]
Node B: [6, 7, 8]               Node B: [7, 8]
```

---

#### `borrowFromLeft(int leafNode, SiblingInfo &siblings)`
```cpp
void borrowFromLeft(int leafNode, SiblingInfo &siblings)
```
**Purpose**: Borrows the last (largest) key from the left sibling when the current node has underflow.

**How it works**:
1. Gets the last (max) key from the left sibling
2. Clears that slot in the left sibling
3. Inserts the borrowed key at the beginning of the current node
4. Updates the parent's entry for the left sibling with its new maximum key

**Parameters**:
- `leafNode`: Record number of the node with underflow
- `siblings`: Sibling information containing parent and left sibling references

**Visual Example**:
```
Before:                          After:
Parent: [..., 5→A, 7→B, ...]    Parent: [..., 4→A, 7→B, ...]
Node A: [3, 4, 5]               Node A: [3, 4]
Node B: [7]  (underflow)        Node B: [5, 7]
```

---

#### `mergeNodes(int dstRecord, int srcRecord, IndexNode dstParentEntry, IndexNode srcParentEntry)`
```cpp
int mergeNodes(int dstRecord, int srcRecord, IndexNode dstParentEntry, IndexNode srcParentEntry)
```
**Purpose**: Merges two sibling nodes when neither can spare a key for borrowing.

**How it works**:
1. Copies all keys from `srcRecord` to the end of `dstRecord`
2. Clears all slots in `srcRecord`
3. Marks `srcRecord` as free and adds it to the free list:
   - Sets node type to `-1`
   - Points to previous free list head
   - Updates record 0's free pointer to point to this record
4. Removes both parent entries
5. Inserts a single new parent entry pointing to the merged node

**Parameters**:
- `dstRecord`: Destination record (will contain merged content)
- `srcRecord`: Source record (will be freed)
- `dstParentEntry`: Parent's entry pointing to destination
- `srcParentEntry`: Parent's entry pointing to source

**Returns**: The parent's record number (for recursive underflow checking)

**Visual Example**:
```
Before:                          After:
Parent: [3→A, 5→B, 9→C]         Parent: [5→A, 9→C]
Node A: [1, 2, 3]               Node A: [1, 2, 3, 4, 5]
Node B: [4, 5] (underflow)      Node B: [FREE → old head]
Free List: 0 → 7 → ...          Free List: 0 → B → 7 → ...
```

---

#### `handleUnderflow(int nodeRecord, vector<IndexNode> &path)`
```cpp
void handleUnderflow(int nodeRecord, vector<IndexNode> &path)
```
**Purpose**: Main orchestrator for handling underflow conditions. Decides whether to borrow or merge, and handles recursive underflow in parent nodes.

**How it works**:
1. Gets sibling information for the underflowing node
2. Checks if right sibling can spare a key → `borrowFromRight()`
3. Else checks if left sibling can spare a key → `borrowFromLeft()`
4. Else must merge with a sibling → `mergeNodes()`
5. After merge:
   - If parent has only 1 child → **Collapse**: pull child content up to parent
   - If parent has underflow → **Recursive** call to `handleUnderflow()`
   - If parent is root → Done (root can have fewer than minKeys)

**Parameters**:
- `nodeRecord`: Record number of the node with underflow
- `path`: Traversal path for finding parent/sibling information

---

### Public Functions

#### `Index(IndexFileHandler *handler)`
```cpp
Index(IndexFileHandler *handler)
```
**Purpose**: Constructor that initializes the Index with a reference to the file handler.

**Parameters**:
- `handler`: Pointer to IndexFileHandler that manages the binary file operations

---

#### `updateParentsMax(vector<IndexNode> &path, int oldMax, int newMax)`
```cpp
void updateParentsMax(vector<IndexNode> &path, int oldMax, int newMax)
```
**Purpose**: Updates parent entries when a node's maximum key changes (after deletion).

**How it works**:
- Traverses the path from leaf to root
- Updates any parent entry where the key equals `oldMax`
- Stops when encountering an entry with a different key

**Parameters**:
- `path`: Traversal path containing parent nodes
- `oldMax`: The previous maximum key that was deleted
- `newMax`: The new maximum key of the node

---

#### `searchARecordInIndex(char *filename, int RecordID)`
```cpp
vector<IndexNode> searchARecordInIndex(char *filename, int RecordID)
```
**Purpose**: Searches for a key in the B-Tree and returns the path taken.

**How it works**:
1. Starts at the root (record 1)
2. At each internal node:
   - Finds the first key ≥ search key
   - Adds the entry to the path
   - Follows the child pointer
3. At leaf node:
   - Searches for exact key match
   - If found, adds to path and returns
   - If not found, returns empty vector

**Parameters**:
- `filename`: Path to the index file
- `RecordID`: The key to search for

**Returns**: Vector of IndexNodes representing the path (empty if not found)

---

#### `SearchARecord(char *filename, int RecordID)`
```cpp
int SearchARecord(char *filename, int RecordID)
```
**Purpose**: Public interface for searching a key and returning its associated address.

**Parameters**:
- `filename`: Path to the index file
- `RecordID`: The key to search for

**Returns**: 
- The address associated with the key if found
- `-1` if not found

---

#### `DeleteARecord(char *filename, int RecordID)`
```cpp
void DeleteARecord(char *filename, int RecordID)
```
**Purpose**: Deletes a key from the B-Tree index.

**How it works**:
1. Search for the record → get path
2. Delete the key from the leaf node
3. If deleted key was max → update parent entries
4. If node has underflow → handle it (borrow/merge)

**Parameters**:
- `filename`: Path to the index file
- `RecordID`: The key to delete

**Throws**: `runtime_error` if record not found

---

## Data Structures

### IndexNode
```cpp
struct IndexNode {
    int key;      // The key value
    int address;  // Data address (leaf) or child record number (internal)
    int pos;      // Byte position in the file
}
```

### SiblingInfo
```cpp
struct SiblingInfo {
    IndexNode parentNode;    // Parent entry pointing to current node
    IndexNode leftSibling;   // Entry for left sibling (key=-1 if none)
    IndexNode rightSibling;  // Entry for right sibling (key=-1 if none)
    bool hasParent;          // Whether a valid parent was found
}
```

---

## File Structure

Each record in the index file has the following format:
```
[nodeType, key1, addr1, key2, addr2, ..., keyM, addrM]
```

- **nodeType**: 
  - `0` = leaf node
  - `1` = internal node  
  - `-1` = free/empty node
- **keys**: Sorted in ascending order
- **addresses**: 
  - In leaf nodes: data record addresses
  - In internal nodes: child record numbers
- **Empty slots**: Marked with `key = -1, address = -1`

### Free List
- Record 0 stores the free list head at position 1 (first "key" slot)
- Each free record points to the next free record
- Last free record points to `-1`
