//
// File descriptors
//

#include "types.h"
#include "defs.h"
#include "param.h"
#include "fs.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "file.h"

struct devsw devsw[NDEV];
struct {
  struct spinlock lock;
  struct file file[NFILE];
} ftable;

void
fileinit(void)
{
  initlock(&ftable.lock, "ftable");
}

// Allocate a file structure.  分配一个文件结构
struct file*
filealloc(void)
{
  struct file *f;

  acquire(&ftable.lock); // 请求文件表锁
  for(f = ftable.file; f < ftable.file + NFILE; f++){
    if(f->ref == 0){
      f->ref = 1;
      release(&ftable.lock);
      return f;
    }
  }
  release(&ftable.lock); // 释放文件表锁
  return 0;
}

// Increment ref count for file f.
// 增加文件引用计数
struct file*
filedup(struct file *f)
{
  acquire(&ftable.lock);
  // 文件引用为0，则文件可能未打开，引发内核错误
  if(f->ref < 1)
    panic("filedup");
  f->ref++;
  release(&ftable.lock);
  return f;
}

// Close file f.  (Decrement ref count, close when reaches 0.)
// 关闭文件。减少文件引用计数，当文件引用计数为0时关闭文件。
void
fileclose(struct file *f)
{
  struct file ff;

  acquire(&ftable.lock); //请求文件表锁
  // 文件引用为0，无法执行关闭操作，引发内核错误
  if(f->ref < 1)
    panic("fileclose");
  // 文件引用计数减1后仍然存在引用，释放文件表锁，函数结束
  if(--f->ref > 0){
    release(&ftable.lock); 
    return;
  }
  // 否则关闭文件，再释放文件表锁
  ff = *f;
  f->ref = 0;
  f->type = FD_NONE;
  release(&ftable.lock);

  // 文件类型为管道类型时，唤醒相应进程或释放管道
  if(ff.type == FD_PIPE) 
    pipeclose(ff.pipe, ff.writable);
  // 文件类型为普通文件类型时，删除内存中对inode的引用
  else if(ff.type == FD_INODE){ 
    begin_op();
    iput(ff.ip);
    end_op();
  }
}

// Get metadata about file f.
// 获取文件的元信息
int
filestat(struct file *f, struct stat *st)
{
  if(f->type == FD_INODE){
    ilock(f->ip);
    stati(f->ip, st);
    iunlock(f->ip);
    return 0;
  }
  return -1;
}

// Read from file f.
// 读取文件信息
int
fileread(struct file *f, char *addr, int n)
{
  int r;

  // 文件不可读
  if(f->readable == 0)
    return -1;
  // 文件为管道类型
  if(f->type == FD_PIPE)
    return piperead(f->pipe, addr, n);
  // 文件为普通文件类型
  if(f->type == FD_INODE){
    ilock(f->ip);
    if((r = readi(f->ip, addr, f->off, n)) > 0)
      f->off += r;
    iunlock(f->ip);
    return r;
  }

  // 读取失败，引发内核错误
  panic("fileread");
}

//PAGEBREAK!
// Write to file f.
// 写文件
int
filewrite(struct file *f, char *addr, int n)
{
  int r;

  // 文件不可写
  if(f->writable == 0)
    return -1;
  // 文件为管道类型
  if(f->type == FD_PIPE)
    return pipewrite(f->pipe, addr, n);
  // 文件为普通文件类型
  if(f->type == FD_INODE){
    // write a few blocks at a time to avoid exceeding
    // the maximum log transaction size, including
    // i-node, indirect block, allocation blocks,
    // and 2 blocks of slop for non-aligned writes.
    // this really belongs lower down, since writei()
    // might be writing a device like the console.
    int max = ((MAXOPBLOCKS-1-1-2) / 2) * 512; // 文件数据最大存储大小
    int i = 0;
    while(i < n){
      int n1 = n - i;
      // 不可超过文件数据最大存储大小
      if(n1 > max)
        n1 = max;

      begin_op();
      ilock(f->ip);
      if ((r = writei(f->ip, addr + i, f->off, n1)) > 0)
        f->off += r;
      iunlock(f->ip);
      end_op();

      // 写入完毕
      if(r < 0)
        break;
      // 仍在写入，但写入数据大小与请求大小不等，发生错误
      if(r != n1)
        panic("short filewrite");
      i += r;
    }
    // 判断写入数据大小与请求写入的数据大小是否一致
    // 不一致则写入失败，否则成功
    return i == n ? n : -1;
  }
  // 发生异常
  panic("filewrite");
}

