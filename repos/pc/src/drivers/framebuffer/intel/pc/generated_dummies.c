/*
 * \brief  Dummy definitions of Linux Kernel functions
 * \author Automatically generated file - do no edit
 * \date   2022-07-28
 */

#include <lx_emul.h>


#include <linux/proc_fs.h>

void * PDE_DATA(const struct inode * inode)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/skbuff.h>

struct sk_buff * __alloc_skb(unsigned int size,gfp_t gfp_mask,int flags,int node)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/clk-provider.h>

const char * __clk_get_name(const struct clk * clk)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/sched.h>

int __cond_resched_lock(spinlock_t * lock)
{
	lx_emul_trace_and_stop(__func__);
}


extern void __i915_gpu_coredump_free(struct kref * error_ref);
void __i915_gpu_coredump_free(struct kref * error_ref)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/netlink.h>

struct sock * __netlink_kernel_create(struct net * net,int unit,struct module * module,struct netlink_kernel_cfg * cfg)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/pagevec.h>

void __pagevec_release(struct pagevec * pvec)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/mm.h>

void __put_page(struct page * page)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/sched/task.h>

void __put_task_struct(struct task_struct * tsk)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/srcu.h>

void __srcu_read_unlock(struct srcu_struct * ssp,int idx)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/fs.h>

void __unregister_chrdev(unsigned int major,unsigned int baseminor,unsigned int count,const char * name)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/vmalloc.h>

void * __vmalloc_node(unsigned long size,unsigned long align,gfp_t gfp_mask,int node,const void * caller)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/uaccess.h>

unsigned long _copy_to_user(void __user * to,const void * from,unsigned long n)
{
	lx_emul_trace_and_stop(__func__);
}


#include <drm/drm_lease.h>

bool _drm_lease_held(struct drm_file * file_priv,int id)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/mm.h>

atomic_long_t _totalram_pages;


extern void ack_bad_irq(unsigned int irq);
void ack_bad_irq(unsigned int irq)
{
	lx_emul_trace_and_stop(__func__);
}


#include <acpi/acpi_bus.h>

int acpi_bus_attach_private_data(acpi_handle handle,void * data)
{
	lx_emul_trace_and_stop(__func__);
}


#include <acpi/acpi_bus.h>

void acpi_bus_detach_private_data(acpi_handle handle)
{
	lx_emul_trace_and_stop(__func__);
}


#include <acpi/acpi_bus.h>

int acpi_bus_get_device(acpi_handle handle,struct acpi_device ** device)
{
	lx_emul_trace_and_stop(__func__);
}


#include <acpi/acpi_bus.h>

int acpi_bus_get_private_data(acpi_handle handle,void ** data)
{
	lx_emul_trace_and_stop(__func__);
}


#include <acpi/acpi_bus.h>

int acpi_bus_get_status(struct acpi_device * device)
{
	lx_emul_trace_and_stop(__func__);
}


#include <acpi/acpi_bus.h>

void acpi_dev_clear_dependencies(struct acpi_device * supplier)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/acpi.h>

void acpi_dev_free_resource_list(struct list_head * list)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/acpi.h>

int acpi_dev_get_resources(struct acpi_device * adev,struct list_head * list,int (* preproc)(struct acpi_resource *,void *),void * preproc_data)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/acpi.h>

bool acpi_dev_resource_interrupt(struct acpi_resource * ares,int index,struct resource * res)
{
	lx_emul_trace_and_stop(__func__);
}


extern acpi_status acpi_get_handle(acpi_handle parent,acpi_string pathname,acpi_handle * ret_handle);
acpi_status acpi_get_handle(acpi_handle parent,acpi_string pathname,acpi_handle * ret_handle)
{
	lx_emul_trace_and_stop(__func__);
}


#include <acpi/acpi_bus.h>

int acpi_match_device_ids(struct acpi_device * device,const struct acpi_device_id * ids)
{
	lx_emul_trace_and_stop(__func__);
}


#include <acpi/acpi_bus.h>

void acpi_set_modalias(struct acpi_device * adev,const char * default_id,char * modalias,size_t len)
{
	lx_emul_trace_and_stop(__func__);
}


extern acpi_status acpi_walk_namespace(acpi_object_type type,acpi_handle start_object,u32 max_depth,acpi_walk_callback descending_callback,acpi_walk_callback ascending_callback,void * context,void ** return_value);
acpi_status acpi_walk_namespace(acpi_object_type type,acpi_handle start_object,u32 max_depth,acpi_walk_callback descending_callback,acpi_walk_callback ascending_callback,void * context,void ** return_value)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/kernel.h>

void bust_spinlocks(int yes)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/fb.h>

void cfb_copyarea(struct fb_info * p,const struct fb_copyarea * area)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/fb.h>

void cfb_fillrect(struct fb_info * p,const struct fb_fillrect * rect)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/fb.h>

void cfb_imageblit(struct fb_info * p,const struct fb_image * image)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/swap.h>

void check_move_unevictable_pages(struct pagevec * pvec)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/mm.h>

int clear_page_dirty_for_io(struct page * page)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/console.h>

void console_flush_on_panic(enum con_flush_mode mode)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/console.h>

void console_lock(void)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/printk.h>

int console_printk[] = {};


#include <linux/console.h>

int console_trylock(void)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/console.h>

void console_unblank(void)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/console.h>

