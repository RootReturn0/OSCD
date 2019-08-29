// Simple PIO-based (non-DMA) IDE driver code.
// 磁盘IO的具体实现，xv6维护了一个进程请求磁盘操作的队列(idequeue)。
// 当一个磁盘读写操作完成时，会触发一个中断。
#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"
#include "traps.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "buf.h"

#define SECTOR_SIZE   512
#define IDE_BSY       0x80
#define IDE_DRDY      0x40
#define IDE_DF        0x20
#define IDE_ERR       0x01

#define IDE_CMD_READ  0x20
#define IDE_CMD_WRITE 0x30
#define IDE_CMD_RDMUL 0xc4
#define IDE_CMD_WRMUL 0xc5

// idequeue points to the buf now being read/written to the disk.
// idequeue->qnext points to the next buf to be processed.
// You must hold idelock while manipulating queue. 操作队列时必须持有idelock

static struct spinlock idelock; // 保护idequeue
static struct buf *idequeue; // 磁盘读写操作的请求队列

static int havedisk1;
static void idestart(struct buf*);

// Wait for IDE disk to become ready.
// 等待IDE磁盘进入空闲状态
static int
idewait(int checkerr)
{
  int r;

  while(((r = inb(0x1f7)) & (IDE_BSY|IDE_DRDY)) != IDE_DRDY)
    ;
  if(checkerr && (r & (IDE_DF|IDE_ERR)) != 0)
    return -1;
  return 0;
}

// 初始化IDE磁盘
void
ideinit(void)
{
  int i;

  initlock(&idelock, "ide");
  ioapicenable(IRQ_IDE, ncpu - 1);
  idewait(0);

  // Check if disk 1 is present
  outb(0x1f6, 0xe0 | (1<<4));
  for(i=0; i<1000; i++){
    if(inb(0x1f7) != 0){
      havedisk1 = 1;
      break;
    }
  }

  // Switch back to disk 0.
  outb(0x1f6, 0xe0 | (0<<4));
}

// Start the request for b.  Caller must hold idelock.
// 开始一个磁盘的请求，请求者必须持有ide锁
static void
idestart(struct buf *b)
{
  if(b == 0)
    panic("idestart");
  if(b->blockno >= FSSIZE)
    panic("incorrect blockno");
  int sector_per_block =  BSIZE/SECTOR_SIZE;
  int sector = b->blockno * sector_per_block;
  int read_cmd = (sector_per_block == 1) ? IDE_CMD_READ :  IDE_CMD_RDMUL;
  int write_cmd = (sector_per_block == 1) ? IDE_CMD_WRITE : IDE_CMD_WRMUL;

  if (sector_per_block > 7) panic("idestart");

  idewait(0);
  outb(0x3f6, 0);  // generate interrupt
  outb(0x1f2, sector_per_block);  // number of sectors
  outb(0x1f3, sector & 0xff);
  outb(0x1f4, (sector >> 8) & 0xff);
  outb(0x1f5, (sector >> 16) & 0xff);
  outb(0x1f6, 0xe0 | ((b->dev&1)<<4) | ((sector>>24)&0x0f));
  if(b->flags & B_DIRTY){
    outb(0x1f7, write_cmd);
    outsl(0x1f0, b->data, BSIZE/4);
  } else {
    outb(0x1f7, read_cmd);
  }
}

// Interrupt handler.
// 磁盘完成请求处理后中断处理的函数
// 移除队列开头的请求，唤醒队列开头请求所对应的进程。
void
ideintr(void)
{
  struct buf *b;

  // First queued buffer is the active request.
  acquire(&idelock); // 请求idelock

  // 队列为空时
  if((b = idequeue) == 0){
    release(&idelock);
    return;
  }
  idequeue = b->qnext;

  // Read data if needed.
  if(!(b->flags & B_DIRTY) && idewait(1) >= 0)
    insl(0x1f0, b->data, BSIZE/4);

  // Wake process waiting for this buf.
  // 完成磁盘请求后，唤醒等待队列头的进程
  b->flags |= B_VALID;
  b->flags &= ~B_DIRTY;
  wakeup(b);

  // Start disk on next buf in queue.
  // 当队列不为空时，继续处理下一个请求
  if(idequeue != 0)
    idestart(idequeue);

  release(&idelock); // 释放idelock
}

//PAGEBREAK!
// Sync buf with disk.
// If B_DIRTY is set, write buf to disk, clear B_DIRTY, set B_VALID.
// Else if B_VALID is not set, read buf from disk, set B_VALID.
// 当进程调用void iderw(struct buf *b)请求读写磁盘时，
// 该请求被加入等待队列idequeue，同时进程进入睡眠状态。
// 如果B_DIRTY已设置，则表示缓冲区的内容已经被改变并且需要写回磁盘，
// 此时将缓冲区写入磁盘，清除B_DIRTY状态，设置B_VALID；
// 如果B_VALID未设置，则表示缓冲区尚未拥有磁盘块的有效内容，
// 此时将磁盘内容读入缓冲区，设置B_VALID
void
iderw(struct buf *b)
{
  struct buf **pp;

  // 缓冲区未持有锁，引发内核错误
  if(!holdingsleep(&b->lock)) 
    panic("iderw: buf not locked");
  // 磁盘内容已读入缓冲区但尚未修改，无需调用iderw，引发内核错误
  if((b->flags & (B_VALID|B_DIRTY)) == B_VALID) 
    panic("iderw: nothing to do");
  // 缓冲区所对应的为磁盘1，但磁盘1尚未准备好，引发内核错误
  if(b->dev != 0 && !havedisk1)
    panic("iderw: ide disk 1 not present");

  acquire(&idelock);  //DOC:acquire-lock 请求idelock

  // Append b to idequeue.
  b->qnext = 0;
  for(pp=&idequeue; *pp; pp=&(*pp)->qnext)  //DOC:insert-queue 插入队列
    ;
  *pp = b;

  // Start disk if necessary.
  // 当队列仅有b请求时，开始处理请求
  if(idequeue == b)
    idestart(b);

  // Wait for request to finish.
  // 睡眠等待请求完成
  while((b->flags & (B_VALID|B_DIRTY)) != B_VALID){
    sleep(b, &idelock);
  }

  // 释放锁
  release(&idelock);
}
