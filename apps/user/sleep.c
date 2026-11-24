#include "app.h"

int main() {
  // 请求睡眠 5 秒，由 sys_proc 设置睡眠时刻，调度器跳过尚未到时的进程。
  const uint usec_cnt = 5000000;
  printf("Start to sleep for %d microseconds.\n\r", usec_cnt);
  sleep(usec_cnt);
  printf("Woke up again after %d microseconds.\n\r", usec_cnt);
}
