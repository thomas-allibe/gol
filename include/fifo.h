// Not high performance if a lot of messages are recieved simultaneously
// because of dynamic array elements moved during dequeue.
// Using circular buffer may be faster, but I have to think about buffer
// resizing!!!
//

#ifndef _FIFO_H_
#define _FIFO_H_

#include "error.h"
#include "types.h"
#include <stdbool.h>
#include <threads.h>

typedef struct FifoMsg {
  i32 state;  // Could represent a state / action / ...
  char *msg;  // If any, alloc & free must be handled by the library user
  void *data; // If any, alloc & free must be handled by the library user
} FifoMsg;

typedef struct Fifo {
  FifoMsg *elems; // Dynamic array (not a list to improve cache locality)
  i32 max_size;   // User defined max size, -1: "unlimited"
  // size_t allocated_size; // Dynamic array allocated size
  // size_t next_dequeue_index; // Index of the next element to dequeue
  // size_t size;               // Number of elements in the queue
  mtx_t mut;       // Thread safety
  cnd_t cnd_empty; // Thread safety
  cnd_t cnd_full;  // Thread safety
  i32 dequeuers;   // Number of dequeuers waiting
  i32 enqueuers;   // Number of enqueuers waiting
} Fifo;

void fifo_create(Fifo *self, i32 max_size, Error *err);
void fifo_destroy(Fifo *self, Error *err);
void fifo_enqueue_msg(Fifo *self, FifoMsg msg, i64 timeout_ms, Error *err);
FifoMsg fifo_dequeue_msg(Fifo *self, i64 timeout_ms, Error *err);

#endif // !_FIFO_H_

//////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////

// #define FIFO_IMPLEMENTATION // For syntax highlight during dev
#ifdef FIFO_IMPLEMENTATION

#include "stb_ds.h"
#include <assert.h>
#include <time.h>

void fifo_create(Fifo *const self, i32 max_size, Error *const err) {
  assert(self && "self can't be NULL");
  assert(err && "err can't be NULL, error handling is important!");

  if (err->status) {
    return;
  }

  if (cnd_init(&self->cnd_empty) == thrd_error) {
    err->msg =
        "Could not initialize Conditional Variable (" error_print_err_location
        ").";
    err->status = true;
    err->code = error_generic;
    return;
  }

  if (cnd_init(&self->cnd_full) == thrd_error) {
    err->msg =
        "Could not initialize Conditional Variable (" error_print_err_location
        ").";
    err->status = true;
    err->code = error_generic;
    return;
  }

  if (mtx_init(&self->mut, mtx_timed) == thrd_error) {
    cnd_destroy(&self->cnd_empty);
    err->msg = "Could not initialize Mutex (" error_print_err_location ").";
    err->status = true;
    err->code = error_generic;
    return;
  }

  self->dequeuers = 0;
  self->max_size = max_size;

  // Success
  return;
}

void fifo_destroy(Fifo *const self, Error *const err) {
  assert(self && "self can't be NULL");
  assert(err && "err can't be NULL, error handling is important!");

  // #TODO: Merge errors
  // #TODO: what happens if a thread is waiting on it?
  cnd_destroy(&self->cnd_empty);
  cnd_destroy(&self->cnd_full);
  mtx_destroy(&self->mut);
  if (self->elems != NULL) {
    arrfree(self->elems);
  }

  // Success
  return;
}

struct timespec fifo_compute_timeout(i64 const timeout_ms, Error *const err) {
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

  if (timeout_ms > 0) {
    timeout.tv_nsec += timeout_ms * TENPOW_6;
    const i64 seconds = timeout.tv_nsec / TENPOW_9;
    timeout.tv_sec += seconds;
    timeout.tv_nsec -= seconds * TENPOW_9;
  } else if (timeout_ms < 0) {
    err->msg = "Invalid timeout, can't be <=0 (" error_print_err_location ").";
    err->status = true;
    err->code = error_generic;
    return timeout;
  }

  // Success
  return timeout;
}

void fifo_enqueue_msg(Fifo *const self, FifoMsg const msg, i64 const timeout_ms,
                      Error *const err) {
  assert(self && "self can't be NULL");
  assert(err && "err can't be NULL, error handling is important!");

  if (err->status) {
    return;
  }

  i32 thrd_code = thrd_success;

  if (timeout_ms >= 0) {
    struct timespec timeout = fifo_compute_timeout(timeout_ms, err);
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

    self->enqueuers += 1;
    if (self->max_size > 0) {
      while (arrlen(self->elems) == self->max_size) {
        thrd_code = cnd_timedwait(&self->cnd_full, &self->mut, &timeout);
        if (thrd_code != thrd_success) {
          self->enqueuers -= 1;
          // Ignore mtx_unlock() error because we are in an error state anyway
          mtx_unlock(&self->mut);

          if (thrd_code == thrd_timedout) {
            err->status = true;
            err->code = error_timeout;
            err->msg = "Couldn't recieve conditional variable signal in time "
                       "(" error_print_err_location ").";
            return;
          } else {
            err->msg = "Couldn't wait on conditional variable signal "
                       "(" error_print_err_location ").";
            err->status = true;
            err->code = error_generic;
            return;
          }
        }
      }
    }
    self->enqueuers -= 1;

    arrput(self->elems, msg);

    if (self->dequeuers) {
      thrd_code = cnd_signal(&self->cnd_empty);
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

    self->enqueuers += 1;
    if (self->max_size > 0) {
      while (!arrlen(self->elems)) {
        thrd_code = cnd_wait(&self->cnd_full, &self->mut);
        if (thrd_code != thrd_success) {
          self->enqueuers -= 1;
          // Ignore mtx_unlock() error because we are in an error state anyway
          mtx_unlock(&self->mut);
          err->msg = "Couldn't wait on conditional variable signal "
                     "(" error_print_err_location ").";
          err->status = true;
          err->code = error_generic;
          return;
        }
      }
    }
    self->enqueuers -= 1;

    arrput(self->elems, msg);

    if (self->dequeuers) {
      thrd_code = cnd_signal(&self->cnd_empty);
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

FifoMsg fifo_dequeue_msg(Fifo *const self, i64 const timeout_ms,
                         Error *const err) {
  assert(self && "self can't be NULL");
  assert(err && "err can't be NULL, error handling is important!");

  FifoMsg msg = {0};

  if (err->status) {
    return msg;
  }

  i32 thrd_code = thrd_success;

  if (timeout_ms >= 0) {
    struct timespec timeout = fifo_compute_timeout(timeout_ms, err);

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
      thrd_code = cnd_timedwait(&self->cnd_empty, &self->mut, &timeout);
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

    if (self->enqueuers) {
      thrd_code = cnd_signal(&self->cnd_full);
      if (thrd_code != thrd_success) {
        // Ignore mtx_unlock() error because we are in an error state anyway
        mtx_unlock(&self->mut);
        err->msg = "Couldn't signal the conditional variable "
                   "(" error_print_err_location ").";
        err->status = true;
        err->code = error_generic;
        return msg;
      }
    }

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
      thrd_code = cnd_wait(&self->cnd_empty, &self->mut);
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

    // Was full and enqueuer is waiting
    if (self->enqueuers && arrlen(self->elems) == self->max_size - 1) {
      thrd_code = cnd_signal(&self->cnd_full);
      if (thrd_code != thrd_success) {
        // Ignore mtx_unlock() error because we are in an error state anyway
        mtx_unlock(&self->mut);
        err->msg = "Couldn't signal the conditional variable "
                   "(" error_print_err_location ").";
        err->status = true;
        err->code = error_generic;
        return msg;
      }
    }

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
