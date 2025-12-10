// Main test file for B-tree insertion
#include "addition.cpp"

int main() {
    IndexFileHandler handler;
    const char* filename = "indexfile.bin";
    int numberOfRecords = 10;
    int m = 5;

    try {
        // Create initial index file
        handler.createIndexFile(const_cast<char*>(filename), numberOfRecords, m);
        cout << "Initial index file (all empty rows linked):" << endl;
        handler.DisplayIndexFileContent(const_cast<char*>(filename));
        cout << endl;

        // Initialize B-tree addition handler
        BTreeAddition btree(m,numberOfRecords,const_cast<char*>(filename));
        //btree.initialize(const_cast<char*>(filename), numberOfRecords, m);
/*
        // Add first record
        cout << "Adding record with key=50, address=100 (external data address)" << endl;
        cout << "This should create a LEAF node (type=0) at row 1" << endl;
        cout << "Row 0 should remain as free list head (type=-1)" << endl;*/
        btree.addRecord(50, 100);
/*
        cout << "\nIndex file after first addition:" << endl;
        cout << "Row 0: Free list head (should be: -1 2 -1 -1 ...)" << endl;
        cout << "Row 1: First leaf node (should be: 0 20 400 30 200 50 100 -1 -1 -1)" << endl;
        cout << "       (Note: records start from column 1, no nextEmpty column)" << endl;*/
        handler.DisplayIndexFileContent(const_cast<char*>(filename));
        cout << endl;

        // Add more records to test splitting
        cout << "Adding key=30, address=200 (external)" << endl;
        btree.addRecord(30, 200);

        cout << "\nAfter adding 30:" << endl;
        handler.DisplayIndexFileContent(const_cast<char*>(filename));
        cout << endl;

        cout << "Adding key=70, address=300 (external)" << endl;
        btree.addRecord(70, 300);

        cout << "\nAfter adding 70:" << endl;
        handler.DisplayIndexFileContent(const_cast<char*>(filename));
        cout << endl;

        cout << "Adding key=20, address=400 (external)" << endl;
        btree.addRecord(20, 400);

        cout << "\nAfter adding 20:" << endl;
        handler.DisplayIndexFileContent(const_cast<char*>(filename));
        cout << endl;
/*
        cout << "Adding key=60, address=500 (external) - Node is FULL (5 records), this triggers split!" << endl;
        cout << "Before split: [20, 30, 50, 70] (4 records)" << endl;
        cout << "After adding 60: [20, 30, 50, 60, 70] (5 records = m)" << endl;
        cout << "Node is now at capacity, adding 6th record would trigger split" << endl;*/
        btree.addRecord(60, 500);

        cout << "\nAfter adding 60 (should NOT split yet - only 5 records):" << endl;
        handler.DisplayIndexFileContent(const_cast<char*>(filename));
        cout << endl;
/*
        cout << "Adding key=10, address=600 - NOW split happens (6th record)!" << endl;
        cout << "After adding: [10, 20, 30, 50, 60, 70] (6 records)" << endl;
        cout << "Median index = 3, split: Left=[10,20,30], Right=[50,60,70]" << endl;
        cout << "Promoted key = 30 (largest in left)" << endl;*/
        btree.addRecord(10, 600);

        /*cout << "\nIndex file after split:" << endl;
        cout << "Expected structure (internal nodes at lower rows, leaves at end):" << endl;
        cout << "Row 0: Free list head" << endl;
        cout << "Row 1: Internal node (root) [1 30 2 70 3]" << endl;
        cout << "       Entry: (30→row2), (70→row3)" << endl;
        cout << "Row 2: Left leaf [0 10 600 20 400 30 200]" << endl;
        cout << "Row 3: Right leaf [0 50 100 60 500 70 300]" << endl;
        cout << "Note: Internal nodes first, leaf nodes at the end!" << endl;*/
        handler.DisplayIndexFileContent(const_cast<char*>(filename));

    } catch (const exception& e) {
        cerr << "Error: " << e.what() << endl;
    }

    return 0;
}

