#include <types.h>
#include <mmap.h>
#include <fork.h>
#include <v2p.h>
#include <page.h>

/* 
 * You may define macros and other helper functions here

 * You must not declare and use any static/global variables 
 * */
 #define PToffset       0x1FF             /* (1 << 9) - 1 : PT offset size */
#define L4offset       0x027             /* L4 Offset : 39-48 bits */
#define L3offset       0x01E             /* L3 Offset : 30-39 bits */
#define L2offset       0x015             /* L2 Offset : 21-30 bits */
#define L1offset       0x00C             /* L1 Offset : 12-21 bits */
#define PTEoffset      0x00C             /* PFN is at an offset of 12 in PTE */

#define pr             0x001             /* Present Bit */
#define rw             0x008             /* Read/Write Bit */
#define us             0x010             /* User Mode */
#define PGD_MASK 0xff8000000000UL 
#define PUD_MASK 0x7fc0000000UL
#define PMD_MASK 0x3fe00000UL
#define PTE_MASK 0x1ff000UL

#define PGD_SHIFT 39
#define PUD_SHIFT 30
#define PMD_SHIFT 21
#define PTE_SHIFT 12
#define PAGE_SHIFT 12
# define mask 0x1ff
void FLUSH()
{
    asm volatile(
        "mov %%cr3, %%rax;"
        "mov %%rax, %%cr3;"
        :
        :
        : "%rax");
}
void install_page_table(struct exec_context *src_ctx, struct exec_context *dest_ctx, u64 addr){
    u64 *src_pgd_base = (u64 *)osmap(src_ctx->pgd);
    u64 src_offset_into_pgd = (addr >> 39) & mask;
    u64 *src_offset_loc_in_pgd = src_pgd_base + src_offset_into_pgd;

    if ((*src_offset_loc_in_pgd & 0x1) == 0)
    {
        return;
    }

    u64 *src_pud_base = (u64 *)osmap(*src_offset_loc_in_pgd >> 12);
    u64 src_offset_into_pud = (addr >> 30) & mask;
    u64 *src_offset_loc_in_pud = src_pud_base + src_offset_into_pud;

    if ((*src_offset_loc_in_pud & 0x1) == 0)
    {
        return;
    }

    u64 *src_pmd_base = (u64 *)osmap(*src_offset_loc_in_pud >> 12);
    u64 src_offset_into_pmd = (addr >> 21) & mask;
    u64 *src_offset_loc_in_pmd = src_pmd_base + src_offset_into_pmd;

    if ((*src_offset_loc_in_pmd & 0x1) == 0)
    {
        return;
    }

    u64 *src_pte_base = (u64 *)osmap(*src_offset_loc_in_pmd >> 12);
    u64 src_offset_into_pte = (addr >>12) & mask;
    u64 *src_offset_loc_in_pte = src_pte_base + src_offset_into_pte;
    if ((*src_offset_loc_in_pte & 0x1) == 0)
    {
        return;
    }

    

    u64 *dest_pgd_base = (u64 *)osmap(dest_ctx->pgd);
    u64 dest_offset_into_pgd = (addr >> 39) & mask;
    u64 *dest_offset_loc_in_pgd = dest_pgd_base + dest_offset_into_pgd;

    if ((*dest_offset_loc_in_pgd & 0x1) == 0)
    {
        u64 pfn = os_pfn_alloc(OS_PT_REG);
        *dest_offset_loc_in_pgd = 0x0;
        *dest_offset_loc_in_pgd = (pfn << 12) | 0x19;
    }

    u64 *dest_pud_base = (u64 *)osmap(*dest_offset_loc_in_pgd >> 12);
    u64 dest_offset_into_pud = (addr >> 30) & mask;
    u64 *dest_offset_loc_in_pud = dest_pud_base + dest_offset_into_pud;

    if ((*dest_offset_loc_in_pud & 0x1) == 0)
    {
        u64 pfn = os_pfn_alloc(OS_PT_REG);
        *dest_offset_loc_in_pud = 0x0;
        *dest_offset_loc_in_pud = (pfn << 12) | 0x19;
    }

    u64 *dest_pmd_base = (u64 *)osmap(*dest_offset_loc_in_pud >> 12);
    u64 dest_offset_into_pmd = (addr >> 21) & mask;
    u64 *dest_offset_loc_in_pmd = dest_pmd_base + dest_offset_into_pmd;

    if ((*dest_offset_loc_in_pmd & 0x1) == 0)
    {
        u64 pfn = os_pfn_alloc(OS_PT_REG);
        *dest_offset_loc_in_pmd = 0x0;
        *dest_offset_loc_in_pmd = (pfn << 12) | 0x19;
    }

    u64 *dest_pte_base = (u64 *)osmap(*dest_offset_loc_in_pmd >> 12);
    u64 dest_offset_into_pte = (addr >>12) & mask;
    u64 *dest_offset_loc_in_pte = dest_pte_base + dest_offset_into_pte;

    *src_offset_loc_in_pte |= 8; 
    *src_offset_loc_in_pte ^= 8;
    *dest_offset_loc_in_pte = *src_offset_loc_in_pte;
    
    get_pfn(*dest_offset_loc_in_pte >> 12);
    
    FLUSH();
}

