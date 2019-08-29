// 声明目录、文件及设备的属性结构

#define T_DIR  1   // Directory 目录
#define T_FILE 2   // File 文件
#define T_DEV  3   // Device 设备

struct stat {
  short type;  // Type of file 文件类型
  int dev;     // File system's disk device 文件系统的磁盘设备号
  uint ino;    // Inode number i节点号
  short nlink; // Number of links to file 文件的链接数
  uint size;   // Size of file in bytes 文件大小
};
