#include "IndexFileHandler.cpp"
#include <cmath>
#include <stdexcept>
#include <vector>
using namespace std;

class Index {
private:
  IndexFileHandler *handler;

  // Helper: Get minimum keys required (ceil(m/2))
  int getMinKeys() { return (int)ceil(handler->m / 2.0); }

  // Helper: Count keys in a node starting from an IndexNode
  int countKeysFrom(IndexFileHandler::IndexNode node) {
    int count = 0;
    for (int i = 0; i < handler->m; i++) {
      if (node.key == -1)
        break;
      count++;
      node = node.getNextRecord(handler->indexFileName, handler->fileFieldSize);
    }
    return count;
  }

  // Helper: Get max key node in a record starting from first node
  IndexFileHandler::IndexNode getMaxKeyNode(IndexFileHandler::IndexNode node) {
    IndexFileHandler::IndexNode maxNode = node;
    for (int i = 0; i < handler->m; i++) {
      if (node.key == -1)
        break;
      maxNode = node;
      node = node.getNextRecord(handler->indexFileName, handler->fileFieldSize);
    }
    return maxNode;
  }

  // Helper: Find node with specific key starting from first node
  // Returns node with key=-1 if not found
  IndexFileHandler::IndexNode findKeyNode(IndexFileHandler::IndexNode node, int key) {
    for (int i = 0; i < handler->m; i++) {
      if (node.key == -1)
        break;
      if (node.key == key)
        return node;
      node = node.getNextRecord(handler->indexFileName, handler->fileFieldSize);
    }
    // Return a node with key=-1 to indicate not found
    return node;
  }

  // Helper: Delete at node position and shift remaining keys left
  void deleteAtNode(IndexFileHandler::IndexNode deleteNode) {
    int recordNumber = deleteNode.getRecordNumber(handler->fileFieldSize, handler->m);
    int recordStart = handler->getRecordStart(recordNumber);
    int recordEnd = recordStart + handler->fileFieldSize * (2 * handler->m + 1);
    
    IndexFileHandler::IndexNode currNode = deleteNode;
    IndexFileHandler::IndexNode nextNode = currNode.getNextRecord(handler->indexFileName, handler->fileFieldSize);
    
    // Shift all keys and addresses left (within record bounds)
    while (nextNode.pos < recordEnd && nextNode.key != -1) {
      currNode.key = nextNode.key;
      currNode.address = nextNode.address;
      handler->writeIndexItem(currNode);
      currNode = nextNode;
      nextNode = nextNode.getNextRecord(handler->indexFileName, handler->fileFieldSize);
    }
    // Clear the last slot
    currNode.key = -1;
    currNode.address = -1;
    handler->writeIndexItem(currNode);
  }

  // Helper: Insert key-addr at node position (shift remaining right)
  void insertAtNode(IndexFileHandler::IndexNode insertNode, int key, int addr) {
    // First find the last valid node
    IndexFileHandler::IndexNode node = insertNode;
    IndexFileHandler::IndexNode lastNode = insertNode;
    int count = 0;
    while (node.key != -1) {
      lastNode = node;
      node = node.getNextRecord(handler->indexFileName, handler->fileFieldSize);
      count++;
    }
    
    // Shift from end to insert position
    IndexFileHandler::IndexNode emptySlot = node; // First empty slot
    while (emptySlot.pos > insertNode.pos) {
      IndexFileHandler::IndexNode prevNode = IndexFileHandler::IndexNode(
          emptySlot.pos - 2 * handler->fileFieldSize, 
          handler->indexFileName, handler->fileFieldSize);
      emptySlot.key = prevNode.key;
      emptySlot.address = prevNode.address;
      handler->writeIndexItem(emptySlot);
      emptySlot = prevNode;
    }
    
    // Insert at position
    insertNode.key = key;
    insertNode.address = addr;
    handler->writeIndexItem(insertNode);
  }

  // Helper: Find sibling info for a node using path
  struct SiblingInfo {
    IndexFileHandler::IndexNode parentNode;    // The parent entry pointing to current node
    IndexFileHandler::IndexNode leftSibling;   // Entry for left sibling (key=-1 if none)
    IndexFileHandler::IndexNode rightSibling;  // Entry for right sibling (key=-1 if none)
    bool hasParent;
    
