/*
 * \brief  Linux emulation environment specific to this driver
 * \author Josef Soentgen
 * \date   2022-02-09
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <lx_emul.h>
#include <linux/slab.h>

#include <lx_emul/alloc.h>
#include <lx_emul/io_mem.h>


#include <asm-generic/delay.h>

void __const_udelay(unsigned long xloops)
{
	lx_emul_time_udelay(xloops / 0x10C7UL);
}


void __udelay(unsigned long usecs)
{
	lx_emul_time_udelay(usecs);
}


#include <linux/cpumask.h>

atomic_t __num_online_cpus = ATOMIC_INIT(1);


#include <linux/dma-mapping.h>

dma_addr_t dma_map_page_attrs(struct device * dev,
                              struct page * page,
                              size_t offset,
                              size_t size,
                              enum dma_data_direction dir,
                              unsigned long attrs)
{
	dma_addr_t    const dma_addr  = page_to_phys(page);
	unsigned long const virt_addr = (unsigned long)page_to_virt(page);

	lx_emul_mem_cache_clean_invalidate((void *)(virt_addr + offset), size);
	return dma_addr + offset;
}


#include <linux/dmapool.h>

struct dma_pool {
	size_t size;
	size_t align;
};

void * dma_pool_alloc(struct dma_pool * pool, gfp_t mem_flags, dma_addr_t * handle)
{
	void * ret =
		lx_emul_mem_alloc_aligned_uncached(pool->size, pool->align);
	*handle = lx_emul_mem_dma_addr(ret);
	return ret;
}


struct dma_pool * dma_pool_create(const char * name,
                                  struct device * dev,
                                  size_t size,
                                  size_t align,
                                  size_t boundary)
{
	struct dma_pool * pool = kmalloc(sizeof(struct dma_pool), GFP_KERNEL);
	pool->size  = size;
	pool->align = align;
	return pool;
}


void dma_pool_free(struct dma_pool * pool,void * vaddr,dma_addr_t dma)
{
	lx_emul_mem_free(vaddr);
}


struct dma_pool *dmam_pool_create(const char *name,
                                  struct device *dev,
                                  size_t size,
                                  size_t align,
                                  size_t allocation)
{
	/* 
	 * Only take care of allocating the pool because
	 * we do not detach the driver anyway.
	 */
	return dma_pool_create(name, dev, size, align, 0);
}


#include <linux/dma-mapping.h>

int dma_supported(struct device * dev,u64 mask)
{
	lx_emul_trace(__func__);
	return 1;
}


#include <linux/dma-mapping.h>

void dma_unmap_page_attrs(struct device * dev,
                          dma_addr_t addr,
                          size_t size,
                          enum dma_data_direction dir,
                          unsigned long attrs)
{
	unsigned long const virt_addr = lx_emul_mem_virt_addr((void*)addr);

	if (!virt_addr)
		return;

	if (dir == DMA_FROM_DEVICE)
		lx_emul_mem_cache_invalidate((void *)virt_addr, size);
}


#include <linux/dma-mapping.h>

void dma_sync_single_for_cpu(struct device *dev, dma_addr_t addr,
                             size_t size, enum dma_data_direction dir)
{
	unsigned long const virt_addr = lx_emul_mem_virt_addr((void*)addr);

	if (!virt_addr)
		return;

	lx_emul_mem_cache_invalidate((void *)virt_addr, size);
}


#include <linux/dma-mapping.h>

void dma_sync_single_for_device(struct device *dev, dma_addr_t addr,
                                size_t size, enum dma_data_direction dir)
{
	unsigned long const virt_addr = lx_emul_mem_virt_addr((void*)addr);

	if (!virt_addr)
		return;

	lx_emul_mem_cache_clean_invalidate((void *)virt_addr, size);
}


#include <linux/slab.h>

void * kmalloc_order(size_t size, gfp_t flags, unsigned int order)
{
	return kmalloc(size, flags);
}


#include <linux/fs.h>
#include <linux/mount.h>
#include <linux/slab.h>

int simple_pin_fs(struct file_system_type * type, struct vfsmount ** mount, int * count)
{
	*mount = kmalloc(sizeof(struct vfsmount), GFP_KERNEL);
	return 0;
}


#include <linux/fs.h>

void simple_release_fs(struct vfsmount ** mount,int * count)
{
	kfree(*mount);
}


#include <linux/fs.h>