struct vm_area *alloc_vm_area(unsigned long start, unsigned long end, u32 access_flags) {
    struct vm_area *vma = (struct vm_area *)os_alloc(sizeof(struct vm_area));
    if (vma == NULL) {
        // Allocation failed, return NULL or handle the error as needed.
        return NULL;
    }

    // Initialize the fields of the newly allocated struct vm_area.
    vma->vm_start = start;
    vma->vm_end = end;
    vma->access_flags = access_flags;
    vma->vm_next = NULL;  // This new VMA is not yet linked to the list.
  //  printk("Allocated new VMA: start = %x, end = %x, access_flags = %x\n", start, end, access_flags);
    stats -> num_vm_area++;
    return vma;
}

u64* get_process_pte(struct exec_context *ctx, u64 addr) 
{
    u64 *vaddr_base = (u64 *)osmap(ctx->pgd);
    u64 *entry;
    u32 phy_addr;
    
    entry = vaddr_base + ((addr ) >> PGD_SHIFT);
    phy_addr = (*entry >> PTE_SHIFT) & 0xFFFFFFFF;
    vaddr_base = (u64 *)osmap(phy_addr);
  
    /* Address should be mapped as un-priviledged in PGD*/
    if( (*entry & 0x1) == 0 )
        goto out;

     entry = vaddr_base + ((addr ) >> PUD_SHIFT);
     phy_addr = (*entry >> PTE_SHIFT) & 0xFFFFFFFF;
     vaddr_base = (u64 *)osmap(phy_addr);
    
     /* Address should be mapped as un-priviledged in PUD*/
      if( (*entry & 0x1) == 0 )
          goto out;


      entry = vaddr_base + ((addr ) >> PMD_SHIFT);
      phy_addr = (*entry >> PTE_SHIFT) & 0xFFFFFFFF;
      vaddr_base = (u64 *)osmap(phy_addr);
      
      
      if( (*entry & 0x1) == 0  )
          goto out;
     
      entry = vaddr_base + ((addr ) >> PTE_SHIFT);
      
      if( (*entry & 0x1) == 0 )
          goto out;
      return entry;

out:
      return NULL;
}

/**
 * mprotect System call Implementation.
 */
