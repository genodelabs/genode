/*
 * \brief  Module for splitting unaligned/uneven I/O requests
 * \author Martin Stein
 * \author Josef Soentgen
 * \date   2023-09-11
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _TRESOR__IO_SPLITTER_H_
#define _TRESOR__IO_SPLITTER_H_

/* tresor includes */
#include <tresor/superblock_control.h>

namespace Tresor { class Splitter; }

struct Tresor::Splitter : Noncopyable
{
	public:

		class Read : Noncopyable
		{
			public:

				using Module = Splitter;

				struct Attr
				{
					Request_offset const in_off;
					Generation const in_gen;
					char *const in_buf_start;
					size_t const in_buf_num_bytes;
				};

				struct Execute_attr
				{
					Superblock_control &sb_control;
					Virtual_block_device &vbd;
					Client_data_interface &client_data;
					Block_io &block_io;
					Crypto &crypto;
				};

			private:

				enum State {
					INIT, COMPLETE, READ_FIRST_VBA, READ_FIRST_VBA_SUCCEEDED, READ_LAST_VBA, READ_LAST_VBA_SUCCEEDED,
					READ_MIDDLE_VBAS, READ_MIDDLE_VBAS_SUCCEEDED };

				Request_helper<Read, State> _helper;
				Attr const _attr;
				addr_t _curr_off { };
				addr_t _curr_buf_addr { };
				Block _blk  { };
				Generation _gen { };
				Constructible<Superblock_control::Read_vbas> _read_vbas { };

				Virtual_block_address _curr_vba() const { return (Virtual_block_address)(_curr_off / BLOCK_SIZE); }

				void _generate_read(State target_state, bool &progress)
				{
					Number_of_blocks num_blocks =
						target_state == READ_MIDDLE_VBAS ? _num_remaining_bytes() / BLOCK_SIZE : 1;

					_read_vbas.construct(Superblock_control::Read_vbas::Attr{_curr_vba(), num_blocks, 0, 0});
					_helper.state = target_state;
					progress = true;
				}

				bool _execute_read(State succeeded_state, Execute_attr const &attr)
				{
					bool progress = attr.sb_control.execute(*_read_vbas, attr.vbd, attr.client_data, attr.block_io, attr.crypto);
					if (_read_vbas->complete()) {
						if (_read_vbas->success())
							_helper.generated_req_succeeded(succeeded_state, progress);
						else
							_helper.generated_req_failed(progress);
					}
					return progress;
				}

				addr_t _curr_buf_off() const
				{
					ASSERT(_curr_off >= _attr.in_off && _curr_off <= _attr.in_off + _attr.in_buf_num_bytes);
					return _curr_off - _attr.in_off;
				}

				addr_t _num_remaining_bytes() const
				{
					ASSERT(_curr_off >= _attr.in_off && _curr_off <= _attr.in_off + _attr.in_buf_num_bytes);
					return _attr.in_off + _attr.in_buf_num_bytes - _curr_off;
				}

				void _advance_curr_off(size_t advance, bool &progress)
				{
					_curr_off += advance;
					if (!_num_remaining_bytes()) {
						_helper.mark_succeeded(progress);
					} else if (_curr_off % BLOCK_SIZE) {
						_curr_buf_addr = (addr_t)&_blk;
						_generate_read(READ_FIRST_VBA, progress);
					} else if (_num_remaining_bytes() < BLOCK_SIZE) {
						_curr_buf_addr = (addr_t)&_blk;
						_generate_read(READ_LAST_VBA, progress);
					} else {
						_curr_buf_addr = (addr_t)_attr.in_buf_start + _curr_buf_off();
						_generate_read(READ_MIDDLE_VBAS, progress);
					}
				}

			public:

				Read(Attr const &attr) : _helper(*this), _attr(attr) { }

				void print(Output &out) const { Genode::print(out, "read"); }

				bool execute(Execute_attr const &attr)
				{
					bool progress = false;
					switch (_helper.state) {
					case INIT:

						_gen = _attr.in_gen;
						_advance_curr_off(_attr.in_off, progress);
						break;

					case READ_FIRST_VBA: progress |= _execute_read(READ_FIRST_VBA_SUCCEEDED, attr); break;
					case READ_FIRST_VBA_SUCCEEDED:
					{
						size_t num_outside_bytes { _curr_off % BLOCK_SIZE };
						size_t num_inside_bytes { min(_num_remaining_bytes(), BLOCK_SIZE - num_outside_bytes) };
						memcpy(_attr.in_buf_start, (void *)((addr_t)&_blk + num_outside_bytes), num_inside_bytes);
						_advance_curr_off(num_inside_bytes, progress);
						break;
					}
					case READ_MIDDLE_VBAS: progress |= _execute_read(READ_MIDDLE_VBAS_SUCCEEDED, attr); break;
					case READ_MIDDLE_VBAS_SUCCEEDED:

						_advance_curr_off((_num_remaining_bytes() / BLOCK_SIZE) * BLOCK_SIZE, progress);
						break;

					case READ_LAST_VBA: progress |= _execute_read(READ_LAST_VBA_SUCCEEDED, attr); break;
					case READ_LAST_VBA_SUCCEEDED:

						memcpy((void *)((addr_t)_attr.in_buf_start + _curr_buf_off()), &_blk, _num_remaining_bytes());
						_advance_curr_off(_num_remaining_bytes(), progress);
						break;

					default: break;
					}
					return progress;
				}

