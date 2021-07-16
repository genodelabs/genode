/*
 * \brief  Linux kernel API
 * \author Norman Feske
 * \author Sebastian Sumpf
 * \author Josef Soentgen
 * \date   2014-08-21
 *
 * Based on the prototypes found in the Linux kernel's 'include/'.
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

/*************************
 ** linux/scatterlist.h **
 *************************/

enum {
	SG_MITER_TO_SG   = 2,
	SG_MITER_FROM_SG = 4,
};

struct scatterlist {
	unsigned long page_link;
	unsigned int  offset;
	unsigned int  length;
	dma_addr_t    dma_address;
	int           last;
};

struct sg_table
{
	struct scatterlist *sgl;  /* the list */
	unsigned int nents;       /* number of mapped entries */
	unsigned int orig_nents;
};

struct sg_page_iter
{
	struct scatterlist *sg;
	unsigned int        sg_pgoffset;	/* page offset within the sg */
	unsigned int        __nents;
	int                 __pg_advance;
};

struct sg_mapping_iter
{
	void               *addr;
	size_t              length;

	struct sg_page_iter piter;
};

struct page;

#define sg_is_chain(sg) ((sg)->page_link & 0x01)
#define sg_chain_ptr(sg) \
	((struct scatterlist *) ((sg)->page_link & ~0x03))

void sg_init_table(struct scatterlist *, unsigned int);
void sg_set_buf(struct scatterlist *sg, const void *buf, unsigned int buflen);
void sg_set_page(struct scatterlist *sg, struct page *page,
                 unsigned int len, unsigned int offset);

struct page        *sg_page(struct scatterlist *sg);
void               *sg_virt(struct scatterlist *sg);
struct scatterlist *sg_next(struct scatterlist *);

int    sg_nents(struct scatterlist *sg);
size_t sg_copy_from_buffer(struct scatterlist *sgl, unsigned int nents,
                           void *buf, size_t buflen);
size_t sg_copy_to_buffer(struct scatterlist *sgl, unsigned int nents,
                         void *buf, size_t buflen);

bool __sg_page_iter_next(struct sg_page_iter *piter);
void __sg_page_iter_start(struct sg_page_iter *piter,
			  struct scatterlist *sglist, unsigned int nents,
			  unsigned long pgoffset);

#define for_each_sg(sglist, sg, nr, __i) \
	for (__i = 0, sg = (sglist); __i < (nr); __i++, sg = sg_next(sg))

#define for_each_sg_page(sglist, piter, nents, pgoffset)		   \
	for (__sg_page_iter_start((piter), (sglist), (nents), (pgoffset)); \
	     __sg_page_iter_next(piter);)

#define sg_dma_address(sg) ((sg)->dma_address)
#define sg_dma_len(sg)     ((sg)->length)

void sg_miter_start(struct sg_mapping_iter *miter, struct scatterlist *sgl,
                    unsigned int nents, unsigned int flags);
bool sg_miter_skip(struct sg_mapping_iter *miter, off_t offset);
bool sg_miter_next(struct sg_mapping_iter *miter);
void sg_miter_stop(struct sg_mapping_iter *miter);
