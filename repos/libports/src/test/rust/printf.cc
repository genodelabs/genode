#include <base/log.h>
extern "C" void print_num(int num) {
  Genode::log("Number from rust: ", num);
}
