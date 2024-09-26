#include<context.h>
#include<memory.h>
#include<lib.h>
#include<entry.h>
#include<file.h>
#include<tracer.h>

#define TRACE_BUFFER_MAX_SIZE 4096
///////////////////////////////////////////////////////////////////////////
//// 		Start of Trace buffer functionality 		      /////
///////////////////////////////////////////////////////////////////////////

int is_valid_mem_range(unsigned long buff, u32 count, int access_bit) 
{
     // Get the current execution context
    struct exec_context *current = get_current_ctx();
    
    // Check if the current context is valid
    if (!current) {
        return -EINVAL; // Invalid context
    }

    // Define the valid memory segments and their access permissions
    struct {
        unsigned long start;
        unsigned long end;
        int read_access;
        int write_access;
    } valid_segments[] = {
        {current->mms[MM_SEG_CODE].start, current->mms[MM_SEG_CODE].next_free - 1, 1, 0},     // MM_SEG_CODE
        {current->mms[MM_SEG_RODATA].start, current->mms[MM_SEG_RODATA].next_free - 1, 1, 0}, // MM_SEG_RODATA
        {current->mms[MM_SEG_DATA].start, current->mms[MM_SEG_DATA].next_free - 1, 1, 1},     // MM_SEG_DATA
        {current->mms[MM_SEG_STACK].start, current->mms[MM_SEG_STACK].end - 1, 1, 1}         // MM_SEG_STACK
    };

    // Check buffer validity based on the specified memory segments
    for (int i = 0; i < sizeof(valid_segments) / sizeof(valid_segments[0]); ++i) {
        if (buff >= valid_segments[i].start && buff + count - 1 <= valid_segments[i].end) {
            // The buffer lies within a valid memory segment
            if ((access_bit & 1) && valid_segments[i].read_access) {
                // Check read access permission
                return 0;
            }
            if ((access_bit & 2) && valid_segments[i].write_access) {
                // Check write access permission
                return 0;
            }
        }
    }

    return -EBADMEM;
}



long trace_buffer_close(struct file *filep)
{
 if (filep->type != TRACE_BUFFER) {
        return -EINVAL; // Invalid file type
    }

    struct trace_buffer_info *trace_buffer = filep->trace_buffer;
    if (!trace_buffer) {
        return -EINVAL; // Trace buffer not properly initialized
    }


    // Deallocate the memory used for the trace buffer
    os_page_free(USER_REG, trace_buffer->buffer);

    // Deallocate the trace buffer structure itself
    os_free(trace_buffer, sizeof(struct trace_buffer_info));

    // Deallocate the file operations structure
    os_free(filep->fops, sizeof(struct fileops));

    // Deallocate the file structure
    os_free(filep, sizeof(struct file));

    // Clear the file descriptor entry in the process's context
    struct exec_context *current = get_current_ctx();
    current->files[filep->type] = NULL;

    return 0;
}



int trace_buffer_read(struct file *filep, char *buff, u32 count)
{
if (filep->type != TRACE_BUFFER || filep->mode == O_WRITE) {
     //   printk("Failed here! in read 1\n");
        return -EINVAL; // Invalid file type or mode

    }

    struct trace_buffer_info *trace_buffer = filep->trace_buffer;
    if (!trace_buffer) {
       // printk("trace_buffer is not properly initialized\n");
        return -EINVAL; // Trace buffer not properly initialized
    }
    if(is_valid_mem_range((unsigned long)buff, count, 2 ) != 0){
        //printk("invalid memory range\n");
            return -EBADMEM;
    }
    u32 write_offset = trace_buffer->write_offset;
    u32 read_offset = trace_buffer->read_offset;
    u32 size = TRACE_BUFFER_MAX_SIZE; // Assuming defined in tracer.h
    //printk("write_offset = %d\n", write_offset);
    //printk("read_offset = %d\n", read_offset);
    // Calculate the number of available bytes to read
    u32 available_bytes;
    if (write_offset >= read_offset && trace_buffer->is_full == 0) {
        available_bytes = write_offset - read_offset;
    } else {
        available_bytes = size - read_offset + write_offset;
    }

    if (available_bytes == 0) {
        //printk("here!");
        return 0; // Trace buffer is empty
    }

    u32 bytes_to_read = count;
    if (bytes_to_read > available_bytes) {
        bytes_to_read = available_bytes;
    }
    if(bytes_to_read == 0) {
        //printk("bytes_to_read in read are zeero here!");
        return 0;
    }
    // Read data from the trace buffer and copy it to the user buffer
    u32 i;
    for (i = 0; i < bytes_to_read; i++) {
        buff[i] = trace_buffer->buffer[read_offset];
        read_offset = (read_offset + 1) % size;
    }

    // Update the read offset
    trace_buffer->read_offset = read_offset;
    //printk("read_offset in read call = %d\n", read_offset);
    return bytes_to_read;
}


