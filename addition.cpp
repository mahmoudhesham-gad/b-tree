//
// Created by midoz on 12/9/2025.
//

#include "IndexFileHandler.cpp"
#include <vector>
#include <algorithm>

class BTreeAddition : public IndexFileHandler {
private:
    int m; // B-tree order
    int numberOfRecords;
    char* filename;

    struct Record {
        int key;
        int address;

        Record(int k = -1, int addr = -1) : key(k), address(addr) {}
    };

    struct Node {
        int nodeType; // 1 = internal, 0 = leaf
        int nextEmpty; // for free list
        vector<Record> records;

        Node() : nodeType(-1), nextEmpty(-1) {}
    };

    // Read a node from file
    Node readNode(int rowNum) {
        ifstream file(filename, ios::binary);
        if (!file) {
            throw runtime_error("Could not open file for reading");
        }

        Node node;
        int cols = 2 * m + 1;

        // Seek to the row
        file.seekg(rowNum * cols * sizeof(int), ios::beg);

        // Read node type (column 0)
        file.read(reinterpret_cast<char*>(&node.nodeType), sizeof(int));

        // For empty rows (nodeType=-1), column 1 is nextEmpty pointer
        if (node.nodeType == -1) {
            file.read(reinterpret_cast<char*>(&node.nextEmpty), sizeof(int));
            // Skip reading records for empty rows
            file.close();
            return node;
        }

        // For data nodes (type 0 or 1), records start from column 1
        node.nextEmpty = -1;

        // Read records (key, address pairs) - starting from column 1
        for (int i = 0; i < m; i++) {
            int key, addr;
            file.read(reinterpret_cast<char*>(&key), sizeof(int));
            file.read(reinterpret_cast<char*>(&addr), sizeof(int));

            if (key != -1) {
                node.records.push_back(Record(key, addr));
            }
        }

        file.close();
        return node;
    }

    // Write a node to file
    void writeNode(int rowNum, const Node& node) {
        fstream file(filename, ios::binary | ios::in | ios::out);
        if (!file) {
            throw runtime_error("Could not open file for writing");
        }

        int cols = 2 * m + 1;

        // Seek to the row
        file.seekp(rowNum * cols * sizeof(int), ios::beg);

        // Write node type (column 0)
        file.write(reinterpret_cast<const char*>(&node.nodeType), sizeof(int));

        // For empty rows (nodeType=-1), write nextEmpty in column 1
        if (node.nodeType == -1) {
            file.write(reinterpret_cast<const char*>(&node.nextEmpty), sizeof(int));
            // Fill rest with -1
            for (int i = 2; i < cols; i++) {
                int value = -1;
                file.write(reinterpret_cast<const char*>(&value), sizeof(int));
            }
            file.close();
            return;
        }

        // For data nodes (type 0 or 1), write records starting from column 1
        int recordIdx = 0;
        for (int i = 0; i < m; i++) {
            int key = -1, addr = -1;
            if (recordIdx < node.records.size()) {
                key = node.records[recordIdx].key;
                addr = node.records[recordIdx].address;
                recordIdx++;
            }
            file.write(reinterpret_cast<const char*>(&key), sizeof(int));
            file.write(reinterpret_cast<const char*>(&addr), sizeof(int));
        }

        file.close();
    }

    // Find the correct position for insertion
    int findInsertPosition(int key, int currentRow) {
        Node node = readNode(currentRow);

        // If leaf node (0), we found where to insert
        if (node.nodeType == 0) {
            return currentRow;
        }

        // Internal node (1), navigate to child
        for (int i = 0; i < node.records.size(); i++) {
            if (key < node.records[i].key) {
                return findInsertPosition(key, node.records[i].address);
            }
        }

        // Key is larger than all, go to rightmost child
        if (node.records.size() > 0) {
            return findInsertPosition(key, node.records.back().address);
        }

        return currentRow;
    }

    // Insert into a node and handle splits
    bool insertIntoNode(int rowNum, int key, int address, int& promotedKey, int& newChildRow) {
        Node node = readNode(rowNum);

        // Check if node is already full BEFORE inserting
        // A node can hold m records maximum
        if (node.records.size() >= m) {
            // Node is full, need to split
            // First, insert the new record temporarily
            node.records.push_back(Record(key, address));
            sort(node.records.begin(), node.records.end(),
                 [](const Record& a, const Record& b) { return a.key < b.key; });

            // Now split (we have m+1 records total)
            int medianIdx = (node.records.size()) / 2;

            // For parent: use LARGEST key from LEFT child
            promotedKey = node.records[medianIdx - 1].key;

            // Create new node for right half (same type as original)
            Node newNode;
            newNode.nodeType = node.nodeType;
            newNode.nextEmpty = -1;

            // Right half goes to new node (starts from median)
            for (int i = medianIdx; i < node.records.size(); i++) {
                newNode.records.push_back(node.records[i]);
            }

            // Left half stays in current node (everything before median)
            node.records.resize(medianIdx);

            // Find empty row for new node
            newChildRow = findEmptyRow();

            // Write both nodes
            writeNode(rowNum, node);
            writeNode(newChildRow, newNode);

            return true; // Split occurred
        } else {
            // Node is not full, just insert
            node.records.push_back(Record(key, address));
            sort(node.records.begin(), node.records.end(),
                 [](const Record& a, const Record& b) { return a.key < b.key; });
            writeNode(rowNum, node);
            return false;
        }
    }

