/*
 * \brief   SCSI support emulation
 * \author  Christian Helmuth
 * \author  Sebastian Sumpf
 * \date    2009-10-29
 *
 * XXX NOTES XXX
 *
 * struct scsi_host_template
 *
 * struct scsi_host
 *
 *   host_lock used by scsi_unlock, scsi_lock
 *   max_id    used by usb_stor_report_device_reset
 *
 * struct scsi_cmnd
 *
 * functions
 *
 *   scsi_add_host
 *   scsi_host_alloc
 *   scsi_host_get
 *   scsi_host_put
 *   scsi_remove_host
 *   scsi_report_bus_reset
 *   scsi_report_device_reset
 *   scsi_scan_host
 */

/*
 * Copyright (C) 2009-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <lx_emul.h>

#include "scsi.h"

#define DEBUG_SCSI 0

/***************
 ** SCSI host **
 ***************/

struct Scsi_Host *scsi_host_alloc(struct scsi_host_template *t, int priv_size)
{
	dde_kit_log(DEBUG_SCSI, "t=%p, priv_size=%d", t, priv_size);

	static int free = 1;

	/* XXX we not some extra space for hostdata[] */
	static char buffer[4096] __attribute__((aligned(4096)));
	static struct Scsi_Host *host = (struct Scsi_Host *)buffer;

	/* FIXME we support only one host for now */
	if (!free) return 0;
	free = 0;

	host->host_lock = &host->default_lock;
	spin_lock_init(host->host_lock);

	host->host_no = 13;
	host->max_id = 8;
	host->hostt = t;

//	rval = scsi_setup_command_freelist(shost);
//	if (rval)
//		goto fail_kfree;

//	shost->ehandler = kthread_run(scsi_error_handler, shost,
//			"scsi_eh_%d", shost->host_no);
//	if (IS_ERR(shost->ehandler)) {
//		rval = PTR_ERR(shost->ehandler);
//		goto fail_destroy_freelist;
//	}

	return host;
}


static struct page *_page(struct scsi_cmnd *cmnd)
{
	return (struct page *)cmnd->sdb.table.sgl->page_link;
}


void scsi_alloc_buffer(size_t size, struct scsi_cmnd *cmnd)
{
	scsi_setup_buffer(cmnd, size, 0, 0);
	struct scatterlist *sgl = cmnd->sdb.table.sgl;
	struct page *page       = _page(cmnd);
	page->virt              = kmalloc(size, GFP_NOIO);
	page->phys              = dma_map_single_attrs(0, page->virt, 0, 0, 0);
	sgl->dma_address        = page->phys;
}


void scsi_setup_buffer(struct scsi_cmnd *cmnd, size_t size, void *virt, dma_addr_t addr)
{
	cmnd->sdb.table.nents = 1;
	cmnd->sdb.length      = size;

	struct scatterlist *sgl = cmnd->sdb.table.sgl;

	struct page *page = _page(cmnd);
	page->virt = virt;
	page->phys = addr;

	sgl->page_link   = (unsigned long)page;
	sgl->offset      = 0;
	sgl->length      = size;
	sgl->dma_address = addr;
	sgl->last        = 1;
}


void scsi_free_buffer(struct scsi_cmnd *cmnd)
{
	struct page *page = _page(cmnd);
	if (page)
		kfree(page->virt);
}


void *scsi_buffer_data(struct scsi_cmnd *cmnd)
{
	return _page(cmnd)->virt;
}


struct scsi_cmnd *_scsi_alloc_command()
{
	struct scsi_cmnd *cmnd = (struct scsi_cmnd *)kmalloc(sizeof(struct scsi_cmnd), GFP_KERNEL);
	cmnd->sdb.table.sgl = (struct scatterlist *)kmalloc(sizeof(struct scatterlist), GFP_KERNEL);
	cmnd->cmnd = kzalloc(MAX_COMMAND_SIZE, 0);
	cmnd->sdb.table.sgl->page_link = (unsigned long) kzalloc(sizeof(struct page), 0);
	return cmnd;
}


void _scsi_free_command(struct scsi_cmnd *cmnd)
{
	kfree((void *)cmnd->sdb.table.sgl->page_link);
	kfree(cmnd->sdb.table.sgl);
	kfree(cmnd->cmnd);
	kfree(cmnd);
}


static void inquiry_done(struct scsi_cmnd *cmnd)
{
	char *data = (char *)scsi_buffer_data(cmnd);
	dde_kit_printf("Vendor id: %c%c%c%c%c%c%c%c Product id: %s\n",
	               data[8], data[9], data[10], data[11], data[12],
	               data[13], data[14], data[15], &data[16]);
	complete(cmnd->back);
}


static void scsi_done(struct scsi_cmnd *cmd)
{
	complete(cmd->back);
}


void scsi_scan_host(struct Scsi_Host *host)
{
	struct scsi_cmnd   *cmnd;
	struct scsi_device *sdev;
	struct scsi_target *target;
	struct completion compl;
	void *result;

	init_completion(&compl);

	sdev = (struct scsi_device *)kmalloc(sizeof(struct scsi_device), GFP_KERNEL);
	target = (struct scsi_target *)kmalloc(sizeof(struct scsi_target), GFP_KERNEL);

	cmnd = _scsi_alloc_command();

	/* init device */
	sdev->sdev_target = target;
	sdev->host = host;
	sdev->id  = 0;
	sdev->lun = 0;
	host->hostt->slave_alloc(sdev);
	host->hostt->slave_configure(sdev);

	/* inquiry (36 bytes for usb) */
	scsi_alloc_buffer(sdev->inquiry_len, cmnd);
	cmnd->cmnd[0] = INQUIRY;
	cmnd->cmnd[4] = sdev->inquiry_len;
	cmnd->device  = sdev;
	cmnd->cmd_len = 6;
	cmnd->sc_data_direction = DMA_FROM_DEVICE;

	cmnd->back = &compl;
	cmnd->scsi_done = inquiry_done;

	host->hostt->queuecommand(host, cmnd);
	wait_for_completion(&compl);

	/* if PQ and PDT are zero we have a direct access block device conntected */
	result = scsi_buffer_data(cmnd);
	if (!((char*)result)[0])
		scsi_add_device(sdev);
	else {
		kfree(sdev);
		kfree(target);
	}

	scsi_free_buffer(cmnd);
	_scsi_free_command(cmnd);
}


/**********************
 ** scsi/scsi_cmnd.h **
 **********************/

unsigned scsi_bufflen(struct scsi_cmnd *cmnd)           { return cmnd->sdb.length; }
struct scatterlist *scsi_sglist(struct scsi_cmnd *cmnd) { return cmnd->sdb.table.sgl; }
unsigned scsi_sg_count(struct scsi_cmnd *cmnd)          { return cmnd->sdb.table.nents; }