//  void tlb_flush(){
//     u64 cr3;
//     asm volatile(
//         "mov %%cr3, %0;"
//         :"=r"(cr3)
//         :
//         :"memory"
//     );
//     asm volatile(
//         "mov %0, %%cr3;"
//         :
//         :"r"(cr3)
//         :"memory"
//     );
//  }
long vm_area_mprotect(struct exec_context *current, u64 addr, int length, int prot)
{   
    // check the validity of arguments
    if(prot != PROT_READ && prot != PROT_WRITE && prot != (PROT_READ|PROT_WRITE)){
        return -1;
    }
    if(length<=0) return -1;
    //if length is greater thsn 2 MB return -1
    
    if(addr % PAGE_SIZE != 0){
        return -1;
    }
    if(length % PAGE_SIZE != 0){
        length = length + (PAGE_SIZE - length % PAGE_SIZE);
    }
    
    //if the address is not in vma return -1

    struct vm_area *temp = current->vm_area;
    int temp1 = 0;
    // while(temp != NULL){
    //     if(addr >= temp->vm_start && addr + length <= temp->vm_end){
    //         temp1 = 1;
    //         break;
    //     }
    //     temp = temp->vm_next;
    // }
    // if(!temp1){
    //     return -1;
    // }
    u64 temp_addr = addr;
    int temp_length = length;
    ///////////////////////
     for(  ;temp_addr < addr + length; temp_addr += PAGE_SIZE ){
    int access_flags = temp->access_flags;
    int flag = rw;
    //if prot is write flag=rw else 0
    if(prot == PROT_READ){
        flag = 0;}
     unsigned long *l4addr = osmap(current->pgd);
        unsigned long *l3addr, *l2addr, *l1addr;
        u32 offsetL4, offsetL3, offsetL2, offsetL1;
        u32 pfnL1, pfnL2, pfnL3, dataPFN;
        
        /* Extract out all 4 level page offsets */
        offsetL4 = PToffset & (temp_addr >> L4offset);
        offsetL3 = PToffset & (temp_addr >> L3offset);
        offsetL2 = PToffset & (temp_addr >> L2offset);
        offsetL1 = PToffset & (temp_addr >> L1offset);

        /* Cleanup Page Table entries */
        if (*(l4addr + offsetL4) & 1) {
            //printk("level 1 walk\n");
                pfnL3 = *(l4addr + offsetL4) >> PTEoffset;
                // *(l4addr + offsetL4) = (pfnL3 << PTEoffset) | (pr | rw| us);
                l3addr = osmap(pfnL3);
        }
        else continue;

        if (*(l3addr + offsetL3) & 1) {

            //printk("level 2 walk\n");
                pfnL2 = *(l3addr + offsetL3) >> PTEoffset;
                l2addr = osmap(pfnL2);
                // *(l3addr + offsetL3) = (pfnL2 << PTEoffset) | (pr | rw| us);
        }
        else continue;
        if (*(l2addr + offsetL2) & 1) {
            //printk("level 3 walk\n");
                pfnL1 = *(l2addr + offsetL2) >> PTEoffset;
                l1addr = osmap(pfnL1);
                // *(l2addr + offsetL2) = (pfnL1 << PTEoffset) | (pr | rw| us);
        }
        else continue;
        if (*(l1addr + offsetL1) & 1) {
            //printk("level 4 walk\n");
                dataPFN = *(l1addr + offsetL1) >> PTEoffset;
                //if pfn reference count is greater than 1 then do nothing
                if(get_pfn_refcount(dataPFN) > 1){
                    //printk("reference count is greater than 1 in mprot\n");
                    continue;
                }
                else *(l1addr + offsetL1) = (dataPFN << PTEoffset) | (pr | flag| us);
        }
        else {//printk("no way!\n");
        continue;}
            // tlb_flush();
 asm volatile("invlpg (%0);" 
                :: "r"(addr) 
                : "memory");
                FLUSH();
    //printk("flag = %x\n",flag);
     }

        ///////////////////////
    struct vm_area *vma = current->vm_area;
    struct vm_area *prev = NULL;

    while(vma != NULL){
        unsigned long start = vma->vm_start;
        unsigned long end = vma->vm_end;
        if(vma->access_flags == prot){
            prev = vma;
            vma = vma->vm_next;
            
            continue;
        }
        if(addr >= start && addr + length <= end){
            //if the vma is already protected with the same prot, then return 0
            if(vma->access_flags == prot){
                //printk("Nothing to be done!\n");
                asm volatile("invlpg (%0);" 
                :: "r"(addr) 
                : "memory");
                return 0;
            }
            //if the vma is not protected with the same prot, then split the vma into new vm_area
            else{
                if(addr == vma->vm_start && addr + length == vma->vm_end){
                    vma->access_flags = prot;
                    if(vma->vm_next != NULL && vma->vm_next->access_flags == prot){
                        struct vm_area *temp = vma->vm_next;
                        vma->vm_end = temp->vm_end;
                        vma->vm_next = temp->vm_next;
                        os_free(temp, sizeof(struct vm_area));
                        stats->num_vm_area--;
                    }
                    if(prev!=NULL && prev->access_flags == prot){
                        prev->vm_end = vma->vm_end;
                        prev->vm_next = vma->vm_next;
                        os_free(vma, sizeof(struct vm_area));
                        stats->num_vm_area--;
                    }
                //printk("Completely inclusive\n");
                //  tlb_flush();
                asm volatile("invlpg (%0);" 
                :: "r"(addr) 
                : "memory");
                    return 0;
                }
                if(addr == vma->vm_start){
                    struct vm_area *new_vma = alloc_vm_area(addr + length, vma->vm_end, vma->access_flags);
                    if(new_vma == NULL){
                        return -1;
                    }
                    new_vma->vm_next = vma->vm_next;
                    vma->vm_next = new_vma;
                    vma->vm_end = addr + length;
                    vma->access_flags = prot;
                   // printk("#here\n");
                    if(prev!=NULL){
                        if(prev->access_flags == prot){
                            prev->vm_end = vma->vm_end;
                            prev->vm_next = vma->vm_next;
                            os_free(vma, sizeof(struct vm_area));
                            stats->num_vm_area--;
                        }
                    }
                    asm volatile("invlpg (%0);" 
                :: "r"(addr) 
                : "memory");
                    return 0;
                }
                if(addr + length == vma->vm_end){
                    struct vm_area *new_vma = alloc_vm_area(addr, vma->vm_end, prot);
                    if(new_vma == NULL){
                        return -1;
                    }
                    new_vma->vm_next = vma->vm_next;
                    vma->vm_next = new_vma;
                    vma->vm_end = addr;
                    if(new_vma->vm_next!=NULL){
                        if(new_vma->vm_next->access_flags == prot){
                            struct vm_area *temp = new_vma->vm_next;
                            new_vma->vm_end = temp->vm_end;
                            new_vma->vm_next = temp->vm_next;
                            os_free(temp, sizeof(struct vm_area));
                            stats->num_vm_area--;
                        }
                    }
                    asm volatile("invlpg (%0);" 
                :: "r"(addr) 
                : "memory");
                    return 0;
                }
                // split the vm_area into three vm_areas 
                struct vm_area *new_vma2 = alloc_vm_area(addr, addr + length, prot);
                if(new_vma2 == NULL){
                    return -1;
                }
                unsigned long temp = vma->vm_end;
                new_vma2->vm_next = vma->vm_next;
                vma->vm_next = new_vma2;
                vma->vm_end = addr;
                struct vm_area *new_vma3 = alloc_vm_area(addr + length, temp, vma->access_flags);
                if(new_vma3 == NULL){
                    return -1;
                }
                new_vma3->vm_next = new_vma2->vm_next;
                new_vma2->vm_next = new_vma3;
                
            }
            
        }
      else  if(addr<= start  && addr+length >= end){
        vma->access_flags = prot;
        if(vma->vm_next != NULL && vma->vm_next->access_flags == prot){
            struct vm_area *temp = vma->vm_next;
            vma->vm_end = temp->vm_end;
            vma->vm_next = temp->vm_next;
            os_free(temp, sizeof(struct vm_area));
            stats->num_vm_area--;
        }
        if(prev!=NULL){
            if(prev->access_flags == prot){
                prev->vm_end = vma->vm_end;
                prev->vm_next = vma->vm_next;
                os_free(vma, sizeof(struct vm_area));
                stats->num_vm_area--;
            }
        }
        }
        else if(addr > start && addr <end && addr + length >end){
            vma->vm_end = addr;
            struct vm_area* new_vma = alloc_vm_area(addr, end, prot);
            if(new_vma == NULL){
                return -1;
            }
            new_vma->vm_next = vma->vm_next;
            vma->vm_next = new_vma;
            if(new_vma->vm_next!=NULL){
                if(new_vma->vm_next->access_flags == prot){
                    struct vm_area *temp = new_vma->vm_next;
                    new_vma->vm_end = temp->vm_end;
                    new_vma->vm_next = temp->vm_next;
                    os_free(temp, sizeof(struct vm_area));
                    stats->num_vm_area--;
                }
            }
        }
        else if(addr <start && addr+length>start && addr+length <end){
            vma->vm_start = addr + length;
            struct vm_area* new_vma = alloc_vm_area(start, addr  + length, prot);
            if(new_vma == NULL){
                return -1;
            }
            new_vma->vm_next = vma;
            if(prev!=NULL){
                prev->vm_next = new_vma;
                if(prev->access_flags == prot){
                    prev->vm_end = new_vma->vm_end;
                    prev->vm_next = new_vma->vm_next;
                    os_free(new_vma, sizeof(struct vm_area));
                    stats->num_vm_area--;
                }
            }
            
        }
        prev = vma;
        vma = vma->vm_next;
        
    }
    asm volatile("invlpg (%0);" 
                :: "r"(addr) 
                : "memory");
    return 0;
    
            
            
}