void console_unlock(void)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/property.h>

int device_add_software_node(struct device * dev,const struct software_node * node)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/property.h>

void device_remove_software_node(struct device * dev)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/dma-buf.h>

struct dma_buf_attachment * dma_buf_attach(struct dma_buf * dmabuf,struct device * dev)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/dma-buf.h>

void dma_buf_detach(struct dma_buf * dmabuf,struct dma_buf_attachment * attach)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/dma-buf.h>

struct dma_buf * dma_buf_export(const struct dma_buf_export_info * exp_info)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/dma-buf.h>

int dma_buf_fd(struct dma_buf * dmabuf,int flags)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/dma-buf.h>

struct dma_buf * dma_buf_get(int fd)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/dma-buf.h>

struct sg_table * dma_buf_map_attachment(struct dma_buf_attachment * attach,enum dma_data_direction direction)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/dma-buf.h>

void dma_buf_put(struct dma_buf * dmabuf)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/dma-buf.h>

void dma_buf_unmap_attachment(struct dma_buf_attachment * attach,struct sg_table * sg_table,enum dma_data_direction direction)
{
	lx_emul_trace_and_stop(__func__);
}


extern void dma_resv_prune(struct dma_resv * resv);
void dma_resv_prune(struct dma_resv * resv)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/netlink.h>

void do_trace_netlink_extack(const char * msg)
{
	lx_emul_trace_and_stop(__func__);
}


#include <drm/drm_ioctl.h>

int drm_invalid_op(struct drm_device * dev,void * data,struct drm_file * file_priv)
{
	lx_emul_trace_and_stop(__func__);
}


#include <drm/drm_ioctl.h>

long drm_ioctl(struct file * filp,unsigned int cmd,unsigned long arg)
{
	lx_emul_trace_and_stop(__func__);
}


#include <drm/drm_lease.h>

void drm_lease_destroy(struct drm_master * master)
{
	lx_emul_trace_and_stop(__func__);
}


#include <drm/drm_lease.h>

struct drm_master * drm_lease_owner(struct drm_master * master)
{
	lx_emul_trace_and_stop(__func__);
}


#include <drm/drm_lease.h>

void drm_lease_revoke(struct drm_master * top)
{
	lx_emul_trace_and_stop(__func__);
}


#include <drm/drm_ioctl.h>

int drm_noop(struct drm_device * dev,void * data,struct drm_file * file_priv)
{
	lx_emul_trace_and_stop(__func__);
}


#include <drm/drm_writeback.h>

void drm_writeback_cleanup_job(struct drm_writeback_job * job)
{
	lx_emul_trace_and_stop(__func__);
}


#include <drm/drm_writeback.h>

int drm_writeback_prepare_job(struct drm_writeback_job * job)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/irq.h>

struct irq_chip dummy_irq_chip;


#include <linux/printk.h>

asmlinkage __visible void dump_stack(void)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/reboot.h>

void emergency_restart(void)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/fb.h>

void fb_set_suspend(struct fb_info * info,int state)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/capability.h>

bool file_ns_capable(const struct file * file,struct user_namespace * ns,int cap)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/fb.h>

void framebuffer_release(struct fb_info * info)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/property.h>

struct fwnode_handle * fwnode_create_software_node(const struct property_entry * properties,const struct fwnode_handle * parent)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/property.h>

void fwnode_remove_software_node(struct fwnode_handle * fwnode)
{
	lx_emul_trace_and_stop(__func__);
}


extern void gen5_gt_enable_irq(struct intel_gt * gt,u32 mask);
void gen5_gt_enable_irq(struct intel_gt * gt,u32 mask)
{
	lx_emul_trace_and_stop(__func__);
}


extern void gen5_gt_irq_handler(struct intel_gt * gt,u32 gt_iir);
void gen5_gt_irq_handler(struct intel_gt * gt,u32 gt_iir)
{
	lx_emul_trace_and_stop(__func__);
}


extern void gen5_rps_irq_handler(struct intel_rps * rps);
void gen5_rps_irq_handler(struct intel_rps * rps)
{
	lx_emul_trace_and_stop(__func__);
}


extern void gen6_gt_irq_handler(struct intel_gt * gt,u32 gt_iir);
void gen6_gt_irq_handler(struct intel_gt * gt,u32 gt_iir)
{
	lx_emul_trace_and_stop(__func__);
}


extern void gen6_rps_irq_handler(struct intel_rps * rps,u32 pm_iir);
void gen6_rps_irq_handler(struct intel_rps * rps,u32 pm_iir)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/kernel.h>

int get_option(char ** str,int * pint)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/i2c.h>

s32 i2c_smbus_read_block_data(const struct i2c_client * client,u8 command,u8 * values)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/i2c.h>

s32 i2c_smbus_read_byte(const struct i2c_client * client)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/i2c.h>

s32 i2c_smbus_read_byte_data(const struct i2c_client * client,u8 command)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/i2c.h>

s32 i2c_smbus_read_word_data(const struct i2c_client * client,u8 command)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/i2c.h>

s32 i2c_smbus_write_block_data(const struct i2c_client * client,u8 command,u8 length,const u8 * values)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/i2c.h>

s32 i2c_smbus_write_byte(const struct i2c_client * client,u8 value)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/i2c.h>

s32 i2c_smbus_write_byte_data(const struct i2c_client * client,u8 command,u8 value)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/i2c.h>

