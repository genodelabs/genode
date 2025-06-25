/**
 * \brief  Shadow copy of linux/swapops.h
 * \author Josef Soentgen
 * \date   2022-01-14
 */

#ifndef _LINUX__SWAPOPS_H_
#define _LINUX__SWAPOPS_H_

swp_entry_t pte_to_swp_entry(pte_t pte);
int is_swap_pte(pte_t pte);
int non_swap_entry(swp_entry_t entry);
unsigned swp_type(swp_entry_t entry);
#endif
