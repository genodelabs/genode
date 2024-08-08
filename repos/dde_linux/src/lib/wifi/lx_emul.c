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
#include <linux/version.h>

#include <lx_emul/random.h>
#include <lx_emul/alloc.h>
#include <lx_emul/io_mem.h>

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

struct firmware_work {
	struct work_struct work;
	struct firmware *firmware;
	char const *name;
	void *context;
	void (*cont)(struct firmware const *, void *);
};

extern int lx_emul_request_firmware_nowait(const char *name, void *dest,
                                           size_t *result, bool warn);

static void request_firmware_work_func(struct work_struct *work)
{
	struct firmware_work *fw_work =
		container_of(work, struct firmware_work, work);
	struct firmware *fw = fw_work->firmware;

	if (lx_emul_request_firmware_nowait(fw_work->name,
	                                    &fw->data, &fw->size, true)) {
		/*
		 * Free and set to NULL here as passing NULL to
		 * 'cont()' triggers requesting next possible ucode
		 * version.
		 */
		kfree(fw);
		fw = NULL;
	}

	fw_work->cont(fw, fw_work->context);

	kfree(fw_work);
}


int request_firmware_nowait(struct module * module,
                            bool uevent, const char * name,
                            struct device * device, gfp_t gfp,
                            void * context,
                            void (* cont)(const struct firmware * fw,
                                          void * context))
{
	struct firmware *fw = kzalloc(sizeof (struct firmware), GFP_KERNEL);
	struct firmware_work *fw_work;

	fw_work = kzalloc(sizeof (struct firmware_work), GFP_KERNEL);
	if (!fw_work) {
		kfree(fw);
		return -1;
	}

	fw_work->name     = name;
	fw_work->firmware = fw;
	fw_work->context  = context;
	fw_work->cont     = cont;

	INIT_WORK(&fw_work->work, request_firmware_work_func);
	schedule_work(&fw_work->work);

	return 0;
}


int request_firmware_common(const struct firmware **firmware_p,
                            const char *name, struct device *device, bool warn)
{
	struct firmware *fw;

	if (!*firmware_p)
		return -1;

	fw = kzalloc(sizeof(struct firmware), GFP_KERNEL);

	if (lx_emul_request_firmware_nowait(name, &fw->data, &fw->size, warn)) {
		kfree(fw);
		return -1;
	}

	*firmware_p = fw;
	return 0;
}


int request_firmware(const struct firmware ** firmware_p,
                     const char * name, struct device * device)
{
	return request_firmware_common(firmware_p, name, device, true);
}


extern void lx_emul_release_firmware(void const *data, size_t size);


void release_firmware(const struct firmware * fw)
{
	if (!fw)
		return;

	lx_emul_release_firmware(fw->data, fw->size);
	kfree(fw);
}


int firmware_request_nowarn(const struct firmware ** firmware,const char * name,struct device * device)
{
	return request_firmware_common(firmware, name, device, false);
}


// XXX add kernel/task_work.c as well
#ifdef CONFIG_X86
#include <linux/task_work.h>

int task_work_add(struct task_struct * task,struct callback_head * work,enum task_work_notify_mode notify)
{
	printk("%s: task: %p work: %p notify: %u\n", __func__, task, work, notify);
	return -1;
}
#endif


#include <linux/gfp.h>
#include <linux/slab.h>

unsigned long get_zeroed_page(gfp_t gfp_mask)
{
	return (unsigned long)__alloc_pages(GFP_KERNEL, 0, 0, NULL)->virtual;
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

#ifndef INLINE_COPY_FROM_USER
unsigned long _copy_from_user(void * to, const void __user * from,
                              unsigned long n)
{
	memcpy(to, from, n);
	return 0;
}
#endif


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
	lx_emul_backtrace();
}


#include <linux/prandom.h>

void prandom_bytes(void *buf, size_t bytes)
{
	lx_emul_random_gen_bytes(buf, bytes);
}


u32 prandom_u32(void)
{
	return lx_emul_random_gen_u32();
}


#include <linux/version.h>
#include <linux/gfp.h>

void *page_frag_alloc_align(struct page_frag_cache *nc,
                            unsigned int fragsz, gfp_t gfp_mask,
                            unsigned int align_mask)
{
	unsigned int const order = fragsz / PAGE_SIZE;
#if LINUX_VERSION_CODE > KERNEL_VERSION(5,12,0)
	struct page *page = __alloc_pages(gfp_mask, order, 0, NULL);
#else
	struct page *page = __alloc_pages(gfp_mask, order, 0);
#endif

	if (!page)
		return NULL;

	/* see page_frag_free */
	if (order > 0)
		printk("%s: alloc might leak memory: fragsz: %u PAGE_SIZE: %lu "
		       "order: %u page: %p addr: %p\n", __func__, fragsz, PAGE_SIZE, order, page, page->virtual);

	return page_address(page);
}


#include <linux/gfp.h>

void page_frag_free(void * addr)
{
	struct page *page = virt_to_page(addr);
	if (!page) {
		printk("BUG %s: page for addr: %p not found\n", __func__, addr);
		lx_emul_backtrace();
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
#include <../net/rfkill/rfkill.h>

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


#include <linux/dma-mapping.h>

void *dmam_alloc_attrs(struct device *dev, size_t size, dma_addr_t *dma_handle,
                       gfp_t gfp, unsigned long attrs)
{
	return dma_alloc_attrs(dev, size, dma_handle, gfp, attrs);
}


unsigned long __FIXADDR_TOP = 0xfffff000;