/**
 * mmap system call implementation.
 */
long vm_area_map(struct exec_context *current, u64 addr, int length, int prot, int flags)
{           
   if(length<=0 || length > 2*1024*1024){
    return -1;
   }
   if(prot != PROT_READ && prot != PROT_WRITE && prot != (PROT_READ|PROT_WRITE)){
    return -1;
   }
   if(addr % PAGE_SIZE != 0){
    return -1;
   }
   if(flags != MAP_FIXED && flags != 0){
    return -1;
   }
   if(flags == MAP_FIXED && addr == NULL){
    return -1;
   }
   if(length % PAGE_SIZE != 0){
    length = length + (PAGE_SIZE - length % PAGE_SIZE);
   }
   if(current->vm_area == NULL){
       // printk("vm_area is empty\n");
        struct vm_area *new_vma = alloc_vm_area(MMAP_AREA_START, MMAP_AREA_START + PAGE_SIZE, 0x0);
        if(new_vma == NULL){
            return -1;
        }
        
        current->vm_area = new_vma;
    }
    struct vm_area *temp = current->vm_area;
    if(addr!=NULL){
    while(temp != NULL){
        if(addr >= temp->vm_start && addr  < temp->vm_end ){
            if(flags == MAP_FIXED)
            return -1;
            else addr = NULL;
        }
        if(addr+length > temp->vm_start && addr+length <= temp->vm_end){
            if(flags == MAP_FIXED)
            return -1;
            else addr = NULL;
          //  printk("Address becomes null\n");
        }
        temp = temp->vm_next;
    }
    }
   if(addr == NULL){
    struct vm_area *vma = current->vm_area;
    if(vma->vm_next == NULL){
        if(vma->access_flags == prot) vma->vm_end = vma->vm_end+ length;
        else{
            struct vm_area *new_vma = alloc_vm_area(vma->vm_end, vma->vm_end + length, prot);
            if(new_vma == NULL){
                return -1;
            }
            new_vma->vm_next = NULL;
            vma->vm_next = new_vma;
        }
        return vma->vm_end;
    }
    while(vma -> vm_next != NULL){
        if(vma->vm_next->vm_start - vma->vm_end >= length){
            addr = vma->vm_end;
            if(vma->vm_next->vm_start - vma->vm_end == length){
                if(vma->access_flags == prot){
                    vma->vm_end = vma->vm_next->vm_start;
                    if(vma->vm_next->access_flags == prot ){
                        struct vm_area *temp = vma->vm_next;
                        vma->vm_end = temp->vm_end;
                        vma->vm_next = temp->vm_next;
                        os_free(temp, sizeof(struct vm_area));
                        stats->num_vm_area--;
                    }
                    return addr;
                }
                if(vma->vm_next->access_flags == prot){
                    vma->vm_next->vm_start = vma->vm_end;
                    return addr;
                }
            }
            else{
                //allocate a new vm area
                struct vm_area *new_vma = alloc_vm_area(addr, addr + length, prot);
                if(new_vma == NULL){
                    return -1;
                }
                new_vma->vm_next = vma->vm_next;
                vma->vm_next = new_vma;
                return addr;
            }
            
        }
        vma = vma->vm_next;
    }
    //if the address is not found in the vma, then allocate a new vm area
    addr = vma->vm_end;
    if(vma->access_flags == prot){ vma -> vm_end = vma->vm_end + length;
        return addr;
    }
    struct vm_area *new_vma = alloc_vm_area(addr, addr + length, prot);
    if(new_vma == NULL){
        return -1;
    }
    new_vma->vm_next = NULL;
    vma->vm_next = new_vma;
    return addr;
    
   }
   else{
    struct vm_area *vma = current->vm_area;
    if(vma->vm_next == NULL){
        if(addr == vma->vm_end && prot== vma->access_flags) vma->vm_end = addr+ length;
        else{
            struct vm_area *new_vma = alloc_vm_area(addr, addr + length, prot);
            if(new_vma == NULL){
                return -1;
            }
            new_vma->vm_next = NULL;
            vma->vm_next = new_vma;
        }
        return addr;
    }
    while(vma->vm_next != NULL){
       if(addr >=vma ->vm_end &&  addr+length < vma->vm_next->vm_start){
        break;
    }
    vma = vma->vm_next;
    }
    //if the address is not found in the vma, then allocate a new vm area
    if(addr == vma->vm_end && prot== vma->access_flags){
        vma->vm_end = addr + length;
        if(vma->vm_next != NULL && vma->vm_next->access_flags == prot && vma->vm_next->vm_start == vma->vm_end){
            struct vm_area *temp = vma->vm_next;
            vma->vm_end = temp->vm_end;
            vma->vm_next = temp->vm_next;
            os_free(temp, sizeof(struct vm_area));
            stats->num_vm_area--;
        }
        return addr;
    }
    if(vma->vm_next && addr + length == vma->vm_next->vm_start && prot == vma->vm_next->access_flags){
        vma->vm_next->vm_start = addr;
       // printk("@here!\n");
        return addr;
    }
    struct vm_area *new_vma = alloc_vm_area(addr, addr + length, prot);
    if(new_vma == NULL){
        return -1;
    }
    new_vma->vm_next = vma->vm_next;
    vma->vm_next = new_vma;
    
    return addr;
}
}