    // Find an empty row from free list (skip row 0 which manages the free list)
    int findEmptyRow() {
        Node freeListHead = readNode(0);
        if (freeListHead.nextEmpty != -1) {
            int emptyRow = freeListHead.nextEmpty;
            Node emptyNode = readNode(emptyRow);
            // Update free list head to point to next empty
            freeListHead.nextEmpty = emptyNode.nextEmpty;
            writeNode(0, freeListHead);
            return emptyRow;
        }
        throw runtime_error("No empty rows available");
    }

    // Find parent of a given row, returns -1 if no parent (is root)
    int findParent(int targetRow, int currentRow, int& posInParent) {
        if (currentRow == targetRow) return -1;

        Node node = readNode(currentRow);

        // Check if any child matches
        for (int i = 0; i < node.records.size(); i++) {
            if (node.records[i].address == targetRow) {
                posInParent = i;
                return currentRow;
            }
        }

        // If internal node, search recursively in children
        if (node.nodeType == 1) {
            for (int i = 0; i < node.records.size(); i++) {
                int parent = findParent(targetRow, node.records[i].address, posInParent);
                if (parent != -1) return parent;
            }
        }

        return -1;
    }

    int rootRow = -1; // Track the root row

public:
    void initialize(char* fname, int numRecords, int order) {
        filename = fname;
        numberOfRecords = numRecords;
        m = order;
        rootRow = -1;
    }

    // Main addition function
    void addRecord(int key, int dataAddress) {
        // Find the first row with nodeType = 1 (internal) or 0 (leaf) as root
        // Start from row 1 (row 0 is for free list management)
        if (rootRow == -1) {
            for (int i = 1; i < numberOfRecords; i++) {
                Node node = readNode(i);
                if (node.nodeType == 1 || node.nodeType == 0) {
                    rootRow = i;
                    break;
                }
            }
        }

        if (rootRow == -1) {
            // No root exists, create first leaf node at row 1
            rootRow = findEmptyRow(); // This will get row 1 from free list
            Node root;
            root.nodeType = 0; // Leaf
            root.nextEmpty = -1;
            root.records.push_back(Record(key, dataAddress));
            writeNode(rootRow, root);
            return;
        }

        // Find correct leaf position
        int leafRow = findInsertPosition(key, rootRow);

        // Insert into leaf
        int promotedKey, newChildRow;
        bool split = insertIntoNode(leafRow, key, dataAddress, promotedKey, newChildRow);

        if (split) {
            // Handle split - need to promote to parent
            handleSplit(leafRow, promotedKey, newChildRow);
        }
    }

    void handleSplit(int leftChildRow, int promotedKey, int rightChildRow) {
        // Find parent of the child that was split
        int posInParent;
        int parentRow = findParent(leftChildRow, rootRow, posInParent);

        if (parentRow == -1) {
            // No parent exists, create new root
            // Internal nodes should be at lower row numbers than leaves

            // Get largest key from right child
            Node rightChild = readNode(rightChildRow);
            int rightLargestKey = rightChild.records.empty() ? 999999 : rightChild.records.back().key;

            Node newRoot;
            newRoot.nodeType = 1; // Internal
            newRoot.nextEmpty = -1;

            // Get row for new root
            int newRootRow = findEmptyRow();

            // If the new root row is higher than the left child row, swap them
            // We want internal nodes at lower rows, leaves at higher rows
            if (newRootRow > leftChildRow) {
                // Read the left child
                Node leftChild = readNode(leftChildRow);

                // Write left child to the newRootRow position (higher row number)
                writeNode(newRootRow, leftChild);

                // Update the parent to point to the new location
                newRoot.records.push_back(Record(promotedKey, newRootRow)); // Left child now at newRootRow
                newRoot.records.push_back(Record(rightLargestKey, rightChildRow)); // Right child stays

                // Write root to the lower row (leftChildRow)
                writeNode(leftChildRow, newRoot);
                rootRow = leftChildRow; // Update root tracker
            } else {
                // Normal case - root is already at lower row
                newRoot.records.push_back(Record(promotedKey, leftChildRow)); // Left child
                newRoot.records.push_back(Record(rightLargestKey, rightChildRow)); // Right child

                writeNode(newRootRow, newRoot);
                rootRow = newRootRow; // Update root
            }
        } else {
            // Parent exists, insert promoted key and new child pointer
            Node parent = readNode(parentRow);

            // Get the largest key in the right child
            Node rightChild = readNode(rightChildRow);
            int rightLargestKey = rightChild.records.empty() ? 999999 : rightChild.records.back().key;

            // Update the existing entry for leftChildRow to have the new promoted key
            parent.records[posInParent].key = promotedKey;

            // Insert the new entry for rightChildRow after it
            parent.records.insert(parent.records.begin() + posInParent + 1,
                                 Record(rightLargestKey, rightChildRow));

            // Check if parent needs to split
            if (parent.records.size() >= m) {
                int parentPromotedKey, newParentRow;
                // Recursively split parent
                writeNode(parentRow, parent);
                insertIntoNode(parentRow, rightLargestKey, rightChildRow, parentPromotedKey, newParentRow);
                handleSplit(parentRow, parentPromotedKey, newParentRow);
            } else {
                writeNode(parentRow, parent);
            }
        }
    }
};