struct inode * alloc_anon_inode(struct super_block * s)
{
	return kmalloc(sizeof(struct inode), GFP_KERNEL);
}


#include <linux/interrupt.h>

// softirq.c
void tasklet_setup(struct tasklet_struct * t,
                   void (* callback)(struct tasklet_struct *))
{
	t->next = NULL;
	t->state = 0;
	atomic_set(&t->count, 0);
	t->callback = callback;
	t->use_callback = true;
	t->data = 0;
}


void __tasklet_schedule(struct tasklet_struct * t)
{
	if (test_and_clear_bit(TASKLET_STATE_SCHED, &t->state))
		t->callback(t);
}


void __tasklet_hi_schedule(struct tasklet_struct * t)
{
	if (test_and_clear_bit(TASKLET_STATE_SCHED, &t->state))
		t->callback(t);
}


#include <linux/rcupdate.h>

void call_rcu(struct rcu_head * head,rcu_callback_t func)
{
	enum { KVFREE_RCU_OFFSET = 4096, };
	if (func < (rcu_callback_t)KVFREE_RCU_OFFSET) {
		kvfree((void*)head - (unsigned long)func);
		return;
	}

	func(head);
}


#include <asm-generic/logic_io.h>

void __iomem * ioremap(resource_size_t phys_addr, unsigned long size)
{
	return lx_emul_io_mem_map(phys_addr, size);
}


#include <asm-generic/logic_io.h>

void iounmap(volatile void __iomem * addr)
{
	(void)addr;
}


#include <linux/slab.h>

struct kmem_cache * kmem_cache_create_usercopy(const char * name,
                                               unsigned int size,
                                               unsigned int align,
                                               slab_flags_t flags,
                                               unsigned int useroffset,
                                               unsigned int usersize,
                                               void (* ctor)(void *))
{
    return kmem_cache_create(name, size, align, flags, ctor);
}


#include <linux/fs.h>

int register_filesystem(struct file_system_type * fs)
{
	lx_emul_trace(__func__);
	return 0;
}


#include <linux/slab.h>
#include <linux/mount.h>
#include <linux/fs.h>
#include <linux/fs_context.h>
#include <linux/pseudo_fs.h>

struct pseudo_fs_context * init_pseudo(struct fs_context *fc,
                                       unsigned long magic)
{
	struct pseudo_fs_context *pfs_ctx;

	pfs_ctx = kzalloc(sizeof (struct pseudo_fs_context), GFP_KERNEL);
	if (pfs_ctx) {
		pfs_ctx->magic = magic;
		fc->fs_private = pfs_ctx;
	}
	return pfs_ctx;
}


struct vfsmount * kern_mount(struct file_system_type * type)
{
	struct vfsmount *m;

	/*
	 * This sets everything up so that 'new_inode_pseudo()'
	 * called from 'sock_alloc()' properly allocates the inode.
	 */

	m = kzalloc(sizeof (struct vfsmount), 0);
	if (m) {

		struct fs_context fs_ctx;

		if (type->init_fs_context) {
			type->init_fs_context(&fs_ctx);

			m->mnt_sb = kzalloc(sizeof (struct super_block), GFP_KERNEL);
			m->mnt_sb->s_type = type;
			m->mnt_sb->s_op =
				((struct pseudo_fs_context*)fs_ctx.fs_private)->ops;
		} else {
			kfree(m);
			m = (struct vfsmount*)ERR_PTR(-ENOMEM);
		}
	}

	return m;
}


struct inode * new_inode_pseudo(struct super_block * sb)
{
    const struct super_operations *ops = sb->s_op;
    struct inode *inode;

    if (ops->alloc_inode) {
        inode = ops->alloc_inode(sb);
	}

	if (!inode)
		return (struct inode*)ERR_PTR(-ENOMEM);

	return inode;
}


#include <linux/sysfs.h>

int sysfs_create_dir_ns(struct kobject * kobj,const void * ns)
{
    lx_emul_trace(__func__);
    kobj->sd = kzalloc(sizeof(*kobj->sd), GFP_KERNEL);
    return 0;
}


#include <linux/firmware.h>

#if 0
struct firmware_work {
	struct work_struct work;
	struct firmware const *firmware;
	void *context;
	void (*cont)(struct firmware const *, void *);
};


