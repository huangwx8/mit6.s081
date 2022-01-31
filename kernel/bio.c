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

struct {
  struct spinlock lock;
  struct buf buf[NBUF];

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  struct buf head;

#define HASH_TABLE_SIZE 65537
#define HASH_FUNC(dev, blockno) (((dev + 7) * (blockno + 11) + 57) % HASH_TABLE_SIZE)
  int key2bidx[HASH_TABLE_SIZE];
  int bidx2key[NBUF];
  uint timestamp[NBUF];
  
  struct spinlock buflocks[NBUF];
} bcache;

void addkey(uint64 key, int bindex) {
  bcache.key2bidx[key] = bindex;
  bcache.bidx2key[bindex] = key;
}

void delkey(int bindex) {
  uint64 key = bcache.bidx2key[bindex];
  if (key != -1) {
    bcache.key2bidx[key] = -1;
    bcache.bidx2key[bindex] = -1;
  }
}

uint64 findevictee() {
  uint eticks = 0xffffffff;
  uint64 eindex = -1;
  struct buf* b;

  for (uint64 i = 0; i < NBUF; i++) {
    acquire(&bcache.buflocks[i]);
    b = &bcache.buf[i];
    if (b->refcnt != 0) {
      release(&bcache.buflocks[i]);
      continue;
    }
    if (bcache.timestamp[i] <= eticks) {
      // release old evictee's lock
      if (eindex != -1)
        release(&bcache.buflocks[eindex]);
      // update
      eticks = bcache.timestamp[i];
      eindex = i;
      // hold new evictee's lock
      continue;
    }
    release(&bcache.buflocks[i]);
  }

  return eindex;
}

void
binit(void)
{
  struct buf *b;

  initlock(&bcache.lock, "bcache");

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

  for (int i = 0; i < HASH_TABLE_SIZE; i++)
    bcache.key2bidx[i] = -1;
  for (int i = 0; i < NBUF; i++) {
    bcache.bidx2key[i] = -1;
    bcache.timestamp[i] = 0;
    initlock(&bcache.buflocks[i], "bcache");
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  //printf("bget dev = %d, blockno = %d\n", dev, blockno);
  struct buf *b;

  //acquire(&bcache.lock);

  // Is the block already cached?
  /*for(b = bcache.head.next; b != &bcache.head; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.lock);
      acquiresleep(&b->lock);
      return b;
    }
  }*/
  uint64 key, bindex;
  key = HASH_FUNC(dev, blockno);
  if ((bindex = bcache.key2bidx[key]) != -1) {
    acquire(&bcache.buflocks[bindex]);
    b = &bcache.buf[bindex];
    if (b->dev != dev || b->blockno != blockno)
      panic("hash conflict");
    b->refcnt++;
    release(&bcache.buflocks[bindex]);
    //release(&bcache.lock);
    acquiresleep(&b->lock);
    return b;
  }

  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
  /*for(b = bcache.head.prev; b != &bcache.head; b = b->prev){
    if(b->refcnt == 0) {
      b->dev = dev;
      b->blockno = blockno;
      b->valid = 0;
      b->refcnt = 1;
      release(&bcache.lock);
      acquiresleep(&b->lock);
      return b;
    }
  }*/

  bindex = findevictee(); // could implictly acquire evictee's lock here
  if (bindex != -1) {
    delkey(bindex);
    addkey(key, bindex);
    b = &bcache.buf[bindex];
    b->dev = dev;
    b->blockno = blockno;
    b->valid = 0;
    b->refcnt = 1;
    release(&bcache.buflocks[bindex]);
    //release(&bcache.lock);
    acquiresleep(&b->lock);
    return b;
  }
  
  panic("bget: no buffers");
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
  //printf("brelse dev = %d, blockno = %d\n", b->dev, b->blockno);
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  //acquire(&bcache.lock);

  uint64 key, bindex;
  key = HASH_FUNC(b->dev, b->blockno);
  bindex = bcache.key2bidx[key];

  acquire(&bcache.buflocks[bindex]);
  b->refcnt--;
  if (b->refcnt == 0) {
    // no one is waiting for it.
    /*b->next->prev = b->prev;
    b->prev->next = b->next;
    b->next = bcache.head.next;
    b->prev = &bcache.head;
    bcache.head.next->prev = b;
    bcache.head.next = b;*/
    bcache.timestamp[bindex] = ticks;
  }
  release(&bcache.buflocks[bindex]);

  //release(&bcache.lock);
}

void
bpin(struct buf *b) {
  acquire(&bcache.lock);
  b->refcnt++;
  release(&bcache.lock);
}

void
bunpin(struct buf *b) {
  acquire(&bcache.lock);
  b->refcnt--;
  release(&bcache.lock);
}


