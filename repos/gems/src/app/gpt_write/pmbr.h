/*
 * \brief  Protective MBR partition table definitions
 * \author Josef Soentgen
 * \date   2018-05-03
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _PMBR_H_
#define _PMBR_H_

/* Genode includes */
#include <base/fixed_stdint.h>


namespace Protective_mbr
{
	enum { TYPE_PROTECTIVE = 0xEE, };

	/**
	 * Partition table entry format
	 */
	struct Partition
	{
		Genode::uint8_t  unused[4]  {  };
		Genode::uint8_t  type       {  };
		Genode::uint8_t  unused2[3] {  };
		Genode::uint32_t lba        {  };
		Genode::uint32_t sectors    {  };
	} __attribute__((packed));

	/**
	 * Master boot record header
	 */
	struct Header
	{
		Genode::uint8_t  unused[446]   { };
		Partition        partitions[4] { };
		Genode::uint16_t magic         { 0xaa55 };
	} __attribute__((packed));
}

#endif /* _PMBR_H_ */
