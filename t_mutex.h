
#ifndef T_MUTEX_H
#define T_MUTEX_H
#include <stdbool.h>
// 自旋cycles次，每次自旋执行PAUSE指令
// 	PAUSE
// 	SUBL	$1, AX
// 	JNZ	again
// 	RET
#define lock_spin(c) __asm__ volatile("again:\n\r"      \
                                      "pause\n\r"       \
                                      "subl $1, %0\n\r" \
                                      "jnz again"       \
                                      :                 \
                                      : "a"(c)          \
                                      : "cc", "memory")

// void lock_spin  (int  c) {

// 	  for (int idx = 0; idx < c; idx++){
//             asm ("pause");
//       }
// }

//系统调用24# 让出cpu
#define osyield() __asm__("movl	$24, %eax\n\r" \
                          "SYSCALL")
#define CYCLES 30

#define R_OK 0
#define R_ERR 1
#define EBUY 2
#define SPIN_COUNT 4
#define can_spin(iter) iter < SPIN_COUNT ? true : false

int mutex_unlocked = 0; //未锁
int mutex_locked = 1;   //已锁
int mutex_sleeping = 2;

typedef struct __lock_t
{
    //locked标识
    int state;
    int sema;
 
} lock_t;

/**
 * 自旋 cycles 个时钟周期
 */
void procyield(int cycles);

void lock_init(lock_t *m);
void lock_mutex(lock_t *m);
void unlock_mutex(lock_t *m);

#endif