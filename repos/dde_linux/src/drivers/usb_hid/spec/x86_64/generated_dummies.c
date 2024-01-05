/*
 * \brief  Dummy definitions of Linux Kernel functions
 * \author Automatically generated file - do no edit
 * \date   2024-02-05
 */

#include <lx_emul.h>


#include <linux/ratelimit_types.h>

int ___ratelimit(struct ratelimit_state * rs,const char * func)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/cpumask.h>

struct cpumask __cpu_active_mask;


#include <linux/printk.h>

int __printk_ratelimit(const char * func)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/printk.h>

void __printk_safe_enter(void)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/printk.h>

void __printk_safe_exit(void)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/sched/task.h>

void __put_task_struct(struct task_struct * tsk)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/uaccess.h>

unsigned long _copy_to_user(void __user * to,const void * from,unsigned long n)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/printk.h>

int _printk_deferred(const char * fmt,...)
{
	lx_emul_trace_and_stop(__func__);
}


extern void ack_bad_irq(unsigned int irq);
void ack_bad_irq(unsigned int irq)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/random.h>

void add_device_randomness(const void * buf,size_t len)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/kobject.h>

int add_uevent_var(struct kobj_uevent_env * env,const char * format,...)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/async.h>

async_cookie_t async_schedule_node(async_func_t func,void * data,int node)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/async.h>

void async_synchronize_full(void)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/arch_topology.h>

const struct cpumask * cpu_clustergroup_mask(int cpu)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/leds.h>

int devm_led_classdev_register_ext(struct device * parent,struct led_classdev * led_cdev,struct led_init_data * init_data)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/leds.h>

int devm_led_trigger_register(struct device * dev,struct led_trigger * trig)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/power_supply.h>

struct power_supply * __must_check devm_power_supply_register(struct device * parent,const struct power_supply_desc * desc,const struct power_supply_config * cfg)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/printk.h>

asmlinkage __visible void dump_stack(void)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/rcuwait.h>

void finish_rcuwait(struct rcuwait * w)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/uuid.h>

const u8 guid_index[16] = {};


#include <linux/init.h>

bool initcall_debug;


extern void input_dev_poller_finalize(struct input_dev_poller * poller);
void input_dev_poller_finalize(struct input_dev_poller * poller)
{
	lx_emul_trace_and_stop(__func__);
}


extern void input_dev_poller_start(struct input_dev_poller * poller);
void input_dev_poller_start(struct input_dev_poller * poller)
{
	lx_emul_trace_and_stop(__func__);
}


extern void input_dev_poller_stop(struct input_dev_poller * poller);
void input_dev_poller_stop(struct input_dev_poller * poller)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/input.h>

int input_ff_create_memless(struct input_dev * dev,void * data,int (* play_effect)(struct input_dev *,void *,struct ff_effect *))
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/input.h>

int input_ff_event(struct input_dev * dev,unsigned int type,unsigned int code,int value)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/math.h>

unsigned long int_sqrt(unsigned long x)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/sched.h>

void __sched io_schedule(void)
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


#include <linux/swiotlb.h>

struct io_tlb_mem io_tlb_default_mem;


#include <linux/interrupt.h>

int irq_can_set_affinity(unsigned int irq)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/interrupt.h>

int irq_set_affinity(unsigned int irq,const struct cpumask * cpumask)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/irq_work.h>

void irq_work_tick(void)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/slab.h>

void * kmem_cache_alloc_lru(struct kmem_cache * cachep,struct list_lru * lru,gfp_t flags)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/kobject.h>

int kobject_synth_uevent(struct kobject * kobj,const char * buf,size_t count)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/kobject.h>

int kobject_uevent_env(struct kobject * kobj,enum kobject_action action,char * envp_ext[])
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/leds.h>

void led_trigger_event(struct led_trigger * trig,enum led_brightness brightness)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/moduleparam.h>

int param_set_copystring(const char * val,const struct kernel_param * kp)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/power_supply.h>

void power_supply_changed(struct power_supply * psy)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/power_supply.h>

void * power_supply_get_drvdata(struct power_supply * psy)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/power_supply.h>

int power_supply_powers(struct power_supply * psy,struct device * dev)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/smp.h>

void smp_call_function_many(const struct cpumask * mask,smp_call_func_t func,void * info,bool wait)
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


#include <linux/timerqueue.h>

bool timerqueue_add(struct timerqueue_head * head,struct timerqueue_node * node)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/timerqueue.h>

bool timerqueue_del(struct timerqueue_head * head,struct timerqueue_node * node)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/timerqueue.h>

struct timerqueue_node * timerqueue_iterate_next(struct timerqueue_node * node)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/usb.h>

void usb_block_urb(struct urb * urb)
{
	lx_emul_trace_and_stop(__func__);
}


extern void usb_devio_cleanup(void);
void usb_devio_cleanup(void)
{
	lx_emul_trace_and_stop(__func__);
}


extern void usb_disable_endpoint(struct usb_device * dev,unsigned int epaddr,bool reset_hardware);
void usb_disable_endpoint(struct usb_device * dev,unsigned int epaddr,bool reset_hardware)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/usb.h>

int usb_free_streams(struct usb_interface * interface,struct usb_host_endpoint ** eps,unsigned int num_eps,gfp_t mem_flags)
{
	lx_emul_trace_and_stop(__func__);
}


extern int usb_get_device_descriptor(struct usb_device * dev,unsigned int size);
int usb_get_device_descriptor(struct usb_device * dev,unsigned int size)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/usb.h>

int usb_get_status(struct usb_device * dev,int recip,int type,int target,void * data)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/usb/hcd.h>

int usb_hcd_alloc_bandwidth(struct usb_device * udev,struct usb_host_config * new_config,struct usb_host_interface * cur_alt,struct usb_host_interface * new_alt)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/usb/hcd.h>

int usb_hcd_find_raw_port_number(struct usb_hcd * hcd,int port1)
{
	lx_emul_trace_and_stop(__func__);
}


extern int usb_hub_create_port_device(struct usb_hub * hub,int port1);
int usb_hub_create_port_device(struct usb_hub * hub,int port1)
{
	lx_emul_trace_and_stop(__func__);
}


extern void usb_hub_remove_port_device(struct usb_hub * hub,int port1);
void usb_hub_remove_port_device(struct usb_hub * hub,int port1)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/usb.h>

int usb_interrupt_msg(struct usb_device * usb_dev,unsigned int pipe,void * data,int len,int * actual_length,int timeout)
{
	lx_emul_trace_and_stop(__func__);
}


extern void usb_major_cleanup(void);
void usb_major_cleanup(void)
{
	lx_emul_trace_and_stop(__func__);
}


extern int usb_set_isoch_delay(struct usb_device * dev);
int usb_set_isoch_delay(struct usb_device * dev)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/usb/ch9.h>

const char * usb_speed_string(enum usb_device_speed speed)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/usb.h>

int usb_unlink_urb(struct urb * urb)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/usb.h>

void usb_unpoison_urb(struct urb * urb)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/uuid.h>

const u8 uuid_index[16] = {};


#include <linux/sched/wake_q.h>

void wake_q_add_safe(struct wake_q_head * head,struct task_struct * task)
{
	lx_emul_trace_and_stop(__func__);
}

