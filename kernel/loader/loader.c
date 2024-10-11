#include <os/task.h>
#include <os/string.h>
#include <os/kernel.h>
#include <type.h>

uint64_t load_task_img(int taskid)
{
    /**
     * TODO:
     * 1. [p1-task3] load task from image via task id, and return its entrypoint
     * 2. [p1-task4] load task via task name, thus the arg should be 'char *taskname'
     */
    //通过tasks得到task相关的大小、偏移    和名字（不关键）

    int offset = tasks[taskid].offset;
    int size = tasks[taskid].size;

    //找到包含整个过程的扇区

    int start_sec = offset / SECTOR_SIZE ;//起始扇区
    int end_sec = (offset+size)/SECTOR_SIZE;//结束扇区
    int num_sec = end_sec - start_sec + 1;//扇区数
    int start_address = TASK_MEM_BASE + taskid * TASK_SIZE;//内存位置
    int start_offset = offset % SECTOR_SIZE;//起始位置相对于扇区头的偏移
    sd_read(start_address,num_sec,start_sec);
    char* src = (char*)(start_address + start_offset);
    char* dst = (char*)start_address;
    for(int i=0;i<size;i++)
    {
        *dst=*src;
        dst++;
        src++;
    }
    return start_address;

    return 0;
}