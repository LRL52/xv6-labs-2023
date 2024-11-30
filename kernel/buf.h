struct buf {
  int valid;   // has data been read from disk?
  int disk;    // does disk "own" buf?
  uint dev;
  uint blockno;
  struct sleeplock lock;
  struct spinlock spinlock; // protect fields except data
  uint refcnt;
  uchar data[BSIZE];
};

