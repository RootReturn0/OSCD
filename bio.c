// Buffer cache.

// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.

// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.

// The implementation uses two state flags internally:
// * B_VALID: the buffer data has been read from the disk.
// * B_DIRTY: the buffer data has been modified
//     and needs to be written to disk.

// Buffer Cache的具体实现。因为读写磁盘操作效率不高，
// 根据时间与空间局部性原理，这里将最近经常访问的磁盘块缓存在内存中。
// 主要接口有struct buf bread(uint dev, uint sector)、void bwrite(struct buf b)，
// bread会首先从缓存中去寻找块是否存在，如果存在直接返回，如果不存在则请求磁盘读操作，读到缓存中后再返回结果。
// bwrite直接将缓存中的数据写入磁盘。

#include "types.h"
#include "defs.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "buf.h"

struct {
  struct spinlock lock;
  struct buf buf[NBUF];

  // Linked list of all buffers, through prev/next.
  // head.next is most recently used.
  struct buf head;
} bcache;

// 从一个静态数组 buf 中构建出一个有 NBUF 个元素的双向链表。所有对块缓冲的访问都通过链表而非静态数组。
void
binit(void)
{
  struct buf *b;

  initlock(&bcache.lock, "bcache");

//PAGEBREAK!
  // Create linked list of buffers
  bcache.head.prev = &bcache.head;
  bcache.head.next = &bcache.head;
  for(b = bcache.buf; b < bcache.buf+NBUF; b++){
    b->next = bcache.head.next;
    b->prev = &bcache.head;
    initsleeplock(&b->lock, "buffer");
    bcache.head.next->prev = b;
    bcache.head.next = b;
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
// 扫描缓冲区链表，通过给定的设备号和扇区号找到对应的缓冲区
// 如果未找到，则分配一个缓冲区
// 否则返回一个已锁的缓冲区
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;

  acquire(&bcache.lock); // 请求锁

  // Is the block already cached?
  // 如果存在这样一个缓冲区，并且它还不是处于 B_BUSY 状态，
  // 就设置它的 B_BUSY 位并且返回。
  // 如果找到的缓冲区已经在使用中，就睡眠等待它被释放。
  // 当 sleep 返回的时候，并不能假设这块缓冲区现在可用了，
  // 事实上，sleep 时释放了 buf_table_lock, 醒来后重新获取了它，
  // 这就不能保证 b 仍然是可用的缓冲区：它有可能被用来缓冲另外一个扇区。
  for(b = bcache.head.next; b != &bcache.head; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++; // 引用计数加一
      release(&bcache.lock); // 释放锁
      acquiresleep(&b->lock); // 请求sleeplock
      return b;
    }
  }

  // Not cached; recycle an unused buffer. 未找到则重新查找
  // Even if refcnt==0, B_DIRTY indicates a buffer is in use
  // because log.c has modified it but not yet committed it.
  // 即使refcnt等于0，也可通过B_DIRTY知道缓冲区被使用，此时日志已修改但未提交 
  for(b = bcache.head.prev; b != &bcache.head; b = b->prev){
    if(b->refcnt == 0 && (b->flags & B_DIRTY) == 0) {
      b->dev = dev;
      b->blockno = blockno;
      b->flags = 0;
      b->refcnt = 1;
      release(&bcache.lock);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // 寻找和分配缓冲均失败，引发内核错误
  panic("bget: no buffers");
}

// Return a locked buf with the contents of the indicated block.
// 调用 bget 获得指定扇区的缓冲区。如果缓冲区需要从磁盘中读出，bread 会在返回缓冲区前调用 iderw。
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if((b->flags & B_VALID) == 0) {
    iderw(b);
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
// 设置 B_DIRTY 位并且调用的 iderw 将缓冲区的内容写到磁盘。
void
bwrite(struct buf *b)
{
  // 缓冲区未持有sleeplock，无法写入，引发内核错误
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  b->flags |= B_DIRTY;
  iderw(b);
}

// Release a locked buffer.
// Move to the head of the MRU list.
// 将一块缓冲区移动到链表的头部，清除 B_BUSY，唤醒睡眠在这块缓冲区上的进程。
void
brelse(struct buf *b)
{
  // 缓冲区未持有sleeplock，引发内核错误
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock); // 释放sleeplock

  acquire(&bcache.lock); // 请求锁
  b->refcnt--; //引用计数减一
  if (b->refcnt == 0) {
    // no one is waiting for it.
    b->next->prev = b->prev;
    b->prev->next = b->next;
    b->next = bcache.head.next;
    b->prev = &bcache.head;
    bcache.head.next->prev = b;
    bcache.head.next = b;
  }
  
  release(&bcache.lock); //释放锁
}
//PAGEBREAK!
// Blank page.

