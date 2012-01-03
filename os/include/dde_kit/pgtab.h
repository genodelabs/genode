/*
 * \brief   Virtual page-table facility
 * \author  Christian Helmuth
 * \date    2008-08-15
 */

/*
 * Copyright (C) 2008-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__DDE_KIT__PGTAB_H_
#define _INCLUDE__DDE_KIT__PGTAB_H_

#include <dde_kit/types.h>

/**
 * Set virtual->physical mapping for VM pages
 *
 * \param   virt   virtual start address for region
 * \param   phys   physical start address for region
 * \param   pages  number of pages in region
 */
void dde_kit_pgtab_set_region(void *virt, dde_kit_addr_t phys, unsigned pages);

/**
 * Set virtual->physical mapping for VM region
 *
 * \param   virt  virtual start address for region
 * \param   phys  physical start address for region
 * \param   size  number of bytes in region
 */
void dde_kit_pgtab_set_region_with_size(void *virt, dde_kit_addr_t phys,
                                        dde_kit_size_t size);

/**
 * Clear virtual->physical mapping for VM region
 *
 * \param   virt  virtual start address for region
 */
void dde_kit_pgtab_clear_region(void *virt);

/**
 * Get physical address for virtual address
 *
 * \param   virt  virtual address
 *
 * \return physical address
 */
dde_kit_addr_t dde_kit_pgtab_get_physaddr(void *virt);

/**
 * Get virtual address for physical address
 *
 * \param   phys  physical address
 *
 * \return  virtual address
 */
dde_kit_addr_t dde_kit_pgtab_get_virtaddr(dde_kit_addr_t phys);

/**
 * Get size of VM region.
 *
 * \param   virt  virtual address
 *
 * \return  VM region size (in bytes)
 */
dde_kit_size_t dde_kit_pgtab_get_size(void *virt);

#endif /* _INCLUDE__DDE_KIT__PGTAB_H_ */
