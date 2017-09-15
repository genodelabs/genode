#include <lx_emul.h>
#include <lx_emul_c.h>
#include <drm/drmP.h>
#include <drm/drm_crtc.h>
#include <drm/drm_dp_mst_helper.h>
#include <drm/drm_rect.h>
#include <i915/i915_drv.h>

bool access_ok(int access, void *addr, size_t size)
{
	TRACE_AND_STOP;
	return -1;
}

int acpi_device_uevent_modalias(struct device *dev, struct kobj_uevent_env *ev)
{
	TRACE_AND_STOP;
	return -1;
}

bool acpi_driver_match_device(struct device *dev, const struct device_driver *drv)
{
	TRACE_AND_STOP;
	return -1;
}

int acpi_lid_notifier_unregister(struct notifier_block *nb)
{
	TRACE_AND_STOP;
	return -1;
}

int acpi_lid_open(void)
{
	TRACE_AND_STOP;
	return -1;
}

void acpi_video_unregister(void)
{
	TRACE_AND_STOP;
}

int add_uevent_var(struct kobj_uevent_env *env, const char *format, ...)
{
	TRACE_AND_STOP;
	return -1;
}


int bitmap_weight(const unsigned long *src, unsigned int nbits)
{
	TRACE_AND_STOP;
	return -1;
}

bool capable(int cap)
{
	TRACE_AND_STOP;
	return false;
}

void cfb_fillrect(struct fb_info *info, const struct fb_fillrect *rect)
{
	TRACE_AND_STOP;
}

void cfb_copyarea(struct fb_info *info, const struct fb_copyarea *area)
{
	TRACE_AND_STOP;
}

void cfb_imageblit(struct fb_info *info, const struct fb_image *image)
{
	TRACE_AND_STOP;
}

void console_lock(void)
{
	TRACE_AND_STOP;
}

int console_trylock(void)
{
	TRACE_AND_STOP;
	return -1;
}

void console_unlock(void)
{
	TRACE_AND_STOP;
}

size_t copy_from_user(void *to, void const *from, size_t len)
{
	TRACE_AND_STOP;
	return -1;
}

size_t copy_to_user(void *dst, void const *src, size_t len)
{
	TRACE_AND_STOP;
	return -1;
}

void cpufreq_cpu_put(struct cpufreq_policy *policy)
{
	TRACE_AND_STOP;
}

void destroy_timer_on_stack(struct timer_list *timer)
{
	TRACE_AND_STOP;
}

void destroy_workqueue(struct workqueue_struct *wq)
{
	TRACE_AND_STOP;
}

void *dev_get_drvdata(const struct device *dev)
{
	TRACE_AND_STOP;
	return NULL;
}

int device_for_each_child(struct device *dev, void *data, int (*fn)(struct device *dev, void *data))
{
	TRACE_AND_STOP;
	return -1;
}

int device_init_wakeup(struct device *dev, bool val)
{
	TRACE_AND_STOP;
	return -1;
}

void device_unregister(struct device *dev)
{
	TRACE_AND_STOP;
}

const char *dev_name(const struct device *dev)
{
	TRACE_AND_STOP;
	return NULL;
}

int dma_set_coherent_mask(struct device *dev, u64 mask)
{
	TRACE_AND_STOP;
	return -1;
}

void dma_unmap_page(struct device *dev, dma_addr_t dma_address, size_t size, enum dma_data_direction direction)
{
	TRACE_AND_STOP;
}

void down_read(struct rw_semaphore *sem)
{
	TRACE_AND_STOP;
}

void drm_clflush_virt_range(void *addr, unsigned long length)
{
	TRACE_AND_STOP;
}

int drm_dp_mst_hpd_irq(struct drm_dp_mst_topology_mgr *mgr, u8 *esi, bool *handled)
{
	TRACE_AND_STOP;
	return -1;
}

int drm_dp_mst_topology_mgr_resume(struct drm_dp_mst_topology_mgr *mgr)
{
	TRACE_AND_STOP;
	return -1;
}

int drm_dp_mst_topology_mgr_set_mst(struct drm_dp_mst_topology_mgr *mgr, bool mst_state)
{
	TRACE_AND_STOP;
	return -1;
}

void drm_dp_mst_topology_mgr_suspend(struct drm_dp_mst_topology_mgr *mgr)
{
	TRACE_AND_STOP;
}

void drm_free_large(void *ptr)
{
	TRACE_AND_STOP;
}