    // Constructor with all nodes initialized
    SiblingInfo(IndexFileHandler::IndexNode parent, IndexFileHandler::IndexNode left, 
                IndexFileHandler::IndexNode right, bool hasP)
        : parentNode(parent), leftSibling(left), rightSibling(right), hasParent(hasP) {}
  };

  SiblingInfo getSiblingInfo(vector<IndexFileHandler::IndexNode> &path, int childNodeIndex) {
    // Create dummy nodes for initialization (will be replaced if found)
    IndexFileHandler::IndexNode dummyNode = handler->getFirstNode(0);
    dummyNode.key = -1;
    
    if (path.empty())
      return SiblingInfo(dummyNode, dummyNode, dummyNode, false);

    // Find parent entry that points to childNodeIndex
    for (int i = path.size() - 1; i >= 0; i--) {
      if (path[i].address == childNodeIndex) {
        IndexFileHandler::IndexNode parentNode = path[i];
        IndexFileHandler::IndexNode leftSibling = dummyNode;
        IndexFileHandler::IndexNode rightSibling = path[i].getNextRecord(handler->indexFileName, handler->fileFieldSize);
        
        // Get left sibling using previous position
        int parentRecord = path[i].getRecordNumber(handler->fileFieldSize, handler->m);
        if (path[i].pos > handler->getRecordStart(parentRecord) + handler->fileFieldSize) {
          leftSibling = IndexFileHandler::IndexNode(
              path[i].pos - 2 * handler->fileFieldSize,
              handler->indexFileName, handler->fileFieldSize);
        }
        
        return SiblingInfo(parentNode, leftSibling, rightSibling, true);
      }
    }
    return SiblingInfo(dummyNode, dummyNode, dummyNode, false);
  }

  // Borrow from right sibling
  void borrowFromRight(int leafNode, SiblingInfo &siblings) {
    IndexFileHandler::IndexNode rightFirstNode = handler->getFirstNode(siblings.rightSibling.address);
    IndexFileHandler::IndexNode borrowNode = rightFirstNode;

    // Delete from right sibling
    deleteAtNode(borrowNode);

    // Insert into current node at end - find last slot
    IndexFileHandler::IndexNode leafFirstNode = handler->getFirstNode(leafNode);
    int currentCount = countKeysFrom(leafFirstNode);
    IndexFileHandler::IndexNode insertNode = leafFirstNode;
    for (int i = 0; i < currentCount; i++) {
      insertNode = insertNode.getNextRecord(handler->indexFileName, handler->fileFieldSize);
    }
    insertNode.key = borrowNode.key;
    insertNode.address = borrowNode.address;
    handler->writeIndexItem(insertNode);

    // Update parent: our max changed
    IndexFileHandler::IndexNode newMaxNode = getMaxKeyNode(handler->getFirstNode(leafNode));
    siblings.parentNode.key = newMaxNode.key;
    handler->writeIndexItem(siblings.parentNode);
  }

  // Borrow from left sibling
  void borrowFromLeft(int leafNode, SiblingInfo &siblings) {
    IndexFileHandler::IndexNode leftFirstNode = handler->getFirstNode(siblings.leftSibling.address);
    IndexFileHandler::IndexNode borrowNode = getMaxKeyNode(leftFirstNode);
    int borrowKey = borrowNode.key;
    int borrowAddr = borrowNode.address;

    // Clear last slot in left sibling
    borrowNode.key = -1;
    borrowNode.address = -1;
    handler->writeIndexItem(borrowNode);

    // Insert into current node at beginning
    IndexFileHandler::IndexNode leafFirstNode = handler->getFirstNode(leafNode);
    insertAtNode(leafFirstNode, borrowKey, borrowAddr);

    // Update parent: left sibling's max changed
    IndexFileHandler::IndexNode newLeftMaxNode = getMaxKeyNode(handler->getFirstNode(siblings.leftSibling.address));
    siblings.leftSibling.key = newLeftMaxNode.key;
    handler->writeIndexItem(siblings.leftSibling);
  }

