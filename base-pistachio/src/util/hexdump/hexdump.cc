#include <base/printf.h>
#include <util/hexdump.h>

using Genode::printf;

void
Util::hexdump(const unsigned char *addr, unsigned long length)
{
  hexdump(addr, length, (unsigned long)addr);
}

void
Util::hexdump(const unsigned char *addr, unsigned long length, unsigned long real_addr)
{
  unsigned long addr_int = (unsigned int)addr;
  const unsigned long step = 16;

  real_addr = real_addr&(~(step-1));

  for (unsigned long pos = addr_int&(~(step-1)); pos < (addr_int + length); 
       pos += step, real_addr += step) {

    printf(" 0x%08lx:", real_addr);
    for (unsigned int lpos = pos; lpos < (pos + step); lpos ++) {

      if ((lpos & 3) == 0) printf(" ");

      if ((lpos < addr_int) || (lpos > (addr_int + length)))
        printf("  ");
      else
        printf(" %02x", addr[lpos - addr_int]);
    }

    printf(" | ");

    for (unsigned int lpos = pos; lpos < (pos + step); lpos ++) {

      if ((lpos & 3) == 0) printf(" ");

      unsigned char ch;

      if ((lpos < addr_int) || (lpos > (addr_int + length)))
        ch = ' ';
      else 
        ch = addr[lpos - addr_int];

      if ((ch < 32) || (ch >= 127))
        ch = '.';

      printf("%c", ch);
    }

    printf("\n");
  }
}
