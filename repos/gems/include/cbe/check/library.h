/*
 * \brief  Integration of the Consistent Block Encrypter (CBE)
 * \author Martin Stein
 * \author Josef Soentgen
 * \date   2020-11-10
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CBE__CHECK__LIBRARY_H_
#define _CBE__CHECK__LIBRARY_H_

/* CBE includes */
#include <cbe/types.h>
#include <cbe/spark_object.h>


extern "C" void cbe_check_cxx_init();
extern "C" void cbe_check_cxx_final();


namespace Cbe_check {

	class Library;

	Genode::uint32_t object_size(Library const &);

}

struct Cbe_check::Library : Cbe::Spark_object<46160>
{
	Library();

	bool client_request_acceptable() const;

	void submit_client_request(Cbe::Request const &request);

	Cbe::Request peek_completed_client_request() const;

	void drop_completed_client_request(Cbe::Request const &req);

	void execute(Cbe::Io_buffer const &io_buf);

	bool execute_progress() const;

	void io_request_completed(Cbe::Io_buffer::Index const &data_index,
	                          bool                  const  success);

	void has_io_request(Cbe::Request &, Cbe::Io_buffer::Index &) const;

	void io_request_in_progress(Cbe::Io_buffer::Index const &data_index);
};

#endif /* _CBE__CHECK__LIBRARY_H_ */
