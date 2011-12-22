
#include <stdarg.h>
#include <stdio.h>

void
printk(const char *format,...)
{
  va_list list;
  va_start(list, format);
  vprintf(format, list);
  va_end(list);
}
