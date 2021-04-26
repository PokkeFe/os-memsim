#include "mmu.h"

Mmu::Mmu(int memory_size)
{
    _next_pid = 1024;
    _max_size = memory_size;
}

Mmu::~Mmu()
{
}

uint32_t Mmu::createProcess()
{
    Process *proc = new Process();
    proc->pid = _next_pid;

    Variable *var = new Variable();
    var->name = "<FREE_SPACE>";
    var->type = DataType::FreeSpace;
    var->virtual_address = 0;
    var->size = _max_size;
    proc->variables.push_back(var);

    _processes.push_back(proc);

    _next_pid++;
    return proc->pid;
}

void Mmu::addVariableToProcess(uint32_t pid, std::string var_name, DataType type, uint32_t size, uint32_t address)
{
    int i;
    Process *proc = NULL;
    for (i = 0; i < _processes.size(); i++) // add condition to leave process loop once process found? optimization
    {
        if (_processes[i]->pid == pid)
        {
            proc = _processes[i];
        }
    }

    Variable *var = new Variable();
    var->name = var_name;
    var->type = type;
    var->virtual_address = address;
    var->size = size;
    if (proc != NULL)
    {
        proc->variables.push_back(var);
    }
}

void Mmu::print()
{
    int i, j;

    std::cout << " PID  | Variable Name | Virtual Addr | Size" << std::endl;
    std::cout << "------+---------------+--------------+------------" << std::endl;
    for (i = 0; i < _processes.size(); i++)
    {
        for (j = 0; j < _processes[i]->variables.size(); j++)
        {
            // TODO: print all variables (excluding <FREE_SPACE> entries)
            Variable* v = _processes[i]->variables[j];
            printf(" %4d | %-14s|   0x%08X |%11d\n", _processes[i]->pid, v->name.c_str(), v->virtual_address, v->size);
        }
    }
}



// ---------------------------------------------------------------------------------------------------------------- //
// ------------------------------------------------CUSTOM FUNCTIONS------------------------------------------------ //
// ---------------------------------------------------------------------------------------------------------------- //

Process* Mmu::getProcessByPID(int pid) {
    int i;
    for(i = 0; i < _processes.size(); i++) {
        if(_processes[i]->pid == pid) return _processes[i];
    }
    return NULL;
}

std::vector<Variable*> Mmu::getFreeSpaceVector(int pid) {
    // loop through each variable for process with pid
    Process* p = getProcessByPID(pid);
    std::vector<Variable*> freeSpaceVariables;

    int i;
    for(i = 0; i < p->variables.size(); i++) {
        Variable* v = p->variables[i];
        if(v->type == FreeSpace) freeSpaceVariables.push_back(v);
    }

    return freeSpaceVariables;
}

uint32_t Mmu::getFreeSpaceInPage(int pid, int page, int size, int page_size, int num_elements) {
    Process* p = getProcessByPID(pid);
    // starting at base addr of page
    // for each addr until addr - size + 1
    // - check each free space variable
    // - if addr -> addr + size can fit in free space variable, we have a match (freeSpace addr <= addr AND freeSpace addr + freeSpace size >= addr + size)
    // - return addr
    // if no match found, return -1
    int addr = page * page_size;
    while((addr % page_size) < page_size - size + 1) {
        for(int vi = 0; vi < p->variables.size(); vi++) {
            Variable* v = p->variables[vi];
            if(v->type != FreeSpace) {
                continue;
            } else {
                if(v->virtual_address <= addr && (v->virtual_address + v->size) >= (addr + (size * num_elements))) {
                    // if here, addr is contained by free space
                    return addr;
                }
            }
        }
        addr += 1;
    }
    return -1;
}

uint32_t Mmu::getFreeSpaceAnywhere(int pid, int size, int page_size, int num_elements) {
    Process* p = getProcessByPID(pid);
    
    // for each free space in process
    for(int vi = 0; vi < p->variables.size(); vi++) {
        Variable* v = p->variables[vi];
        if(v->type != FreeSpace) {
            continue;
        } else {
            // if the free space address can fit the var and free space addr + size does not overflow page boundaries, return the free space address 
            if(v->size >= size && (v->virtual_address % page_size) + size <= page_size) {
                // can fit variable at start of free space
                // if multiple, can it fit the whole array?
                if(num_elements > 1) {
                    if(v->size >= size * num_elements) {
                        return v->virtual_address;
                    }
                } else {
                    return v->virtual_address;
                }
                
            } else {
                // can't fit variable at start of free space, try to fit at start of next page
                int next_page_addr = v->virtual_address + (page_size - (v->virtual_address % page_size));
                if(next_page_addr + size <= v->virtual_address + v->size) {
                    // if multiple, can it fit the whole array?
                    if(num_elements > 1) {
                        if(next_page_addr + (size * num_elements) <= v->virtual_address + v->size) {
                            return next_page_addr;
                        }
                    } else {
                        return next_page_addr;
                    }
                }
            }
        }
    }
    return -1;
}

void Mmu::updateFreeSpace(int pid, int virtual_address, int size) {
    Process* p = getProcessByPID(pid);
    for(int vi = 0; vi < p->variables.size(); vi++) {
        Variable* v = p->variables[vi];
        if(v->type != FreeSpace) {
            continue;
        } else {
            // check if the free space contains the provided slice
            if(v->virtual_address <= virtual_address && v->virtual_address + v->size >= virtual_address + size) {
                int left_slice = virtual_address - v->virtual_address;
                int right_slice = v->size - (left_slice + size);
                // if a left slice exists, set the original to the left slice and add a new right free space of the remaining size. otherwise, set the original to the right slice
                if(left_slice > 0) {
                    v->size = left_slice;
                    if(right_slice > 0) {
                        addVariableToProcess(pid, "<FREE_SPACE>", FreeSpace, right_slice, virtual_address + size);
                    }
                } else {
                    v->virtual_address = virtual_address + size;
                    v->size = right_slice;
                }
            }
        }
    }

}