static void request_firmware_work_func(struct work_struct *work)
{
	struct firmware_work *fw_work =
		container_of(work, struct firmware_work, work);

	fw_work->cont(fw_work->firmware, fw_work->context);

	kfree(fw_work);
}
#endif


extern int lx_emul_request_firmware_nowait(const char *name, void *dest, size_t *result);
extern void lx_emul_release_firmware(void const *data, size_t size);

extern void rtnl_lock(void);
extern void rtnl_unlock(void);

int request_firmware_nowait(struct module * module,
                            bool uevent, const char * name,
                            struct device * device, gfp_t gfp,
                            void * context,
                            void (* cont)(const struct firmware * fw,
                                          void * context))
{
	struct firmware *fw = kzalloc(sizeof (struct firmware), GFP_KERNEL);
#if 0
	struct firmware_work *fw_work;
#endif
	bool reg_db;

	if (lx_emul_request_firmware_nowait(name, &fw->data, &fw->size)) {
		kfree(fw);
		return -1;
	}

	/*
	 * Normally we would schedule fw_work but for reasons not
	 * yet understood doing so will lead to a page-fault. So
	 * for the time being we will execute the callback directly
	 * and we have to make sure to manage the RTNL lock as the
	 * callback will grab it while we already hold it.
	 */
	reg_db = strcmp(name, "regulatory.db") == 0;

	if (reg_db)
		rtnl_unlock();

	cont(fw, context);

	if (reg_db)
		rtnl_lock();
	return 0;

#if 0
	fw_work = kzalloc(sizeof (struct firmware_work), GFP_KERNEL);
	if (!fw_work) {
		kfree(fw);
		return -1;
	}

	fw_work->firmware = fw;
	fw_work->context  = context;
	fw_work->cont     = cont;

	INIT_WORK(&fw_work->work, request_firmware_work_func);
	schedule_work(&fw_work->work);

	return 0;
#endif
}


int request_firmware(const struct firmware ** firmware_p,
                     const char * name, struct device * device)
{
	struct firmware *fw;

	if (!*firmware_p)
		return -1;

	fw = kzalloc(sizeof (struct firmware), GFP_KERNEL);

	if (lx_emul_request_firmware_nowait(name, &fw->data, &fw->size)) {
		kfree(fw);
		return -1;
	}

	*firmware_p = fw;
	return 0;
}


void release_firmware(const struct firmware * fw)
{
	lx_emul_release_firmware(fw->data, fw->size);
	kfree(fw);
}


/*
 * This function is only called when using newer WIFI6 devices to
 * load 'iwl-debug-yoyo.bin'. We simply deny the request.
 */
int firmware_request_nowarn(const struct firmware ** firmware,const char * name,struct device * device)
{
	return -1;
}


#include <linux/pci.h>

int pcim_iomap_regions_request_all(struct pci_dev * pdev,int mask,const char * name)
{
	return 0;
}


#include <linux/pci.h>

static unsigned long *_pci_iomap_table;

void __iomem * const * pcim_iomap_table(struct pci_dev * pdev)
{
	unsigned i;

	if (!_pci_iomap_table)
		_pci_iomap_table = kzalloc(sizeof (unsigned long*) * 6, GFP_KERNEL);

	if (!_pci_iomap_table)
		return NULL;

	for (i = 0; i < 6; i++) {
		struct resource *r = &pdev->resource[i];
		unsigned long phys_addr = r->start;
		unsigned long size      = r->end - r->start;

		if (!phys_addr || !size)
			continue;

		_pci_iomap_table[i] =
			(unsigned long)lx_emul_io_mem_map(phys_addr, size);
	}

	return (void const *)_pci_iomap_table;
}


#include <linux/task_work.h>

int task_work_add(struct task_struct * task,struct callback_head * work,enum task_work_notify_mode notify)
{
	printk("%s: task: %p work: %p notify: %u\n", __func__, task, work, notify);
	return -1;
}


#include <linux/vmalloc.h>

void vfree(const void * addr)
{
	kfree(addr);
}


#include <linux/vmalloc.h>

void * vmalloc(unsigned long size)
{
	return kmalloc(size, GFP_KERNEL);
}


#include <linux/vmalloc.h>

void * vzalloc(unsigned long size)
{
	return kzalloc(size, GFP_KERNEL);
}


#include <linux/interrupt.h>

void __raise_softirq_irqoff(unsigned int nr)
{
    raise_softirq(nr);
}


#include <linux/slab.h>

