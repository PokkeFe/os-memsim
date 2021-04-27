#include "pagetable.h"
#include <cmath>

PageTable::PageTable(int page_size)
{
    _page_size = page_size;
}

PageTable::~PageTable()
{
}

std::vector<std::string> PageTable::sortedKeys()
{
    std::vector<std::string> keys;

    std::map<std::string, int>::iterator it;
    for (it = _table.begin(); it != _table.end(); it++)
    {
        keys.push_back(it->first);
    }

    std::sort(keys.begin(), keys.end(), PageTableKeyComparator());

    return keys;
}

void PageTable::addEntry(uint32_t pid, int page_number)
{
    // Combination of pid and page number act as the key to look up frame number
    std::string entry = std::to_string(pid) + "|" + std::to_string(page_number);

    int frame = 0; 
    // Find free frame
    // TODO: TEST!
    // check each frame, if frame is already assigned, increment frame and try again
    std::map<std::string,int>::iterator it = _table.begin();
    while(it != _table.end()) {
        if(it->second == frame) {
            frame++;
            it = _table.begin();
        } else {
            it++;
        }
    }
    _table[entry] = frame;
}

int PageTable::getPhysicalAddress(uint32_t pid, uint32_t virtual_address)
{
    // Convert virtual address to page_number and page_offset
    // TODO: TEST
    int offset_size = (int)log2((double)_page_size);
    int page_number = (virtual_address >> offset_size);
    int page_offset = ((0xFFFFFFFF >> offset_size) & virtual_address);
    

    // Combination of pid and page number act as the key to look up frame number
    std::string entry = std::to_string(pid) + "|" + std::to_string(page_number);
    
    // If entry exists, look up frame number and convert virtual to physical address
    int address = -1;
    if (_table.count(entry) > 0)
    {
        // TODO: TEST
        address = (_table[entry] * _page_size) + page_offset;
    }

    return address;
}

void PageTable::print()
{
    int i;

    std::cout << " PID  | Page Number | Frame Number" << std::endl;
    std::cout << "------+-------------+--------------" << std::endl;

    std::vector<std::string> keys = sortedKeys();

    std::string s, pid, page_n;
    int delim_pos, frame_n;

    for (i = 0; i < keys.size(); i++)
    {
        // get pid and page number from string
        s = keys[i];
        delim_pos = s.find("|");
        pid = s.substr(0, delim_pos);
        page_n = s.substr(delim_pos + 1, s.length() - 1);
        frame_n = _table[keys[i]];
        printf("%6s|%13s|%14d\n", pid.c_str(), page_n.c_str(), frame_n);
    }
}



// ---------------------------------------------------------------------------------------------------------------- //
// ------------------------------------------------CUSTOM FUNCTIONS------------------------------------------------ //
// ---------------------------------------------------------------------------------------------------------------- //

/** Gets all the pages for a given PID
 * @param pid ID of process.
 * @return Vector of all the entries associated with the provided process.
 */
std::vector<std::string> PageTable::getAllPagesForPID(uint32_t pid) 
{
    std::vector<std::string> keys = sortedKeys();
    std::vector<std::string> entries;
    std::string s;
    int i, entry_pid;

    for (i = 0; i < keys.size(); i++)
    {
        s = keys[i];
        entry_pid = stoi(s.substr(0, s.find("|")));
        if(entry_pid == pid)
        {
            entries.push_back(keys[i]);
        }
    }

    return entries;
}

/** Gets the size of the page
 * @return The size of a page
 */
int PageTable::getPageSize() {
    return _page_size;
}

/** Gets the number of bytes to offset for retrieving page value.
 * @return Size of offset in bytes.
 */
int PageTable::getOffsetSize() {
    return (int)log2((double)_page_size);
}

/** Checks the table to see if the page exists for the given PID
 * @param pid ID of the process to check.
 * @param page_number Page to check.
 * @return True if the page exists for that process. False otherwise.
 */
bool PageTable::entryExists(uint32_t pid, int page_number) {
    std::string entry = std::to_string(pid) + "|" + std::to_string(page_number);
    if(_table.count(entry) > 0) {
        return true;
    }
    return false;
}

/** Removes an entry from the page table
 * @param pid ID of process to remove entry from
 * @param page_number Page number to remove.
 */
void PageTable::removeEntry(uint32_t pid, int page_number) {
    std::string entry = std::to_string(pid) + "|" + std::to_string(page_number);
    if(_table.count(entry) > 0) {
        _table.erase(entry);
    }
}

/** Removes an entry from the page table
 * @param entry Entry key to try to remove from the table
 */
void PageTable::removeEntry(std::string entry) {
    if(_table.count(entry) > 0) {
        _table.erase(entry);
    }
}