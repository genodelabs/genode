/**
 * \brief  Interface used between Genode config/report (C++) and lx_user (C)
 * \author Alexander Boettcher
 * \date   2022-02-17
 */

/*
 * Copyright (C) 2022-2024 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#ifndef _LX_I915_H_
#define _LX_I915_H_

struct genode_mode {
	unsigned width;
	unsigned height;
	unsigned force_width;
	unsigned force_height;
	unsigned max_width;
	unsigned max_height;
	unsigned width_mm;
	unsigned height_mm;
	unsigned hz;
	unsigned brightness;
	unsigned enabled;
	unsigned preferred;
	unsigned inuse;
	unsigned id;
	char mirror;
	char name[32];
};

enum Action {
	ACTION_IDLE         = 0,
	ACTION_DETECT_MODES = 1,
	ACTION_CONFIGURE    = 2,
	ACTION_REPORT       = 3,
	ACTION_NEW_CONFIG   = 4,
	ACTION_READ_CONFIG  = 5,
	ACTION_HOTPLUG      = 6,
	ACTION_EXIT         = 7,
	ACTION_FAILED       = 9,
};

int  lx_emul_i915_blit(unsigned const connector_id, char const may_sleep);
void lx_emul_i915_wakeup(unsigned connector_id);
void lx_emul_i915_report_discrete(void * genode_xml);
void lx_emul_i915_report_non_discrete(void * genode_xml);
void lx_emul_i915_hotplug_connector(void);
int  lx_emul_i915_action_to_process(int);

void lx_emul_i915_report_connector(void * lx_data, void * genode_xml,
                                   char const *name, char connected,
                                   char valid_fb,
                                   unsigned brightness,
                                   unsigned width_mm, unsigned height_mm);
void lx_emul_i915_iterate_modes(void *lx_data, void * genode_data);
void lx_emul_i915_report_modes(void * genode_xml, struct genode_mode *);
void lx_emul_i915_connector_config(char * name, struct genode_mode *);
int  lx_emul_i915_config_done_and_block(void);
void lx_emul_i915_framebuffer_ready(unsigned connector_id,
                                    char const * const connector_name,
                                    void * base,
                                    unsigned long size,
                                    unsigned xres, unsigned yres,
                                    unsigned virtual_width,
                                    unsigned virtual_height,
                                    unsigned mm_width,
                                    unsigned mm_height);

#endif /* _LX_I915_H_ */
