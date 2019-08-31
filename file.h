struct file {
  enum { FD_NONE, FD_PIPE, FD_INODE } type; // 文件分为管道文件和普通文件等，FD_NONE表示文件未使用
  int ref; // reference count 引用计数
  char readable; // 可读
  char writable; // 可写
  struct pipe *pipe; // 管道
  struct inode *ip; // 指向i节点
  uint off;
};


// in-memory copy of an inode
// 磁盘中的结构体 dinode 在内存中的拷贝
struct inode {
  uint dev;           // Device number 设备号
  uint inum;          // Inode number inode号
  int ref;            // Reference count 引用数
  struct sleeplock lock; // protects everything below here 保护以下所有数据
  int valid;          // inode has been read from disk? i节点是否已从磁盘中读取

  // 以下均为磁盘上的i节点，即dinode，的拷贝
  short type;         // copy of disk inode
  short major;
  short minor;
  short nlink;
  uint size;
  uint addrs[NDIRECT+1];
};

// table mapping major device number to
// device functions
struct devsw {
  int (*read)(struct inode*, char*, int); // 读
  int (*write)(struct inode*, char*, int); // 写
};

extern struct devsw devsw[];

#define CONSOLE 1
