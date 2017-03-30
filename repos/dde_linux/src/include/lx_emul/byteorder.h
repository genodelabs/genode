/*
 * \brief  Linux kernel API
 * \author Norman Feske
 * \author Sebastian Sumpf
 * \author Josef Soentgen
 * \date   2014-08-21
 *
 * Based on the prototypes found in the Linux kernel's 'include/'.
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

/*************************************
 ** linux/byteorder/little_endian.h **
 *************************************/

#include <uapi/linux/byteorder/little_endian.h>


/*******************************
 ** linux/byteorder/generic.h **
 *******************************/

#define le16_to_cpu  __le16_to_cpu
#define be16_to_cpu  __be16_to_cpu
#define le32_to_cpu  __le32_to_cpu
#define be32_to_cpu  __be32_to_cpu
#define le16_to_cpus __le16_to_cpus
#define cpu_to_le16p __cpu_to_le16p
#define cpu_to_be16p __cpu_to_be16p
#define cpu_to_le16  __cpu_to_le16
#define cpu_to_le16s __cpu_to_le16s
#define cpu_to_be16  __cpu_to_be16
#define cpu_to_le32  __cpu_to_le32
#define cpu_to_le32p __cpu_to_le32p
#define cpu_to_be32  __cpu_to_be32
#define cpu_to_be32p __cpu_to_be32p
#define cpu_to_le32s __cpu_to_le32s
#define cpu_to_le64  __cpu_to_le64
#define le16_to_cpup __le16_to_cpup
#define be16_to_cpup __be16_to_cpup
#define le32_to_cpup __le32_to_cpup
#define le32_to_cpus __le32_to_cpus
#define be32_to_cpup __be32_to_cpup
#define le64_to_cpu  __le64_to_cpu
