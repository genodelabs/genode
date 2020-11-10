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

#ifndef _CBE_LIBRARY_H_
#define _CBE_LIBRARY_H_

/* Genode includes */
#include <base/stdint.h>
#include <base/output.h>

/* CBE includes */
#include <cbe/types.h>
#include <cbe/spark_object.h>


extern "C" void cbe_cxx_init();
extern "C" void cbe_cxx_final();


namespace Cbe {

	using namespace Genode;

	class Library;

	Genode::uint32_t object_size(Library const &);

} /* namespace Cbe */


class Cbe::Library : public Cbe::Spark_object<353944>
{
	private:

		/*
		 * Ada/SPARK compatible bindings
		 *
		 * Ada functions cannot have out parameters. Hence we call Ada
		 * procedures that return the 'progress' result as last out parameter.
		 */

		void _has_io_request(Request &, Io_buffer::Index &) const;

		void _crypto_add_key_required(Request &, Key &) const;
		void _crypto_remove_key_required(Request &, Key::Id &) const;

		void _crypto_cipher_data_required(Request &, Crypto_plain_buffer::Index &) const;
		void _crypto_plain_data_required(Request &, Crypto_cipher_buffer::Index &) const;

		void _info(Info &) const;

		void _peek_generated_ta_request(Trust_anchor_request &) const;
		void _peek_generated_ta_sb_hash(Trust_anchor_request const &, Hash &) const;
		void _peek_generated_ta_key_value_plaintext(Trust_anchor_request const &, Key_plaintext_value &) const;
		void _peek_generated_ta_key_value_ciphertext(Trust_anchor_request const &, Key_ciphertext_value &) const;

	public:

	Library();

	/**
	 * Get highest virtual-block-address useable by the current active snapshot
	 *
	 * \return  highest addressable virtual-block-address
	 */
	Virtual_block_address max_vba() const;

	/**
	 * Get information about the CBE
	 *
	 * \return  information structure
	 */
	Info info() const
	{
		Info inf { };
		_info(inf);
		return inf;
	}

	void execute(Io_buffer            &io_buf,
	             Crypto_plain_buffer  &crypto_plain_buf,
	             Crypto_cipher_buffer &crypto_cipher_buf);

	/**
	 * Return whether the last call to 'execute' has made progress or not
	 */
	bool execute_progress() const;

	/**
	 * Check if the CBE can accept a new requeust
	 *
	 * \return true if a request can be accepted, otherwise false
	 */
	bool client_request_acceptable() const;

	/**
	 * Submit a new request
	 *
	 * This method must only be called after executing 'request_acceptable'
	 * returned true.
	 *
	 * \param request  block request
	 */
	void submit_client_request(Request const &request, uint32_t id);

	/**
	 * Check for any completed request
	 *
	 * \return a valid block request will be returned if there is an
	 *         completed request, otherwise an invalid one
	 */
	Request peek_completed_client_request() const;

	/**
	 * Drops the completed request
	 *
	 * This method must only be called after executing
	 * 'peek_completed_request' returned a valid request.
	 *
	 */
	void drop_completed_client_request(Request const &req);

	/*
	 * Backend block I/O
	 */

	/**
	 * Submit read request data from the backend block session to the CBE
	 *
	 * The given data will be transfered to the CBE.
	 *
	 * \param  request  reference to the request from the CBE
	 * \param  data     reference to the data associated with the
	 *                  request
	 *
	 * \return  true if the CBE acknowledged the request
	 */
	void io_request_completed(Io_buffer::Index const &data_index,
	                          bool             const  success);

	/**
	 * Return a write request for the backend block session
	 *
	 * \param result  valid request in case the is one pending that
	 *                needs data, otherwise an invalid one is returned
	 */
	Request has_io_request(Io_buffer::Index &data_index) const
	{
		Request result { };
		_has_io_request(result, data_index);
		return result;
	}
	void has_io_request(Request &req, Io_buffer::Index &data_index) const
	{
		_has_io_request(req, data_index);
	}

	/**
	 * Obtain data for write request for the backend block session
	 *
	 * The CBE will transfer the payload to the given data.
	 *
	 * \param  request  reference to the Block::Request processed
	 *                  by the CBE
	 * \param  data     reference to the data associated with the
	 *                  Request
	 *
	 * \return  true if the CBE could process the request
	 */
	void io_request_in_progress(Io_buffer::Index const &data_index);

	void client_transfer_read_data_required(Request &,
	                                        uint64_t &,
	                                        Crypto_plain_buffer::Index &) const;

	void client_transfer_read_data_in_progress(Crypto_plain_buffer::Index const &);

	void client_transfer_read_data_completed(Crypto_plain_buffer::Index const &, bool);

	void client_transfer_write_data_required(Request &,
	                                         uint64_t &,
	                                         Crypto_plain_buffer::Index &) const;

	void client_transfer_write_data_in_progress(Crypto_plain_buffer::Index const &);

	void client_transfer_write_data_completed(Crypto_plain_buffer::Index const &, bool);

	/**
	 * Query list of active snapshots
	 *
	 * \param  ids  reference to destination buffer
	 */
	void active_snapshot_ids(Active_snapshot_ids &ids) const;

	Request crypto_add_key_required(Key &key) const
	{
		Request result { };
		_crypto_add_key_required(result, key);
		return result;
	}

	void crypto_add_key_requested(Request const &req);

	void crypto_add_key_completed(Request const &req);

	Request crypto_remove_key_required(Key::Id &key_id) const
	{
		Request result { };
		_crypto_remove_key_required(result, key_id);
		return result;
	}

	void crypto_remove_key_requested(Request const &req);

