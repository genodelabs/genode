/**
 * \brief  Shadow copy of asm/uaccess.h
 * \author Josef Soentgen
 * \date   2022-01-14
 */

#ifndef _ASM__UACCESS_H_
#define _ASM__UACCESS_H_

#include_next <asm/uaccess.h>

#undef put_user
#undef get_user

#define get_user(x, ptr) ({  (x)   = *(ptr); 0; })
#define put_user(x, ptr) ({ *(ptr) =  (x);   0; })

#undef __put_user
#define __put_user(x, ptr) ({ *(ptr) =  (x);   0; })

#endif