				Block &destination_buffer(Virtual_block_address vba)
				{
					return *(Block *)(_curr_buf_addr + (vba - _curr_vba()) * BLOCK_SIZE);
				}

				bool complete() const { return _helper.complete(); }
				bool success() const { return _helper.success(); }
		};

		class Write : Noncopyable
		{
			public:

				using Module = Splitter;

				struct Attr
				{
					Request_offset const in_off;
					Generation const in_gen;
					char const *const in_buf_start;
					size_t const in_buf_num_bytes;
				};

				struct Execute_attr
				{
					Superblock_control &sb_control;
					Virtual_block_device &vbd;
					Client_data_interface &client_data;
					Block_io &block_io;
					Free_tree &free_tree;
					Meta_tree &meta_tree;
					Crypto &crypto;
				};

			private:

				enum State {
					INIT, COMPLETE, READ_FIRST_VBA, READ_FIRST_VBA_SUCCEEDED, READ_LAST_VBA, READ_LAST_VBA_SUCCEEDED,
					WRITE_FIRST_VBA, WRITE_FIRST_VBA_SUCCEEDED, WRITE_LAST_VBA, WRITE_LAST_VBA_SUCCEEDED,
					WRITE_MIDDLE_VBAS, WRITE_MIDDLE_VBAS_SUCCEEDED };


				Request_helper<Write, State> _helper;
				Attr const _attr;
				addr_t _curr_off { };
				addr_t _curr_buf_addr { };
				Block _blk  { };
				Generation _gen { };
				Constructible<Superblock_control::Read_vbas> _read_vbas { };
				Constructible<Superblock_control::Write_vbas> _write_vbas { };

				Virtual_block_address _curr_vba() const { return (Virtual_block_address)(_curr_off / BLOCK_SIZE); }

				addr_t _curr_buf_off() const
				{
					ASSERT(_curr_off >= _attr.in_off && _curr_off <= _attr.in_off + _attr.in_buf_num_bytes);
					return _curr_off - _attr.in_off;
				}

				addr_t _num_remaining_bytes() const
				{
					ASSERT(_curr_off >= _attr.in_off && _curr_off <= _attr.in_off + _attr.in_buf_num_bytes);
					return _attr.in_off + _attr.in_buf_num_bytes - _curr_off;
				}

				void _generate_sb_control_request(State target_state, bool &progress)
				{
					Number_of_blocks num_blocks =
						target_state == WRITE_MIDDLE_VBAS ? _num_remaining_bytes() / BLOCK_SIZE : 1;

					switch (target_state) {
					case READ_FIRST_VBA:
					case READ_LAST_VBA: _read_vbas.construct(Superblock_control::Read_vbas::Attr{_curr_vba(), num_blocks, 0, 0}); break;
					case WRITE_FIRST_VBA:
					case WRITE_MIDDLE_VBAS:
					case WRITE_LAST_VBA: _write_vbas.construct(Superblock_control::Write_vbas::Attr{_curr_vba(), num_blocks, 0, 0}); break;
					default: ASSERT_NEVER_REACHED;
					}
					_helper.state = target_state;
					progress = true;
				}

				void _advance_curr_off(size_t advance, bool &progress)
				{
					_curr_off += advance;
					if (!_num_remaining_bytes()) {
						_helper.mark_succeeded(progress);
					} else if (_curr_off % BLOCK_SIZE) {
						_curr_buf_addr = (addr_t)&_blk;
						_generate_sb_control_request(READ_FIRST_VBA, progress);
					} else if (_num_remaining_bytes() < BLOCK_SIZE) {
						_curr_buf_addr = (addr_t)&_blk;
						_generate_sb_control_request(READ_LAST_VBA, progress);
					} else {
						_curr_buf_addr = (addr_t)_attr.in_buf_start + _curr_buf_off();
						_generate_sb_control_request(WRITE_MIDDLE_VBAS, progress);
					}
				}

				bool _execute_read(State succeeded_state, Execute_attr const &attr)
				{
					bool progress = attr.sb_control.execute(*_read_vbas, attr.vbd, attr.client_data, attr.block_io, attr.crypto);
					if (_read_vbas->complete()) {
						if (_read_vbas->success())
							_helper.generated_req_succeeded(succeeded_state, progress);
						else
							_helper.generated_req_failed(progress);
					}
					return progress;
				}

