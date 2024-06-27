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

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6,3,0)
unsigned long __must_check clear_user(void __user *mem, unsigned long len);
unsigned long __must_check __clear_user(void __user *mem, unsigned long len);
#endif

#endif /* _ASM__UACCESS_32_H_ */
