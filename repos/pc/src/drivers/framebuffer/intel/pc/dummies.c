/*
 * \brief  Dummy definitions of Linux Kernel functions - handled manually
 * \author Alexander Boettcher
 * \date   2022-01-21
 */

/*
 * Copyright (C) 2021-2024 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <lx_emul/debug.h>

#include "i915_drv.h"

#include <acpi/video.h>
#include <linux/clocksource.h>
#include <linux/cpuhotplug.h>
#include <linux/kernel_stat.h>
#include <linux/kernfs.h>
#include <linux/kobject.h>
#include <linux/nls.h>
#include <linux/property.h>
#include <linux/random.h>
#include <linux/rcupdate.h>
#include <linux/sched/loadavg.h>
#include <linux/sched/signal.h>
#include <linux/syscore_ops.h>
#include <linux/timekeeper_internal.h>


void register_syscore_ops(struct syscore_ops * ops)
{
	lx_emul_trace(__func__);
}


void unregister_shrinker(struct shrinker * shrinker)
{
	lx_emul_trace(__func__);
}


void unregister_handler_proc(unsigned int irq,struct irqaction * action)
{
	lx_emul_trace(__func__);
}


int drm_aperture_remove_conflicting_pci_framebuffers(struct pci_dev * pdev,
                                                     const char * name)
{
	lx_emul_trace(__func__);
	return 0;
}


#include <linux/firmware.h>

void release_firmware(const struct firmware * fw)
{
	lx_emul_trace(__func__);
}


#include <linux/firmware.h>

int request_firmware(const struct firmware ** firmware_p,const char * name,struct device * device)
{
	lx_emul_trace(__func__);
	return -1;
}


#include <linux/firmware.h>

int request_firmware_direct(const struct firmware ** firmware_p,const char * name,struct device * device)
{
	lx_emul_trace(__func__);
	return -1;
}


int ___ratelimit(struct ratelimit_state * rs, const char * func)
{
	/*
	 * from lib/ratelimit.c:
	 * " 0 means callbacks will be suppressed.
	 *   1 means go ahead and do it. "
	 */
	lx_emul_trace(__func__);
	return 1;
}


#include <net/net_namespace.h>

struct net init_net;


#include <net/net_namespace.h>

int register_pernet_subsys(struct pernet_operations * ops)
{
	lx_emul_trace(__func__);
	return 0;
}


int set_pages_uc(struct page * page,int numpages)
{
	lx_emul_trace(__func__);
	return 0;
}



bool irq_work_queue(struct irq_work * work)
{
	lx_emul_trace(__func__);
	return false;
}


void acpi_video_unregister(void)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/acpi.h>

acpi_status acpi_remove_address_space_handler(acpi_handle device,
                                              acpi_adr_space_type space_id,
                                              acpi_adr_space_handler handler)
{
	lx_emul_trace_and_stop(__func__);
}


int acpi_video_register(void)
{
	lx_emul_trace(__func__);
	return 0;
}


acpi_status acpi_get_name(acpi_handle object, u32 name_type,
                          struct acpi_buffer *ret_path_ptr)
{
	lx_emul_trace_and_stop(__func__);
}


void cpu_latency_qos_add_request(struct pm_qos_request *req, s32 value)
{
	lx_emul_trace(__func__);
}


void cpu_latency_qos_update_request(struct pm_qos_request *req, s32 new_value)
{
	lx_emul_trace(__func__);
}


bool cpu_latency_qos_request_active(struct pm_qos_request *req)
{
	lx_emul_trace(__func__);
	return false;
}


void cpu_latency_qos_remove_request(struct pm_qos_request *req)
{
	lx_emul_trace(__func__);
}


enum acpi_backlight_type acpi_video_get_backlight_type(void)
{
	lx_emul_trace_and_stop(__func__);
}


int pnp_range_reserved(resource_size_t start, resource_size_t end)
{
	lx_emul_trace_and_stop(__func__);
}


int __get_user_nocheck_1(void)
{
	lx_emul_trace_and_stop(__func__);
}