s32 i2c_smbus_write_word_data(const struct i2c_client * client,u8 command,u16 value)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/i2c.h>

s32 i2c_smbus_xfer(struct i2c_adapter * adapter,u16 addr,unsigned short flags,char read_write,u8 command,int protocol,union i2c_smbus_data * data)
{
	lx_emul_trace_and_stop(__func__);
}


extern int i915_cmd_parser_get_version(struct drm_i915_private * dev_priv);
int i915_cmd_parser_get_version(struct drm_i915_private * dev_priv)
{
	lx_emul_trace_and_stop(__func__);
}


extern struct i915_gpu_coredump * i915_first_error_state(struct drm_i915_private * i915);
struct i915_gpu_coredump * i915_first_error_state(struct drm_i915_private * i915)
{
	lx_emul_trace_and_stop(__func__);
}


extern int i915_gem_busy_ioctl(struct drm_device * dev,void * data,struct drm_file * file);
int i915_gem_busy_ioctl(struct drm_device * dev,void * data,struct drm_file * file)
{
	lx_emul_trace_and_stop(__func__);
}


extern void i915_gem_cleanup_early(struct drm_i915_private * dev_priv);
void i915_gem_cleanup_early(struct drm_i915_private * dev_priv)
{
	lx_emul_trace_and_stop(__func__);
}


extern void i915_gem_context_close(struct drm_file * file);
void i915_gem_context_close(struct drm_file * file)
{
	lx_emul_trace_and_stop(__func__);
}


extern int i915_gem_context_create_ioctl(struct drm_device * dev,void * data,struct drm_file * file);
int i915_gem_context_create_ioctl(struct drm_device * dev,void * data,struct drm_file * file)
{
	lx_emul_trace_and_stop(__func__);
}


extern int i915_gem_context_destroy_ioctl(struct drm_device * dev,void * data,struct drm_file * file);
int i915_gem_context_destroy_ioctl(struct drm_device * dev,void * data,struct drm_file * file)
{
	lx_emul_trace_and_stop(__func__);
}


extern int i915_gem_context_getparam_ioctl(struct drm_device * dev,void * data,struct drm_file * file);
int i915_gem_context_getparam_ioctl(struct drm_device * dev,void * data,struct drm_file * file)
{
	lx_emul_trace_and_stop(__func__);
}


extern void i915_gem_context_release(struct kref * ref);
void i915_gem_context_release(struct kref * ref)
{
	lx_emul_trace_and_stop(__func__);
}


extern int i915_gem_context_reset_stats_ioctl(struct drm_device * dev,void * data,struct drm_file * file);
int i915_gem_context_reset_stats_ioctl(struct drm_device * dev,void * data,struct drm_file * file)
{
	lx_emul_trace_and_stop(__func__);
}


extern int i915_gem_context_setparam_ioctl(struct drm_device * dev,void * data,struct drm_file * file);
int i915_gem_context_setparam_ioctl(struct drm_device * dev,void * data,struct drm_file * file)
{
	lx_emul_trace_and_stop(__func__);
}


extern int i915_gem_create_ext_ioctl(struct drm_device * dev,void * data,struct drm_file * file);
int i915_gem_create_ext_ioctl(struct drm_device * dev,void * data,struct drm_file * file)
{
	lx_emul_trace_and_stop(__func__);
}


extern int i915_gem_create_ioctl(struct drm_device * dev,void * data,struct drm_file * file);
int i915_gem_create_ioctl(struct drm_device * dev,void * data,struct drm_file * file)
{
	lx_emul_trace_and_stop(__func__);
}


extern void i915_gem_driver_release(struct drm_i915_private * dev_priv);
void i915_gem_driver_release(struct drm_i915_private * dev_priv)
{
	lx_emul_trace_and_stop(__func__);
}


extern void i915_gem_driver_remove(struct drm_i915_private * dev_priv);
void i915_gem_driver_remove(struct drm_i915_private * dev_priv)
{
	lx_emul_trace_and_stop(__func__);
}


extern void i915_gem_driver_unregister(struct drm_i915_private * i915);
void i915_gem_driver_unregister(struct drm_i915_private * i915)
{
	lx_emul_trace_and_stop(__func__);
}


extern int i915_gem_dumb_create(struct drm_file * file,struct drm_device * dev,struct drm_mode_create_dumb * args);
int i915_gem_dumb_create(struct drm_file * file,struct drm_device * dev,struct drm_mode_create_dumb * args)
{
	lx_emul_trace_and_stop(__func__);
}


extern int i915_gem_dumb_mmap_offset(struct drm_file * file,struct drm_device * dev,u32 handle,u64 * offset);
int i915_gem_dumb_mmap_offset(struct drm_file * file,struct drm_device * dev,u32 handle,u64 * offset)
{
	lx_emul_trace_and_stop(__func__);
}


extern int i915_gem_evict_for_node(struct i915_address_space * vm,struct drm_mm_node * target,unsigned int flags);
int i915_gem_evict_for_node(struct i915_address_space * vm,struct drm_mm_node * target,unsigned int flags)
{
	lx_emul_trace_and_stop(__func__);
}


