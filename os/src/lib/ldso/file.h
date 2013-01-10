/*
 * \brief  Meta-information for Genode on Linux
 * \author Sebastian Sumpf <Sebastian.Sumpf@genode-labs.com>
 * \date   2010-01-04
 */

/*
 * Copyright (C) 2010-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _FILE_H_
#define _FILE_H_

#include <machine/elf.h>

#ifdef __cplusplus
extern "C" {
#endif

int find_binary_name(int fd, char *buf, size_t buf_size);

/**
 * Map file into address space
 *
 * \param fd   File handle
 * \param segs Elf segment descriptors (must currently be two)
 *
 * \return  Address on success; MAP_FAILED otherwise
 */
void *genode_map(int fd, Elf_Phdr **segs);

#ifdef __cplusplus
}
#endif

#endif /* _FILE_H_ */
