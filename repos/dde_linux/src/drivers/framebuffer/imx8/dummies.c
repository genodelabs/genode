#include <lx_emul.h>
#include <lx_emul_c.h>

#include <drm/drm_device.h>
#include <drm/drm_gem.h>
#include <linux/component.h>
#include <linux/irqdomain.h>
#include <linux/irqhandler.h>
#include <linux/ratelimit.h>
#include <soc/imx8/sc/types.h>
#include <uapi/linux/i2c.h>


/************************
 ** drivers/base/bus.c **
 ************************/

int bus_for_each_drv(struct bus_type *bus, struct device_driver *start,
                     void *data, int (*fn)(struct device_driver *, void *))
{
	TRACE_AND_STOP;
	return -1;
}


/*************************
 ** drivers/base/core.c **
 *************************/

int device_for_each_child(struct device *dev, void *data,
                          int (*fn)(struct device *dev, void *data))
{
	TRACE_AND_STOP;
	return -1;
}

int device_register(struct device *dev)
{
	TRACE_AND_STOP;
	return -1;
}


/********************************
 ** drivers/base/dma-mapping.c **
 ********************************/

void dmam_free_coherent(struct device *dev, size_t size, void *vaddr,
                        dma_addr_t dma_handle)
{
	TRACE_AND_STOP;
}


/*****************************
 ** drivers/base/platform.c **
 *****************************/

int platform_device_put(struct platform_device *pdev)
{
	TRACE_AND_STOP;
	return 0;
}

void platform_device_unregister(struct platform_device *pdev)
{
	TRACE_AND_STOP;
}


/*****************************
 ** drivers/base/property.c **
 *****************************/

int device_add_properties(struct device *dev, const struct property_entry *properties)
{
	TRACE_AND_STOP;
}

void device_remove_properties(struct device *dev)
{
	TRACE_AND_STOP;
}


/***********************
 ** drivers/clk/clk.c **
 ***********************/

struct clk *clk_get_parent(struct clk *clk)
{
	TRACE_AND_STOP;
	return NULL;
}

bool clk_is_match(const struct clk *p, const struct clk *q)
{
	TRACE_AND_STOP;
	return false;
}

int clk_set_parent(struct clk *clk, struct clk *parent)
{
	TRACE;
	return 0;
}


/*******************************
 ** drivers/gpu/drm/drm_drv.c **
 *******************************/

void drm_dev_unref(struct drm_device *dev)
{
	TRACE_AND_STOP;
}

void drm_dev_unregister(struct drm_device *dev)
{
	TRACE_AND_STOP;
}


/*****************************************
 ** drivers/gpu/drm/drm_fb_cma_helper.c **
 *****************************************/

struct drm_framebuffer *drm_fb_cma_create(struct drm_device *dev,
                                          struct drm_file *file_priv,
                                          const struct drm_mode_fb_cmd2 *mode_cmd)
{
	TRACE_AND_STOP;
	return NULL;
}

struct drm_fbdev_cma;

void drm_fbdev_cma_hotplug_event(struct drm_fbdev_cma *fbdev_cma)
{
	TRACE;
}


/*******************************
 ** drivers/i2c/i2c-core-of.c **
 *******************************/

struct i2c_adapter;

void of_i2c_register_devices(struct i2c_adapter *adap)
{
	TRACE_AND_STOP;
}


/**********************************
 ** drivers/i2c/i2c-core-smbus.c **
 **********************************/

struct i2c_adapter;

s32 i2c_smbus_xfer(struct i2c_adapter *adapter, u16 addr, unsigned short flags,
		   char read_write, u8 command, int protocol,
		   union i2c_smbus_data *data) {
	TRACE_AND_STOP;
}


/***********************
 ** drivers/of/base.c **
 ***********************/

bool of_device_is_available(const struct device_node *device)
{
	return device;
}


/**************************
 ** drivers/of/dynamic.c **
 **************************/

struct device_node *of_node_get(struct device_node *node)
{
	TRACE;
	return node;
}

void of_node_put(struct device_node *node)
{
	TRACE;
}


/***********************************
 ** drivers/soc/imx/sc/main/ipc.c **
 ***********************************/

int sc_ipc_getMuID(uint32_t *mu_id)
{
	TRACE_AND_STOP;
	return -1;
}

void sc_ipc_close(sc_ipc_t handle)
{
	TRACE_AND_STOP;
}

