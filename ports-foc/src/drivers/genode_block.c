/*
 * \brief  Block driver to access Genode's block service
 * \author Stefan Kalkowski <stefan.kalkowski@genode-labs.com>
 * \date   2010-07-08
 */

/*
 * Copyright (C) 2010-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Linux includes */
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/genhd.h>
#include <linux/blkdev.h>
#include <linux/hdreg.h>
#include <linux/semaphore.h>
#include <linux/interrupt.h>

/* Genode support library includes */
#include <genode/block.h>
#include <l4/log/log.h>

enum Geometry {
	KERNEL_SECTOR_SIZE = 512,      /* sector size used by kernel */
	GENODE_BLK_MINORS  = 16        /* number of minor numbers */
};


/*
 * The internal representation of our device.
 */
struct genode_blk_device {
	unsigned              blk_cnt;    /* Total block count */
	unsigned long         blk_sz;     /* Single block size */
	spinlock_t            lock;       /* For mutual exclusion */
	struct gendisk       *gd;         /* Generic disk structure */
	struct request_queue *queue;      /* The device request queue */
	struct semaphore      queue_wait; /* Used to block, when queue is full */
	short                 stopped;    /* Indicates queue availability */
	unsigned              irq;        /* IRQ number */
	l4_cap_idx_t          irq_cap;    /* IRQ capability slot */
	unsigned              idx;        /* drive index */
};

enum { MAX_DISKS = 16 };
static struct genode_blk_device blk_devs[MAX_DISKS];

/*
 * Handle an I/O request.
 */
static void genode_blk_request(struct request_queue *q)
{
	struct request *req;
	unsigned long  queue_offset;
	void          *buf;
	unsigned long  offset;
	unsigned long  nbytes;
	short          write;
	struct genode_blk_device* dev;

	while ((req = blk_fetch_request(q))) {
		dev = req->rq_disk->private_data;
		buf    = 0;
		offset = blk_rq_pos(req) * KERNEL_SECTOR_SIZE;
		nbytes = blk_rq_bytes(req);
		write  = rq_data_dir(req) == WRITE;

		if (req->cmd_type != REQ_TYPE_FS) {
			printk(KERN_NOTICE "Skip non-fs request\n");
			__blk_end_request_all(req, -EIO);
			continue;
		}

		while (!buf) {
			unsigned long flags;

			if ((buf = genode_block_request(dev->idx, nbytes, req, &queue_offset)))
				break;

			/* stop_queue needs disabled interrupts */
			local_irq_save(flags);
			local_irq_disable();
			blk_stop_queue(q);
			local_irq_restore(flags);
			local_irq_enable();

			dev->stopped = 1;

			/*
			 * This function is called with the request queue lock held, unlock to
			 * enable VCPU IRQs
			 */
			spin_unlock_irq(q->queue_lock);
			/* block until new responses are available */
			down(&dev->queue_wait);
			spin_lock_irq(q->queue_lock);

			/* start_queue needs disabled interrupts */
			local_irq_save(flags);
			local_irq_disable();
			blk_start_queue(q);
			local_irq_restore(flags);
			local_irq_enable();
		}

		if (write) {
			char               *ptr = (char*) buf;
			struct req_iterator iter;
			struct bio_vec     *bvec;

			rq_for_each_segment(bvec, req, iter) {
				void *buffer = page_address(bvec->bv_page) + bvec->bv_offset;
				memcpy((void*)ptr, buffer, bvec->bv_len);
				ptr += bvec->bv_len;
			}
		}

		genode_block_submit(dev->idx, queue_offset, nbytes, offset, write);
	}
}


static void genode_end_request(void *request, short write,
                               void *buf, unsigned long sz) {
	struct request *req = (struct request*) request;
	struct genode_blk_device *dev = req->rq_disk->private_data;
	char *ptr = (char*) buf;

	if (!write) {
		struct req_iterator iter;
		struct bio_vec     *bvec;

		rq_for_each_segment(bvec, req, iter) {
			void *buffer = page_address(bvec->bv_page) + bvec->bv_offset;
			memcpy(buffer, (void*)ptr, bvec->bv_len);
			ptr += bvec->bv_len;
		}
	}

	__blk_end_request_all(req, 0);

	if (dev->stopped) {
		dev->stopped = 0;
		up(&dev->queue_wait);
	}
}


static int genode_blk_getgeo(struct block_device *bdev, struct hd_geometry *geo)
{
	struct genode_blk_device *dev  = bdev->bd_disk->private_data;
	unsigned long             size = dev->blk_cnt * dev->blk_sz *
	                                 (dev->blk_sz / KERNEL_SECTOR_SIZE);
	geo->cylinders = size << 5;
	geo->heads     = 4;
	geo->sectors   = 32;
	return 0;
}


/*
 * The device operations structure.
 */
