#include "FileSystem.h"


ushort BLOCKSIZE;// default : 1024
ushort BLOCKNUM; // default : SIZE/1024
ushort BMAPBLOCKNUM;// default : 1
ushort DATABLOCKNUM;// default : 1000-1-1=998
ushort INDEXNUM_PER_BLOCK; // default : 512

char USERNMAE[]="20051124-hdbone"; 
char myfsys_name[]="myfsys"; 

Disk disk;
UserTable usertable;


void InitVariable()
{
    BLOCKSIZE=1024;
    BLOCKNUM=DISKSIZE/BLOCKSIZE;
    BMAPBLOCKNUM=divup(BLOCKNUM*sizeof(ushort),BLOCKSIZE);
    DATABLOCKNUM=BLOCKNUM-1-BMAPBLOCKNUM;
    INDEXNUM_PER_BLOCK=BLOCKSIZE/sizeof(ushort);
}


// 由 main()函数调用,进入并初始化我们所建立的文件系统,以供用户使用。
void startsys()
{
    // 申请虚拟磁盘空间
    disk.myvhard=(uchar*)malloc(DISKSIZE);
    memset(disk.myvhard,0,DISKSIZE);// !!!
    // fopen()打开 myfsys 文件
    FILE* fp;
    fp=fopen(myfsys_name,"rb");

    if(fp!=NULL)// 若文件存在
    {
        uchar* buf=(uchar*)malloc(DISKSIZE);
        int fd=open(myfsys_name,O_RDONLY);
        read(fd, buf, DISKSIZE);
        uchar magic[8];
        strncpy(magic,buf,8);
        if(!memcmp(magic,FSMAGIC,8))
        {
            memcpy(disk.myvhard,buf,DISKSIZE);

            disk.block0=(superblock*)(disk.myvhard);
            BLOCKNUM=disk.block0->blocknum;
            BLOCKSIZE=disk.block0->blocksize;
            BMAPBLOCKNUM=disk.block0->bmapblocknum;
            DATABLOCKNUM=disk.block0->datablocknum;

            disk.BMAP_BLOCK=(bmap*)(disk.myvhard+BLOCKSIZE);
            disk.dataAddr=(uchar*)(disk.myvhard+BLOCKSIZE*(BMAPBLOCKNUM+1));
            // 复制到内存中的虚拟磁盘空间中
            printf("读取myfys文件系统成功\n\n");       
            inode* root=(inode*)(disk.myvhard+BLOCKSIZE*(BMAPBLOCKNUM+1));
        }
        printf("文件系统空间大小为: %d B\n",DISKSIZE);
        printf("磁盘块大小为: %d B\n",BLOCKSIZE);
        printf("磁盘块数量为: %d 块\n",BLOCKNUM);
        printf("是否需要更改磁盘块大小?(y/n): \n");
        printRed("更改磁盘块大小会格式化磁盘空间!");
        char ch[20];
        scanf("%s",ch);
        while(strcmp(ch,"y")&&strcmp(ch,"n"))
        {
            printf("error, y or n\n");
            scanf("%s",ch);
        }
        if(!strcmp(ch,"y"))
        {
            my_format();
            fwrite(disk.myvhard,DISKSIZE,1,fp);
        }else{
            
        }
    }
    else{// 若文件不存在,则创建之
        printf("myfsys 文件系统不存在\n");
        my_format();
        fp=fopen(myfsys_name,"wb");
        fwrite(disk.myvhard,DISKSIZE,1,fp);
    }
    fclose(fp);
    
    // usertable init
    usertable.currentFd=0;
    memcpy(usertable.USERNMAE,USERNMAE,strlen(USERNMAE));
    inode* root=(inode*)(disk.myvhard+(1+BMAPBLOCKNUM)*BLOCKSIZE);
    memcpy(&usertable.openfilelist[0].open_inode,root,sizeof(inode));

    usertable.openfilelist[0].count=0;
    strcpy(usertable.openfilelist[0].dir,"/");
    usertable.openfilelist[0].dirno=1+BMAPBLOCKNUM;
    usertable.openfilelist[0].diroff=0;
    usertable.openfilelist[0].inodestate=0;
    usertable.openfilelist[0].topenfile=1;

    INDEXNUM_PER_BLOCK=BLOCKSIZE/sizeof(ushort);// !!! index num per block
}

// 改变当前目录到指定的名为 dirname 的目录。
int my_cd(char *dirname)
{
    /*
        dirname：新的当前目录的目录名
    */
   /*
        ① 调用 my_open()打开指定目录名的父目录文件，并调用 do_read()读入该父目录文件内容到内存中；
        ② 在父目录文件中检查新的当前目录名是否存在，如果存在则转③，否则返回，并显示出错信息；
        ③ 调用 my_close()关闭①中打开的父目录文件；
        ④ 调用 my_close()关闭原当前目录文件；
        ⑤ 如果新的当前目录文件没有打开，则打开该目录文件
        ⑥ 设置当前目录为该目录。
   */
    int currentfd=usertable.currentFd;

    char *buf=(char*)malloc(MAX_TEXT_SIZE);
    usertable.openfilelist[currentfd].count=0;
    int r_len_fa=do_read(currentfd,usertable.openfilelist[currentfd].open_inode.length,buf);
    if(r_len_fa<0)
    {
        printf("read error\n");
        return -1;
    }    
    int off=-1;
    inode* inode_p=(inode*)buf;
    for(int i=0;i<usertable.openfilelist[currentfd].open_inode.length/sizeof(inode);i++,inode_p++)
    {
        if(!strcmp(inode_p->filename,dirname)&&inode_p->attribute==T_DIR)
        {
            off=i;
            break;
        }
    }   
    if(off<0)
    {
        printRed("cd : this directory doesn't exist\n");
        return -1;
    }
    if(memcmp(inode_p->exname,"DIR",3))
    {
        printRed("just allow to cd to a directory\n");
        return -1;
    }

    if(!strcmp(inode_p->filename,"."))// cd itself
    {
        return currentfd;
    }
    else if(!strcmp(inode_p->filename,".."))
    {
        if(currentfd==0)// root
        {
            return currentfd;
        }else{
            usertable.currentFd=my_close(currentfd);// return to its father directory
            return usertable.currentFd;
        }
    }else{
        int fd_new=allocFreeusertable();
        if(fd_new<0)
        {
            printf("the useropentable is full\n");
            return -1;
        }
        memcpy(&usertable.openfilelist[fd_new].open_inode,inode_p,sizeof(inode));
        usertable.openfilelist[fd_new].inodestate=0;
        usertable.openfilelist[fd_new].topenfile=1;
        usertable.openfilelist[fd_new].count=0;

        // dirname -> dirname_abs
        char dirname_abs[MAX_FILE_NAME];
        memset(dirname_abs,0,sizeof(dirname_abs));
        strcpy(dirname_abs,usertable.openfilelist[currentfd].dir);
        if(currentfd!=0)
            dirname_abs[strlen(dirname_abs)]='/';
        dirname_abs[strlen(dirname_abs)]='\0';
        strcat(dirname_abs,dirname);
        dirname_abs[strlen(dirname_abs)]='\0';
        // copy dirname_abs
        strcpy(usertable.openfilelist[fd_new].dir,dirname_abs);
        usertable.openfilelist[fd_new].dirno=usertable.openfilelist[currentfd].open_inode.addrs[0];
        usertable.openfilelist[fd_new].diroff=off;
        
        usertable.currentFd=fd_new;
        return fd_new;
    }
}


//关闭前面由 my_open()打开的文件描述符为 fd 的文件。
int my_close(int fd)
{
    // 检查 fd 的有效性
    if(fd>=MAX_OPEN_FILE || fd<0)
    {
        printf("文件无效\n");
        return -1;
    }

    int father_fd=find_father_fd(fd);
    if(father_fd==-1)
    {
        printf("error, father directory doesn't exist\n");
        return -1;
    }
    if(usertable.openfilelist[fd].inodestate==1)
    {
        char buf[MAX_TEXT_SIZE];
        // father->buf
        usertable.openfilelist[father_fd].count=0;// !!!
        int len_r=do_read(father_fd,usertable.openfilelist[father_fd].open_inode.length,buf);
        if(len_r<0)
        {
            printf("error, read fail\n");
            return -1;
        }
        // inode_fd update
        int off=usertable.openfilelist[fd].diroff;
        inode* inode_fd=(inode*)(buf+sizeof(inode)*off);
        memcpy(inode_fd,&usertable.openfilelist[fd].open_inode,sizeof(inode));
        int pos=off*sizeof(inode);
        usertable.openfilelist[father_fd].count=pos;
        int len_w=do_write(father_fd,(char*)inode_fd,sizeof(inode),W_cover,off);
        
        if(len_w<0)
        {
            printf("error, write fail\n");
            return -1;
        }
    }
    memset(&usertable.openfilelist[fd],0,sizeof(useropen));
    usertable.currentFd=father_fd;
    return father_fd;
}

