#include <lx_emul.h>

#if 0
#define TRACE \
	do { \
		lx_printf("%s not implemented\n", __func__); \
	} while (0)
#else
#define TRACE do { ; } while (0)
#endif

#define TRACE_AND_STOP \
	do { \
		lx_printf("%s not implemented\n", __func__); \
		BUG(); \
	} while (0)

int bitmap_subset(const unsigned long *src1, const unsigned long *src2, int nbits)
{
	TRACE;
	return 1;
}

int bitmap_weight(const unsigned long *src, unsigned int nbits)
{
	TRACE;
	return 0;
}

int bus_for_each_dev(struct bus_type *bus, struct device *start, void *data, int (*fn)(struct device *dev, void *data))
{
	TRACE_AND_STOP;
	return -1;
}

int bus_for_each_drv(struct bus_type *bus, struct device_driver *start, void *data, int (*fn)(struct device_driver *, void *))
{
	TRACE;
	return 0;
}

int  bus_register(struct bus_type *bus)
{
	TRACE;
	return 0;
}

void bus_unregister(struct bus_type *bus)
{
	TRACE_AND_STOP;
}

int cdev_device_add(struct cdev *cdev, struct device *dev)
{
	TRACE;
	return 0;
}

void cdev_device_del(struct cdev *cdev, struct device *dev)
{
	TRACE;
}

int class_register(struct class *cls)
{
	TRACE;
	return 0;
}

void class_unregister(struct class *cls)
{
	TRACE_AND_STOP;
}

unsigned long clear_user(void *to, unsigned long n)
{
	TRACE_AND_STOP;
	return -1;
}

long copy_from_user(void *to, const void *from, unsigned long n)
{
	TRACE_AND_STOP;
	return -1;
}

size_t copy_to_user(void *dst, void const *src, size_t len)
{
	TRACE_AND_STOP;
	return -1;
}

int  device_create_file(struct device *device, const struct device_attribute *entry)
{
	TRACE;
	return 0;
}

void device_enable_async_suspend(struct device *dev)
{
	TRACE;
}

void device_initialize(struct device *dev)
{
	TRACE;
}

void device_lock(struct device *dev)
{
	TRACE_AND_STOP;
}

void device_release_driver(struct device *dev)
{
	TRACE_AND_STOP;
}

void device_remove_file(struct device *dev, const struct device_attribute *attr)
{
	TRACE;
}

int  device_set_wakeup_enable(struct device *dev, bool enable)
{
	TRACE;
	return 0;
}

void device_unlock(struct device *dev)
{
	TRACE_AND_STOP;
}

int devm_add_action(struct device *dev, void (*action)(void *), void *data)
{
	TRACE_AND_STOP;
	return -1;
}

int devm_add_action_or_reset(struct device *dev, void (*action)(void *), void *data)
{
	TRACE_AND_STOP;
	return -1;
}

char *devm_kasprintf(struct device *dev, gfp_t gfp, const char *fmt, ...)
{
	TRACE_AND_STOP;
	return NULL;
}

int devm_led_classdev_register(struct device *parent, struct led_classdev *led_cdev)
{
	TRACE_AND_STOP;
	return -1;
}

int devm_led_trigger_register(struct device *dev, struct led_trigger *trigger)
{
	TRACE_AND_STOP;
	return -1;
}

void  devres_add(struct device *dev, void *res)
{
	TRACE_AND_STOP;
}

void *devres_alloc(dr_release_t release, size_t size, gfp_t gfp)
{
	TRACE_AND_STOP;
	return NULL;
}

void devres_close_group(struct device *dev, void *id)
{
	TRACE_AND_STOP;
}

int   devres_destroy(struct device *dev, dr_release_t release,  dr_match_t match, void *match_data)
{
	TRACE_AND_STOP;
	return -1;
}

void * devres_open_group(struct device *dev, void *id, gfp_t gfp)
{
	TRACE_AND_STOP;
	return NULL;
}

int devres_release_group(struct device *dev, void *id)
{
	TRACE_AND_STOP;
	return -1;
}

int dev_set_name(struct device *dev, const char *name, ...)
{
	TRACE;
	return 0;
}

int  down_interruptible(struct semaphore *sem)
{
	TRACE;
	return 0;
}

int  down_trylock(struct semaphore *sem)
{
	TRACE;
	return 0;
}

int  driver_attach(struct device_driver *drv)
{
	TRACE_AND_STOP;
	return -1;
}

