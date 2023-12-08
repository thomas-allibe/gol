
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <threads.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wunused-value"
#define FIFO_IMPLEMENTATION
#include "fifo.h"
#define STB_DS_IMPLEMENTATION
#include "stb_ds.h"
#pragma GCC diagnostic pop

int func1(void *arg) {

  Fifo *fifo = (Fifo *)arg;
  FifoMsg msg = {.data = (void *)1};
  FifoError err = {0};

  // for (double i = 0.0; i < 100000.0; i++) {
  //   fifo_enqueue_msg(fifo, msg, -1, &err);
  // }
  msg.msg = "Hello World";
  fifo_enqueue_msg(fifo, msg, -1, &err);

  msg.state = -1;
  fifo_enqueue_msg(fifo, msg, -1, &err);
  return thrd_success;
}

int func2(void *arg) {

  Fifo *fifo = (Fifo *)arg;
  FifoMsg msg = {.data = (void *)2};
  FifoError err = {0};

  for (double i = 0.0; i < 100000.0; i++) {
    fifo_enqueue_msg(fifo, msg, -1, &err);
  }

  return thrd_success;
}

int main(void) {
  FifoError err = {0};
  Fifo fifo = {0};
  fifo_create(&fifo, &err);

  thrd_t thread1 = {0};
  thrd_t thread2 = {0};
  int res = 0;
  FifoMsg msg = {0};

  if (thrd_create(&thread1, &func1, &fifo) != thrd_success) {
    fprintf(stderr, "Error creating thread 1...\n");
    return -1;
  }

  if (thrd_create(&thread2, &func2, &fifo) != thrd_success) {
    fprintf(stderr, "Error creating thread 2...\n");
    return -1;
  }

  do {
    msg = fifo_dequeue_msg(&fifo, -1, &err);
    if (err.code == fifo_timeout) {
      printf("%s\n", err.msg);
      err.status = false;
      err.code = fifo_success;
    } else if (err.code == fifo_error) {
      printf("error\n");
    } else {
      printf("Received: %s\n", msg.msg);
    }
  } while (msg.state != -1 && !err.status);
  if (thrd_join(thread1, &res)) {
    fprintf(stderr, "Error joining thread 1...\n");
    return -1;
  }

  printf("Thread 1 return code: %d\n", res);

  if (thrd_join(thread2, &res)) {
    fprintf(stderr, "Error joining thread 2...\n");
    return -1;
  }

  printf("Thread 2 return code: %d\n", res);

  fifo_destroy(&fifo, &err);

  return 0;
}
