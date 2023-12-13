// Not high performance if a lot of messages are recieved simultaneously
// because of dynamic array elements moved during dequeue.
// Using circular buffer may be faster, but I have to think about buffer
// resizing!!!
//

#ifndef _FIFO_H_
#define _FIFO_H_

#include "error.h"
#include <stdbool.h>
#include <threads.h>

// #define fifo_to_str_literal(s) _fifo_to_str_literal_(s)
// #define _fifo_to_str_literal_(s) #s
//
// #define error_print_err_location __FILE__ ":" fifo_to_str_literal(__LINE__)
//
// typedef enum FifoErrorCode {
//   fifo_success = 0, // It's all good man!
//   fifo_timeout = 1, // IT'S TIME TO STOP!
//   error_generic = 2    // This is fine...
// } FifoErrorCode;
//
// typedef struct FifoError {
//   bool status;        // T: there is an error, F: no error
//   FifoErrorCode code; // Error code to allow switch case on the error
//   char *msg; // Human readable error cause. Do not try to modify it, it's
//   static
// } FifoError;

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

void fifo_create(Fifo *const self, Error *const err);
void fifo_destroy(Fifo *const self, Error *const err);
void fifo_enqueue_msg(Fifo *const self, FifoMsg const msg, long const timeout_s,
                      Error *const err);
FifoMsg fifo_dequeue_msg(Fifo *const self, long const timeout_s,
                         Error *const err);
#endif // !_FIFO_H_

//////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////

// #define FIFO_IMPLEMENTATION
#ifdef FIFO_IMPLEMENTATION

#include "stb_ds.h"
#include <assert.h>
#include <time.h>

void fifo_create(Fifo *const self, Error *const err) {
  assert(self && "self can't be NULL");
  assert(err && "err can't be NULL, error handling is important!");

  if (err->status) {
    return;
  }

  if (cnd_init(&self->cnd) == thrd_error) {
    err->msg =
        "Could not initialize Conditional Variable (" error_print_err_location
        ").";
    err->status = true;
    err->code = error_generic;
    return;
  }

  if (mtx_init(&self->mut, mtx_timed) == thrd_error) {
    cnd_destroy(&self->cnd);
    err->msg = "Could not initialize Mutex (" error_print_err_location ").";
    err->status = true;
    err->code = error_generic;
    return;
  }

  self->dequeuers = 0;

  // Success
  return;
}

void fifo_destroy(Fifo *const self, Error *const err) {
  assert(self && "self can't be NULL");
  assert(err && "err can't be NULL, error handling is important!");

  // #TODO: Merge errors
  // #TODO: what happens if a thread is waiting on it?
  cnd_destroy(&self->cnd);
  mtx_destroy(&self->mut);
  if (self->elems != NULL) {
    arrfree(self->elems);
  }

  // Success
  return;
}

struct timespec fifo_compute_timeout(long const timeout_s, Error *const err) {
  assert(err && "err can't be NULL, error handling is important!");

  struct timespec timeout = {0};

  if (err->status) {
    return timeout;
  }

  if (!timespec_get(&timeout, TIME_UTC)) {
    err->msg = "Could not get timespec (" error_print_err_location ").";
    err->status = true;
    err->code = error_generic;
    return timeout;
  }

  if (timeout_s > 0) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-conversion"
    timeout.tv_sec += (unsigned long)timeout_s;
#pragma GCC diagnostic pop
  } else {
    err->msg = "Invalid timeout, can't be <=0 (" error_print_err_location ").";
    err->status = true;
    err->code = error_generic;
    return timeout;
  }

  // Success
  return timeout;
}

