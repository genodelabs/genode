/*
 * \brief SCSI helpers
 * \author Sebastian Sumpf
 * \date 2012-05-06
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#ifndef _SCSI_H_
#define _SCSI_H_

struct scsi_cmnd;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Add a SCSI device
 *
 * \param sdev  Device to add
 */
void scsi_add_device(struct scsi_device *sdev);


/**
 * Alloc data buffer for command
 *
 * \param size  Size of buffer
 * \param cmnd  Command to assciate buffer
 */
void scsi_alloc_buffer(size_t size, struct scsi_cmnd *cmnd);


/**
 * Fill command
 *
 * \param cmnd  Command buffer to setup
 * \param size  Data size
 * \param virt  Virtual address of buffer
 * \param addr  DMA address of buffer
 */
void scsi_setup_buffer(struct scsi_cmnd *cmnd, size_t size, void *virt, dma_addr_t addr);


/**
 * Free data buffer of command
 *
 * \param cmnd  Command
 */
void scsi_free_buffer(struct scsi_cmnd *cmnd);


/**
 * Get buffer data for command
 *
 * \param cmnd  Command to retrieve buffer pointer
 *
 * \return Buffer pointer
 */
void *scsi_buffer_data(struct scsi_cmnd *cmnd);


/**
 * Allocate a SCSI command
 *
 * \return Allocated command or zero on failure
 */
struct scsi_cmnd *_scsi_alloc_command();


/**
 * Free a SCSI command
 * 
 * \param cmnd  Command
 */
void _scsi_free_command(struct scsi_cmnd *cmnd);

#ifdef __cplusplus
}
#endif

#endif /* _SCSI_H_ */
