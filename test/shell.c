/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *            Copyright (C) 2018 Institute of Computing Technology, CAS
 *               Author : Han Shukai (email : hanshukai@ict.ac.cn)
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *                  The shell acts as a task running in user mode.
 *       The main function is to make system calls through the user's output.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this
 * software and associated documentation files (the "Software"), to deal in the Software
 * without restriction, including without limitation the rights to use, copy, modify,
 * merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit
 * persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * */

#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <syscall.h>

#define SHELL_BEGIN 20
#define BUFFER_SIZE 128

char buf[BUFFER_SIZE];
int i = 0;//end of string ,buf[i]='\0'

char path [128];
int atoi(char *str)
{
    int num = 0;
    while (*str)
    {
        num = num * 10 + (*str - '0');
        str++;
    }
    return num;
}

void remove_last_slash(char *path)
{
    int len = strlen(path);
    if(len>1&&path[len-1]=='/')
        path[len-1]='\0';
}

int next_printable(int current)
{
    while(current<i&&!isprintable(buf[current]))
        current++;
    
    return current;
}

int next_space(int current)
{
    while(current<i&&!isspace(buf[current]))
        current++;
    return current;
}

int prev_alpha(int current)
{
    while(current>-1&&!isalpha(buf[current-1]))
        current--;
    return current;
}

int prev_space(int current)
{
    while(current>-1&&!isspace(buf[current-1]))
        current--;
    return current;
}

void remove_dot_dot(char *path) 
{
    int len = strlen(path);
    int i = 0;
    int j = 0;

    while (i < len) {
        if (path[i] == '/' && path[i + 1] == '.' && path[i + 2] == '.' && (path[i + 3] == '/' || path[i + 3] == '\0')) {
            // Move j back to the previous directory
            if (j > 0) {
                j--; // Move back from the trailing '/'
                while (j > 0 && path[j] != '/') {
                    j--;
                }
            }
            i += 3; // Skip over "/.."
        } else {
            path[j++] = path[i++];
        }
    }
    if(j!=0)
        path[j] = '\0';
    else
    {
        path[j] = '/';
        path[j+1] = '\0';
    }
}

char history[BUFFER_SIZE];
int history_index = 0;

