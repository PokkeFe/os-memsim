#include <iostream>
#include <string>
#include <cstring>
#include "mmu.h"
#include "pagetable.h"

void printStartMessage(int page_size);
void createProcess(int text_size, int data_size, Mmu *mmu, PageTable *page_table);
int allocateVariable(uint32_t pid, std::string var_name, DataType type, uint32_t num_elements, Mmu *mmu, PageTable *page_table);
void setVariable(uint32_t pid, std::string var_name, uint32_t offset, void *value, Mmu *mmu, PageTable *page_table, void *memory);
void freeVariable(uint32_t pid, std::string var_name, Mmu *mmu, PageTable *page_table);
void terminateProcess(uint32_t pid, Mmu *mmu, PageTable *page_table);

// CUSTOM FUNCTIONS
void printCommand(std::string object, Mmu *mmu, PageTable *page_table, void *memory);
void launchSetVariable(uint32_t pid, std::string var_name, uint32_t offset, Mmu *mmu, PageTable *page_table, void *memory, Variable* variable, std::vector<std::string> command_list);
int getDataTypeSize(DataType type);
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
    std::string user_input;
    std::cout << "> ";
    std::getline (std::cin, user_input);
    while (user_input != "exit") {
        //Split full command line into command and arguments and store in command_list
        splitString(user_input, ' ', command_list);
        std::string command = command_list[0];

        if(command == "create") {
            createProcess(std::stoi(command_list[1]), std::stoi(command_list[2]), mmu, page_table);
        } else if(command == "allocate" || command == "set" || command == "free") {
            uint32_t pid = std::stoul(command_list[1]);
            std::string var_name = command_list[2];
            Process* process = mmu->getProcessByPID(pid);

            // If the command is allocate, set, or free then try to find the PID and variable. Then print the proper error 
            // based on if these are found or not. Otherwise, run the function.
            if(process != NULL) {
                Variable* variable = mmu->getVariableByProcessAndName(process, var_name);
                if(variable != NULL) {
                    if(command == "set") {
                        launchSetVariable(pid, var_name, std::stoul(command_list[3]), mmu, page_table, memory, variable, command_list);
                    } else if(command == "free") {
                        freeVariable(pid, var_name, mmu, page_table);
                    } else {
                        printf("error: variable already exists\n");
                    }
                } else {
                    if(command == "allocate") {
                        int virtual_addr = allocateVariable(pid, var_name, stringToDataType(command_list[3]), std::stoul(command_list[4]), mmu, page_table);
                        if(virtual_addr > -1) {
                            printf("%d\n", virtual_addr);
                        }
                    } else {
                        printf("error: variable not found\n");
                    }
                }
            } else {
                printf("error: process not found\n");
            }
        } else if(command == "terminate") {
            uint32_t pid = std::stoul(command_list[1]);
            if(mmu->getProcessByPID(pid) != NULL) {
                terminateProcess(pid, mmu, page_table);
            } else {
                printf("error: process not found\n");
            }
        } else if(command == "print") {
            printCommand(command_list[1], mmu, page_table, memory);
        } else {
            printf("error: command not recognized\n");
        }

        // Get next command
        std::cout << "> ";
        std::getline (std::cin, user_input);
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

int allocateVariable(uint32_t pid, std::string var_name, DataType type, uint32_t num_elements, Mmu *mmu, PageTable *page_table)
{
    // Initialize Data
    int size = getDataTypeSize(type);
    uint32_t virtual_addr = -1;

    // Search the page table to see if there is a location your variable will fit w/o allocating a new page.
    std::vector<std::string> process_pages = page_table->getAllPagesForPID(pid);
    for(std::vector<std::string>::iterator iter = process_pages.begin(); iter != process_pages.end() && virtual_addr == -1; ++iter)
    {
        // For each page table entry for process, check mmu for free space in that page
        int page = std::stoi(iter->substr(iter->find("|") + 1));
        virtual_addr = mmu->getFreeSpaceInPage(pid, page, size, page_table->getPageSize(), num_elements);
    }

    // Free space in existing page not found.
    if(virtual_addr == -1) 
    {
        // Run the process again for all free spaces, and get the page number to add entry
        virtual_addr = mmu->getFreeSpaceAnywhere(pid, size, page_table->getPageSize(), num_elements);
        //! if -1 returned, there is no free memory anywhere
        if(virtual_addr == -1)
        {
            printf("error: allocation exceeds system memory.\n");
            return -1;
        }
    }
    
    // Load page if memory area falls outside of loaded pages.
    int page = virtual_addr >> page_table->getOffsetSize();
    int end_page = virtual_addr + (size * num_elements) >> page_table->getOffsetSize();
    for(int i = page; i <= end_page; i++)
    {
        if(!page_table->entryExists(pid, i))
        {
            page_table->addEntry(pid, i);
        }
    }

    // Insert Variable into MMU and update Free Space
    mmu->addVariableToProcess(pid, var_name, type, size * num_elements, virtual_addr);
    mmu->updateFreeSpace(pid, virtual_addr, size * num_elements);

    // Print Virtual Memory Address
    //printf("Variable %10s added to Process %5d with size %8d at virtual addr %8d.\n", var_name.c_str(), pid, size * num_elements, virtual_addr);
    return virtual_addr;
}

void setVariable(uint32_t pid, std::string var_name, uint32_t offset, void *value, Mmu *mmu, PageTable *page_table, void *memory)
{
    //   * note: this function only handles a single element (i.e. you'll need to call this within a loop when setting multiple elements of an array)

    // Get process and initialize variable data
    Process* p = mmu->getProcessByPID(pid);
    uint32_t virtual_address = -1;
    DataType type;

    // Get variable information from process
    for(int vi = 0; vi < p->variables.size(); vi++)
    {
        Variable* v = p->variables[vi];
        if(var_name == v->name)
        {
            virtual_address = v->virtual_address;
            type = v->type;
        }
    }

    // Get physical address from page table
    uint32_t physical_address = page_table->getPhysicalAddress(pid, virtual_address + (offset * getDataTypeSize(type)));

    // Copy value into memory with an offset of the physical address
    memcpy(((char*)memory + physical_address), value, getDataTypeSize(type));
}

void freeVariable(uint32_t pid, std::string var_name, Mmu *mmu, PageTable *page_table)
{
    // Get vector of pages exclusively containing var_name
    std::vector<int> exclusive_pages = mmu->getExclusivePages(pid, var_name, page_table->getPageSize());

    // Remove entry from MMU
    mmu->removeVariable(pid, var_name);

    // Loop through the vector of exclusive pages and remove them from the page table
    for(int i = 0; i < exclusive_pages.size(); i++) {
        page_table->removeEntry(pid, exclusive_pages[i]);
    }
}

void terminateProcess(uint32_t pid, Mmu *mmu, PageTable *page_table)
{
    // Remove Process from the MMU
    mmu->removeProcess(pid);

    // Remove all pages for the process from the page table
    auto process_entries = page_table->getAllPagesForPID(pid);
    for(int i = 0; i < process_entries.size(); i++)
    {
        std::string entry = process_entries[i];
        page_table->removeEntry(entry);
    }
}



// ---------------------------------------------------------------------------------------------------------------- //
// ------------------------------------------------CUSTOM FUNCTIONS------------------------------------------------ //
// ---------------------------------------------------------------------------------------------------------------- //

/** Handles the print command if entered by the user.
 *  @param object The object to print. Either "mmu", "page", "processes", or "[PID]:[variable Name]"
 *  @param mmu Pointer to the mmu to print.
 *  @param page_table Pointer to the page table to print.
 *  @param memory Pointer to the memory to print the value of the given variable
 */
void printCommand(std::string object, Mmu *mmu, PageTable *page_table, void *memory) {
    if(object == "mmu") {
        mmu->print();
    } else if(object == "page") {
        page_table->print();
    } else if(object == "processes") {
        // Prints the PIDs of all running processes
        std::vector<Process*> processes = mmu->getProcessesVector();
        for(int i=0; i<processes.size(); i++) {
            std::cout << processes[i]->pid << std::endl;
        }
    } else {
        size_t delim_pos = object.find(":");
        uint32_t pid = std::stoul(object.substr(0, delim_pos));
        std::string var_name = object.substr(delim_pos+1);

        Variable* var = mmu->getVariableByProcessAndName(mmu->getProcessByPID(pid), var_name);
        int physical_address = page_table->getPhysicalAddress(pid, var->virtual_address);
        int data_size = getDataTypeSize(var->type);
        int num_elements = (int)(var->size / data_size);
        for(int i = 0; i < num_elements; i++) {
            // if not the first element, print a comma
            if(i > 0) {
                printf(", ");
            }
        
            if(i >= 4) // if ceiling hit, print etc and break out of the loop
            {
                printf("... [%d items]", num_elements);
                break;
            }
            else // else, print the value as normal
            {
                void* value = malloc(data_size);
                memcpy(value, (char*)memory + physical_address + (i * data_size), data_size);
                switch(var->type) {
                    case DataType::Char:
                        printf("%c", *(char*)value);
                        break;
                    case DataType::Short:
                        printf("%d", *(short*)value);
                        break;
                    case DataType::Int:
                        printf("%d", *(int*)value);
                        break;
                    case DataType::Float:
                        printf("%f", *(float*)value);
                        break;
                    case DataType::Double:
                        printf("%lf", *(double*)value);
                        break;
                }
                free(value);
            }
        }
        printf("\n");
    }
}

/** Launches setVariable() with the correct DataType.
 */
void launchSetVariable(uint32_t pid, std::string var_name, uint32_t offset, Mmu *mmu, PageTable *page_table, void *memory, Variable* variable, std::vector<std::string> command_list) {
    uint32_t var_type_size = getDataTypeSize(variable->type);   // Get the size of the type of variable
    DataType var_type = variable->type;                         // Get the type of the variable
    void* value = malloc(var_type_size);                        // Allocate memory depending on the size of the variable being set
    for(int i=4; i<command_list.size(); i++) {
        int local_offset = i - 4;
        // if offset goes beyond variable limits, break out of for loop
        if(offset + local_offset >= (variable->size/var_type_size)) break;

        // Convert the user input to the correct data type based on the data type of the variable being set
        switch(var_type) {
            case DataType::Char: 
                setVariable(pid, var_name, offset + local_offset, (void*)&command_list[i].c_str()[0], mmu, page_table, memory);
                break;
            case DataType::Int: {
                    int value = std::stoi(command_list[i]);
                    setVariable(pid, var_name, offset + local_offset, (void*)&value, mmu, page_table, memory);
                }
                break;
            case DataType::Long:
                {
                    long value = std::stol(command_list[i]);
                    setVariable(pid, var_name, offset + local_offset, (void*)&value, mmu, page_table, memory);
                }
                break;
            case DataType::Short:
                {
                    short value = (short)std::stoi(command_list[i]);
                    setVariable(pid, var_name, offset + local_offset, (void*)&value, mmu, page_table, memory);
                }
                break;
            case DataType::Float:
                {
                    float value = std::stof(command_list[i]);
                    setVariable(pid, var_name, offset + local_offset, (void*)&value, mmu, page_table, memory);
                }
                break;
            case DataType::Double:
                {
                    double value = std::stod(command_list[i]);
                    setVariable(pid, var_name, offset + local_offset, (void*)&value, mmu, page_table, memory);
                }
                break;
        }
    }
    free(value);
}

/** Converts a DataType to an integer equal to its corresponding size.
 *  @param type The given DataType to get the size of.
 *  @return Returns the corresponding size for the given DataType. Will return 0 if given FreeSpace.
 */
int getDataTypeSize(DataType type) {
    int size = 0; // in bytes
    switch(type)
    {
        case Char:
            size = 1;
            break;
        case Short:
            size = 2;
            break;
        case Int:
        case Float:
            size = 4;
            break;
        case Long:
        case Double:
            size = 8;
            break;
    }
    return size;
}

/** Converts a string to one of the DataType enumerators defined in mmu.cpp based on its string equivalent.
 *  @param input The user input string that is meant to be a DataType represented with text.
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
