/*
 * \brief  Connection to Gpu service
 * \author Josef Soentgen
 * \date   2017-04-28
 */

/*
 * Copyright (C) 2017-2021 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__GPU_SESSION__CONNECTION_H_
#define _INCLUDE__GPU_SESSION__CONNECTION_H_

#include <gpu_session/client.h>
#include <base/connection.h>
#include <base/allocator.h>
#include <base/attached_dataspace.h>

namespace Gpu { struct Connection; }


struct Gpu::Connection : Genode::Connection<Session>, Session_client
{
	/**
	 * Attached GPU information dataspace
	 *
	 * \noapi
	 */
	Genode::Attached_dataspace _info_dataspace;

	/**
	 * Constructor
	 *
	 * \param quota  initial amount of quota used for allocating GPU memory
	 */
	Connection(Genode::Env   &env,
	           Genode::size_t quota = Session::REQUIRED_QUOTA,
	           Label   const &label = Label())
	:
		Genode::Connection<Session>(env, label, Ram_quota { quota }, Args()),
		Session_client(cap()),
		_info_dataspace(env.rm(), info_dataspace())
	{ }

	/**
	 * Get typed pointer to the information dataspace
	 *
	 * \return typed pointer
	 */
	template <typename T>
	T const *attached_info() const
	{
		return _info_dataspace.local_addr<T>();
	}
};

#endif /* _INCLUDE__GPU_SESSION__CONNECTION_H_ */
