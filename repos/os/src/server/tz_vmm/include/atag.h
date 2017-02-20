/**
 * \brief  Arm boot descriptor tags (ATAGs).
 * \author Stefan Kalkowski
 * \date   2012-07-30
 *
 * Based on the code example of Vincent Sanders (published under BSD licence):
 * http://www.simtec.co.uk/products/SWLINUX/files/booting_article.html
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SRC__SERVER__VMM__INCLUDE__ATAG_H_
#define _SRC__SERVER__VMM__INCLUDE__ATAG_H_

#include <base/stdint.h>
#include <util/string.h>

class Atag {

	private:

		enum atags {
			ATAG_NONE      = 0x00000000,
			ATAG_CORE      = 0x54410001,
			ATAG_MEM       = 0x54410002,
			ATAG_VIDEOTEXT = 0x54410003,
			ATAG_RAMDISK   = 0x54410004,
			ATAG_INITRD2   = 0x54420005,
			ATAG_SERIAL    = 0x54410006,
			ATAG_REVISION  = 0x54410007,
			ATAG_VIDEOLFB  = 0x54410008,
			ATAG_CMDLINE   = 0x54410009,
		};

		struct atag_header {
			Genode::uint32_t size;
			Genode::uint32_t tag;
		};

		struct atag_core {
			Genode::uint32_t flags;
			Genode::uint32_t pagesize;
			Genode::uint32_t rootdev;
		};

		struct atag_mem {
			Genode::uint32_t size;
			Genode::uint32_t start;
		};

		struct atag_videotext {
			Genode::uint8_t  x;
			Genode::uint8_t  y;
			Genode::uint16_t video_page;
			Genode::uint8_t  video_mode;
			Genode::uint8_t  video_cols;
			Genode::uint16_t video_ega_bx;
			Genode::uint8_t  video_lines;
			Genode::uint8_t  video_isvga;
			Genode::uint16_t video_points;
		};

		struct atag_ramdisk {
			Genode::uint32_t flags;
			Genode::uint32_t size;
			Genode::uint32_t start;
		};

		struct atag_initrd2 {
			Genode::uint32_t start;
			Genode::uint32_t size;
		};

		struct atag_serialnr {
			Genode::uint32_t low;
			Genode::uint32_t high;
		};

		struct atag_revision {
			Genode::uint32_t rev;
		};

		struct atag_videolfb {
			Genode::uint16_t lfb_width;
			Genode::uint16_t lfb_height;
			Genode::uint16_t lfb_depth;
			Genode::uint16_t lfb_linelength;
			Genode::uint32_t lfb_base;
			Genode::uint32_t lfb_size;
			Genode::uint8_t  red_size;
			Genode::uint8_t  red_pos;
			Genode::uint8_t  green_size;
			Genode::uint8_t  green_pos;
			Genode::uint8_t  blue_size;
			Genode::uint8_t  blue_pos;
			Genode::uint8_t  rsvd_size;
			Genode::uint8_t  rsvd_pos;
		};

		struct atag_cmdline {
			char    cmdline[1];
		};

		struct atag {
			struct atag_header hdr;
			union {
				struct atag_core         core;
				struct atag_mem          mem;
				struct atag_videotext    videotext;
				struct atag_ramdisk      ramdisk;
				struct atag_initrd2      initrd2;
				struct atag_serialnr     serialnr;
				struct atag_revision     revision;
				struct atag_videolfb     videolfb;
				struct atag_cmdline      cmdline;
			} u;
		};

		struct atag *_params; /* used to point at the current tag */

		inline void _next() {
			_params = ((struct atag *)((Genode::uint32_t *)(_params)
			                           + _params->hdr.size)); }

		template <typename TAG>
		Genode::size_t _size() {
			return ((sizeof(struct atag_header) + sizeof(TAG)) >> 2); }

	public:

		Atag(void* base) : _params((struct atag *)base)
		{
			_params->hdr.tag         = ATAG_CORE;
			_params->hdr.size        = _size<struct atag_core>();
			_params->u.core.flags    = 1;
			_params->u.core.pagesize = 0x1000;
			_params->u.core.rootdev  = 0;
			_next();
		}

		void setup_ramdisk_tag(Genode::size_t size)
		{
			_params->hdr.tag         = ATAG_RAMDISK;
			_params->hdr.size        = _size<struct atag_ramdisk>();
			_params->u.ramdisk.flags = 0;
			_params->u.ramdisk.size  = size;
			_params->u.ramdisk.start = 0;
			_next();
		}

		void setup_initrd2_tag(Genode::addr_t start, Genode::size_t size)
		{
			_params->hdr.tag         = ATAG_INITRD2;
			_params->hdr.size        = _size<struct atag_initrd2>();
			_params->u.initrd2.start = start;
			_params->u.initrd2.size  = size;
			_next();
		}

		void setup_rev_tag(Genode::addr_t rev)
		{
			_params->hdr.tag         = ATAG_REVISION;
			_params->hdr.size        = _size<struct atag_revision>();
			_params->u.revision.rev  = rev;
			_next();
		}

		void setup_mem_tag(Genode::addr_t start, Genode::size_t len)
		{
			_params->hdr.tag     = ATAG_MEM;
			_params->hdr.size    = _size<struct atag_mem>();
			_params->u.mem.start = start;
			_params->u.mem.size  = len;
			_next();
		}

		void setup_cmdline_tag(const char * line)
		{
			int len = Genode::strlen(line);
			if(!len)
				return;

			_params->hdr.tag  = ATAG_CMDLINE;
			_params->hdr.size = (sizeof(struct atag_header) + len + 1 + 4) >> 2;
			Genode::strncpy(_params->u.cmdline.cmdline, line, len + 1);
			_next();
		}

		void setup_end_tag(void)
		{
			_params->hdr.tag  = ATAG_NONE;
			_params->hdr.size = 0;
		}
};

#endif /* _SRC__SERVER__VMM__INCLUDE__ATAG_H_ */