int drm_gem_create_mmap_offset(struct drm_gem_object *obj)
{
	TRACE_AND_STOP;
	return -1;
}

int drm_gem_dumb_destroy(struct drm_file *file, struct drm_device *dev, uint32_t handle)
{
	TRACE_AND_STOP;
	return -1;
}

int drm_gem_handle_create(struct drm_file *file_priv, struct drm_gem_object *obj, u32 *handlep)
{
	TRACE_AND_STOP;
	return -1;
}

int drm_gem_mmap(struct file *filp, struct vm_area_struct *vma)
{
	TRACE_AND_STOP;
	return -1;
}

struct drm_gem_object *drm_gem_object_lookup(struct drm_device *dev, struct drm_file *filp, u32 handle)
{
	TRACE_AND_STOP;
	return NULL;
}

int drm_gem_prime_fd_to_handle(struct drm_device *dev, struct drm_file *file_priv, int prime_fd, uint32_t *handle)
{
	TRACE_AND_STOP;
	return -1;
}

int drm_gem_prime_handle_to_fd(struct drm_device *dev, struct drm_file *file_priv, uint32_t handle, uint32_t flags, int *prime_fd)
{
	TRACE_AND_STOP;
	return -1;
}

void drm_gem_vm_close(struct vm_area_struct *vma)
{
	TRACE_AND_STOP;
}

void drm_gem_vm_open(struct vm_area_struct *vma)
{
	TRACE_AND_STOP;
}

long drm_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	TRACE_AND_STOP;
	return -1;
}

void *drm_malloc_ab(size_t nmemb, size_t size)
{
	TRACE_AND_STOP;
	return NULL;
}

int drm_noop(struct drm_device *dev, void *data, struct drm_file *file_priv)
{
	TRACE_AND_STOP;
	return -1;
}

int drm_open(struct inode *inode, struct file *filp)
{
	TRACE_AND_STOP;
	return -1;
}

struct drm_dma_handle *drm_pci_alloc(struct drm_device *dev, size_t size, size_t align)
{
	TRACE_AND_STOP;
	return NULL;
}

void drm_pci_free(struct drm_device *dev, struct drm_dma_handle * dmah)
{
	TRACE_AND_STOP;
}

unsigned int drm_poll(struct file *filp, struct poll_table_struct *wait)
{
	TRACE_AND_STOP;
	return -1;
}

void drm_prime_gem_destroy(struct drm_gem_object *obj, struct sg_table *sg)
{
	TRACE_AND_STOP;
}

void drm_put_dev(struct drm_device *dev)
{
	TRACE_AND_STOP;
}

ssize_t drm_read(struct file *filp, char __user *buffer, size_t count, loff_t *offset)
{
	TRACE_AND_STOP;
	return -1;
}

int drm_release(struct inode *inode, struct file *filp)
{
	TRACE_AND_STOP;
	return -1;
}

bool drm_vma_node_has_offset(struct drm_vma_offset_node *node)
{
	TRACE_AND_STOP;
	return -1;
}

__u64 drm_vma_node_offset_addr(struct drm_vma_offset_node *node)
{
	TRACE_AND_STOP;
	return -1;
}

void drm_vma_node_unmap(struct drm_vma_offset_node *node, struct address_space *file_mapping)
{
	TRACE_AND_STOP;
}

int fault_in_multipages_readable(const char __user *uaddr, int size)
{
	TRACE_AND_STOP;
	return -1;
}

int fault_in_multipages_writeable(char __user *uaddr, int size)
{
	TRACE_AND_STOP;
	return -1;
}

void fb_set_suspend(struct fb_info *info, int state)
{
	TRACE_AND_STOP;
}

struct vm_area_struct *find_vma(struct mm_struct *mm, unsigned long addr)
{
	TRACE_AND_STOP;
	return NULL;
}

void flush_scheduled_work(void)
{
	TRACE_AND_STOP;
}

void flush_workqueue(struct workqueue_struct *wq)
{
	TRACE_AND_STOP;
}

void __free_pages(struct page *page, unsigned int order)
{
	TRACE_AND_STOP;
}

unsigned long __get_free_pages(gfp_t gfp_mask, unsigned int order)
{
	TRACE_AND_STOP;
	return -1;
}

unsigned long get_seconds(void)
{
	TRACE_AND_STOP;
	return -1;
}

void gpio_free(unsigned gpio)
{
	TRACE_AND_STOP;
}

