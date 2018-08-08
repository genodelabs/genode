/*
 * \brief  Zircon interrupt handling
 * \author Johannes Kliemann
 * \date   2018-07-25
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <util/reconstructible.h>
#include <platform_session/client.h>

#include <zircon/types.h>

#include <zx/irq.h>
#include <zx/static_resource.h>
#include <zx/device.h>

static Genode::Constructible<ZX::Irq<Genode::Irq_connection>> _irq_reg_connection[ZX::IRQ_LINES] = {};
static Genode::Constructible<ZX::Irq<Genode::Irq_session_client>> _irq_reg_client[ZX::IRQ_LINES] = {};

extern "C" {

	zx_status_t zx_interrupt_create(zx_handle_t,
	                                Genode::uint32_t irq,
	                                Genode::uint32_t,
	                                zx_handle_t *irq_handle)
	{
		ZX::Device &dev = ZX::Resource<ZX::Device>::get_component();
		if (irq < ZX::IRQ_LINES){
			if (dev.platform()){
				try{
					_irq_reg_client[irq].construct(ZX::Resource<Genode::Env>::get_component(),
							dev.irq_resource(irq));
					*irq_handle = irq;
					return ZX_OK;
				}catch (...){
					Genode::error("Failed to register for IRQ ", irq);
				}
			}else{
				if (!_irq_reg_connection[irq].constructed()){
					_irq_reg_connection[irq].construct(ZX::Resource<Genode::Env>::get_component(), irq);
					*irq_handle = irq;
					return ZX_OK;
				}
			}
		}
		return ZX_ERR_NO_RESOURCES;
	}

	zx_status_t zx_interrupt_wait(zx_handle_t irq, zx_time_t *)
	{
		if (irq < ZX::IRQ_LINES){
			if (ZX::Resource<ZX::Device>::get_component().platform()){
				if (_irq_reg_client[irq].constructed()){
					_irq_reg_client[irq]->wait();
					return ZX_OK;
				}
			}else{
				if (_irq_reg_connection[irq].constructed()){
					_irq_reg_connection[irq]->wait();
					return ZX_OK;
				}
			}
		}
		return ZX_ERR_BAD_HANDLE;
	}

	zx_status_t zx_interrupt_destroy(zx_handle_t irq)
	{
		if (irq < ZX::IRQ_LINES){
			if (ZX::Resource<ZX::Device>::get_component().platform()){
				if (_irq_reg_client[irq].constructed()){
					_irq_reg_client[irq].destruct();
				}
			}else{
				if (_irq_reg_connection[irq].constructed()){
					_irq_reg_connection[irq].destruct();
				}
			}
			return ZX_OK;
		}
		return ZX_ERR_BAD_HANDLE;
	}
}
