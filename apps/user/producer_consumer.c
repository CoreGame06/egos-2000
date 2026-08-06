#include "app.h"
#include <stdlib.h>

typedef struct {
  int* lock;
  int flag;
} cv_t;


void cv_init(cv_t* cv,int *lock)
{
  cv->lock = lock;
  cv->flag =0;
}

void cv_wait(cv_t* cv)
{
  cv->flag=0;
  release(*(cv->lock));
  while(cv->flag==0); // 纯自旋等待，依靠时钟中断切换线程，避免 IPC 拥堵
  acquire(*(cv->lock));
}

void cv_signal(cv_t* cv)
{
  cv->flag = 1;
}

// 生产者消费者模型
int count = 0;
int lock = 0;
cv_t nonfull;  // 没空
cv_t nonempty; // 没满

// 生产者线程入口
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
  while(1); // 线程结束时挂起，不能调用 exit(0)，会把同一个pid全部杀死。主线程和子线程这里公用一个pid
}

int main(int argc,char** argv)
{
  cv_init(&nonempty,&lock);
  cv_init(&nonfull,&lock);

  thread_create(produce,NULL);

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