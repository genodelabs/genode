/*
 * \brief  Special handling for the ARM architecture
 * \author Sebastian Sumpf <Sebastian.Sumpf@genode-labs.com>
 * \date   2010-07-05
 */

/*
 * Copyright (C) 2010-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */
#include "rtld.h"
#include <fcntl.h>
#include <machine/elf.h>
#include <sys/param.h>
#include <unistd.h>
#include "dl_extensions.h"

/*
 * cbass: Added EHABI exception table section type
 *
 */
#ifndef PT_ARM_EXIDX
#define PT_ARM_EXIDX  0x70000001
#endif

extern Obj_Entry *obj_list;

typedef struct exidx_t {
	unsigned long base;
	int           count;
} exidx_t;


/*
 * Scan for exception index section and parse information
 */
static void platform_section(Elf_Phdr *phdr, void **priv)
{
	exidx_t *exidx;
	switch (phdr->p_type) {

		case PT_ARM_EXIDX:
			exidx = (exidx_t *)xmalloc(sizeof(exidx_t));
			exidx->base  = phdr->p_vaddr;
			/* Each exception table entry is 8 Byte */
			exidx->count = phdr->p_memsz / 8;
			*priv = (void *)exidx;
			return;
	}
}


/*
 * Read program header and setup exception information
 */
static void find_exidx(Obj_Entry *obj)
{
	char *phdr = file_phdr(obj->path, (void *)obj->mapbase);

	Elf_Ehdr *ehdr = (Elf_Ehdr *)phdr;
	Elf_Phdr *ph_table = (Elf_Phdr *)(phdr + ehdr->e_phoff);

	unsigned i;
	size_t start = ~0;
	size_t end = ~0;

	for (i = 0; i < ehdr->e_phnum; i++) {
		platform_section(&ph_table[i], &obj->priv);

		/* determine map size */
		if (ph_table[i].p_type == PT_LOAD) {

			if (start == ~0)
				start = trunc_page(ph_table[i].p_vaddr);

			end = round_page(ph_table[i].p_vaddr + ph_table[i].p_memsz);
		}
	}

	/* since linker is not setup by map_object map info is not set correctly */
	if (obj->rtld) {
		obj->vaddrbase = (Elf_Addr)obj->mapbase;
		obj->mapsize   = end - start;
	}
}


/*
 * Find exceptions index that matches given IP (PC)
 */
unsigned long dl_unwind_find_exidx(unsigned long pc, int *pcount)
{
	Obj_Entry *obj;
	extern Obj_Entry obj_rtld;
	caddr_t addr = (caddr_t)pc;

	/* 
	 * Used during startup before ldso's main function is called and therefore
	 * obj_list has not been initialized
	 */
	Obj_Entry *list = obj_list;
	if (!list)
		list = &obj_rtld;

	for (obj = list; obj != NULL; obj = obj->next) {

		/* initialize exceptions for object */
		if (!obj->priv)
			find_exidx(obj);

		if (addr >= obj->mapbase && addr < obj->mapbase + obj->mapsize && obj->priv) {
			*pcount = ((exidx_t *)obj->priv)->count;
			return (unsigned long)(((exidx_t *)obj->priv)->base + obj->mapbase - obj->vaddrbase);
		}
	}

	*pcount = 0;
	return 0;
}