extern int i915_gem_evict_something(struct i915_address_space * vm,u64 min_size,u64 alignment,unsigned long color,u64 start,u64 end,unsigned flags);
int i915_gem_evict_something(struct i915_address_space * vm,u64 min_size,u64 alignment,unsigned long color,u64 start,u64 end,unsigned flags)
{
	lx_emul_trace_and_stop(__func__);
}


extern int i915_gem_evict_vm(struct i915_address_space * vm);
int i915_gem_evict_vm(struct i915_address_space * vm)
{
	lx_emul_trace_and_stop(__func__);
}


extern int i915_gem_execbuffer2_ioctl(struct drm_device * dev,void * data,struct drm_file * file);
int i915_gem_execbuffer2_ioctl(struct drm_device * dev,void * data,struct drm_file * file)
{
	lx_emul_trace_and_stop(__func__);
}


extern int i915_gem_freeze(struct drm_i915_private * i915);
int i915_gem_freeze(struct drm_i915_private * i915)
{
	lx_emul_trace_and_stop(__func__);
}


extern int i915_gem_freeze_late(struct drm_i915_private * i915);
int i915_gem_freeze_late(struct drm_i915_private * i915)
{
	lx_emul_trace_and_stop(__func__);
}


extern int i915_gem_get_aperture_ioctl(struct drm_device * dev,void * data,struct drm_file * file);
int i915_gem_get_aperture_ioctl(struct drm_device * dev,void * data,struct drm_file * file)
{
	lx_emul_trace_and_stop(__func__);
}


extern int i915_gem_madvise_ioctl(struct drm_device * dev,void * data,struct drm_file * file_priv);
int i915_gem_madvise_ioctl(struct drm_device * dev,void * data,struct drm_file * file_priv)
{
	lx_emul_trace_and_stop(__func__);
}


extern int i915_gem_mmap(struct file * filp,struct vm_area_struct * vma);
int i915_gem_mmap(struct file * filp,struct vm_area_struct * vma)
{
	lx_emul_trace_and_stop(__func__);
}


extern int i915_gem_mmap_gtt_version(void);
int i915_gem_mmap_gtt_version(void)
{
	lx_emul_trace_and_stop(__func__);
}


extern int i915_gem_mmap_ioctl(struct drm_device * dev,void * data,struct drm_file * file);
int i915_gem_mmap_ioctl(struct drm_device * dev,void * data,struct drm_file * file)
{
	lx_emul_trace_and_stop(__func__);
}


extern int i915_gem_mmap_offset_ioctl(struct drm_device * dev,void * data,struct drm_file * file);
int i915_gem_mmap_offset_ioctl(struct drm_device * dev,void * data,struct drm_file * file)
{
	lx_emul_trace_and_stop(__func__);
}


extern int i915_gem_object_attach_phys(struct drm_i915_gem_object * obj,int align);
int i915_gem_object_attach_phys(struct drm_i915_gem_object * obj,int align)
{
	lx_emul_trace_and_stop(__func__);
}


extern int i915_gem_object_pread_phys(struct drm_i915_gem_object * obj,const struct drm_i915_gem_pread * args);
int i915_gem_object_pread_phys(struct drm_i915_gem_object * obj,const struct drm_i915_gem_pread * args)
{
	lx_emul_trace_and_stop(__func__);
}


extern void i915_gem_object_put_pages_phys(struct drm_i915_gem_object * obj,struct sg_table * pages);
void i915_gem_object_put_pages_phys(struct drm_i915_gem_object * obj,struct sg_table * pages)
{
	lx_emul_trace_and_stop(__func__);
}


extern int i915_gem_object_pwrite_phys(struct drm_i915_gem_object * obj,const struct drm_i915_gem_pwrite * args);
int i915_gem_object_pwrite_phys(struct drm_i915_gem_object * obj,const struct drm_i915_gem_pwrite * args)
{
	lx_emul_trace_and_stop(__func__);
}


extern void i915_gem_object_release_mmap_gtt(struct drm_i915_gem_object * obj);
void i915_gem_object_release_mmap_gtt(struct drm_i915_gem_object * obj)
{
	lx_emul_trace_and_stop(__func__);
}


extern int i915_gem_object_userptr_validate(struct drm_i915_gem_object * obj);
int i915_gem_object_userptr_validate(struct drm_i915_gem_object * obj)
{
	lx_emul_trace_and_stop(__func__);
}


extern int i915_gem_pread_ioctl(struct drm_device * dev,void * data,struct drm_file * file);
int i915_gem_pread_ioctl(struct drm_device * dev,void * data,struct drm_file * file)
{
	lx_emul_trace_and_stop(__func__);
}


extern struct dma_buf * i915_gem_prime_export(struct drm_gem_object * gem_obj,int flags);
struct dma_buf * i915_gem_prime_export(struct drm_gem_object * gem_obj,int flags)
{
	lx_emul_trace_and_stop(__func__);
}


extern struct drm_gem_object * i915_gem_prime_import(struct drm_device * dev,struct dma_buf * dma_buf);
struct drm_gem_object * i915_gem_prime_import(struct drm_device * dev,struct dma_buf * dma_buf)
{
	lx_emul_trace_and_stop(__func__);
}


extern int i915_gem_pwrite_ioctl(struct drm_device * dev,void * data,struct drm_file * file);
int i915_gem_pwrite_ioctl(struct drm_device * dev,void * data,struct drm_file * file)
{
	lx_emul_trace_and_stop(__func__);
}