/**
 * munmap system call implemenations
 */

long vm_area_unmap(struct exec_context *current, u64 addr, int length)
{   
    if(addr == NULL) return -1;
    if(addr % PAGE_SIZE != 0){
        return -1;
    }

    if(length<=0) return -1;
    if(length % PAGE_SIZE != 0){
        length = length + (PAGE_SIZE - length % PAGE_SIZE);
    }
    //if the address is not in vma return -1
    struct vm_area *temp = current->vm_area;
    
    u64 temp_addr = addr;

    //Physical memory deletion
    for(  ;temp_addr < addr + length; temp_addr += PAGE_SIZE ){
     unsigned long *l4addr = osmap(current->pgd);
        unsigned long *l3addr, *l2addr, *l1addr;
        u32 offsetL4, offsetL3, offsetL2, offsetL1;
        u32 pfnL1, pfnL2, pfnL3, dataPFN;
            //
        /* Extract out all 4 level page offsets */
        offsetL4 = PToffset & (temp_addr >> L4offset);
        offsetL3 = PToffset & (temp_addr >> L3offset);
        offsetL2 = PToffset & (temp_addr >> L2offset);
        offsetL1 = PToffset & (temp_addr >> L1offset);

        /* Cleanup Page Table entries */
        if (*(l4addr + offsetL4) & 1) {
              //  printk("level 1 walk in unmap\n");
                pfnL3 = *(l4addr + offsetL4) >> PTEoffset;
                l3addr = osmap(pfnL3);
        }
        else{
            continue;
        }
        
        if (*(l3addr + offsetL3) & 1) {
               // printk("level 2 walk in unmap\n");
                pfnL2 = *(l3addr + offsetL3) >> PTEoffset;
                l2addr = osmap(pfnL2);
                // os_pfn_free(OS_PT_REG, pfnL3);
        }
        else{
            continue;
        }
        if (*(l2addr + offsetL2) & 1) {
               // printk("level 3 walk in unmap\n");
                pfnL1 = *(l2addr + offsetL2) >> PTEoffset;
                l1addr = osmap(pfnL1);
                // os_pfn_free(OS_PT_REG, pfnL2);
        }
        else{
            continue;
        }
        if (*(l1addr + offsetL1) & 1) {
               // printk("level 4 walk in unmap\n");
                dataPFN = *(l1addr + offsetL1) >> PTEoffset;
                // os_pfn_free(OS_PT_REG, pfnL1);
        }
        else{
            continue;
        }
        // if pfn reference count is greater than 1 then reduce ref count by 1 and do nothing
        
        if(get_pfn_refcount(dataPFN) > 0){
           // printk("reference count is greater than 0 in unmap\n");
            put_pfn(dataPFN);
            
        }
        if(get_pfn_refcount(dataPFN)==0) os_pfn_free(USER_REG, dataPFN);
        *(l1addr + offsetL1) = 0;
        asm volatile("invlpg (%0);" 
                :: "r"(addr) 
                : "memory");
        FLUSH();
    // pfn_reaper1(current,temp_addr);
   // printk("in unmap pte\n ");
    }
    
    struct vm_area *vma = current->vm_area;
    // if the addr + length lies inside the vma, then split the vma into two vm areas
    struct vm_area *prev = NULL;
    while(vma!=NULL){
        unsigned long start = vma -> vm_start;
        unsigned long end = vma -> vm_end;
        if(addr>= start && addr + length <= end){
            if(addr == start && addr + length == end){
                if(prev!=NULL){
                    struct vm_area *temp = vma;
                    prev->vm_next = vma->vm_next;
                    os_free(temp, sizeof(struct vm_area));
                    stats->num_vm_area--;
                }
                else{
                    struct vm_area *temp = vma;
                    current->vm_area = vma->vm_next;
                    os_free(temp, sizeof(struct vm_area));
                    stats->num_vm_area--;
                }
                asm volatile("invlpg (%0);" 
                :: "r"(addr) 
                : "memory");
                return 0;
            }
             if(addr == start){
                vma ->vm_start = addr+length;
                return 0;
            }
             if(addr + length == end){
                vma ->vm_end = addr;
                asm volatile("invlpg (%0);" 
                :: "r"(addr) 
                : "memory");
                return 0;
            }
            struct vm_area *new_vma = alloc_vm_area(addr + length, end, vma->access_flags);
            if(new_vma == NULL){
                return -1;
            }
            new_vma->vm_next = vma->vm_next;
            vma->vm_next = new_vma;
            vma->vm_end = addr;
            asm volatile("invlpg (%0);" 
                :: "r"(addr) 
                : "memory");
            return 0;
        }
        else if(addr<=start && addr + length >=end){
            if(prev!=NULL){
                printk("^here\n");
                struct vm_area *temp = vma;
                prev->vm_next = vma->vm_next;
                vma = vma->vm_next;
                os_free(temp, sizeof(struct vm_area));
                stats->num_vm_area--;
                continue;
            }
            else{
                struct vm_area *temp = vma;
                current->vm_area = vma->vm_next;
                vma = vma->vm_next;
                os_free(temp, sizeof(struct vm_area));
                stats->num_vm_area--;
                continue;
            }
        }
        else if(addr > start && addr < end && addr + length > end){
            vma->vm_end = addr;
            
        }
        else if(addr < start && addr + length > start && addr + length < end){
            vma->vm_start = addr + length;
            
        }
        prev = vma;
        vma  = vma -> vm_next;
    }
    asm volatile("invlpg (%0);" 
                :: "r"(addr) 
                : "memory");
    return 0;

}