int gpio_get_value(unsigned int gpio)
{
	TRACE_AND_STOP;
	return -1;
}

bool gpio_is_valid(int number)
{
	TRACE_AND_STOP;
	return -1;
}

int gpio_request_one(unsigned gpio, unsigned long flags, const char *label)
{
	TRACE_AND_STOP;
	return -1;
}

void gpio_set_value(unsigned int gpio, int value)
{
	TRACE_AND_STOP;
}

void ips_link_to_i915_driver(void)
{
	TRACE;
}

void i915_audio_component_cleanup(struct drm_i915_private *dev_priv)
{
	TRACE_AND_STOP;
}

void i915_capture_error_state(struct drm_device *dev, bool wedge, const char *error_msg)
{
	TRACE_AND_STOP;
}

int i915_cmd_parser_get_version(void)
{
	TRACE_AND_STOP;
	return -1;
}

void i915_destroy_error_state(struct drm_device *dev)
{
	TRACE_AND_STOP;
}

void i915_gem_batch_pool_fini(struct i915_gem_batch_pool *pool)
{
	TRACE_AND_STOP;
}

int __must_check i915_gem_evict_something(struct drm_device *dev, struct i915_address_space *vm, int min_size, unsigned alignment, unsigned cache_level, unsigned long start, unsigned long end, unsigned flags)
{
	TRACE_AND_STOP;
	return -1;
}

int i915_gem_execbuffer(struct drm_device *dev, void *data, struct drm_file *file_priv)
{
	TRACE_AND_STOP;
	return -1;
}

int i915_gem_execbuffer2(struct drm_device *dev, void *data, struct drm_file *file_priv)
{
	TRACE_AND_STOP;
	return -1;
}

int i915_gem_get_tiling(struct drm_device *dev, void *data, struct drm_file *file)
{
	TRACE_AND_STOP;
	return -1;
}

int i915_gem_set_tiling(struct drm_device *dev, void *data, struct drm_file *file)
{
	TRACE_AND_STOP;
	return -1;
}

struct dma_buf *i915_gem_prime_export(struct drm_device *dev, struct drm_gem_object *gem_obj, int flags)
{
	TRACE_AND_STOP;
	return NULL;
}

struct drm_gem_object *i915_gem_prime_import(struct drm_device *dev, struct dma_buf *dma_buf)
{
	TRACE_AND_STOP;
	return NULL;
}

int i915_gem_ringbuffer_submission(struct i915_execbuffer_params *params, struct drm_i915_gem_execbuffer2 *args, struct list_head *vmas)
{
	TRACE_AND_STOP;
	return -1;
}

unsigned long i915_gem_shrink(struct drm_i915_private *dev_priv, unsigned long target, unsigned flags)
{
	TRACE_AND_STOP;
	return -1;
}

unsigned long i915_gem_shrink_all(struct drm_i915_private *dev_priv)
{
	TRACE_AND_STOP;
	return -1;
}

int i915_gem_userptr_ioctl(struct drm_device *dev, void *data, struct drm_file *file)
{
	TRACE_AND_STOP;
	return -1;
}

void i915_get_extra_instdone(struct drm_device *dev, uint32_t *instdone)
{
	TRACE_AND_STOP;
}

int i915_restore_state(struct drm_device *dev)
{
	TRACE_AND_STOP;
	return -1;
}

int i915_save_state(struct drm_device *dev)
{
	TRACE_AND_STOP;
	return -1;
}

void i915_teardown_sysfs(struct drm_device *dev_priv)
{
	TRACE_AND_STOP;
}

void idr_destroy(struct idr *idp)
{
	TRACE_AND_STOP;
}

void *idr_get_next(struct idr *idp, int *nextid)
{
	TRACE_AND_STOP;
	return NULL;
}

void intel_csr_load_program(struct drm_device *dev)
{
	TRACE_AND_STOP;
}

void intel_csr_ucode_fini(struct drm_device *dev)
{
	TRACE_AND_STOP;
}

void intel_dp_mst_encoder_cleanup(struct intel_digital_port *intel_dig_port)
{
	TRACE_AND_STOP;
}

void intel_dsi_init(struct drm_device *dev)
{
	TRACE_AND_STOP;
}

void intel_dvo_init(struct drm_device *dev)
{
	TRACE_AND_STOP;
}

void intel_execlists_retire_requests(struct intel_engine_cs *ring)
{
	TRACE_AND_STOP;
}

