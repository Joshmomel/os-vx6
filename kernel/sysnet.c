//
// network system calls.
//

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"
#include "fs.h"
#include "sleeplock.h"
#include "file.h"
#include "net.h"

struct sock
{
  struct sock *next;    // the next socket in the list
  uint32 raddr;         // the remote IPv4 address
  uint16 lport;         // the local UDP port number
  uint16 rport;         // the remote UDP port number
  struct spinlock lock; // protects the rxq
  struct mbufq rxq;     // a queue of packets waiting to be received
};

static struct spinlock lock;
static struct sock *sockets;

void sockinit(void)
{
  initlock(&lock, "socktbl");
}

int sockalloc(struct file **f, uint32 raddr, uint16 lport, uint16 rport)
{
  struct sock *si, *pos;

  si = 0;
  *f = 0;
  if ((*f = filealloc()) == 0)
    goto bad;
  if ((si = (struct sock *)kalloc()) == 0)
    goto bad;

  // initialize objects
  si->raddr = raddr;
  si->lport = lport;
  si->rport = rport;
  initlock(&si->lock, "sock");
  mbufq_init(&si->rxq);
  (*f)->type = FD_SOCK;
  (*f)->readable = 1;
  (*f)->writable = 1;
  (*f)->sock = si;

  // add to list of sockets
  acquire(&lock);
  pos = sockets;
  while (pos)
  {
    if (pos->raddr == raddr &&
        pos->lport == lport &&
        pos->rport == rport)
    {
      release(&lock);
      goto bad;
    }
    pos = pos->next;
  }
  si->next = sockets;
  sockets = si;
  release(&lock);
  return 0;

bad:
  if (si)
    kfree((char *)si);
  if (*f)
    fileclose(*f);
  return -1;
}

//
// Your code here.
//
// Add and wire in methods to handle closing, reading,
// and writing for network sockets.
//

void sockclose(struct sock *sock)
{
  // printf("##sock close\n");
  acquire(&sock->lock);
  wakeup(&sock->rxq);
  release(&sock->lock);

  acquire(&lock);
  struct sock *sop = sockets;
  if (sop->next == 0)
  {
    sockets = 0;
    // printf("sop next == 0\n");
  }
  while (sop && sop->next)
  {
    if (sop->next->raddr == sock->raddr &&
        sop->next->lport == sock->lport &&
        sop->next->rport == sock->rport)
    {
      sop->next = sock->next;
      break;
    }
    sop = sop->next;
  }
  release(&lock);

  kfree((char *)sock);
}

int sockwrite(struct sock *sock, uint64 addr, int n)
{
  // printf("##sock write\n");
  struct mbuf *buf = mbufalloc(sizeof(struct eth) + sizeof(struct ip) + sizeof(struct udp));
  mbufput(buf, n);

  pagetable_t pagetable = myproc()->pagetable;
  if (copyin(pagetable, buf->head, addr, n) == -1)
  {
    mbuffree(buf);
    return -1;
  }
  // printf("buf pointer in write is: %s\n", buf->head);
  net_tx_udp(buf, sock->raddr, sock->lport, sock->rport);

  return n;
}

int sockread(struct sock *sock, uint64 addr, int n)
{
  // printf("##socket read\n");
  acquire(&sock->lock);
  if (mbufq_empty(&sock->rxq))
  {
    while (mbufq_empty(&sock->rxq))
      sleep(&sock->rxq, &sock->lock);
  }

  struct mbuf *buf = mbufq_pophead(&sock->rxq);
  int len = n < buf->len ? n : buf->len;
  pagetable_t pagetable = myproc()->pagetable;
  if (copyout(pagetable, addr, buf->head, len) == -1)
  {
    release(&sock->lock);
    mbuffree(buf);
    return -1;
  }
  // printf("buf pointer in read is %s\n", buf->head);
  release(&sock->lock);
  mbuffree(buf);
  return len;
}

// called by protocol handler layer to deliver UDP packets
void sockrecvudp(struct mbuf *m, uint32 raddr, uint16 lport, uint16 rport)
{
  //
  // Your code here.
  //
  // Find the socket that handles this mbuf and deliver it, waking
  // any sleeping reader. Free the mbuf if there are no sockets
  // registered to handle it.
  //
  // printf("##sockrecvudp called %s\n", m->head);
  struct sock *so = sockets;
  acquire(&lock);
  while (so)
  {
    if (so->raddr == raddr &&
        so->lport == lport &&
        so->rport == rport)
    {
      break;
    }
    so = so->next;
  }
  release(&lock);
  if (so == 0)
  {
    mbuffree(m);
    return;
  }

  acquire(&so->lock);
  mbufq_pushtail(&so->rxq, m);
  release(&so->lock);
  wakeup(&so->rxq);
}