int trace_buffer_write(struct file *filep, char *buff, u32 count)
{
    if (filep->type != TRACE_BUFFER || filep->mode == O_READ) {
        return -EINVAL; // Invalid file type or mode
    }

    struct trace_buffer_info *trace_buffer = filep->trace_buffer;
    if (!trace_buffer) {
        return -EINVAL; // Trace buffer not properly initialized
    }
    if(is_valid_mem_range((unsigned long)buff, count,1 ) != 0){
            return -EBADMEM;
    }
    u32 write_offset = trace_buffer->write_offset;
    u32 read_offset = trace_buffer->read_offset;
    u32 size = TRACE_BUFFER_MAX_SIZE; 

    // Calculate the available space in the trace buffer
    u32 available_space;
    if (read_offset <= write_offset && trace_buffer->is_full == 0) {
        available_space = size - write_offset + read_offset ;
    } else {
        available_space = read_offset - write_offset ;
    }

    if (available_space == 0) {
        return 0; // Trace buffer is full
    }

    u32 bytes_to_write = count;
    if (bytes_to_write > available_space) {
        bytes_to_write = available_space;
    }
   // printk("bytes_to_write = %d\n", bytes_to_write);
    if(bytes_to_write == 0) {
        return 0;
    }
    // Write data from the user buffer to the trace buffer
    u32 i;
    for (i = 0; i < bytes_to_write; i++) {
        trace_buffer->buffer[write_offset] = buff[i];
        write_offset = (write_offset + 1) % size;
    }

    // Update the write offset
    trace_buffer->write_offset = write_offset;
    if(write_offset == read_offset && bytes_to_write != 0){
    trace_buffer-> is_full = 1;}
   // printk("write_offset in write call = %d\n", write_offset);
    return bytes_to_write;
}

int sys_create_trace_buffer(struct exec_context *current, int mode)
{	
	 if (mode != O_READ && mode != O_WRITE && mode != O_RDWR) {
        // Invalid mode, return EINVAL
        return -EINVAL;
    }

    // Check if the current process has any free file descriptors
    int fd;
    for (fd = 0; fd < MAX_OPEN_FILES; fd++) {
        if (!current->files[fd]) {
            break;
        }
    }

    if (fd == MAX_OPEN_FILES) {
        // No free file descriptors available, return EINVAL
        return -EINVAL;
    }
	struct file *filep;
    struct fileops *fops;
    struct trace_buffer_info *trace_buffer;

	filep = (struct file*)os_alloc(sizeof(struct file));

    if (!filep) {
        return -ENOMEM; // Memory allocation error
    }
	 // Initialize file structure
    filep->type = TRACE_BUFFER;
    filep->mode = mode;
    filep->offp = 0;
    filep->ref_count = 1; // Initial reference count
    filep->inode = NULL; // No associated inode for trace buffer
    filep->fops = (struct fileops*)os_alloc(sizeof(struct fileops));
    if (!filep->fops) {
        os_free(filep, sizeof(struct file));
        return -ENOMEM; // Memory allocation error
    }
	trace_buffer = (struct trace_buffer_info*)os_alloc(sizeof(struct trace_buffer_info));
    if (!trace_buffer) {
        os_free(filep->fops, sizeof(struct fileops));
        os_free(filep, sizeof(struct file));
        return -ENOMEM; // Memory allocation error
    }
	trace_buffer -> buffer = (char *)os_page_alloc(USER_REG);
	 if (!trace_buffer->buffer) {
        os_free(trace_buffer, sizeof(struct trace_buffer_info));
        os_free(filep->fops, sizeof(struct fileops));
        os_free(filep, sizeof(struct file));
        return -ENOMEM; // Memory allocation error
    }
	fops = filep->fops;
    fops->read = trace_buffer_read; // Implement trace_buffer_read function
    fops->write = trace_buffer_write; // Implement trace_buffer_write function
    fops->lseek = NULL; // lseek not supported for trace buffer
    fops->close = trace_buffer_close; // Implement trace_buffer_close function

    // Update file structure with trace buffer information
    filep->trace_buffer = trace_buffer;

    // Assign file structure to file descriptor
    current->files[fd] = filep;

    // Return allocated file descriptor number on success
    return fd;

}

