#include <os/string.h>
#include <os/fs.h>
#include <os/mm.h>
#include <common.h>
#include <printk.h>


#define SEC_SIZE 512

static fdesc_t fdesc_array[NUM_FDESCS];
static uint8_t inode_map[INODE_NUM];
char path_buffer[128];

char name_buffer[32];

superblock_t superblock;
int current_inode;

uint8_t * block_map = (uint8_t *)BLOCK_MAP_PLACE;

inode_t * inodes = (inode_t *)INODE_PLACE;

uint32_t * indirect_pointer = (uint32_t *)TEMP;

uint32_t * indirect_pointer1 = (uint32_t *)TEMP2;

uint32_t * indirect_pointer2 = (uint32_t *)TEMP3;

//实际上最后57个块不能使用

//helper functions
inline static int get_start_sec(int offset)
{
    return superblock.fs_start_sec + offset / SEC_SIZE;
}

inline static void store_superblock(void)
{
    sd_write(kva2pa((uintptr_t)&superblock),1,get_start_sec(0));
}

inline static void load_superblock(void)
{
    sd_read(kva2pa(TEMP),1,START_SEC);
    memcpy((uint8_t*)&superblock,(uint8_t*)TEMP,sizeof(superblock_t));
}

inline static void store_inode_map(void)
{
    sd_write(kva2pa((uintptr_t)inode_map),INODE_NUM/SEC_SIZE,get_start_sec(superblock.inode_map_offset));
}

inline static void load_inode_map(void)
{
    sd_read(kva2pa((uintptr_t)inode_map),INODE_NUM/SEC_SIZE,get_start_sec(superblock.inode_map_offset));
}

inline static void store_block_map(void)
{
    sd_write(kva2pa((uintptr_t)block_map),BLOCK_MAP_SIZE/SEC_SIZE,get_start_sec(superblock.block_map_offset));
}