// 删除名为 filename 的文件
int my_rm(char *filename)
{
    /*
        ① 若欲删除文件的父目录文件还没有打开，则调用 my_open()打开；若打开失败，则
        返回，并显示错误信息；
        ② 调用 do_read()读出该父目录文件内容到内存，检查该目录下欲删除文件是否存在，
        若不存在则返回，并显示错误信息；
        ③ 检查该文件是否已经打开，若已打开则关闭掉；
        ④ 回收该文件所占据的磁盘块，修改 FAT；
        ⑤ 从文件的父目录文件中清空该文件的目录项，且 free 字段置为 0：以覆盖写方式调
        用 do_write()来实现；；
        ⑥ 修改该父目录文件的用户打开文件表项中的长度信息，并将该表项中的 fcbstate
        置为 1；
    */
    int currentfd=usertable.currentFd;
    // abs path
    char filename_abs[MAX_FILE_NAME];
    memset(filename_abs,0,sizeof(filename_abs));
    strcpy(filename_abs,usertable.openfilelist[currentfd].dir);
    if(currentfd!=0)
        filename_abs[strlen(filename_abs)]='/';
    strcat(filename_abs,filename);
    filename_abs[strlen(filename_abs)]='\0';
    for(int i=0;i<MAX_OPEN_FILE;i++)
    {
        if(!strcmp(usertable.openfilelist[i].dir,filename_abs))
        {
            printRed("file has not closed, you can use close command to close a open file\n");
            return -1;
        }
    }


    char* fname=strtok(filename,".");
    char* exname=strtok(NULL,".");
    if(!strcmp(fname,""))
    {
        printRed("filename can not start with . \n");
        return -1;
    }
    if(!exname)
    {
        printRed("without exname\n");
        return -1;
    }
    
    // read current directory
    char buf[10000];
    usertable.openfilelist[currentfd].count=0;
    int r_len_cur=do_read(currentfd,usertable.openfilelist[currentfd].open_inode.length,buf);
    if(r_len_cur<0)
    {
        printRed("read error\n");
        return -1;
    }
    // existed ?
    inode* inode_p=(inode*)buf;

    int off=-1;
    for(int i=0;i<usertable.openfilelist[currentfd].open_inode.length/sizeof(inode);i++,inode_p++)
    {

        if(!memcmp(inode_p->filename,fname,strlen(fname))&&!memcmp(inode_p->exname,exname,3))
        {
            off=i;
            break;
        }
    }
    if(off<0)
    {
        printRed("this file does not exist\n");
        return -1;
    }

    // free  

    ushort* addrs=inode_p->addrs;
    ushort level=0;
    ushort block_idx=0;// !!! 

    level=getIndexLevel(block_idx);
    ushort l1=0;
    ushort l2=0;
    ushort l3=0;

    ushort inode_iter=nextFcbIter(addrs,level,block_idx,&l1,&l2,&l3);
    
    while(inode_iter!=FREE)
    {
        allocThreeLevelIndex(addrs,level,block_idx,FREE);
        block_idx++;
        level=getIndexLevel(block_idx);
        inode_iter=nextFcbIter(addrs,level,block_idx,&l1,&l2,&l3);
    }
    


    int len_current=usertable.openfilelist[currentfd].open_inode.length;
    /*begin 消除碎片*/
    char buffer[MAX_TEXT_SIZE];
    usertable.openfilelist[currentfd].count=(off+1)*sizeof(inode);
    int len_fragment=len_current-(off+1)*sizeof(inode);


    r_len_cur=do_read(currentfd,len_fragment,buffer);
    if(r_len_cur<0)
    {
        printRed("read error 2\n");
        return -1;
    }

    usertable.openfilelist[currentfd].count=off*sizeof(inode);
    
    int w_len_cur=do_write(currentfd,(char*)buffer,len_fragment,W_cover,off*sizeof(inode));
    if(r_len_cur<0)
    {
        printRed("read error 3\n");
        return -1;
    }
    /*end 消除碎片*/

    // free inode
    inode_p->attribute=0;
    inode_p->date=0;
    inode_p->time=0;
    inode_p->exname[0]='\0';
    inode_p->filename[0]='\0';
    memset(inode_p->addrs,0,sizeof(inode_p->addrs));
    inode_p->free=0;
    inode_p->length=0;
    // inode->block
    usertable.openfilelist[currentfd].count=len_current-sizeof(inode);
    w_len_cur=do_write(currentfd,(char*)inode_p,sizeof(inode),W_cover,len_current-sizeof(inode));
    // length - = sizeof(inode)
    usertable.openfilelist[currentfd].open_inode.length-=sizeof(inode);


    usertable.openfilelist[currentfd].count=0;
    r_len_cur=do_read(currentfd,usertable.openfilelist[currentfd].open_inode.length,buffer);
    if(r_len_cur<0)
    {
        printRed("read error\n");
        return -1;
    }
    // bmap move reconstruction
    addrs=usertable.openfilelist[currentfd].open_inode.addrs;
    ushort first=addrs[0];
    inode_iter=first;
    level=0;
    block_idx=0;// !!! 
    block_idx++;
    level=getIndexLevel(block_idx);

    ushort next=nextFcbIter(addrs,level,block_idx,&l1,&l2,&l3);
    int cnt_iter=2*sizeof(inode);
    if(r_len_cur/sizeof(inode)==2){
        if(next!=FREE)
        {
            allocThreeLevelIndex(addrs,level,block_idx,FREE);
        }          
    }
    inode_iter=next;

    for(int i=2;i<r_len_cur/sizeof(inode);i++)
    {
        cnt_iter+=sizeof(inode);
        if(cnt_iter>BLOCKSIZE)
        {
            block_idx++;
            level=getIndexLevel(block_idx);
            next=nextFcbIter(addrs,level,block_idx,&l1,&l2,&l3);

            cnt_iter-=BLOCKSIZE;
            inode_iter=next;
            
            ushort check=nextFcbIter(addrs,getIndexLevel(block_idx+1),block_idx+1,&l1,&l2,&l3);

            if(check==FREE)
            {
                allocThreeLevelIndex(addrs,getIndexLevel(block_idx),block_idx,FREE);
            }
        }
    }

    // current -> block (update length)
    inode_p=(inode*)buffer;
    inode_p->length=usertable.openfilelist[currentfd].open_inode.length;
    memcpy(inode_p->addrs,usertable.openfilelist[currentfd].open_inode.addrs,sizeof(inode_p->addrs));//!!!!!!!!!!!!!!
    if(currentfd==0){
        inode* inode_p_2=inode_p+1;
        inode_p_2->length=usertable.openfilelist[currentfd].open_inode.length;    
        memcpy(inode_p_2->addrs,usertable.openfilelist[currentfd].open_inode.addrs,sizeof(inode_p->addrs));//!!!!!!!!!!!!!!
        usertable.openfilelist[currentfd].count=sizeof(inode);
        int w_len_cur=do_write(currentfd,(char*)inode_p_2,sizeof(inode),W_cover,sizeof(inode));
    }

    usertable.openfilelist[currentfd].count=0;
    w_len_cur=do_write(currentfd,(char*)inode_p,sizeof(inode),W_cover,0);
    usertable.openfilelist[currentfd].inodestate=1;

    return 1;
}

// 对虚拟磁盘进行格式化,布局虚拟磁盘,建立根目录文件(或根目录区)
void my_format()
{
    printf("输入磁盘块大小(B):%d ~ %d\n",MIN_BLOCK_SIZE,MAX_BLOCK_SIZE);
    while(1)
    {
        scanf("%hd",&BLOCKSIZE);
        if(BLOCKSIZE>=MIN_BLOCK_SIZE&&BLOCKSIZE<=MAX_BLOCK_SIZE&&DISKSIZE%BLOCKSIZE==0)
        {
            break;
        }else{
            printf("error, try again\n");
        }
    }

    BLOCKNUM=DISKSIZE/BLOCKSIZE;
    BMAPBLOCKNUM=divup(BLOCKNUM*sizeof(ushort),BLOCKSIZE);
    DATABLOCKNUM=BLOCKNUM-1-BMAPBLOCKNUM;
    

    // 将虚拟磁盘第一个块作为引导块
    disk.block0=(superblock*)disk.myvhard;
    // 开始的 8 个字节是文件系统的魔数,记为“10101010”
    memcpy(disk.block0->magic_number, FSMAGIC,8);

    
    disk.block0->root=1+BMAPBLOCKNUM+1;
    disk.block0->blocknum=BLOCKNUM;
    disk.block0->blocksize=BLOCKSIZE;
    disk.block0->datablocknum=DATABLOCKNUM;
    disk.block0->bmapblocknum=BMAPBLOCKNUM;
    disk.block0->startblock=disk.myvhard + BLOCKSIZE * (1+BMAPBLOCKNUM);
    disk.block0->maxopenfile=MAX_OPEN_FILE;


    disk.BMAP_BLOCK=(bmap*)(disk.myvhard+BLOCKSIZE*1);
    disk.dataAddr=(uchar*)(disk.myvhard+BLOCKSIZE*(BMAPBLOCKNUM+1));
    
    for(int i=0;i<1+BMAPBLOCKNUM;i++)
        disk.BMAP_BLOCK[i].free=NFREE;

    // 创建根目录文件 root,将数据区的第 1 块
    // 分配给根目录文件,在该磁盘上创建两个特殊的目录项:“.”和“..”,其内容除了文件
    // 名不同之外,其他字段完全相同
    
    disk.BMAP_BLOCK[BMAPBLOCKNUM+1].free=FREE;
    

    inode* root1=(inode*)(disk.myvhard+(1+BMAPBLOCKNUM)*BLOCKSIZE);
    strcpy(root1->filename,".");
    memcpy(root1->exname,"DIR",3);
    root1->attribute=T_DIR;

    ushort bmap_new=allocFreeBlock();

    root1->addrs[0]=bmap_new;// !!!
    root1->free=1;
    root1->length=2*sizeof(inode);

    // time
	time_t rawTime = time(NULL);
	struct tm *time = localtime(&rawTime);
    // date : 2000-01-01 ~ 2127-12-31
    // time : 00-00-00 ~ 23-59-59
    root1->date=((time->tm_year-100)<<9)+((time->tm_mon+1)<<5)+(time->tm_mday);
    root1->time=((time->tm_hour)<<12)+((time->tm_min)<<6)+(time->tm_sec);
    inode* root2=root1+1;
    memcpy(root2,root1,sizeof(inode));
    strcpy(root2->filename,"..");

}


