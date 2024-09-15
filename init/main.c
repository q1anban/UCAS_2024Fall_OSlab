#include <common.h>
#include <asm.h>
#include <os/kernel.h>
#include <os/task.h>
#include <os/string.h>
#include <os/loader.h>
#include <type.h>

#define VERSION_BUF 50

#define TASK_NUM_LOC 0x502001f6
#define TASK_INFO_OFFSET 0x502001f8
#define KERNEL_LOC 0x50201000
#define MYSTERIOUS_PLACE 0x52000000

int version = 2; // version must between 0 and 9
char buf[VERSION_BUF];

// Task info array
task_info_t tasks[TASK_MAXNUM];
int tasknum;

static int bss_check(void)
{
    for (int i = 0; i < VERSION_BUF; ++i)
    {
        if (buf[i] != 0)
        {
            return 0;
        }
    }
    return 1;
}

static void init_jmptab(void)
{
    volatile long (*(*jmptab))() = (volatile long (*(*))())KERNEL_JMPTAB_BASE;

    jmptab[CONSOLE_PUTSTR]  = (long (*)())port_write;
    jmptab[CONSOLE_PUTCHAR] = (long (*)())port_write_ch;
    jmptab[CONSOLE_GETCHAR] = (long (*)())port_read_ch;
    jmptab[SD_READ]         = (long (*)())sd_read;
}

static void init_task_info(void)
{
    // TODO: [p1-task4] Init 'tasks' array via reading app-info sector
    // NOTE: You need to get some related arguments from bootblock first
    //use the exact position to find task info
    int16_t *ptasknum = (int16_t*) TASK_NUM_LOC;
    int32_t *taskinfo_offset = (int32_t*)TASK_INFO_OFFSET;

    int num = *ptasknum;
    tasknum=num;
    int offset = *taskinfo_offset;
    int infosize = sizeof(task_info_t)*num;
    int start_sec = offset / SECTOR_SIZE;
    int end_sec =(offset+infosize)/SECTOR_SIZE;
    int num_sec = end_sec-start_sec+1;
    sd_read(MYSTERIOUS_PLACE,num_sec,start_sec);
    char* ptr = (char*)(MYSTERIOUS_PLACE +offset%SECTOR_SIZE);
    for(int i=0;i<num;i++)
    {
        for(int j=0;j<32;j++)
        {
            tasks[i].name[j]=*ptr;
            ptr++;
        }
        tasks[i].offset=*((int*)ptr);
        ptr+=4;
        tasks[i].size=*((int*)ptr);
        ptr+=4;
    }
}

/************************************************************/
/* Do not touch this comment. Reserved for future projects. */
/************************************************************/

int main(void)
{
    // Check whether .bss section is set to zero
    int check = bss_check();

    // Init jump table provided by kernel and bios(ΦωΦ)
    init_jmptab();

    // Init task information (〃'▽'〃)
    init_task_info();

    // Output 'Hello OS!', bss check result and OS version
    char output_str[] = "bss check: _ version: _\n\r";
    char output_val[2] = {0};
    int i, output_val_pos = 0;

    output_val[0] = check ? 't' : 'f';
    output_val[1] = version + '0';
    for (i = 0; i < sizeof(output_str); ++i)
    {
        buf[i] = output_str[i];
        if (buf[i] == '_')
        {
            buf[i] = output_val[output_val_pos++];
        }
    }

    bios_putstr("Hello OS!\n\r");
    bios_putstr(buf);

    // TODO: Load tasks by either task id [p1-task3] or task name [p1-task4],
    //   and then execute them.

    // Infinite while loop, where CPU stays in a low-power state (QAQQQQQQQQQQQ)
    char ch;
    char usr_input[100];
    int idx=0;
    int entry_point;
    while (1)
    {
        ch=bios_getchar();
        if(ch=='\n' || ch == '\r')//读取输入
        {
            bios_putchar('\n');
            usr_input[idx]='\0';
            idx=0;
            for(i=0;i<tasknum;i++)
            {
                if(strcmp(usr_input,tasks[i].name)==0)
                {
                    entry_point=load_task_img(i);
                    ((void(*)())entry_point)();
                }
            }
            usr_input[idx]='\0';
        }else if('a'<=ch && ch<='z' || 'A'<=ch && ch<='Z' || '0' <=ch && ch <='9')//有效输入
        {
            usr_input[idx] = ch;
            idx++;
            bios_putchar(ch);
            continue;
        }
        else
        {
            //什么都不做
        }
    }

    return 0;
}
