#include <stdlib.h>
#include <iostream>
#include <unordered_map>
#include <bits/stdc++.h>
#include <omp.h>

#define NUM_BLOCKS 4
#define SIZE_BLOCK 4
#define NUM_LINES 2
#define SIZE_LINE 2
#define NUM_CORES 4

// defining global state variables

enum cache_status
{
    invalid=-1,
    shared=0,
    exclusive=1,
    modified=2
};

enum inst_type
{
    read=0,
    write=1
};

enum core_status
{
    available=0,
    busy=1
};

std::unordered_map<int,cache_status> access_log; // int is line number on the cache line state mentions the status of the line 
std::unordered_map<int,core_status> core_log; // int is the core number on the cpu and core     status tells if the core is busy or not
std::unordered_map<int,int> lookup_table; // int is the cache line and second int is the addresses present in the line

struct decoded_inst 
{
    inst_type type; // 0 is RD, 1 is WR
    int address;
    short value; // Only used for WR 
};
typedef struct decoded_inst decoded;

struct cache_byte
{
    int addr;
    short val;
};
typedef struct cache_byte byte;

struct cache_line
{
    int base;
    byte* bytes=(byte*)malloc(SIZE_LINE*sizeof(byte));
    cache_status line_state;
};
typedef struct cache_line line;

struct cache_block
{
    line *lines=(line*)malloc(SIZE_BLOCK*sizeof(line));
};
typedef struct cache_block block;

decoded decode_inst_line(const char * buffer)
{
    decoded inst;
    char inst_type[2];
    sscanf(buffer, "%s", inst_type);
    if(!strcmp(inst_type, "RD")){
        inst.type = read;
        int addr = 0;
        sscanf(buffer, "%s %d", inst_type, &addr);
        inst.value = -1;
        inst.address = addr;
    } else if(!strcmp(inst_type, "WR")){
        inst.type = write;
        int addr = 0;
        int val = 0;
        sscanf(buffer, "%s %d %d", inst_type, &addr, &val);
        inst.address = addr;
        inst.value = (val);
    }
    return inst;
}

byte empty_byte()
{
    byte b;
    b.addr=0;
    b.val=0;
    return b;
}

class RAM_Module
{
private:
    int blocks,block_size;
    int overall;
    int *Memory;
public:
    // class constructor and deconstructor

    RAM_Module(int n=1,int s=1)
    {
        this->blocks=n;
        this->block_size=s;
    };
    ~RAM_Module()
    {
        free(Memory);
    }
    
    // setter functions 

    void set_size(int n)
    {
        this->block_size=n;
    }

    void set_num(int n)
    {
        this->blocks=n;
    }

    void init()
    {
        this->Memory=(int *)malloc(this->blocks*this->block_size*sizeof(int));
        this->overall=this->blocks*this->block_size;
    }

    void populate()
    {
        for (int i = 0; i < this->overall; ++i)
        {
            *(Memory+i)=rand()%8*this->block_size;
        }
    }

    void empty_block(int b)
    {
        int ini=b*SIZE_BLOCK;
        for (int i = 0; i < SIZE_BLOCK; ++i)
        {
            *(Memory+ini+i)=0;
        }
    }

    int get_size()
    {
        return this->block_size;
    }

    int get_num()
    {
        return this->blocks;
    }

    int *get_block(int n)
    {
        int ini=n*this->block_size;
        int *arr=(int *)malloc(this->block_size*sizeof(int));
        for (int i =0 ; i < this->block_size; ++i)
        {
            *(arr+i)=*(Memory+ini+i);
        }
        return arr;
    }

    void write_block(int b,int *arr)
    {
        int ini=b*this->block_size;
        for (int i = 0; i < this->block_size; ++i)
            {
                *(Memory+ini+i)=*(arr+i);
            }
    }

    line get_line(int b,int l,int size)
    {
        line* temp=(line*)(malloc(sizeof(line)));
        int ini=b*this->block_size+l*size;
        temp->base=ini;
        for (int i = 0; i < size; ++i)
        {
            byte b1;
            b1.addr=ini+i;
            b1.val=*(Memory+ini+i);
            temp->bytes[i]=b1;
        }
        temp->line_state=exclusive;
        return *temp;
    }

    void write_line(int b,int l, int size, int* arr)
    {
        int ini=b*this->block_size+l*size;
        for (int i = 0; i < size; ++i)
            {
                *(Memory+ini+i)=*(arr+i);
            }
    }

    void print_block(int n)
    {
        int ini=n*this->block_size;
        for (int i =0 ; i < this->block_size; ++i)
        {
            std::cout<<*(Memory+ini+i)<<std::endl;
        }
    }
};

class CPU_unit
{
private:
    RAM_Module ram;
    int cores=NUM_CORES;
    block caches[NUM_CORES];
public:
    CPU_unit(RAM_Module r)
    {
        this->ram=r;
    }

    void init_caches()
    {
        for (int j = 0; j < NUM_CORES; ++j)
        {
            block cache=caches[j];
            for (int i = 0; i < NUM_LINES; ++i)
            {
                line l=cache.lines[i];
                l.line_state=invalid;
                for (int k = 0; k < SIZE_LINE; ++k)
                {
                    byte b=empty_byte();
                    l.bytes[i]=b;
                }
            }
        }
    }

