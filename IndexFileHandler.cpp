#include <fstream>
#include <iostream>
using namespace std;
  struct IndexNode {
    // make sure to check for end of record when using nextRecordPos can access
    // next record
    int key;
    int address;
    int pos;

    IndexNode(int pos, char *indexFileName,
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

    IndexNode getNextRecord(char *indexFileName, int fileFieldSize) {
      return IndexNode(this->pos + 2 * fileFieldSize, indexFileName,
                       fileFieldSize);
    }

    int getRecordNumber(int fileFieldSize, int m) {
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


  int getRecordStart(int recordNumber) {
    return fileFieldSize * (2 * this->m + 1) * recordNumber;
  }

  IndexNode getNodeByRecordAndIndex(int recordNumber, int keyIndex) {
    int pos = getRecordStart(recordNumber) + fileFieldSize * (1 + 2 * keyIndex);
    return IndexNode(pos, this->indexFileName, this->fileFieldSize);
  }

  IndexNode getFirstNode(int recordNumber) {
    return getNodeByRecordAndIndex(recordNumber, 0);
  }

  bool isLeafNode(int recordNumber) {
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
