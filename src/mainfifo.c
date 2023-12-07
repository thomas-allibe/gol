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

#define fifo_to_str_literal(s) _fifo_to_str_literal_(s)
#define _fifo_to_str_literal_(s) #s

#define fifo_print_err_location __FILE__ ":" fifo_to_str_literal(__LINE__)

typedef enum FifoErrorCode {
  fifo_success = 0, // It's all good man!
  fifo_timeout = 1, // IT'S TIME TO STOP!
  fifo_error = 2    // This is fine...
} FifoErrorCode;

typedef struct FifoError {
  bool status;        // T: there is an error, F: no error
  FifoErrorCode code; // Error code to allow switch case on the error
  char *msg; // Human readable error cause. Do not try to modify it, it's static
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

  if (err->status) {
    return;
  }

  if (cnd_init(&self->cnd) == thrd_error) {
    err->msg =
        "Could not initialize Conditional Variable (" fifo_print_err_location
        ").";
    err->status = true;
    err->code = fifo_error;
    return;
  }

  if (mtx_init(&self->mut, mtx_timed) == thrd_error) {
    cnd_destroy(&self->cnd);
    err->msg = "Could not initialize Mutex (" fifo_print_err_location ").";
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
  if (self->elems != NULL) {
    arrfree(self->elems);
  }

  // Success
  return;
}

struct timespec fifo_compute_timeout(long const timeout_s,
                                     FifoError *const err) {
  assert(err && "err can't be NULL, error handling is important!");

  struct timespec timeout = {0};

  if (err->status) {
    return timeout;
  }

  if (!timespec_get(&timeout, TIME_UTC)) {
    err->msg = "Could not get timespec (" fifo_print_err_location ").";
    err->status = true;
    err->code = fifo_error;
    return timeout;
  }

  if (timeout_s > 0) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-conversion"
    timeout.tv_sec += (unsigned long)timeout_s;
#pragma GCC diagnostic pop
  } else {
    err->msg = "Invalid timeout, can't be <=0 (" fifo_print_err_location ").";
    err->status = true;
    err->code = fifo_error;
    return timeout;
  }

  // Success
  return timeout;
}

void fifo_enqueue_msg(Fifo *const self, FifoMsg const msg, long const timeout_s,
                      FifoError *const err) {
  assert(self && "self can't be NULL");
  assert(err && "err can't be NULL, error handling is important!");

  if (err->status) {
    return;
  }

  int thrd_code = thrd_success;

  if (timeout_s > 0) {
    struct timespec timeout = fifo_compute_timeout(timeout_s, err);
    if (err->status) {
      return;
    }

    thrd_code = mtx_timedlock(&self->mut, &timeout);
    if (thrd_code == thrd_timedout) {
      err->msg =
          "Couldn't lock the mutex in time (" fifo_print_err_location ").";
      err->status = true;
      err->code = fifo_timeout;
      return;
    } else if (thrd_code != thrd_success) {
      err->msg = "Couldn't lock the mutex (" fifo_print_err_location ").";
      err->status = true;
      err->code = fifo_error;
      return;
    }

    arrput(self->elems, msg);

    if (self->dequeuers) {
      thrd_code = cnd_signal(&self->cnd);
      if (thrd_code != thrd_success) {
        // Ignore mtx_unlock() error because we are in an error state anyway
        mtx_unlock(&self->mut);
        err->msg =
            "Couldn't signal the conditional variable (" fifo_print_err_location
            ").";
        err->status = true;
        err->code = fifo_error;
        return;
      }
    }

    thrd_code = mtx_unlock(&self->mut);
    if (thrd_code != thrd_success) {
      err->msg = "Couldn't unlock the mutex (" fifo_print_err_location ").";
      err->status = true;
      err->code = fifo_error;
      return;
    }

  } else {

    thrd_code = mtx_lock(&self->mut);
    if (thrd_code != thrd_success) {
      err->msg = "Couldn't lock the mutex (" fifo_print_err_location ").";
      err->status = true;
      err->code = fifo_error;
      return;
    }

    arrput(self->elems, msg);

    if (self->dequeuers) {
      thrd_code = cnd_signal(&self->cnd);
      if (thrd_code != thrd_success) {
        // Ignore mtx_unlock() error because we are in an error state anyway
        mtx_unlock(&self->mut);
        err->msg =
            "Couldn't signal the conditional variable (" fifo_print_err_location
            ").";
        err->status = true;
        err->code = fifo_error;
        return;
      }
    }

    thrd_code = mtx_unlock(&self->mut);
    if (thrd_code != thrd_success) {
      err->msg = "Couldn't unlock the mutex (" fifo_print_err_location ").";
      err->status = true;
      err->code = fifo_error;
      return;
    }
  }

  // Success
  return;
}