// 显示当前目录的内容（子目录和文件信息）
int my_ls(void)
{
    /*
        ① 调用 do_read()读出当前目录文件内容到内存；
        ② 将读出的目录文件的信息按照一定的格式显示到屏幕上；
        ③ 返回。
    */
    int currentfd=usertable.currentFd;
    char buf[MAX_TEXT_SIZE];
    usertable.openfilelist[currentfd].count=0;
    int r_len_cur=do_read(currentfd,usertable.openfilelist[currentfd].open_inode.length,buf);

    if(r_len_cur<0)
    {
        printRed("my_ls read error\n");
        return -1;
    }
    
    inode* inode_p=(inode*)buf;
    for(int i=0;i<usertable.openfilelist[currentfd].open_inode.length/sizeof(inode);i++)
    {
        // printf("%d %s %d\n",i,inode_p[i].filename,inode_p[i].free);
        if(inode_p[i].free==1)
        {
            ushort year=(inode_p[i].date>>9)+2000;
            ushort month=(inode_p[i].date>>5)&0x000f;
            ushort day=(inode_p[i].date)&0x001f;
            ushort hour =(inode_p[i].time>>12);
            ushort minute =(inode_p[i].time>>6)&0x003f;
            ushort sencond =(inode_p[i].time)&0x003f;
            // root1->date=(time->tm_year-100)<<9+(time->tm_mon+1)<<5+(time->tm_mday);
            // root1->time=(time->tm_hour)<<12+(time->tm_min)<<6+(time->tm_sec);
            char exname_tmp[4];
            if(inode_p[i].attribute==T_DIR)
            {
                printf("%-8s\t<DIR>\t%ld\t%02d-%02d-%02d %02d.%02d.%02d\n",inode_p[i].filename,inode_p[i].length/sizeof(inode)-2,
                        year,month,day,hour,minute,sencond);
            }else if(inode_p[i].attribute==T_FILE)
            {
                memcpy(exname_tmp,inode_p[i].exname,3);
                exname_tmp[3]='\0';

                // check open ?
                // abs path
                char filename_abs[MAX_FILE_NAME];
                memset(filename_abs,0,sizeof(filename_abs));
                strcpy(filename_abs,usertable.openfilelist[currentfd].dir);
                if(currentfd!=0)
                    filename_abs[strlen(filename_abs)]='/';
                strcat(filename_abs,inode_p[i].filename);
                filename_abs[strlen(filename_abs)]='.';
                strcat(filename_abs,exname_tmp);
                filename_abs[strlen(filename_abs)]='\0';
                // dirname -> fd
                // find fd! 
                int fd=-1;
                for(int i=0;i<MAX_OPEN_FILE;i++)
                if(usertable.openfilelist[i].topenfile==1)
                    if(!strcmp(usertable.openfilelist[i].dir,filename_abs))
                        fd=i;
                
                if(fd!=-1)
                {
                    if(usertable.openfilelist[fd].inodestate)
                        printf(GREEN"%-8s\t<%3s>\t%dB\t%02d-%02d-%02d %02d.%02d.%02d open(need close to save)"NONE,inode_p[i].filename,exname_tmp,inode_p[i].length,
                        year,month,day,hour,minute,sencond);
                    else
                        printf(GREEN"%-8s\t<%3s>\t%dB\t%02d-%02d-%02d %02d.%02d.%02d open"NONE,inode_p[i].filename,exname_tmp,inode_p[i].length,
                        year,month,day,hour,minute,sencond);
                    
                    printf("\n");
                }
                else
                    printf("%-8s\t<%3s>\t%dB\t%02d-%02d-%02d %02d.%02d.%02d\n",inode_p[i].filename,exname_tmp,inode_p[i].length,
                        year,month,day,hour,minute,sencond);
            }
        }
    }
    return 1;
}

// 创建名为 filename 的新文件
int my_create (char *filename)
{
    int currentfd=usertable.currentFd;

    char* fname = strtok(filename, ".");
	char* exname = strtok(NULL, ".");
    if(!strcmp(fname,""))
    {
        printRed("filename can not start with . \n");
        return -1;
    }
    if(!exname)
    {
        printRed("without exname\n");
        return -1;
    }

    char buf[MAX_TEXT_SIZE];
    usertable.openfilelist[currentfd].count=0;
    int r_len_fa=do_read(currentfd,usertable.openfilelist[currentfd].open_inode.length,buf);
    if(r_len_fa<0)
    {
        printRed("read error\n");
        return -1;
    }

    inode* inode_p=(inode*)buf;
    for(int i=0;i<r_len_fa/sizeof(inode);i++)
    {
        if(!memcmp(inode_p[i].filename,fname,strlen(fname))&&!memcmp(inode_p[i].exname,exname,3))
        {
            printRed("this file has existed\n");
            return -1;
        }
    }

    // fat
    ushort bmap_new=allocFreeBlock();
    if(bmap_new==END)
    {
        printRed("error, disk is full\n");
        return -1;
    }

    int off=0;
    for(int i=0;i<r_len_fa/sizeof(inode);i++)
    {
        off=i+1;
        if(inode_p[i].free==0)// !!! 内部可能有碎片
        {
            off=i;
            break;
        }
    }

    inode* inode_new=(inode*)malloc(sizeof(inode));
    inode_new->attribute=T_FILE;

    time_t rawtime = time(NULL);
    struct tm* time=localtime(&rawtime);
    // TODO : time and date
    inode_new->date=((time->tm_year-100)<<9)+((time->tm_mon+1)<<5)+(time->tm_mday);
    inode_new->time=((time->tm_hour)<<12)+((time->tm_min)<<6)+(time->tm_sec);
    strcpy(inode_new->filename,filename);
    memcpy(inode_new->exname,exname,3);// 
    inode_new->addrs[0]=bmap_new;
    inode_new->length=0;// !!!
    inode_new->free=1;


    int count=off*sizeof(inode);
    usertable.openfilelist[currentfd].count=count;
    int w_len=do_write(currentfd,(char *)inode_new,sizeof(inode),W_cover,count);
    usertable.openfilelist[currentfd].inodestate=1;

    if(w_len<0)
    {
        printf("write error\n");
        return -1;
    }

    // currentfd inode update
    inode_p=(inode*)buf;
    
    inode_p->length=usertable.openfilelist[currentfd].open_inode.length;
    memcpy(inode_p->addrs,usertable.openfilelist[currentfd].open_inode.addrs,sizeof(inode_p->addrs));//!!!!!!!!!!!!!!
    if(currentfd==0){
        inode* inode_p_2=inode_p+1;
        inode_p_2->length=usertable.openfilelist[currentfd].open_inode.length;    
        memcpy(inode_p_2->addrs,usertable.openfilelist[currentfd].open_inode.addrs,sizeof(inode_p->addrs));//!!!!!!!!!!!!!!
        usertable.openfilelist[currentfd].count=sizeof(inode);
        int w_len_cur=do_write(currentfd,(char*)inode_p_2,sizeof(inode),W_cover,sizeof(inode));
    }
    
    usertable.openfilelist[currentfd].count=0;
    int w_len_cur=do_write(currentfd,(char*)inode_p,sizeof(inode),W_cover,0);
    
    return bmap_new;
}


// 打开当前目录下名为 filename 的文件
int my_open(char *filename)
{
    /*
        1 检查该文件是否已经打开,若已打开则返回-1,并显示错误信息;
        2 调用 do_read()读出父目录文件的内容到内存,检查该目录下欲打开文件是否存在,
            若不存在则返回-1,并显示错误信息;
        3 检查用户打开文件表中是否有空表项,若有则为欲打开文件分配一个空表项,
            若没有则返回-1,并显示错误信息;
        4 为该文件填写空白用户打开文件表表项内容,读写指针置为 0;
        5 将该文件所分配到的空白用户打开文件表表项序号(数组下标)作为文件描述符 fd 返回。
    */
    int fd=usertable.currentFd;
        char filename_abs[MAX_FILE_NAME];
    strcpy(filename_abs,usertable.openfilelist[fd].dir);
    if(fd!=0)
        filename_abs[strlen(filename_abs)]='/';
    strcat(filename_abs,filename);
    filename_abs[strlen(filename_abs)]='\0';

    char* filename_tmp=strtok(filename,".");
    char* exname=strtok(NULL,".");
    if(exname==NULL)
    {
        printRed("no exname\n");
        return -1;
    }

    int flag=0;
    for(int i=0;i<MAX_OPEN_FILE;i++)
    {
        if(!strcmp(usertable.openfilelist[i].dir,filename_abs))
        {
            printRed("this file has open\n");
            return -1;
        }
    }

    char buf[10000];
    usertable.openfilelist[fd].count=0;
    int len_r_fa=do_read(fd,usertable.openfilelist[fd].open_inode.length,buf);
    // father memory -> buf
    int off=-1;
    inode* inode_p=(inode*)buf;
    for(int i=0;i<usertable.openfilelist[fd].open_inode.length/sizeof(inode);i++,inode_p++)
    {
        if(!memcmp(inode_p->filename,filename_tmp,strlen(filename_tmp))&&!memcmp(inode_p->exname,exname,3)&&inode_p->attribute==T_FILE)
        {
            off=i;
            break;
        }
    }
    if(off<0)
    {
        printf("this file doesn't exist\n");
        return -1;
    }
    int fd_new=allocFreeusertable();
    if(fd_new<0)
    {
        printf("usertable is full\n");
        return -1;
    }


    memcpy(&usertable.openfilelist[fd_new].open_inode,inode_p,sizeof(inode));

    usertable.openfilelist[fd_new].count=0;
    usertable.openfilelist[fd_new].inodestate=0;
    usertable.openfilelist[fd_new].topenfile=1;
    strcpy(usertable.openfilelist[fd_new].dir,filename_abs);
    usertable.openfilelist[fd_new].dirno=usertable.openfilelist[fd].open_inode.addrs[0];
    usertable.openfilelist[fd_new].diroff=off;
    
    // usertable.currentFd=fd_new;
    return fd_new;

}
// 被 my_read()调用,读出指定文件中从读写指针开始的长度为 len 的内容到用户空间的 text 中
int do_read (int fd, int len,char *text)
{
    /*
        fd: open()函数的返回值,文件的描述符
        len: 要求从文件中读出的字节数
        text: 指向存放读出数据的用户区地址
        return : 实际读出的字节数
    */
    
    // 用 malloc()申请 BLOCKSIZE 的内存空间作为读写磁盘的缓冲区 buf,
    // 申请失败则返回-1,并显示出错信息   
    // inode* tmp=(inode*)text;
    uchar *buf=(uchar*)malloc(BLOCKSIZE);
    if(buf==NULL)
    {
        printf("malloc error\n");
        return -1;
    }
    int count=usertable.openfilelist[fd].count;
    ushort* addrs=usertable.openfilelist[fd].open_inode.addrs;
    ushort first=addrs[0];// !!!
    ushort inode_iter=first;
    ushort level=0;
    ushort block_idx=0;// !!! 

    ushort l1=0;
    ushort l2=0;
    ushort l3=0;
    // printf("%d %d\n",count,BLOCKSIZE);
    // move count, len doesn't change 
    while(count>=BLOCKSIZE) // three level index
    {
        block_idx++;
        level=getIndexLevel(block_idx);
        inode_iter=nextFcbIter(addrs,level,block_idx,&l1,&l2,&l3);
        if(inode_iter==0)
        {
            printRed("error, no such block!");
            return -1;
        }
        count-=BLOCKSIZE;
    }

    uchar* block_p=(uchar*)(disk.myvhard+BLOCKSIZE*inode_iter);

    memcpy(buf,block_p,BLOCKSIZE);// block->buf
    
    
    int lenTmp=len;
    char* text_p=text;
    while(len>0)
    {
        if(len<=BLOCKSIZE-count)// is too short
        {
            memcpy(text_p,buf+count,len);// buf->text

            text_p+=len;
            count+=len;
            usertable.openfilelist[fd].count+=len;
            len=0;
        }else{// is longer than BLOCKSIZE-count
            memcpy(text_p,buf+count,BLOCKSIZE-count);
            
            text_p+=BLOCKSIZE-count;
            len-=BLOCKSIZE-count;
            count=0;  

            block_idx++;
            level=getIndexLevel(block_idx);
            inode_iter=nextFcbIter(addrs,level,block_idx,&l1,&l2,&l3);
            if(inode_iter==0)
            {
                printf("error, len is too long\n");
                return -1;
            }

            // !!! block->buf
            block_p=disk.myvhard+BLOCKSIZE*inode_iter;
            memcpy(buf,block_p,BLOCKSIZE);
        }
    }

    free(buf);
    return lenTmp-len; 
}