extern void i915_gem_resume(struct drm_i915_private * i915);
void i915_gem_resume(struct drm_i915_private * i915)
{
	lx_emul_trace_and_stop(__func__);
}


extern void i915_gem_runtime_suspend(struct drm_i915_private * i915);
void i915_gem_runtime_suspend(struct drm_i915_private * i915)
{
	lx_emul_trace_and_stop(__func__);
}


extern void i915_gem_suspend(struct drm_i915_private * i915);
void i915_gem_suspend(struct drm_i915_private * i915)
{
	lx_emul_trace_and_stop(__func__);
}


extern void i915_gem_suspend_late(struct drm_i915_private * i915);
void i915_gem_suspend_late(struct drm_i915_private * i915)
{
	lx_emul_trace_and_stop(__func__);
}


extern int i915_gem_sw_finish_ioctl(struct drm_device * dev,void * data,struct drm_file * file);
int i915_gem_sw_finish_ioctl(struct drm_device * dev,void * data,struct drm_file * file)
{
	lx_emul_trace_and_stop(__func__);
}


extern int i915_gem_throttle_ioctl(struct drm_device * dev,void * data,struct drm_file * file);
int i915_gem_throttle_ioctl(struct drm_device * dev,void * data,struct drm_file * file)
{
	lx_emul_trace_and_stop(__func__);
}


extern int i915_gem_userptr_ioctl(struct drm_device * dev,void * data,struct drm_file * file);
int i915_gem_userptr_ioctl(struct drm_device * dev,void * data,struct drm_file * file)
{
	lx_emul_trace_and_stop(__func__);
}


extern int i915_gem_vm_create_ioctl(struct drm_device * dev,void * data,struct drm_file * file);
int i915_gem_vm_create_ioctl(struct drm_device * dev,void * data,struct drm_file * file)
{
	lx_emul_trace_and_stop(__func__);
}


extern int i915_gem_vm_destroy_ioctl(struct drm_device * dev,void * data,struct drm_file * file);
int i915_gem_vm_destroy_ioctl(struct drm_device * dev,void * data,struct drm_file * file)
{
	lx_emul_trace_and_stop(__func__);
}


extern int __must_check i915_gem_ww_ctx_backoff(struct i915_gem_ww_ctx * ww);
int __must_check i915_gem_ww_ctx_backoff(struct i915_gem_ww_ctx * ww)
{
	lx_emul_trace_and_stop(__func__);
}


extern void i915_gemfs_fini(struct drm_i915_private * i915);
void i915_gemfs_fini(struct drm_i915_private * i915)
{
	lx_emul_trace_and_stop(__func__);
}


extern ssize_t i915_gpu_coredump_copy_to_buffer(struct i915_gpu_coredump * error,char * buf,loff_t off,size_t rem);
ssize_t i915_gpu_coredump_copy_to_buffer(struct i915_gpu_coredump * error,char * buf,loff_t off,size_t rem)
{
	lx_emul_trace_and_stop(__func__);
}


extern void i915_lut_handle_free(struct i915_lut_handle * lut);
void i915_lut_handle_free(struct i915_lut_handle * lut)
{
	lx_emul_trace_and_stop(__func__);
}


extern int i915_perf_add_config_ioctl(struct drm_device * dev,void * data,struct drm_file * file);
int i915_perf_add_config_ioctl(struct drm_device * dev,void * data,struct drm_file * file)
{
	lx_emul_trace_and_stop(__func__);
}


extern void i915_perf_fini(struct drm_i915_private * i915);
void i915_perf_fini(struct drm_i915_private * i915)
{
	lx_emul_trace_and_stop(__func__);
}


extern int i915_perf_ioctl_version(void);
int i915_perf_ioctl_version(void)
{
	lx_emul_trace_and_stop(__func__);
}


extern int i915_perf_open_ioctl(struct drm_device * dev,void * data,struct drm_file * file);
int i915_perf_open_ioctl(struct drm_device * dev,void * data,struct drm_file * file)
{
	lx_emul_trace_and_stop(__func__);
}


extern int i915_perf_remove_config_ioctl(struct drm_device * dev,void * data,struct drm_file * file);
int i915_perf_remove_config_ioctl(struct drm_device * dev,void * data,struct drm_file * file)
{
	lx_emul_trace_and_stop(__func__);
}


extern void i915_perf_unregister(struct drm_i915_private * i915);
void i915_perf_unregister(struct drm_i915_private * i915)
{
	lx_emul_trace_and_stop(__func__);
}


extern void i915_pmu_exit(void);
void i915_pmu_exit(void)
{
	lx_emul_trace_and_stop(__func__);
}


extern void i915_pmu_unregister(struct drm_i915_private * i915);
void i915_pmu_unregister(struct drm_i915_private * i915)
{
	lx_emul_trace_and_stop(__func__);
}


extern struct i915_ppgtt * i915_ppgtt_create(struct intel_gt * gt);
struct i915_ppgtt * i915_ppgtt_create(struct intel_gt * gt)
{
	lx_emul_trace_and_stop(__func__);
}


extern int i915_query_ioctl(struct drm_device * dev,void * data,struct drm_file * file);
int i915_query_ioctl(struct drm_device * dev,void * data,struct drm_file * file)
{
	lx_emul_trace_and_stop(__func__);
}


extern void i915_request_add(struct i915_request * rq);
void i915_request_add(struct i915_request * rq)
{
	lx_emul_trace_and_stop(__func__);
}


