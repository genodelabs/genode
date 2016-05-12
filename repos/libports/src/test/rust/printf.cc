#include <base/printf.h>
extern "C" void print_num(int num) {
  Genode::printf("Number from rust: %d \n",num);
}
