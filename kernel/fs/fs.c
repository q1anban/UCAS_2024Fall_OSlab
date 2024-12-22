#include <os/string.h>
#include <os/fs.h>
#include <os/mm.h>
#include <common.h>
#include <printk.h>


#define SEC_SIZE 512

static fdesc_t fdesc_array[NUM_FDESCS];
static uint8_t inode_map[INODE_NUM];
char path_buffer[128];

superblock_t superblock;
int current_inode;

uint8_t * block_map = (uint8_t *)BLOCK_MAP_PLACE;

inode_t * inodes = (inode_t *)INODE_PLACE;

//实际上最后57个块不能使用

//helper functions
inline static int get_start_sec(int offset)
{
    return superblock.fs_start_sec + offset / SEC_SIZE;
}

inline static void store_superblock(void)
{
    sd_write(kva2pa(&superblock),1,get_start_sec(0));
}

inline static void load_superblock(void)
{
    sd_read(kva2pa(TEMP),1,START_SEC);
    memcpy(&superblock,TEMP,sizeof(superblock_t));
}

inline static void store_inode_map(void)
{
    sd_write(kva2pa(inode_map),INODE_NUM/SEC_SIZE,get_start_sec(superblock.inode_map_offset));
}

inline static void load_inode_map(void)
{
    sd_read(kva2pa(inode_map),INODE_NUM/SEC_SIZE,get_start_sec(superblock.inode_map_offset));
}

inline static void store_block_map(void)
{
    sd_write(kva2pa(block_map),BLOCK_MAP_SIZE/SEC_SIZE,get_start_sec(superblock.block_map_offset));
}

inline static void load_block_map(void)
{
    sd_read(kva2pa(block_map),BLOCK_MAP_SIZE/SEC_SIZE,get_start_sec(superblock.block_map_offset));
}

inline static void store_inodes(void)
{
    sd_write(kva2pa(INODE_PLACE),INODE_NUM*sizeof(inode_t)/SEC_SIZE,get_start_sec(superblock.inode_offset));
}

inline static void load_inodes(void)
{
    sd_read(kva2pa(INODE_PLACE),INODE_NUM*sizeof(inode_t)/SEC_SIZE,get_start_sec(superblock.inode_offset));
}

inline static int directory_sec_is_not_used(int inode_num,int sec_num)
{
    return (inodes[inode_num].sec[sec_num] == 0 && ((inode_num != 0) || (inode_num ==0 && sec_num > 0)));
}

//assume data block is already in TEMP
//block is the block number of the data block
inline static void store_a_data_block(int block) 
{
    sd_write(kva2pa(TEMP),BLOCK_SIZE / SEC_SIZE,get_start_sec(superblock.data_offset+block*BLOCK_SIZE));
}

//load the block to TEMP
inline static void load_a_data_block(int block)
{
    sd_read(kva2pa(TEMP),BLOCK_SIZE / SEC_SIZE,get_start_sec(superblock.data_offset+block*BLOCK_SIZE));
}

int get_free_inode(void)
{
    for(int i=0;i<INODE_NUM;i++)
    {
        if(inode_map[i]==0)
        {
            inode_map[i] = 1;
            return i;
        }
    }
    return -1;
}

int get_free_block(void)
{
    for(int i=0;i<BLOCK_MAP_SIZE;i++)
    {
        if(block_map[i]==0)
        {
            block_map[i] = 1;
            return i;
        }
    }
    return -1;
}

char *split_path(char *path) {
    static char *last;
    if (path == NULL) {
        path = last;
    }
    if (path == NULL) {
        return NULL;
    }

    // Skip leading slashes
    while (*path == '/') {
        path++;
    }

    if (*path == '\0') {
        last = NULL;
        return NULL;
    }

    char *token_start = path;

    // Find the end of the token
    while (*path && *path != '/') {
        path++;
    }

    if (*path) {
        *path = '\0';
        last = path + 1;
    } else {
        last = NULL;
    }

    return token_start;
}

//sepreate the parent directory and the name of the file , return the name of the file
char* seperate_parent(char* path)
{
    char* name = path + strlen(path)-1;
    if (*name = '/')
    {
        *name = '\0';//remove the last '/'
    }
    while(*name!='/')
        name--;
    *name = '\0';//find the parent directory
    name++;//now name is the name of the directory to create , and path is the parent directory
    return name;
}

int parse_path(char* path)
{
    //parse the path and return the inode number of the file
    //return -1 if the file does not exist
    int follow_inode = 0;
    int find=0;
    char *token = split_path(path);
    while(token!=NULL)
    {
        //now find token in follow_inode 's dentry
        dentry_t* de = TEMP;
        for(int i=0;i< 8 ;i++) //every inode contain 8 sec
        {
            if(directory_sec_is_not_used(follow_inode,i)) //no more dentry
                break;
            load_a_data_block(inodes[follow_inode].sec[i]);
            for(int j=0;j<BLOCK_SIZE/sizeof(dentry_t);j++)
            {
                if(strcmp(de[j].name,token)==0)
                {
                    follow_inode = de[j].inode_num;
                    find = 1;
                    break;
                }
            }
            if(find)
            {
                break;
            }
        }
        if(!find)
        {
            return -1; //no such file
        }else
        {
            find = 0;
        }
        token = split_path(NULL);
    }
    return follow_inode;
}