///////////////////////////////////////////////////////////////////////////
//// 		Start of strace functionality 		      	      /////
///////////////////////////////////////////////////////////////////////////
int syscall_argument_count(int syscall_number) {
    switch (syscall_number) {
        case SYSCALL_EXIT:
            return 1;  // Only one argument: exit code
        case SYSCALL_GETPID:
            return 0;  // No arguments
        case SYSCALL_EXPAND:
            return 2;  // Two arguments: size, flags
        case SYSCALL_SHRINK:
            return 2;  // Two arguments: size, flags
        case SYSCALL_ALARM:
            return 1;  // One argument: ticks
        case SYSCALL_SLEEP:
            return 1;  // One argument: ticks
        case SYSCALL_SIGNAL:
            return 2;  // Two arguments: num, handler
        case SYSCALL_CLONE:
            return 2;  // Two arguments: func, stack_addr
        case SYSCALL_FORK:
            return 0;  // No arguments
        case SYSCALL_STATS:
            return 0;  // No arguments
        case SYSCALL_CONFIGURE:
            return 1;  // One argument: new_config
        case SYSCALL_PHYS_INFO:
            return 0;  // No arguments
        case SYSCALL_DUMP_PTT:
            return 1;  // One argument: address
        case SYSCALL_CFORK:
            return 0;  // No arguments
        case SYSCALL_MMAP:
            return 4;  // Four arguments: addr, length, prot, flags
        case SYSCALL_MUNMAP:
            return 2;  // Two arguments: addr, length
        case SYSCALL_MPROTECT:
            return 3;  // Three arguments: addr, length, prot
        case SYSCALL_PMAP:
            return 1;  // One argument: details
        case SYSCALL_VFORK:
            return 0;  // No arguments
        case SYSCALL_GET_USER_P:
            return 0;  // No arguments
        case SYSCALL_GET_COW_F:
            return 0;  // No arguments
        case SYSCALL_OPEN:
            return 2;  // Two arguments: filename, mode
        case SYSCALL_READ:
            return 3;  // Three arguments: fd, buf, count
        case SYSCALL_WRITE:
            return 3;  // Three arguments: fd, buf, count
        case SYSCALL_DUP:
            return 1;  // One argument: oldfd
        case SYSCALL_DUP2:
            return 2;  // Two arguments: oldfd, newfd
        case SYSCALL_CLOSE:
            return 1;  // One argument: fd
        case SYSCALL_LSEEK:
            return 3;  // Three arguments: fd, offset, whence
        case SYSCALL_FTRACE:
            return 4;  // Four arguments: func_addr, action, nargs, fd_trace_buffer
        case SYSCALL_TRACE_BUFFER:
            return 0;  // No arguments
        case SYSCALL_READ_STRACE:
            return 3;  // Three arguments: fd, buff, count
        case SYSCALL_STRACE:
            return 2;  // Two arguments: syscall_num, action
        case SYSCALL_READ_FTRACE:
            return 3;  // Three arguments: fd, buff, count
        case SYSCALL_GETPPID:
            return 0;  // No arguments

        default:
            return -1;  // Invalid syscall number
    }
    }
