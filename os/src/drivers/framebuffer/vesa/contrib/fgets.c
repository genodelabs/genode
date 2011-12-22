

#include <stdio.h>
#include <flux/x86/pc/direct_cons.h>

char *fgets(char *str, int size, FILE *stream);

char
*fgets(char *str, int size, FILE *stream)
{
  int i;
  int c;
  
  if (size <= 0)
    return NULL;

  i = 0;
  while (i < size)
    {
      c = direct_cons_getchar();
      if (c == '\r')
	{
	  putchar('\n');
	  putchar('\r');
	  if (i == 0)
	    {
	      str[0] = '\n';
	      str[1] = '\0';
	      return NULL;
	    }
	  break;
	}
      else if (c == 8)
	{
	  if (i>0)
	    {
	      putchar(c);
	      putchar(' ');
	      putchar(c);
	      i--;
	    }
	}
      else
	{
	  putchar(c);
	  str[i++] = c;
	}
    }
  if (++i < size)
    str[i] = '\n';
  if (i < size)
    str[i] = '\0';

  return str;
}