int fasync_helper(int a1, struct file * f, int a2, struct fasync_struct ** fas)
{
	TRACE_AND_STOP;
	return -1;
}

u64 get_unaligned_le64(const void *p)
{
	TRACE_AND_STOP;
	return -1;
}

int ida_simple_get(struct ida *ida, unsigned int start, unsigned int end, gfp_t gfp_mask)
{
	TRACE;
	return 0;
}

void ida_simple_remove(struct ida *ida, unsigned int id)
{
	TRACE;
}

struct input_event;
int input_event_from_user(const char __user *buffer, struct input_event *event)
{
	TRACE_AND_STOP;
	return -1;
}

int input_event_to_user(char __user *buffer, const struct input_event *event)
{
	TRACE_AND_STOP;
	return -1;
}

struct input_dev;
void input_ff_destroy(struct input_dev *dev)
{
	TRACE;
}

struct ff_effect;
int input_ff_effect_from_user(const char __user *buffer, size_t size, struct ff_effect *effect)
{
	TRACE_AND_STOP;
	return -1;
}

int input_ff_erase(struct input_dev *dev, int effect_id, struct file *file)
{
	TRACE_AND_STOP;
	return -1;
}

int input_ff_event(struct input_dev *dev, unsigned int type, unsigned int code, int value)
{
	TRACE_AND_STOP;
	return -1;
}

int input_ff_upload(struct input_dev *dev, struct ff_effect *effect, struct file *file)
{
	TRACE_AND_STOP;
	return -1;
}

unsigned long int_sqrt(unsigned long x)
{
	TRACE_AND_STOP;
	return -1;
}

unsigned int jiffies_to_usecs(const unsigned long j)
{
	TRACE;
	return 0;
}

void kill_fasync(struct fasync_struct **a0, int a1, int a2)
{
	TRACE_AND_STOP;
}

struct kobject * kobject_create_and_add(const char * a0, struct kobject * a1)
{
	TRACE_AND_STOP;
	return NULL;
}

char *kobject_get_path(struct kobject *kobj, gfp_t gfp_mask)
{
	TRACE;
	return NULL;
}

void  kobject_put(struct kobject * a0)
{
	TRACE_AND_STOP;
}

struct device *kobj_to_dev(struct kobject *kobj)
{
	TRACE_AND_STOP;
	return NULL;
}

int kstrtou8(const char *s, unsigned int base, u8 *res)
{
	TRACE_AND_STOP;
	return -1;
}

int kstrtouint(const char *s, unsigned int base, unsigned int *res)
{
	TRACE_AND_STOP;
	return -1;
}

int kstrtoul(const char *s, unsigned int base, unsigned long *res)
{
	TRACE_AND_STOP;
	return -1;
}

ktime_t ktime_get_boottime(void)
{
	TRACE_AND_STOP;
	return -1;
}

ktime_t ktime_get_real(void)
{
	TRACE_AND_STOP;
	return -1;
}

ktime_t ktime_mono_to_any(ktime_t tmono, enum tk_offsets offs)
{
	TRACE_AND_STOP;
	return -1;
}

ktime_t ktime_mono_to_real(ktime_t mono)
{
	TRACE_AND_STOP;
	return -1;
}

struct timespec64 ktime_to_timespec64(const s64 nsec)
{
	struct timespec64 ret;
	TRACE_AND_STOP;
	return ret;
}

void kvfree(const void *addr)
{
	TRACE_AND_STOP;
}

int    memcmp(const void * a0, const void * a1, size_t a2)
{
	TRACE_AND_STOP;
	return -1;
}

void __module_get(struct module *module)
{
	TRACE;
}

void module_put(struct module * module)
{
	TRACE;
}

loff_t no_llseek(struct file *file, loff_t offset, int whence)
{
	TRACE_AND_STOP;
	return -1;
}

int nonseekable_open(struct inode *inode, struct file *filp)
{
	TRACE_AND_STOP;
	return -1;
}

void *power_supply_get_drvdata(struct power_supply *psy)
{
	TRACE_AND_STOP;
	return NULL;
}

int power_supply_powers(struct power_supply *psy, struct device *dev)
{
	TRACE_AND_STOP;
	return -1;
}

int  register_chrdev_region(dev_t a0, unsigned a1, const char * a2)
{
	TRACE;
	return 0;
}

int scnprintf(char *buf, size_t size, const char *fmt, ...)
{
	TRACE_AND_STOP;
	return -1;
}

void sema_init(struct semaphore *sem, int val)
{
	TRACE;
}

