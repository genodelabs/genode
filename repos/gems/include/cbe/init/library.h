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

#ifndef _CBE__INIT__LIBRARY_H_
#define _CBE__INIT__LIBRARY_H_

/* CBE includes */
#include <cbe/types.h>
#include <cbe/spark_object.h>


extern "C" void cbe_init_cxx_init();
extern "C" void cbe_init_cxx_final();


namespace Cbe_init {

	class Library;

	Genode::uint32_t object_size(Library const &);

}

struct Cbe_init::Library : Cbe::Spark_object<60960>
{
	/*
	 * Ada/SPARK compatible bindings
	 */

	void _peek_generated_ta_request(Cbe::Trust_anchor_request &) const;
	void _peek_generated_ta_sb_hash(Cbe::Trust_anchor_request const &, Cbe::Hash &) const;
	void _peek_generated_ta_key_value_plaintext(Cbe::Trust_anchor_request const &,
	                                            Cbe::Key_plaintext_value &) const;
	void _peek_generated_ta_key_value_ciphertext(Cbe::Trust_anchor_request const &,
	                                             Cbe::Key_ciphertext_value &) const;

	Library();

	bool client_request_acceptable() const;

	void submit_client_request(Cbe::Request const &request,
	                           Genode::uint64_t    vbd_max_lvl_idx,
	                           Genode::uint64_t    vbd_degree,
	                           Genode::uint64_t    vbd_nr_of_leafs,
	                           Genode::uint64_t    ft_max_lvl_idx,
	                           Genode::uint64_t    ft_degree,
	                           Genode::uint64_t    ft_nr_of_leafs);

	Cbe::Request peek_completed_client_request() const;

	void drop_completed_client_request(Cbe::Request const &req);

	void execute(Cbe::Io_buffer &io_buf);

	bool execute_progress() const;

	void io_request_completed(Cbe::Io_buffer::Index const &data_index,
	                          bool                  const  success);

	void has_io_request(Cbe::Request &, Cbe::Io_buffer::Index &) const;

	void io_request_in_progress(Cbe::Io_buffer::Index const &data_index);

	Cbe::Trust_anchor_request peek_generated_ta_request() const
	{
		Cbe::Trust_anchor_request request { };
		_peek_generated_ta_request(request);
		return request;
	}

	void drop_generated_ta_request(Cbe::Trust_anchor_request const &request);

	Cbe::Hash peek_generated_ta_sb_hash(Cbe::Trust_anchor_request const &request) const
	{
		Cbe::Hash hash { };
		_peek_generated_ta_sb_hash(request, hash);
		return hash;
	}

	void mark_generated_ta_secure_sb_request_complete(Cbe::Trust_anchor_request const &request);

	void mark_generated_ta_create_key_request_complete(Cbe::Trust_anchor_request const &request,
	                                                   Cbe::Key_plaintext_value  const &key);

	Cbe::Key_ciphertext_value peek_generated_ta_key_value_ciphertext(Cbe::Trust_anchor_request const &request) const
	{
		Cbe::Key_ciphertext_value ck { };
		_peek_generated_ta_key_value_ciphertext(request, ck);
		return ck;
	}

	Cbe::Key_plaintext_value peek_generated_ta_key_value_plaintext(Cbe::Trust_anchor_request const &request) const
	{
		Cbe::Key_plaintext_value pk { };
		_peek_generated_ta_key_value_plaintext(request, pk);
		return pk;
	}

	void mark_generated_ta_decrypt_key_request_complete(Cbe::Trust_anchor_request const &reference,
	                                                    Cbe::Key_plaintext_value  const &key);

	void mark_generated_ta_encrypt_key_request_complete(Cbe::Trust_anchor_request const &request,
	                                                    Cbe::Key_ciphertext_value const &key);

	void mark_generated_ta_last_sb_hash_request_complete(Cbe::Trust_anchor_request const &,
	                                                     Cbe::Hash                 const &)
	{
		struct Not_supported { };
		throw Not_supported();
	}
};

#endif /* _CBE__INIT__LIBRARY_H_ */
