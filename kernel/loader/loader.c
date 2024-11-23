#include <os/task.h>
#include <os/mm.h>
#include <os/string.h>
#include <os/kernel.h>
#include <type.h>
#include <pgtable.h>



//输入taskid
//将对应的task加载至内存，完成页表映射
//返回对应的页表地址(物理地址)
uint64_t load_task_img(int taskid,uint8_t asid)
{
    /**
     * TODO:
     * 1. [p1-task3] load task from image via task id, and return its entrypoint
     * 2. [p1-task4] load task via task name, thus the arg should be 'char *taskname'
     */
    //通过tasks得到task相关的大小、偏移    和名字（不关键）

    int offset = tasks[taskid].offset;
    int file_size = tasks[taskid].file_size;
    int mem_size = tasks[taskid].mem_size;  

    //找到包含整个过程的扇区
    int start_sec = offset / SECTOR_SIZE ;//起始扇区
    int end_sec = (offset+file_size)/SECTOR_SIZE;//结束扇区
    int num_sec = end_sec - start_sec + 1;//扇区数
    int start_offset = offset % SECTOR_SIZE;//起始偏移

    //随便把东西找个地方放了先
    sd_read(kva2pa(TEMP_TASK_IMG),num_sec,start_sec);
    char* start_ptr = (char*)(TEMP_TASK_IMG+start_offset);
    //为bss段填充0
    memset((void*)(start_ptr+file_size),0,mem_size-file_size);

    //新增页表映射：1、内核页表映射。2、task数据映射
    uintptr_t page_dir = allocPagetable(asid);
    uintptr_t kernel_page_dir = pa2kva(PGDIR_PA);
    //内核页表映射
    share_pgtable(page_dir,kernel_page_dir);
    //task数据映射
    int page_num = mem_size / PAGE_SIZE + (mem_size % PAGE_SIZE != 0);
    for(int i=0;i<page_num;i++)
    {
        uint64_t kva=alloc_page_helper(0x10000+ i*PAGE_SIZE,page_dir,asid);//分配物理页
        memcpy((void*)kva,start_ptr+i*PAGE_SIZE,PAGE_SIZE);//将task数据复制至其对应的页上
    }
    //至此，完成了1、加载任务2、完成页表映射
    return kva2pa(page_dir);
}