// 在当前目录下创建名为 dirname 的子目录。
int my_mkdir(char *dirname)
{
    /*
        dirname：新建目录的目录名   
    */

    int currentfd=usertable.currentFd;

    char* fname = strtok(dirname, ".");
	char* exname = strtok(NULL, ".");

	if (exname) {
        printRed("just can make a directory\n");
		return -1;
	}

    char buf[MAX_TEXT_SIZE];
    usertable.openfilelist[currentfd].count=0;
    int r_len_fa=do_read(currentfd,usertable.openfilelist[currentfd].open_inode.length,buf);
    if(r_len_fa<0)
    {
        printRed("read error\n");
        return -1;
    }

    inode* inode_p=(inode*)buf;
    for(int i=0;i<r_len_fa/sizeof(inode);i++)
    {
        if(!strcmp(dirname,inode_p[i].filename)&&inode_p[i].attribute==T_DIR)
        {
            printf("dir %s has existed\n",dirname);
            return -1;
        }
    }

    // existed ?
    int fd_new=allocFreeusertable();
    if(fd_new<0)
    {
        printRed("error, useropentable is full\n");
        return -1;
    }

    // bmap
    ushort bmap_new=allocFreeBlock();
    if(bmap_new==END)
    {
        usertable.openfilelist[fd_new].topenfile=0;
        printRed("error, disk is full\n");
        return -1;
    }

    int off=0;
    for(int i=0;i<r_len_fa/sizeof(inode);i++)
    {
        off=i+1;
        if(inode_p[i].free==0)// !!! 内部可能有碎片
        {
            off=i;
            break;
        }
    }

    int count=off*sizeof(inode);
    usertable.openfilelist[currentfd].count=count;
    usertable.openfilelist[currentfd].inodestate=1;

    inode* inode_new=(inode*)malloc(sizeof(inode));
    inode_new->attribute=T_DIR;
    time_t rawtime = time(NULL);
    struct tm* time=localtime(&rawtime);
    // TODO : time and date
    inode_new->date=((time->tm_year-100)<<9)+((time->tm_mon+1)<<5)+(time->tm_mday);
    inode_new->time=((time->tm_hour)<<12)+((time->tm_min)<<6)+(time->tm_sec);
    strcpy(inode_new->filename,dirname);
    memcpy(inode_new->exname,"DIR",3);
    inode_new->addrs[0]=bmap_new;
    inode_new->length=sizeof(inode)*2;// ".." and "."
    inode_new->free=1;
    int w_len=do_write(currentfd,(char *)inode_new,sizeof(inode),W_cover,count);
    if(w_len<0)
    {
        printf("write error\n");
        return -1;
    }

    // usertable for fd_new
    memcpy(&usertable.openfilelist[fd_new].open_inode,inode_new,sizeof(inode));
    usertable.openfilelist[fd_new].count=0;
    // dirname -> dirname_abs
    char dirname_abs[MAX_FILE_NAME];
    strcpy(dirname_abs,usertable.openfilelist[currentfd].dir);
    if(currentfd!=0)
        dirname_abs[strlen(dirname_abs)]='/';
    strcat(dirname_abs,dirname);
    dirname_abs[strlen(dirname_abs)]='\0';
    // copy dirname_abs
    strcpy(usertable.openfilelist[fd_new].dir,dirname_abs);
    
    usertable.openfilelist[fd_new].dirno=usertable.openfilelist[currentfd].open_inode.addrs[0];
    usertable.openfilelist[fd_new].diroff=off;
    usertable.openfilelist[fd_new].inodestate=0;
    usertable.openfilelist[fd_new].topenfile=1;

    // "."
    inode* inode_1=(inode*)malloc(sizeof(inode));
    inode_1->attribute=T_DIR;
    inode_1->date=((time->tm_year-100)<<9)+((time->tm_mon+1)<<5)+(time->tm_mday);
    inode_1->time=((time->tm_hour)<<12)+((time->tm_min)<<6)+(time->tm_sec);
    // TODO : date and time
    strcpy(inode_1->filename,".");
    memcpy(inode_1->exname,"DIR",3);
    inode_1->addrs[0]=bmap_new;// addrs[0]
    inode_1->free=1;
    inode_1->length=sizeof(inode)*2;


    int w_len_inode1=do_write(fd_new,(char*)inode_1,sizeof(inode),W_cover,0);
    if(w_len_inode1<0)
    {
        printf("write error\n");
        return -1;
    }

    // ".."
    inode* inode_2=(inode*)malloc(sizeof(inode));
    memcpy(inode_2,inode_1,sizeof(inode));
    strcpy(inode_2->filename,"..");
    memcpy(inode_2->exname,"DIR",3);
    inode_2->addrs[0]=usertable.openfilelist[currentfd].open_inode.addrs[0];
    inode_2->length=usertable.openfilelist[currentfd].open_inode.length;
    inode_2->time=usertable.openfilelist[currentfd].open_inode.time;
    inode_2->date=usertable.openfilelist[currentfd].open_inode.date;

    int w_len_inode2=do_write(fd_new,(char*)inode_2,sizeof(inode),W_cover,sizeof(inode));
    
    my_close(fd_new);

    inode_p=(inode*)buf;

    inode_p->length=usertable.openfilelist[currentfd].open_inode.length;
    memcpy(inode_p->addrs,usertable.openfilelist[currentfd].open_inode.addrs,sizeof(inode_p->addrs));//!!!!!!!!!!!!!!
    if(currentfd==0){
        inode* inode_p_2=inode_p+1;
        inode_p_2->length=usertable.openfilelist[currentfd].open_inode.length;    
        memcpy(inode_p_2->addrs,usertable.openfilelist[currentfd].open_inode.addrs,sizeof(inode_p->addrs));//!!!!!!!!!!!!!!
        usertable.openfilelist[currentfd].count=sizeof(inode);
        int w_len_cur=do_write(currentfd,(char*)inode_p_2,sizeof(inode),W_cover,sizeof(inode));
    }
    
    usertable.openfilelist[currentfd].count=0;
    int w_len_cur=do_write(currentfd,(char*)inode_p,sizeof(inode),W_cover,0);
    usertable.openfilelist[currentfd].inodestate=1;

    free(inode_1);
    free(inode_2);

    return bmap_new;
}

