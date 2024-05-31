/*
 * \brief  VBE constants and definitions
 * \author Sebastian Sumpf
 * \date   2007-09-11
 */

/*
 * Copyright (C) 2007-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _VBE_H_
#define _VBE_H_

#include <base/stdint.h>

namespace Vesa {

	using namespace Genode;

/* VBE controller information */
struct mb_vbe_ctrl_t
{
	uint8_t signature[4];
	uint16_t version;
	uint32_t oem_string;
	uint32_t capabilities;
	uint32_t video_mode;
	uint16_t total_memory;
	uint16_t oem_software_rev;
	uint32_t oem_vendor_name;
	uint32_t oem_product_name;
	uint32_t oem_product_rev;
	uint8_t reserved[222];
	uint8_t oem_data[256];
} __attribute__((packed));


/* VBE mode information */
struct mb_vbe_mode_t
{
	/* all VESA versions */
	uint16_t mode_attributes;
	uint8_t win_a_attributes;
	uint8_t win_b_attributes;
	uint16_t win_granularity;
	uint16_t win_size;
	uint16_t win_a_segment;
	uint16_t win_b_segment;
	uint32_t win_func;
	uint16_t bytes_per_scanline;

	/* >= VESA version 1.2 */
	uint16_t x_resolution;
	uint16_t y_resolution;
	uint8_t x_char_size;
	uint8_t y_char_size;
	uint8_t number_of_planes;
	uint8_t bits_per_pixel;
	uint8_t number_of_banks;
	uint8_t memory_model;
	uint8_t bank_size;
	uint8_t number_of_image_pages;
	uint8_t reserved0;

	/* direct color */
	uint8_t red_mask_size;
	uint8_t red_field_position;
	uint8_t green_mask_size;
	uint8_t green_field_position;
	uint8_t blue_mask_size;
	uint8_t blue_field_position;
	uint8_t reserved_mask_size;
	uint8_t reserved_field_position;
	uint8_t direct_color_mode_info;

	/* >= VESA version 2.0 */
	uint32_t phys_base;
	uint32_t reserved1;
	uint16_t reserved2;

	/* >= VESA version 3.0 */
	uint16_t linear_bytes_per_scanline;
	uint8_t banked_number_of_image_pages;
	uint8_t linear_number_of_image_pages;
	uint8_t linear_red_mask_size;
	uint8_t linear_red_field_position;
	uint8_t linear_green_mask_size;
	uint8_t linear_green_field_position;
	uint8_t linear_blue_mask_size;
	uint8_t linear_blue_field_position;
	uint8_t linear_reserved_mask_size;
	uint8_t linear_reserved_field_position;
	uint32_t max_pixel_clock;

	uint8_t reserved3[190];
} __attribute__ ((packed));

}

#endif /* _VBE_H_ */
