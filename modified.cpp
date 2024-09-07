#include <iostream>
#include <cstdint>
#include <vector>
#include <cmath>
#include <limits>
#include <fstream>
#include <sstream>

using namespace std;

int hit = 0;
int miss = 0;
int accesscount = 0;

struct CacheLine
{
    bool valid;
    uint32_t tag;
    int LRUcount;

    CacheLine() : valid(false), tag(0), LRUcount(0) {};
};

struct CacheSet
{
    vector<CacheLine> lines;

    CacheSet(int associativity) : lines(associativity) {};
};

uint32_t getIndex(uint32_t address, uint32_t blockSize, uint32_t cacheLineNum)
{
    uint32_t offsetBits = (uint32_t)log2(blockSize);
    uint32_t indexBits = (uint32_t)log2(cacheLineNum);
    return ((address >> offsetBits) & ((1 << indexBits) - 1));
}

uint32_t getTag(uint32_t address, uint32_t blockSize, uint32_t cacheLineNum)
{
    uint32_t offsetBits = (uint32_t)log2(blockSize);
    uint32_t indexBits = (uint32_t)log2(cacheLineNum);
    return address >> (offsetBits + indexBits);
}

void CacheAccess(uint32_t address, vector<CacheSet> &cache, int setAssociativity, uint32_t blockSize, uint32_t cacheLineNum)
{
    uint32_t index = getIndex(address, blockSize, cacheLineNum);
    uint32_t tag = getTag(address, blockSize, cacheLineNum);

    CacheSet &cachemem = cache[index];

    // Cache Hit case
    for (int i = 0; i < setAssociativity; i++)
    {
        if (cachemem.lines[i].valid && cachemem.lines[i].tag == tag)
        {
            hit++;
            cachemem.lines[i].LRUcount = accesscount++;
            // Update LRU counts for all lines
            for (int j = 0; j < setAssociativity; j++)
            {
                if (i != j && cachemem.lines[j].valid)
                {
                    cachemem.lines[j].LRUcount++;
                }
            }
            return;
        }
    }

    int tracker = 0;
    uint32_t minUsed = std::numeric_limits<uint32_t>::max();

    // Cache Miss case by LRU principle
    for (int i = 0; i < setAssociativity; i++)
    {
        if (!cachemem.lines[i].valid)
        {
            tracker = i;
            break;
        }
        if (cachemem.lines[i].LRUcount < minUsed)
        {
            tracker = i;
            minUsed = cachemem.lines[i].LRUcount;
        }
    }

    // Cache Miss case where we have to replace a block
    miss++;
    cachemem.lines[tracker].valid = true;
    cachemem.lines[tracker].tag = tag;
    cachemem.lines[tracker].LRUcount = accesscount++;
    // Update LRU counts for all lines
    for (int i = 0; i < setAssociativity; i++)
    {
        if (tracker != i && cachemem.lines[i].valid)
        {
            cachemem.lines[i].LRUcount++;
        }
    }
}

void simulateCache(uint32_t cacheSize, uint32_t blockSize, int setAssociativity, const string &traceFile, ofstream &csvFile)
{
    // Calculate number of cache sets
    uint32_t cacheLineNum = cacheSize / (blockSize * setAssociativity);
    if (cacheLineNum == 0)
    {
        cerr << "Invalid configuration: Cache line number is zero" << endl;
        return;
    }

    // Initialize the cache
    vector<CacheSet> cache(cacheLineNum, CacheSet(setAssociativity));

    // Reset hit/miss counters
    hit = 0;
    miss = 0;
    accesscount = 0;

    ifstream tracefile(traceFile);
    if (!tracefile.is_open())
    {
        cerr << "Unable to open file: " << traceFile << endl;
        return;
    }

    string line;
    while (getline(tracefile, line))
    {
        stringstream ss(line);
        char accessType; //'l' or 's' which we will ignore
        uint32_t address;
        int instructionsSinceLastAccess; // number of instructions since last access which we will ignore

        ss >> accessType >> hex >> address >> instructionsSinceLastAccess;
        CacheAccess(address, cache, setAssociativity, blockSize, cacheLineNum);
    }

    tracefile.close();

    // Output the results to the CSV file
    double hitRate = (double)hit / (hit + miss) * 100;
    double missRate = (double)miss / (hit + miss) * 100;

    csvFile << traceFile << "," << cacheSize / 1024 << "," << blockSize << "," << setAssociativity << ","
            << hitRate << "," << missRate << endl;
}

int main()
{
    // Fixed trace file paths
    vector<string> traceFiles = {
        "gcc.trace",
        "gzip.trace",
        "mcf.trace",
        "swim.trace",
        "twolf.trace"};

    // Create and open a CSV file
    ofstream csvFile("cache_simulation_results.csv");
    if (!csvFile.is_open())
    {
        cerr << "Unable to open CSV file for writing" << endl;
        return 1;
    }

    // Write CSV headers
    csvFile << "Trace File,Cache Size (KB),Block Size (Bytes),Associativity,Hit Rate (%),Miss Rate (%)" << endl;

    // Constants for varying one parameter at a time
    uint32_t fixedCacheSize = 1024 * 1024; // 1 MB cache size
    uint32_t fixedBlockSize = 4;           // 4-byte block size
    int fixedAssociativity = 4;            // 4-way associative

    // Question a: Fixed configuration
    for (const string &traceFile : traceFiles)
    {
        simulateCache(fixedCacheSize, fixedBlockSize, fixedAssociativity, traceFile, csvFile);
    }

    // Question b: Vary cache size from 128 KB to 4096 KB
    for (const string &traceFile : traceFiles)
    {
        for (uint32_t cacheSize = 128 * 1024; cacheSize <= 4096 * 1024; cacheSize *= 2)
        {
            simulateCache(cacheSize, fixedBlockSize, fixedAssociativity, traceFile, csvFile);
        }
    }

    // Question c: Vary block size from 1 Byte to 128 Bytes
    for (const string &traceFile : traceFiles)
    {
        for (uint32_t blockSize = 1; blockSize <= 128; blockSize *= 2)
        {
            simulateCache(fixedCacheSize, blockSize, fixedAssociativity, traceFile, csvFile);
        }
    }

    // Question d: Vary associativity from 1-way to 64-way
    for (const string &traceFile : traceFiles)
    {
        for (int associativity = 1; associativity <= 64; associativity *= 2)
        {
            simulateCache(fixedCacheSize, fixedBlockSize, associativity, traceFile, csvFile);
        }
    }

    // Close the CSV file
    csvFile.close();

    return 0;
}