extern struct i915_request * i915_request_create(struct intel_context * ce);
struct i915_request * i915_request_create(struct intel_context * ce)
{
	lx_emul_trace_and_stop(__func__);
}


extern long i915_request_wait(struct i915_request * rq,unsigned int flags,long timeout);
long i915_request_wait(struct i915_request * rq,unsigned int flags,long timeout)
{
	lx_emul_trace_and_stop(__func__);
}


extern void i915_reset_error_state(struct drm_i915_private * i915);
void i915_reset_error_state(struct drm_i915_private * i915)
{
	lx_emul_trace_and_stop(__func__);
}


extern int i915_vm_alloc_pt_stash(struct i915_address_space * vm,struct i915_vm_pt_stash * stash,u64 size);
int i915_vm_alloc_pt_stash(struct i915_address_space * vm,struct i915_vm_pt_stash * stash,u64 size)
{
	lx_emul_trace_and_stop(__func__);
}


extern void i915_vm_free_pt_stash(struct i915_address_space * vm,struct i915_vm_pt_stash * stash);
void i915_vm_free_pt_stash(struct i915_address_space * vm,struct i915_vm_pt_stash * stash)
{
	lx_emul_trace_and_stop(__func__);
}


extern int i915_vm_map_pt_stash(struct i915_address_space * vm,struct i915_vm_pt_stash * stash);
int i915_vm_map_pt_stash(struct i915_address_space * vm,struct i915_vm_pt_stash * stash)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/pseudo_fs.h>

struct pseudo_fs_context * init_pseudo(struct fs_context * fc,unsigned long magic)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/init.h>

bool initcall_debug;


extern void intel_dsb_indexed_reg_write(const struct intel_crtc_state * crtc_state,i915_reg_t reg,u32 val);
void intel_dsb_indexed_reg_write(const struct intel_crtc_state * crtc_state,i915_reg_t reg,u32 val)
{
	lx_emul_trace_and_stop(__func__);
}


extern void intel_dsb_reg_write(const struct intel_crtc_state * crtc_state,i915_reg_t reg,u32 val);
void intel_dsb_reg_write(const struct intel_crtc_state * crtc_state,i915_reg_t reg,u32 val)
{
	lx_emul_trace_and_stop(__func__);
}


extern int intel_dsi_dcs_init_backlight_funcs(struct intel_connector * intel_connector);
int intel_dsi_dcs_init_backlight_funcs(struct intel_connector * intel_connector)
{
	lx_emul_trace_and_stop(__func__);
}


extern void intel_dvo_init(struct drm_i915_private * dev_priv);
void intel_dvo_init(struct drm_i915_private * dev_priv)
{
	lx_emul_trace_and_stop(__func__);
}


extern int intel_engine_flush_barriers(struct intel_engine_cs * engine);
int intel_engine_flush_barriers(struct intel_engine_cs * engine)
{
	lx_emul_trace_and_stop(__func__);
}


extern struct intel_engine_cs * intel_engine_lookup_user(struct drm_i915_private * i915,u8 class,u8 instance);
struct intel_engine_cs * intel_engine_lookup_user(struct drm_i915_private * i915,u8 class,u8 instance)
{
	lx_emul_trace_and_stop(__func__);
}


extern unsigned int intel_engines_has_context_isolation(struct drm_i915_private * i915);
unsigned int intel_engines_has_context_isolation(struct drm_i915_private * i915)
{
	lx_emul_trace_and_stop(__func__);
}


extern int intel_freq_opcode(struct intel_rps * rps,int val);
int intel_freq_opcode(struct intel_rps * rps,int val)
{
	lx_emul_trace_and_stop(__func__);
}


extern int intel_gpu_freq(struct intel_rps * rps,int val);
int intel_gpu_freq(struct intel_rps * rps,int val)
{
	lx_emul_trace_and_stop(__func__);
}


#include <drm/i915_drm.h>

struct resource intel_graphics_stolen_res;


extern void intel_gt_check_and_clear_faults(struct intel_gt * gt);
void intel_gt_check_and_clear_faults(struct intel_gt * gt)
{
	lx_emul_trace_and_stop(__func__);
}


extern void intel_gt_driver_unregister(struct intel_gt * gt);
void intel_gt_driver_unregister(struct intel_gt * gt)
{
	lx_emul_trace_and_stop(__func__);
}


extern void intel_gt_flush_ggtt_writes(struct intel_gt * gt);
void intel_gt_flush_ggtt_writes(struct intel_gt * gt)
{
	lx_emul_trace_and_stop(__func__);
}


extern long intel_gt_retire_requests_timeout(struct intel_gt * gt,long timeout);
long intel_gt_retire_requests_timeout(struct intel_gt * gt,long timeout)
{
	lx_emul_trace_and_stop(__func__);
}


extern int intel_gt_runtime_resume(struct intel_gt * gt);
int intel_gt_runtime_resume(struct intel_gt * gt)
{
	lx_emul_trace_and_stop(__func__);
}


extern void intel_gt_runtime_suspend(struct intel_gt * gt);
void intel_gt_runtime_suspend(struct intel_gt * gt)
{
	lx_emul_trace_and_stop(__func__);
}


