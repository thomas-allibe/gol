#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <threads.h>
#include <time.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wunused-value"
#define STB_DS_IMPLEMENTATION
#include "stb_ds.h"
#pragma GCC diagnostic pop

#define FIFO_ERR_MAX_MSG_LENGTH 256

typedef enum FifoErrorCode {
  fifo_success = 0, // It's all good man!
  fifo_timeout = 1, // IT'S TIME TO STOP!
  fifo_error = 2    // This is fine...
} FifoErrorCode;

typedef struct FifoError {
  bool status;        // T: there is an error, F: no error
  FifoErrorCode code; // Error code to allow switch case on the error
  char msg[FIFO_ERR_MAX_MSG_LENGTH]; // Human readable error cause
} FifoError;

typedef struct FifoMsg {
  int state;  // Could represent a state / action / ...
  char *msg;  // If any, alloc & free must be handled by the library user
  void *data; // If any, alloc & free must be handled by the library user
} FifoMsg;

typedef struct Fifo {
  FifoMsg *elems; // Dynamic array handled by stb_ds.h
  mtx_t mut;
  cnd_t cnd;
  int dequeuers;
} Fifo;

void fifo_create(Fifo *const self, FifoError *const err) {
  assert(self && "self can't be NULL");
  assert(err && "err can't be NULL, error handling is important!");

  if (cnd_init(&self->cnd) == thrd_error) {
    // #TODO: Error Return
    err->status = true;
    err->code = fifo_error;
    return;
  }

  if (mtx_init(&self->mut, mtx_timed) == thrd_error) {
    // #TODO: Error Return
    err->status = true;
    err->code = fifo_error;
    return;
  }

  self->dequeuers = 0;

  // Success
  return;
}

void fifo_destroy(Fifo *const self, FifoError *const err) {
  assert(self && "self can't be NULL");
  assert(err && "err can't be NULL, error handling is important!");

  // #TODO: what happens if a thread is waiting on it?
  cnd_destroy(&self->cnd);
  mtx_destroy(&self->mut);
  arrfree(self->elems);

  // Success
  return;
}

struct timespec fifo_compute_timeout(double const timeout_s,
                                     FifoError *const err) {
  assert(err && "err can't be NULL, error handling is important!");

  struct timespec timeout = {0};

  if (!timespec_get(&timeout, TIME_UTC)) {
    // #TODO: Error Return
    err->status = true;
    err->code = fifo_error;
    return timeout;
  }

  double integer_part;
  timeout.tv_nsec += modf(timeout_s, &integer_part) * 1000000000.0;
  timeout.tv_sec += (unsigned long)integer_part;

  // Success
  return timeout;
}

void fifo_enqueue_msg(Fifo *const self, FifoMsg const msg,
                      double const timeout_s, FifoError *const err) {
  assert(self && "self can't be NULL");
  assert(err && "err can't be NULL, error handling is important!");

  int thrd_code = thrd_success;

  if (timeout_s > 0.0) {
    struct timespec timeout = fifo_compute_timeout(timeout_s, err);

    if (err->status) {
      return;
    }

    thrd_code = mtx_timedlock(&self->mut, &timeout);
    if (thrd_code == thrd_timedout) {
      // #TODO: Error Return
      err->status = true;
      err->code = fifo_timeout;
      return;
    } else if (thrd_code != thrd_success) {
      // #TODO: Error Return
      err->status = true;
      err->code = fifo_error;
      return;
    }

    arrput(self->elems, msg);

    if (self->dequeuers) {
      thrd_code = cnd_signal(&self->cnd);
      if (thrd_code != thrd_success) {
        // #TODO: Error Return
        err->status = true;
        err->code = fifo_error;
        return;
      }
    }

    thrd_code = mtx_unlock(&self->mut);
    if (thrd_code != thrd_success) {
      // #TODO: Error Return
      err->status = true;
      err->code = fifo_error;
      return;
    }

  } else {

    thrd_code = mtx_lock(&self->mut);
    if (thrd_code != thrd_success) {
      // #TODO: Error Return
      err->status = true;
      err->code = fifo_error;
      return;
    }

    arrput(self->elems, msg);

    if (self->dequeuers) {
      thrd_code = cnd_signal(&self->cnd);
      if (thrd_code != thrd_success) {
        // #TODO: Error Return
        err->status = true;
        err->code = fifo_error;
        return;
      }
    }

    thrd_code = mtx_unlock(&self->mut);
    if (thrd_code != thrd_success) {
      // #TODO: Error Return
      err->status = true;
      err->code = fifo_error;
      return;
    }
  }

  // Success
  return;
}

