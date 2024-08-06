/*
 * \brief  Shared-memory file utility for intel/display
 * \author Christian Helmuth
 * \author Josef Söntgen
 * \author Alexander Boettcher
 * \date   2023-12-04
 *
 * This utility implements limited shared-memory file semantics as required by
 * Linux graphic driver intel_fb
 */

/*
 * Copyright (C) 2023-2024 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <linux/shmem_fs.h>


struct shmem_file_buffer
{
	struct folio *folio;
};


struct file *shmem_file_setup(char const *name, loff_t size,
                              unsigned long flags)
{
	struct file *f;
	struct inode *inode;
	struct address_space *mapping;
	struct shmem_file_buffer *private_data;
	loff_t const nrpages = DIV_ROUND_UP(size, PAGE_SIZE);

	if (!size)
		return (struct file*)ERR_PTR(-EINVAL);

	f = kzalloc(sizeof (struct file), 0);
	if (!f) {
		return (struct file*)ERR_PTR(-ENOMEM);
	}

	inode = kzalloc(sizeof (struct inode), 0);
	if (!inode) {
		goto err_inode;
	}

	mapping = kzalloc(sizeof (struct address_space), 0);
	if (!mapping) {
		goto err_mapping;
	}

	private_data = kzalloc(sizeof (struct shmem_file_buffer), 0);
	if (!private_data) {
		goto err_private_data;
	}

	mapping->private_data = private_data;
	mapping->nrpages = nrpages;

	inode->i_mapping = mapping;

	atomic_long_set(&f->f_count, 1);
	f->f_inode    = inode;
	f->f_mapping  = mapping;
	f->f_flags    = flags;
	f->f_mode     = OPEN_FMODE(flags);
	f->f_mode    |= FMODE_OPENED;

	return f;

err_private_data:
	kfree(mapping);
err_mapping:
	kfree(inode);
err_inode:
	kfree(f);
	return (struct file*)ERR_PTR(-ENOMEM);
}


static void _free_file(struct file *file)
{
	struct inode *inode;
	struct address_space *mapping;
	struct shmem_file_buffer *private_data;

	mapping      = file->f_mapping;
	inode        = file->f_inode;

	if (mapping) {
		private_data = mapping->private_data;

		if (private_data->folio) {
			/* freed by indirect call of __folio_batch_release in lx_emul.c */
			private_data->folio = NULL;
		}

		kfree(private_data);
		kfree(mapping);
	}

	kfree(inode);
	kfree(file->f_path.dentry);
	kfree(file);
}


void fput(struct file *file)
{
	if (!file)
		return;

	if (atomic_long_sub_and_test(1, &file->f_count)) {
		_free_file(file);
	}
}


/*
 * Identical to original from mm/page_alloc.c
 */
struct folio *__folio_alloc(gfp_t gfp, unsigned int order, int preferred_nid,
		nodemask_t *nodemask)
{
	struct page *page = __alloc_pages(gfp | __GFP_COMP, order,
			preferred_nid, nodemask);
	struct folio *folio = (struct folio *)page;

	if (folio && order > 1)
		folio_prep_large_rmappable(folio);
	return folio;
}


struct folio *shmem_read_folio_gfp(struct address_space *mapping,
                                   pgoff_t index, gfp_t gfp)
{
	struct shmem_file_buffer *private_data;

	if (index > mapping->nrpages)
		return NULL;

	private_data = mapping->private_data;

	if (index != 0) {
		printk("%s unsupported case - fail\n", __func__);
		return NULL;
	}

	if (!private_data->folio) {
		unsigned order       = order_base_2(mapping->nrpages);
		/* essence of shmem_alloc_folio function */
		private_data->folio  = vma_alloc_folio(gfp, order, NULL, 0, true);
	}

	return private_data->folio;
}