  // Merge two nodes: copies all keys from srcRecord to dstRecord
  // Removes both parent entries and inserts a single new entry with the merged max
  // Returns the parent record number for recursive underflow handling
  int mergeNodes(int dstRecord, int srcRecord, 
                  IndexFileHandler::IndexNode dstParentEntry, 
                  IndexFileHandler::IndexNode srcParentEntry) {
    IndexFileHandler::IndexNode dstFirstNode = handler->getFirstNode(dstRecord);
    int dstKeyCount = countKeysFrom(dstFirstNode);
    IndexFileHandler::IndexNode srcFirstNode = handler->getFirstNode(srcRecord);
    int srcKeyCount = countKeysFrom(srcFirstNode);

    // Get parent record number before modifying
    int parentRecord = dstParentEntry.getRecordNumber(handler->fileFieldSize, handler->m);

    // Find position to insert in destination node (after existing keys)
    IndexFileHandler::IndexNode dstNode = dstFirstNode;
    for (int i = 0; i < dstKeyCount; i++) {
      dstNode = dstNode.getNextRecord(handler->indexFileName, handler->fileFieldSize);
    }

    // Copy all keys from source to destination
    IndexFileHandler::IndexNode srcNode = srcFirstNode;
    for (int i = 0; i < srcKeyCount; i++) {
      dstNode.key = srcNode.key;
      dstNode.address = srcNode.address;
      handler->writeIndexItem(dstNode);
      
      // Clear source node slot
      srcNode.key = -1;
      srcNode.address = -1;
      handler->writeIndexItem(srcNode);
      
      srcNode = srcNode.getNextRecord(handler->indexFileName, handler->fileFieldSize);
      dstNode = dstNode.getNextRecord(handler->indexFileName, handler->fileFieldSize);
    }

    // Remove both parent entries (delete the one with higher pos first to avoid shifting issues)
    if (srcParentEntry.pos > dstParentEntry.pos) {
      deleteAtNode(srcParentEntry);
      deleteAtNode(dstParentEntry);
    } else {
      deleteAtNode(dstParentEntry);
      deleteAtNode(srcParentEntry);
    }

    // Insert new entry with merged max at the position of the earlier parent entry
    IndexFileHandler::IndexNode insertPos = (dstParentEntry.pos < srcParentEntry.pos) ? dstParentEntry : srcParentEntry;
    IndexFileHandler::IndexNode newMaxNode = getMaxKeyNode(handler->getFirstNode(dstRecord));
    insertAtNode(insertPos, newMaxNode.key, dstRecord);

    return parentRecord;
  }

  // Handle underflow after deletion (recursive)
  void handleUnderflow(int nodeRecord, vector<IndexFileHandler::IndexNode> &path) {
    SiblingInfo siblings = getSiblingInfo(path, nodeRecord);
    if (!siblings.hasParent) {
      return; // No parent means we're root, nothing to do
    }

    int minKeys = getMinKeys();

    // Try to borrow from right sibling first
    if (siblings.rightSibling.key != -1) {
      IndexFileHandler::IndexNode rightFirstNode = handler->getFirstNode(siblings.rightSibling.address);
      int rightKeyCount = countKeysFrom(rightFirstNode);
      if (rightKeyCount > minKeys) {
        borrowFromRight(nodeRecord, siblings);
        return;
      }
    }

    // Try to borrow from left sibling
    if (siblings.leftSibling.key != -1) {
      IndexFileHandler::IndexNode leftFirstNode = handler->getFirstNode(siblings.leftSibling.address);
      int leftKeyCount = countKeysFrom(leftFirstNode);
      if (leftKeyCount > minKeys) {
        borrowFromLeft(nodeRecord, siblings);
        return;
      }
    }

    // Must merge
    int parentRecord;
    if (siblings.leftSibling.key != -1) {
      // Merge current into left sibling
      parentRecord = mergeNodes(siblings.leftSibling.address, nodeRecord, siblings.leftSibling, siblings.parentNode);
    } else if (siblings.rightSibling.key != -1) {
      // Merge right sibling into current
      parentRecord = mergeNodes(nodeRecord, siblings.rightSibling.address, siblings.parentNode, siblings.rightSibling);
    } else {
      return; // No sibling to merge with
    }

    // Check for recursive underflow in parent (skip if parent is root)
    if (parentRecord == 1) {
      return;
    }

    // Remove entries belonging to this parent from path
    while (!path.empty() && path.back().getRecordNumber(handler->fileFieldSize, handler->m) == parentRecord) {
      path.pop_back();
    }

    // Check if parent has underflow
    IndexFileHandler::IndexNode parentFirstNode = handler->getFirstNode(parentRecord);
    int parentKeyCount = countKeysFrom(parentFirstNode);
    if (parentKeyCount < minKeys) {
      handleUnderflow(parentRecord, path);
    }
  }

public:
  Index(IndexFileHandler *handler) { this->handler = handler; }