int strace_write(struct file *filep, char *buff, u32 count)
{   

    if (filep->type != TRACE_BUFFER || filep->mode == O_READ) {
        return -EINVAL; // Invalid file type or mode
    }

    struct trace_buffer_info *trace_buffer = filep->trace_buffer;
    if (!trace_buffer) {
        return -EINVAL; // Trace buffer not properly initialized
    }
    u32 write_offset = trace_buffer->write_offset;
    u32 read_offset = trace_buffer->read_offset;
    u32 size = TRACE_BUFFER_MAX_SIZE; 

    // Calculate the available space in the trace buffer
    u32 available_space;
    if (read_offset <= write_offset && trace_buffer->is_full == 0) {
        available_space = size - write_offset + read_offset ;
    } else {
        available_space = read_offset - write_offset ;
    }

    if (available_space == 0) {
        return 0; // Trace buffer is full
    }

    u32 bytes_to_write = count;
    if (bytes_to_write > available_space) {
        bytes_to_write = available_space;
    }
   // printk("bytes_to_write in strace_write = %d\n", bytes_to_write);

   if(bytes_to_write == 0) {
        return 0;
    }
    // Write data from the user buffer to the trace buffer
    u32 i;
    for (i = 0; i < bytes_to_write; i++) {
        trace_buffer->buffer[write_offset] = buff[i];
        write_offset = (write_offset + 1) % size;
    }

    // Update the write offset
    trace_buffer->write_offset = write_offset;
    if(write_offset == read_offset && bytes_to_write != 0) {
    trace_buffer-> is_full = 1;}
    //printk("write_offset in strace_write call = %d\n", write_offset);
    return bytes_to_write;
}

int perform_tracing(u64 syscall_num, u64 param1, u64 param2, u64 param3, u64 param4)
{   //printk("in perform_tracing enter\n");
    if(syscall_num == 37 || syscall_num == 38 || syscall_num == 1){
       // printk("syscall_num_not_track = %d\n", syscall_num);
        return 0;
    }

    struct exec_context *current = get_current_ctx();
    if ( current->st_md_base->is_traced == 0) {
        //printk("in perform_tracing is_traced = 0\n");
        return 0;  
    }
    struct strace_head *st_head = current->st_md_base;
    struct strace_info *cur = st_head->next;
    struct strace_info *last = st_head->last;
    //printk("in perform_tracing st_head->count = %d , tracing_mode=%d\n", st_head->count , st_head->tracing_mode) ;
    if(st_head -> tracing_mode == FILTERED_TRACING){
        while (cur!=NULL) {
            if (cur->syscall_num == syscall_num) {
                //printk("in perform_tracing syscall_num = %d\n", syscall_num);
                break;  // The syscall is being traced
            }
            cur = cur->next;
        }
        if (cur->syscall_num != syscall_num) {
           // printk("in perform_tracing syscall_num not = %d\n", syscall_num);
            return 0;  // The syscall is not being traced
        }
    }
    
    int num_arg = syscall_argument_count(syscall_num);
    if (num_arg == -1) {
        return 0;  // Error: Invalid syscall number
    }
    int fd = st_head->strace_fd;
    if(fd <0 ) {
        return 0;
    }
    u64 temp[] = {syscall_num,param1, param2, param3, param4};
    u64 buffer[num_arg+1];
    for(int i = 0; i < num_arg+1; i++){
        buffer[i] = temp[i];
        //printk("in perform_tracing buffer[%d] = %x\n", i, buffer[i]);
    }
    u32 count = 8*(num_arg+1);
    int ret = strace_write(current->files[fd], buffer, count);

    return 0;  // Success
}


int sys_strace(struct exec_context *current, int syscall_num, int action)
{
	if (!current) {
        return -EINVAL; // Invalid context
    }

    if (action != ADD_STRACE && action != REMOVE_STRACE) {
        return -EINVAL; // Invalid action
    }

    if (current->st_md_base == NULL) {
        current->st_md_base = (struct strace_head *)os_alloc(sizeof(struct strace_head));
        if (!current->st_md_base) {
            return -ENOMEM; // Memory allocation failed
        }
        current->st_md_base->count = 0;
        current->st_md_base->is_traced = 0;
        current->st_md_base->next = NULL;
        current->st_md_base->last = NULL;
    }

    struct strace_info *info = (struct strace_info *)os_alloc(sizeof(struct strace_info));
    if (!info) {
        return -ENOMEM; // Memory allocation failed
    }

    info->syscall_num = syscall_num;
    info->next = NULL;

    if (action == ADD_STRACE) {
        // Add the system call to the traced list
        if(current->st_md_base->count == STRACE_MAX){
            return -EINVAL;
        }
        if (current->st_md_base->last) {
            current->st_md_base->last->next = info;
            current->st_md_base->last = info;
        } else {
            current->st_md_base->next = info;
            current->st_md_base->last = info;
        }
        current->st_md_base->count++;
        //printk("added sys_strace: current->st_md_base->count = %d\n", current->st_md_base->count);
    } else {
        // Remove the system call from the traced list
        struct strace_info *prev = NULL;
        struct strace_info *cur = current->st_md_base->next;

        while (cur) {
            if (cur->syscall_num == syscall_num) {
                if (prev) {
                    prev->next = cur->next;
                } else {
                    current->st_md_base->next = cur->next;
                }

                os_free( cur , sizeof(struct strace_info));
               // printk("removed sys_strace: current->st_md_base->count = %d\n", current->st_md_base->count);
                current->st_md_base->count--;
                break;
            }

            prev = cur;
            cur = cur->next;
        }
    }

    return 0;
}

