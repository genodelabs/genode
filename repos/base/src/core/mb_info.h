/**
 * \brief  Multiboot info structure as defined by GRUB
 * \author Christian Helmuth
 * \date   2006-05-09
 *
 * This is a stripped down version. Original code in
 * l4/pkg/l4util/include/ARCH-x86/mb_info.h.
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _MB_INFO_H_
#define _MB_INFO_H_

#include <base/stdint.h>

/**
 * Multi-boot module
 */
typedef struct
{
  uint32_t mod_start;  /* starting address of module in memory. */
  uint32_t mod_end;    /* end address of module in memory. */
  uint32_t cmdline;    /* module command line */
  uint32_t pad;        /* padding to take it to 16 bytes */
} mb_mod_t;


/** VBE controller information. */
typedef struct
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
} __attribute__((packed)) mb_vbe_ctrl_t;


/** VBE mode information. */
typedef struct
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
  uint16_t reversed2;

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

  uint8_t reserved3[189];
} __attribute__ ((packed)) mb_vbe_mode_t;


/**
 *  Multi-boot information
 */
typedef struct
{
  uint32_t flags;        /* MultiBoot info version number */
  uint32_t mem_lower;    /* available memory below 1MB */
  uint32_t mem_upper;    /* available memory starting from 1MB [kB] */
  uint32_t boot_device;  /* "root" partition */
  uint32_t cmdline;      /* Kernel command line */
  uint32_t mods_count;   /* number of modules */
  uint32_t mods_addr;    /* module list */

  union
  {
    struct
    {
      /* (a.out) Kernel symbol table info */
      uint32_t tabsize;
      uint32_t strsize;
      uint32_t addr;
      uint32_t pad;
    } a;

    struct
    {
      /* (ELF) Kernel section header table */
      uint32_t num;
      uint32_t size;
      uint32_t addr;
      uint32_t shndx;
    } e;
  } syms;

  uint32_t mmap_length;        /* size of memory mapping buffer */
  uint32_t mmap_addr;          /* address of memory mapping buffer */
  uint32_t drives_length;      /* size of drive info buffer */
  uint32_t drives_addr;        /* address of driver info buffer */
  uint32_t config_table;       /* ROM configuration table */
  uint32_t boot_loader_name;   /* Boot Loader Name */
  uint32_t apm_table;          /* APM table */
  uint32_t vbe_ctrl_info;      /* VESA video contoller info */
  uint32_t vbe_mode_info;      /* VESA video mode info */
  uint16_t vbe_mode;           /* VESA video mode number */
  uint16_t vbe_interface_seg;  /* VESA segment of prot BIOS interface */
  uint16_t vbe_interface_off;  /* VESA offset of prot BIOS interface */
  uint16_t vbe_interface_len;  /* VESA lenght of prot BIOS interface */
} mb_info_t;

/**
 *  Flags to be set in the 'flags' parameter above
 */

/** is the command-line defined? */
#define MB_CMDLINE           0x00000004

/** Is there video information?  */
#define MB_VIDEO_INFO        0x00000800

/** If we are multiboot-compliant, this value is present in the eax register */
#define MB_VALID             0x2BADB002

#endif