int intel_execlists_submission(struct i915_execbuffer_params *params, struct drm_i915_gem_execbuffer2 *args, struct list_head *vmas)
{
	TRACE_AND_STOP;
	return -1;
}

int intel_guc_resume(struct drm_device *dev)
{
	TRACE_AND_STOP;
	return -1;
}

int intel_guc_suspend(struct drm_device *dev)
{
	TRACE_AND_STOP;
	return -1;
}

void intel_guc_ucode_fini(struct drm_device *dev)
{
	TRACE_AND_STOP;
}

int intel_logical_ring_alloc_request_extras(struct drm_i915_gem_request *request)
{
	TRACE_AND_STOP;
	return -1;
}

void intel_logical_ring_cleanup(struct intel_engine_cs *ring)
{
	TRACE_AND_STOP;
}

int intel_logical_ring_reserve_space(struct drm_i915_gem_request *request)
{
	TRACE_AND_STOP;
	return -1;
}

void intel_logical_ring_stop(struct intel_engine_cs *ring)
{
	TRACE_AND_STOP;
}

void intel_lr_context_unpin(struct drm_i915_gem_request *req)
{
	TRACE_AND_STOP;
}

void intel_tv_init(struct drm_device *dev)
{
	TRACE_AND_STOP;
}

void io_mapping_free(struct io_mapping *mapping)
{
	TRACE_AND_STOP;
}

void *io_mapping_map_atomic_wc(struct io_mapping *mapping, unsigned long offset)
{
	TRACE_AND_STOP;
	return NULL;
}

void __iomem * io_mapping_map_wc(struct io_mapping *mapping, unsigned long offset)
{
	TRACE_AND_STOP;
	return NULL;
}

void io_mapping_unmap(void __iomem *vaddr)
{
	TRACE_AND_STOP;
}

void io_mapping_unmap_atomic(void *vaddr)
{
	TRACE_AND_STOP;
}

void io_schedule(void)
{
	TRACE_AND_STOP;
}

unsigned int jiffies_to_usecs(const unsigned long j)
{
	TRACE_AND_STOP;
	return -1;
}

int kobject_uevent_env(struct kobject *kobj, enum kobject_action action, char *envp[])
{
	TRACE_AND_STOP;
	return -1;
}

int  kref_get_unless_zero(struct kref *kref)
{
	TRACE_AND_STOP;
	return -1;
}

u64 local_clock(void)
{
	TRACE_AND_STOP;
	return -1;
}

int logical_ring_flush_all_caches(struct drm_i915_gem_request *req)
{
	TRACE_AND_STOP;
	return -1;
}

void *memchr_inv(const void *s, int c, size_t n)
{
	TRACE_AND_STOP;
	return NULL;
}

void memcpy_toio(volatile void __iomem *dst, const void *src, size_t count)
{
	TRACE_AND_STOP;
}

void ndelay(unsigned long ns)
{
	TRACE_AND_STOP;
}

bool need_resched(void)
{
	TRACE_AND_STOP;
	return -1;
}

loff_t noop_llseek(struct file *file, loff_t offset, int whence)
{
	TRACE_AND_STOP;
	return -1;
}

extern u64 nsecs_to_jiffies64(u64 n)
{
	TRACE_AND_STOP;
	return -1;
}

int of_alias_get_id(struct device_node *np, const char *stem)
{
	TRACE_AND_STOP;
	return -1;
}

int of_driver_match_device(struct device *dev, const struct device_driver *drv)
{
	TRACE_AND_STOP;
	return -1;
}

int of_irq_get(struct device_node *dev, int index)
{
	TRACE_AND_STOP;
	return -1;
}

int of_irq_get_byname(struct device_node *dev, const char *name)
{
	TRACE_AND_STOP;
	return -1;
}

void of_node_clear_flag(struct device_node *n, unsigned long flag)
{
	TRACE_AND_STOP;
}

resource_size_t pcibios_align_resource(void * p, const struct resource *r, resource_size_t s1, resource_size_t s2)
{
	TRACE_AND_STOP;
	return -1;
}

void pci_disable_device(struct pci_dev *dev)
{
	TRACE_AND_STOP;
}

void pci_disable_msi(struct pci_dev *dev)
{
	TRACE_AND_STOP;
}

int pci_enable_device(struct pci_dev *dev)
{
	TRACE_AND_STOP;
	return -1;
}

struct pci_dev *pci_get_device(unsigned int vendor, unsigned int device, struct pci_dev *from)
{
	TRACE_AND_STOP;
	return NULL;
}