// 被写文件函数 my_write()调用,用来将键盘输入的内容写到相应的文件中去
int do_write(int fd,char *text,int len,char wstyle, int pos)
{
    /*
        params
        fd: open()函数的返回值,文件的描述符;
        text:指向要写入的内容的指针;
        len:本次要求写入字节数
        wstyle:写方式
        return 实际写入的字节数。
    */
    // 用 malloc()申请 BLOCKSIZE 的内存空间作为读写磁盘的缓冲区 buf,
    // 申请失败则返回-1,并显示出错信息   
    uchar *buf=(uchar*)malloc(BLOCKSIZE);
    if(buf==NULL)
    {
        printf("malloc error\n");
        return -1;
    }
    ushort first=usertable.openfilelist[fd].open_inode.addrs[0];

    // 文件打开之后立即清空原有内容
    if(wstyle==W_truncate)
    {
        usertable.openfilelist[fd].count=0;
        usertable.openfilelist[fd].open_inode.length=0;
    }
    // 文件打开之后不清空原有内容，可以在文件任意位置写入
    else if(wstyle==W_cover)
    {
        if(usertable.openfilelist[fd].open_inode.attribute==T_FILE&&usertable.openfilelist[fd].open_inode.length!=0)
        {
            if(pos!=-1&&pos<usertable.openfilelist[fd].open_inode.length){
                usertable.openfilelist[fd].count=pos;            
            }else{
                usertable.openfilelist[fd].count-=1;
            }
        }
    }
    // 文件打开之后不清空原有内容，每次只能在文件最后写入
    else if(wstyle==W_append)
    {
        if(usertable.openfilelist[fd].open_inode.attribute==T_DIR)
        {
            usertable.openfilelist[fd].count=usertable.openfilelist[fd].open_inode.length;
        }
        else if(usertable.openfilelist[fd].open_inode.attribute==T_FILE&&usertable.openfilelist[fd].open_inode.length!=0)
        {
            usertable.openfilelist[fd].count-=1;
        }   
    }

    // use fat to move count!!!
    int count = usertable.openfilelist[fd].count;
    ushort* addrs=usertable.openfilelist[fd].open_inode.addrs;
    ushort inode_iter=first;
    ushort level=0;
    ushort block_idx=0;// !!! 
    ushort l1=0;
    ushort l2=0;
    ushort l3=0;
    // printf("%d %d\n",count,BLOCKSIZE);
    while(count>=BLOCKSIZE)
    {
        block_idx++;
        level=getIndexLevel(block_idx);

        if(nextFcbIter(addrs,level,block_idx,&l1,&l2,&l3)==0)
        {
            ushort bmap_new=allocFreeBlock();
            if(bmap_new==END)
            {
                printf("error , disk is full\n");
                return -1;
            }
            else{
                allocThreeLevelIndex(addrs,level,block_idx,bmap_new);// !!!
            }
        }
        inode_iter=nextFcbIter(addrs,level,block_idx,&l1,&l2,&l3);
        count-=BLOCKSIZE;
    }
    

    // 1. short text : text+block->buf->block
    // 2. long text : text -> buf -> block
    uchar* block_p=(uchar*)(disk.myvhard+BLOCKSIZE*inode_iter);
    int text_p=0;
    while(text_p<len)
    {
        memcpy(buf,block_p,BLOCKSIZE);// block->buf            
        while(count<BLOCKSIZE)// text->buf
        {   
            // buf[count]=text[text_p];
            memcpy(buf+count,text+text_p,1);
            text_p++;
            count++;
            if(text_p==len)// text is short , break directly
                break;
        }
        memcpy(block_p,buf,BLOCKSIZE);// buf->block

        if(count==BLOCKSIZE&&text_p!=len)// text is too long
        {
            count=0;
            // next block
            block_idx++;
            level=getIndexLevel(block_idx);
            
            ushort nextfcb=nextFcbIter(addrs,level,block_idx,&l1,&l2,&l3);

            if(nextfcb==0||disk.BMAP_BLOCK[nextfcb].free==FREE)
            {            
                ushort bmap_new=allocFreeBlock();
                if(bmap_new==END)
                {
                    printf("error , disk is full\n");
                    return text_p;// !!! 
                }else{
                    // !!! need to update block_p
                    block_p=(uchar*)(disk.myvhard+BLOCKSIZE*bmap_new);
                    allocThreeLevelIndex(addrs,level,block_idx,bmap_new);
                    inode_iter = bmap_new;
                }
            }else{
                // !!! need to update block_p
                inode_iter=nextFcbIter(addrs,level,block_idx,&l1,&l2,&l3);
                block_p=(uchar*)(disk.myvhard+BLOCKSIZE*inode_iter);
            }
        }
    }

    usertable.openfilelist[fd].count+=len;

    int c=usertable.openfilelist[fd].count;
    int l=usertable.openfilelist[fd].open_inode.length;
    usertable.openfilelist[fd].open_inode.length=max(c,l);
    free(buf);

    return len;
}
// 退出文件系统
int my_exitsys()
{
    /*
        ① 使用 C 库函数 fopen()打开磁盘上的 myfsys 文件；
        ② 将虚拟磁盘空间中的所有内容保存到磁盘上的 myfsys 文件中；
        ③ 使用 c 语言的库函数 fclose()关闭 myfsys 文件；
        ④ 撤销用户打开文件表，释放其内存空间 释放虚拟磁盘空间。
    */


    FILE* fp=fopen(myfsys_name,"wb");
    while(usertable.currentFd)
    {
        int fd=my_close(usertable.currentFd);
        if(fd<0)
        {
            printf("close error\n");
            return -1;
        }
    }


    fwrite(disk.myvhard,1,DISKSIZE,fp);
    fclose(fp);
}


int find_father_fd(int fd)
{
    int ret=-1;
    for(int i=0;i<MAX_OPEN_FILE;i++)
    {
        if(usertable.openfilelist[i].open_inode.addrs[0]==usertable.openfilelist[fd].dirno)
        {
            ret=i;
            break;
        }
    }
    return ret;
}

int allocFreeusertable()
{
    int ret=-1;
    for(int i=0;i<MAX_OPEN_FILE;i++)
    {
        if(!usertable.openfilelist[i].topenfile)
        {
            usertable.openfilelist[i].topenfile=1;
            ret=i;
            break;
        }
    }
    return ret;
}


// 在当前目录下删除名为 dirname 的子目录
int my_rmdir(char *dirname, int flag)
{
    if(!strcmp(dirname,".")||!strcmp(dirname,".."))
    {
        printRed("can not rmdir .. or .\n");
        return -1;
    }
    char* fname = strtok(dirname,".");
    if(strcmp(fname,dirname))
    {
        printRed("rmdir a file is not invalid\n");
        return -1;
    }

    int currentfd=usertable.currentFd;
    char buf[MAX_TEXT_SIZE];
    usertable.openfilelist[currentfd].count=0;


    int r_len_cur=do_read(currentfd,usertable.openfilelist[currentfd].open_inode.length,buf);
    if(r_len_cur<0)
    {
        printRed("read error 1\n");
        return -1;
    }

    int off=-1;

    inode* inode_p =(inode*)buf;

    for(int i=0;i<usertable.openfilelist[currentfd].open_inode.length/sizeof(inode);i++,inode_p++)
    {
        if(!strcmp(inode_p->filename,dirname)&&inode_p->attribute==T_DIR)
        {
            off=i;
            break;
        }
    }
    if(off<0)
    {
        printRed("this dir does not exist\n");
        return -1;
    }


    if(inode_p->length>sizeof(inode)*2&&flag==0)
    {
        printRed("error, this directory contains other files\n");
        return -1;
    }

    // free  bmap(directory  . and ..)
    RecurDelete(inode_p);
    

    int len_current=usertable.openfilelist[currentfd].open_inode.length;
    /*begin 消除碎片*/
    char buffer[MAX_TEXT_SIZE];
    usertable.openfilelist[currentfd].count=(off+1)*sizeof(inode);
    int len_fragment=len_current-(off+1)*sizeof(inode);


    r_len_cur=do_read(currentfd,len_fragment,buffer);
    if(r_len_cur<0)
    {
        printRed("read error 2\n");
        return -1;
    }

    usertable.openfilelist[currentfd].count=off*sizeof(inode);
    
    int w_len_cur=do_write(currentfd,(char*)buffer,len_fragment,W_cover,off*sizeof(inode));
    if(r_len_cur<0)
    {
        printRed("read error 3\n");
        return -1;
    }
    /*end 消除碎片*/

    // free inode
    inode_p->attribute=0;
    inode_p->date=0;
    inode_p->time=0;
    inode_p->exname[0]='\0';
    inode_p->filename[0]='\0';
    memset(inode_p->addrs,0,sizeof(inode_p->addrs));
    inode_p->free=0;
    inode_p->length=0;
    // inode->block
    usertable.openfilelist[currentfd].count=len_current-sizeof(inode);
    w_len_cur=do_write(currentfd,(char*)inode_p,sizeof(inode),W_cover,len_current-sizeof(inode));
    // length - = sizeof(inode)
    usertable.openfilelist[currentfd].open_inode.length-=sizeof(inode);


    usertable.openfilelist[currentfd].count=0;
    r_len_cur=do_read(currentfd,usertable.openfilelist[currentfd].open_inode.length,buffer);
    if(r_len_cur<0)
    {
        printRed("read error\n");
        return -1;
    }

    // bmap move reconstruction
    ushort* addrs=usertable.openfilelist[currentfd].open_inode.addrs;
    ushort first=addrs[0];
    ushort inode_iter=first;
    int level=0;
    ushort l1=0;
    ushort l2=0;
    ushort l3=0;
    ushort block_idx=0;// !!! 
    block_idx++;
    level=getIndexLevel(block_idx);

    ushort next=nextFcbIter(addrs,level,block_idx,&l1,&l2,&l3);
    int cnt_iter=2*sizeof(inode);
    if(r_len_cur/sizeof(inode)==2){
        if(next!=FREE)
        {
            allocThreeLevelIndex(addrs,level,block_idx,FREE);
        }          
    }
    inode_iter=next;

    for(int i=2;i<r_len_cur/sizeof(inode);i++)
    {
        cnt_iter+=sizeof(inode);
        if(cnt_iter>BLOCKSIZE)
        {
            block_idx++;
            level=getIndexLevel(block_idx);
            next=nextFcbIter(addrs,level,block_idx,&l1,&l2,&l3);

            cnt_iter-=BLOCKSIZE;
            inode_iter=next;
            
            ushort check=nextFcbIter(addrs,getIndexLevel(block_idx+1),block_idx+1,&l1,&l2,&l3);

            if(check==FREE)
            {
                allocThreeLevelIndex(addrs,getIndexLevel(block_idx),block_idx,FREE);
            }
        }
    }


    // current -> block (update length)
    inode_p=(inode*)buffer;
    inode_p->length=usertable.openfilelist[currentfd].open_inode.length;
    memcpy(inode_p->addrs,usertable.openfilelist[currentfd].open_inode.addrs,sizeof(inode_p->addrs));//!!!!!!!!!!!!!!
    if(currentfd==0){
        inode* inode_p_2=inode_p+1;
        inode_p_2->length=usertable.openfilelist[currentfd].open_inode.length;    
        memcpy(inode_p_2->addrs,usertable.openfilelist[currentfd].open_inode.addrs,sizeof(inode_p->addrs));//!!!!!!!!!!!!!!
        usertable.openfilelist[currentfd].count=sizeof(inode);
        int w_len_cur=do_write(currentfd,(char*)inode_p_2,sizeof(inode),W_cover,sizeof(inode));
    }

    usertable.openfilelist[currentfd].count=0;
    w_len_cur=do_write(currentfd,(char*)inode_p,sizeof(inode),W_cover,0);
    usertable.openfilelist[currentfd].inodestate=1;

    return 1;
}

void my_listBMAP()
{
    int fatnum=BLOCKSIZE*BMAPBLOCKNUM/sizeof(bmap);
    for(int i=0;i<fatnum;i++)
    {
        if(i<1+BMAPBLOCKNUM)
            printf("\033[31m%1d(%05d)\033[0m ",disk.BMAP_BLOCK[i].free,i);
        else    
            printf("%1d(%05d) ",disk.BMAP_BLOCK[i].free,i);
        if((i+1)%16==0)
            printf("\n");
    }
}


