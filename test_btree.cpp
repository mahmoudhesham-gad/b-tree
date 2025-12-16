// Interactive B-tree menu system
#include "addition.cpp"
#include "Index.cpp"
#include <limits>

void clearInputBuffer() {
    cin.clear();
    cin.ignore(numeric_limits<streamsize>::max(), '\n');
}

void displayMenu() {
    cout << "\n========== B-TREE OPERATIONS MENU ==========" << endl;
    cout << "1. Insert a record" << endl;
    cout << "2. Search for a record" << endl;
    cout << "3. Delete a record" << endl;
    cout << "4. Display entire B-tree" << endl;
    cout << "5. Exit" << endl;
    cout << "============================================" << endl;
    cout << "Enter your choice: ";
}

int main() {
    IndexFileHandler handler;
    const char* filename = "indexfile.bin";
    int numberOfRecords, m;

    // Initial setup
    cout << "========== B-TREE INITIALIZATION ==========" << endl;
    cout << "Enter the maximum number of records: ";
    while (!(cin >> numberOfRecords) || numberOfRecords <= 0) {
        cout << "Invalid input. Please enter a positive integer: ";
        clearInputBuffer();
    }

    cout << "Enter the order of B-tree (m): ";
    while (!(cin >> m) || m < 3) {
        cout << "Invalid input. Order must be at least 3: ";
        clearInputBuffer();
    }

    try {
        // Create initial index file
        handler.createIndexFile(const_cast<char*>(filename), numberOfRecords, m);
        cout << "\nIndex file created successfully!" << endl;
        cout << "Initial state (empty B-tree):" << endl;
        handler.DisplayIndexFileContent(const_cast<char*>(filename));

        // Initialize B-tree addition handler and Index
        BTreeAddition btree(m, numberOfRecords, const_cast<char*>(filename));
        Index indexSearch(&handler);  // Create Index object for searching

        int choice;
        bool running = true;

        while (running) {
            displayMenu();

            if (!(cin >> choice)) {
                clearInputBuffer();
                cout << "Invalid input. Please enter a number." << endl;
                continue;
            }

            switch (choice) {
                case 1: { // Insert
                    int key, address;
                    cout << "\n--- INSERT OPERATION ---" << endl;
                    cout << "Enter key to insert: ";
                    while (!(cin >> key)) {
                        cout << "Invalid input. Enter an integer key: ";
                        clearInputBuffer();
                    }

                    cout << "Enter address (external data address): ";
                    while (!(cin >> address) || address < 0) {
                        cout << "Invalid input. Enter a non-negative integer: ";
                        clearInputBuffer();
                    }

                    try {
                        btree.addRecord(key, address);
                        cout << "\nRecord inserted successfully!" << endl;
                        cout << "Key: " << key << ", Address: " << address << endl;
                        cout << "\nUpdated B-tree structure:" << endl;
                        handler.DisplayIndexFileContent(const_cast<char*>(filename));
                    } catch (const exception& e) {
                        cerr << "Error during insertion: " << e.what() << endl;
                    }
                    break;
                }

                case 2: { // Search
                    int key;
                    cout << "\n--- SEARCH OPERATION ---" << endl;
                    cout << "Enter key to search: ";
                    while (!(cin >> key)) {
                        cout << "Invalid input. Enter an integer key: ";
                        clearInputBuffer();
                    }

                    try {
                        int address = indexSearch.SearchARecord(const_cast<char*>(filename), key);
                        if (address != -1) {
                            cout << "Record found!" << endl;
                            cout << "Key: " << key << ", Address: " << address << endl;
                        } else {
                            cout << "Record with key " << key << " not found." << endl;
                        }
                    } catch (const exception& e) {
                        cerr << "Error during search: " << e.what() << endl;
                    }
                    break;
                }

                case 3: { // Delete
                    int key;
                    cout << "\n--- DELETE OPERATION ---" << endl;
                    cout << "Enter key to delete: ";
                    while (!(cin >> key)) {
                        cout << "Invalid input. Enter an integer key: ";
                        clearInputBuffer();
                    }

                    try {
                        indexSearch.DeleteARecord(const_cast<char*>(filename), key);
                        cout << "\nRecord deleted successfully!" << endl;
                        cout << "Key: " << key << endl;
                        cout << "\nUpdated B-tree structure:" << endl;
                    } catch (const exception& e) {
                        cerr << "Error during deletion: " << e.what() << endl;
                        //handler.DisplayIndexFileContent(const_cast<char*>(filename));
                    }
                    break;
                }

                case 4: { // Display
                    cout << "\n--- CURRENT B-TREE STRUCTURE ---" << endl;
                    handler.DisplayIndexFileContent(const_cast<char*>(filename));
                    break;
                }

                case 5: { // Exit
                    cout << "\nExiting B-tree operations. Goodbye!" << endl;
                    running = false;
                    break;
                }

                default:
                    cout << "Invalid choice. Please select 1-5." << endl;
            }
        }

    } catch (const exception& e) {
        cerr << "Fatal error: " << e.what() << endl;
        return 1;
    }

    return 0;
}