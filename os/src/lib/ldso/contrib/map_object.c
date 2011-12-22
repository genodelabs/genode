/*-
 * Copyright 1996-1998 John D. Polstra.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD: src/libexec/rtld-elf/map_object.c,v 1.18.8.1 2009/04/15 03:14:26 kensmith Exp $
 */

#include <sys/param.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include <errno.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "debug.h"
#include "file.h"
#include "rtld.h"

static Elf_Ehdr *get_elf_header (int, const char *);

/*
 * Map a shared object into memory.  The "fd" argument is a file descriptor,
 * which must be open on the object and positioned at its beginning.
 * The "path" argument is a pathname that is used only for error messages.
 *
 * The return value is a pointer to a newly-allocated Obj_Entry structure
 * for the shared object.  Returns NULL on failure.
 */
Obj_Entry *
map_object(int fd, const char *path, const struct stat *sb)
{
    Obj_Entry *obj;
    Elf_Ehdr *hdr;
    Elf_Phdr *phdr;
    Elf_Phdr *phlimit;
    Elf_Phdr **segs;
    int nsegs;
    Elf_Phdr *phdyn;
    Elf_Phdr *phinterp;
    Elf_Phdr *phtls;
    caddr_t mapbase;
    size_t mapsize;
    Elf_Addr base_vaddr;
    Elf_Addr base_vlimit;
    size_t phsize;

    hdr = get_elf_header(fd, path);
    if (hdr == NULL)
	return (NULL);

    /*
     * Scan the program header entries, and save key information.
     *
     * We rely on there being exactly two load segments, text and data,
     * in that order.
     */
    phdr = (Elf_Phdr *) ((char *)hdr + hdr->e_phoff);
    phsize  = hdr->e_phnum * sizeof (phdr[0]);
    phlimit = phdr + hdr->e_phnum;
    nsegs = -1;
    phdyn = phinterp = phtls = NULL;
    segs = alloca(sizeof(segs[0]) * hdr->e_phnum);
    while (phdr < phlimit) {
	switch (phdr->p_type) {

	case PT_INTERP:
	    phinterp = phdr;
	    break;

	case PT_LOAD:
	    segs[++nsegs] = phdr;
    	    if ((segs[nsegs]->p_align & (PAGE_SIZE - 1)) != 0) {
		_rtld_error("%s: PT_LOAD segment %d not page-aligned",
		    path, nsegs);
		return NULL;
	    }
	    break;

	case PT_PHDR:
	    phsize = phdr->p_memsz;
	    break;

	case PT_DYNAMIC:
	    phdyn = phdr;
	    break;

	case PT_TLS:
	    phtls = phdr;
	    break;
	}

	++phdr;
    }
    if (phdyn == NULL) {
	_rtld_error("%s: object is not dynamically-linked", path);
	return NULL;
    }

    if (nsegs < 0) {
	_rtld_error("%s: too few PT_LOAD segments", path);
	return NULL;
    }

    /*
     * Map the entire address space of the object, to stake out our
     * contiguous region, and to establish the base address for relocation.
     */
    base_vaddr = trunc_page(segs[0]->p_vaddr);
    base_vlimit = round_page(segs[nsegs]->p_vaddr + segs[nsegs]->p_memsz);
    mapsize = base_vlimit - base_vaddr;

    mapbase = genode_map(fd, segs);

    obj = obj_new();
    if (sb != NULL) {
	obj->dev = sb->st_dev;
	obj->ino = sb->st_ino;
    }
    obj->mapbase = mapbase;
    obj->mapsize = mapsize;
    obj->textsize = round_page(segs[0]->p_vaddr + segs[0]->p_memsz) -
      base_vaddr;
    obj->vaddrbase = base_vaddr;
    obj->relocbase = mapbase - base_vaddr;
    obj->dynamic = (const Elf_Dyn *) (obj->relocbase + phdyn->p_vaddr);
    if (hdr->e_entry != 0)
	obj->entry = (caddr_t) (obj->relocbase + hdr->e_entry);

    /* cbass: copy program header */
    obj->phdr = malloc(phsize);
    if (obj->phdr == NULL) {
	obj_free(obj);
	_rtld_error("%s: cannot allocate program header", path);
	return NULL;
    }
    memcpy((char *)obj->phdr, (char *)hdr + hdr->e_phoff, phsize);
    obj->phdr_alloc = true;
    obj->phsize = phsize;

    if (phinterp != NULL)
	obj->interp = (const char *) (obj->relocbase + phinterp->p_vaddr);
    if (phtls != NULL) {
	tls_dtv_generation++;
	obj->tlsindex = ++tls_max_index;
	obj->tlssize = phtls->p_memsz;
	obj->tlsalign = phtls->p_align;
	obj->tlsinitsize = phtls->p_filesz;
	obj->tlsinit = mapbase + phtls->p_vaddr;
    }
    return obj;
}

