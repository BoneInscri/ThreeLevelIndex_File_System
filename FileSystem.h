#ifndef __FILESYSTEM_H
#define __FILESYSTEM_H
#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <wait.h>

typedef unsigned int   uint;
typedef unsigned short ushort;
typedef unsigned char  uchar;

#define NDIRECT 2
#define FSMAGIC "10101010"

#define DISKSIZE 1024000     
#define MAX_BLOCK_SIZE 1024
#define MIN_BLOCK_SIZE 128
#define MAX_BLOCK_NUM 16000
#define MIN_BLOCK_NUM 1000

#define MAX_TEXT_SIZE  10000
#define MAX_FILENAME 8
#define MAX_OPEN_FILE 10 
#define MAX_FILE_NAME 80 

#define END 65535
#define FREE 0
#define NFREE 1

#define max(a, b) (((a) > (b)) ? (a) : (b))
#define min(a, b) (((a) < (b)) ? (a) : (b))
#define divup(x, y) ((x - 1) / (y) +1)

#define GREEN        "\033[0;32;32m"
#define RED          "\033[0;32;31m"
#define NONE         "\033[m"

#define T_DIR     1   // Directory
#define T_FILE    2   // File


#define W_truncate 1
#define W_cover 2
#define W_append 3

extern ushort BLOCKNUM;
extern ushort BLOCKSIZE;
extern ushort BMAPBLOCKNUM;
extern ushort DATABLOCKNUM;

extern ushort INDEXNUM_PER_BLOCK;


typedef struct BMAP// bit map
{
    uchar free;
}bmap;

typedef struct INODE {// Byte :8+4+4+4+3+1+1+13*2
    uchar filename[8]; // 文件名
    uint length;// 文件长度(字节数)
    uint time;  // 文件创建时间
    uint date;  // 文件创建日期
    uchar exname[3];// 文件扩展名
    uchar attribute;// 文件属性字段
    uchar free; // 表示目录项是否为空，若值为0，表示空，值为1，表示已分配
    ushort addrs[NDIRECT+3]; // three level index 三级索引
}inode;

typedef struct SUPERBLOCK {
    // 文件系统的魔数 
    uchar magic_number[9];
    // Block Size of file system
    ushort blocksize;    
    // Block Num of file system 
    ushort blocknum;      
    // Block Num of bitmap
    ushort bmapblocknum;
    // Block Num of data block 
    ushort datablocknum;
   	// 最多打开文件数
    ushort maxopenfile;
    //根目录文件的起始盘块号
    ushort root; 
    //虚拟磁盘上数据区开始位置
    uchar *startblock; 
}superblock;

typedef struct DISK{ 
    uchar* myvhard; // 虚拟磁盘的起始地址
    superblock* block0; //  超级块
    bmap* BMAP_BLOCK; // 位视图
    uchar* dataAddr; // 数据区的起始地址
}Disk;


typedef struct USEROPEN{
    //相应打开文件所在的目录名，这样方便快速检查号出指定文件是否已经打开
    uchar dir[MAX_FILE_NAME]; 
    //读写指针在文件中的位置
    uint count; 
    //相应打开文件的目录项在父目录文件中的盘块
    ushort dirno; 
    // 相应打开文件的目录项在父目录文件的dirno盘块中的目录项序号
    ushort diroff;
    // INODE
    inode open_inode;  
    //是否修改了文件的 FCB 的内容，如果修改了置为 1，否则为 0
    uchar inodestate; 
    //表示该用户打开表项是否为空，
    //若值为 0，表示为空，否则表示已被某打开文件占据
    uchar topenfile;     
}useropen;

typedef struct USERTABLE{
    useropen openfilelist[MAX_OPEN_FILE]; // 用户打开文件表数组
    int currentFd; // 当前目录的文件描述符fd
    char USERNMAE[100];// 用户名
}UserTable;

extern Disk disk;// disk
extern UserTable usertable;// Byte : 1264


// 对全局变量进行初始化
void InitVariable();
// 进入文件系统
void startsys();
// 磁盘格式化
void my_format();
// 更改当前目录
int my_cd(char *dirname);
// 创建子目录
int my_mkdir(char *dirname);
// 删除子目录
int my_rmdir(char *dirname, int flag);
// 显示目录
int my_ls(void);
// 创建文件
int my_create (char *filename);
// 删除文件
int my_rm(char *filename);
// 打开文件
int my_open(char *filename);
// 关闭文件
int my_close(int fd);
// 写文件
int my_write(int fd,int pos);
// 实际写文件
int do_write(int fd,char *text,int len,char wstyle, int pos);
// 读文件
int my_read (int fd, int pos, int len);
// 实际读文件
int do_read (int fd, int len,char *text);
// 退出文件系统
int my_exitsys();

// 打印位视图
void my_listBMAP();

// 分配一个usertable
int allocFreeusertable();

// help 界面
void my_help(); 
// Start
void printStart();
// Red
void printRed(char* s);
// pwd
void my_pwd();
// info
void showFileSystem();
// string to int
int str2int(char* s);
// string compare
int mystrcmp(char *s1,char* s2);
// is a file ?
int checkfile(char* s);
// power
int power(int a,int p);
// block_idx-> index level
int getIndexLevel(ushort block_idx);
// fcb iter 
ushort nextFcbIter(ushort*addrs, int level, ushort block_idx,ushort* l1,ushort* l2,ushort* l3);
// 分配一个block
unsigned short allocFreeBlock();
// 分配三级索引
ushort allocThreeLevelIndex(ushort*addrs, int level, ushort block_idx, ushort block_i);
// 寻找父节点
int find_father_fd(int fd);
// show 三级索引
void showThreeLevelIndex(ushort*addrs);
// 递归删除一个目录下的全部文件
void RecurDelete(inode* inode_p);


#endif 