void spin_lock(spinlock_t *lock)
{
	TRACE;
}

void spin_lock_irq(spinlock_t *lock)
{
	TRACE;
}

void spin_unlock_irq(spinlock_t *lock)
{
	TRACE;
}

int sscanf(const char * a0, const char * a1, ...)
{
	TRACE_AND_STOP;
	return -1;
}

int    strcmp(const char *s1, const char *s2)
{
	TRACE_AND_STOP;
	return -1;
}

size_t strlcpy(char *dest, const char *src, size_t size)
{
	TRACE_AND_STOP;
	return -1;
}

int    strncmp(const char *cs, const char *ct, size_t count)
{
	TRACE_AND_STOP;
	return -1;
}

char  *strrchr(const char * a0, int a1)
{
	TRACE_AND_STOP;
	return NULL;
}

char *strstr(const char * a0, const char * a1)
{
	TRACE_AND_STOP;
	return NULL;
}

int sysfs_create_files(struct kobject *kobj, const struct attribute **ptr)
{
	TRACE_AND_STOP;
	return -1;
}

void sysfs_remove_group(struct kobject *kobj, const struct attribute_group *grp)
{
	TRACE_AND_STOP;
}

struct urb;
void usb_block_urb(struct urb *urb)
{
	TRACE_AND_STOP;
}

struct usb_device;
int usb_clear_halt(struct usb_device *dev, int pipe)
{
	TRACE;
	return -1;
}

int usb_interrupt_msg(struct usb_device *usb_dev, unsigned int pipe, void *data, int len, int *actual_length, int timeout)
{
	TRACE_AND_STOP;
	return -1;
}

struct usb_interface;
void usb_queue_reset_device(struct usb_interface *dev)
{
	TRACE;
}

int usb_string(struct usb_device *dev, int index, char *buf, size_t size)
{
	TRACE;
	return -1;
}

void usb_unpoison_urb(struct urb *urb)
{
	TRACE_AND_STOP;
}

struct __kfifo;
int __kfifo_alloc(struct __kfifo *fifo, unsigned int size, size_t esize, gfp_t gfp_mask)
{
	TRACE_AND_STOP;
	return -1;
}

void __kfifo_free(struct __kfifo *fifo)
{
	TRACE_AND_STOP;
}

unsigned int __kfifo_in(struct __kfifo *fifo, const void *buf, unsigned int len)
{
	TRACE_AND_STOP;
	return 0;
}

unsigned int __kfifo_in_r(struct __kfifo *fifo, const void *buf, unsigned int len, size_t recsize)
{
	TRACE_AND_STOP;
	return 0;
}

unsigned int __kfifo_out(struct __kfifo *fifo, void *buf, unsigned int len)
{
	TRACE_AND_STOP;
	return 0;
}

unsigned int __kfifo_out_r(struct __kfifo *fifo, void *buf, unsigned int len, size_t recsize)
{
	TRACE_AND_STOP;
	return 0;
}

void __kfifo_skip_r(struct __kfifo *fifo, size_t recsize)
{
	TRACE_AND_STOP;
}

unsigned int __kfifo_max_r(unsigned int len, size_t recsize)
{
	TRACE_AND_STOP;
}

void usb_kill_urb(struct urb *urb)
{
	TRACE;
}

int usb_unlink_urb(struct urb *urb)
{
	TRACE_AND_STOP;
	return -1;
}

int add_uevent_var(struct kobj_uevent_env *env, const char *format, ...)
{
	TRACE_AND_STOP;
	return -1;
}

struct power_supply;
struct power_supply_desc;
struct power_supply_config;
struct power_supply * devm_power_supply_register(struct device *parent, const struct power_supply_desc *desc, const struct power_supply_config *cfg)
{
	TRACE_AND_STOP;
	return NULL;
}

void devres_free(void *res)
{
	TRACE_AND_STOP;
}

char *kasprintf(gfp_t gfp, const char *fmt, ...)
{
	TRACE_AND_STOP;
	return NULL;
}

void poll_wait(struct file *a0, wait_queue_head_t *a1, poll_table *a2)
{
	TRACE_AND_STOP;
}

void power_supply_changed(struct power_supply *psy)
{
	TRACE_AND_STOP;
}

int  sysfs_create_group(struct kobject *kobj, const struct attribute_group *grp)
{
	TRACE;
	return 0;
}

void up(struct semaphore *sem)
{
	TRACE;
}

bool usb_device_is_owned(struct usb_device *udev)
{
	TRACE;
	return false;
}
