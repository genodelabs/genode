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

#include <lx_emul/random.h>
#include <lx_emul/alloc.h>
#include <lx_emul/io_mem.h>


#include <asm-generic/delay.h>
#include <linux/delay.h>

void __const_udelay(unsigned long xloops)
{
       unsigned long usecs = xloops / 0x10C7UL;
       if (usecs < 100)
               lx_emul_time_udelay(usecs);
       else
               usleep_range(usecs, usecs * 10);
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


void kmem_cache_free_bulk(struct kmem_cache *s, size_t size, void **p)
{
	size_t i;

	for (i = 0; i < size; i++) {
		kmem_cache_free(s, p[i]);
	}
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

	if (ops->alloc_inode)
		inode = ops->alloc_inode(sb);

	if (!inode)
		return (struct inode*)ERR_PTR(-ENOMEM);

	if (!inode->free_inode)
		inode->free_inode = ops->free_inode;

	return inode;
}


void iput(struct inode * inode)
{
	if (!inode)
		return;

	if (atomic_read(&inode->i_count)
	    && !atomic_dec_and_test(&inode->i_count))
		return;

	if (inode->free_inode)
		inode->free_inode(inode);
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


#include <linux/prandom.h>

void prandom_bytes(void *buf, size_t bytes)
{
	lx_emul_gen_random_bytes(buf, bytes);
}


u32 prandom_u32(void)
{
	return lx_emul_gen_random_u32();
}


#include <linux/gfp.h>

void *page_frag_alloc_align(struct page_frag_cache *nc,
                            unsigned int fragsz, gfp_t gfp_mask,
                            unsigned int align_mask)
{
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


#include <linux/miscdevice.h>

int misc_register(struct miscdevice *misc)
{
	return 0;
}

void misc_deregister(struct miscdevice *misc)
{
}


/* rfkill support */

#include <linux/rfkill.h>
#include <net/rfkill/rfkill.h>

int __init rfkill_handler_init(void)
{
	return 0;
}

static struct
{
	int rfkilled;
	int blocked;
} _rfkill_state;


struct task_struct *rfkill_task_struct_ptr;


int lx_emul_rfkill_get_any(void)
{
	/*
	 * Since this function may also be called from non EPs
	 * _do not_ execute _any_ kernel code.
	 */
	return _rfkill_state.rfkilled;
}


void lx_emul_rfkill_switch_all(int blocked)
{
	_rfkill_state.blocked = blocked;
}


static int rfkill_task_function(void *arg)
{
	(void)arg;

	for (;;) {

		bool rfkilled = !!rfkill_get_global_sw_state(RFKILL_TYPE_WLAN);

		if (rfkilled != _rfkill_state.blocked)
			rfkill_switch_all(RFKILL_TYPE_WLAN, !!_rfkill_state.blocked);

		_rfkill_state.rfkilled = rfkilled;

		lx_emul_task_schedule(true);
	}

	return 0;
}


void rfkill_init(void)
{
	pid_t pid;

	pid = kernel_thread(rfkill_task_function, NULL, CLONE_FS | CLONE_FILES);

	rfkill_task_struct_ptr = find_task_by_pid_ns(pid, NULL);
}