int do_mkfs(void) 
{
    // TODO [P6-task1]: Implement do_mkfs
    int start_sec_times = 1<<20; //512MB / 512B = 1024*1024
    int i=1;
    if(sd_read(kva2pa(TEMP),1,i*start_sec_times))
    {   
        return -1;//failed to read
    }
    memcpy(&superblock,TEMP,sizeof(superblock_t));
    if(superblock.magic == SUPERBLOCK_MAGIC)
    {
        return  -2;//fs already exists
    }
    superblock.magic = SUPERBLOCK_MAGIC;
    superblock.fs_size = 1<<29; //512MB
    superblock.fs_start_sec = i*start_sec_times;

    superblock.inode_map_offset = SEC_SIZE;
    superblock.inode_offset = superblock.inode_map_offset + INODE_NUM ;
    superblock.inode_size = sizeof(inode_t);

    superblock.block_map_offset = superblock.inode_offset + superblock.inode_size * INODE_NUM;
    superblock.block_size = BLOCK_SIZE;

    superblock.dentry_size = sizeof(dentry_t);
    superblock.data_offset = superblock.block_map_offset + superblock.fs_size / BLOCK_SIZE;
    
    //clear block map and inode map
    memset(block_map,0,BLOCK_MAP_SIZE);
    memset(inode_map,0,INODE_NUM);

    //initialize root inode
    //root inode is the first inode
    inode_map[0] = 1;
    inode_t* root_inode = INODE_PLACE; // the first inode
    //no need to add excute permission to root since it will be opened by operating system
    root_inode->mode = OWNER_READ | OWNER_WRITE | GROUP_READ | OTHER_READ;
    root_inode->size = 0;
    root_inode->indirect_flag = 0;
    root_inode->sec[0] = 0;
    root_inode->link = 1;

    block_map[0] = 1; //block 0 is used by root inode

    //every directory has a dentry "." , every directory except root has a dentry ".."
    dentry_t* de =  TEMP; 
    memset(de,0,BLOCK_SIZE);
    strcpy(de->name,".");
    de->inode_num = 0;    

    //store everything
    store_superblock();
    store_inode_map();
    store_block_map();
    store_inodes();

    //store dentry of root
    store_a_data_block(0);

    return 0;  // do_mkfs succeeds
}

int do_statfs(void)
{
    // TODO [P6-task1]: Implement do_statfs
    load_superblock();
    if(superblock.magic != SUPERBLOCK_MAGIC)
    {
        printk("no filesystem found\n");
        return -1;
    }
    printk("Filesystem size: %d bytes\n",superblock.fs_size);
    printk("Block size: %d bytes\n",superblock.block_size);
    printk("Inode size: %d bytes\n",superblock.inode_size);
    printk("Inodes: %d\n",INODE_NUM);
    printk("Block map offset: %d bytes\n",superblock.block_map_offset);
    printk("Inode map offset: %d bytes\n",superblock.inode_map_offset);
    printk("Data offset: %d bytes\n",superblock.data_offset);
    printk("Dentry size: %d bytes\n",superblock.dentry_size);
    printk("Root inode number: %d\n",0);
    return 0;  // do_statfs succeeds
}

int init_fs()
{
    //called by OS to initialize the file system
    load_superblock();
    if(superblock.magic != SUPERBLOCK_MAGIC)
    {
        return -1;
    }
    //FS exists, load it
    current_inode = 0; //root
    //load inode map
    load_inode_map();
    //load block map
    load_block_map();
    //load inodes
    load_inodes();
    
    return 0;
}


int do_cd(char *path)
{
    // TODO [P6-task1]: Implement do_cd
    //path is absolute path
    strcpy(path_buffer,path);
    int follow_inode = parse_path(path_buffer);
    current_inode = follow_inode;
    return follow_inode;  // -1 for failed
}

