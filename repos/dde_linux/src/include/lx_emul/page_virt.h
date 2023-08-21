/*
 * \brief  Lx_emul support for page-struct management
 * \author Norman Feske
 * \author Christian Helmuth
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
struct page *lx_emul_associated_page(void const *virt);


/**
 * Return page struct for the page at a given virtual address (virt_to_page.cc)
 *
 * As in Linux page structs of contiguous pages of attached DMA/RAM buffers
 * (i.e., page ranges) are contiguous too.
 */
struct page *lx_emul_virt_to_page(void const *virt);


/**
 * Release page structs for specified virtual-address range (virt_to_page.c)
 */
void lx_emul_remove_page_range(void const *virt_addr, unsigned long size);

/**
 * Initialize page structs for specified virtual-address range (virt_to_page.c)
 */
void lx_emul_add_page_range(void const *virt_addr, unsigned long size);


#ifdef __cplusplus
}
#endif

#endif /* _LX_EMUL__PAGE_VIRT_H_ */