int sys_read_strace(struct file *filep, char *buff, u64 count)
{
	if (!filep || !buff || count <= 0) {
       // printk("Invalid arguments in 1 \n");
        return -EINVAL; // Invalid arguments
    }

    struct exec_context *current = get_current_ctx();
    if (!current) {
       // printk("Invalid arguments in 2 \n");
        return -EINVAL; // Invalid context
    }

    int offset = 0;
    int r = 0;
    for(int i=0 ; i<count ; i++){
        r = trace_buffer_read(filep, buff+offset, 8);
        // if(r == 0){
        //     break;
        // }
        int num_arg = syscall_argument_count(buff[offset]);
        
        if(num_arg == -1){
           // printk("Invalid arguments in 3 \n");
            return offset;
        }
        
        offset += r;
      //  printk("sys: %x\n", buff[offset-8]);
        for(int j=0 ; j<num_arg ; j++){
            offset+= trace_buffer_read(filep, buff+offset, 8);
           // printk("read: %x\n", buff[offset-8]);
        
        }
    }
    return offset;
}

int sys_start_strace(struct exec_context *current, int fd, int tracing_mode)
{   
    
    // Create a new strace_head and set its attributes
    if(current->st_md_base==NULL){
    struct strace_head *head = (struct strace_head *)os_alloc(sizeof(struct strace_head));
    if (!head) {
        return -ENOMEM;  // Error: Out of memory
    }
    head->count = 0;
    head->is_traced = 1;  // Enable system call tracing
    head->strace_fd = fd;
    head->tracing_mode = tracing_mode;
    head->next = NULL;
    head->last = NULL;
    
    // Link the strace_head to the process's exec context
    current->st_md_base = head;
  //  printk("start_strace: head->strace_fd = %d\n", head->strace_fd);
  }
    else{
        current->st_md_base->is_traced = 1;
        current->st_md_base->strace_fd = fd;
        current->st_md_base->tracing_mode = tracing_mode;
    }
	return 0;
}

int sys_end_strace(struct exec_context *current)
{    if (current->st_md_base == NULL) {
        return -EINVAL;  // Error: System call tracing is not enabled
    }

    // Cleanup the meta-data structures related to system call tracing
    while(current -> st_md_base -> next != NULL){
        struct strace_info *temp = current -> st_md_base -> next;
        current -> st_md_base -> next = temp -> next;
        os_free(temp, sizeof(struct strace_info));
    }
    current -> st_md_base -> is_traced = 0;
    current -> st_md_base -> last = NULL;
    current -> st_md_base -> count = 0;
    // Success
	return 0;
}



///////////////////////////////////////////////////////////////////////////
//// 		Start of ftrace functionality 		      	      /////
///////////////////////////////////////////////////////////////////////////