void fifo_enqueue_msg(Fifo *const self, FifoMsg const msg, long const timeout_s,
                      Error *const err) {
  assert(self && "self can't be NULL");
  assert(err && "err can't be NULL, error handling is important!");

  if (err->status) {
    return;
  }

  int thrd_code = thrd_success;

  if (timeout_s >= 0) {
    struct timespec timeout = fifo_compute_timeout(timeout_s, err);
    if (err->status) {
      return;
    }

    thrd_code = mtx_timedlock(&self->mut, &timeout);
    if (thrd_code == thrd_timedout) {
      err->msg =
          "Couldn't lock the mutex in time (" error_print_err_location ").";
      err->status = true;
      err->code = error_timeout;
      return;
    } else if (thrd_code != thrd_success) {
      err->msg = "Couldn't lock the mutex (" error_print_err_location ").";
      err->status = true;
      err->code = error_generic;
      return;
    }

    arrput(self->elems, msg);

    if (self->dequeuers) {
      thrd_code = cnd_signal(&self->cnd);
      if (thrd_code != thrd_success) {
        // Ignore mtx_unlock() error because we are in an error state anyway
        mtx_unlock(&self->mut);
        err->msg = "Couldn't signal the conditional variable "
                   "(" error_print_err_location ").";
        err->status = true;
        err->code = error_generic;
        return;
      }
    }

    thrd_code = mtx_unlock(&self->mut);
    if (thrd_code != thrd_success) {
      err->msg = "Couldn't unlock the mutex (" error_print_err_location ").";
      err->status = true;
      err->code = error_generic;
      return;
    }

  } else {

    thrd_code = mtx_lock(&self->mut);
    if (thrd_code != thrd_success) {
      err->msg = "Couldn't lock the mutex (" error_print_err_location ").";
      err->status = true;
      err->code = error_generic;
      return;
    }

    arrput(self->elems, msg);

    if (self->dequeuers) {
      thrd_code = cnd_signal(&self->cnd);
      if (thrd_code != thrd_success) {
        // Ignore mtx_unlock() error because we are in an error state anyway
        mtx_unlock(&self->mut);
        err->msg = "Couldn't signal the conditional variable "
                   "(" error_print_err_location ").";
        err->status = true;
        err->code = error_generic;
        return;
      }
    }

    thrd_code = mtx_unlock(&self->mut);
    if (thrd_code != thrd_success) {
      err->msg = "Couldn't unlock the mutex (" error_print_err_location ").";
      err->status = true;
      err->code = error_generic;
      return;
    }
  }

  // Success
  return;
}

FifoMsg fifo_dequeue_msg(Fifo *const self, long const timeout_s,
                         Error *const err) {
  assert(self && "self can't be NULL");
  assert(err && "err can't be NULL, error handling is important!");

  FifoMsg msg = {0};

  if (err->status) {
    return msg;
  }

  int thrd_code = thrd_success;

  if (timeout_s >= 0) {
    struct timespec timeout = fifo_compute_timeout(timeout_s, err);

    if (err->status) {
      return msg;
    }

    thrd_code = mtx_timedlock(&self->mut, &timeout);
    if (thrd_code == thrd_timedout) {
      err->msg =
          "Couldn't lock the mutex in time (" error_print_err_location ").";
      err->status = true;
      err->code = error_timeout;
      return msg;
    } else if (thrd_code != thrd_success) {
      err->msg = "Couldn't lock the mutex (" error_print_err_location ").";
      err->status = true;
      err->code = error_generic;
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
          err->code = error_timeout;
          err->msg = "Couldn't recieve conditional variable signal in time "
                     "(" error_print_err_location ").";
          return msg;
        } else {
          err->msg = "Couldn't wait on conditional variable signal "
                     "(" error_print_err_location ").";
          err->status = true;
          err->code = error_generic;
          return msg;
        }
      }
    }
    self->dequeuers -= 1;

    msg = self->elems[0];
    arrdel(self->elems, 0);

    thrd_code = mtx_unlock(&self->mut);
    if (thrd_code != thrd_success) {
      err->msg = "Couldn't unlock the mutex (" error_print_err_location ").";
      err->status = true;
      err->code = error_generic;
      return msg;
    }

  } else {

    thrd_code = mtx_lock(&self->mut);
    if (thrd_code != thrd_success) {
      err->msg = "Couldn't lock the mutex (" error_print_err_location ").";
      err->status = true;
      err->code = error_generic;
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
                   "(" error_print_err_location ").";
        err->status = true;
        err->code = error_generic;
        return msg;
      }
    }
    self->dequeuers -= 1;

    msg = self->elems[0];
    arrdel(self->elems, 0);

    thrd_code = mtx_unlock(&self->mut);
    if (thrd_code == thrd_error) {
      err->msg = "Couldn't unlock the mutex (" error_print_err_location ").";
      err->status = true;
      err->code = error_generic;
      return msg;
    }
  }

  // Success
  return msg;
}
#endif // FIFO_IMPLEMENTATION