    void print_all_caches()
    {
        for (int j = 0; j < NUM_CORES; ++j)
        {
            block cache=caches[j];
            std::cout<<"printing cache j = "<<j<<std::endl;
            for (int i = 0; i < NUM_LINES; ++i)
            {
                line l=cache.lines[i];
                std::cout<<"line number = "<<i<<" status = "<<l.line_state<<std::endl;
                print_line(l);
            }
        }
    }

    void print_line(line l)
    {
        for (int j = 0; j < SIZE_LINE; ++j)
        {
            byte b=l.bytes[j];
            std::cout<<b.addr<<" = "<<b.val<<"\t";
        }
    }

    void auto_assign(std::string paths[],int n)
    {
        #pragma omp parallel for num_threads(NUM_CORES)
        for (int i = 0; i < n; ++i)
        {
            assign_file_core(i,paths[i]);
        }

        #pragma omp single
        {
            print_all_caches();
        }
    }

    void assign_file_core(int n,std::string file_path)
    {
        if(core_log[n]==available)
        {   
            core_log[n]=busy;
            core_run_file(file_path,n,caches[n]);
            core_log[n]=available;
        }
        else
            perror("[+] core is busy");
    }

    void core_run_file(std::string file_path,int n,block cache)
    {
        std::cout<<"starting execution of program on core "<<n<<std::endl;
        std::ifstream f(file_path);
        int ctr=0;
        while(!f.eof())
        {
            std::cout<<"\t running instruction "<<ctr<<std::endl;
            std::string inst_line;
            getline(f,inst_line);
            const char *buff=inst_line.c_str();
            decoded inst = decode_inst_line(buff);
            run_inst(inst,n,cache);
            ctr++;
        }
        std::cout<<"end of program"<<std::endl;
    }

    void run_inst(decoded inst,int n,block cache)
    {
        int addr=inst.address;
        if (inst.type==read)
        {
            bool c=check_cache(cache,addr);
            bool flag=check_other_caches(addr);
            if(!c)
            {
                cache.lines[addr%NUM_LINES]=get_line_from_other_cache(addr);
                if(flag)
                    cache.lines[addr%NUM_LINES].line_state=shared;
                else
                    cache.lines[addr%NUM_LINES].line_state=exclusive;
            }
            std::cout<<"reading data from address "<<addr<<"value = "<<get_val(cache,addr)<<std::endl;
        }
        else if(inst.type==write)
        {
            bool c=check_cache(cache,addr);
            bool flag=check_other_caches(addr);
            if (!c)
            {
                cache.lines[addr%NUM_LINES]=get_line_from_other_cache(addr);
                invalidate_other_caches(addr);
            }
            cache.lines[addr%NUM_LINES].line_state=modified;
            cache.lines[addr%NUM_LINES].bytes[addr%SIZE_LINE].val=inst.value;
            std::cout<<"writing data to address "<<addr<<std::endl;

        }
    }

    void invalidate_other_caches(int addr)
    {
        int b=addr-(addr%SIZE_LINE);
        for (int j = 0; j < NUM_CORES; ++j)
        {
            block cache=caches[j];
            for (int i = 0; i < NUM_LINES; ++i)
            {
                if(cache.lines[i].base==b)
                    cache.lines[i].line_state=invalid;
            }
        }
    }

    bool check_other_caches(int addr)
    {
        int b=addr-(addr%SIZE_LINE);
        for (int j = 0; j < NUM_CORES; ++j)
        {
            block cache=caches[j];
            for (int i = 0; i < NUM_LINES; ++i)
            {
                if(cache.lines[i].base==b)
                    return true;
            }
        }
        return false;
    }

    line get_line_from_other_cache(int addr)
    {
        int b=addr-(addr%SIZE_LINE);
        for (int j = 0; j < NUM_CORES; ++j)
        {
            block cache=caches[j];
            for (int i = 0; i < NUM_LINES; ++i)
            {
                if(cache.lines[i].base==b && cache.lines[i].line_state==shared)
                    return cache.lines[i];
                else if(cache.lines[i].base==b && cache.lines[i].line_state==exclusive)
                {
                    cache.lines[i].line_state=shared;
                    return cache.lines[i];
                }
                else if(cache.lines[i].base==b && cache.lines[i].line_state==modified)
                {
                    cache.lines[i].line_state=shared;
                    return cache.lines[i];
                }
            }
        }
        return ram.get_line(addr%NUM_BLOCKS,addr%SIZE_BLOCK,SIZE_LINE);
    }

    short get_val(block cache,int addr)
    {
        int l=addr/NUM_LINES;
        int off=addr%SIZE_LINE;
        return cache.lines[l].bytes[off].val;
    }
    
    bool check_cache(block cache,int addr)
    {
        int b=addr-(addr%SIZE_LINE);
        for (int i = 0; i < NUM_LINES; ++i)
        {
            if(cache.lines[i].base==b)
                return true;
        }
        return false;
    }
};

int main(int argc, char const *argv[])
{
    RAM_Module r1(NUM_BLOCKS,SIZE_BLOCK);
    r1.init();
    r1.populate();
    CPU_unit cpu1(r1);
    std::string files[NUM_CORES]={
        "..//inputs//input_0.txt",
        "..//inputs//input_1.txt",
        "..//inputs//input_2.txt",
        "..//inputs//input_3.txt"
    };
    omp_set_num_threads(NUM_CORES);
    cpu1.auto_assign(files,NUM_CORES);
    return 0;
}