unsigned long
raw_copy_from_user(void *to, const void __user * from, unsigned long n)
{
	lx_emul_trace_and_stop(__func__);
}


int register_acpi_notifier(struct notifier_block * nb)
{
	lx_emul_trace(__func__);
	return 0;
}


int unregister_acpi_notifier(struct notifier_block * nb)
{
	lx_emul_trace(__func__);
	return 0;
}


int acpi_reconfig_notifier_register(struct notifier_block * nb)
{
	lx_emul_trace(__func__);
	return 0;
}


/* 
 * i915 specific dummies follow below.
 */

void intel_engines_add_sysfs(struct drm_i915_private * i915)
{
	lx_emul_trace(__func__);
}


int i915_pmu_init(void)
{
	lx_emul_trace(__func__);
	return 0;
}


void i915_pmu_register(struct drm_i915_private * i915)
{
	lx_emul_trace(__func__);
}


bool intel_vgpu_active(struct drm_i915_private * dev_priv)
{
	lx_emul_trace(__func__);
	return false;
}


int intel_gt_init_mmio(struct intel_gt * gt)
{
	lx_emul_trace(__func__);
	return 0;
}


int __intel_gt_reset(struct intel_gt * gt, intel_engine_mask_t engine_mask)
{
	lx_emul_trace(__func__);
	return 0;
}


void i915_perf_init(struct drm_i915_private * i915)
{
	lx_emul_trace(__func__);
}


void gen8_gt_irq_handler(struct intel_gt * gt,u32 master_ctl)
{
	lx_emul_trace(__func__);
}


void gen11_gt_irq_reset(struct intel_gt * gt)
{
	lx_emul_trace(__func__);
}


void gen11_gt_irq_handler(struct intel_gt * gt, const u32 master_ctl)
{
	lx_emul_trace(__func__);
}


void gen11_gt_irq_postinstall(struct intel_gt * gt)
{
	lx_emul_trace(__func__);
}


void gen8_gt_irq_reset(struct intel_gt * gt)
{
	lx_emul_trace(__func__);
}


void gen8_gt_irq_postinstall(struct intel_gt * gt)
{
	lx_emul_trace(__func__);
}


void gen5_gt_irq_reset(struct intel_gt * gt)
{
	lx_emul_trace(__func__);
}


void gen5_gt_irq_postinstall(struct intel_gt * gt)
{
	lx_emul_trace(__func__);
}


void intel_vgpu_register(struct drm_i915_private * i915)
{
	lx_emul_trace(__func__);
}


void i915_perf_register(struct drm_i915_private * i915)
{
	lx_emul_trace(__func__);
}


void intel_gt_driver_register(struct intel_gt * gt)
{
	lx_emul_trace(__func__);
}


void i915_perf_sysctl_register(void)
{
	lx_emul_trace(__func__);
}


int i915_gemfs_init(struct drm_i915_private * i915)
{
	lx_emul_trace(__func__);
	return 0;
}


int intel_vgt_balloon(struct i915_ggtt * ggtt)
{
	lx_emul_trace(__func__);
	return 0;
}


void intel_gt_info_print(const struct intel_gt_info * info,struct drm_printer * p)
{
	lx_emul_trace(__func__);
}


void i915_gem_object_release_mmap_offset(struct drm_i915_gem_object * obj)
{
	lx_emul_trace(__func__);
}


#include <asm/smp.h>

struct smp_ops smp_ops = { };
EXPORT_SYMBOL_GPL(smp_ops);


int wbinvd_on_all_cpus(void)
{
	lx_emul_trace(__func__);
	return 0;
}


#include <linux/pci.h>

int pci_bus_read_config_byte(struct pci_bus *bus, unsigned int devfn,
                             int where, u8 *val)
{
	lx_emul_trace_and_stop(__func__);
}


int pci_bus_read_config_word(struct pci_bus *bus, unsigned int devfn,
                             int where, u16 *val)
{
	lx_emul_trace_and_stop(__func__);
}


