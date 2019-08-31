// 内存中的磁盘块结构
struct buf {
  int flags; // 标记磁盘状态，valid/dirty
  uint dev; // 磁盘设备号
  uint blockno; // 块编号
  struct sleeplock lock;
  uint refcnt; // 引用计数
  struct buf *prev; // LRU cache list 使用LRU替换
  struct buf *next; // 链式结构连接磁盘块
  struct buf *qnext; // 磁盘队列
  uchar data[BSIZE]; // 块大小为512字节
};
#define B_VALID 0x2  // 缓冲区拥有磁盘块的有效内容
#define B_DIRTY 0x4  // 缓冲区的内容已经被改变并且需要写回磁盘
