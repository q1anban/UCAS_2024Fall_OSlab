#ifndef __INCLUDE_OS_FS_H__
#define __INCLUDE_OS_FS_H__

#include <type.h>

/* macros of file system */
#define SUPERBLOCK_MAGIC 0xDF4C4459
#define NUM_FDESCS 16

#define INODE_NUM 2048

#define START_SEC (1<<20)

#define BLOCK_SIZE 4096 
#define BLOCK_MAP_SIZE 1<<17 

#define INDIRECT_SIZE (BLOCK_SIZE/sizeof(uint32_t))

#define OWNER_READ 0x100
#define OWNER_WRITE 0x080
#define OWNER_EXEC 0x040
#define GROUP_READ 0x020
#define GROUP_WRITE 0x010
#define GROUP_EXEC 0x008
#define OTHER_READ 0x004
#define OTHER_WRITE 0x002
#define OTHER_EXEC 0x001

/* 
 *
 *
**/

/* data structures of file system */
typedef struct superblock {
    // TODO [P6-task1]: Implement the data structure of superblock
    uint32_t magic;
    uint32_t fs_size;
    uint32_t fs_start_sec;

    uint32_t block_size;
    uint32_t block_map_offset;
    uint32_t inode_map_offset;
    uint32_t inode_size;
    uint32_t dentry_size;
    uint32_t inode_offset;
    uint32_t data_offset;
} superblock_t;


typedef struct dentry {
    // TODO [P6-task1]: Implement the data structure of directory entry
    char name[28];
    uint32_t inode_num;
} dentry_t;


typedef struct inode{ 
    // TODO [P6-task1]: Implement the data structure of inode
    uint32_t mode;
    uint32_t size;
    uint32_t link;
    uint32_t sec[8];
    uint32_t indirect_flag; 
} inode_t;

typedef struct fdesc {
    // TODO [P6-task2]: Implement the data structure of file descriptor
    int mode;
    int offset;
    int inode_num;
    int ref_count;
} fdesc_t;

/* modes of do_open */
#define O_RDONLY 1  /* read only open */
#define O_WRONLY 2  /* write only open */
#define O_RDWR   3  /* read/write open */

/* whence of do_lseek */
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

/* fs function declarations */
extern int do_mkfs(void);
extern int do_statfs(void);
extern int do_cd(char *path);
extern int do_mkdir(char *path);
extern int do_rmdir(char *path);
extern int do_ls(char *path, int option);
extern int do_open(char *path, int mode);
extern int do_read(int fd, char *buff, int length);
extern int do_write(int fd, char *buff, int length);
extern int do_close(int fd);
extern int do_ln(char *src_path, char *dst_path);
extern int do_rm(char *path);
extern int do_lseek(int fd, int offset, int whence);

//helper functions
extern int init_fs();

#endif