  // Update parents in path where key equals oldMax with newMax
  void updateParentsMax(vector<IndexFileHandler::IndexNode> &path, int oldMax, int newMax) {
    for (int i = path.size() - 1; i >= 0; i--) {
      if (path[i].key == oldMax) {
        path[i].key = newMax;
        handler->writeIndexItem(path[i]);
      } else {
        break;
      }
    }
  }

  vector<IndexFileHandler::IndexNode> searchARecordInIndex(char *filename,
                                                           int RecordID) {
    // Start from root node (node index 1)
    int currentRecord = 1;

    vector<IndexFileHandler::IndexNode> path; // To store the path taken
    while (currentRecord != -1) {

      // Create first IndexRecord for this node
      IndexFileHandler::IndexNode record = handler->getFirstNode(currentRecord);

      bool isLeaf = handler->isLeafNode(currentRecord);

      if (isLeaf) {
        // Search through keys in leaf node
        for (int i = 0; i < handler->m; i++) {
          if (record.key == -1) {
            return vector<IndexFileHandler::IndexNode>();
          }
          if (record.key == RecordID) {
            path.push_back(record);
            return path;
          }
          record = record.getNextRecord(handler->indexFileName,
                                        handler->fileFieldSize);
        }
        return vector<IndexFileHandler::IndexNode>();
      }

      for (int itemCol = 0; itemCol < handler->m; itemCol++) {
        if (record.key == -1) {
          return vector<IndexFileHandler::IndexNode>();
        }
        if (RecordID <= record.key) {
          path.push_back(record);
          currentRecord = record.address;
          break;
        }
        record = record.getNextRecord(handler->indexFileName,
                                      handler->fileFieldSize);
      }
    }
    return vector<IndexFileHandler::IndexNode>();
  }

  int SearchARecord(char *filename, int RecordID) {
    vector<IndexFileHandler::IndexNode> results =
        searchARecordInIndex(filename, RecordID);
    if (results.empty()) {
      return -1;
    } else {
      return results.back().address;
    }
  }

  // Delete a record from the index
  void DeleteARecord(char *filename, int RecordID) {
    // Step 1: Search for the record
    vector<IndexFileHandler::IndexNode> path =
        searchARecordInIndex(filename, RecordID);
    if (path.empty()) {
      throw runtime_error("Record not found");
    }

    // Get the leaf node where the record is
    IndexFileHandler::IndexNode foundRecord = path.back();
    int recordNumber = foundRecord.getRecordNumber(handler->fileFieldSize, handler->m);
    path.pop_back(); // Remove the found record from path (keep only parents)

    // Get current state before deletion
    IndexFileHandler::IndexNode firstNode = handler->getFirstNode(recordNumber);
    IndexFileHandler::IndexNode maxNode = getMaxKeyNode(firstNode);
    int oldMax = maxNode.key;
    int keyCount = countKeysFrom(firstNode);

    // Step 2: Delete the key from leaf
    deleteAtNode(foundRecord);
    keyCount--;

    // Step 3: Check if deleted key was the max and update parents
    if (RecordID == oldMax && keyCount > 0) {
      IndexFileHandler::IndexNode newMaxNode = getMaxKeyNode(handler->getFirstNode(recordNumber));
      updateParentsMax(path, oldMax, newMaxNode.key);
    }

    // Step 4: Check for underflow
    int minKeys = getMinKeys();
    if (keyCount >= minKeys || recordNumber == 1) {
      return; // No underflow, or we're the root
    }

    // Step 5: Handle underflow
    handleUnderflow(recordNumber, path);
  }
};
