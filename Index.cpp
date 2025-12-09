#include "IndexFileHandler.cpp"
using namespace std;

class Index {
private:
    IndexFileHandler* handler;

public:
    Index(IndexFileHandler* handler) {
        this->handler = handler;
    }

    int SearchARecord(char* filename, int RecordID) {
        // Start from root node (node index 1)
        int currentRecord = 1;
        
        while (currentRecord != -1) {
            // Get the start position of this record/node
            unsigned char recordStart = handler->getRecordStart(currentRecord);
            
            // Create first IndexRecord for this node
            // First position is at recordStart + fileFieldSize (skip node type)
            IndexFileHandler::IndexRecord record(recordStart + handler->fileFieldSize, 
                                                  handler->indexFileName, 
                                                  handler->fileFieldSize);
            
            bool isLeaf = handler->isLeafNode(currentRecord);
         
            if (isLeaf) {
                // Search through keys in leaf node
                for (int i = 0; i < handler->m; i++) {
                    if(record.key == -1) {
                        return -1; // No more keys in this leaf node
                    }
                    if (record.key == RecordID) {
                        return record.address; // Found the record
                    }
                    // Move to next key-address pair
                    record = record.getNextRecord(handler->indexFileName, handler->fileFieldSize);

                }
                return -1; // Record not found in leaf node
            }

            for(int itemCol = 0; itemCol < handler->m; itemCol++) {
                if(record.key == -1) {
                    return -1; // No more keys in this internal node and RecordID is larger than all keys 
                }
                if (RecordID <= record.key) {
                    // Move to the child pointer before this key
                    currentRecord = record.address;
                    break;
                }
                // Move to next key-address pair
                record = record.getNextRecord(handler->indexFileName, handler->fileFieldSize);
            }
        }
    }
};
