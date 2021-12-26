// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.


#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"

#define NBUCKET 13

extern uint ticks;

struct {
  struct spinlock lock;
  struct buf buf[NBUF];
} bcache;

struct {
  struct spinlock lock;
  struct buf head;
} bbucket[NBUCKET];

void
newbuf(struct buf *buf, uint dev, uint blockno) {
  buf->dev = dev;
  buf->blockno = blockno;
  buf->valid = 0;
  buf->refcnt = 1;
  buf->lasttick = ticks;
}

void
binit(void)
{
  struct buf *b;

  initlock(&bcache.lock, "bcache");

  for(int i = 0; i < NBUCKET; i++) {
	initlock(&bbucket[i].lock, "bcache_bucket");
  }

  // Create linked list of buffers
  for(b = bcache.buf; b < bcache.buf+NBUF; b++){
    initsleeplock(&b->lock, "buffer");
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;
  struct buf *bufend;

  int bid = blockno % NBUCKET;
  acquire(&bbucket[bid].lock);

  // Is the block already cached?
  for(b = bbucket[bid].head.next; b; b = b->next) {
	  if(b->dev == dev && b->blockno == blockno) {
		  b->refcnt++;
		  release(&bbucket[bid].lock);
		  acquiresleep(&b->lock);
		  return b;
	  }
  }

  release(&bbucket[bid].lock);

  // 大小锁问题，统一为先加大锁，后小锁
  acquire(&bcache.lock);
  acquire(&bbucket[bid].lock);

  bufend = &bbucket[bid].head;
  for(b = bbucket[bid].head.next; b; b = b->next) {
	  bufend = b;
	  if(b->dev == dev && b->blockno == blockno) {
		  b->refcnt++;
		  release(&bbucket[bid].lock);
		  release(&bcache.lock);
		  acquiresleep(&b->lock);
		  return b;
	  }
  }

  struct buf *lrubuf = 0;
  while(1) {
	  for(b = bcache.buf; b < bcache.buf + NBUF; b++) {
		  if(b->refcnt == 0) {
			 if(!lrubuf || lrubuf->lasttick > b->lasttick) {
				 lrubuf = b;
			 } 
		  }
	  }
	
	  if(!lrubuf) {
		  panic("bget: no buffers");
	  }
	  int lrubid = lrubuf->blockno % NBUCKET;
	  
	  if(lrubuf->lasttick == 0) {
		  if(lrubuf->refcnt != 0) {
			  printf("ERROR1: %d!\n", lrubuf->refcnt);
		  }
		  newbuf(lrubuf, dev, blockno);
		  lrubuf->prev = bufend;
		  bufend->next = lrubuf;
		  lrubuf->next = 0;
		  break;
	  } else if(lrubid == bid) {
		  newbuf(lrubuf, dev, blockno);
		  break;
	  } else {
		  acquire(&bbucket[lrubid].lock);
		  if(lrubuf->refcnt != 0) {
			  // 锁所对应资源管控不当导致的。
			  // 在原本的设计中，buf结构中的refcnt是由粒度为整个cache
			  // 的锁管理的。
			  // 然而，随着粒度细化，出现了可以只获取桶锁就能够访问
			  // buf中refcnt的代码，即get开头部分。以及下面3个函数
			  // 这就导致我们在对refcnt进行访问时，需要对其加桶锁
			  // 而读取refcnt并没有加桶锁
			  // 解决方法：读取时就要加锁；或者二次检验
			  release(&bbucket[lrubid].lock);
			  continue;
		  }
		  newbuf(lrubuf, dev, blockno);
		  lrubuf->prev->next = lrubuf->next;
		  if(lrubuf->next) {
			  lrubuf->next->prev = lrubuf->prev;
		  }
		  lrubuf->prev = bufend;
		  bufend->next = lrubuf;
		  lrubuf->next = 0;
		  release(&bbucket[lrubid].lock);
		  break;
	  }
 
  }
  release(&bbucket[bid].lock);
  release(&bcache.lock);
  acquiresleep(&lrubuf->lock);
  return lrubuf;
/*
  for(b = bcache.head.next; b != &bcache.head; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
	  // sleeplock的存在使得只有一个线程能够操纵buf，
	  // 那么refcnt为什么要存在呢？
	  // 为了统计正在等待sleeplock释放锁的有几个线程
	  // 这些线程也要使用这个buf。
	  // 对于refcnt的操作，必须要在获得cache锁的前提下操作
      b->refcnt++;
      release(&bcache.lock);
      acquiresleep(&b->lock);
      return b;
    }
  }
  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
  for(b = bcache.head.prev; b != &bcache.head; b = b->prev){
    if(b->refcnt == 0) {
      b->dev = dev;
      b->blockno = blockno;
      b->valid = 0;
      b->refcnt = 1;
      release(&bcache.lock);
      acquiresleep(&b->lock);
      return b;
    }
  }
  */
}

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  int bid = b->blockno % NBUCKET;
  acquire(&bbucket[bid].lock);

  b->refcnt--;
  
  release(&bbucket[bid].lock);
}

void
bpin(struct buf *b) {
  int bid = b->blockno % NBUCKET;
  acquire(&bbucket[bid].lock);

  b->refcnt++;
  
  release(&bbucket[bid].lock);
}

void
bunpin(struct buf *b) {
  int bid = b->blockno % NBUCKET;
  acquire(&bbucket[bid].lock);

  b->refcnt--;
  
  release(&bbucket[bid].lock);
}