				bool _execute_write(State succeeded_state, Execute_attr const &attr)
				{
					bool progress = attr.sb_control.execute(*_write_vbas, attr.vbd, attr.client_data, attr.block_io, attr.free_tree, attr.meta_tree, attr.crypto);
					if (_write_vbas->complete()) {
						if (_write_vbas->success())
							_helper.generated_req_succeeded(succeeded_state, progress);
						else
							_helper.generated_req_failed(progress);
					}
					return progress;
				}

			public:

				Write(Attr const &attr) : _helper(*this), _attr(attr) { }

				void print(Output &out) const { Genode::print(out, "write"); }

				bool execute(Execute_attr const &attr)
				{
					bool progress = false;
					switch (_helper.state) {
					case INIT:

						_gen = _attr.in_gen;
						_advance_curr_off(_attr.in_off, progress);
						break;

					case READ_FIRST_VBA: progress |= _execute_read(READ_FIRST_VBA_SUCCEEDED, attr); break;
					case READ_FIRST_VBA_SUCCEEDED:
					{
						size_t num_outside_bytes { _curr_off % BLOCK_SIZE };
						size_t num_inside_bytes { min(_num_remaining_bytes(), BLOCK_SIZE - num_outside_bytes) };
						memcpy((void *)((addr_t)&_blk + num_outside_bytes), _attr.in_buf_start, num_inside_bytes);
						_curr_buf_addr = (addr_t)&_blk;
						_generate_sb_control_request(WRITE_FIRST_VBA, progress);
						break;
					}
					case WRITE_FIRST_VBA: progress |= _execute_write(WRITE_FIRST_VBA_SUCCEEDED, attr); break;
					case WRITE_FIRST_VBA_SUCCEEDED:
					{
						size_t num_outside_bytes { _curr_off % BLOCK_SIZE };
						size_t num_inside_bytes { min(_num_remaining_bytes(), BLOCK_SIZE - num_outside_bytes) };
						_advance_curr_off(num_inside_bytes, progress);
						break;
					}
					case WRITE_MIDDLE_VBAS: progress |= _execute_write(WRITE_MIDDLE_VBAS_SUCCEEDED, attr); break;
					case WRITE_MIDDLE_VBAS_SUCCEEDED:

						_advance_curr_off((_num_remaining_bytes() / BLOCK_SIZE) * BLOCK_SIZE, progress);
						break;

					case READ_LAST_VBA: progress |= _execute_read(READ_LAST_VBA_SUCCEEDED, attr); break;
					case READ_LAST_VBA_SUCCEEDED:

						memcpy(&_blk, (void *)((addr_t)_attr.in_buf_start + _curr_buf_off()), _num_remaining_bytes());
						_curr_buf_addr = (addr_t)&_blk;
						_generate_sb_control_request(WRITE_LAST_VBA, progress);
						break;

					case WRITE_LAST_VBA: progress |= _execute_write(WRITE_LAST_VBA_SUCCEEDED, attr); break;
					case WRITE_LAST_VBA_SUCCEEDED: _advance_curr_off(_num_remaining_bytes(), progress); break;
					default: break;
					}
					return progress;
				}

				Block const &source_buffer(Virtual_block_address vba)
				{
					return *(Block *)(_curr_buf_addr + (vba - _curr_vba()) * BLOCK_SIZE);
				}

				Block &destination_buffer()
				{
					ASSERT(_helper.state == READ_FIRST_VBA || _helper.state == READ_LAST_VBA);
					return _blk;
				}

				bool complete() const { return _helper.complete(); }
				bool success() const { return _helper.success(); }
		};

	private:

		Read *_read_ptr { };
		Write *_write_ptr { };

	public:

		bool execute(Read &req, Read::Execute_attr const &attr)
		{
			if (!_read_ptr && !_write_ptr)
				_read_ptr = &req;

			if (_read_ptr != &req)
				return false;

			bool progress = req.execute(attr);
			if (req.complete())
				_read_ptr = nullptr;

			return progress;
		}

		bool execute(Write &req, Write::Execute_attr const &attr)
		{
			if (!_write_ptr && !_write_ptr)
				_write_ptr = &req;

			if (_write_ptr != &req)
				return false;

			bool progress = req.execute(attr);
			if (req.complete())
				_write_ptr = nullptr;

			return progress;
		}

		Block const &source_buffer(Virtual_block_address vba)
		{
			ASSERT(_write_ptr);
			return _write_ptr->source_buffer(vba);
		}

		Block &destination_buffer(Virtual_block_address vba)
		{
			if (_read_ptr)
				return _read_ptr->destination_buffer(vba);
			if (_write_ptr)
				return _write_ptr->destination_buffer();
			ASSERT_NEVER_REACHED;
		}

		static constexpr char const *name() { return "sb_control"; }
};

#endif /* _TRESOR__IO_SPLITTER_H_ */
