/*
 * \brief  Shadow copy of asm/uaccess_64.h
 * \author Alexander Boettcher
 * \date   2022-02-17
 */

#ifndef _ASM__UACCESS_64_H_
#define _ASM__UACCESS_64_H_

unsigned long raw_copy_from_user(void *to, const void * from, unsigned long n);
unsigned long raw_copy_to_user(void *to, const void *from, unsigned long n);

unsigned long clear_user(void *mem, unsigned long len);

#endif /* _ASM__UACCESS_64_H_ */