long do_ftrace(struct exec_context *ctx, unsigned long faddr, long action, long nargs, int fd_trace_buffer)
{   
   // printk("in do_ftrace enter\n");
    if(action == ADD_FTRACE){
        if(ctx->ft_md_base == NULL){
            ctx->ft_md_base = (struct ftrace_head *)os_alloc(sizeof(struct ftrace_head));
            if (!ctx->ft_md_base) {
                return -EINVAL; // Memory allocation failed
            }
            ctx->ft_md_base->count = 0;
            ctx->ft_md_base->next = NULL;
            ctx->ft_md_base->last = NULL;
        }
        if(ctx->ft_md_base->count == FTRACE_MAX){
            return -EINVAL;
        }
        //look if the function is already added in the list
        struct ftrace_info *cur = ctx->ft_md_base->next;
        while (cur!=NULL) {
            if (cur->faddr == faddr) {
                return -EINVAL;  // The function is being traced
            }
            cur = cur->next;
        }
        // if the function is not already in the traced list add that function in the list
        struct ftrace_info *info = (struct ftrace_info *)os_alloc(sizeof(struct ftrace_info));
        if (!info) {
            return -EINVAL; // Memory allocation failed
        }
        info->faddr = faddr;
        info->num_args = nargs;
        info->next = NULL;
        info->fd = fd_trace_buffer;
        if (!ctx->ft_md_base->next) {
            ctx->ft_md_base->next = info;
        } 
        if(!ctx->ft_md_base->last){
            ctx->ft_md_base->last = info;
        }
        else {
            ctx->ft_md_base->last->next = info;
            ctx->ft_md_base->last = info;
        }
        ctx->ft_md_base->count++;
       // printk("added do_ftrace: ctx->ft_md_base->count = %d\n", ctx->ft_md_base->count);
        return 0;
    }
    if(action == REMOVE_FTRACE){
        if(ctx->ft_md_base == NULL || ctx->ft_md_base->next == NULL){
            return -EINVAL;
        }
        //check if function is already in the list
        struct ftrace_info *cur = ctx->ft_md_base->next;
        while (cur!=NULL) {
            if (cur->faddr == faddr) {
                break;  // The function is being traced
            }
            cur = cur->next;
        }
        if(cur == NULL) {
            return -EINVAL;  // The function is not being traced
        }
        //remove the function from the list
        struct ftrace_info *prev = NULL;
        cur= ctx->ft_md_base->next;
        while (cur!=NULL) {
            if (cur->faddr == faddr) {
                if (prev) {
                    prev->next = cur->next;
                } else {
                    ctx->ft_md_base->next = cur->next;
                }
                os_free( cur , sizeof(struct ftrace_info));
               // printk("removed do_ftrace: ctx->ft_md_base->count = %d\n", ctx->ft_md_base->count);
                ctx->ft_md_base->count--;
                break;
            }
            prev = cur;
            cur = cur->next;
        }
        // struct ftrace_info *prev = NULL;
        // struct ftrace_info *cur = ctx->ft_md_base->next;
        // while (cur) {
        //     if (cur->faddr == faddr) {
        //         if (prev) {
        //             prev->next = cur->next;
        //         } else {
        //             ctx->ft_md_base->next = cur->next;
        //         }
        //         os_free( cur , sizeof(struct ftrace_info));
        //         printk("removed do_ftrace: ctx->ft_md_base->count = %d\n", ctx->ft_md_base->count);
        //         ctx->ft_md_base->count--;
        //         break;
        //     }
        //     prev = cur;
        //     cur = cur->next;
        // }
        // if(cur== NULL) {
        //     return -EINVAL;
        // }

       // printk("removed do_ftrace: ctx->ft_md_base->count = %d\n", ctx->ft_md_base->count);
        return 0;
    }
    if(action == ENABLE_FTRACE){
        
        //This action starts tracing of an existing (already added) function. After this call, whenever this function is called, its information (function address and values of arguments passed to it) should be stored in a trace buffer. To enable the tracing of a function, you have to manipulate the address space of the current process, so that, whenever the first instruction of the traced function gets executed, invalid opcode fault gets triggered. To cause the invalid opcode fault upon execution, you can make use of INV OPCODE defined in gemOS/src/include/tracer.h. The function on which tracing is being enabled, should have already been added into the list of functions to be traced (using ftrace(func addr, ADD FTRACE, num args, fd)). If ftrace tries to enable tracing on a function not yet added to the list of functions to trace, return -EINVAL. On success, return 0. In case of any other error, return -EINVAL.
        if(ctx->ft_md_base == NULL || ctx->ft_md_base->next == NULL){
            return -EINVAL;
        }
        struct ftrace_info *cur = ctx->ft_md_base->next;
        while (cur!=NULL) {
            if (cur->faddr == faddr) {
                break;  // The function is being traced
            }
            cur = cur->next;
        }
        if (cur->faddr != faddr) {
            return -EINVAL;  // The function is not being traced
        }
        //Manipulate address space of the current process
        // so that whenever the first instruction of the traced function gets executed, invalid opcode fault gets triggered
        u8 *addr = (u8 *)faddr;
        if(addr[0] == INV_OPCODE){
            return 0;
        }
        //change the 32 bits of the instruction to INV_OPCODE and write that data into code_bakup
        for(int i=0 ; i<4 ; i++){
            cur->code_backup[i] = addr[i];
            addr[i] = INV_OPCODE;
        }
        return 0;

    }
    if(action == DISABLE_FTRACE){
        
        //To disable the tracing of a function, you have to manipulate the address space of the current process such that the function execution takes place without any invalid-opcode faults. 
        //If ftrace tries to disable tracing on a function not yet added to the list of functions to trace, return -EINVAL
        if(ctx->ft_md_base == NULL || ctx->ft_md_base->next == NULL){
            return -EINVAL;
        }
        struct ftrace_info *cur = ctx->ft_md_base->next;
        while (cur!=NULL) {
            if (cur->faddr == faddr) {
                break;  // The function is being traced
            }
            cur = cur->next;
        }
        if (cur->faddr != faddr) {
            return -EINVAL;  // The function is not being traced
        }
        //Manipulate address space of the current process
        // so that whenever the first instruction of the traced function gets executed, invalid opcode fault gets triggered
        u8 *addr = (u8 *)faddr;
        if(addr[0] != INV_OPCODE){
            return 0;
        }
        //change the 32 bits of the instruction to INV_OPCODE and write that data into code_bakup
        for(int i=0 ; i<4 ; i++){
            addr[i] = cur->code_backup[i];
        }
        return 0;
    }
    if(action == ENABLE_BACKTRACE){
        

        //ENABLE BACKTRACE: This action conveys the OS to capture the call back-trace of the function (whose address is passed to the ftrace system call along with this action). Note that, the call back-trace of the function should be captured along with the normal function call trace information i.e., function address and arguments. Back-trace should report the return addresses pushed on to the stack as one function calls another function. All the return addresses (starting from the function being traced) till the main function should be filled in the trace buffer. Example: Assume main() calls func1 and func1 calls func2. Suppose tracing (with back trace) is enabled for func2. When func2 is called, as part of the call back-trace, address of the first instruction of func2, return address in func1 (saved in the stack when func2 was called) and the return address in main (saved in stack when func1 was called) should be stored in the trace buffer. Note that, the return address of main in the stack frame is END ADDR (defined in gemOS/src/include/tracer.h). When this address is encountered, the backtracing should stop. Moreover, the END ADDR should not be stored in the back-trace.
        if(ctx->ft_md_base == NULL || ctx->ft_md_base->next == NULL){
            return -EINVAL;
        }
        struct ftrace_info *cur = ctx->ft_md_base->next;
        while (cur!=NULL) {
            if (cur->faddr == faddr) {
                break;  // The function is being traced
            }
            cur = cur->next;
        }
        if (cur->faddr != faddr) {
            return -EINVAL;  // The function is not being traced
        }
        cur->capture_backtrace = 1;
         u8 *addr = (u8 *)faddr; 
        if(addr[0] == INV_OPCODE){
            return 0;
        }
        //change the 32 bits of the instruction to INV_OPCODE and write that data into code_bakup
        for(int i=0 ; i<4 ; i++){
            cur->code_backup[i] = addr[i];
            addr[i] = INV_OPCODE;
        }
        return 0;

    }
    if(action == DISABLE_BACKTRACE){
        if(ctx->ft_md_base == NULL || ctx->ft_md_base->next == NULL){
            return -EINVAL;
        }
        struct ftrace_info *cur = ctx->ft_md_base->next;
        while (cur!=NULL) {
            if (cur->faddr == faddr) {
                break;  // The function is being traced
            }
            cur = cur->next;
        }
        if (cur->faddr != faddr) {
            return -EINVAL;  // The function is not being traced
        }
        cur->capture_backtrace = 0;
        u8 *addr = (u8 *)faddr;
        //change the 32 bits of the instruction to INV_OPCODE and write that data into code_bakup
        for(int i=0 ; i<4 ; i++){
            addr[i] = cur->code_backup[i];
        }
        return 0;
    }
    return -EINVAL;
    
}