void *pci_get_drvdata(struct pci_dev *pdev)
{
	TRACE_AND_STOP;
	return NULL;
}

void pci_iounmap(struct pci_dev *dev, void __iomem *p)
{
	TRACE_AND_STOP;
}

int pci_save_state(struct pci_dev *dev)
{
	TRACE_AND_STOP;
	return -1;
}

int pci_set_power_state(struct pci_dev *dev, pci_power_t state)
{
	TRACE_AND_STOP;
	return -1;
}

void pci_unmap_page(struct pci_dev *hwdev, dma_addr_t dma_address, size_t size, int direction)
{
	TRACE_AND_STOP;
}

pgprot_t pgprot_writecombine(pgprot_t prot)
{
	TRACE_AND_STOP;
	return prot;
}

void pm_qos_remove_request(struct pm_qos_request *req)
{
	TRACE_AND_STOP;
}

void print_hex_dump(const char *level, const char *prefix_str, int prefix_type, int rowsize, int groupsize, const void *buf, size_t len, bool ascii)
{
	TRACE_AND_STOP;
}

int PTR_ERR_OR_ZERO(__force const void *ptr)
{
	TRACE_AND_STOP;
	return -1;
}

void put_pid(struct pid *pid)
{
	TRACE_AND_STOP;
}

int pwm_config(struct pwm_device *pwm, int duty_ns, int period_ns)
{
	TRACE_AND_STOP;
	return -1;
}

void pwm_disable(struct pwm_device *pwm)
{
	TRACE_AND_STOP;
}

int pwm_enable(struct pwm_device *pwm)
{
	TRACE_AND_STOP;
	return -1;
}

struct pwm_device *pwm_get(struct device *dev, const char *con_id)
{
	TRACE_AND_STOP;
	return NULL;
}

unsigned int pwm_get_duty_cycle(const struct pwm_device *pwm)
{
	TRACE_AND_STOP;
	return -1;
}

void pwm_put(struct pwm_device *pwm)
{
	TRACE_AND_STOP;
}

int register_reboot_notifier(struct notifier_block *nb)
{
	TRACE_AND_STOP;
	return -1;
}

int release_resource(struct resource *r)
{
	TRACE_AND_STOP;
	return -1;
}

int request_resource(struct resource *root, struct resource *r)
{
	TRACE_AND_STOP;
	return -1;
}

void seq_printf(struct seq_file *m, const char *fmt, ...)
{
	TRACE_AND_STOP;
}

void seq_puts(struct seq_file *m, const char *s)
{
	TRACE_AND_STOP;
}

int set_page_dirty(struct page *page)
{
	TRACE_AND_STOP;
	return -1;
}

int set_pages_wb(struct page *page, int numpages)
{
	TRACE_AND_STOP;
	return -1;
}

int    sg_nents(struct scatterlist *sg)
{
	TRACE_AND_STOP;
	return -1;
}

struct page *shmem_read_mapping_page( struct address_space *mapping, pgoff_t index)
{
	TRACE_AND_STOP;
	return NULL;
}

void shmem_truncate_range(struct inode *inode, loff_t start, loff_t end)
{
	TRACE_AND_STOP;
}

int signal_pending_state(long state, struct task_struct *p)
{
	TRACE_AND_STOP;
	return -1;
}

int    strcmp(const char *s1, const char *s2)
{
	TRACE_AND_STOP;
	return -1;
}

void sysfs_remove_link(struct kobject *kobj, const char *name)
{
	TRACE_AND_STOP;
}

int unregister_oom_notifier(struct notifier_block *nb)
{
	TRACE_AND_STOP;
	return -1;
}

int unregister_reboot_notifier(struct notifier_block *nb)
{
	TRACE_AND_STOP;
	return -1;
}

void unregister_shrinker(struct shrinker *s)
{
	TRACE_AND_STOP;
}

void up_read(struct rw_semaphore *sem)
{
	TRACE_AND_STOP;
}

int vga_switcheroo_process_delayed_switch(void)
{
	TRACE_AND_STOP;
	return -1;
}

void vga_switcheroo_unregister_client(struct pci_dev *dev)
{
	TRACE_AND_STOP;
}

phys_addr_t virt_to_phys(volatile void *address)
{
	TRACE_AND_STOP;
	return -1;
}

