#include "types.h"
#include "defs.h"
#include "param.h"
#include "mmu.h"
#include "proc.h"
#include "fs.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "file.h"

#define PIPESIZE 512

struct pipe {
  struct spinlock lock;
  char data[PIPESIZE];
  uint nread;     // number of bytes read 被读取字节数
  uint nwrite;    // number of bytes written 被写入字节数
  int readopen;   // read fd is still open 读文件描述符依旧处于open状态
  int writeopen;  // write fd is still open 写文件描述符依旧处于open状态
};

// 管道分配
int
pipealloc(struct file **f0, struct file **f1)
{
  struct pipe *p;

  p = 0;
  *f0 = *f1 = 0;
  // 文件结构分配失败，则无法分配管道
  if((*f0 = filealloc()) == 0 || (*f1 = filealloc()) == 0)
    goto bad;
  // 无法分配内存，则无法分配管道
  if((p = (struct pipe*)kalloc()) == 0)
    goto bad;
  p->readopen = 1;
  p->writeopen = 1;
  p->nwrite = 0;
  p->nread = 0;
  initlock(&p->lock, "pipe");
  (*f0)->type = FD_PIPE;
  (*f0)->readable = 1;
  (*f0)->writable = 0;
  (*f0)->pipe = p;
  (*f1)->type = FD_PIPE;
  (*f1)->readable = 0;
  (*f1)->writable = 1;
  (*f1)->pipe = p;
  return 0;

//PAGEBREAK: 20
// 分配失败，回退至初始状态
 bad:
  if(p)
    kfree((char*)p);
  if(*f0)
    fileclose(*f0);
  if(*f1)
    fileclose(*f1);
  return -1;
}

// 管道关闭
void
pipeclose(struct pipe *p, int writable)
{
  acquire(&p->lock); // 请求管道锁
  // 如果可写，则将写open设置为0，唤醒写的一方；
  // 否则将读open设置为0，唤醒读
  if(writable){
    p->writeopen = 0;
    wakeup(&p->nread);
  } else {
    p->readopen = 0;
    wakeup(&p->nwrite);
  }
  
  // 释放管道锁
  if(p->readopen == 0 && p->writeopen == 0){
    release(&p->lock);
    kfree((char*)p);// 不可读写，释放管道内存
  } else
    release(&p->lock);
}

//PAGEBREAK: 40
// 管道写
int
pipewrite(struct pipe *p, char *addr, int n)
{
  int i;

  acquire(&p->lock); // 请求获得管道锁，以保护计数器、数据以及相关不变量
  for(i = 0; i < n; i++){
    while(p->nwrite == p->nread + PIPESIZE){  //DOC: pipewrite-full 管道满
      // 如果管道文件不可读或进程被杀，释放管道锁并终止管道写
      if(p->readopen == 0 || myproc()->killed){
        release(&p->lock);
        return -1;
      }
      wakeup(&p->nread); // 通知睡眠中的读者缓冲区中有数据可读
      sleep(&p->nwrite, &p->lock);  //DOC: pipewrite-sleep 管道写进程进入睡眠（睡眠时将释放管道锁）
    }
    p->data[p->nwrite++ % PIPESIZE] = addr[i]; // 将所需读取的数据块内数据赋值给管道数据
  }
  wakeup(&p->nread);  //DOC: pipewrite-wakeup1 通知睡眠中的读者缓冲区中有数据可读
  release(&p->lock); // 释放管道锁
  return n;
}

//管道读
int
piperead(struct pipe *p, char *addr, int n)
{
  int i;

  acquire(&p->lock); // 请求获得管道锁，以保护计数器、数据以及相关不变量
  while(p->nread == p->nwrite && p->writeopen){  //DOC: pipe-empty 读取字节数等于写入字节数（管道为空）且管道可写，则可进入管道读进程可进入睡眠
    // 进程被杀，释放管道锁，终止管道读进程
    if(myproc()->killed){
      release(&p->lock);
      return -1;
    }
    sleep(&p->nread, &p->lock); //DOC: piperead-sleep 管道读进程进入睡眠（睡眠时将释放管道锁）
  }
  for(i = 0; i < n; i++){  //DOC: piperead-copy
    if(p->nread == p->nwrite) // 读取字节数等于写入字节数，表示读取完成
      break;
    addr[i] = p->data[p->nread++ % PIPESIZE]; // 将所需读取的管道数据赋值给数据块内数据
  }
  wakeup(&p->nwrite);  //DOC: piperead-wakeup 通知睡眠中的写者缓冲区中可写
  release(&p->lock);
  return i;
}
