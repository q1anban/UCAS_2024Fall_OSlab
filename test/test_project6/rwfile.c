#include <stdio.h>
#include <string.h>
#include <unistd.h>

static char buff[64];

int main(void)
{
    int fd = sys_open("1.txt", O_RDWR);

    // write 'hello world!' * 10
    // for (int i = 0; i < 10; i++)
    // {
    //     sys_write(fd, "hello world!\n", 13);
    // }
    // sys_lseek(fd, 0, SEEK_SET);
    // // read
    // for (int i = 0; i < 10; i++)
    // {
    //     sys_read(fd, buff, 13);
    //     for (int j = 0; j < 13; j++)
    //     {
    //         printf("%c", buff[j]);
    //     }
    // }
    const char * str = "hello world!\n";
    sys_write(fd, str, strlen(str));
    sys_lseek(fd,128*1024*1024,SEEK_SET);
    sys_write(fd, str, strlen(str));

    sys_lseek(fd,0,SEEK_SET);
    sys_read(fd,buff,13);
    printf("%s",buff);
    sys_lseek(fd,128*1024*1024,SEEK_SET);
    sys_read(fd,buff,13);
    printf("%s",buff);

    sys_close(fd);

    return 0;
}