sc_err_t sc_ipc_open(sc_ipc_t *handle, uint32_t id)
{
	TRACE_AND_STOP;
	return -1;
}


/********************************************
 ** drivers/soc/imx/sc/svc/misc/rpc_clnt.c **
 ********************************************/

sc_err_t sc_misc_set_control(sc_ipc_t ipc, sc_rsrc_t resource,
                             sc_ctrl_t ctrl, uint32_t val)
{
	TRACE_AND_STOP;
	return -1;
}


/***********************
 ** kernel/irq/chip.c **
 ***********************/

void handle_level_irq(struct irq_desc *desc)
{
	TRACE_AND_STOP;
}

void handle_simple_irq(struct irq_desc *desc)
{
	TRACE_AND_STOP;
}

void irq_chip_eoi_parent(struct irq_data *data)
{
	TRACE;
}

struct irq_data *irq_get_irq_data(unsigned int irq)
{
	TRACE_AND_STOP;
}

int irq_set_chip_data(unsigned int irq, void *data)
{
	TRACE;
	return 0;
}


/****************************
 ** kernel/irq/irqdomain.c **
 ****************************/

unsigned int irq_create_mapping(struct irq_domain *host, irq_hw_number_t hwirq)
{
	TRACE_AND_STOP;
}

int irq_domain_xlate_twocell(struct irq_domain *d, struct device_node *ctrlr,
                             const u32 *intspec, unsigned int intsize,
                             irq_hw_number_t *out_hwirq, unsigned int *out_type)
{
	TRACE_AND_STOP;
	return -1;
}


/******************
 ** lib/string.c **
 ******************/

char *strstr(const char * a0, const char * a1)
{
	TRACE_AND_STOP;
	return NULL;
}


/*****************
 ** linux/clk.h **
 *****************/

void clk_disable_unprepare(struct clk *clk)
{
	TRACE;
}

int clk_prepare_enable(struct clk *clk)
{
	TRACE;
	return 0;
}


/******************
 ** linux/gpio.h **
 ******************/

void gpio_free(unsigned gpio)
{
	TRACE_AND_STOP;
}

int gpio_get_value(unsigned int gpio)
{
	TRACE_AND_STOP;
}

bool gpio_is_valid(int number)
{
	TRACE_AND_STOP;
	return false;
}

int gpio_request_one(unsigned gpio, unsigned long flags, const char *label)
{
	TRACE_AND_STOP;
}

void gpio_set_value(unsigned int gpio, int value)
{
	TRACE_AND_STOP;
}


/*****************
 ** linux/i2c.h **
 *****************/

struct i2c_client;

const struct of_device_id *i2c_of_match_device(const struct of_device_id *matches,
                                               struct i2c_client *client)
{
	TRACE_AND_STOP;
	return NULL;
}


/*****************
 ** linux/irq.h **
 *****************/

void irq_set_status_flags(unsigned int irq, unsigned long set)
{
	TRACE;
}

void irqd_set_trigger_type(struct irq_data *d, u32 type)
{
	TRACE_AND_STOP;
}


/****************
 ** linux/of.h **
 ****************/

bool is_of_node(const struct fwnode_handle *fwnode)
{
	TRACE_AND_STOP;
	return false;
}


/************************
 ** linux/pm_runtime.h **
 ************************/

int pm_runtime_get_sync(struct device *dev)
{
	TRACE;
	return 0;
}

int pm_runtime_put_sync(struct device *dev)
{
	TRACE_AND_STOP;
	return 0;
}


/**********************
 ** linux/spinlock.h **
 **********************/

void assert_spin_locked(spinlock_t *lock)
{
	TRACE;
}


/*************************
 ** linux/timekeeping.h **
 *************************/

ktime_t ktime_get_real(void)
{
	TRACE_AND_STOP;
	return -1;
}


/*************************
 ** linux/dma-mapping.h **
 *************************/

int
dma_get_sgtable_attrs(struct device *dev, struct sg_table *sgt, void *cpu_addr,
                      dma_addr_t dma_addr, size_t size,
                      unsigned long attrs)
{
	TRACE_AND_STOP;
	return -1;
}

int dma_mmap_wc(struct device *dev,
                struct vm_area_struct *vma,
                void *cpu_addr, dma_addr_t dma_addr,
                size_t size)
{
	TRACE_AND_STOP;
	return -1;
}


/************************
 ** linux/pm-runtime.h **
 ************************/

void pm_runtime_enable(struct device *dev)
{
	TRACE;
}