/**
 * Function will invoked whenever there is page fault for an address in the vm area region
 * created using mmap
 */

long vm_area_pagefault(struct exec_context *current, u64 addr, int error_code)
{   
    //printk("vm_area_pagefault\n");
  //  printk("error_code = %x\n", error_code);
    struct vm_area *vma = current->vm_area;
    int flag =0 ;
    // check if vma exists for addr
    while(vma != NULL){
        if(addr>= vma -> vm_start && addr < vma -> vm_end){ flag=1; break;}
        vma = vma->vm_next;
    }
    if(!flag){
        //printk("No such virtual address\n");
     return -1;}
   if(error_code&1){
    // check if second bit is 1 if yes, then check if the vma is writeable or not
    if(error_code&2 && !(vma->access_flags & PROT_WRITE)) {
       // printk("Invalid access \n")
       return -1;}
    else {
        // call handle_cow_fault
       // printk("handle_cow_fault\n");
        return handle_cow_fault(current, addr, vma->access_flags);
        
    }
   }
   else{
    int access_flags = vma->access_flags;
   // printk("access_flags = %x\n", access_flags);
    int flag = rw;
    if(!(access_flags & PROT_WRITE)){
        flag = 0;
    }
   // printk("flag = %x\n",flag);
    if(flag==0 && error_code&2){
      //  printk("Invalid access \n");
      return -1;
    }
    //Allocate a new physical page frame (PFN), set access flags and update the page table entry(s) of the faulting address. Note that pgd member of a process’s exec context can be used to find the virtual address of the PGD level of the page table using osmap(ctx->pgd).You have to structure the PTE entry(s) (Figure 2) at different levels depending on the access flags of the virtual address that is being mapped.
    u64 *l4addr = osmap(current->pgd);
    unsigned long *l3addr, *l2addr, *l1addr;
        u32 offsetL4, offsetL3, offsetL2, offsetL1;
        u32 pfnL1, pfnL2, pfnL3, dataPFN;
        /* Extract out all 4 level page offsets */
        offsetL4 = PToffset & (addr >> L4offset);
        offsetL3 = PToffset & (addr >> L3offset);
        offsetL2 = PToffset & (addr >> L2offset);
        offsetL1 = PToffset & (addr >> L1offset);

        /* Fill up Page Table Pages entries */
        if (*(l4addr + offsetL4) & 1)        // If PT already present
                pfnL3 = *(l4addr + offsetL4) >> PTEoffset;
        else {
                pfnL3 = os_pfn_alloc(OS_PT_REG);
                *(l4addr + offsetL4) = (pfnL3 << PTEoffset) | (pr | rw | us);
        }

        l3addr = osmap(pfnL3);
        if (*(l3addr + offsetL3) & 1)        // If PT already present
                pfnL2 = *(l3addr + offsetL3) >> PTEoffset;
        else {
                pfnL2 = os_pfn_alloc(OS_PT_REG);
                *(l3addr + offsetL3) = (pfnL2 << PTEoffset) | (pr | rw | us);
        }

        l2addr = osmap(pfnL2);
        if (*(l2addr + offsetL2) & 1)        // If PT already present
                pfnL1 = *(l2addr + offsetL2) >> PTEoffset;
        else {
                pfnL1 = os_pfn_alloc(OS_PT_REG);
                *(l2addr + offsetL2) = (pfnL1 << PTEoffset) | (pr | rw | us);
        }

        /* Final mapping to Data Physical Page */
        l1addr = osmap(pfnL1);
        if (*(l1addr + offsetL1) & 1)        // If PT already present
                dataPFN = *(l1addr + offsetL1) >> PTEoffset;
        else {
                dataPFN = os_pfn_alloc(USER_REG);
                *(l1addr + offsetL1) = (dataPFN << PTEoffset) | (pr | flag | us);
        }
   }
   return 1; 


}

