#include "IndexFileHandler.cpp"
#include <cmath>
#include <stdexcept>
#include <vector>
using namespace std;

class Index {
private:
  IndexFileHandler *handler;

  // Helper: Get minimum keys required (floor(m/2))
  int getMinKeys() const { return handler->m / 2; }

  // Helper: Find sibling info for a node using path
  struct SiblingInfo {
    IndexNode parentNode;    // The parent entry pointing to current node
    IndexNode leftSibling;   // Entry for left sibling (key=-1 if none)
    IndexNode rightSibling;  // Entry for right sibling (key=-1 if none)
    bool hasParent;
    
    // Constructor with all nodes initialized
    SiblingInfo(IndexNode parent, IndexNode left, 
                IndexNode right, bool hasP)
        : parentNode(parent), leftSibling(left), rightSibling(right), hasParent(hasP) {}
  };

  SiblingInfo getSiblingInfo(vector<IndexNode> &path, int childNodeIndex) {
    // Create dummy nodes for initialization (will be replaced if found)
    IndexNode dummyNode = handler->getFirstNode(0);
    dummyNode.key = -1;
    
    if (path.empty())
      return SiblingInfo(dummyNode, dummyNode, dummyNode, false);

    // Find parent entry that points to childNodeIndex
    for (int i = path.size() - 1; i >= 0; i--) {
      if (path[i].address == childNodeIndex) {
        IndexNode parentNode = path[i];
        IndexNode leftSibling = dummyNode;
        IndexNode rightSibling = path[i].getNextRecord(handler->indexFileName, handler->fileFieldSize);
        
        // Get left sibling using previous position
        int parentRecord = path[i].getRecordNumber(handler->fileFieldSize, handler->m);
        if (path[i].pos > handler->getFirstNode(parentRecord).pos) {
          leftSibling = IndexNode(
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
    IndexNode borrowNode = handler->getFirstNode(siblings.rightSibling.address);

    // Delete from right sibling
    handler->deleteAtNode(borrowNode);

    // Insert into current node at end
    int currentCount = handler->countKeys(leafNode);
    IndexNode insertNode = handler->getNodeByRecordAndIndex(leafNode, currentCount);
    insertNode.key = borrowNode.key;
    insertNode.address = borrowNode.address;
    handler->writeIndexItem(insertNode);

    // Update parent: our max changed
    IndexNode newMaxNode = handler->getMaxKeyNode(leafNode);
    siblings.parentNode.key = newMaxNode.key;
    handler->writeIndexItem(siblings.parentNode);
  }

  // Borrow from left sibling
  void borrowFromLeft(int leafNode, SiblingInfo &siblings) {
    IndexNode borrowNode = handler->getMaxKeyNode(siblings.leftSibling.address);
    int borrowKey = borrowNode.key;
    int borrowAddr = borrowNode.address;

    // Clear last slot in left sibling
    borrowNode.key = -1;
    borrowNode.address = -1;
    handler->writeIndexItem(borrowNode);

    // Insert into current node at beginning
    IndexNode leafFirstNode = handler->getFirstNode(leafNode);
    handler->insertAtNode(leafFirstNode, borrowKey, borrowAddr);

    // Update parent: left sibling's max changed
    IndexNode newLeftMaxNode = handler->getMaxKeyNode(siblings.leftSibling.address);
    siblings.leftSibling.key = newLeftMaxNode.key;
    handler->writeIndexItem(siblings.leftSibling);
  }

  // Merge two nodes: copies all keys from srcRecord to dstRecord
  // Removes both parent entries and inserts a single new entry with the merged max
  // Returns the parent record number for recursive underflow handling
  int mergeNodes(int dstRecord, int srcRecord, 
                  IndexNode dstParentEntry, 
                  IndexNode srcParentEntry) {
    int dstKeyCount = handler->countKeys(dstRecord);
    int srcKeyCount = handler->countKeys(srcRecord);

    // Get parent record number before modifying
    int parentRecord = dstParentEntry.getRecordNumber(handler->fileFieldSize, handler->m);

    // Copy all keys from source to destination (starting after existing keys)
    for (int i = 0; i < srcKeyCount; i++) {
      IndexNode srcNode = handler->getNodeByRecordAndIndex(srcRecord, i);
      IndexNode dstNode = handler->getNodeByRecordAndIndex(dstRecord, dstKeyCount + i);
      dstNode.key = srcNode.key;
      dstNode.address = srcNode.address;
      handler->writeIndexItem(dstNode);
      
      // Clear source node slot
      srcNode.key = -1;
      srcNode.address = -1;
      handler->writeIndexItem(srcNode);
    }

    // Mark source record as free and add to free list
    handler->addToFreeList(srcRecord);

    // Remove both parent entries (delete the one with higher pos first to avoid shifting issues)
    if (srcParentEntry.pos > dstParentEntry.pos) {
      handler->deleteAtNode(srcParentEntry);
      handler->deleteAtNode(dstParentEntry);
    } else {
      handler->deleteAtNode(dstParentEntry);
      handler->deleteAtNode(srcParentEntry);
    }

    // Insert new entry with merged max at the position of the earlier parent entry
    IndexNode insertPos = (dstParentEntry.pos < srcParentEntry.pos) ? dstParentEntry : srcParentEntry;
    IndexNode newMaxNode = handler->getMaxKeyNode(dstRecord);
    handler->insertAtNode(insertPos, newMaxNode.key, dstRecord);

    return parentRecord;
  }

  // Handle underflow after deletion (recursive)
  void handleUnderflow(int nodeRecord, vector<IndexNode> &path) {
    SiblingInfo siblings = getSiblingInfo(path, nodeRecord);
    if (!siblings.hasParent) {
      return; // No parent means we're root, nothing to do
    }

    int minKeys = getMinKeys();

    // Try to borrow from right sibling first
    if (siblings.rightSibling.key != -1) {
      int rightKeyCount = handler->countKeys(siblings.rightSibling.address);
      if (rightKeyCount > minKeys) {
        borrowFromRight(nodeRecord, siblings);
        return;
      }
    }

    // Try to borrow from left sibling
    if (siblings.leftSibling.key != -1) {
      int leftKeyCount = handler->countKeys(siblings.leftSibling.address);
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

    // Check if parent has only 1 child - need to collapse
    IndexNode parentFirstNode = handler->getFirstNode(parentRecord);
    int parentKeyCount = handler->countKeys(parentRecord);
    
    if (parentKeyCount == 1) {
      // Parent has only 1 child - collapse: pull child's content up to parent
      int onlyChildRecord = parentFirstNode.address;
      
      // Copy child's node type to parent
      int childNodeType = handler->isLeafNode(onlyChildRecord) ? 0 : 1;
      handler->setNodeType(parentRecord, childNodeType);
      
      // Copy all keys from child to parent
      for (int i = 0; i < handler->m; i++) {
        IndexNode childNode = handler->getNodeByRecordAndIndex(onlyChildRecord, i);
        IndexNode parentNode = handler->getNodeByRecordAndIndex(parentRecord, i);
        
        parentNode.key = childNode.key;
        parentNode.address = childNode.address;
        handler->writeIndexItem(parentNode);
        
        // Clear child node slot
        childNode.key = -1;
        childNode.address = -1;
        handler->writeIndexItem(childNode);
        
        if (parentNode.key == -1) break;
      }
      
      // If parent is not root, we need to update grandparent's pointer
      // The grandparent already points to parentRecord, so no change needed
      // But we may need to handle underflow in grandparent if parent was merged
      return;
    }

    // If parent is root, no underflow check needed
    if (parentRecord == 1) {
      return;
    }

    // Remove entries belonging to this parent from path
    while (!path.empty() && path.back().getRecordNumber(handler->fileFieldSize, handler->m) == parentRecord) {
      path.pop_back();
    }

    // Check if parent has underflow (less than minKeys)
    if (parentKeyCount < minKeys) {
      handleUnderflow(parentRecord, path);
    }
  }

public:
  Index(IndexFileHandler *handler) { this->handler = handler; }

  // Update parents in path where key equals oldMax with newMax
  void updateParentsMax(vector<IndexNode> &path, int oldMax, int newMax) {
    for (int i = path.size() - 1; i >= 0; i--) {
      if (path[i].key == oldMax) {
        path[i].key = newMax;
        handler->writeIndexItem(path[i]);
      } else {
        break;
      }
    }
  }

  vector<IndexNode> searchARecordInIndex(char *filename,
                                                           int RecordID) {
    // Start from root node (node index 1)
    int currentRecord = 1;

    vector<IndexNode> path; // To store the path taken
    while (currentRecord != -1) {

      // Create first IndexRecord for this node
      IndexNode record = handler->getFirstNode(currentRecord);

      bool isLeaf = handler->isLeafNode(currentRecord);

      if (isLeaf) {
        // Search through keys in leaf node
        for (int i = 0; i < handler->m; i++) {
          if (record.key == -1) {
            return vector<IndexNode>();
          }
          if (record.key == RecordID) {
            path.push_back(record);
            return path;
          }
          record = record.getNextRecord(handler->indexFileName,
                                        handler->fileFieldSize);
        }
        return vector<IndexNode>();
      }

      for (int itemCol = 0; itemCol < handler->m; itemCol++) {
        if (record.key == -1) {
          return vector<IndexNode>();
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
    return vector<IndexNode>();
  }

  int SearchARecord(char *filename, int RecordID) {
    vector<IndexNode> results =
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
    vector<IndexNode> path =
        searchARecordInIndex(filename, RecordID);
    if (path.empty()) {
      throw runtime_error("Record not found");
    }

    // Get the leaf node where the record is
    IndexNode foundRecord = path.back();
    int recordNumber = foundRecord.getRecordNumber(handler->fileFieldSize, handler->m);
    path.pop_back(); // Remove the found record from path (keep only parents)

    // Get current state before deletion
    IndexNode maxNode = handler->getMaxKeyNode(recordNumber);
    int oldMax = maxNode.key;
    int keyCount = handler->countKeys(recordNumber);

    // Step 2: Delete the key from leaf
    handler->deleteAtNode(foundRecord);
    keyCount--;

    // Step 3: Check if deleted key was the max and update parents
    if (RecordID == oldMax && keyCount > 0) {
      IndexNode newMaxNode = handler->getMaxKeyNode(recordNumber);
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