void pm_runtime_disable(struct device *dev)
{
	TRACE_AND_STOP;
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

const char *acpi_dev_name(struct acpi_device *adev)
{
	TRACE_AND_STOP;
	return NULL;
}

int add_uevent_var(struct kobj_uevent_env *env, const char *format, ...)
{
	TRACE_AND_STOP;
	return -1;
}

void destroy_workqueue(struct workqueue_struct *wq)
{
	TRACE_AND_STOP;
}

int device_init_wakeup(struct device *dev, bool val)
{
	TRACE_AND_STOP;
	return -1;
}

void down_read(struct rw_semaphore *sem)
{
	TRACE_AND_STOP;
}

struct dma_buf *drm_gem_prime_export(struct drm_device *dev,
                                     struct drm_gem_object *obj,
                                     int flags)
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

struct drm_gem_object *drm_gem_prime_import(struct drm_device *dev,
                                            struct dma_buf *dma_buf)
{
	TRACE_AND_STOP;
	return NULL;
}

long drm_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	TRACE_AND_STOP;
	return -1;
}

int drm_open(struct inode *inode, struct file *filp)
{
	TRACE_AND_STOP;
	return -1;
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

void ndelay(unsigned long ns)
{
	TRACE_AND_STOP;
}

loff_t noop_llseek(struct file *file, loff_t offset, int whence)
{
	TRACE_AND_STOP;
	return -1;
}

int of_alias_get_id(struct device_node *np, const char *stem)
{
	TRACE_AND_STOP;
	return -ENOSYS;
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

pgprot_t pgprot_writecombine(pgprot_t prot)
{
	TRACE_AND_STOP;
	return prot;
}

void print_hex_dump(const char *level, const char *prefix_str, int prefix_type, int rowsize, int groupsize, const void *buf, size_t len, bool ascii)
{
	TRACE;
}

int PTR_ERR_OR_ZERO(__force const void *ptr)
{
	TRACE_AND_STOP;
	return -1;
}

void up_read(struct rw_semaphore *sem)
{
	TRACE_AND_STOP;
}

pgprot_t vm_get_page_prot(unsigned long vm_flags)
{
	pgprot_t prot;
	TRACE_AND_STOP;
	return prot;
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

int  ww_mutex_lock_interruptible(struct ww_mutex *lock, struct ww_acquire_ctx *ctx)
{
	TRACE_AND_STOP;
	return -1;
}

void might_lock(struct mutex *m)
{
	TRACE;
}

void write_lock(rwlock_t *l)
{
	TRACE;
}

void write_unlock(rwlock_t *l)
{
	TRACE;
}

void read_lock(rwlock_t *l)
{
	TRACE_AND_STOP;
}

void read_unlock(rwlock_t *l)
{
	TRACE_AND_STOP;
}

void write_seqlock(seqlock_t *l)
{
	TRACE;
}

void write_sequnlock(seqlock_t *l)
{
	TRACE;
}

unsigned read_seqbegin(const seqlock_t *s)
{
	TRACE;
	return 0;
}

unsigned read_seqretry(const seqlock_t *s, unsigned x)
{
	TRACE;
	return 0;
}

struct dma_fence * reservation_object_get_excl_rcu(struct reservation_object *obj)
{
	TRACE;
	return obj->fence_excl;
}

void rwlock_init(rwlock_t *rw)
{
	TRACE;
}

unsigned long vma_pages(struct vm_area_struct *p)
{
	TRACE_AND_STOP;
	return 0;
}

void call_rcu(struct rcu_head *head, void (*func)(struct rcu_head *))
{
	TRACE;

	func(head);
}

void seqlock_init (seqlock_t *s)
{
	TRACE;
}

void irq_domain_remove(struct irq_domain *d)
{
	TRACE_AND_STOP;
}

pgprot_t pgprot_decrypted(pgprot_t prot)
{
	TRACE_AND_STOP;
	return prot;
}

void dma_buf_put(struct dma_buf *buf)
{
	TRACE_AND_STOP;
}

int ___ratelimit(struct ratelimit_state *rs, const char *func)
{
	TRACE_AND_STOP;
	return 0;
}

bool _drm_lease_held(struct drm_file *f, int x)
{
	TRACE_AND_STOP;
	return false;
}

long long atomic64_add_return(long long i, atomic64_t *p)
{
	TRACE;
	p->counter += i;
	return p->counter;
}