void kfree_sensitive(const void *p)
{
	size_t ks;
	void *mem = (void *)p;

	ks = ksize(mem);
	if (ks)
		memset(mem, 0, ks);

	kfree(mem);
}


#include <linux/gfp.h>

void free_pages(unsigned long addr,unsigned int order)
{
	__free_pages(virt_to_page((void *)addr), order);
}


#include <linux/gfp.h>
#include <linux/slab.h>

unsigned long get_zeroed_page(gfp_t gfp_mask)
{
    return (unsigned long)kzalloc(PAGE_SIZE, gfp_mask | __GFP_ZERO);
}


#include <linux/sched.h>

pid_t __task_pid_nr_ns(struct task_struct * task,
                       enum pid_type type,
                       struct pid_namespace * ns)
{
	(void)type;
	(void)ns;

	return lx_emul_task_pid(task);
}


#include <linux/uaccess.h>

unsigned long _copy_from_user(void * to, const void __user * from,
                              unsigned long n)
{
	memcpy(to, from, n);
	return 0;
}


#include <linux/uio.h>

size_t _copy_from_iter(void * addr, size_t bytes, struct iov_iter * i)
{
	char               *kdata;
	struct iovec const *iov;
	size_t              len;

	if (bytes > i->count)
		bytes = i->count;

	if (bytes == 0)
		return 0;

	kdata = (char*)(addr);
	iov   = i->iov;

	len = bytes;
	while (len > 0) {
		if (iov->iov_len) {
			size_t copy_len = (size_t)len < iov->iov_len ? len
			                                             : iov->iov_len;
			memcpy(kdata, iov->iov_base, copy_len);

			len -= copy_len;
			kdata += copy_len;
		}
		iov++;
	}

	return bytes;
}


#include <linux/uio.h>

size_t _copy_to_iter(const void * addr, size_t bytes, struct iov_iter * i)
{
	char               *kdata;
	struct iovec const *iov;
	size_t              len;

	if (bytes > i->count)
		bytes = i->count;

	if (bytes == 0)
		return 0;

	kdata = (char*)(addr);
	iov   = i->iov;

	len = bytes;
	while (len > 0) {
		if (iov->iov_len) {
			size_t copy_len = (size_t)len < iov->iov_len ? len
			                                             : iov->iov_len;
			memcpy(iov->iov_base, kdata, copy_len);

			len -= copy_len;
			kdata += copy_len;
		}
		iov++;
	}

	return bytes;
}


#include <linux/printk.h>

asmlinkage __visible void dump_stack(void)
{
	lx_backtrace();
}


#include <linux/mm.h>

void __put_page(struct page * page)
{
	__free_pages(page, 0);
}


#include <linux/random.h>

u32 get_random_u32(void)
{
	return lx_emul_get_random_u32();
}


int __must_check get_random_bytes_arch(void *buf, int nbytes)
{
	if (nbytes < 0)
		return -1;

	lx_emul_get_random_bytes(buf, (unsigned long)nbytes);
	return 0;
}


void get_random_bytes(void *buf, int nbytes)
{
	get_random_bytes_arch(buf, nbytes);
}


#include <linux/prandom.h>

void prandom_bytes(void *buf, size_t bytes)
{
	lx_emul_get_random_bytes(buf, bytes);
}


u32 prandom_u32(void)
{
	return lx_emul_get_random_u32();
}


#include <linux/gfp.h>

void *page_frag_alloc_align(struct page_frag_cache *nc,
                            unsigned int fragsz, gfp_t gfp_mask,
                            unsigned int align_mask)
{
	(void)nc;
	unsigned int const order = fragsz / PAGE_SIZE;
	struct page *page = __alloc_pages(gfp_mask, order, 0, NULL);

	if (!page)
		return NULL;

	/* see page_frag_free */
	if (order > 0)
		printk("%s: alloc might leak memory: fragsz: %u PAGE_SIZE: %u "
		       "order: %u page: %p addr: %p\n", __func__, fragsz, PAGE_SIZE, order, page, page->virtual);

	return page->virtual;
}


#include <linux/gfp.h>

void page_frag_free(void * addr)
{
	struct page *page = lx_emul_virt_to_pages(addr, 1ul);
	if (!page) {
		printk("BUG %s: page for addr: %p not found\n", __func__, addr);
		lx_backtrace();
	}

	__free_pages(page, 0ul);
}
