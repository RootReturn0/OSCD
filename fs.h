// On-disk file system format. 磁盘文件系统格式
// Both the kernel and user programs use this header file. 内核和用户程序都使用此头文件

// 声明超级块、dinode、文件和目录数据结构，以及相关的宏定义。


#define ROOTINO 1  // root i-number
#define BSIZE 512  // block size

// Disk layout:
// [ boot block | super block | log | inode blocks |
//                                          free bit map | data blocks]
//
// mkfs computes the super block and builds an initial file system. The
// super block describes the disk layout:   
// 磁盘分层为【 引导块 | 超级块 | 日志 | i节点块 | 空闲位图 | 数据块】  
// 文件系统不使用引导块，引导块中存有bootloader
// 超级块包含了文件系统的元信息（如文件系统的总块数，数据块块数，i 节点数，以及日志的块数）
struct superblock {
  uint size;         // Size of file system image (blocks)  文件系统大小
  uint nblocks;      // Number of data blocks  数据块的数量
  uint ninodes;      // Number of inodes.  i节点的数量
  uint nlog;         // Number of log blocks  日志块的数量
  uint logstart;     // Block number of first log block  日志起始块
  uint inodestart;   // Block number of first inode block i节点起始块
  uint bmapstart;    // Block number of first free map block 空闲映射起始块
};

#define NDIRECT 12 // 直接块
#define NINDIRECT (BSIZE / sizeof(uint)) // 间接块
#define MAXFILE (NDIRECT + NINDIRECT) // 最大文件

// On-disk inode structure
// 磁盘上inode节点体现形式
struct dinode {
  short type;           // 区分文件、目录和特殊文件的 i 节点，0表示为空闲节点
  short major;          // Major device number (T_DEV only) 主设备号（仅限T_DEV）
  short minor;          // Minor device number (T_DEV only) 辅设备号（仅限T_DEV）
  short nlink;          // Number of links to inode in file system 文件系统中的i节点连接数
  uint size;            // Size of file (bytes) 记录了文件的字节数
  uint addrs[NDIRECT+1];   // Data block addresses 用于这个文件的数据块的块号
};

// Inodes per block.
// 块中可存储的i节点数量
#define IPB           (BSIZE / sizeof(struct dinode))

// Block containing inode i
// 包含i节点的块
#define IBLOCK(i, sb)     ((i) / IPB + sb.inodestart)

// Bitmap bits per block
// 块中存放的位图位数
#define BPB           (BSIZE*8)

// Block of free map containing bit for block b
// 包含块b的位的空闲映射块
#define BBLOCK(b, sb) (b/BPB + sb.bmapstart)

// Directory is a file containing a sequence of dirent structures.

#define DIRSIZ 14 // 目录大小

// 文件或目录数据结构，目录本身是以文件的方式存储到磁盘上的，叫做目录文件
struct dirent {
  ushort inum; // i节点号
  char name[DIRSIZ]; // 目录名
};