static Elf_Ehdr *
get_elf_header (int fd, const char *path)
{
    static union {
	Elf_Ehdr hdr;
	char buf[PAGE_SIZE];
    } u;
    ssize_t nbytes;

    if ((nbytes = read(fd, u.buf, PAGE_SIZE)) == -1) {
	_rtld_error("%s: read error: %s", path, strerror(errno));
	return NULL;
    }

    /* Make sure the file is valid */
    if (nbytes < (ssize_t)sizeof(Elf_Ehdr) || !IS_ELF(u.hdr)) {
	_rtld_error("%s: invalid file format", path);
	return NULL;
    }
    if (u.hdr.e_ident[EI_CLASS] != ELF_TARG_CLASS
      || u.hdr.e_ident[EI_DATA] != ELF_TARG_DATA) {
	_rtld_error("%s: unsupported file layout", path);
	return NULL;
    }
    if (u.hdr.e_ident[EI_VERSION] != EV_CURRENT
      || u.hdr.e_version != EV_CURRENT) {
	_rtld_error("%s: unsupported file version", path);
	return NULL;
    }
    if (u.hdr.e_type != ET_EXEC && u.hdr.e_type != ET_DYN) {
	_rtld_error("%s: unsupported file type", path);
	return NULL;
    }
    if (u.hdr.e_machine != ELF_TARG_MACH) {
	_rtld_error("%s: unsupported machine", path);
	return NULL;
    }

    /*
     * We rely on the program header being in the first page.  This is
     * not strictly required by the ABI specification, but it seems to
     * always true in practice.  And, it simplifies things considerably.
     */
    if (u.hdr.e_phentsize != sizeof(Elf_Phdr)) {
	_rtld_error(
	  "%s: invalid shared object: e_phentsize != sizeof(Elf_Phdr)", path);
	return NULL;
    }
    if (u.hdr.e_phoff + u.hdr.e_phnum * sizeof(Elf_Phdr) > (size_t)nbytes) {
	_rtld_error("%s: program header too large", path);
	return NULL;
    }

    return (&u.hdr);
}

void
obj_free(Obj_Entry *obj)
{
    Objlist_Entry *elm;

    if (obj->tls_done)
	free_tls_offset(obj);
    while (obj->needed != NULL) {
	Needed_Entry *needed = obj->needed;
	obj->needed = needed->next;
	free(needed);
    }
    while (!STAILQ_EMPTY(&obj->names)) {
	Name_Entry *entry = STAILQ_FIRST(&obj->names);
	STAILQ_REMOVE_HEAD(&obj->names, link);
	free(entry);
    }
    while (!STAILQ_EMPTY(&obj->dldags)) {
	elm = STAILQ_FIRST(&obj->dldags);
	STAILQ_REMOVE_HEAD(&obj->dldags, link);
	free(elm);
    }
    while (!STAILQ_EMPTY(&obj->dagmembers)) {
	elm = STAILQ_FIRST(&obj->dagmembers);
	STAILQ_REMOVE_HEAD(&obj->dagmembers, link);
	free(elm);
    }
    if (obj->vertab)
	free(obj->vertab);
    if (obj->origin_path)
	free(obj->origin_path);
    if (obj->priv)
	free(obj->priv);
    if (obj->path)
	free(obj->path);
    if (obj->phdr_alloc)
	free((void *)obj->phdr);
    free(obj);
}

Obj_Entry *
obj_new(void)
{
    Obj_Entry *obj;

    obj = CNEW(Obj_Entry);
    STAILQ_INIT(&obj->dldags);
    STAILQ_INIT(&obj->dagmembers);
    STAILQ_INIT(&obj->names);
    return obj;
}