	void crypto_remove_key_completed(Request const &req);

	/**
	 * CBE requests encrytion
	 *
	 * \param result  valid request in case the is one pending that
	 *                needs encrytion, otherwise an invalid one is
	 *                returned
	 */
	Request crypto_cipher_data_required(Crypto_plain_buffer::Index &data_index) const
	{
		Request result { };
		_crypto_cipher_data_required(result, data_index);
		return result;
	}

	/**
	 *  Return plain data for given encryption request
	 *
	 * \param  request  reference to the Block::Request processed
	 *                  by the CBE
	 * \param  data     reference to the data associated with the
	 *                  Block::Request
	 */
	void crypto_cipher_data_requested(
		Crypto_plain_buffer::Index const &data_index);

	/**
	 *  Collect cipher data for given completed encryption request
	 *
	 * \param  request  reference to the Block::Request processed
	 *                  by the CBE
	 * \param  data     reference to the data associated with the
	 *                  Block::Request
	 *
	 * \return  true if the CBE could obtain the encrypted data,
	 *          otherwise false
	 */
	void supply_crypto_cipher_data(Crypto_cipher_buffer::Index const &data_index,
	                               bool                        const  data_valid);

	/**
	 * CBE requests decryption
	 *
	 * \param result  valid request in case the is one pending that
	 *                needs decrytion, otherwise an invalid one is
	 *                returned
	 */
	Request crypto_plain_data_required(Crypto_cipher_buffer::Index &data_index) const
	{
		Request result { };
		_crypto_plain_data_required(result, data_index);
		return result;
	}

	/**
	 *  Return cipher data for given decryption request
	 *
	 * \param  request  reference to the Block::Request processed
	 *                  by the CBE
	 * \param  data     reference to the data associated with the
	 *                  Block::Request
	 *
	 * \return  true if the CBE could supply the ciphr data,
	 *          otherwise false
	 */
	void crypto_plain_data_requested(
		Crypto_cipher_buffer::Index const &data_index);

	/**
	 *  Collect plain data for given completed decryption request
	 *
	 * \param  request  reference to the Block::Request processed
	 *                  by the CBE
	 * \param  data     reference to the data associated with the
	 *                  Block::Request
	 *
	 * \return  true if the CBE could obtain the decrypted data,
	 *          otherwise false
	 */
	void supply_crypto_plain_data(Crypto_plain_buffer::Index const &data_index,
	                              bool                       const  data_valid);

	/**
	 * CBE trust anchor request
	 *
	 * \return  valid TA request in case there is one pending, otherwise an
	 *          invalid one is returned
	 */
	Trust_anchor_request peek_generated_ta_request() const
	{
		Trust_anchor_request request { };
		_peek_generated_ta_request(request);
		return request;
	}

	/**
	 * Drop generated TA request
	 *
	 * \param  request  reference to the request processed by the TA
	 */
	void drop_generated_ta_request(Trust_anchor_request const &request);

	/**
	 * Peek generated TA superblock hash
	 *
	 * \param  request  reference to the request
	 * \return superblock hash
	 */
	Hash peek_generated_ta_sb_hash(Trust_anchor_request const &request) const
	{
		Cbe::Hash hash { };
		_peek_generated_ta_sb_hash(request, hash);
		return hash;
	}

	/**
	 * Mark generated TA secure superblock request complete
	 *
	 * \param  request  reference to the request completed by the TA
	 */
	void mark_generated_ta_secure_sb_request_complete(Trust_anchor_request const &request);

	/**
	 * Mark generated TA create key request complete
	 *
	 * \param  request  reference to the request completed by the TA
	 * \param  key      reference to the key plaintext created by the TA
	 */
	void mark_generated_ta_create_key_request_complete(Trust_anchor_request const &request,
	                                                   Key_plaintext_value  const &key);

	/**
	 * Peek generated TA key ciphertext
	 *
	 * \param  request  reference to the request
	 * \return key ciphertext
	 */
	Key_ciphertext_value peek_generated_ta_key_value_ciphertext(Trust_anchor_request const &request) const
	{
		Cbe::Key_ciphertext_value ck { };
		_peek_generated_ta_key_value_ciphertext(request, ck);
		return ck;
	}

	/**
	 * Peek generated TA key plaintext
	 *
	 * \param  request  reference to the request
	 * \return key plaintext
	 */
	Key_plaintext_value peek_generated_ta_key_value_plaintext(Trust_anchor_request const &request) const
	{
		Cbe::Key_plaintext_value pk { };
		_peek_generated_ta_key_value_plaintext(request, pk);
		return pk;
	}

	/**
	 * Mark generated TA decrypt key request complete
	 *
	 * \param  request  reference to the request completed by the TA
	 * \param  key      reference to the key plaintext decrypted by the TA
	 */
	void mark_generated_ta_decrypt_key_request_complete(Trust_anchor_request const &reference,
	                                                    Key_plaintext_value  const &key);

	/**
	 * Mark generated TA encrypt key request complete
	 *
	 * \param  request  reference to the request completed by the TA
	 * \param  key      reference to the key ciphertext encrypted by the TA
	 */
	void mark_generated_ta_encrypt_key_request_complete(Trust_anchor_request const &request,
	                                                    Key_ciphertext_value const &key);

	/**
	 * Mark generated TA last superblock hash request complete
	 *
	 * \param  request  reference to the request completed by the TA
	 * \param  hash     reference to the superblock hash stored in the TA
	 */
	void mark_generated_ta_last_sb_hash_request_complete(Trust_anchor_request const &request,
	                                                     Hash                 const &hash);
};

#endif /* _CBE_LIBRARY_H_ */