pgprot_t vm_get_page_prot(unsigned long vm_flags)
{
	pgprot_t prot;
	TRACE_AND_STOP;
	return prot;
}

int vm_insert_pfn(struct vm_area_struct *vma, unsigned long addr, unsigned long pfn)
{
	TRACE_AND_STOP;
	return -1;
}

unsigned long vm_mmap(struct file *f, unsigned long l1, unsigned long l2, unsigned long l3, unsigned long l4, unsigned long l5)
{
	TRACE_AND_STOP;
	return -1;
}

int vscnprintf(char *buf, size_t size, const char *fmt, va_list args)
{
	TRACE_AND_STOP;
	return -1;
}

int __wait_completion(struct completion *work, unsigned long to)
{
	TRACE_AND_STOP;
	return -1;
}

int wake_up_process(struct task_struct *tsk)
{
	TRACE_AND_STOP;
	return -1;
}

void wbinvd_on_all_cpus()
{
	TRACE_AND_STOP;
}

void yield(void)
{
	TRACE_AND_STOP;
}
void bus_unregister(struct bus_type *bus)
{
	TRACE_AND_STOP;
}
int drm_pci_set_busid(struct drm_device *dev, struct drm_master *master)
{
	TRACE_AND_STOP;
	return -1;
}

enum csr_state intel_csr_load_status_get(struct drm_i915_private *dev_priv)
{
	TRACE_AND_STOP;
	return -1;
}

void assert_csr_loaded(struct drm_i915_private *dev_priv)
{
	TRACE_AND_STOP;
}

bool drm_bridge_mode_fixup(struct drm_bridge *bridge, const struct drm_display_mode *mode, struct drm_display_mode *adjusted_mode)
{
	TRACE_AND_STOP;
	return -1;
}

void i915_cmd_parser_fini_ring(struct intel_engine_cs *ring)
{
	TRACE_AND_STOP;
}

unsigned long find_next_zero_bit(const unsigned long *addr, unsigned long size, unsigned long offset)
{
	TRACE_AND_STOP;
	return -1;
}

void kvfree(const void *addr)
{
	TRACE_AND_STOP;
}

int pci_map_sg(struct pci_dev *hwdev, struct scatterlist *sg, int nents, int direction)
{
	TRACE_AND_STOP;
	return -1;
}

void pci_unmap_sg(struct pci_dev *hwdev, struct scatterlist *sg, int nents, int direction)
{
	TRACE_AND_STOP;
}

void *vmalloc(unsigned long size)
{
	TRACE_AND_STOP;
	return NULL;
}
void intel_lr_context_free(struct intel_context *ctx)
{
	TRACE_AND_STOP;
}

void intel_lr_context_reset(struct drm_device *dev, struct intel_context *ctx)
{
	TRACE_AND_STOP;
}

int idr_for_each(struct idr *idp, int (*fn)(int id, void *p, void *data), void *data)
{
	TRACE_AND_STOP;
	return -1;
}

void ww_mutex_lock_slow(struct ww_mutex *lock, struct ww_acquire_ctx *ctx)
{
	TRACE_AND_STOP;
}

int  ww_mutex_lock_slow_interruptible(struct ww_mutex *lock, struct ww_acquire_ctx *ctx)
{
	TRACE_AND_STOP;
	return -1;
}

int  ww_mutex_trylock(struct ww_mutex *lock)
{
	TRACE_AND_STOP;
	return -1;
}

void free_irq(unsigned int irq, void *dev_id)
{
	TRACE_AND_STOP;
}

ktime_t ktime_get_real(void)
{
	ktime_t ret;
	TRACE_AND_STOP;
	return ret;
}

ktime_t ktime_mono_to_real(ktime_t mono)
{
	ktime_t ret;
	TRACE_AND_STOP;
	return ret;
}

void* bl_get_data(struct backlight_device *bl_dev)
{
	TRACE_AND_STOP;
	return NULL;
}

void backlight_device_unregister(struct backlight_device *bd)
{
	TRACE_AND_STOP;
}
int  ww_mutex_lock_interruptible(struct ww_mutex *lock, struct ww_acquire_ctx *ctx)
{
	TRACE_AND_STOP;
	return -1;
}

struct page *virt_to_page(void *addr)
{
	TRACE_AND_STOP;
	return NULL;
}

void ClearPageReserved(struct page *page)
{
	TRACE_AND_STOP;
}

const char *acpi_dev_name(struct acpi_device *adev)
{
	TRACE_AND_STOP;
	return NULL;
}

