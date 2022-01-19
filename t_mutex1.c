/**
  * 锁的实现
  * 
  *  gcc -o mutex t_mutex1.c -lpthread
 * 
 
 */
#include "t_mutex.h"
#include "atomic_x86.h"

#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <assert.h>
#include <linux/futex.h>
#include <limits.h>
#include <sys/syscall.h>



lock_t lock;
int share_int = 0;


 
int sys_futex(void *addr1, int op, int val1, struct timespec *timeout, void *addr2, int val3)
{
    //syscall 202
    return syscall(SYS_futex, addr1, op, val1, timeout, addr2, val3);
}

// int compare_and_swap(int exchange_value, volatile int *dest, int compare_value);

// int compare_and_swap(int exchange_value, volatile int *dest, int compare_value)
// {
//     int r = atomic_cmpxchg(exchange_value, dest, compare_value);
//     return r != *dest ? 1 : 0;
// }




void procyield(int cycles)
{

    lock_spin(cycles);
}

void lock_init(lock_t *m)
{
    m->state = 0;
    m->sema = 0;
}
void lock_mutex(lock_t *m)
{

    if (!compare_and_swap(&(m->state), mutex_locked, mutex_unlocked))
    {
        // printf("------ get lock: %lu\n", pthread_self());
        return;
    }

    int iter = 0;
    int spin = 0;
    //int wait = (m->state);
    if (IS_MP() > 1)
    {
        spin = SPIN_COUNT;
    }
    // 1.自旋 最多${SPIN_COUNT}次 每次 ${CYCLES}
    while (true)
    {
        for (int i = 0; i < spin; i++)
        {
            //未加锁时
            while ((m->state) == mutex_unlocked)
            {
                if (!compare_and_swap(&(m->state),mutex_locked , mutex_unlocked)) {
                    return;
				}
            }
         //   printf("------ procyield: %lu state=%d\n", pthread_self(),m->state);
            procyield(CYCLES);
        }

        // 2.让出cpu
        for (int i = 0; i < 1; i++)
        {
            while ( (m->state) == mutex_unlocked)
            {
                if (!compare_and_swap(&(m->state), mutex_locked ,mutex_unlocked) ){
                    return;
				}
            }
            // printf("------ osyield: %lu state=%d\n", pthread_self(),m->state);
            osyield();
        }

        // 3.sleeping
        int v = atomic_xchg(mutex_sleeping, &(m->state)) ;
        if (v == mutex_unlocked)
        {

            atomic_xchg(mutex_locked, &(m->state)) ;
            return;
        }
      
        // printf("------ sys_futex wait: %lu state=%d\n", pthread_self(),m->state);
        sys_futex(&( m->state), FUTEX_WAIT, mutex_sleeping, NULL, NULL, 0);
    }
}
//解锁
void unlock_mutex(lock_t *m)
{
    int v = atomic_xchg(mutex_unlocked, &(m->state));
    if (v == mutex_unlocked)
    {

        assert("unlock of unlocked lock");
    }
    if (v == mutex_sleeping)
    {
       //  m->state = mutex_unlocked;
        atomic_store_lock_int(mutex_unlocked,&(m->state));
        sys_futex( &(m->state), FUTEX_WAKE, 1, NULL, NULL, 0);  //此地址上锁的 唤醒一个
        
       //  printf("------ unlock: %lu  state=%d\n", pthread_self(),m->state);
    }
    else
    {
        assert("unlock error ");
        //*mutex->state=unlock_mutex;
    }
}

// 生产者
void *producer(void *arg)
{
    printf("------ producer: %lu \n", pthread_self() );
    while (1)
    {
       lock_mutex(&lock);
        share_int= rand() % 1000;
        printf("------ producer: %lu, %d\n", pthread_self(), share_int);
        unlock_mutex(&lock);
        sleep(1);// 
       
    }
    return NULL;
}

void *customer(void *arg)
{
    printf("------ customer: %lu \n", pthread_self() );
    while (1)
    {

         lock_mutex(&lock);

         if(share_int==-1){
             unlock_mutex(&lock);
            continue;
         }
        printf("------ customer: %lu, %d\n", pthread_self(), share_int);
        share_int=-1; //消费后重置
        // sleep(1);
        unlock_mutex(&lock);
       
    }
    return NULL;
}

int main()
{

    // int val = 0;
    // //  procyield(276447232);
   

    // lock_t lock;
    // lock_init(&lock);
    // lock_mutex(&lock);
    // printf("osyield\n");
    // osyield();
    // osyield();
    // osyield();
    // printf("end\n");

    pthread_t p1, p2;
    lock_init(&lock);

    // 创建生产者线程
    pthread_create(&p1, NULL, producer, NULL);
    
  
    // 创建消费者线程
    pthread_create(&p2, NULL, customer, NULL);

    // 阻塞回收子线程
    pthread_join(p1, NULL);
    pthread_join(p2, NULL);

    return 0;
}