FifoMsg fifo_dequeue_msg(Fifo *const self, long const timeout_s,
                         FifoError *const err) {
  assert(self && "self can't be NULL");
  assert(err && "err can't be NULL, error handling is important!");

  FifoMsg msg = {0};

  if (err->status) {
    return msg;
  }

  int thrd_code = thrd_success;

  if (timeout_s > 0) {
    struct timespec timeout = fifo_compute_timeout(timeout_s, err);

    if (err->status) {
      return msg;
    }

    thrd_code = mtx_timedlock(&self->mut, &timeout);
    if (thrd_code == thrd_timedout) {
      err->msg =
          "Couldn't lock the mutex in time (" fifo_print_err_location ").";
      err->status = true;
      err->code = fifo_timeout;
      return msg;
    } else if (thrd_code != thrd_success) {
      err->msg = "Couldn't lock the mutex (" fifo_print_err_location ").";
      err->status = true;
      err->code = fifo_error;
      return msg;
    }

    self->dequeuers += 1;
    while (!arrlen(self->elems)) {
      thrd_code = cnd_timedwait(&self->cnd, &self->mut, &timeout);
      if (thrd_code != thrd_success) {
        self->dequeuers -= 1;
        // Ignore mtx_unlock() error because we are in an error state anyway
        mtx_unlock(&self->mut);

        if (thrd_code == thrd_timedout) {
          err->status = true;
          err->code = fifo_timeout;
          err->msg = "Couldn't recieve conditional variable signal in time "
                     "(" fifo_print_err_location ").";
          return msg;
        } else {
          err->msg = "Couldn't wait on conditional variable signal "
                     "(" fifo_print_err_location ").";
          err->status = true;
          err->code = fifo_error;
          return msg;
        }
      }
    }
    self->dequeuers -= 1;

    msg = self->elems[0];
    arrdel(self->elems, 0);

    thrd_code = mtx_unlock(&self->mut);
    if (thrd_code != thrd_success) {
      err->msg = "Couldn't unlock the mutex (" fifo_print_err_location ").";
      err->status = true;
      err->code = fifo_error;
      return msg;
    }

  } else {

    thrd_code = mtx_lock(&self->mut);
    if (thrd_code != thrd_success) {
      err->msg = "Couldn't lock the mutex (" fifo_print_err_location ").";
      err->status = true;
      err->code = fifo_error;
      return msg;
    }

    self->dequeuers += 1;
    while (!arrlen(self->elems)) {
      thrd_code = cnd_wait(&self->cnd, &self->mut);
      if (thrd_code != thrd_success) {
        self->dequeuers -= 1;
        // Ignore mtx_unlock() error because we are in an error state anyway
        mtx_unlock(&self->mut);
        err->msg = "Couldn't wait on conditional variable signal "
                   "(" fifo_print_err_location ").";
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
      err->msg = "Couldn't unlock the mutex (" fifo_print_err_location ").";
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

  thrd_sleep(&(struct timespec){.tv_sec = 2}, NULL);
  msg.msg = MSGS[0];
  fifo_enqueue_msg(fifo, msg, -1, &err);
  if (err.status) {
    printf("Error Enqueue 1\n");
  }
  msg.msg = MSGS[1];
  thrd_sleep(&(struct timespec){.tv_sec = 2}, NULL);
  fifo_enqueue_msg(fifo, msg, -1, &err);
  if (err.status) {
    printf("Error Enqueue 2\n");
  }
  msg.msg = MSGS[2];
  thrd_sleep(&(struct timespec){.tv_sec = 2}, NULL);
  fifo_enqueue_msg(fifo, msg, -1, &err);
  if (err.status) {
    printf("Error Enqueue 3\n");
  }
  msg.msg = MSGS[3];
  msg.state = -1;
  thrd_sleep(&(struct timespec){.tv_sec = 2}, NULL);
  fifo_enqueue_msg(fifo, msg, -1, &err);
  if (err.status) {
    printf("Error Enqueue 4\n");
  }

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
    msg = fifo_dequeue_msg(&fifo, 1, &err);
    if (err.code == fifo_timeout) {
      printf("%s\n", err.msg);
      err.status = false;
      err.code = fifo_success;
    } else if (err.code == fifo_error) {
      printf("error\n");
    } else {
      printf("Msg recieved from thread %ld: %s\n", (long)msg.data, msg.msg);
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