FifoMsg fifo_dequeue_msg(Fifo *const self, double const timeout_s,
                         FifoError *const err) {
  assert(self && "self can't be NULL");
  assert(err && "err can't be NULL, error handling is important!");

  FifoMsg msg = {0};
  int thrd_code = thrd_success;

  if (timeout_s > 0.0) {
    struct timespec timeout = fifo_compute_timeout(timeout_s, err);

    if (err->status) {
      return msg;
    }

    thrd_code = mtx_timedlock(&self->mut, &timeout);
    if (thrd_code == thrd_timedout) {
      // #TODO: Error Return
      err->status = true;
      err->code = fifo_timeout;
      return msg;
    } else if (thrd_code != thrd_success) {
      // #TODO: Error Return
      err->status = true;
      err->code = fifo_error;
      return msg;
    }

    self->dequeuers += 1;
    while (!arrlen(self->elems)) {
      thrd_code = cnd_timedwait(&self->cnd, &self->mut, &timeout);
      if (thrd_code == thrd_timedout) {
        // #TODO: Error Return
        err->status = true;
        err->code = fifo_timeout;
        return msg;
      } else if (thrd_code != thrd_success) {
        // #TODO: Error Return
        err->status = true;
        err->code = fifo_error;
        return msg;
      }
    }
    self->dequeuers -= 1;

    msg = self->elems[0];
    arrdel(self->elems, 0);

    thrd_code = mtx_unlock(&self->mut);
    if (thrd_code != thrd_success) {
      // #TODO: Error Return
      err->status = true;
      err->code = fifo_error;
      return msg;
    }

  } else {

    thrd_code = mtx_lock(&self->mut);
    if (thrd_code != thrd_success) {
      // #TODO: Error Return
      err->status = true;
      err->code = fifo_error;
      return msg;
    }

    self->dequeuers += 1;
    while (!arrlen(self->elems)) {
      thrd_code = cnd_wait(&self->cnd, &self->mut);
      if (thrd_code != thrd_success) {
        // #TODO: Error Return
        err->status = true;
        err->code = fifo_error;
        return msg;
      }
    }
    self->dequeuers -= 1;

    msg = self->elems[0];
    arrdel(self->elems, 0);

    thrd_code = mtx_unlock(&self->mut);
    if (thrd_code == thrd_error) {
      // #TODO: Error Return
      err->status = true;
      err->code = fifo_error;
      return msg;
    }
  }

  // Success
  return msg;
}

int func1(void *arg) {

  Fifo *fifo = (Fifo *)arg;
  FifoMsg msg = {.data = (void *)1};
  char *MSGS[] = {"Hello", "World", "foo", "Quit"};
  FifoError err = {0};

  msg.msg = MSGS[0];
  fifo_enqueue_msg(fifo, msg, -1, &err);
  msg.msg = MSGS[1];
  thrd_sleep(&(struct timespec){.tv_sec = 2}, NULL);
  fifo_enqueue_msg(fifo, msg, -1, &err);
  msg.msg = MSGS[2];
  thrd_sleep(&(struct timespec){.tv_sec = 2}, NULL);
  fifo_enqueue_msg(fifo, msg, -1, &err);
  msg.msg = MSGS[2];
  msg.state = -1;
  thrd_sleep(&(struct timespec){.tv_sec = 2}, NULL);
  fifo_enqueue_msg(fifo, msg, -1, &err);

  return thrd_success;
}

int func2(void *arg) {

  Fifo *fifo = (Fifo *)arg;
  FifoMsg msg = {.data = (void *)2};
  char *MSGS[] = {"Hello", "World", "foo"};
  FifoError err = {0};

  msg.msg = MSGS[0];
  thrd_sleep(&(struct timespec){.tv_sec = 1}, NULL);
  fifo_enqueue_msg(fifo, msg, -1, &err);
  msg.msg = MSGS[1];
  thrd_sleep(&(struct timespec){.tv_sec = 2}, NULL);
  fifo_enqueue_msg(fifo, msg, -1, &err);
  msg.msg = MSGS[2];
  thrd_sleep(&(struct timespec){.tv_sec = 2}, NULL);
  fifo_enqueue_msg(fifo, msg, -1, &err);

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
    msg = fifo_dequeue_msg(&fifo, 0.5, &err);
    if (err.code == fifo_timeout) {
      printf("timeout, break\n");
    } else if (err.code == fifo_error) {

      printf("error, break\n");
    }

    printf("Msg recieved from thread %ld: %s\n", (long)msg.data, msg.msg);
  } while (msg.state != -1 && !err.status);

  if (thrd_join(thread1, &res)) {
    fprintf(stderr, "Error joining thread 2...\n");
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
