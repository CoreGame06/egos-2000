#include "app.h"
#include <stdlib.h>

/* 生产者消费者示例：主线程消费，工作线程生产，共享 count 和锁，最多存 3 个产品。
 * cv_t 用 flag 模拟等待/通知，实际是忙等，不会让线程阻塞；未完整处理跨核可见性。 */
typedef struct {
  int* lock; // 保护共享 count 的锁。
  int flag; // 0 等待，1 已收到通知。
} cv_t;


// 记住使用哪把锁，并初始化通知标志。
void cv_init(cv_t* cv,int *lock)
{
  cv->lock = lock;
  cv->flag =0;
}

// 调用前已持锁；等待时先释放，收到通知后再加锁，调用者仍需用 while 检查条件。
void cv_wait(cv_t* cv)
{
  // 等待期间释放锁，否则另一线程无法生产或消费，条件就不会改变。
  cv->flag=0;
  release(*(cv->lock));
  while(cv->flag==0); // 忙等通知，仍占用 CPU。
  acquire(*(cv->lock));
}

void cv_signal(cv_t* cv)
{
  cv->flag = 1; // 通知等待者重新检查条件。
}

// count 只记录产品数量，两条线程修改它时都要加锁。
int count = 0;
int lock = 0;
cv_t nonfull;  // 还有空位，可以生产。
cv_t nonempty; // 已有产品，可以消费。

// 生产 5 次，满 3 个时等待消费者腾出空位。
void produce(void* arg)
{
  for(int i=0;i<5;i++)
  {
    acquire(lock);
    while(count==3) cv_wait(&nonfull);

    ++count;
    printf("Produce a new item! Count is %d now\n",count);
    cv_signal(&nonempty);
    release(lock);
  }
  // 没有独立线程退出和返回处理，生产完先忙等，主线程退出时一起回收。
  // 这里不能调用 exit，否则会结束整个共享 PID。
  while(1);
}

int main(int argc,char** argv)
{
  cv_init(&nonempty,&lock);
  cv_init(&nonfull,&lock);

  thread_create(produce,NULL); // 新线程执行 produce(NULL)，主线程继续消费。

  // 消费 5 次；没有产品时等待生产者通知。
  for(int i=0;i<5;i++)
  {
    acquire(lock);
    while(count==0) cv_wait(&nonempty);
    --count;
    printf("Consumed an item! Count is %d now\n",count);
    cv_signal(&nonfull);
    release(lock);
  }
  return 0;
}