inline static void load_block_map(void)
{
    sd_read(kva2pa((uintptr_t)block_map),BLOCK_MAP_SIZE/SEC_SIZE,get_start_sec(superblock.block_map_offset));
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

inline static int inode_valid(int inode_num)
{
    return inode_map[inode_num];
}

//assume data block is already in TEMP
//block is the block number of the data block
inline static void store_a_data_block(int block) 
{
    sd_write(kva2pa(TEMP),BLOCK_SIZE / SEC_SIZE,get_start_sec(superblock.data_offset+block*BLOCK_SIZE));
}

inline static void store_a_data_block_from(int block,uint8_t* src)
{
    sd_write(kva2pa((uintptr_t)src),BLOCK_SIZE / SEC_SIZE,get_start_sec(superblock.data_offset+block*BLOCK_SIZE));
}

//load the block to TEMP
inline static void load_a_data_block(int block)
{
    sd_read(kva2pa(TEMP),BLOCK_SIZE / SEC_SIZE,get_start_sec(superblock.data_offset+block*BLOCK_SIZE));
}

inline static void load_a_data_block_to(int block,uint8_t* dest)
{
    sd_read(kva2pa((uintptr_t)dest),BLOCK_SIZE / SEC_SIZE,get_start_sec(superblock.data_offset+block*BLOCK_SIZE));
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
    // if (*name == '/')
    // {
    //     *name = '\0';//remove the last '/'
    // }
    while(*name!='/'&& name!=path)
        name--;
    if(*name == '/')
    {
        *name = '\0';//find the parent directory
        name++;//now name is the name of the directory to create , and path is the parent directory
        return name;
    }else
    {
        strcpy(name_buffer,name);
        path[0] = '\0';
        return name_buffer;
    }
}

int parse_path(char* path)
{
    //parse the path and return the inode number of the file
    //return -1 if the file does not exist
    int follow_inode = (path[0]=='/')?0:current_inode;
    int find=0;
    char *token = split_path(path);
    while(token!=NULL)
    {
        //now find token in follow_inode 's dentry
        dentry_t* de = (dentry_t*)TEMP;
        for(int i=0;i< 8 ;i++) //every inode contain 8 sec
        {
            if(directory_sec_is_not_used(follow_inode,i)) //no more dentry
                break;
            load_a_data_block(inodes[follow_inode].sec[i]);
            for(int j=0;j<BLOCK_SIZE/sizeof(dentry_t);j++)
            {
                if(inode_valid(de[j].inode_num) && strcmp(de[j].name,token)==0)
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
    memcpy((uint8_t*)&superblock,(uint8_t*)TEMP,sizeof(superblock_t));
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
    inode_t* root_inode =(inode_t*) INODE_PLACE; // the first inode
    //no need to add excute permission to root since it will be opened by operating system
    root_inode->mode = OWNER_READ | OWNER_WRITE | GROUP_READ | OTHER_READ;
    root_inode->size = 0;
    root_inode->parent_inode = -1;//root has no parent
    root_inode->sec[0] = 0;
    root_inode->link = 1;

    block_map[0] = 1; //block 0 is used by root inode

    //every directory has a dentry "." , every directory except root has a dentry ".."
    dentry_t* de = (dentry_t*) TEMP; 
    memset(de,0,BLOCK_SIZE);
    strcpy(de->name,".");
    de->inode_num = 0;    
    current_inode = 0;

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
    //clear file descriptor array
    memset(fdesc_array,0,sizeof(fdesc_array));
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

    char* name;
    int follow_inode;
    
    name = seperate_parent(path_buffer);
    follow_inode = parse_path(path_buffer);
    

    if (follow_inode ==-1)
    {
        return -1; //no such directory
    }
    //create the directory
    //find a free inode
    int new_inode = get_free_inode();
    inodes[new_inode].mode = OWNER_READ | OWNER_WRITE| OWNER_EXEC | GROUP_READ | GROUP_EXEC |OTHER_READ | OTHER_EXEC;
    inodes[new_inode].size = 0;
    inodes[new_inode].parent_inode = follow_inode;
    inodes[new_inode].link = 2;
    
    //now follow_inode is the place for the directory to create
    //scan if the directory already exists
    dentry_t* de =(dentry_t*) TEMP;
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
            if(de[j].name[0]==0)
            {
                //available dentry 
                strcpy(de[j].name,name);
                de[j].inode_num = new_inode;
                inodes[follow_inode].link++;
                finished = 1;
                break;
            }
            else if(strcmp(de[j].name,name)==0 && inode_valid(de[j].inode_num))
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
        inodes[follow_inode].parent_inode = follow_inode;
        inodes[follow_inode].link++;
        memset((uint8_t*)TEMP,0,BLOCK_SIZE);
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
    memset((uint8_t*)TEMP,0,BLOCK_SIZE);
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
    char* name;
    int follow_inode;
    
    name = seperate_parent(path_buffer);
    follow_inode = parse_path(path_buffer);
    
    if (follow_inode == -1)
    {
        return -1; //no such directory
    }
    //find the dentry of the directory to remove
    dentry_t* de = (dentry_t*)TEMP;
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
            if(strcmp(de[j].name,name)==0 && inode_valid(de[j].inode_num))
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
    
    if(option)
    {
        printk("        size\t  mode\t  link\t ino\t");
    }
    printk("name\n");
    for(int i=0;i<8;i++)
    {
        if(directory_sec_is_not_used(follow_inode,i))
            break;
        load_a_data_block(inodes[follow_inode].sec[i]);
        dentry_t* de = (dentry_t*)TEMP;
        for(int j=0;j<BLOCK_SIZE/sizeof(dentry_t);j++)
        {
            if(de[j].name[0]!=0 && inode_valid(de[j].inode_num))
            {
                
                if(option)
                {
                    printk("%12d\t%6x\t%6d\t%4d\t",inodes[de[j].inode_num].size,inodes[de[j].inode_num].mode,inodes[de[j].inode_num].link,de[j].inode_num);
                }
                printk("%s\n",de[j].name);
            }
        }
    }
    return 0;  // do_ls succeeds
}

int do_open(char *path, int mode)
{
    // TODO [P6-task2]: Implement do_open
    //create the file if it does not exist
    strcpy(path_buffer,path);
    char* name;
    int follow_inode;
    name = seperate_parent(path_buffer);
    follow_inode = parse_path(path_buffer);
    
    
    if (follow_inode == -1)
    {
        return -1; //no such directory
    }
    //find the dentry of the file to open
    dentry_t* de =(dentry_t*) TEMP;
    int i;
    int j;
    int finished = 0;
    int inode_to_open;
    int empty_dentry=-1;
    int empty_i=-1;
    int find_empty_dentry = 0;
    for(i=0;i<8;i++)
    {
        if(directory_sec_is_not_used(follow_inode,i))
            break;
        load_a_data_block(inodes[follow_inode].sec[i]);
        for(j=0;j<BLOCK_SIZE/sizeof(dentry_t);j++)
        {
            if(strcmp(de[j].name,name)==0 && inode_valid(de[j].inode_num))//file exists
            {
                inode_to_open = de[j].inode_num;
                finished = 1;
                break;
            }else if(de[j].name[0] == 0 && !find_empty_dentry)
            {
                empty_i = i;
                empty_dentry = j;
                find_empty_dentry = 1;
            }
        }
        if(finished)
        {
            break;
        }
    }
    if(!finished) //not find , create the file
    {
        //test if there are empty dentry to use
        if(find_empty_dentry)
        {
            load_a_data_block(inodes[follow_inode].sec[empty_i]);
            strcpy(de[empty_dentry].name,name);
            de[empty_dentry].inode_num = get_free_inode();
            inode_to_open = de[empty_dentry].inode_num;
            store_a_data_block(inodes[follow_inode].sec[empty_i]);
        }
        //else allocate a new sec
        else
        {
            inodes[follow_inode].sec[i] = get_free_block();
            memset((uint8_t*)TEMP,0,BLOCK_SIZE);
            strcpy(de[0].name,name);
            de[0].inode_num = get_free_inode();
            inode_to_open = de[0].inode_num;
            store_a_data_block(inodes[follow_inode].sec[i]);
        }
        
        inodes[inode_to_open].link += 1;
        inodes[inode_to_open].parent_inode = follow_inode;
        inodes[inode_to_open].mode = OWNER_EXEC | OWNER_READ | OWNER_WRITE | GROUP_READ | GROUP_EXEC | OTHER_READ | OTHER_EXEC;
        inodes[inode_to_open].size = 0;
    }
    
    
    store_inode_map();
    store_inodes();

    //initialize the file descriptor
    int fd;
    for(fd=0;fd<NUM_FDESCS;fd++)
    {
        if(fdesc_array[fd].ref_count == 0)
        {
            fdesc_array[fd].ref_count = 1;
            fdesc_array[fd].inode_num = inode_to_open;
            fdesc_array[fd].mode = mode;
            fdesc_array[fd].offset = 0;
            fdesc_array[fd].follow_inode = follow_inode;
            break;
        }
    }

    return fd;  // return the id of file descriptor
}

int do_read(int fd, char *buff, int length)
{
    // TODO [P6-task2]: Implement do_read
    int inode_num = fdesc_array[fd].inode_num;
    int offset = fdesc_array[fd].offset;
    int read_size = length;
    int offset_block_num = offset / BLOCK_SIZE;
    int offset_in_block = offset % BLOCK_SIZE;
    //8 block pointers in inode , the first 7 points to data block , the last points to indirect block
    int finished = 0;
    if(length >= inodes[inode_num].size - offset)
    {
        length = inodes[inode_num].size - offset > 0 ? inodes[inode_num].size - offset : 0;
        read_size = length;
    }
    while(offset_block_num < 7 && read_size > 0)
    {
        //no need to test if the block is used because the file size is already checked
        //now is direct block
        if (read_size == 0) {
            finished = 1;
            break;
        }
        load_a_data_block(inodes[inode_num].sec[offset_block_num]);
        int copy_size = (BLOCK_SIZE - offset_in_block < read_size) ? BLOCK_SIZE - offset_in_block : read_size;
        if (copy_size > 0) {
            memcpy((uint8_t*)buff, (uint8_t*)(TEMP + offset_in_block), copy_size);
        }
        read_size -= copy_size;
        buff += copy_size;
        offset_in_block = 0;
        offset_block_num++;
    }
    offset_block_num -= 7;

    if(!finished)
    {
        //indirect block
        int indirect_block = inodes[inode_num].sec[7];
        int first_level_indirect_block = offset_block_num / INDIRECT_SIZE;
        int second_level_indirect_block = offset_block_num % INDIRECT_SIZE;

        //indirect_pointer points to the first level indirect block, locate at TEMP
        load_a_data_block_to(indirect_block,(uint8_t*) indirect_pointer); // indirect_block can't be zero since read size is not zero

        int i = first_level_indirect_block; // used to control the first level indirect block
        int j = second_level_indirect_block; // used to control the second level indirect block
        int k = offset_in_block; // used to control in-block offset
        while (read_size > 0)
        {
            if (indirect_pointer[i] == 0) // first level hole
            {
                // first level hole indicates BLOCK_SIZE * INDIRECT_SIZE bytes of hole
                int hole_size = BLOCK_SIZE * INDIRECT_SIZE - k - j * BLOCK_SIZE;
                int copy_size = (read_size >= hole_size) ? hole_size : read_size;
                memset(buff, 0, copy_size);
                read_size -= copy_size;
                buff += copy_size;

                // update i, j, k
                i++;
                j = 0;
                k = 0;
                if (read_size == 0) {
                    finished = 1;
                    break;
                }
            }
            else // first level data
            {
                load_a_data_block_to(indirect_pointer[i], (uint8_t*)indirect_pointer1);
                if (indirect_pointer1[j] == 0) // second level hole
                {
                    // second level hole indicates BLOCK_SIZE bytes of hole
                    int hole_size = BLOCK_SIZE - k;
                    int copy_size = (read_size >= hole_size) ? hole_size : read_size;
                    memset(buff, 0, copy_size);
                    read_size -= copy_size;
                    buff += copy_size;

                    // update i, j, k
                    j++;
                    if (j == INDIRECT_SIZE)
                    {
                        i++;
                        j = 0;
                    }
                    k = 0;
                    if (read_size == 0) {
                        finished = 1;
                        break;
                    }
                }
                else // second level data
                {
                    load_a_data_block_to(indirect_pointer1[j], (uint8_t*)indirect_pointer2);
                    int copy_size = (read_size >= BLOCK_SIZE - k) ? BLOCK_SIZE - k : read_size;
                    memcpy((uint8_t*)buff, (uint8_t*)(indirect_pointer2) + k, copy_size);
                    read_size -= copy_size;
                    buff += copy_size;

                    // update i, j, k
                    j++;
                    if (j == INDIRECT_SIZE)
                    {
                        i++;
                        j = 0;
                    }
                    k = 0;
                    if (read_size == 0) {
                        finished = 1;
                        break;
                    }
                }
            }
        }
    }

    fdesc_array[fd].offset += length;
    return length;  // return the length of trully read data
}

int do_write(int fd, char *buff, int length)
{
    // The do_write function writes data from the buffer to the file associated with the file descriptor.
    // It returns the number of bytes actually written.
    int inode_num = fdesc_array[fd].inode_num;
    int offset = fdesc_array[fd].offset;
    int write_size = length;
    int offset_block_num = offset / BLOCK_SIZE;
    int offset_in_block = offset % BLOCK_SIZE;
    while(offset_block_num < 7)
    {
        if (write_size == 0) {
            break;
        }
        if (inodes[inode_num].sec[offset_block_num] == 0) {
            inodes[inode_num].sec[offset_block_num] = get_free_block();//don't foget to update inode
        }   
        else// Load the data block into TEMP4 to modify its content before writing back
            load_a_data_block_to(inodes[inode_num].sec[offset_block_num],(uint8_t*)TEMP4);
        int copy_size = (BLOCK_SIZE - offset_in_block < write_size) ? BLOCK_SIZE - offset_in_block : write_size;
        memcpy((uint8_t*)(TEMP4 + offset_in_block), (uint8_t*)buff, copy_size);
        store_a_data_block_from(inodes[inode_num].sec[offset_block_num], (uint8_t*)TEMP4);
        write_size -= copy_size;
        buff += copy_size;
        offset_in_block = 0;
        offset_block_num++;
    }

    offset_block_num -= 7;

    if (write_size > 0)
    {
        //indirect block
        int indirect_block = inodes[inode_num].sec[7];
        int first_level_indirect_block = offset_block_num / INDIRECT_SIZE;
        int second_level_indirect_block = offset_block_num % INDIRECT_SIZE;

        //indirect_pointer points to the first level indirect block, locate at TEMP
        memset(indirect_pointer, 0, BLOCK_SIZE);
        if (indirect_block == 0) {
            inodes[inode_num].sec[7] = indirect_block = get_free_block();//don't modify here or you will be punished
            inodes[inode_num].sec[7] = indirect_block;
        }else
            load_a_data_block_to(indirect_block,(uint8_t*) indirect_pointer);

        int i = first_level_indirect_block; // used to control the first level indirect block
        int j = second_level_indirect_block; // used to control the second level indirect block
        int k = offset_in_block; // used to control in-block offset
        while (write_size > 0)
        {
            if (indirect_pointer[i] == 0) // first level hole
            {
                // alloc a new second level indirect block
                indirect_pointer[i] = get_free_block();
                memset(indirect_pointer1, 0, BLOCK_SIZE);
            }
            else // first level indirect block
            {
                load_a_data_block_to(indirect_pointer[i],(uint8_t*) indirect_pointer1);
            }
            if(indirect_pointer1[j] == 0) // second level hole
            {
                // alloc a new data block
                indirect_pointer1[j] = get_free_block();
                memset(indirect_pointer2, 0, BLOCK_SIZE);
            }
            else // second level indirect block
            {
                load_a_data_block_to(indirect_pointer1[j], (uint8_t*)indirect_pointer2);
            }
            int copy_size = (BLOCK_SIZE - k < write_size) ? BLOCK_SIZE - k : write_size;
            memcpy((uint8_t*)(indirect_pointer2) + k, (uint8_t*)buff, copy_size);
            store_a_data_block_from(indirect_pointer1[j], (uint8_t*)indirect_pointer2);
            write_size -= copy_size;
            buff += copy_size;
            k=0;
            j++;
            if (j == INDIRECT_SIZE)
            {
                j = 0;
                store_a_data_block_from(indirect_pointer[i], (uint8_t*)indirect_pointer1);
                i++;
            }
        }
        store_a_data_block_from(indirect_pointer[i], (uint8_t*)indirect_pointer1);
        store_a_data_block_from(inodes[inode_num].sec[7],(uint8_t*)indirect_pointer);
    }

    int add_size;
    if (offset + length > inodes[inode_num].size) {
        add_size = offset + length - inodes[inode_num].size;
        //follow back to all it parent directory to update the size
        int it = inode_num;
        do
        {
            inodes[it].size += add_size;
            it = inodes[it].parent_inode;
        } while (it != -1);
    }
    fdesc_array[fd].offset += length;
    store_block_map();
    store_inodes();
    return length;  // return the length of trully written data
}

int do_close(int fd)
{
    // TODO [P6-task2]: Implement do_close
    fdesc_array[fd].ref_count--;
    return 0;  // do_close succeeds
}

int do_ln(char *src_path, char *dst_path)
{
    // TODO [P6-task2]: Implement do_ln
    //create link at dst_path to src_path
    strcpy(path_buffer,src_path);
    char* name;
    int follow_inode;

    name = seperate_parent(path_buffer);
    follow_inode = parse_path(path_buffer);

    if (follow_inode == -1)
    {
        return -1; //no such directory
    }
    //find the dentry of the file to link
    dentry_t* de = (dentry_t*)TEMP;
    int i;
    int j;
    int finished = 0;
    int inode_to_link;
    for(i=0;i<8;i++)
    {
        if(directory_sec_is_not_used(follow_inode,i))
            break;
        load_a_data_block(inodes[follow_inode].sec[i]);
        for(j=0;j<BLOCK_SIZE/sizeof(dentry_t);j++)
        {
            if(strcmp(de[j].name,name)==0 && inode_valid(de[j].inode_num))
            {
                inode_to_link = de[j].inode_num;
                finished = 1;
                break;
            }
        }
        if(finished)
        {
            break;
        }
    }
    if(!finished)
    {
        return -2; //no such file
    }
    //now inode_to_link is the inode of the file to link
    //now find the parent directory of the link
    strcpy(path_buffer,dst_path);
    name = seperate_parent(path_buffer);
    follow_inode = parse_path(path_buffer);
    if (follow_inode == -1)
    {
        return -3; //no such directory
    }
    //create the link
    //find a free dentry
    dentry_t* de1 = (dentry_t*)TEMP;
    int k;
    int l;
    int find_empty_dentry = 0;
    int empty_dentry = -1;
    int empty_sec = -1;
    for(k=0;k<8;k++)
    {
        if(directory_sec_is_not_used(follow_inode,k))
            break;
        load_a_data_block(inodes[follow_inode].sec[k]);
        for(l=0;l<BLOCK_SIZE/sizeof(dentry_t);l++)
        {
            if(de1[l].name[0] == 0 && !find_empty_dentry)
            {
                empty_sec = k;
                empty_dentry = l;
                find_empty_dentry = 1;
            }
            else if(strcmp(de1[l].name,name)==0 && inode_valid(de1[l].inode_num))
            {
                return -4; //link already exists
            }
        }
        if(find_empty_dentry)
        {
            break;
        }
    }
    if(!find_empty_dentry)
    {
        //no available dentry
        //create a new sec 
        inodes[follow_inode].sec[k] = get_free_block();
        inodes[follow_inode].link++;
        memset((void*)TEMP,0,BLOCK_SIZE);
        strcpy(de1[0].name,name);
        de1[0].inode_num = inode_to_link;
    }else//find a empty dentry
    {
        k=empty_sec;
        load_a_data_block(inodes[follow_inode].sec[k]);
        strcpy(de1[empty_dentry].name,name);
        de1[empty_dentry].inode_num = inode_to_link;
    }
    inodes[inode_to_link].link++;
    store_a_data_block(inodes[follow_inode].sec[k]);

    store_inode_map();
    store_inodes();
    store_block_map();
    return 0;  // do_ln succeeds 
}

int do_rm(char *path)
{
    // TODO [P6-task2]: Implement do_rm
    char* name;
    int follow_inode;

    name = seperate_parent(path);
    follow_inode = parse_path(path);

    if (follow_inode == -1)
    {
        return -1; //no such directory
    }

    //find the dentry of the file to remove
    dentry_t* de = (dentry_t*)TEMP;
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
            if(strcmp(de[j].name,name)==0 && inode_valid(de[j].inode_num))
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
    //no such file is ok
    store_a_data_block(inodes[follow_inode].sec[i]);
    inodes[inode_to_remove].link--;
    if(inodes[inode_to_remove].link <=0)
    {
        inode_map[inode_to_remove] = 0;
        for(int i=0;i<7;i++)
        {
            if(inodes[inode_to_remove].sec[i]!=0)
               block_map[inodes[inode_to_remove].sec[i]] = 0;
        }

        //remove the indirect block
        if(inodes[inode_to_remove].sec[7]!=0)
        {
            load_a_data_block(inodes[inode_to_remove].sec[7]);
            for(int i=0;i<INDIRECT_SIZE;i++)
            {
                if(indirect_pointer[i]!=0)
                {
                    load_a_data_block_to(indirect_pointer[i],(uint8_t*) indirect_pointer1);
                    for(int j=0;j<INDIRECT_SIZE;j++)
                    {
                        if(indirect_pointer1[j]!=0)
                        {
                            block_map[indirect_pointer1[j]] = 0;
                        }
                    }
                    block_map[indirect_pointer[i]] = 0;
                }
            }
            block_map[inodes[inode_to_remove].sec[7]] = 0;
        }
        int sz = inodes[inode_to_remove].size;
        int it = inodes[inode_to_remove].parent_inode;
        while(it != -1)
        {
            inodes[it].size -= sz;
            it = inodes[it].parent_inode;
        }
    }
    

    store_inode_map();
    store_inodes();
    store_block_map();
    return 0;  // do_rm succeeds 
}

int do_lseek(int fd, int offset, int whence)
{
    // TODO [P6-task2]: Implement do_lseek
    if(whence == SEEK_SET)
    {
        fdesc_array[fd].offset = offset;
    }
    else if(whence == SEEK_CUR)
    {
        fdesc_array[fd].offset += offset;
    }
    else if(whence == SEEK_END)
    {
        fdesc_array[fd].offset = inodes[fdesc_array[fd].inode_num].size + offset;
    }
    return 0;  // the resulting offset location from the beginning of the file
}
