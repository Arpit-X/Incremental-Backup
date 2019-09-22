#include <iostream>
#include <vector>
#include <unordered_map>
#include <set>
#include <list>

#define CHUNK_SIZE 8
#define MEMORY_SIZE 1024

using namespace std;

char *DISK = new char[MEMORY_SIZE];

typedef enum Instruction_Type {DELETE=1, ALTER, APPEND} Instruction_Type;

typedef struct Instruction{
    Instruction_Type type;
    int block_number;
    char* data;
} Instruction;

int DISK_INDEX = 0;

typedef struct Update{
    int timestamp;
    int start_index;

    bool operator <(const Update& node) const {
        return this->timestamp < node.timestamp;
    }
} Update;

typedef struct Block{
    set<Update> data;
} Block;

typedef struct FullBackup{
    int timestamp;
    mutable list<Block> blocks;

    bool operator <(const FullBackup& node) const {
        return this->timestamp < node.timestamp;
    }
} FullBackup;

unordered_map< string, set<FullBackup> > Meta;

bool canWriteToDisk(string chunk){
    return DISK_INDEX + chunk.size() < MEMORY_SIZE;
}

bool fileExists(string filename, bool show_message= false){
    if(Meta.find(filename) != Meta.end()){
        return true;
    }
    if(show_message)
        cout << filename << " not found"<< endl;
    return false;
}

Update getClosestUpdate(set<Update> &updates, int timestamp){
    auto it = updates.begin();
    if(it->timestamp > timestamp){
        return {-1};
    }
    Update update = *--updates.upper_bound({timestamp});
    return update;
}

FullBackup getClosestFullBackup(set<FullBackup> &backups, int timestamp){
    auto it = backups.begin();
    if(it->timestamp > timestamp){
        return {-1};
    }
    FullBackup fb =  *--backups.upper_bound({timestamp});
    return fb;
}

void printAllUpdates(set<Update> updates){
    for(auto it = updates.begin(); it != updates.end(); it++ ){
        cout << it->timestamp << "," << it->start_index << " | ";
    }
    cout << endl;
}

string readFromDisk(/* You decide */int index){
    string chunk;
    for(int i=0; i < CHUNK_SIZE;i++){
        if(DISK[index+i] == '\0') break;
        chunk.push_back(DISK[index+i]);
    }
    return chunk;
}

void writeToDisk(/* You decide */ string block){
    if( canWriteToDisk(block) && block.size() <= CHUNK_SIZE){
        for(int i=0; i < CHUNK_SIZE; i++){
            DISK[DISK_INDEX++] = block[i];
        }
    }else if(!canWriteToDisk(block)){
        cout << "Insufficient Memory" << endl;
    }else{
        cout << "Chunk it too big!"<< endl;
    }
}

Update initUpdate(int timestamp, string data){
    if (canWriteToDisk(data)) {
        Update update;
        update.timestamp = timestamp;
        update.start_index = DISK_INDEX;
        writeToDisk(data);
        return update;
    }
    else{
        throw "Insufficient Memory";
    }
}

Block initBlock(int timestamp, string data){
    Block block;
    block.data.insert(initUpdate(timestamp, data));
    return block;
}

FullBackup initFullBackup(int timestamp){
    FullBackup backup;
    backup.timestamp = timestamp;
    return backup;
}

// Full backup
void fullBackup(string filename, int timestamp, string data ){
    if(canWriteToDisk(data)){
        int index = 0;
        FullBackup backup = initFullBackup(timestamp);
        while(data[index] != '\0'){
            string chunk;
            for(int i=0;i < CHUNK_SIZE; i++){
                if(data[index] == '\0') break;
                chunk.push_back(data[index++]);
            }
            backup.blocks.push_back(initBlock(timestamp,chunk));
        }
        if(!fileExists(filename)){
            set<FullBackup> fb;
            Meta[filename] = fb;
        }
        Meta[filename].insert(backup);
    }else{
        cout << "Insufficient memory";
    }
}

// Print the file
void printFile(string filename, int timestamp) {
    if(fileExists(filename, true)){
        FullBackup backup = getClosestFullBackup(Meta[filename], timestamp);
        if(backup.timestamp != -1){
            for(auto it = backup.blocks.begin(); it != backup.blocks.end(); it++){
                Block block = *it;
                Update update = getClosestUpdate(block.data, timestamp);
                if(update.timestamp && update.start_index != -1){
                    cout << readFromDisk(update.start_index);
                }
            }
            cout << endl;
        }
        else{
            cout << "No data found at"<< timestamp << endl;
        }
    }

}

Update initDeleteBlock(int timestamp){
    return {timestamp, -1};
}


Block* getNthBlock(list<Block> &blocks, int number, int timestamp){
    int block_number = 0;
    auto it = blocks.begin();
    while(it != blocks.end()){
        Update up = getClosestUpdate(it->data, timestamp);
        if(up.timestamp != -1 && up.start_index != -1){
            block_number++;
        }
        if(block_number == number){
            return &(*it);
        }
        it++;
    }
    return NULL;
}

void updateBlock(string filename, int timestamp, int block_number, Update update) {
    FullBackup fb = getClosestFullBackup(Meta[filename], timestamp);
    if (fb.timestamp != -1) {
        Block *block = getNthBlock(fb.blocks, block_number, timestamp);
        if (block) {
            Meta[filename].erase(fb);
            block->data.insert(update);
            Meta[filename].insert(fb);
        }
    } else {
        cout << "Block not found" << endl;
    }
}

void deleteBlock(string filename, int timestamp, int block_number){
    updateBlock(filename, timestamp, block_number, initDeleteBlock(timestamp));
}

void alterBlock(string filename, int timestamp, int block_number, string data ){
    if(data.size() <= CHUNK_SIZE){
        updateBlock(filename, timestamp, block_number, initUpdate(timestamp, data));
    }else{
        cout << "Chunk is too big cannot update!"<< endl;
    }

}

void appendBlock(string filename, int timestamp, string data) {
    FullBackup fb = getClosestFullBackup(Meta[filename], timestamp);
    if (fb.timestamp != -1) {
        Meta[filename].erase(fb);
        fb.blocks.push_back(initBlock(timestamp, data));
        Meta[filename].insert(fb);
    }
}


// Incremental backup
void incrementalBackup(string filename, int timestamp, vector<Instruction> instructions){
    for (auto it = instructions.begin(); it != instructions.end(); it++) {
        if (fileExists(filename, true)) {
            switch (it->type) {
                case DELETE:
                    deleteBlock(filename, timestamp, it->block_number);
                    break;
                case ALTER:
                    alterBlock(filename, timestamp, it->block_number, it->data);
                    break;
                case APPEND:
                    appendBlock(filename, timestamp, it->data);
                    break;
                default:
                    cout << "Invalid instruction";
                    break;
            }
        }
    }
}
int main() {
    fullBackup("f1", 1, "this is the file");
    printFile("f1", 5);
    fullBackup("f2", 3, "I am Iron Man");
    printFile("f3", 3);
    printFile("f2", 3);
    fullBackup("f1", 4, "this is the new file");
    printFile("f1", 4);
    vector<Instruction> instructions;
    instructions.push_back({DELETE, 1, "\0"});
    incrementalBackup("f1", 2, instructions);
    printFile("f1", 3);
    instructions.clear();
    instructions.push_back({APPEND, -1, ", Morgan"});
    instructions.push_back({ALTER, 2, "CHANGED!"});
    incrementalBackup("f2", 4, instructions);
    printFile("f2",5);
    return 0;
}