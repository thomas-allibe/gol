#ifndef _ERROR_H_
#define _ERROR_H_

#define error_to_str_literal(s) _error_to_str_literal_(s)
#define _error_to_str_literal_(s) #s

#define error_print_err_location __FILE__ ":" error_to_str_literal(__LINE__)

typedef enum ErrorCode {
  error_ok = 0,      // It's all good man!
  error_generic = 1, // This is fine...
  error_timeout = 2, // IT'S TIME TO STOP!
} ErrorCode;

typedef struct Error {
  bool status;    // T: there is an error, F: no error
  ErrorCode code; // Error code to allow switch case on the error
  char *msg; // Human readable error cause. Do not try to modify it, it's static
} Error;

#endif // _ERROR_H_
