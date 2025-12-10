#ifndef INDEX_FILE_HANDLER_CPP
#define INDEX_FILE_HANDLER_CPP

#include <fstream>
#include <iostream>
using namespace std;

class IndexFileHandler; // Forward declaration

struct IndexNode {
    // make sure to check for end of record when using nextRecordPos can access
    // next record
    int key;
    int address;
    int pos;

    IndexNode(int pos, const char *indexFileName,
              int fileFieldSize) {
      ifstream indexFile = ifstream(indexFileName, ios::binary);
      if (!indexFile) {
        throw runtime_error("Could not open index file");
      }
      indexFile.seekg(pos);
      indexFile.read(reinterpret_cast<char *>(&key), fileFieldSize);
      indexFile.seekg(pos + fileFieldSize);
      indexFile.read(reinterpret_cast<char *>(&address), fileFieldSize);
      indexFile.close();
      this->pos = pos;
    }

    IndexNode getNextRecord(const char *indexFileName, int fileFieldSize) const {
      return IndexNode(this->pos + 2 * fileFieldSize, indexFileName,
                       fileFieldSize);
    }

    int getRecordNumber(int fileFieldSize, int m) const {
      return this->pos / (fileFieldSize * (2 * m + 1));
    }

  };

class IndexFileHandler {
public:
  int fileFieldSize = sizeof(int);
  char *indexFileName;
  int numberOfRecords;
  int m;

  void writeIndexItem(IndexNode node) {
    fstream indexFile =
        fstream(indexFileName, ios::binary | ios::in | ios::out);
    indexFile.seekp(node.pos);
    indexFile.write(reinterpret_cast<char *>(&node.key), fileFieldSize);
    indexFile.write(reinterpret_cast<char *>(&node.address), fileFieldSize);
    indexFile.close();
  }

  int getRecordStart(int recordNumber) const {
    return fileFieldSize * (2 * this->m + 1) * recordNumber;
  }

  IndexNode getNodeByRecordAndIndex(int recordNumber, int keyIndex) const {
    int pos = getRecordStart(recordNumber) + fileFieldSize * (1 + 2 * keyIndex);
    return IndexNode(pos, this->indexFileName, this->fileFieldSize);
  }

  IndexNode getFirstNode(int recordNumber) const {
    return getNodeByRecordAndIndex(recordNumber, 0);
  }

  // Count keys in a record
  int countKeys(int recordNumber) const {
    int count = 0;
    IndexNode node = getFirstNode(recordNumber);
    for (int i = 0; i < m; i++) {
      if (node.key == -1)
        break;
      count++;
      node = node.getNextRecord(indexFileName, fileFieldSize);
    }
    return count;
  }

  // Get max key node in a record (last valid key)
  IndexNode getMaxKeyNode(int recordNumber) const {
    IndexNode node = getFirstNode(recordNumber);
    IndexNode maxNode = node;
    for (int i = 0; i < m; i++) {
      if (node.key == -1)
        break;
      maxNode = node;
      node = node.getNextRecord(indexFileName, fileFieldSize);
    }
    return maxNode;
  }

  bool isLeafNode(int recordNumber) const {
    int pos = getRecordStart(recordNumber);
    ifstream indexFile = ifstream(this->indexFileName, ios::binary);
    if (!indexFile) {
      throw runtime_error("Could not open index file");
    }
    indexFile.seekg(pos);
    int nodeType;
    indexFile.read(reinterpret_cast<char *>(&nodeType), sizeof(int));
    return nodeType == 0;
  }

  // Set node type for a record (0=leaf, 1=internal, -1=free)
  void setNodeType(int recordNumber, int nodeType) {
    int pos = getRecordStart(recordNumber);
    fstream file(indexFileName, ios::binary | ios::in | ios::out);
    file.seekp(pos);
    file.write(reinterpret_cast<char*>(&nodeType), fileFieldSize);
    file.close();
  }

