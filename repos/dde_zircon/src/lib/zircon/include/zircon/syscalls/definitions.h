/*
 * \brief  Zircon syscall declarations
 * \author Johannes Kliemann
 * \date   2018-07-25
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _DDE_ZIRCON_DEFINITIONS_H_
#define _DDE_ZIRCON_DEFINITIONS_H_

zx_status_t zx_ioports_request(
	ZX_SYSCALL_PARAM_ATTR(handle_use) zx_handle_t resource,
	uint16_t io_addr,
	uint32_t len) __LEAF_FN;

zx_status_t zx_interrupt_wait(
	ZX_SYSCALL_PARAM_ATTR(handle_use) zx_handle_t handle,
	zx_time_t *out_timestamp) __LEAF_FN;

zx_status_t zx_interrupt_destroy(
	ZX_SYSCALL_PARAM_ATTR(handle_use) zx_handle_t handle) __LEAF_FN;

zx_status_t zx_handle_close(
	ZX_SYSCALL_PARAM_ATTR(handle_release_always) zx_handle_t handle) __LEAF_FN;

zx_status_t zx_interrupt_create(
	ZX_SYSCALL_PARAM_ATTR(handle_use) zx_handle_t src_obj,
	uint32_t src_num,
	uint32_t options,
	zx_handle_t *out) __NONNULL((4)) __LEAF_FN;

#endif /* ifndef _DDE_ZIRCON_DEFINITIONS_H_ */
