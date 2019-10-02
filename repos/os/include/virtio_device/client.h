/*
 * \brief  Client-side VirtIO bus session interface
 * \author Piotr Tworek
 * \date   2019-09-27
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#pragma once

#include <base/rpc_client.h>

#include <virtio_device/capability.h>
#include <virtio_device/virtio_device.h>
#include <virtio_device/virt_queue.h>

namespace Virtio { struct Device_client; }

struct Virtio::Device_client : Genode::Rpc_client<Virtio::Device>
{
	explicit Device_client(Device_capability device)
	: Genode::Rpc_client<Virtio::Device>(device) { }

	uint32_t vendor_id() override { return call<Rpc_vendor_id>(); }
	uint32_t device_id() override { return call<Rpc_device_id>(); }

	uint8_t get_status() override {
		return call<Rpc_get_status>(); }

	bool set_status(uint8_t status) override {
		return call<Rpc_set_status>(status); }

	uint32_t get_features(uint32_t selection) override {
		return call<Rpc_get_features>(selection); }

	void set_features(uint32_t selection, uint32_t features) override {
		call<Rpc_set_features>(selection, features); }

	uint32_t read_config(uint8_t offset, Access_size size) override {
		return call<Rpc_read_config>(offset, size); }

	void write_config(uint8_t offset, Access_size size, uint32_t value) override {
		call<Rpc_write_config>(offset, size, value); }

	uint8_t get_config_generation() override {
		return call<Rpc_get_config_generation>(); }

	uint16_t get_max_queue_size(uint16_t index) override {
		return call<Rpc_get_max_queue_size>(index); }

	bool configure_queue(uint16_t          queue_index,
	                     Queue_description desc) override {
		return call<Rpc_configure_queue>(queue_index, desc); }

	Genode::Irq_session_capability irq() override {
		return call<Rpc_irq>(); }

	uint32_t read_isr() override { return call<Rpc_read_isr>(); }

	void notify_buffers_available(uint16_t queue_index) override {
		call<Rpc_notify_buffers_available>(queue_index); }
};