static struct block_device_operations genode_blk_ops = {
		.owner  = THIS_MODULE,
		.getgeo = genode_blk_getgeo
};


static irqreturn_t event_interrupt(int irq, void *data)
{
	struct genode_blk_device *dev = (struct genode_blk_device *)data;
	genode_block_collect_responses(dev->idx);
	return IRQ_HANDLED;
}


static int __init genode_blk_init(void)
{
	int      err;
	unsigned drive;
	unsigned drive_cnt = (genode_block_count() > MAX_DISKS)
	                     ? MAX_DISKS : genode_block_count();

	/**
	 * Loop through all Genode block devices and register them in Linux.
	 */
	for (drive = 0 ; drive < drive_cnt; drive++) {
		int           major_num;
		int           writeable    = 0;
		unsigned long req_queue_sz = 0;

		/* Initialize device structure */
		memset (&blk_devs[drive], 0, sizeof(struct genode_blk_device));
		blk_devs[drive].idx = drive;
		spin_lock_init(&blk_devs[drive].lock);

		genode_block_geometry(drive, (unsigned long*)&blk_devs[drive].blk_cnt,
		                      &blk_devs[drive].blk_sz, &writeable, &req_queue_sz);

		/**
		 * Obtain an IRQ for the drive.
		 */
		blk_devs[drive].irq_cap = genode_block_irq_cap(drive);
		if ((blk_devs[drive].irq = l4x_register_irq(blk_devs[drive].irq_cap)) < 0)
			return -ENOMEM;
		if ((err = request_irq(blk_devs[drive].irq, event_interrupt, IRQF_SAMPLE_RANDOM,
		                       "Genode block", &blk_devs[drive]))) {
			printk(KERN_WARNING "%s: request_irq failed: %d\n", __func__, err);
			return err;
		}

		/*
		 * Get a request queue.
		 */
		if(!(blk_devs[drive].queue = blk_init_queue(genode_blk_request,
		                                           &blk_devs[drive].lock)))
				return -ENOMEM;

		/*
		 * Align queue requests to hardware sector size.
		 */
		blk_queue_logical_block_size(blk_devs[drive].queue, blk_devs[drive].blk_sz);

		/*
		 * Important, limit number of sectors per request,
		 * as Genode's block-session has a limited request-transmit-queue.
		 */
		blk_queue_max_hw_sectors(blk_devs[drive].queue, req_queue_sz / KERNEL_SECTOR_SIZE);
		blk_devs[drive].queue->queuedata = &blk_devs[drive];

		sema_init(&blk_devs[drive].queue_wait, 0);
		blk_devs[drive].stopped = 0;

		/*
		 * Register block device and gain major number.
		 */
		major_num = register_blkdev(0, genode_block_name(drive));
		if(major_num < 1) {
				printk(KERN_WARNING "genode_blk: unable to get major number\n");
				return -EBUSY;
		}

		/*
		 * Allocate and setup generic disk structure.
		 */
		if(!(blk_devs[drive].gd = alloc_disk(GENODE_BLK_MINORS))) {
				unregister_blkdev(major_num, genode_block_name(drive));
				return -ENOMEM;
		}
		blk_devs[drive].gd->major        = major_num;
		blk_devs[drive].gd->first_minor  = 0;
		blk_devs[drive].gd->fops         = &genode_blk_ops;
		blk_devs[drive].gd->private_data = &blk_devs[drive];
		blk_devs[drive].gd->queue        = blk_devs[drive].queue;
		strncpy(blk_devs[drive].gd->disk_name, genode_block_name(drive),
		        sizeof(blk_devs[drive].gd->disk_name));
		set_capacity(blk_devs[drive].gd, blk_devs[drive].blk_cnt *
		                         (blk_devs[drive].blk_sz / KERNEL_SECTOR_SIZE));

		/* Set it read-only or writeable */
		if (!writeable)
			set_disk_ro(blk_devs[drive].gd, 1);

		if (drive == 0)
			genode_block_register_callback(genode_end_request);

		/* Make the block device available to the system */
		add_disk(blk_devs[drive].gd);
	}

	printk(KERN_NOTICE "Genode blk-file driver initialized\n");
	return 0;
}


static void __exit
genode_blk_exit(void)
{
	unsigned drive, drive_cnt = (genode_block_count() > MAX_DISKS)
	                            ? MAX_DISKS : genode_block_count();
	for (drive = 0 ; drive < drive_cnt; drive++) {
		del_gendisk(blk_devs[drive].gd);
		put_disk(blk_devs[drive].gd);
		unregister_blkdev(blk_devs[drive].gd->major, genode_block_name(drive));
		blk_cleanup_queue(blk_devs[drive].queue);
	}
}

module_init(genode_blk_init);
module_exit(genode_blk_exit);

MODULE_LICENSE("GPL");
