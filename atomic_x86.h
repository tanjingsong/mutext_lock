#ifndef ATOMIC_X86_H
#define ATOMIC_X86_H
#include <sys/sysinfo.h>
#define LOCK_IF_MP(mp) "cmp $0, " mp "; je 1f; lock; 1: "
// cpu核心数
#define IS_MP() get_nprocs_conf()

// // cas 比较交换
// int atomic_cmpxchg(int exchange_value, volatile int *dest, int compare_value);
// //交换 自带lock
// int atomic_xchg(int exchange_value, volatile int *dest);

// //增加值
// int atomic_add(int add_value, volatile int *dest);
// //自增 ++
// void atomic_inc(volatile int *dest);
// //自减--
// void atomic_dec(volatile int *dest);
// void atomic_store_int(int store_value, volatile int *dest);
// //存储值 atomic_xchg 交换
// void atomic_store_lock_int(int store_value, volatile int *dest);

//换种方式宏定义cas   http://lkml.org/lkml/2007/5/1/408
#define __arch_compare_and_exchange_val_32_acq(mem, newval, oldval) \
  ({ __typeof (*mem) ret; \
    __asm __volatile ("lock\n" "cmpxchgl %2, %1\n"		 \
		       : "=a" (ret), "=m" (*mem)		 \
		       : "r" ((int) (newval)), "m" (*mem),	 \
			 "0" ((int) (oldval)));		 \
     ret; })
#define compare_and_swap(mem, newval, oldval) \
  __arch_compare_and_exchange_val_32_acq(mem, newval, oldval)

#define __arch_compare_and_exchange_val_64_acq(mem, newval, oldval) \
  ({ __typeof (*mem) ret; \
    __asm __volatile ("lock\n" "cmpxchgq %q2, %1\n"		 \
		       : "=a" (ret), "=m" (*mem)		 \
		       : "r" ((long int) (newval)), "m" (*mem),	 \
			 "0" ((long int) (oldval)));		 \
     ret; })
#define compare_and_swap_ptr(mem, newval, oldval) \
  __arch_compare_and_exchange_val_64_acq(mem, newval, oldval)

 


static inline  int atomic_cmpxchg(int new_value, volatile int *dest, int old_value)
{
 
    int mp = IS_MP();
    __asm__ volatile(LOCK_IF_MP("%4") "cmpxchgl %1,(%3)"
                     : "=a"(new_value)
                     : "r"(new_value), "a"(old_value), "r"(dest), "r"(mp)
                     : "cc", "memory");
    return new_value;
}

static inline int atomic_xchg(int exchange_value, volatile int *dest)
{
    __asm__ volatile("xchgl (%2),%0"
                     : "=r"(exchange_value)
                     : "0"(exchange_value), "r"(dest)
                     : "memory");
    return exchange_value;
}

static inline int atomic_add(int add_value, volatile int *dest)
{
    int addend = add_value;
    int mp = IS_MP();
    __asm__ volatile(LOCK_IF_MP("%3") "xaddl %0,(%2)"
                     : "=r"(addend)
                     : "0"(addend), "r"(dest), "r"(mp)
                     : "cc", "memory");
    return addend + add_value;
}

static inline void atomic_inc(volatile int *dest)
{
    int mp = IS_MP();
    __asm__ volatile(LOCK_IF_MP("%1") "addl $1,(%0)"
                     :
                     : "r"(dest), "r"(mp)
                     : "cc", "memory");
}
static inline void atomic_dec(volatile int *dest)
{
    int mp = IS_MP();
    __asm__ volatile(LOCK_IF_MP("%1") "subl $1,(%0)"
                     :
                     : "r"(dest), "r"(mp)
                     : "cc", "memory");
}
static inline void atomic_store_int(int store_value, volatile int *dest) { *dest = store_value; }
void atomic_store_lock_int(int store_value, volatile int *dest)
{
    atomic_xchg(store_value, dest);
}
#endif
