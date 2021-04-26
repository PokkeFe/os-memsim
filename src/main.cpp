#include <iostream>
#include <string>
#include <cstring>
#include "mmu.h"
#include "pagetable.h"

void printStartMessage(int page_size);
void createProcess(int text_size, int data_size, Mmu *mmu, PageTable *page_table);
void allocateVariable(uint32_t pid, std::string var_name, DataType type, uint32_t num_elements, Mmu *mmu, PageTable *page_table);
void setVariable(uint32_t pid, std::string var_name, uint32_t offset, void *value, Mmu *mmu, PageTable *page_table, void *memory);
void freeVariable(uint32_t pid, std::string var_name, Mmu *mmu, PageTable *page_table);
void terminateProcess(uint32_t pid, Mmu *mmu, PageTable *page_table);

// CUSTOM FUNCTIONS
int getDataTypeSize(DataType type);
void printCommand(std::string object, Mmu *mmu, PageTable *page_table);
DataType stringToDataType(std::string input);
void splitString(std::string text, char d, std::vector<std::string>& result);

int main(int argc, char **argv)
{
    // Ensure user specified page size as a command line parameter
    if (argc < 2)
    {
        fprintf(stderr, "Error: you must specify the page size\n");
        return 1;
    }

    // Print opening instuction message
    int page_size = std::stoi(argv[1]);
    printStartMessage(page_size);

    // Create physical 'memory'
    uint32_t mem_size = 67108864;
    void *memory = malloc(mem_size); // 64 MB (64 * 1024 * 1024)

    // Create MMU and Page Table
    Mmu *mmu = new Mmu(mem_size);
    PageTable *page_table = new PageTable(page_size);

    // Prompt loop
    std::vector<std::string> command_list;
    std::string command;
    std::cout << "> ";
    std::getline (std::cin, command);
    while (command != "exit") {
        //Split full command line into command and arguments and store in command_list
        splitString(command, ' ', command_list);

        //<---------------------------------------------------Currently will break unless all the arguments are formatted correctly
        if(command_list[0] == "create") {
            createProcess(std::stoi(command_list[1]), std::stoi(command_list[2]), mmu, page_table);
        } else if(command_list[0] == "allocate") {
            allocateVariable(std::stoul(command_list[1]), command_list[2], stringToDataType(command_list[3]), std::stoul(command_list[4]), mmu, page_table);
        } else if(command_list[0] == "set") {
            // TODO: Implement set
        } else if(command_list[0] == "free") {
            freeVariable(std::stoul(command_list[1]), command_list[2], mmu, page_table);
        } else if(command_list[0] == "terminate") {
            terminateProcess(std::stoul(command_list[1]), mmu, page_table);
        } else if(command_list[0] == "print") {
            printCommand(command_list[1], mmu, page_table);
        } else {
            std::cout << "error: command not recognized" << std:: endl;
        }

        // Get next command
        std::cout << "> ";
        std::getline (std::cin, command);
    }

    // Clean up
    free(memory);
    delete mmu;
    delete page_table;

    return 0;
}

void printStartMessage(int page_size)
{
    std::cout << "Welcome to the Memory Allocation Simulator! Using a page size of " << page_size << " bytes." << std:: endl;
    std::cout << "Commands:" << std:: endl;
    std::cout << "  * create <text_size> <data_size> (initializes a new process)" << std:: endl;
    std::cout << "  * allocate <PID> <var_name> <data_type> <number_of_elements> (allocated memory on the heap)" << std:: endl;
    std::cout << "  * set <PID> <var_name> <offset> <value_0> <value_1> <value_2> ... <value_N> (set the value for a variable)" << std:: endl;
    std::cout << "  * free <PID> <var_name> (deallocate memory on the heap that is associated with <var_name>)" << std:: endl;
    std::cout << "  * terminate <PID> (kill the specified process)" << std:: endl;
    std::cout << "  * print <object> (prints data)" << std:: endl;
    std::cout << "    * If <object> is \"mmu\", print the MMU memory table" << std:: endl;
    std::cout << "    * if <object> is \"page\", print the page table" << std:: endl;
    std::cout << "    * if <object> is \"processes\", print a list of PIDs for processes that are still running" << std:: endl;
    std::cout << "    * if <object> is a \"<PID>:<var_name>\", print the value of the variable for that process" << std:: endl;
    std::cout << std::endl;
}

void createProcess(int text_size, int data_size, Mmu *mmu, PageTable *page_table)
{
    //   - create new process in the MMU
    int pid = mmu->createProcess();
    //   - allocate new variables for the <TEXT>, <GLOBALS>, and <STACK>
    allocateVariable(pid, "<TEXT>", Char, text_size, mmu, page_table);
    allocateVariable(pid, "<GLOBALS>", Char, data_size, mmu, page_table);
    allocateVariable(pid, "<STACK>", Char, 65536, mmu, page_table);
    //   - print pid
    printf("%d\n", pid);
}