void my_help()
{
    int cnt_tmp=150;
    char str_tmp[200]="─";
    char str_tmp_c[200]="│";
    printf("┌─");
    for(int i=0;i<cnt_tmp;i++)
        printf("%s",str_tmp);
    printf("┐");
    printf("\n");
    printf("%scommand name\t\t\tcommand parameters\t\t\tcommand function\t\t\t\t\t\t\t\t%s\n",str_tmp_c,str_tmp_c);
    printf("├─");
    for(int i=0;i<cnt_tmp;i++)
        printf("%s",str_tmp);    
    printf("┤");
    printf("\n");
    printf("%smkdir\t\t\t\tdirectory name\t\t\t\tcreate a new directory in the current directory\t\t\t\t\t%s\n",str_tmp_c,str_tmp_c);
    printf("%srmdir\t\t\t\tdirectory name\t\t\t\tdelete the specified directory from the current directory\t\t\t%s\n",str_tmp_c,str_tmp_c);
    printf("%sls\t\t\t\t----\t\t\t\t\tdisplay directories and files in the current directory\t\t\t\t%s\n",str_tmp_c,str_tmp_c);
    printf("%scd\t\t\t\tdirectory name or path name\t\tswitch the current directory to the specified directory\t\t\t\t%s\n",str_tmp_c,str_tmp_c);
    printf("%screate\t\t\t\tfile name\t\t\t\tcreate the specified file in the current directory\t\t\t\t%s\n",str_tmp_c,str_tmp_c);
    printf("%sopen\t\t\t\tfile name\t\t\t\topen the specified file in the current directory\t\t\t\t%s\n",str_tmp_c,str_tmp_c);
    printf("%sclose\t\t\t\tfile name\t\t\t\tclose the specified file in the current directory\t\t\t\t%s\n",str_tmp_c,str_tmp_c);
    printf("%swrite\t\t\t\tfile name\t\t\t\tin the open file state, write the specified file in the current directory\t%s\n",str_tmp_c,str_tmp_c);
    printf("%sread\t\t\t\tfile name\t\t\t\tin the open file state, read the specified file in the current directory\t%s\n",str_tmp_c,str_tmp_c);
    printf("%srm\t\t\t\tfile name\t\t\t\tdelete the specified file from the current directory\t\t\t\t%s\n",str_tmp_c,str_tmp_c);
    printf("%sexit\t\t\t\t----\t\t\t\t\tlog out\t\t\t\t\t\t\t\t\t\t%s\n",str_tmp_c,str_tmp_c);
    printf("└─");
    for(int i=0;i<cnt_tmp;i++)
        printf("%s",str_tmp);
    printf("┘");
    printf("\n");
}

void printStart(){   
    printf("♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡\n"
           "♡♡♡♡♡♡♡♡♡♥♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♥♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♥♥♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♥♥♥♡♡♡♡♡♡♡♡♡♡♡♡♥♡♡♡♡♡♡♥♥♡♡♡♡♡♡♡\n"
           "♡♡♡♡♡♡♡♡♥♥♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♥♥♥♡♡♡♡♥♥♥♡♡♡♡♡♡♡♡♡♡♥♥♥♥♥♥♥♥♥♥♥♥♥♥♡♡♡♡♡♡♡♥♥♡♡♡♡♡♥♥♥♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♥♥♡♡♡♡♡♡♥♥♥♡♡♡♡♡♡♡♡♡♡♡♥♥♥♡♡♡♡♥♥♥♥♡♡♡♡♡♡\n"
           "♡♡♡♡♡♡♡♡♥♥♥♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♥♥♥♡♥♥♡♥♥♥♡♡♡♡♡♡♡♡♡♥♥♥♥♥♥♥♥♥♥♥♥♥♥♥♡♡♡♡♡♡♡♥♥♥♡♡♡♡♥♥♥♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♥♥♥♥♡♡♡♡♡♥♥♥♡♡♡♡♡♡♡♡♡♡♡♥♥♥♡♡♡♡♥♥♥♡♡♡♡♡♡♡\n"
           "♡♡♡♡♡♡♡♡♡♥♥♥♡♡♡♡♡♡♡♡♡♡♡♡♡♥♥♥♡♥♥♥♡♥♥♥♡♡♡♡♡♡♡♡♡♥♥♥♥♥♥♥♥♡♡♡♡♡♡♡♡♡♡♡♡♡♥♥♥♡♡♥♥♥♥♥♥♥♥♥♥♥♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♥♥♥♥♡♡♡♡♥♥♡♡♡♡♡♡♡♡♡♡♡♥♥♥♥♥♥♡♥♥♥♥♥♥♥♥♡♡♡\n"
           "♡♡♥♥♥♥♥♥♥♥♥♥♥♥♥♥♥♥♥♡♡♡♡♡♡♥♥♥♡♥♥♥♡♥♥♥♡♡♥♡♡♡♡♡♡♡♡♡♡♥♥♥♡♡♥♥♥♥♡♡♡♡♡♡♡♡♥♥♡♥♡♥♥♥♥♥♥♥♥♥♥♥♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♥♥♥♥♥♥♥♥♥♥♥♥♥♥♡♡♡♡♡♥♥♥♥♥♥♥♥♥♥♥♥♥♥♥♡♡♡\n"
           "♡♡♥♥♥♥♥♥♥♥♥♥♥♥♥♥♥♥♥♡♡♡♡♡♥♥♥♡♡♥♥♥♥♥♥♥♥♥♥♡♡♡♡♡♡♡♥♥♥♥♥♡♡♥♥♥♥♥♡♡♡♡♡♡♡♥♥♥♡♥♥♥♡♡♥♥♥♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♥♥♥♥♥♥♥♥♥♥♥♥♡♡♡♡♥♥♥♡♡♡♡♥♥♥♡♡♥♥♥♡♡♡♡\n"
           "♡♡♥♡♡♥♥♥♡♡♡♡♥♥♥♥♥♥♥♡♡♡♡♥♥♥♥♡♥♥♥♥♥♥♥♥♥♥♥♡♡♡♡♡♡♡♥♥♥♥♥♥♥♥♥♥♡♡♡♡♡♡♡♡♥♥♥♥♥♥♥♡♡♥♥♥♡♡♥♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♥♥♥♡♡♡♥♥♡♡♥♥♡♡♡♥♥♥♡♡♡♥♥♥♥♥♥♥♥♥♥♥♥♡♥♥♥♡♡♡♡\n"
           "♡♡♡♡♡♥♥♥♡♡♡♡♥♥♥♡♡♡♡♡♡♡♥♥♥♥♥♡♥♥♡♡♡♥♥♥♡♡♡♡♡♡♡♡♡♡♥♥♥♥♥♥♥♥♡♥♥♥♡♡♡♡♡♡♥♥♥♥♥♥♡♡♥♥♥♡♡♥♥♥♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♥♥♥♥♥♡♥♥♡♡♥♥♡♡♡♥♥♥♡♡♡♥♥♥♥♥♥♥♥♡♡♥♥♥♥♥♡♡♡♡♡\n"
           "♡♡♡♡♡♡♥♥♡♡♡♡♥♥♡♡♡♡♡♡♡♡♡♥♥♥♥♥♥♥♡♡♡♥♥♥♡♡♡♡♡♡♡♡♡♡♡♡♥♥♥♥♥♡♡♥♥♥♥♡♡♡♡♡♡♡♡♥♥♥♡♥♥♥♥♥♥♥♥♥♥♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♥♥♥♥♡♥♥♡♡♥♥♥♥♡♡♥♡♡♡♡♡♡♡♥♥♥♡♡♡♡♡♥♥♥♡♡♡♡♡♡\n"
           "♡♡♡♡♡♡♥♥♥♡♡♥♥♥♡♡♡♡♡♡♡♡♡♡♡♥♥♡♥♥♡♡♡♥♥♥♡♡♥♥♡♡♡♡♡♥♥♥♥♥♥♡♥♥♥♥♥♥♥♥♡♡♡♡♡♡♥♥♥♡♡♥♥♥♥♥♥♥♥♥♥♥♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♥♡♡♡♡♡♥♥♥♥♥♡♡♡♡♡♡♡♡♥♥♥♥♥♥♥♡♥♥♥♥♡♡♡♡♡♡♡\n"
           "♡♡♡♡♡♡♥♥♥♡♡♥♥♥♡♡♡♡♡♡♡♡♡♡♡♥♥♥♥♥♥♥♥♥♥♥♥♥♥♥♥♡♡♡♡♥♥♥♥♥♥♥♥♥♥♥♥♥♥♥♥♡♡♡♡♥♥♥♥♥♥♥♥♥♥♥♥♥♡♥♥♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♥♥♡♡♡♥♥♥♥♥♡♡♡♡♡♡♡♡♥♥♥♥♥♥♥♥♥♥♥♥♥♥♥♥♥♡♡\n"
           "♡♡♡♡♡♡♡♥♥♥♥♥♥♡♡♡♡♡♡♡♡♡♡♡♡♥♥♥♥♥♥♥♥♥♥♥♥♥♥♥♡♡♡♡♡♥♥♥♥♥♥♥♥♥♥♡♡♡♥♥♥♡♡♡♡♥♥♥♥♥♥♡♡♥♥♥♥♥♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♥♥♥♡♡♡♥♥♥♥♥♡♡♡♡♡♡♡♡♥♥♥♥♥♥♥♥♥♥♥♥♥♥♥♥♥♡♡\n"
           "♡♡♡♡♡♡♡♥♥♥♥♥♥♡♡♡♡♡♡♡♡♡♡♡♡♥♥♡♡♡♡♡♡♥♥♥♡♡♡♡♡♡♡♡♡♡♡♥♥♡♡♡♥♥♡♥♥♥♡♡♡♡♡♡♡♥♥♥♥♡♡♡♡♥♥♡♥♥♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♥♥♥♡♡♥♥♥♥♥♥♡♡♡♡♡♡♡♡♡♡♥♥♥♡♡♥♥♥♥♡♡♡♥♥♡♡♡\n"
           "♡♡♡♡♡♡♡♡♥♥♥♥♡♡♡♡♡♡♡♡♡♡♡♡♡♥♥♡♡♡♡♡♡♥♥♥♡♡♡♡♡♡♡♡♡♡♥♥♥♥♡♡♥♥♡♥♥♥♥♡♡♡♡♡♡♡♡♡♡♥♥♥♡♥♥♡♥♥♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♥♥♥♡♡♡♥♥♥♥♥♥♡♡♥♡♡♡♡♡♡♡♥♥♥♥♥♡♡♥♥♡♡♡♥♥♡♡♡\n"
           "♡♡♡♡♡♡♥♥♥♥♥♥♥♥♡♡♡♡♡♡♡♡♡♡♡♥♥♡♡♡♡♡♡♥♥♥♡♡♡♡♡♡♡♡♡♥♥♥♥♡♡♡♥♥♡♡♥♥♥♥♡♡♡♡♥♥♥♥♥♥♥♥♥♥♥♡♥♥♡♡♥♥♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♥♥♥♡♡♥♥♥♡♥♥♥♡♡♥♥♡♡♡♡♡♡♥♥♥♥♥♡♡♥♥♡♡♡♥♥♡♡♡\n"
           "♡♡♡♡♥♥♥♥♥♥♡♥♥♥♥♥♥♡♡♡♡♡♡♡♡♥♥♥♡♡♡♡♡♥♥♥♡♡♡♡♡♡♡♡♥♥♥♡♡♡♥♥♥♥♡♡♡♥♥♥♥♡♡♡♥♥♥♥♥♥♡♥♥♥♡♡♥♥♥♥♥♥♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♥♥♥♡♡♥♥♥♥♡♥♥♥♥♥♥♥♡♡♡♡♡♡♥♥♥♥♡♡♡♥♥♥♥♥♥♥♡♡♡\n"
           "♡♡♥♥♥♥♥♥♡♡♡♡♥♥♥♥♥♥♥♡♡♡♡♡♡♥♥♥♡♡♡♡♡♥♥♥♡♡♡♡♡♡♡♡♥♥♡♡♡♥♥♥♥♥♡♡♡♡♥♥♡♡♡♡♡♥♥♡♡♡♥♥♥♥♡♡♥♥♥♥♥♥♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♥♥♥♡♥♥♥♥♡♡♥♥♥♥♥♥♥♡♡♡♡♡♡♥♥♥♡♡♡♡♥♥♥♥♥♥♥♥♡♡\n"
           "♡♡♥♥♥♥♡♡♡♡♡♡♡♡♡♥♥♥♡♡♡♡♡♡♡♥♥♥♡♡♡♡♡♥♥♥♡♡♡♡♡♡♡♡♡♡♡♡♡♡♥♥♥♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♥♥♥♡♡♡♡♥♥♥♥♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♥♥♡♥♥♥♡♡♡♡♥♥♥♥♥♡♡♡♡♡♡♡♡♥♡♡♡♡♡♥♥♡♡♡♥♥♥♡♡\n"
           "♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡\n"
           "♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡♡\n");
}