  // Add a record to the free list
  void addToFreeList(int recordNumber) {
    // Read current free list head from record 0
    IndexNode freeListHead = getFirstNode(0);
    int currentFreeHead = freeListHead.key;
    
    // Mark record as free: nodeType=-1, first slot points to old free head
    int recordStart = getRecordStart(recordNumber);
    fstream file(indexFileName, ios::binary | ios::in | ios::out);
    int freeMarker = -1;
    file.seekp(recordStart);
    file.write(reinterpret_cast<char*>(&freeMarker), fileFieldSize); // nodeType = -1
    file.write(reinterpret_cast<char*>(&currentFreeHead), fileFieldSize); // points to old head
    file.close();
    
    // Update free list head in record 0 to point to this record
    freeListHead.key = recordNumber;
    writeIndexItem(freeListHead);
  }

  // Delete at node position and shift remaining keys left
  void deleteAtNode(IndexNode deleteNode) {
    int recordNumber = deleteNode.getRecordNumber(fileFieldSize, m);
    int recordEnd = getRecordStart(recordNumber) + fileFieldSize * (2 * m + 1);
    
    IndexNode currNode = deleteNode;
    IndexNode nextNode = currNode.getNextRecord(indexFileName, fileFieldSize);
    
    // Shift all keys and addresses left (within record bounds)
    while (nextNode.pos < recordEnd && nextNode.key != -1) {
      currNode.key = nextNode.key;
      currNode.address = nextNode.address;
      writeIndexItem(currNode);
      currNode = nextNode;
      nextNode = nextNode.getNextRecord(indexFileName, fileFieldSize);
    }
    // Clear the last slot
    currNode.key = -1;
    currNode.address = -1;
    writeIndexItem(currNode);
  }

  // Insert key-addr at node position (shift remaining right)
  void insertAtNode(IndexNode insertNode, int key, int addr) {
    // First find the last valid node (first empty slot)
    IndexNode node = insertNode;
    while (node.key != -1) {
      node = node.getNextRecord(indexFileName, fileFieldSize);
    }
    
    // Shift from end to insert position
    IndexNode emptySlot = node;
    while (emptySlot.pos > insertNode.pos) {
      IndexNode prevNode = IndexNode(
          emptySlot.pos - 2 * fileFieldSize, 
          indexFileName, fileFieldSize);
      emptySlot.key = prevNode.key;
      emptySlot.address = prevNode.address;
      writeIndexItem(emptySlot);
      emptySlot = prevNode;
    }
    
    // Insert at position
    insertNode.key = key;
    insertNode.address = addr;
    writeIndexItem(insertNode);
  }

  void createIndexFile(char *filename, int numberOfRecords, int m) {
    ofstream indexFile;
    indexFile.open(filename, ios::binary);
    if (!indexFile) {
      throw runtime_error("Could not create index file");
    }
    this->numberOfRecords = numberOfRecords;
    this->m = m;

    int cols = 2 * this->m + 1;

    // Initialize all records
    for (int record = 0; record < this->numberOfRecords; record++) {
      for (int col = 0; col < cols; col++) {
        int value;
        if (col == 1) {
          // Second column: linked list of empty nodes
          // Points to next empty node (record+1), or -1 if last
          if (record < this->numberOfRecords - 1) {
            value = record + 1;
          } else {
            value = -1;
          }
        } else {
          value = -1;
        }
        indexFile.write(reinterpret_cast<char *>(&value), sizeof(int));
      }
    }

    indexFile.close();
    this->indexFileName = filename;
  }

  void DisplayIndexFileContent(char *filename) {
    ifstream indexFile;
    indexFile.open(filename, ios::binary);
    if (!indexFile) {
      throw runtime_error("Could not open index file");
    }

    int cols = 2 * this->m + 1;

    for (int record = 0; record < this->numberOfRecords; record++) {
      for (int col = 0; col < cols; col++) {
        int value;
        indexFile.read(reinterpret_cast<char *>(&value), sizeof(int));
        cout << value;
        if (col < cols - 1) {
          cout << " ";
        }
      }
      cout << endl;
    }

    indexFile.close();
  }
};

/*int main() {
    IndexFileHandler handler;
    const char* filename = "indexfile.bin";
    int numberOfRecords = 10; // Example number of records
    int m = 5; // Example m value

    try {
        handler.createIndexFile(const_cast<char*>(filename), numberOfRecords,
m); handler.DisplayIndexFileContent(const_cast<char*>(filename)); } catch (const
runtime_error& e) { cerr << e.what() << endl;
    }

    return 0;
}*/

#endif // INDEX_FILE_HANDLER_CPP