int pci_bus_write_config_byte(struct pci_bus *bus, unsigned int devfn,
                              int where, u8 val)
{
	lx_emul_trace_and_stop(__func__);
}


int pci_enable_msi(struct pci_dev *dev)
{
	lx_emul_trace(__func__);
	return -ENOSYS;
}


void pci_disable_msi(struct pci_dev *dev)
{
	lx_emul_trace(__func__);
}


#include <asm/smp.h>

DEFINE_PER_CPU_READ_MOSTLY(cpumask_var_t, cpu_sibling_map);


const struct attribute_group dev_attr_physical_location_group = {};


typedef u32 acpi_status;
struct acpi_resource;

extern acpi_status acpi_buffer_to_resource(u8 * aml_buffer,u16 aml_buffer_length,struct acpi_resource ** resource_ptr);
acpi_status acpi_buffer_to_resource(u8 * aml_buffer,u16 aml_buffer_length,struct acpi_resource ** resource_ptr)
{
	lx_emul_trace_and_stop(__func__);
}


struct acpi_device;

extern int acpi_bus_get_status(struct acpi_device * device);
int acpi_bus_get_status(struct acpi_device * device)
{
	lx_emul_trace_and_stop(__func__);
}


void acpi_device_notify(struct device * dev)
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

void __fix_address kfree_skb_reason(struct sk_buff * skb,enum skb_drop_reason reason)
{
	lx_emul_trace(__func__);
}


#include <linux/sysctl.h>

struct ctl_table_header * register_sysctl(const char * path,struct ctl_table * table)
{
	lx_emul_trace(__func__);
	return NULL;
}


#include <acpi/acpi_bus.h>

int register_acpi_bus_type(struct acpi_bus_type * type)
{
	lx_emul_trace(__func__);
	return 0;
}


#include <linux/sysctl.h>

void __init __register_sysctl_init(const char * path,struct ctl_table * table,const char * table_name)
{
	lx_emul_trace(__func__);
}


#include <linux/sysfs.h>

int sysfs_add_file_to_group(struct kobject * kobj,const struct attribute * attr,const char * group)
{
	lx_emul_trace(__func__);
	return 0;
}


#include <linux/random.h>

u16 get_random_u16(void)
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

int acpi_bus_get_private_data(acpi_handle handle,void ** data)
{
	lx_emul_trace_and_stop(__func__);
}


#include <acpi/acpi_bus.h>

void acpi_dev_clear_dependencies(struct acpi_device * supplier)
{
	lx_emul_trace_and_stop(__func__);
}


extern acpi_status acpi_install_address_space_handler(acpi_handle device,acpi_adr_space_type space_id,acpi_adr_space_handler handler,acpi_adr_space_setup setup,void * context);
acpi_status acpi_install_address_space_handler(acpi_handle device,acpi_adr_space_type space_id,acpi_adr_space_handler handler,acpi_adr_space_setup setup,void * context)
{
	lx_emul_trace_and_stop(__func__);
}


#include <drm/drm_drv.h>

bool drm_firmware_drivers_only(void)
{
	lx_emul_trace(__func__);
	return 0;
}


extern int __init i915_context_module_init(void);
int __init i915_context_module_init(void)
{
	lx_emul_trace(__func__);
	return 0;
}


extern int __init i915_gem_context_module_init(void);
int __init i915_gem_context_module_init(void)
{
	lx_emul_trace(__func__);
	return 0;
}


extern int __init i915_request_module_init(void);
int __init i915_request_module_init(void)
{
	lx_emul_trace(__func__);
	return 0;
}


extern int __init i915_scheduler_module_init(void);
int __init i915_scheduler_module_init(void)
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


#include <linux/iommu.h>

void iommu_device_unuse_default_domain(struct device * dev)
{
	lx_emul_trace(__func__);
}


extern struct ttm_device_funcs * i915_ttm_driver(void);
struct ttm_device_funcs * i915_ttm_driver(void)
{
	lx_emul_trace(__func__);
	return NULL;
}


#include <linux/shrinker.h>