void printRed(char* s){
    printf("\033[31m%s\033[0m\n",s); 
}

void my_pwd(){
    int currentfd=usertable.currentFd;
    printf("%s\n",usertable.openfilelist[currentfd].dir);
    printf("current fd : %d\n",currentfd);
}   

void showFileSystem(){
    system("clear");
    printf("********************File System Based On Three Level Index********************\n");
    printf("********************author : 20051124 Shenming********************************\n");
    printf("磁盘总大小 : %dB\n",disk.block0->blocksize*disk.block0->blocknum);
    printf("磁盘数量 : %d\n",disk.block0->blocknum);
    printf("磁盘大小 : %dB\n",disk.block0->blocksize);
    printf("数据块数量 : %d\n",disk.block0->datablocknum);
    printf("bmap块数量 : %d\n",disk.block0->bmapblocknum);
    printf("bootblock 大小 : %ldB\n",sizeof(superblock));
    printf("inode大小 : %ldB\n",sizeof(inode));
    printf("file system magic number : %s\n",disk.block0->magic_number);
    printf("file system max open file number : %d\n",disk.block0->maxopenfile);
}


int str2int(char* s)
{
    int l=strlen(s);
    int ret=0;
    for(int i=0;i<l;i++)
    {
        ret*=10;
        ret+=(s[i]-'0');
    }
    return ret;
}

int mystrcmp(char *s1,char* s2)
{
    int a1=strlen(s1);
    int a2=strlen(s2);
    if(a1!=a2)
        return -1;
    for(int i=0;i<a1;i++)
        if(s1[i]!=s2[i])
            return -1;
    return 1;
}

int checkfile(char* s)
{
    int l=strlen(s);
    for(int i=0;i<l;i++)
    {
        if(s[i]=='.')
            return 1;
    }
    return 0;
}

int mypower(int a,int b)
{
    int ret=1;
    for(int i=0;i<b;i++)
    {
        ret*=a;
    }
    return ret;
}

int getIndexLevel(ushort block_idx)
{
    int level=0;
    if(block_idx>=NDIRECT)
        level=1;
    else if(block_idx>=NDIRECT+INDEXNUM_PER_BLOCK)
        level=2;
    else if(block_idx>=NDIRECT+mypower(INDEXNUM_PER_BLOCK,2))
        level=3;
    else if(block_idx>=NDIRECT+mypower(INDEXNUM_PER_BLOCK,3))
    {
        printRed("error,file is too large!!!");
        return -1;
    }
    return level;
}

ushort nextFcbIter(ushort*addrs, int level, ushort block_idx, ushort* l1,ushort* l2,ushort* l3)// three level index 
{
    ushort inode_iter=-1;
    ushort idx_1,idx_2,idx_3,off;
    if(level==0)
    {
        inode_iter=addrs[block_idx];
    }else if(level==1)
    {
        idx_1=level+NDIRECT-1;// 10
        off=block_idx-NDIRECT;
        inode_iter=((ushort*)(disk.myvhard+BLOCKSIZE*addrs[idx_1]))[off];
        *l1=off;// !
    }else if(level==2)
    {
        idx_1=level+NDIRECT-1;// 11
        off=block_idx-NDIRECT-INDEXNUM_PER_BLOCK;

        idx_2=off/INDEXNUM_PER_BLOCK;
        off=off%INDEXNUM_PER_BLOCK;
        
        *l1=idx_2;//!!
        *l2=off;//!!

        idx_2=((ushort*)(disk.myvhard+BLOCKSIZE*addrs[idx_1]))[idx_2];
        inode_iter=((ushort*)(disk.myvhard+BLOCKSIZE*idx_2))[off];   
    }else if(level==3)// 12
    {
        idx_1=level+NDIRECT-1;
        off=block_idx-NDIRECT-INDEXNUM_PER_BLOCK-mypower(INDEXNUM_PER_BLOCK,2);
        
        idx_2=off/(mypower(INDEXNUM_PER_BLOCK,2));
        off=off%(mypower(INDEXNUM_PER_BLOCK,2));
        *l1=idx_2;

        idx_3=off/INDEXNUM_PER_BLOCK;
        
        *l2=idx_3;//!!!

        off=off%INDEXNUM_PER_BLOCK;
        *l3=off;/// !!!

        idx_2=((ushort*)(disk.myvhard+BLOCKSIZE*addrs[idx_1]))[idx_2];
        idx_3=((ushort*)(disk.myvhard+BLOCKSIZE*idx_2))[idx_3];
        inode_iter=((ushort*)(disk.myvhard+BLOCKSIZE*idx_3))[off];
    }
    return inode_iter;
}


ushort allocThreeLevelIndex(ushort*addrs, int level, ushort block_idx, ushort block_i)
{
    ushort inode_iter;
    ushort idx_1,idx_2,idx_3,off;
    if(level==0)
    {
        if(block_i==FREE)
        {
            disk.BMAP_BLOCK[addrs[block_idx]].free=FREE;
            return 1;
        }
        addrs[block_idx]=block_i;
    }else if(level==1)
    {
        idx_1=level+NDIRECT-1;// 10
        off=block_idx-NDIRECT;
        if(off==0)
        {
            ushort bmap_new=allocFreeBlock();
            if(bmap_new==END)
                printRed("no block to allocate!\n");
            addrs[idx_1]=bmap_new;
        }

        if(block_i==FREE)
        {
            disk.BMAP_BLOCK[((ushort*)(disk.myvhard+BLOCKSIZE*addrs[idx_1]))[off]].free=FREE;
            if(off==0)
            {
                disk.BMAP_BLOCK[addrs[idx_1]].free=FREE;   
            }
            return 1;
        }

        
        ((ushort*)(disk.myvhard+BLOCKSIZE*addrs[idx_1]))[off]=block_i;
    }else if(level==2)
    {
        idx_1=level+NDIRECT-1;// 11
        off=block_idx-NDIRECT-INDEXNUM_PER_BLOCK;
        if(off==0)
        {
            ushort bmap_new=allocFreeBlock();
            if(bmap_new==END)
                printRed("no block to allocate!\n");
            addrs[idx_1]=bmap_new;
        }
        idx_2=off/INDEXNUM_PER_BLOCK;
        off=off%INDEXNUM_PER_BLOCK;
        

        if(off==0)
        {
            ushort bmap_new=allocFreeBlock();
            if(bmap_new==END)
                printRed("no block to allocate!\n");
            ((ushort*)(disk.myvhard+BLOCKSIZE*addrs[idx_1]))[idx_2]=bmap_new;
        }

        idx_2=((ushort*)(disk.myvhard+BLOCKSIZE*addrs[idx_1]))[idx_2];
        
        if(block_i==FREE)
        {
            disk.BMAP_BLOCK[((ushort*)(disk.myvhard+BLOCKSIZE*idx_2))[off]].free=FREE;
            if(off==0)
            {
                disk.BMAP_BLOCK[((ushort*)(disk.myvhard+BLOCKSIZE*addrs[idx_1]))[idx_2]].free=FREE;        
            } 
            if(block_idx-NDIRECT-INDEXNUM_PER_BLOCK==0)
            {
                disk.BMAP_BLOCK[addrs[idx_1]].free=FREE;
            }
            return 1;
        }
        ((ushort*)(disk.myvhard+BLOCKSIZE*idx_2))[off]=block_i;
    }else if(level==3)// 12
    {
        int off_1,off_2,off_3;
        idx_1=level+NDIRECT-1;
        off_1=block_idx-NDIRECT-INDEXNUM_PER_BLOCK-mypower(INDEXNUM_PER_BLOCK,2);
        if(off_1==0)
        {
            ushort bmap_new=allocFreeBlock();
            if(bmap_new==END)
                printRed("no block to allocate!\n");
            addrs[idx_1]=bmap_new;
        }


        idx_2=off_1/(mypower(INDEXNUM_PER_BLOCK,2));
        off_2=off_1%(mypower(INDEXNUM_PER_BLOCK,2));

        if(off_2==0)
        {
            ushort bmap_new=allocFreeBlock();
            if(bmap_new==END)
                printRed("no block to allocate!\n");
            ((ushort*)(disk.myvhard+BLOCKSIZE*addrs[idx_1]))[idx_2]=bmap_new;
        }

        idx_3=off_2/INDEXNUM_PER_BLOCK;
        off_3=off_2%INDEXNUM_PER_BLOCK;
        if(off_3==0)
        {
            ushort bmap_new=allocFreeBlock();
            if(bmap_new==END)
                printRed("no block to allocate!\n");
            ((ushort*)(disk.myvhard+BLOCKSIZE*idx_2))[idx_3]=bmap_new;
        }

        idx_2=((ushort*)(disk.myvhard+BLOCKSIZE*addrs[idx_1]))[idx_2];
        idx_3=((ushort*)(disk.myvhard+BLOCKSIZE*idx_2))[idx_3];

        if(block_i==FREE)
        {
            disk.BMAP_BLOCK[((ushort*)(disk.myvhard+BLOCKSIZE*idx_3))[off]].free=FREE;
            if(off_3==0)
            {
                disk.BMAP_BLOCK[((ushort*)(disk.myvhard+BLOCKSIZE*idx_2))[idx_3]].free=FREE;    
            }
            if(off_2==0)
            {
                disk.BMAP_BLOCK[((ushort*)(disk.myvhard+BLOCKSIZE*addrs[idx_1]))[idx_2]].free=FREE;
            }
            if(off_1==0)
            {
                disk.BMAP_BLOCK[addrs[idx_1]].free=FREE;
            }
            return 1;
        }
        ((ushort*)(disk.myvhard+BLOCKSIZE*idx_3))[off]=block_i;
    }
    else
        return -1;
    return 1;
}

