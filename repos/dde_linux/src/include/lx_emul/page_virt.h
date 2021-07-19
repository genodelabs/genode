/*
 * \brief  Lx_emul support for page-struct management
 * \author Norman Feske
 * \date   2021-07-01
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#ifndef _LX_EMUL__PAGE_VIRT_H_
#define _LX_EMUL__PAGE_VIRT_H_

#ifdef __cplusplus
extern "C" {
#endif

struct page;


/*
 * Accessors for the associative data structure implemented in page_virt.cc
 */

void lx_emul_associate_page_with_virt_addr(struct page *, void const *virt);
void lx_emul_disassociate_page_from_virt_addr(void const *virt);
struct page *lx_emul_associated_page(void const *virt, unsigned long size);


/**
 * Return page struct for the page at a given virtual address
 *
 * If no page struct exists for the virtual address, it is created.
 */
struct page *lx_emul_virt_to_pages(void const *virt, unsigned num);


/**
 * Release page structs for specified virtual-address range
 *
 * \param size  size of range in bytes
 */
void lx_emul_forget_pages(void const *virt, unsigned long size);


/**
 * Perform unit test
 */
void lx_emul_associate_page_selftest(void);


#ifdef __cplusplus
}
#endif

#endif /* _LX_EMUL__PAGE_VIRT_H_ */
