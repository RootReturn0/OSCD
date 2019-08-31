#define NPROC        64  // maximum number of processes 最大进程数
#define KSTACKSIZE 4096  // size of per-process kernel stack 每个进程的内核栈大小
#define NCPU          8  // maximum number of CPUs 最大CPU数
#define NOFILE       16  // open files per process 每个进程能打开的文件描述符数
#define NFILE       100  // open files per system 每个系统能打开的文件描述符数
#define NINODE       50  // maximum number of active i-nodes 最大活跃的inode数
#define NDEV         10  // maximum major device number 最大主设备数
#define ROOTDEV       1  // device number of file system root disk 文件系统根磁盘设备数
#define MAXARG       32  // max exec arguments 最大处理参数
#define MAXOPBLOCKS  10  // max # of blocks any FS op writes 文件系统操作最大可写块数
#define LOGSIZE      (MAXOPBLOCKS*3)  // max data blocks in on-disk log 磁盘日志最大数据块数
#define NBUF         (MAXOPBLOCKS*3)  // size of disk block cache 磁盘块cache大小
#define FSSIZE       1000  // size of file system in blocks 文件系统块数

