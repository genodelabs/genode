/*
 * \brief   Low level disk I/O module
 * \author  Sebastian Sumpf <Sebastian.Sumpf@genode-labs.com>
 * \date    2009-11-20
 */

/*
 * Copyright (C) 2009-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <ffat/diskio.h>
#include <dde_linux26/block.h>

typedef __SIZE_TYPE__ size_t;
extern void *memcpy(void *dest, const void *src, size_t n);
/*-----------------------------------------------------------------------*/


/*-----------------------------------------------------------------------*/
/* Inidialize a Drive                                                    */

DSTATUS disk_initialize (
	BYTE drv				/* Physical drive nmuber (0..) */
)
{
	if (dde_linux26_block_present((int)drv))
		return 0;
	
	return STA_NOINIT;
}


/*-----------------------------------------------------------------------*/
/* Return Disk Status                                                    */

DSTATUS disk_status (
	BYTE drv		/* Physical drive nmuber (0..) */
)
{
	if (dde_linux26_block_present((int)drv))
		return 0;

	return STA_NODISK;
}


/*-----------------------------------------------------------------------*/
/* Read Sector(s)                                                        */

DRESULT disk_read (
	BYTE drv,		/* Physical drive nmuber (0..) */
	BYTE *buff,		/* Data buffer to store read data */
	DWORD sector,	/* Sector address (LBA) */
	BYTE count		/* Number of sectors to read (1..255) */
)
{
	int result;
	BYTE i;
	void * dma_buf = dde_linux26_block_malloc(512);
	for (i = 0; i < count; i++) {
		result = dde_linux26_block_read((int)drv, (unsigned long)(sector + i), dma_buf);

		switch (result) {
			case -EBLK_NODEV: return RES_NOTRDY;
			case -EBLK_FAULT: return RES_PARERR;
		}

		memcpy(buff + (i * 512), dma_buf, 512);
	}
	dde_linux26_block_free(dma_buf);
	return RES_OK;
}


/*-----------------------------------------------------------------------*/
/* Write Sector(s)                                                       */

#if _READONLY == 0
DRESULT disk_write (
	BYTE drv,			/* Physical drive nmuber (0..) */
	const BYTE *buff,	/* Data to be written */
	DWORD sector,		/* Sector address (LBA) */
	BYTE count			/* Number of sectors to write (1..255) */
)
{
	int result;
 	BYTE i;
	void * dma_buf = dde_linux26_block_malloc(512);
	for (i = 0; i < count; i++) {
		result = dde_linux26_block_write((int)drv, (unsigned long)(sector + i), dma_buf);

		switch (result) {
			case -EBLK_NODEV: return RES_NOTRDY;
			case -EBLK_FAULT: return RES_PARERR;
		}
		memcpy(dma_buf, buff + (i * 512), 512);
	}
	dde_linux26_block_free(dma_buf);
	return RES_OK;
}
#endif /* _READONLY */


/*-----------------------------------------------------------------------*/
/* Miscellaneous Functions                                               */

DRESULT disk_ioctl (
	BYTE drv,		/* Physical drive nmuber (0..) */
	BYTE ctrl,		/* Control code */
	void *buff		/* Buffer to send/receive control data */
)
{
	int result;
	DWORD dresult = 1;
	if(!dde_linux26_block_present((int)drv))
		return RES_PARERR;
	switch (ctrl) {
		case CTRL_SYNC:
			break;
		case GET_SECTOR_SIZE:
			result = dde_linux26_block_size((int)drv);
			if (result < 0)
				return RES_ERROR;
			memcpy(buff, &result, sizeof(int));
			break;
		case GET_SECTOR_COUNT:
			result = dde_linux26_block_count((int)drv);
			if(result < 0)
				return RES_ERROR;
			memcpy(buff, &result, sizeof(int));
			break;
		case GET_BLOCK_SIZE:
			memcpy(buff, &dresult, sizeof(DWORD));
			break;
	}
	return RES_OK;
}

DWORD get_fattime(void)
{
	return 0;
}