/**
 * cfork system call implemenations
 * The parent returns the pid of child process. The return path of
 * the child process is handled separately through the calls at the 
 * end of this function (e.g., setup_child_context etc.)
 */
long do_cfork(){
    u32 pid;
    struct exec_context *new_ctx = get_new_ctx();
    struct exec_context *ctx = get_current_ctx();
     /* Do not modify above lines
     * 
     * */   
     /*--------------------- Your code [start]---------------*/
    if(new_ctx == NULL)
    {
        return -1;
    }
    
    u32 pid_child = new_ctx->pid;
    memcpy((void*)new_ctx,(void*)ctx,sizeof(struct exec_context));
    new_ctx->ppid = ctx->pid;
    new_ctx->pid = pid_child;
    pid = new_ctx->pid;
    u64 newpfn = os_pfn_alloc(OS_PT_REG);
    new_ctx->pgd = newpfn;
    for(u64 i = ctx->mms[MM_SEG_CODE].start; i < ctx->mms[MM_SEG_CODE].next_free; i+= 4096){
        install_page_table(ctx, new_ctx, i);
    }
    for(u64 i = ctx->mms[MM_SEG_RODATA].start; i < ctx->mms[MM_SEG_RODATA].next_free; i+= 4096){
        install_page_table(ctx, new_ctx, i);
    }
    for(u64 i = ctx->mms[MM_SEG_STACK].start; i < ctx->mms[MM_SEG_STACK].end; i+= 4096){
        install_page_table(ctx, new_ctx, i);
    }
    for(u64 i = ctx->mms[MM_SEG_DATA].start; i < ctx->mms[MM_SEG_DATA].next_free; i+= 4096){
        install_page_table(ctx, new_ctx, i);
    }

    //funnction to create a copy of list of vmas present in parent in child
    struct vm_area *dummy = ctx->vm_area->vm_next;
    
    struct vm_area *new_vm_area = os_alloc(sizeof(struct vm_area));
        new_vm_area->vm_start = MMAP_AREA_START;
        new_vm_area->vm_end = MMAP_AREA_START + 4096;
        new_vm_area->access_flags = 0x0;
        new_vm_area->vm_next = NULL;
        new_ctx->vm_area = new_vm_area;
    struct vm_area *temp = new_ctx->vm_area;
    while(dummy != NULL)
    {
        struct vm_area *new_vm_area = os_alloc(sizeof(struct vm_area));
        new_vm_area->vm_start = dummy->vm_start;
        new_vm_area->vm_end = dummy->vm_end;
        new_vm_area->access_flags = dummy->access_flags;
        new_vm_area->vm_next = NULL;
        temp->vm_next = new_vm_area;
        temp = temp->vm_next;
        dummy = dummy->vm_next;
    }
    //calling the create page tables function for the vmas present in the parent
    temp = new_ctx->vm_area->vm_next;
    while(temp != NULL)
    {
        for(u64 i = temp->vm_start; i < temp->vm_end; i+= 4096){
            install_page_table(ctx, new_ctx, i);
        }
        temp = temp->vm_next;
    }
     /*--------------------- Your code [end] ----------------*/
    
     /*
     * The remaining part must not be changed
     */
    copy_os_pts(ctx->pgd, new_ctx->pgd);
    do_file_fork(new_ctx);
    setup_child_context(new_ctx);
    return pid;
}



