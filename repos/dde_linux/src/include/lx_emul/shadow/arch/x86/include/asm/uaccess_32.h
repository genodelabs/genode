/*
 * \brief  Shadow copy of asm/uaccess_32.h
 * \author Alexander Boettcher
 * \date   2022-02-17
 */

#ifndef _ASM__UACCESS_32_H_
#define _ASM__UACCESS_32_H_

unsigned long raw_copy_from_user(void *to, const void * from, unsigned long n);
unsigned long raw_copy_to_user(void *to, const void *from, unsigned long n);

int __copy_from_user_inatomic_nocache(void *dst, const void __user *src,
                                      unsigned size);
#endif /* _ASM__UACCESS_32_H_ */