void parse()
{
    int command_start = next_printable(0);
    int command_end = next_space(command_start);
    buf[command_end] = '\0';

    int arg_start = next_printable(command_end);

    if(strcmp(buf+command_start, "ps") == 0)
    {
        sys_ps();
    }
    else if(strcmp(buf+command_start, "exec") == 0)
    {
        int wait_flag = i-1;
        while(isspace(buf[wait_flag]))
            wait_flag--;
        if(buf[wait_flag]=='&')
        {
            buf[wait_flag]='\0';
            wait_flag=0;
            
        }
        else
            wait_flag=1;
        char* name = buf+arg_start;
        int name_end = next_space(arg_start);
        buf[name_end] = '\0';
        //split args
        int argc = 1;
        char *argv[16];
        argv[0] = name;
        int p = name_end+1;
        while(p<i)
        {
            p = next_printable(p);
            if (p>=i)
                break;
            argv[argc] = buf+p;
            argc++;
            p = next_space(p);
            buf[p] = '\0';
        }
        int pid ;
        if((pid =sys_exec(name, argc, argv))!=-1) 
        {
            printf("Info:Process %s created, pid: %d\n", name, pid);
            if(wait_flag)
            sys_waitpid(pid);

        }
        else
            printf("Error:Failed to create process %s!\n", name);
    }
    else if (strcmp(buf+command_start, "kill") == 0)
    {
        int arg_end = next_space(arg_start);
        buf[arg_end] = '\0';
        if(!sys_kill(atoi(buf+arg_start)))
            printf("Error:no such process with given pid!\n");
    }
    else if(strcmp(buf+command_start, "clear") == 0)
    {
        sys_clear();
        sys_move_cursor(0, SHELL_BEGIN);
        printf("------------------- COMMAND -------------------\n");
    }else if(strcmp(buf+command_start, "mkfs") == 0)
    {
        int result = sys_mkfs();
        if(result==0)
            printf("Info:File system created successfully!\n");
        else if(result==-1)
            printf("Error:Failed to read disk!\n");
        else if(result==-2)
            printf("Error:File system already exists!\n");
        
    }
    else if(strcmp(buf+command_start, "statfs") == 0)
    {
        sys_statfs();
    }else if(strcmp(buf+command_start, "cd") == 0)
    {
        //don't support blank input
        buf[next_space(arg_start)]='\0';
        char new_path[128];
        printf("current:%s",path);
        if(buf[arg_start]=='/')
        {
            //absolute path
            strcpy(new_path, buf+arg_start);
        }
        else
        {
            strcpy(new_path, path);
            if(new_path[strlen(new_path)-1]!='/')
                strcat(new_path,"/");
            strcat(new_path,buf+arg_start);
        }
        printf("before remove dot dot new_path:%s\n", new_path);
        remove_dot_dot(new_path);
        printf("after remove dot dot new_path:%s\n", new_path);
        remove_last_slash(new_path);
        printf("%s\n", new_path);
        if(sys_cd(new_path)==-1)
        {
            printf("Error:No such directory!\n");
        }
        else
        {
            strcpy(path, new_path);
        }
    }
    else if(strcmp(buf+command_start, "mkdir") == 0)
    {
        int arg_end = next_space(arg_start);
        buf[arg_end] = '\0';
        char new_path[128];
        strcpy(new_path, buf+arg_start);
        if (sys_mkdir(new_path)<0)
        {
            printf("Error:Failed to create directory!\n");
        }
    }
    else if(strcmp(buf+command_start, "rmdir") == 0)
    {
        int arg_end = next_space(arg_start);
        buf[arg_end] = '\0';
        char new_path[128];
        strcpy(new_path, buf+arg_start);
        if (sys_rmdir(new_path)<0)
        {
            printf("Error:Failed to remove directory!\n");
        }
    }
    else if (strcmp(buf+command_start, "ls") == 0)
    {
        int option = 0;
        if(i>arg_start)
        {
            if(buf[arg_start]=='-')
            {
                option = 1;
                arg_start = next_printable(next_space(arg_start));
            }
        }
        char new_path[128];
        buf[next_space(arg_start)] = '\0';
        strcpy(new_path, buf+arg_start);
        if(sys_ls(new_path, option)<0)
        {
            printf("Error:Failed to list directory!\n");
        }
    }else if(strcmp(buf+command_start, "touch") == 0)
    {
        buf[next_space(arg_start)]='\0';
        char new_path[128];
        
        strcpy(new_path, buf+arg_start);
        
        int fd = sys_open(new_path, O_RDWR);
        sys_close(fd);
    }
    else if(strcmp(buf+command_start,"cat")==0)
    {
        buf[next_space(arg_start)]='\0';
        char new_path[128];
        strcpy(new_path, buf+arg_start);
        int fd = sys_open(new_path, O_RDWR);
        if(fd<0)
        {
            printf("Error:Failed to open file!\n");
            return;
        }
        char buff[128];
        int len;
        while((len = sys_read(fd, buff, 128))<128)
        {
            for(int i=0;i<len;i++)
                printf("%c", buff[i]);
        }
        for(int i=0;i<len;i++)
            printf("%c", buff[i]);
        sys_close(fd);
    }
    else if(strcmp(buf+command_start,"ln")==0)
    {
        buf[next_space(next_printable(next_space(arg_start)))]='\0';
        int ptr = next_printable(next_space(arg_start)) - arg_start;
        char new_path[128];
        strcpy(new_path,buf+arg_start);
        new_path[ptr-1]='\0';
        sys_ln(new_path,new_path+ptr);   
    }else if(strcmp(buf+command_start,"rm")==0)
    {
        buf[next_space(arg_start)]='\0';
        char new_path[128];
        strcpy(new_path, buf+arg_start);
        sys_rm(new_path);
    }
    else
    {
        printf("Error:Unknown command %s!\n", buf+command_start);//no need for \n
    }
    //clear buf after parse
    i=0;
}

int main(void)
{
    sys_clear();
    sys_move_cursor(0, SHELL_BEGIN);
    printf("------------------- COMMAND -------------------\n");

    strcpy(path, "/");
    printf("> root@UCAS_OS: %s#", path);
    while (1)
    {
        // TODO [P3-task1]: call syscall to read UART port
        char ch = sys_getchar();
        // TODO [P3-task1]: parse input
        // note: backspace maybe 8('\b') or 127(delete)

        if(ch==0x1b)//arrow key
        {
            ch = sys_getchar();
            if(ch==91)
            {
                ch = sys_getchar();
                if(ch==65) //up
                {
                    //use history
                    if(history_index>0)
                    {
                        for(int j=0;j<i;j++)
                            printf("\b \b");
                        i=strlen(history);
                        strcpy(buf, history);
                        printf("%s", history);
                    }
                }
            }
        }    
        else if (ch == 8 || ch == 127)
        {
            if (i > 0)
            {
                i--;                
                printf("\b \b");
            }
        }else if(ch == 10 || ch == 13)//enter
        {
            buf[i] = '\0';
            printf("\n");
            history_index=1;
            strcpy(history, buf);
            parse();
            printf("> root@UCAS_OS: %s#", path);
        }
        else if(isprintable(ch) || isspace(ch))
        {
            buf[i] = ch;
            printf("%c", ch);
            i++;
        }
        // TODO [P3-task1]: ps, exec, kill, clear    

        /************************************************************/
        // TODO [P6-task1]: mkfs, statfs, cd, mkdir, rmdir, ls

        // TODO [P6-task2]: touch, cat, ln, ls -l, rm
        /************************************************************/
    }

    return 0;
}
