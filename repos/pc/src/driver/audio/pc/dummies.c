/*
 * \brief  Dummies - used but of no use
 * \author Sebastian Sumpf
 * \date   2022-05-31
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <lx_emul.h>

#include <linux/acpi.h>


void acpi_device_notify(struct device * dev)
{
	lx_emul_trace(__func__);
}


void acpi_device_notify_remove(struct device * dev)
{
	lx_emul_trace(__func__);
}


void acpi_put_table(struct acpi_table_header *table)
{
	lx_emul_trace(__func__);
}


#include <acpi/acpi_bus.h>

bool acpi_dev_present(const char * hid,const char * uid,s64 hrv)
{
	lx_emul_trace(__func__);
	return false;
}


struct acpi_device * acpi_dev_get_first_match_dev(const char * hid,const char * uid,s64 hrv)
{
	lx_emul_trace(__func__);
	return NULL;
}


#include <linux/kobject.h>

int kobject_uevent(struct kobject * kobj,enum kobject_action action)
{
	lx_emul_trace(__func__);
	return 0;
}


#include <linux/syscore_ops.h>

void register_syscore_ops(struct syscore_ops * ops)
{
	lx_emul_trace(__func__);
}


#include <linux/pci.h>

struct irq_domain * pci_host_bridge_acpi_msi_domain(struct pci_bus * bus)
{
	lx_emul_trace(__func__);
	return NULL;
}


#include <linux/task_work.h>

int task_work_add(struct task_struct * task,struct callback_head * work,enum task_work_notify_mode notify)
{
	lx_emul_trace(__func__);
	return -1;
}


extern int hda_widget_sysfs_reinit(struct hdac_device * codec,hda_nid_t start_nid,int num_nodes);
int hda_widget_sysfs_reinit(struct hdac_device * codec,hda_nid_t start_nid,int num_nodes)
{
	lx_emul_trace(__func__);
	return 0;
}


extern int hda_widget_sysfs_init(struct hdac_device * codec);
int hda_widget_sysfs_init(struct hdac_device * codec)
{
	lx_emul_trace(__func__);
	return 0;
}


extern void hda_widget_sysfs_exit(struct hdac_device * codec);
void hda_widget_sysfs_exit(struct hdac_device * codec)
{
	lx_emul_trace(__func__);
}


extern void snd_hda_sysfs_init(struct hda_codec * codec);
void snd_hda_sysfs_init(struct hda_codec * codec)
{
	lx_emul_trace(__func__);
}


extern int snd_hda_get_int_hint(struct hda_codec * codec,const char * key,int * valp);
int snd_hda_get_int_hint(struct hda_codec * codec,const char * key,int * valp)
{
	lx_emul_trace(__func__);
	return -ENOENT;
}


extern int snd_hda_get_bool_hint(struct hda_codec * codec,const char * key);
int snd_hda_get_bool_hint(struct hda_codec * codec,const char * key)
{
	lx_emul_trace(__func__);
	return -ENOENT;
}


extern void snd_hda_sysfs_clear(struct hda_codec * codec);
void snd_hda_sysfs_clear(struct hda_codec * codec)
{
	lx_emul_trace(__func__);
}



struct input_dev_poller;
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


#include <linux/proc_fs.h>

struct proc_dir_entry { unsigned dummy; };

struct proc_dir_entry * proc_create(const char * name,umode_t mode,struct proc_dir_entry * parent,const struct proc_ops * proc_ops)
{
	static struct proc_dir_entry entry;
	lx_emul_trace(__func__);
	return &entry;
}


struct proc_dir_entry * proc_mkdir(const char * name,struct proc_dir_entry * parent)
{
	static struct proc_dir_entry entry;
	lx_emul_trace(__func__);
	return &entry;
}


#include <linux/fs.h>

int alloc_chrdev_region(dev_t * dev,unsigned baseminor,unsigned count,const char * name)
{
	lx_emul_trace(__func__);
	return 0;
}


int nonseekable_open(struct inode * inode,struct file * filp)
{
	lx_emul_trace(__func__);
	return 0;
}


int stream_open(struct inode * inode,struct file * filp)
{
	lx_emul_trace(__func__);
	return 0;
}


#include <linux/leds.h>

void ledtrig_audio_set(enum led_audio type,enum led_brightness state)
{
	lx_emul_trace(__func__);
}


void led_trigger_register_simple(const char * name,struct led_trigger ** tp)
{
	lx_emul_trace(__func__);
}


void led_trigger_event(struct led_trigger * trig,enum led_brightness brightness)
{
	lx_emul_trace(__func__);
}




bool dma_can_mmap(struct device * dev)
{
	lx_emul_trace(__func__);
	return false;
}


#include <linux/pm_qos.h>

bool cpu_latency_qos_request_active(struct pm_qos_request * req)
{
	lx_emul_trace(__func__);
	return false;
}


void cpu_latency_qos_add_request(struct pm_qos_request * req,s32 value)
{
	lx_emul_trace(__func__);
}


#include <linux/pid.h>

void put_pid(struct pid * pid)
{
	lx_emul_trace(__func__);
}


#include <asm/smp.h>

struct smp_ops smp_ops = { };

DEFINE_PER_CPU_READ_MOSTLY(cpumask_var_t, cpu_sibling_map);


const struct attribute_group dev_attr_physical_location_group = {};

#include <net/net_namespace.h>

void __init net_ns_init(void)
{
	lx_emul_trace(__func__);
}


#include <linux/sysctl.h>

void __init __register_sysctl_init(const char * path, const struct ctl_table * table,
                                   const char * table_name, size_t table_size)
{
	lx_emul_trace(__func__);
}


#include <linux/sysfs.h>

int sysfs_add_file_to_group(struct kobject * kobj,const struct attribute * attr,const char * group)
{
	lx_emul_trace(__func__);
	return 0;
}


#include <linux/iommu.h>

int iommu_device_use_default_domain(struct device * dev)
{
	lx_emul_trace(__func__);
	return 0;
}


void iommu_device_unuse_default_domain(struct device * dev)
{
	lx_emul_trace(__func__);
}


#include <linux/context_tracking_irq.h>

noinstr void ct_irq_enter(void)
{
	lx_emul_trace(__func__);
}


noinstr void ct_irq_exit(void)
{
	lx_emul_trace(__func__);
}


#include <linux/dma-mapping.h>

bool __dma_need_sync(struct device * dev,dma_addr_t dma_addr)
{
	lx_emul_trace(__func__);
	return false;
}


const struct dma_map_ops *dma_ops = NULL;
const guid_t guid_null;


#include <linux/auxiliary_bus.h>

int auxiliary_device_init(struct auxiliary_device * auxdev)
{
	lx_emul_trace(__func__);
	return 0;
}


int __auxiliary_device_add(struct auxiliary_device * auxdev,const char * modname)
{
	lx_emul_trace(__func__);
	return 0;
}


#include <linux/cdev.h>

void cdev_init(struct cdev * cdev,const struct file_operations * fops)
{
	lx_emul_trace(__func__);
}


extern bool dev_add_physical_location(struct device * dev);
bool dev_add_physical_location(struct device * dev)
{
	lx_emul_trace(__func__);
	return false;
}


#include <linux/skbuff.h>

void skb_init()
{
	lx_emul_trace(__func__);
}


extern void software_node_notify_remove(struct device * dev);
void software_node_notify_remove(struct device * dev)
{
	lx_emul_trace(__func__);
}


#include <linux/property.h>

int software_node_notify(struct device * dev,unsigned long action)
{
	lx_emul_trace(__func__);
	return 0;
}


bool is_software_node(const struct fwnode_handle * fwnode)
{
	lx_emul_trace(__func__);
	return false;
}


#include <linux/pinctrl/devinfo.h>

int pinctrl_bind_pins(struct device * dev)
{
	lx_emul_trace(__func__);
	return 0;
}


#include <linux/pinctrl/devinfo.h>

int pinctrl_init_done(struct device * dev)
{
	lx_emul_trace(__func__);
	return 0;
}


#include <linux/pinctrl/consumer.h>

bool pinctrl_gpio_can_use_line(struct gpio_chip *, unsigned offset)
{
	return true;
}


#include <linux/pinctrl/consumer.h>

int pinctrl_select_state(struct pinctrl *p, struct pinctrl_state *s)
{
	return 0;
}


#include <acpi/acpixf.h>

acpi_status
acpi_remove_notify_handler(acpi_handle device,
                           u32 handler_type, acpi_notify_handler handler)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/srcu.h>

void __srcu_read_unlock(struct srcu_struct * ssp,int idx)
{
	lx_emul_trace(__func__);
}


#include <acpi/nhlt.h>

acpi_status acpi_nhlt_get_gbl_table(void)
{
	lx_emul_trace(__func__);
	return 0;
}


void acpi_nhlt_put_gbl_table(void)
{
	lx_emul_trace(__func__);
}


struct acpi_nhlt_endpoint * acpi_nhlt_find_endpoint(int link_type,int dev_type,int dir,int bus_id)
{
	lx_emul_trace(__func__);
	return (struct acpi_nhlt_endpoint *)0xdeade01;
}


#include <video/nomodeset.h>

bool video_firmware_drivers_only(void)
{
	lx_emul_trace(__func__);
	return true;
}


#include <sound/soc-acpi-intel-ssp-common.h>

enum snd_soc_acpi_intel_codec snd_soc_acpi_intel_detect_amp_type(struct device * dev)
{
	lx_emul_trace(__func__);
	return CODEC_NONE;
}


enum snd_soc_acpi_intel_codec snd_soc_acpi_intel_detect_codec_type(struct device * dev)
{
	lx_emul_trace(__func__);
	return CODEC_NONE;
}

#if LINUX_VERSION_CODE > KERNEL_VERSION(6,16,0)
int pcim_request_all_regions(struct pci_dev *pdev, const char *name)
{
	return 0;
}
#endif
