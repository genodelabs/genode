/*
 * \brief  Platform interface of DRM code
 * \author Norman Feske
 * \date   2015-08-19
 */

#ifndef _DRMP_H_
#define _DRMP_H_

#ifdef __cplusplus
extern "C" {
#endif

/* local includes */
#include <lx_emul.h>

/*
 * Unfortunately, DRM headers use certain C++ keywords as variable names.
 * To enable the inclusion of 'drmP.h' from C++ source codes, we have to
 * rename these identifiers.
 */
#ifdef __cplusplus
#define new     _new
#define virtual _virtual
#define private _private
#endif /* __cplusplus */

#include <uapi/drm/drm.h>
#include <uapi/drm/i915_drm.h>
#include <uapi/drm/drm_fourcc.h>
#include <uapi/drm/drm_mode.h>

#include <drm/i915_pciids.h>
#include <drm/drm_mm.h>
#include <drm/drm_crtc.h>
#include <drm/drm_crtc_helper.h>

#ifdef __cplusplus
#undef virtual
#undef new
#undef private
#endif /* __cplusplus */

extern unsigned int drm_debug;


/*******************
 ** DRM constants **
 *******************/

enum {
	DRM_MINOR_CONTROL    = 2,
//	DRM_MINOR_RENDER     = 3,
//	DRM_MAGIC_HASH_ORDER = 4,
};

enum {
	DRM_AUTH          =  0x1,
	DRM_MASTER        =  0x2,
	DRM_ROOT_ONLY     =  0x4,
	DRM_CONTROL_ALLOW =  0x8,
	DRM_UNLOCKED      = 0x10,
	DRM_RENDER_ALLOW  = 0x20,
};

enum {
	DRIVER_USE_AGP     = 0x1,
	DRIVER_REQUIRE_AGP = 0x2,
	DRIVER_HAVE_IRQ    = 0x40,
	DRIVER_IRQ_SHARED  = 0x80,
	DRIVER_GEM         = 0x1000,
	DRIVER_MODESET     = 0x2000,
	DRIVER_PRIME       = 0x4000,
	DRIVER_RENDER      = 0x8000,
};

//enum { DRM_HZ = HZ };

enum {
	DRM_SCANOUTPOS_VALID    = (1 << 0),
	DRM_SCANOUTPOS_INVBL    = (1 << 1),
	DRM_SCANOUTPOS_ACCURATE = (1 << 2),
};

enum { DRM_CALLED_FROM_VBLIRQ = 1 };

//enum {
//	DRM_CONNECTOR_POLL_HPD        = 1 << 0,
//	DRM_CONNECTOR_POLL_CONNECT    = 1 << 1,
//	DRM_CONNECTOR_POLL_DISCONNECT = 1 << 2,
//};


/****************
 ** DRM macros **
 ****************/

#define obj_to_crtc(x) container_of(x, struct drm_crtc, base)

///*
// * Type and function mappings
// */
//#define DRM_IRQ_ARGS        void *arg
//#define DRM_ARRAY_SIZE      ARRAY_SIZE
//#define DRM_WAKEUP          wake_up
//#define DRM_INIT_WAITQUEUE  init_waitqueue_head
//#define DRM_AGP_KERN        struct agp_kern_info
//#define DRM_AGP_MEM         struct agp_memory
//#define DRM_COPY_TO_USER    copy_to_user
//
///*
// * Debug macros
// */
#define DRM_VERBOSE 0

#if DRM_VERBOSE
#define DRM_INFO(fmt, arg...) do { \
	lx_printfln("[" DRM_NAME ":%s] *INFO* "   fmt , __func__ , ##arg); } while (0)

#define DRM_ERROR(fmt, arg...) do { \
	lx_printfln("[" DRM_NAME ":%s] *ERROR* "  fmt , __func__ , ##arg); } while (0)

#define DRM_DEBUG(fmt, arg...) do { \
	lx_printfln("[" DRM_NAME ":%s] *DEBUG* "  fmt , __func__ , ##arg); } while (0)

#define DRM_DEBUG_DRIVER(fmt, arg...) do { \
	lx_printfln("[" DRM_NAME ":%s] *DRIVER* " fmt , __func__ , ##arg); } while (0)

#define DRM_DEBUG_KMS(fmt, arg...) do { \
	lx_printfln("[" DRM_NAME ":%s] *KMS* "    fmt , __func__ , ##arg); } while (0)
#else
#define DRM_INFO(fmt, arg...)         do { } while (0)
#define DRM_ERROR(fmt, arg...)        do { } while (0)
#define DRM_DEBUG(fmt, arg...)        do { } while (0)
#define DRM_DEBUG_DRIVER(fmt, arg...) do { } while (0)
#define DRM_DEBUG_KMS(fmt, arg...)    do { } while (0)
#endif

#define DRM_ARRAY_SIZE(x) ARRAY_SIZE(x)

#define DRM_UT_KMS 0x04


///***************
// ** DRM types **
// ***************/
//
///*
// * Forward type declarations
// */
struct drm_device;
struct drm_mm_node;
struct drm_master;
struct drm_file;
struct drm_crtc;
struct drm_plane;
struct drm_display_mode;
struct drm_connector;
struct drm_mode_create_dumb;
struct drm_mode_fb_cmd2;
struct drm_cmdline_mode;


/**
 * Ioctl handler function
 */
typedef int drm_ioctl_t(struct drm_device *dev, void *data,
                        struct drm_file *file_priv);

/**
 * Ioctl representation
 */
struct drm_ioctl_desc {
	unsigned int cmd;
	int flags;
	drm_ioctl_t *func;
	unsigned int cmd_drv;
	const char *name;
};

#define DRM_IOCTL_NR(n) (n & 0xff)

#define DRM_IOCTL_DEF_DRV(ioctl, _func, _flags)			\
	[DRM_IOCTL_NR(DRM_##ioctl)] = {.cmd = DRM_##ioctl, .func = _func, .flags = _flags, .cmd_drv = DRM_IOCTL_##ioctl, .name = #ioctl}

#if 0
#define DRM_IOCTL_DEF(ioctl, _func, _flags) \
	[ioctl & 0xff] = {.cmd = ioctl, .func = _func, .flags = _flags}
#endif


struct drm_gem_object;


struct drm_driver {
	u32 driver_features;
	const struct vm_operations_struct *gem_vm_ops;
	const struct drm_ioctl_desc *ioctls;
	int num_ioctls;
	const struct file_operations *fops;
	int major;
	int minor;
	int patchlevel;
	char *name;
	char *desc;
	char *date;

	int (*load) (struct drm_device *, unsigned long flags);
	int (*unload) (struct drm_device *);
	int (*open) (struct drm_device *, struct drm_file *);
	void (*lastclose) (struct drm_device *);
	void (*preclose) (struct drm_device *, struct drm_file *file_priv);
	void (*postclose) (struct drm_device *, struct drm_file *);

	int (*suspend) (struct drm_device *, pm_message_t state);
	int (*resume) (struct drm_device *);

	int (*device_is_agp) (struct drm_device *dev);
	int (*master_create)(struct drm_device *dev, struct drm_master *master);
	void (*master_destroy)(struct drm_device *dev, struct drm_master *master);
	void (*gem_free_object) (struct drm_gem_object *obj);

	int (*prime_handle_to_fd)(struct drm_device *dev, struct drm_file *file_priv,
	                          uint32_t handle, uint32_t flags, int *prime_fd);
	int (*prime_fd_to_handle)(struct drm_device *dev, struct drm_file *file_priv,
	                          int prime_fd, uint32_t *handle);
	struct dma_buf * (*gem_prime_export)(struct drm_device *dev,
	                                     struct drm_gem_object *obj, int flags);
	struct drm_gem_object * (*gem_prime_import)(struct drm_device *dev,
	                                            struct dma_buf *dma_buf);

	int (*dumb_create)(struct drm_file *file_priv,
	                   struct drm_device *dev,
	                   struct drm_mode_create_dumb *args);
	int (*dumb_map_offset)(struct drm_file *file_priv,
	                       struct drm_device *dev, uint32_t handle,
	                       uint64_t *offset);
	int (*dumb_destroy)(struct drm_file *file_priv,
	                    struct drm_device *dev,
	                    uint32_t handle);
	int (*get_vblank_timestamp) (struct drm_device *dev, int crtc,
	                             int *max_error,
	                             struct timeval *vblank_time,
	                             unsigned flags);
	u32 (*get_vblank_counter) (struct drm_device *dev, int crtc);
	int (*get_scanout_position) (struct drm_device *dev, int crtc,
	                             unsigned int flags,
	                             int *vpos, int *hpos, ktime_t *stime,
	                             ktime_t *etime);
	irqreturn_t(*irq_handler) (int irq, void *arg);
	void (*irq_preinstall) (struct drm_device *dev);
	int (*irq_postinstall) (struct drm_device *dev);
	void (*irq_uninstall) (struct drm_device *dev);
	int (*enable_vblank) (struct drm_device *dev, int crtc);
	void (*disable_vblank) (struct drm_device *dev, int crtc);
};


///* needed by drm_agpsupport.c */
//struct drm_agp_mem {
//	unsigned long handle;
//	DRM_AGP_MEM *memory;
//	unsigned long bound;
//	int pages;
//	struct list_head head;
//};
//
//struct drm_agp_head {
//	DRM_AGP_KERN agp_info;
//	unsigned long base;
//
//	/*
//	 * Members used by drm_agpsupport.c
//	 */
//	int acquired;
//	struct agp_bridge_data *bridge;
//	int enabled;
//	unsigned long mode;
//	struct list_head memory;
//	int cant_use_aperture;
//	unsigned long page_mask;
//};

#define DRM_SWITCH_POWER_ON 0
#define DRM_SWITCH_POWER_OFF 1
#define DRM_SWITCH_POWER_CHANGING 2

struct drm_i915_private;

struct drm_vblank_crtc {
	u32 last;
};

struct drm_device {
//	int pci_device;
	struct pci_dev *pdev;
	struct mutex struct_mutex;
	struct drm_driver *driver;
	struct drm_i915_private *dev_private;
//	struct drm_gem_mm *mm_private;
//	enum drm_stat_type types[15];
//	unsigned long counters;
	struct address_space *dev_mapping;
	struct drm_agp_head *agp;
	int irq_enabled; /* needed for i915_dma.c */
	spinlock_t count_lock;
	struct drm_mode_config mode_config;
	int open_count;
	int vblank_disable_allowed;
	u32 max_vblank_count;
	struct drm_minor *primary; /* needed by i915_dma.c */
//	atomic_t object_count;
//	atomic_t object_memory;
//	atomic_t pin_count;
//	atomic_t pin_memory;
//	atomic_t gtt_count;
//	atomic_t gtt_memory;
//	uint32_t gtt_total;
//	uint32_t invalidate_domains;
//	uint32_t flush_domains;
	int switch_power_state;
	spinlock_t event_lock;
	struct device *dev; /* i915_gem_stolen.c */
	struct drm_vblank_crtc *vblank; /* needed by intel_pm.c */
	spinlock_t vbl_lock; /* needed by intel_pm.c */
	struct timer_list vblank_disable_timer;
};

//
//struct drm_map_list {
//	struct drm_hash_item hash;
//	struct drm_local_map *map;
//	struct drm_mm_node *file_offset_node;
//};


/***************************
 ** drm/drm_vma_manager.h **
 ***************************/

struct drm_vma_offset_node {
	int dummy;
};


struct drm_gem_object {

//	/** Related drm device */
	struct drm_device *dev;
//
//	/** File representing the shmem storage */
	struct file *filp;
//
//	/**
//	 * Size of the object, in bytes.  Immutable over the object's
//	 * lifetime.
//	 */
	size_t size;
//
//	/**
//	 * Global name for this object, starts at 1. 0 means unnamed.
//	 * Access is covered by the object_name_lock in the related drm_device
//	 */
//	int name;
//
//	/* Mapping info for this object */
//	struct drm_map_list map_list;
//
//	/**
//	 * Memory domains. These monitor which caches contain read/write data
//	 * related to the object. When transitioning from one set of domains
//	 * to another, the driver is called to ensure that caches are suitably
//	 * flushed and invalidated
//	 */
	uint32_t read_domains;
	uint32_t write_domain;
//
//	/**
//	 * While validating an exec operation, the
//	 * new read/write domain values are computed here.
//	 * They will be transferred to the above values
//	 * at the point that any cache flushing occurs
//	 */
//	uint32_t pending_read_domains;
//	uint32_t pending_write_domain;
//
//	void *driver_private;
	struct drm_vma_offset_node vma_node;
	struct dma_buf_attachment *import_attach;
};

typedef struct drm_dma_handle {
	void *vaddr;
	size_t size;
	dma_addr_t busaddr;  /* needed by i915_drv.h */
} drm_dma_handle_t;

typedef struct drm_local_map {
	size_t offset;             /* Requested physical address (0 for SAREA)*/
	unsigned long size;        /* Requested physical size (bytes) */
//	enum drm_map_type type;    /* Type of memory to map */
//	enum drm_map_flags flags;  /* Flags */
	void *handle;              /* User-space: "Handle" to pass to mmap() */
	                           /* Kernel-space: kernel-virtual address */
	int mtrr;                  /* MTRR slot used */
} drm_local_map_t;

//struct drm_gem_mm {
//	struct drm_mm offset_manager;
//	struct drm_open_hash offset_hash;
//};
//
struct drm_lock_data {
	struct drm_hw_lock *hw_lock; /* for i915_dma.c */
	struct drm_file *file_priv; /* for i915_dma.c */
};
//
struct drm_master {
	struct drm_lock_data lock; /* needed for i915_dma.c */
	void *driver_priv; /* needed for i915_dma.c */
	struct drm_minor *minor;
};

struct drm_file {
	void *driver_priv;
	struct drm_minor *minor;  /* needed for drm_agpsupport.c */
	struct drm_master *master;  /* needed for i915_dma.c */
	struct list_head fbs;
	struct mutex fbs_lock;
	unsigned stereo_allowed :1;
	unsigned is_master :1; /* this file private is a master for a minor */
	int event_space;
};
//

#define DRM_MINOR_LEGACY 1

///*
// * needed for drm_agpsupport.c
// */
struct drm_minor {
	struct device *kdev;  /* needed by i915_irq.c */
	struct drm_device *dev;
	struct drm_master *master; /* needed for i915_dma.c */
	int index;			/**< Minor device number */
	struct drm_mode_group mode_group;
	int type;                       /**< Control or render */
};
//
///*
// * needed for drm_crtc_helper.h, included by i915_dma.c
// */
//struct drm_encoder { void *helper_private; };
//struct drm_mode_set { };


#define DRM_MODE_OBJECT_CRTC 0xcccccccc
//#define DRM_MODE_OBJECT_CONNECTOR 0xc0c0c0c0
//#define DRM_MODE_OBJECT_ENCODER 0xe0e0e0e0
#define DRM_MODE_OBJECT_MODE 0xdededede
//#define DRM_MODE_OBJECT_PROPERTY 0xb0b0b0b0
//#define DRM_MODE_OBJECT_FB 0xfbfbfbfb
//#define DRM_MODE_OBJECT_BLOB 0xbbbbbbbb
//#define DRM_MODE_OBJECT_PLANE 0xeeeeeeee
//#define DRM_MODE_OBJECT_BRIDGE 0xbdbdbdbd

#define DRM_MODE(nm, t, c, hd, hss, hse, ht, hsk, vd, vss, vse, vt, vs, f) \
	.name = nm, .status = 0, .type = (t), .clock = (c), \
	.hdisplay = (hd), .hsync_start = (hss), .hsync_end = (hse), \
	.htotal = (ht), .hskew = (hsk), .vdisplay = (vd), \
	.vsync_start = (vss), .vsync_end = (vse), .vtotal = (vt), \
	.vscan = (vs), .flags = (f), \
	.base.type = DRM_MODE_OBJECT_MODE

#define CRTC_STEREO_DOUBLE	(1 << 1) /* adjust timings for stereo modes */


struct drm_pending_vblank_event;


#include <drm/drm_dp_helper.h>


/***************************
 ** drm/drm_crtc_helper.h **
 ***************************/

struct drm_pending_event {
	struct drm_event *event;
//	struct list_head link;
	struct drm_file *file_priv;
//	pid_t pid; /* pid of requester, no guarantee it's valid by the time
//		      we deliver the event, for tracing only */
	void (*destroy)(struct drm_pending_event *event);
};

struct drm_pending_vblank_event {
	int dummy;
	struct drm_pending_event base;
//	int pipe;
	struct drm_event_vblank event;
};


/***************************
 ** drm/drm_crtc_helper.h **
 ***************************/

struct drm_cmdline_mode
{
	bool specified;
	bool refresh_specified;
	bool bpp_specified;
	int xres, yres;
	int bpp;
	int refresh;
	bool rb;
	bool interlace;
	bool cvt;
	bool margins;
	enum drm_connector_force force;
};


/******************
 ** Misc helpers **
 ******************/

/* normally found in drm_os_linux.h */
#define DRM_WAIT_ON( ret, queue, timeout, condition )		\
do {								\
	DECLARE_WAITQUEUE(entry, current);			\
	unsigned long end = jiffies + (timeout);		\
	add_wait_queue(&(queue), &entry);			\
								\
	for (;;) {						\
		__set_current_state(TASK_INTERRUPTIBLE);	\
		if (condition)					\
			break;					\
		if (time_after_eq(jiffies, end)) {		\
			ret = -EBUSY;				\
			break;					\
		}						\
		schedule_timeout((HZ/100 > 1) ? HZ/100 : 1);	\
		if (signal_pending(current)) {			\
			ret = -EINTR;				\
			break;					\
		}						\
	}							\
	__set_current_state(TASK_RUNNING);			\
	remove_wait_queue(&(queue), &entry);			\
} while (0)


/* normally found in Linux drmP.h */
#define LOCK_TEST_WITH_RETURN( dev, _file_priv )				\
do {										\
	if (!_DRM_LOCK_IS_HELD(_file_priv->master->lock.hw_lock->lock) ||	\
	    _file_priv->master->lock.file_priv != _file_priv)	{		\
		DRM_ERROR( "%s called without lock held, held  %d owner %p %p\n",\
			   __func__, _DRM_LOCK_IS_HELD(_file_priv->master->lock.hw_lock->lock),\
			   _file_priv->master->lock.file_priv, _file_priv);	\
		return -EINVAL;							\
	}									\
} while (0)


static inline int drm_core_check_feature(struct drm_device *dev, int feature) {
	return ((dev->driver->driver_features & feature) ? 1 : 0); }

#if 0

static inline int drm_core_has_AGP(struct drm_device *dev) {
	return drm_core_check_feature(dev, DRIVER_USE_AGP); }

/*
 * Functions normally provided by drm_bufs.c
 */
static inline resource_size_t
drm_get_resource_start(struct drm_device *dev, unsigned int rsc) {
	return pci_resource_start(dev->pdev, rsc); }

static inline resource_size_t
drm_get_resource_len(struct drm_device *dev, unsigned int rsc) {
	return pci_resource_len(dev->pdev, rsc); }

#endif

static __inline__ bool drm_can_sleep(void) { return true; }

extern void drm_pci_free(struct drm_device *dev, drm_dma_handle_t * dmah);
extern int drm_noop(struct drm_device *dev, void *data, struct drm_file *file_priv);
extern int drm_irq_install(struct drm_device *dev);
extern int drm_irq_uninstall(struct drm_device *dev);
extern struct drm_local_map *drm_getsarea(struct drm_device *dev);
extern int drm_vblank_init(struct drm_device *dev, int num_crtcs);
extern void drm_vblank_cleanup(struct drm_device *dev);
extern void drm_kms_helper_poll_disable(struct drm_device *dev);
extern void drm_kms_helper_poll_init(struct drm_device *dev);
extern void drm_mm_takedown(struct drm_mm *mm);
extern bool drm_helper_hpd_irq_event(struct drm_device *dev);
extern void drm_modeset_lock_all(struct drm_device *dev);
extern void drm_mode_config_reset(struct drm_device *dev);
extern void drm_modeset_unlock_all(struct drm_device *dev);
extern void drm_kms_helper_poll_enable(struct drm_device *dev);
extern int drm_get_pci_dev(struct pci_dev *pdev, const struct pci_device_id *ent, struct drm_driver *driver);
extern void drm_put_dev(struct drm_device *dev);
extern void drm_gem_vm_open(struct vm_area_struct *vma);
extern void drm_gem_vm_close(struct vm_area_struct *vma);
extern int drm_open(struct inode *inode, struct file *filp);
extern int drm_release(struct inode *inode, struct file *filp);
extern int drm_mmap(struct file *filp, struct vm_area_struct *vma);
extern long drm_ioctl(struct file *filp, unsigned int cmd, unsigned long arg);
extern unsigned int drm_poll(struct file *filp, struct poll_table_struct *wait);
extern ssize_t drm_read(struct file *filp, char __user *buffer, size_t count, loff_t *offset);
extern int drm_gem_mmap(struct file *filp, struct vm_area_struct *vma);
extern int drm_gem_prime_handle_to_fd(struct drm_device *dev, struct drm_file *file_priv, uint32_t handle, uint32_t flags, int *prime_fd);
extern int drm_gem_prime_fd_to_handle(struct drm_device *dev, struct drm_file *file_priv, int prime_fd, uint32_t *handle);
extern int drm_gem_dumb_destroy(struct drm_file *file, struct drm_device *dev, uint32_t handle);
extern int drm_pci_init(struct drm_driver *driver, struct pci_driver *pdriver);
extern void drm_pci_exit(struct drm_driver *driver, struct pci_driver *pdriver);
extern void drm_vblank_off(struct drm_device *dev, int crtc);
extern void drm_encoder_cleanup(struct drm_encoder *encoder);
extern void drm_vblank_pre_modeset(struct drm_device *dev, int crtc);
extern void drm_vblank_post_modeset(struct drm_device *dev, int crtc);
extern struct drm_connector *drm_select_eld(struct drm_encoder *encoder, struct drm_display_mode *mode);
extern int drm_av_sync_delay(struct drm_connector *connector, struct drm_display_mode *mode);
extern struct drm_gem_object *drm_gem_object_lookup(struct drm_device *dev, struct drm_file *filp, u32 handle);
extern void drm_gem_object_unreference(struct drm_gem_object *obj);
extern void drm_gem_object_unreference_unlocked(struct drm_gem_object *obj);
extern uint32_t drm_mode_legacy_fb_format(uint32_t bpp, uint32_t depth);
extern void drm_framebuffer_unregister_private(struct drm_framebuffer *fb);
extern void drm_framebuffer_unreference(struct drm_framebuffer *fb);
extern void drm_mode_set_name(struct drm_display_mode *mode);
extern void drm_crtc_cleanup(struct drm_crtc *crtc);
extern void drm_send_vblank_event(struct drm_device *dev, int crtc, struct drm_pending_vblank_event *e);
extern void drm_vblank_put(struct drm_device *dev, int crtc);
extern int drm_vblank_get(struct drm_device *dev, int crtc);
extern void drm_gem_object_reference(struct drm_gem_object *obj);
extern void drm_mode_debug_printmodeline(const struct drm_display_mode *mode);
extern void drm_mode_copy(struct drm_display_mode *dst, const struct drm_display_mode *src);
extern void drm_mode_set_crtcinfo(struct drm_display_mode *p, int adjust_flags);
extern int drm_object_property_set_value(struct drm_mode_object *obj, struct drm_property *property, uint64_t val);
extern void drm_calc_timestamping_constants(struct drm_crtc *crtc, const struct drm_display_mode *mode);
extern bool drm_mode_equal(const struct drm_display_mode *mode1, const struct drm_display_mode *mode2);
extern bool drm_encoder_crtc_ok(struct drm_encoder *encoder, struct drm_crtc *crtc);
extern int drm_mode_crtc_set_gamma_size(struct drm_crtc *crtc, int gamma_size);
extern void drm_crtc_helper_add(struct drm_crtc *crtc, const struct drm_crtc_helper_funcs *funcs);
extern struct drm_mode_object *drm_mode_object_find(struct drm_device *dev, uint32_t id, uint32_t type);
extern void drm_helper_move_panel_connectors_to_head(struct drm_device *);
extern void drm_framebuffer_cleanup(struct drm_framebuffer *fb);
extern int drm_gem_handle_create(struct drm_file *file_priv, struct drm_gem_object *obj, u32 *handlep);
extern int drm_helper_mode_fill_fb_struct(struct drm_framebuffer *fb, struct drm_mode_fb_cmd2 *mode_cmd);
extern int drm_framebuffer_init(struct drm_device *dev, struct drm_framebuffer *fb, const struct drm_framebuffer_funcs *funcs);
extern void drm_mode_config_init(struct drm_device *dev);
extern void drm_kms_helper_poll_fini(struct drm_device *dev);
extern void drm_sysfs_connector_remove(struct drm_connector *connector);
extern void drm_mode_config_cleanup(struct drm_device *dev);
extern int drm_mode_connector_attach_encoder(struct drm_connector *connector, struct drm_encoder *encoder);
extern bool drm_dp_enhanced_frame_cap(const u8 dpcd[DP_RECEIVER_CAP_SIZE]);
extern int drm_encoder_init(struct drm_device *dev, struct drm_encoder *encoder, const struct drm_encoder_funcs *funcs, int encoder_type);
extern int drm_dp_bw_code_to_link_rate(u8 link_bw);
extern u8 drm_dp_max_lane_count(const u8 dpcd[DP_RECEIVER_CAP_SIZE]);
extern u8 drm_match_cea_mode(const struct drm_display_mode *to_match);
extern bool drm_probe_ddc(struct i2c_adapter *adapter);
extern struct edid *drm_edid_duplicate(const struct edid *edid);
extern struct edid *drm_get_edid(struct drm_connector *connector, struct i2c_adapter *adapter);
extern bool drm_detect_monitor_audio(struct edid *edid);
extern struct drm_display_mode *drm_mode_duplicate(struct drm_device *dev, const struct drm_display_mode *mode);
extern void drm_mode_probed_add(struct drm_connector *connector, struct drm_display_mode *mode);
extern void drm_connector_cleanup(struct drm_connector *connector);
extern int drm_helper_probe_single_connector_modes(struct drm_connector *connector, uint32_t maxX, uint32_t maxY);
extern int drm_mode_create_scaling_mode_property(struct drm_device *dev);
extern void drm_object_attach_property(struct drm_mode_object *obj, struct drm_property *property, uint64_t init_val);
extern int drm_add_edid_modes(struct drm_connector *connector, struct edid *edid);
extern int drm_mode_connector_update_edid_property(struct drm_connector *connector, struct edid *edid);
extern void drm_edid_to_eld(struct drm_connector *connector, struct edid *edid);
extern int drm_connector_init(struct drm_device *dev, struct drm_connector *connector, const struct drm_connector_funcs *funcs, int connector_type);
extern int drm_sysfs_connector_add(struct drm_connector *connector);
extern void drm_clflush_virt_range(char *addr, unsigned long length);
extern void drm_vma_node_unmap(struct drm_vma_offset_node *node, struct address_space *file_mapping);
extern bool drm_vma_node_has_offset(struct drm_vma_offset_node *node);
extern int drm_gem_create_mmap_offset(struct drm_gem_object *obj);
extern void drm_gem_free_mmap_offset(struct drm_gem_object *obj);
extern __u64 drm_vma_node_offset_addr(struct drm_vma_offset_node *node);
extern bool drm_mm_node_allocated(struct drm_mm_node *node);
extern void drm_mm_remove_node(struct drm_mm_node *node);
extern void drm_clflush_sg(struct sg_table *st);
extern int drm_gem_object_init(struct drm_device *dev, struct drm_gem_object *obj, size_t size);
extern void drm_prime_gem_destroy(struct drm_gem_object *obj, struct sg_table *sg);
extern void drm_gem_object_release(struct drm_gem_object *obj);
extern drm_dma_handle_t *drm_pci_alloc(struct drm_device *dev, size_t size, size_t align);
extern void drm_clflush_pages(struct page *pages[], unsigned long num_pages);
extern void drm_mode_destroy(struct drm_device *dev, struct drm_display_mode *mode);
extern const char *drm_get_connector_name(const struct drm_connector *connector);
extern const char *drm_get_encoder_name(const struct drm_encoder *encoder);
extern const char *drm_get_format_name(uint32_t format);
extern void drm_mm_init(struct drm_mm *mm, unsigned long start, unsigned long size);
extern int drm_mm_reserve_node(struct drm_mm *mm, struct drm_mm_node *node);
extern int drm_calc_vbltimestamp_from_scanoutpos(struct drm_device *dev, int crtc, int *max_error, struct timeval *vblank_time, unsigned flags, const struct drm_crtc *refcrtc, const struct drm_display_mode *mode);
extern const char *drm_get_connector_status_name(enum drm_connector_status status);
extern void drm_kms_helper_hotplug_event(struct drm_device *dev);
extern bool drm_handle_vblank(struct drm_device *dev, int crtc);
extern void drm_gem_private_object_init(struct drm_device *dev, struct drm_gem_object *obj, size_t size);
extern void drm_sysfs_hotplug_event(struct drm_device *dev);
extern bool drm_mode_parse_command_line_for_connector(const char *mode_option, struct drm_connector *connector, struct drm_cmdline_mode *mode);
extern struct drm_display_mode * drm_mode_create_from_cmdline_mode(struct drm_device *dev, struct drm_cmdline_mode *cmd);

#ifdef __cplusplus
}
#endif /* extern "C" */

#endif /* _DRMP_H_ */