//Fault handler
long handle_ftrace_fault(struct user_regs *regs)
{   
    //printk("in handle_ftrace_fault enter\n");
    struct exec_context *current = get_current_ctx();
    if(current->ft_md_base == NULL || current->ft_md_base->next == NULL){
        return -EINVAL;}
    struct ftrace_info *cur = current->ft_md_base->next;
    // check the function being traced is in the list using entry in regs
    u64 addr = regs->entry_rip;
    if(cur==NULL){
    //printk("cur in nnull here!\n");
    }
    while (cur!=NULL) { 
        if (cur->faddr == (addr)) {
            break;  // The function is being traced
        }
        cur = cur->next;
    }
    if(cur == NULL) {
      //  printk("cur is null\n");
        return -EINVAL;
    }
    regs -> entry_rsp -= 8;
    *((u64*)regs->entry_rsp) = regs->rbp;
    regs->rbp = regs->entry_rsp; 
    regs -> entry_rip = regs -> entry_rip + 4;
    int num_arg = cur->num_args;
    u64 temp[num_arg+1];
    u64 temp2[] = { cur->faddr, regs->rdi, regs->rsi, regs->rdx, regs->rcx, regs->r8, regs->r9};
    for(int i = 0; i < num_arg+1; i++){
        temp[i] = temp2[i];
    }
    int fd = cur->fd;
    if(fd<0) {
        return -EINVAL;
    }
    u32 count = 8*(num_arg+1);
    
     int ret = strace_write(current->files[fd], temp, count);
    u64 flag[1];
    flag[0] = 0xFFFFFFFFFFFFFFFF;
    //if capture_backtrace == 0 push a flag  0xFFFFFFFFFFFFFFFF into buffer
    if(cur->capture_backtrace == 0){
        int ret = strace_write(current->files[fd], (char*)flag , 8);
        return 0;
    }
    //if capture_backtrace == 1 push a flag  0xFFFFFFFFFFFFFF69 into buffer and then push the return address of the function into buffer
    u64 sp = regs->entry_rsp;
    u64 rbp = regs->rbp;
    flag[0] = cur -> faddr;
    if(cur->capture_backtrace == 1){
       int x= strace_write(current->files[fd],(char*) flag , 8);
        while(*(u64*)(rbp+8) != END_ADDR){
            flag[0] = *(u64*)(rbp+8);
            int x = strace_write(current->files[fd], (char*) flag , 8);
            rbp = *((u64*)rbp);
        }
        flag[0] = 0xFFFFFFFFFFFFFFFF;
         x = strace_write(current->files[fd], (char*)flag , 8);
    }

    
        return 0;
}


int sys_read_ftrace(struct file *filep, char *buff, u64 count)
{   
    if (!filep || !buff || count <= 0) {
        //printk("Invalid arguments in 1 \n");
        return -EINVAL; // Invalid arguments
    }

    struct exec_context *current = get_current_ctx();
    if (!current) {
       // printk("Invalid arguments in 2 \n");
        return -EINVAL; // Invalid context
    }

    int offset = 0;
    int r = 0;
    
    for(int i=0 ; i< count ; i++){
        r = trace_buffer_read(filep, buff+offset, 8);
        if(r == 0){
            return offset;
        }
        u64 temp = ((u64 *)buff)[offset];
        offset += r;
       // printk("sys: %x\n", buff[offset-8]);
        while(temp != 0xFFFFFFFFFFFFFFFF){
            
            r = trace_buffer_read(filep, buff+offset, 8);
            if(r==0) {
               // printk("r beacame zero!");return offset ;
               }
                if(r<8) return -EINVAL;
            offset+=8;
            temp = buff[offset - 8];
           // printk("read: %x offset %d\n", *(unsigned long *)(buff + offset-8), offset);
        }
       // printk("OUT of loop\n");
        offset-=8;
        buff[offset] = 0;
    }
    return offset;
}