ushort allocFreeBlock()
{
    bmap* bmap_tmp=disk.BMAP_BLOCK;
    for(ushort i=0;i<(int)(BLOCKNUM);i++)
    {
        if(bmap_tmp[i].free==FREE)
        {
            bmap_tmp[i].free=NFREE;
            return i;
        }
    }
    return END;
}

void showThreeLevelIndex(ushort*addrs)
{
    int level=0;
    ushort block_idx=0;
    ushort inode_iter;
    int flag_1=0,flag_2=0,falg_3=0;
    ushort l1,l2,l3;
    ushort l1_tmp,l2_tmp,l3_tmp;
    printRed("level 0 index:");
    
    while(1)
    {
        level=getIndexLevel(block_idx);
        inode_iter=nextFcbIter(addrs,level, block_idx, &l1,&l2,&l3);
        if(!inode_iter)
            break;
        if(level==1&&!flag_1)
        {
            flag_1=1;
            printRed("level 1 index");
            if(nextFcbIter(addrs,level, block_idx+1, &l1_tmp,&l2_tmp,&l3_tmp))
                printf("──addrs_[%d]─┬─[%d]────%d\n",NDIRECT,l1,inode_iter);
            else
                printf("──addrs_[%d]───[%d]─────%d\n",NDIRECT,l1,inode_iter);    
            block_idx++;
            continue;
        }else if(level==2&&!flag_2)
        {
            flag_2=1;
            printRed("level 2 index:");
            // TODO
            block_idx++;
            continue;
        }else if(level==3&&!falg_3)
        {
            falg_3=1;
            printRed("level 3 index:");
            // TODO
            block_idx++;
            continue;
        }

        if(level==0)
        {
            printf("──addrs_[%d]────%d\n",block_idx,inode_iter);
        }
        else if(level==1&&flag_1)
        {
            if(l1==INDEXNUM_PER_BLOCK-1||!nextFcbIter(addrs,level, block_idx+1, &l1_tmp,&l2_tmp,&l2_tmp))
                printf("            └─[%d]────%d\n",l1,inode_iter);
            else
                printf("            ├─[%d]────%d\n",l1,inode_iter);
        }
        // level=2 or level=3
        // TODO
        block_idx++;
    }
}

void RecurDelete(inode* inode_p)
{
    if(inode_p->attribute==T_FILE || (inode_p->attribute==T_DIR&&inode_p->length==2*sizeof*(inode_p)))
    {
        ushort* addrs=inode_p->addrs;
        ushort level=0;
        ushort block_idx=0;// !!! 
        ushort l1=0,l2=0,l3=0;

        level=getIndexLevel(block_idx);
        ushort inode_iter=nextFcbIter(addrs,level,block_idx,&l1,&l2,&l3);
        while(inode_iter!=FREE||disk.BMAP_BLOCK[inode_iter].free==1)
        {
            allocThreeLevelIndex(addrs,level,block_idx,FREE);
            block_idx++;
            level=getIndexLevel(block_idx);
            inode_iter=nextFcbIter(addrs,level,block_idx,&l1,&l2,&l3);
        }
        return;
    }
    else if(inode_p->attribute==T_DIR&&inode_p->length>2*sizeof(inode_p))
    {
        char dirname[20];
        memcpy(dirname,inode_p->filename,8);
        dirname[8]='\0';
        
        my_cd(dirname);
        char file_tmp[MAX_TEXT_SIZE];
        int fd_tmp=usertable.currentFd;
        usertable.openfilelist[fd_tmp].count=0;
        int len_r = do_read(fd_tmp,usertable.openfilelist[fd_tmp].open_inode.length,file_tmp);
        inode* inode_tmp=(inode*)file_tmp;

        inode_tmp+=2;
        for(int i=2;i<len_r/sizeof(inode);i++,inode_tmp++)
        {
            RecurDelete(inode_tmp);
        }

        my_cd("..");
        ushort* addrs=inode_p->addrs;
        ushort level=0;
        ushort block_idx=0;// !!! 
        ushort l1=0,l2=0,l3=0;

        level=getIndexLevel(block_idx);
        ushort inode_iter=nextFcbIter(addrs,level,block_idx,&l1,&l2,&l3);
        while(inode_iter!=FREE||disk.BMAP_BLOCK[inode_iter].free==1)
        {
            allocThreeLevelIndex(addrs,level,block_idx,FREE);
            block_idx++;
            level=getIndexLevel(block_idx);
            inode_iter=nextFcbIter(addrs,level,block_idx,&l1,&l2,&l3);
        }
    }
}


// 将用户通过键盘输入的内容写到 fd 所指定的文件中。
int my_write(int fd, int pos)
{
    // 检查 fd 的有效性
    if(fd>=MAX_OPEN_FILE || fd<0)
    {
        printf("文件无效\n");
        return -1;
    }

    // 提示并等待用户输入写方式 (1:截断写;2:覆盖写;3:追加写)
    int w_op;
    printf("1: truncate write\n2: cover write\n3: append write\n");
    int ch_tmp;
    ch_tmp;
    while((ch_tmp = getchar()) != '\n' && ch_tmp != EOF);

    scanf("%d",&w_op);
    if(w_op<1||w_op>3)
    {  
        printf("error, try again\n");
        return -1;
    }

    // 提示用户:整个输入内容通过 wq! 键(或其他设定的键)结束
    // 用户可分多次输入写入内容,每次用回车结束
    printf("请输入… 以wq!结束\n");
    printf("可分多次输入写入内容,每次用回车结束\n");

	char text[MAX_TEXT_SIZE] = "\0";
    char text_tmp[MAX_TEXT_SIZE]="\0";
    int len=0;


    char ch;
    while(1)
    {
        while((ch_tmp = getchar()) != '\n' && ch_tmp != EOF);
        scanf("%[^\n]",text_tmp);
        if(mystrcmp(text_tmp,"wq!")==1) // similar to vim
            break;
        strcat(text,text_tmp);
        text[strlen(text)]='\n';
    }
    len=strlen(text);
    int w_len;
    usertable.openfilelist[fd].count=usertable.openfilelist[fd].open_inode.length;
    w_len=do_write(fd,text,len,w_op,pos); 

    if(w_len<0)
    {
        printRed("write error, file is full\n");
        return -1;
    }

    // 调用 do_write()函数将通过键盘键入的内容写到文件中
    // 如果 do_write()函数的返回值为非负值,
    // 则将实际写入字节数增加 do_write()函数返回值,否则显示出错信息

    // 如果当前读写指针位置大于用户打开文件表项中的文件长度,
    // 则修改打开文件表项 中的文件长度信息,并将 fcbstate 置1
    int c=usertable.openfilelist[fd].count;
    int l=usertable.openfilelist[fd].open_inode.length;
    usertable.openfilelist[fd].open_inode.length=max(c,l);

    usertable.openfilelist[fd].inodestate=1;
    // after write file, must close!!!
    
    return 1;

}


// 读出指定文件中从读写指针开始的长度为 len 的内容到用户空间中
int my_read (int fd, int pos, int len)
{
    // 检查 fd 的有效性
    if(fd>=MAX_OPEN_FILE || fd<0)
    {
        printf("文件无效\n");
        return -1;
    }
    if(len==-1)
    {
        len=usertable.openfilelist[fd].open_inode.length-pos;
    }
    char text[1000]="\0";
    usertable.openfilelist[fd].count=pos;
    printf("pos = %d, len = %d\n",pos,len);
    int readlen=do_read(fd,len,text);

    // 如果 do_read()的返回值为负,则显示出错信息;
    // 否则将 text[]中的内容显示到屏幕上
    if(readlen<0)
    {
        printf("read error\n");
        return -1;
    }else{
        printf("%s",text);
        if(text[strlen(text)-1]!='\n')
            printf("\n");
        return 1;
    }
}