int register_shrinker(struct shrinker * shrinker,const char * fmt,...)
{
	lx_emul_trace(__func__);
	return 0;
}


extern void intel_guc_init_early(struct intel_guc * guc);
void intel_guc_init_early(struct intel_guc * guc)
{
	lx_emul_trace(__func__);
}


extern void intel_huc_init_early(struct intel_huc * huc);
void intel_huc_init_early(struct intel_huc * huc)
{
	lx_emul_trace(__func__);
}


extern int intel_gt_tiles_init(struct drm_i915_private * i915);
int intel_gt_tiles_init(struct drm_i915_private * i915)
{
	lx_emul_trace(__func__);
	return 0;
}


void rt_mutex_setprio(struct task_struct *p, struct task_struct *pi_task)
{
	lx_emul_trace(__func__);
}


#include <linux/context_tracking_irq.h>

noinstr void ct_irq_enter(void)
{
	lx_emul_trace(__func__);
}


#include <linux/context_tracking_irq.h>

noinstr void ct_irq_exit(void)
{
	lx_emul_trace(__func__);
}


#include <acpi/video.h>

bool acpi_video_backlight_use_native(void)
{
	lx_emul_trace(__func__);
	return 1;
}


#include <net/net_namespace.h>

void net_ns_init(void)
{
	lx_emul_trace(__func__);
}


#include <asm/iomap.h>

void iomap_free(resource_size_t base, unsigned long size)
{
	lx_emul_trace_and_stop(__func__);
}


#include <linux/slab.h>

void *kmalloc_large_node(size_t size, gfp_t flags, int node)
{
	lx_emul_trace_and_stop(__func__);
}


void * high_memory;


void intel_dsb_prepare(struct intel_crtc_state *crtc_state)
{
	lx_emul_trace(__func__);
}


void intel_dsb_cleanup(struct intel_crtc_state *crtc_state)
{
	lx_emul_trace(__func__);
}


void intel_dsb_commit(const struct intel_crtc_state *crtc_state)
{
	lx_emul_trace(__func__);
}


void intel_dsb_reg_write(const struct intel_crtc_state *crtc_state,
                         i915_reg_t reg, u32 val)
{
	lx_emul_trace(__func__);
}


void intel_dsb_indexed_reg_write(const struct intel_crtc_state *crtc_state,
                                 i915_reg_t reg, u32 val)
{
	lx_emul_trace(__func__);
}


void acpi_device_notify_remove(struct device * dev)
{
	lx_emul_trace(__func__);
}


void software_node_notify_remove(struct device * dev)
{
	lx_emul_trace(__func__);
}


#include <linux/cdev.h>

void cdev_init(struct cdev * cdev,const struct file_operations * fops)
{
	lx_emul_trace(__func__);
}



#include <linux/skbuff.h>

void skb_init()
{
	lx_emul_trace(__func__);
}


#include <linux/fs.h>

int __register_chrdev(unsigned int major,unsigned int baseminor,unsigned int count,const char * name,const struct file_operations * fops)
{
	lx_emul_trace(__func__);
	return 0;
}


#include <linux/property.h>

int software_node_notify(struct device * dev,unsigned long action)
{
	lx_emul_trace(__func__);
	return 0;
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

struct pinctrl * devm_pinctrl_get(struct device * dev)
{
	lx_emul_trace(__func__);
	return (struct pinctrl*)ERR_PTR(-ENODEV);
}


#include <linux/pinctrl/consumer.h>

void devm_pinctrl_put(struct pinctrl * p)
{
	lx_emul_trace(__func__);
}


void check_move_unevictable_pages(struct pagevec * pvec)
{
	lx_emul_trace(__func__);
}


void intel_gt_flush_ggtt_writes(struct intel_gt * gt)
{
	lx_emul_trace(__func__);
}


void intel_gt_invalidate_tlb(struct intel_gt * gt,u32 seqno)
{
	lx_emul_trace(__func__);
}


void mark_page_accessed(struct page * page)
{
	lx_emul_trace(__func__);
}
