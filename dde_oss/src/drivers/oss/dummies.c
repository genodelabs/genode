/*
 * \brief  OSS-dummy functions
 * \author Sebastian Sumpf
 * \date   20120-11-20
 */

/*
 * Copyright (C) 20120-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <oss_config.h>
#include <oss_pci.h>


/**
 * PCI
 */

int pci_write_config_dword (oss_device_t * osdev, offset_t where, unsigned int val)
{ TRACE; return 0; }


/**
 * OSS
 */

void oss_pci_byteswap (oss_device_t * osdev, int mode) { }


int oss_register_device (oss_device_t * osdev, const char *name)
{ TRACE; return 0; }


void oss_unregister_device (oss_device_t * osdev)
{ TRACE; }


int oss_disable_device (oss_device_t * osdev)
{ TRACE; return 0; }


void oss_unregister_interrupts (oss_device_t * osdev)
{ TRACE; }


void * oss_get_osid (oss_device_t * osdev) { TRACE; return 0; }


int oss_get_cardinfo (int cardnum, oss_card_info * ci) { TRACE; return 0; }


oss_device_t * osdev_clone (oss_device_t * orig_osdev, int new_instance) { TRACE; return 0; }


timeout_id_t timeout (void (*func) (void *), void *arg, unsigned long long ticks) { TRACE; return 0; }


void untimeout(timeout_id_t id) { TRACE; }


/* wait queues */

void oss_reset_wait_queue (struct oss_wait_queue *wq) {  }