extern int intel_gt_wait_for_idle(struct intel_gt * gt,long timeout);
int intel_gt_wait_for_idle(struct intel_gt * gt,long timeout)
{
	lx_emul_trace_and_stop(__func__);
}


extern bool intel_has_gpu_reset(const struct intel_gt * gt);
bool intel_has_gpu_reset(const struct intel_gt * gt)
{
	lx_emul_trace_and_stop(__func__);
}


extern bool intel_has_reset_engine(const struct intel_gt * gt);
bool intel_has_reset_engine(const struct intel_gt * gt)
{
	lx_emul_trace_and_stop(__func__);
}


extern int intel_huc_check_status(struct intel_huc * huc);
int intel_huc_check_status(struct intel_huc * huc)
{
	lx_emul_trace_and_stop(__func__);
}


extern u8 intel_lookup_range_max_qp(int bpc,int buf_i,int bpp_i);
u8 intel_lookup_range_max_qp(int bpc,int buf_i,int bpp_i)
{
	lx_emul_trace_and_stop(__func__);
}


extern u8 intel_lookup_range_min_qp(int bpc,int buf_i,int bpp_i);
u8 intel_lookup_range_min_qp(int bpc,int buf_i,int bpp_i)
{
	lx_emul_trace_and_stop(__func__);
}


extern u32 * intel_ring_begin(struct i915_request * rq,unsigned int num_dwords);
u32 * intel_ring_begin(struct i915_request * rq,unsigned int num_dwords)
{
	lx_emul_trace_and_stop(__func__);
}


extern void intel_rps_boost(struct i915_request * rq);
void intel_rps_boost(struct i915_request * rq)
{
	lx_emul_trace_and_stop(__func__);
}


extern u32 intel_rps_read_actual_frequency(struct intel_rps * rps);
u32 intel_rps_read_actual_frequency(struct intel_rps * rps)
{
	lx_emul_trace_and_stop(__func__);
}


extern int intel_rps_set(struct intel_rps * rps,u8 val);
int intel_rps_set(struct intel_rps * rps,u8 val)
{
	lx_emul_trace_and_stop(__func__);
}


extern unsigned int intel_sseu_subslice_total(const struct sseu_dev_info * sseu);
unsigned int intel_sseu_subslice_total(const struct sseu_dev_info * sseu)
{
	lx_emul_trace_and_stop(__func__);
}


extern void intel_tv_init(struct drm_i915_private * dev_priv);
void intel_tv_init(struct drm_i915_private * dev_priv)
{
	lx_emul_trace_and_stop(__func__);
}


extern bool intel_vgpu_has_full_ppgtt(struct drm_i915_private * dev_priv);
bool intel_vgpu_has_full_ppgtt(struct drm_i915_private * dev_priv)
{
	lx_emul_trace_and_stop(__func__);
}


extern bool intel_vgpu_has_hwsp_emulation(struct drm_i915_private * dev_priv);
bool intel_vgpu_has_hwsp_emulation(struct drm_i915_private * dev_priv)
{
	lx_emul_trace_and_stop(__func__);
}


extern void intel_vgt_deballoon(struct i915_ggtt * ggtt);
void intel_vgt_deballoon(struct i915_ggtt * ggtt)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/sched.h>

void io_schedule_finish(int token)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/sched.h>

int io_schedule_prepare(void)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/sched.h>

long __sched io_schedule_timeout(long timeout)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/fs.h>

void iput(struct inode * inode)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/irq_work.h>

void irq_work_tick(void)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/property.h>

bool is_software_node(const struct fwnode_handle * fwnode)
{
	lx_emul_trace_and_stop(__func__);
}


extern void kernel_fpu_begin_mask(unsigned int kfpu_mask);
void kernel_fpu_begin_mask(unsigned int kfpu_mask)
{
	lx_emul_trace_and_stop(__func__);
}


extern void kernel_fpu_end(void);
void kernel_fpu_end(void)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/kobject.h>

struct kobject *kernel_kobj;


#include <linux/fs.h>

void kill_anon_super(struct super_block * sb)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/slab.h>

void kmem_cache_destroy(struct kmem_cache * s)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/slab.h>

int kmem_cache_shrink(struct kmem_cache * cachep)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/kmsg_dump.h>

void kmsg_dump(enum kmsg_dump_reason reason)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/swap.h>

void mark_page_accessed(struct page * page)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/io.h>

void memunmap(void * addr)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/netlink.h>

int netlink_broadcast(struct sock * ssk,struct sk_buff * skb,u32 portid,u32 group,gfp_t allocation)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/netlink.h>

int netlink_has_listeners(struct sock * sk,unsigned int group)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/netlink.h>

void netlink_kernel_release(struct sock * sk)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/netlink.h>

bool netlink_ns_capable(const struct sk_buff * skb,struct user_namespace * user_ns,int cap)
{
	lx_emul_trace_and_stop(__func__);
}


#include <net/netlink.h>

int netlink_rcv_skb(struct sk_buff * skb,int (* cb)(struct sk_buff *,struct nlmsghdr *,struct netlink_ext_ack *))
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/irq.h>

struct irq_chip no_irq_chip;


#include <linux/fs.h>

loff_t noop_llseek(struct file * file,loff_t offset,int whence)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/irq.h>

void note_interrupt(struct irq_desc * desc,irqreturn_t action_ret)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/smp.h>

