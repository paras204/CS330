
#include <stdlib.h>
#include <sys/mman.h>

#define ALIGNMENT_PADDING 8
#define CHUNK_SIZE (4 * 1024 * 1024) // 4MB 
#define min_chunk 24

static void* memory_head = NULL;

void *memalloc( long size) {
    if (size == 0) {
        return NULL;
    }
    void *current = memory_head;
    void *previous = NULL;
    
    while (current != NULL) {
        previous = current;
         long required_size = ((size + 8 + 7) / 8) * 8;
         long block_size = *(( long*)current);
        
       
        if (block_size >= required_size) {
             long available = block_size;
            void *block_address = current;
             long remaining = available - required_size;
            
            if (remaining >= min_chunk) {
                 long *temp = ( long *)block_address;
                *temp = required_size;
                void *free_address = block_address + required_size;
                void* prev_node = (void*)*( long *)(block_address + 16);
                
                if (prev_node == NULL) {
                    memory_head = free_address;
                } else {
                    *((long *)(prev_node + 8)) = (long)free_address;
                    *((long *)(free_address + 16)) = (long)prev_node;
                }
                
                void *next_node = (void*)*(long *)(block_address + 8);
                *((long *)(free_address + 8)) = ( long)next_node;
                long *block_value = (long *)free_address;
                *block_value = remaining;
            } else {
                
                void* prev_node = (void*)*(long *)(block_address + 16);
                void* next_node = (void*)*(long *)(block_address + 8);
                
                if (prev_node == NULL) {
                    memory_head = next_node;
                } else {
                    *((long *)(prev_node + 8)) = ( long)next_node;
                }
                
                if (next_node != NULL) {
                    *((long *)(next_node + 16)) = ( long)prev_node;
                }
            }
            
            return (current + 8);
        } else {
            void *next_address = (void*)*(long *)(current + 8);
            current = next_address;
        }
    }
    
    long required_allocation = ((size + 8 + CHUNK_SIZE - 1) / CHUNK_SIZE) * CHUNK_SIZE;
   
   
    void *free_memory = mmap(NULL, required_allocation, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    
    if (free_memory == MAP_FAILED) {
        return NULL;
    }
    
    long *temp = (long *)free_memory;
    *temp = required_allocation;
    
    if (previous == NULL) {
        memory_head = free_memory;
    } else {
        *((long *)(previous + 8)) = (long)free_memory;
        *((long *)(free_memory + 16)) = ( long)previous;
    }
    
    long required_size = ((size + 8 + 7) / 8) * 8;
    if(required_size <min_chunk){
        required_size = min_chunk;
    }
    long available = required_allocation;
    void *block_address = free_memory;
    long remaining = available - required_size;
    
    if (remaining >= min_chunk) {
        long *temp = (long *)block_address;
        *temp = required_size;
        void *free_address = block_address + required_size;
        void* prev_node = (void*)*(long *)(block_address + 16);
        
        if (prev_node == NULL) {
            memory_head = free_address;
        } else {
            *((long *)(prev_node + 8)) = (long)free_address;
            *((long *)(free_address + 16)) = (long)prev_node;
        }
        
        void *next_node = (void*)*( long *)(block_address + 8);
        *((long *)(free_address + 8)) = ( long)next_node;
        *(long *)free_address = remaining;
    } else {
        long *temp = (long *)block_address;
        *temp = available;
        void* prev_node = (void*)*(long *)(block_address + 16);
        void* next_node = (void*)*(long *)(block_address + 8);
        
        if (prev_node == NULL) {
            memory_head = next_node;
        } else {
            *((long *)(prev_node + 8)) = (long)next_node;
        }
        
        if (next_node != NULL) {
            *((long *)(next_node + 16)) = (long)prev_node;
        }
    }
    
    return (free_memory + 8);
    printf("Allocating %lu bytes from %p (mmap)\n", size, free_memory);
    return NULL;
}

int memfree (void *ptr)
{
    if(ptr == NULL){
        return -1;
    }
    ptr = ptr - 8;
    long int size = *(long int *)ptr;
    void * right = ptr + size;
    void* temp = memory_head;
    while(temp != NULL){
        long int  value = *(long int *)temp;
        void * t1 = temp + value;
        int flag = 0;
        if(t1 == ptr){
            long long total = value + *(long int *)ptr;
            ptr = temp;
            *(long int *)ptr = total;
            void * prevnode = (void*)* (long int *)(temp+16);
            void * nextnode = (void*)* (long int *)(temp+8);
            if(prevnode == NULL){
                memory_head = nextnode;
            }
            else{
                *((long int *)(prevnode+8)) = (long)nextnode;}
                if(nextnode != NULL) {
                    *((long int *)(nextnode+16)) = (long)prevnode;}
            flag = 1;
            temp = nextnode;
        }
        if(right == temp){
            *(long int *)ptr = *(long int *)ptr + *(long int *)temp;
            void * prevnode = (void*)* (long int *)(temp+16);
            void * nextnode = (void*)* (long int *)(temp+8);
           if(prevnode == NULL){
                memory_head = nextnode;
            }
            else{
                *((long int *)(prevnode+8)) = (long)nextnode;}
                if(nextnode != NULL) {
                    *((long int *)(nextnode+16)) = (long)prevnode;}
                    temp = nextnode;
                    flag = 1;
        }
        if(flag){
            continue;
        }
        void* nextadd = (void*)* (long int *)(temp+8);
        temp = nextadd;
    }
    *(long int *)(ptr + 8)= (long)memory_head;
    *(long int *)(ptr + 16)= 0;
    memory_head = ptr;
    printf("Freeing %lu bytes from %p\n", size, ptr);
    return 0;
}