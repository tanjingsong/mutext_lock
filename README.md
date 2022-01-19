参考：

[C|并发编程|互斥锁实现 - 知乎 (zhihu.com)](https://zhuanlan.zhihu.com/p/68750396)

[Operating Systems: Three Easy Pieces (wisc.edu)](https://pages.cs.wisc.edu/~remzi/OSTEP/) https://pages.cs.wisc.edu/~remzi/OSTEP/Chinese/28.pdf

 [Futexes的基础知识 - Eli Bendersky的网站 (thegreenplace.net)](https://eli.thegreenplace.net/2018/basics-of-futexes)

[Locks实现:背后不为人知的故事 - MySpace (hitzhangjie.pro)](https://www.hitzhangjie.pro/blog/2021-04-17-locks实现那些不为人知的故事/)

[MESI协议 - 维基百科，自由的百科全书 (wikipedia.org)](https://zh.wikipedia.org/wiki/MESI协议)

https://os2.unexploitable.systems/cal.html  （ **LEC 13:** Lock and Synchronization  **LEC 14:** Concurrency Bugs and Deadlock）

https://docs.huihoo.com/gnu_linux/own_os/preparing-asm_3.htm   内联
代码参考：

https://github.com/malbrain/mutex

go 1.14 src/runtime/lock_futex.go

openjdk1.8 hotspot\src\os_cpu\linux_x86\vm\atomic_linux_x86.inline.hpp


-------------------------------------------------------------------------------------------



### 自旋

```c
typedef struct __lock_t { int flag; } lock_t;

void init(lock_t *mutex) {
// 0 -> lock is available, 1 -> held
 mutex->flag = 0;
 }

 void lock(lock_t *mutex) {
 while (mutex->flag == 1) // TEST the flag   Line a
 ; // spin-wait (do nothing)
 mutex->flag = 1; // now SET it!             Line b
 }

 void unlock(lock_t *mutex) {
 mutex->flag = 0;
```

当flag置1时，锁被获得，而flag置0时，锁被释放。而当锁没有被释放时，程序将会不断检测flag，而不做任何实际事情，因此被称作**自旋**；

自旋会耗费比较多cpu资源。

### 原子交换

由于关中断方式无法在多处理器上运行，所以需要让硬件支持，最简单的硬件支持是测试并设置（**test-and-set**），也称为原子交换（**atomic exchange**）



```c
int TestAndSet(int *old_ptr, int new) {
int old = *old_ptr; // fetch old value at old_ptr
*old_ptr = new; // store ’new’ into old_ptr
return old;
}

typedef struct __lock_t {
int flag;
} lock_t;

 void init(lock_t *lock) {
 // 0: lock is available, 1: lock is held
 lock->flag = 0;
 }

 void lock(lock_t *lock) {
 while (TestAndSet(&lock->flag, 1) == 1)
; // spin-wait (do nothing)
}

 void unlock(lock_t *lock) {
 lock->flag = 0;
}
```

 另一种形式为（CompareAndSwap），作用类似 。

```c
int CompareAndSwap(int *ptr, int expected, int new) {
 int original = *ptr;
 if (original == expected)
 *ptr = new;
 return original;
 }
void lock(lock_t *lock) {
 while (CompareAndSwap(&lock->flag, 0, 1) == 1)
 ; // spin
}




```

从这种交替执行可以看出，很容易构造出两个线程都将标志设置为 1，都能进入临界区的场景。没有满足最基本的要求：互斥。 性能问题主要是线程在等待已经被持有的锁时，采用了自旋等待（spin-waiting）的技术，就是不停地检查标志的值。自旋等待在等待其他线程释放锁的时候会浪费时间。



 在没有硬件支持是无法实现，因此产生了一些指令，在`x86`中，是`xchg`指令。其功能是返回旧值，更新为新值，C的代码描述如上：

```
 int CompareAndSwap(int exchange_value, volatile int *dest, int compare_value){
   
    __asm__ volatile("lock;cmpxchgl %1,(%3)"
                     : "=a"(exchange_value)
                     : "r"(exchange_value), "a"(compare_value), "r"(dest), "r"(mp)
                     : "cc", "memory");
    return exchange_value;
}
```



在竞争锁时一些线程始终获取不到锁造成饥饿。



**饥饿Starvation**

由于CPU调度并不保证先试图获取锁的必定能先获得，可能出现某个线程很久无法获得锁的情况。一个简单的想法就是使用队列，保证FIFO。

先以FetchAndAdd作为原子性的后置++操作。

```c
int FetchAndAdd(int *ptr) {
 int old = *ptr;
 *ptr = old + 1;
 return old;
}

typedef struct __lock_t {
 int ticket;
 int turn;
 } lock_t;

void lock_init(lock_t *lock) {
 lock->ticket = 0;
 lock->turn = 0;
 }

 void lock(lock_t *lock) {
 int myturn = FetchAndAdd(&lock->ticket);
 while (lock->turn != myturn)
; // spin
 }

 void unlock(lock_t *lock) {
 lock->turn = lock->turn + 1;
 }
```

turn表示当前排队拿到的号码，ticket表示手中的号码，每一个 线程用完之后就呼叫下一个号码。

此方法能保证所有线程都能获取锁，而不会导致某一个线程饥饿。

缺点自旋浪费CPU时间，如何避免？

### sleeping+自旋

由于自旋锁导致每个线程都在执行while操作，空转造成了极大浪费，因此一种改进思路是：在没有获得锁之前，令线程直接沉睡，释放cpu资源进行等待。而当释放锁时，再唤醒等待线程。  我们使用queue作为数据结构，但是维护一个显式的链表。

```c
typedef struct __lock_t {
 int flag;
 int guard;
 queue_t *q;
 } lock_t;

 void lock_init(lock_t *m) {
 m->flag = 0;
 m->guard = 0;
 queue_init(m->q);
 }
```

lock_t:

这里的flag表示锁有没有被线程需求，锁可以同时被多个线程所等候，仅当没有线程等候时才会置0。

而guard是lock和unlock过程的一个自旋锁。在过程结束后自动释放。(basically as a spin-lock around the flag and queue manipulations the lock is using)

```c
void lock(lock_t *m) {
 while (TestAndSet(&m->guard, 1) == 1)
 ; //acquire guard lock by spinning
 if (m->flag == 0) {
 m->flag = 1; // lock is acquired
 m->guard = 0;
 } else {
 queue_add(m->q, gettid());
 m->guard = 0;
 park();
 }
 }
```

lock:

当锁中队列为空时: 置flag为1,即flag锁被占用

当锁中队列不为空时: 入队，使用park操作令线程休眠等待唤醒。

```c
void unlock(lock_t *m) {
while (TestAndSet(&m->guard, 1) == 1)
 ; //acquire guard lock by spinning
 if (queue_empty(m->q))
 m->flag = 0; // let go of lock; no one wants it
 else
 unpark(queue_remove(m->q)); // hold lock
// (for next thread!)
 m->guard = 0;
```

unlock:

当锁中队列为空时：置flag为0，即此锁闲置

当锁中队列不为空时：出队，使用unpark操作唤醒下一个线程并释放锁

**Buggy**

```c
 queue_add(m->q, gettid());
 m->guard = 0;             Line a   
 park();                   Line b
```

假如在line a和line b之间正好有一个线程unlock了，那么将会唤醒当前正在加锁的线程，然后再运行line b使得当前线程进入休眠，而队列中当前线程却已经出队。这样一来，陷入休眠的当前线程就不再可以被唤醒了。

为了解决这个问题，如果能直接让ab原子性就好了，然而实际情况却很难做到。

我们可以特异性针对上面的问题处理，例如某种实现中，setpark函数可以令程序进入准备park的状态，**如果在park之前进程已经被unpark，那么park将直接返回**。

```c
queue_add(m->q, gettid());
setpark();
m->guard = 0;                
park();    
```



### 两阶段锁(Two-phase)

两阶段锁的第一阶段会先自旋一段时间，希望可以获得锁。但是如果自旋阶段没有获得锁，第二阶段调用者会睡眠，直到锁可用。

Linux的Mutex实现机制。

```c
 void mutex_lock (int *mutex) {
 int v;
 /* Bit 31 was clear, we got the mutex (the fastpath) */
//自旋锁！
 if (atomic_bit_test_set (mutex, 31) == 0)
 return;
//维护等待队列长度！
 atomic_increment (mutex);
//这里存在一个问题，假如后面setpark部分continue，那么会存在两个醒着的线程抢夺锁。如果刚刚被唤醒的线程抢不到的话
//原本的队首又得重新进队尾了，那就很迷。可能需要调度算法，保证刚刚唤醒的线程能先执行？
 while (1) {
//被unlock唤醒了！！获取锁然后维护等待队列长度
 if (atomic_bit_test_set (mutex, 31) == 0) {
 atomic_decrement (mutex);
 return;
 }
 /* We have to waitFirst make sure the futex value
 we are monitoring is truly negative (locked). */

//类似setpark!防止v = *mutex;前面被插入unlock
v = *mutex;
 if (v >= 0)
 continue;

//类似setpark!防止v = *mutex;后面被插入unlock
 futex_wait (mutex, v);
 }
 }

 void mutex_unlock (int *mutex) {
 /* Adding 0x80000000 to counter results in 0 if and
 only if there are not other interested threads */
//解锁，如果等待队列长度是0就不用唤醒！不把这个逻辑放futex_wake是为了减少sys call的开销。
 if (atomic_add_zero (mutex, 0x80000000))
 return;

/* There are other threads waiting for this mutex,
 wake one of them up. */
//唤醒队首线程！
 futex_wake (mutex);
 }
```

Two-phase 锁意识到对于那些将会马上被释放的锁，使用自旋锁更有益处。而唤醒等操作需要使用更多的sys-call，因此会增大开销。

futex_wait和futex_wake会在内核态维护一个mutex对应的队列。

在第一阶段，线程将会自旋若干次，试图获取锁。

一旦第一阶段没有完成，则会进入第二阶段，线程沉睡，直到锁被释放后将线程唤醒。

上述linux的实现只自旋了一次，但是也可以使用有固定自旋次数的循环。





### 锁膨胀过程

优先尝试走CAS、Spin，挂起等待。