void on_each_cpu_cond_mask(smp_cond_func_t cond_func,smp_call_func_t func,void * info,bool wait,const struct cpumask * mask)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/printk.h>

int oops_in_progress;	/* If set, an oops, panic(), BUG() or die() is in progress */


#include <linux/pagemap.h>

struct page * pagecache_get_page(struct address_space * mapping,pgoff_t index,int fgp_flags,gfp_t gfp_mask)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/fs.h>

int pagecache_write_begin(struct file * file,struct address_space * mapping,loff_t pos,unsigned len,unsigned flags,struct page ** pagep,void ** fsdata)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/fs.h>

int pagecache_write_end(struct file * file,struct address_space * mapping,loff_t pos,unsigned len,unsigned copied,struct page * page,void * fsdata)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/reboot.h>

enum reboot_mode panic_reboot_mode;


#include <linux/pci.h>

int pci_dev_present(const struct pci_device_id * ids)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/pci.h>

void pci_disable_device(struct pci_dev * dev)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/pci.h>

int pci_read_config_byte(const struct pci_dev * dev,int where,u8 * val)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/pci.h>

int pci_save_state(struct pci_dev * dev)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/pci.h>

int pci_set_power_state(struct pci_dev * dev,pci_power_t state)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/pci.h>

void pci_unmap_rom(struct pci_dev * pdev,void __iomem * rom)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/pci.h>

int pci_write_config_byte(const struct pci_dev * dev,int where,u8 val)
{
	lx_emul_trace_and_stop(__func__);
}


extern void ppgtt_bind_vma(struct i915_address_space * vm,struct i915_vm_pt_stash * stash,struct i915_vma * vma,enum i915_cache_level cache_level,u32 flags);
void ppgtt_bind_vma(struct i915_address_space * vm,struct i915_vm_pt_stash * stash,struct i915_vma * vma,enum i915_cache_level cache_level,u32 flags)
{
	lx_emul_trace_and_stop(__func__);
}


extern void ppgtt_unbind_vma(struct i915_address_space * vm,struct i915_vma * vma);
void ppgtt_unbind_vma(struct i915_address_space * vm,struct i915_vma * vma)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/printk.h>

int printk_deferred(const char * fmt,...)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/printk.h>

void printk_safe_flush_on_panic(void)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/pid.h>

void put_pid(struct pid * pid)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/reboot.h>

enum reboot_mode reboot_mode;


#include <linux/sched/rt.h>

void rt_mutex_setprio(struct task_struct * p,struct task_struct * pi_task)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/seq_file.h>

void seq_printf(struct seq_file * m,const char * f,...)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/seq_file.h>

void seq_puts(struct seq_file * m,const char * s)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/mm.h>

int set_page_dirty(struct page * page)
{
	lx_emul_trace_and_stop(__func__);
}


extern int set_pages_wb(struct page * page,int numpages);
int set_pages_wb(struct page * page,int numpages)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/shmem_fs.h>

struct file * shmem_file_setup_with_mnt(struct vfsmount * mnt,const char * name,loff_t size,unsigned long flags)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/shmem_fs.h>

void shmem_truncate_range(struct inode * inode,loff_t lstart,loff_t lend)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/mm.h>

void show_mem(unsigned int filter,nodemask_t * nodemask)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/sched/debug.h>

void show_state_filter(unsigned int state_filter)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/skbuff.h>

struct sk_buff * skb_copy_expand(const struct sk_buff * skb,int newheadroom,int newtailroom,gfp_t gfp_mask)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/skbuff.h>

void * skb_pull(struct sk_buff * skb,unsigned int len)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/skbuff.h>

void * skb_put(struct sk_buff * skb,unsigned int len)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/smp.h>

int smp_call_function_single(int cpu,smp_call_func_t func,void * info,int wait)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/jump_label.h>

bool static_key_initialized;


#include <linux/string_helpers.h>

int string_escape_mem(const char * src,size_t isz,char * dst,size_t osz,unsigned int flags,const char * only)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/printk.h>

int suppress_printk;


#include <linux/rcupdate.h>

void synchronize_rcu(void)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/task_work.h>

int task_work_add(struct task_struct * task,struct callback_head * work,enum task_work_notify_mode notify)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/task_work.h>

struct callback_head * task_work_cancel(struct task_struct * task,task_work_func_t func)
{
	lx_emul_trace_and_stop(__func__);
}


#include <drm/ttm/ttm_bo_driver.h>

void ttm_mem_io_free(struct ttm_device * bdev,struct ttm_resource * mem)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/vt_kern.h>

void unblank_screen(void)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/pagemap.h>

void unlock_page(struct page * page)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/mm.h>

void unmap_mapping_range(struct address_space * mapping,loff_t const holebegin,loff_t const holelen,int even_cows)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/fb.h>

void unregister_framebuffer(struct fb_info * fb_info)
{
	lx_emul_trace_and_stop(__func__);
}


extern void unregister_irq_proc(unsigned int irq,struct irq_desc * desc);
void unregister_irq_proc(unsigned int irq,struct irq_desc * desc)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/vmalloc.h>

void * vmap(struct page ** pages,unsigned int count,unsigned long flags,pgprot_t prot)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/vmalloc.h>

void vunmap(const void * addr)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/sched/wake_q.h>

void wake_q_add_safe(struct wake_q_head * head,struct task_struct * task)
{
	lx_emul_trace_and_stop(__func__);
}