void allocateVariable(uint32_t pid, std::string var_name, DataType type, uint32_t num_elements, Mmu *mmu, PageTable *page_table)
{
    //first search the page table to see if there is a location your variable will fit w/o allocating a new page.
    int size = getDataTypeSize(type);
    uint32_t virtual_addr = -1;
    std::vector<std::string> process_pages = page_table->getAllPagesForPID(pid);
    for(std::vector<std::string>::iterator iter = process_pages.begin(); iter != process_pages.end() && virtual_addr == -1; ++iter) {
        // for each page table entry for process, check mmu for free space in that page
        int page = std::stoi(iter->substr(iter->find("|") + 1));
        virtual_addr = mmu->getFreeSpaceInPage(pid, page, size, page_table->getPageSize(), num_elements);
        // if a free space is found, we don't have to make new page in the page table
    }
    //2 bytes left on page but trying to allocate 4 byte integer -> DO NOt SPLit an individual item accros pages. need to have contiguous memory addresses
    //in mmu loop through adn find the free space variables. 

    if(virtual_addr == -1) {
        // a free spot in an existing page was not found, we need to run the process again for all free spaces, and get the page number to add entry
        virtual_addr = mmu->getFreeSpaceAnywhere(pid, size, page_table->getPageSize(), num_elements);
        // if -1 returned, there is no free memory anywhere
        // allocate new page based off new virtual address
        if(virtual_addr == -1) {
            exit(-5);
        }
    }
    
    int page = virtual_addr >> page_table->getOffsetSize();
    int end_page = virtual_addr + (size * num_elements) >> page_table->getOffsetSize();
    for(int i = page; i <= end_page; i++) {
        if(!page_table->entryExists(pid, i)) {
            page_table->addEntry(pid, i);
        }
    }

    mmu->addVariableToProcess(pid, var_name, type, size * num_elements, virtual_addr);
    mmu->updateFreeSpace(pid, virtual_addr, size * num_elements);

    // TODO: Update this
    printf("Variable %10s added to Process %5d with size %8d at virtual addr %8d.\n", var_name.c_str(), pid, size * num_elements, virtual_addr);

    //to know what page to pass in to the page table.addentry -> add methods to page table
    //   - find first free space within a page already allocated to this process that is large enough to fit the new variable
    //   - if no hole is large enough, allocate new page(s)
    //   - insert variable into MMU
    //   - print virtual memory address
}

void setVariable(uint32_t pid, std::string var_name, uint32_t offset, void *value, Mmu *mmu, PageTable *page_table, void *memory)
{
    // TODO: implement this!
    //   - look up physical address for variable based on its virtual address / offset
    //   - insert `value` into `memory` at physical address
    //   * note: this function only handles a single element (i.e. you'll need to call this within a loop when setting
    //           multiple elements of an array)
}

void freeVariable(uint32_t pid, std::string var_name, Mmu *mmu, PageTable *page_table)
{
    // TODO: implement this!
    //   - remove entry from MMU
    //   - free page if this variable was the only one on a given page
}

void terminateProcess(uint32_t pid, Mmu *mmu, PageTable *page_table)
{
    // TODO: implement this!
    //   - remove process from MMU
    //   - free all pages associated with given process
}



// ---------------------------------------------------------------------------------------------------------------- //
// ------------------------------------------------CUSTOM FUNCTIONS------------------------------------------------ //
// ---------------------------------------------------------------------------------------------------------------- //

void printCommand(std::string object, Mmu *mmu, PageTable *page_table) {
    if(object == "mmu") {
        mmu->print();
    } else if(object == "page") {
        page_table->print();
    } else if(object == "processes") {
        //TODO: Print the PIDs of all running processes
    } else {
        size_t delim_pos = object.find(":");
        uint32_t pid = std::stoul(object.substr(0, delim_pos));
        std::string var_name = object.substr(delim_pos+1);
        //TODO: Print the value of the given variable for the given process PID
    }

}

/** Converts a DataType to an integer equal to its corresponding size.
 *  @return Returns the corresponding size for the given DataType. Will return 0 if given FreeSpace.
 */
int getDataTypeSize(DataType type) {
    int size = 0; // in bytes
    switch(type) {
        case Char:
            size = 1;
            break;
        case Short:
            size = 2;
            break;
        case Int:
        case Float:
            size = 4;
        case Long:
        case Double:
            size = 8;
    }
    return size;
}

/** Converts a string to one of the DataType enumerators defined in mmu.cpp based on its string equivalent.
 *  @return Returns the associated DataType enum or FreeSpace if no DataType can be associated.
 */
DataType stringToDataType(std::string input) {
    if(input == "char") {
        return DataType::Char;
    } else if(input == "short") {
        return DataType::Short;
    } else if(input == "int") {
        return DataType::Int;
    } else if(input == "float") {
        return DataType::Float;
    } else if(input == "long") {
        return DataType::Long;
    } else if(input == "double") {
        return DataType::Double;
    } else {
        //If freespace is returned, that means the string was unrecognized
        return DataType::FreeSpace; 
    }
}

/* [splitSting() from Assignment 2 - osshell]
   text: string to split
   d: character delimiter to split `text` on
   result: vector of strings - result will be stored here
*/
void splitString(std::string text, char d, std::vector<std::string>& result)
{
    enum states { NONE, IN_WORD, IN_STRING } state = NONE;

    int i;
    std::string token;
    result.clear();
    for (i = 0; i < text.length(); i++)
    {
        char c = text[i];
        switch (state) {
            case NONE:
                if (c != d)
                {
                    if (c == '\"')
                    {
                        state = IN_STRING;
                        token = "";
                    }
                    else
                    {
                        state = IN_WORD;
                        token = c;
                    }
                }
                break;
            case IN_WORD:
                if (c == d)
                {
                    result.push_back(token);
                    state = NONE;
                }
                else
                {
                    token += c;
                }
                break;
            case IN_STRING:
                if (c == '\"')
                {
                    result.push_back(token);
                    state = NONE;
                }
                else
                {
                    token += c;
                }
                break;
        }
    }
    if (state != NONE)
    {
        result.push_back(token);
    }
}
