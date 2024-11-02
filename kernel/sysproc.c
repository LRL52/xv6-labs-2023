#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"

uint64
sys_exit(void)
{
  int n;
  argint(0, &n);
  exit(n);
  return 0;  // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return fork();
}

uint64
sys_wait(void)
{
  uint64 p;
  argaddr(0, &p);
  return wait(p);
}

uint64
sys_sbrk(void)
{
  uint64 addr;
  int n;

  argint(0, &n);
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;

  backtrace();
  argint(0, &n);
  if(n < 0)
    n = 0;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(killed(myproc())){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

uint64
sys_kill(void)
{
  int pid;

  argint(0, &pid);
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

// If an application calls sigalarm(n, fn), 
// then after every n "ticks" of CPU time that the program consumes, 
// the kernel should cause application function fn to be called. 
uint64
sys_sigalarm(void) {
  int ticks;
  void (*handler)();
  struct proc *p;

  argint(0, &ticks);
  argaddr(1, (uint64*)&handler);

  p = myproc();
  p->nr_ticks = ticks;
  p->remaining_ticks = ticks;
  p->alarm_handler = handler;
  // It' ok to call sigalarm in an alarm handler,
  // so current process can be in an alarm handler.
  // if (p->in_alarm_handler)
  //   panic("sys_sigalarm: already in alarm handler");

  return 0;
}

// User alarm handlers are required to call the 
// sigreturn system call when they have finished.
uint64
sys_sigreturn(void) {
  struct proc *p;

  p = myproc();
  if (!p->in_alarm_handler)
    panic("sys_sigreturn: not in alarm handler");
  p->in_alarm_handler = 0;
  *p->trapframe = *p->alarm_trapframe;

  return p->trapframe->a0;
}