/* Cow fault handling, for the entire user address space
 * For address belonging to memory segments (i.e., stack, data) 
 * it is called when there is a CoW violation in these areas. 
 *
 * For vm areas, your fault handler 'vm_area_pagefault'
 * should invoke this function
 * */

long handle_cow_fault(struct exec_context *current, u64 vaddr, int access_flags)
{
    // printk("entered handle_cow_fault\n");
    u64 *pgd_base_cow = (u64 *)osmap(current->pgd);
    u64 offset_into_pgd_cow = (vaddr >> L4offset) & mask;
    u64 *offset_loc_in_pgd_cow = pgd_base_cow + offset_into_pgd_cow;

    if ((*offset_loc_in_pgd_cow & 0x1) == 0)
    {
        // printk("Level1 in cow\n");
        u64 pfn_cow = os_pfn_alloc(OS_PT_REG);
        *offset_loc_in_pgd_cow = 0x0;
        *offset_loc_in_pgd_cow = (pfn_cow << PTEoffset) | 0x19;
    }

    u64 *pud_base_cow = (u64 *)osmap(*offset_loc_in_pgd_cow >> PTEoffset);
    u64 offset_into_pud_cow = (vaddr >> 30) & mask;
    u64 *offset_loc_in_pud_cow = pud_base_cow + offset_into_pud_cow;

    if ((*offset_loc_in_pud_cow & 0x1) == 0)
    {
        // printk("Level2 in cow\n");
        u64 pfn_cow = os_pfn_alloc(OS_PT_REG);
        *offset_loc_in_pud_cow = 0x0;
        *offset_loc_in_pud_cow = (pfn_cow << PTEoffset) | 0x19;
    }

    u64 *pmd_base_cow = (u64 *)osmap(*offset_loc_in_pud_cow >> PTEoffset);
    u64 offset_into_pmd_cow = (vaddr >> 21) & mask;
    u64 *offset_loc_in_pmd_cow = pmd_base_cow + offset_into_pmd_cow;

    if ((*offset_loc_in_pmd_cow & 0x1) == 0)
    {
        // printk("Level3 in cow\n");
        u64 pfn_cow = os_pfn_alloc(OS_PT_REG);
        *offset_loc_in_pmd_cow = 0x0;
        *offset_loc_in_pmd_cow = (pfn_cow << PTEoffset) | 0x19;
    }

    u64 *pte_base_cow = (u64 *)osmap(*offset_loc_in_pmd_cow >> PTEoffset);
    u64 offset_into_pte_cow = (vaddr >> PTEoffset) & mask;
    u64 *offset_loc_in_pte_cow = pte_base_cow + offset_into_pte_cow;
    u64 pfn_cow = *offset_loc_in_pte_cow >> PTEoffset;
    //printk("Level4 in cow\n");
    // Allocate a new pfn and map it to the address
    if (get_pfn_refcount((*offset_loc_in_pte_cow) >> PTEoffset) > 1)
    {
        put_pfn(pfn_cow);
        u64 new_page_cow = os_pfn_alloc(USER_REG);
        *offset_loc_in_pte_cow = 0x0;
        *offset_loc_in_pte_cow = (new_page_cow << PTEoffset) | 0x19;

        // Copy the contents of the original page to the new page
        memcpy((void *)osmap(new_page_cow), (void *)osmap(pfn_cow), PAGE_SIZE);
    }
    else
    {
        *offset_loc_in_pte_cow |= 8;
    }
    FLUSH();
}
