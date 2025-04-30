#include "../../include/common.h"
#include <stdarg.h>
#include <time.h>

/**
 * Print error message and exit
 */
void error_exit(const char *msg)
{
  perror(msg);
  exit(EXIT_FAILURE);
}

/**
 * Log message with timestamp
 */
void log_message(const char *format, ...)
{
  va_list args;
  time_t now;
  struct tm *time_info;
  char time_str[20];

  time(&now);
  time_info = localtime(&now);
  strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", time_info);

  printf("[%s] ", time_str);

  va_start(args, format);
  vprintf(format, args);
  va_end(args);

  printf("\n");
  fflush(stdout);
}