int do_mkdir(char *path)
{
    strcpy(path_buffer,path);

    // TODO [P6-task1]: Implement do_mkdir
    //path is absolute path
    //notice : the last token is the name of the directory to create , so remove it when doing path traversal

    char* name = seperate_parent(path_buffer);

    int follow_inode = parse_path(path_buffer); 

    if (follow_inode ==-1)
    {
        return -1; //no such directory
    }
    //create the directory
    //find a free inode
    int new_inode = get_free_inode();
    inodes[new_inode].mode = OWNER_READ | OWNER_WRITE| OWNER_EXEC | GROUP_READ | GROUP_EXEC |OTHER_READ | OTHER_EXEC;
    inodes[new_inode].size = 0;
    inodes[new_inode].indirect_flag = 0;
    inodes[new_inode].link = 2;
    
    //now follow_inode is the place for the directory to create
    //scan if the directory already exists
    dentry_t* de = TEMP;
    int i;//sec for new dentry to store
    int finished=0;
    for ( i= 0; i < 8; i++)
    {
        /* code */
        if(directory_sec_is_not_used(follow_inode,i))
            break;
        load_a_data_block(inodes[follow_inode].sec[i]);
        for(int j=0;j<BLOCK_SIZE/sizeof(dentry_t);j++)
        {
            if(de[j].name[0] == 0)
            {
                //available dentry 
                strcpy(de[j].name,name);
                de[j].inode_num = new_inode;
                inodes[follow_inode].link++;
                finished = 1;
                break;
            }
            else if(strcmp(de[j].name,name)==0)
            {
                return -2; //directory already exists
            }
        }
        if (finished)
        {
            break;
        }
    }
    if(finished==0)
    {
        //no available dentry
        //create a new sec 
        inodes[follow_inode].sec[i] = get_free_block();
        inodes[follow_inode].size += BLOCK_SIZE;
        inodes[follow_inode].indirect_flag = 0;
        inodes[follow_inode].link++;
        memset(TEMP,0,BLOCK_SIZE);
        strcpy(de[0].name,name);
        de[0].inode_num = new_inode;
    }
    //finish creating dentry
    //store dentry
    store_a_data_block(inodes[follow_inode].sec[i]);
    
    //find a free block
    int new_block = get_free_block();
    inodes[new_inode].sec[0] = new_block;
    //store inode and name 
    memset(TEMP,0,BLOCK_SIZE);
    strcpy(de[0].name,".");
    de[0].inode_num = new_inode;
    strcpy(de[1].name,"..");
    de[1].inode_num = follow_inode;
    store_a_data_block(new_block);

    store_block_map();
    store_inode_map();
    store_inodes();

    return 0;  // do_mkdir succeeds
}


int do_rmdir(char *path)
{
    strcpy(path_buffer,path);
    // TODO [P6-task1]: Implement do_rmdir
    char* name = seperate_parent(path_buffer);
    int follow_inode = parse_path(path_buffer);
    if (follow_inode == -1)
    {
        return -1; //no such directory
    }
    //find the dentry of the directory to remove
    dentry_t* de = TEMP;
    int i;
    int j;
    int finished = 0;
    int inode_to_remove;
    for(i=0;i<8;i++)
    {
        if(directory_sec_is_not_used(follow_inode,i))
            break;
        load_a_data_block(inodes[follow_inode].sec[i]);
        for(j=0;j<BLOCK_SIZE/sizeof(dentry_t);j++)
        {
            if(strcmp(de[j].name,name)==0)
            {
                inode_to_remove = de[j].inode_num;
                memset(de+j,0,sizeof(dentry_t));
                finished = 1;
                break;
            }
        }
        if(finished)
        {
            break;
        }
    }
    store_a_data_block(inodes[follow_inode].sec[i]);
    inodes[follow_inode].link--;
    inodes[inode_to_remove].link-=2;
    if(inodes[inode_to_remove].link <=0)
    {
        inode_map[inode_to_remove] = 0;
        for(int i=0;i<8;i++)
        {
            if(inodes[inode_to_remove].sec[i]!=0)
               block_map[inodes[inode_to_remove].sec[i]] = 0;
        }
    }

    store_inode_map();
    store_inodes();
    store_block_map();

    return 0;  // do_rmdir succeeds
}

int do_ls(char *path, int option)
{
    strcpy(path_buffer,path);
    // TODO [P6-task1]: Implement do_ls
    // Note: argument 'option' serves for 'ls -l' in A-core
    int follow_inode = parse_path(path_buffer);
    for(int i=0;i<8;i++)
    {
        if(directory_sec_is_not_used(follow_inode,i))
            break;
        load_a_data_block(inodes[follow_inode].sec[i]);
        dentry_t* de = TEMP;
        for(int j=0;j<BLOCK_SIZE/sizeof(dentry_t);j++)
        {
            if(de[j].name[0]!=0)
            {
                printk("%s\n",de[j].name);
            }
        }
    }
    return 0;  // do_ls succeeds
}

int do_open(char *path, int mode)
{
    // TODO [P6-task2]: Implement do_open

    return 0;  // return the id of file descriptor
}

int do_read(int fd, char *buff, int length)
{
    // TODO [P6-task2]: Implement do_read

    return 0;  // return the length of trully read data
}

int do_write(int fd, char *buff, int length)
{
    // TODO [P6-task2]: Implement do_write

    return 0;  // return the length of trully written data
}

int do_close(int fd)
{
    // TODO [P6-task2]: Implement do_close

    return 0;  // do_close succeeds
}

int do_ln(char *src_path, char *dst_path)
{
    // TODO [P6-task2]: Implement do_ln

    return 0;  // do_ln succeeds 
}

int do_rm(char *path)
{
    // TODO [P6-task2]: Implement do_rm

    return 0;  // do_rm succeeds 
}

int do_lseek(int fd, int offset, int whence)
{
    // TODO [P6-task2]: Implement do_lseek

    return 0;  // the resulting offset location from the beginning of the file
}
