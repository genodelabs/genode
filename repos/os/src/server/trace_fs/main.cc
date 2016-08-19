/*
 * \brief  Trace file system
 * \author Josef Soentgen
 * \date   2014-01-15
 */

/*
 * Copyright (C) 2014-2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <file_system/node_handle_registry.h>
#include <cap_session/connection.h>
#include <file_system_session/rpc_object.h>
#include <os/attached_rom_dataspace.h>
#include <os/config.h>
#include <os/server.h>
#include <os/session_policy.h>
#include <root/component.h>
#include <base/heap.h>
#include <timer_session/connection.h>
#include <trace_session/connection.h>
#include <util/list.h>
#include <util/string.h>
#include <util/xml_node.h>
#include <file_system/util.h>

/* local includes */
#include <buffer.h>
#include <directory.h>
#include <followed_subject.h>
#include <trace_files.h>


/**
 * Return true if 'str' is a valid file name
 */
static inline bool valid_filename(char const *str)
{
	if (!str) return false;

	/* must have at least one character */
	if (str[0] == 0) return false;

	/* must not contain '/' or '\' or ':' */
	if (File_system::string_contains(str, '/') ||
		File_system::string_contains(str, '\\') ||
		File_system::string_contains(str, ':'))
		return false;

	return true;
}

/**
 * This class updates the file system
 *
 * In this context updating means creating the files and directories if
 * needed, refreshing their content or deleting them if they are no
 * longer of any use.
 */
class Trace_file_system
{
	private:

		/* local abbreviations */
		typedef Genode::size_t size_t;

		typedef Genode::Trace::Subject_id   Subject_id;
		typedef Genode::Trace::Subject_info Subject_info;
		typedef Genode::Trace::Connection   Trace;

		typedef Trace_fs::Followed_subject_registry Followed_subject_registry;
		typedef Trace_fs::Followed_subject Followed_subject;

		typedef File_system::Directory Directory;
		typedef File_system::Node      Node;


		/**
		 * Simple node list
		 *
		 * This list is used to temporarily store pointers to all nodes
		 * needed for representing a trace subject in the file system when
		 * creating or cleaning up the file system hierachie.
		 */
		class Node_list
		{
			private:

				/**
				 * Node list element class
				 *
				 * A element only contains a pointer to the actual node.
				 */
				struct Node_list_entry : public Genode::List<Node_list_entry>::Element
				{
					File_system::Node *node;

					Node_list_entry(File_system::Node *n) : node(n) { }
				};

				Genode::Allocator &_md_alloc;

				Genode::List<Node_list_entry> _list;

			public:

				Node_list(Genode::Allocator &md_alloc) : _md_alloc(md_alloc) { }

				/**
				 * Free all memory automatically if the object goes out of
				 * scope or rather is deleted.
				 */
				~Node_list()
				{
					for (Node_list_entry *e = _list.first(); e; ) {
						Node_list_entry *cur = e;
						e = e->next();

						_list.remove(cur);
						destroy(&_md_alloc, cur);
					}
				}

				/**
				 * Insert a node in the list
				 *
				 * \param node pointer to node
				 */
				void push(Node *node)
				{
					Node_list_entry *e = new (&_md_alloc) Node_list_entry(node);
					_list.insert(e);
				}

				/**
				 * Remove the first node from the list
				 *
				 * The first element will be removed from the list and the node
				 * is returned.
				 *
				 * \return pointer to node or 0
				 */
				Node *pop()
				{
					Node_list_entry *e = _list.first();
					if (e) {
						Node *node = e->node;

						_list.remove(e);
						destroy(&_md_alloc, e);

						return node;
					}

					return 0;
				}

				/**
				 * Return the node pointer of the first element in the list
				 *
				 * This method only returns the pointer but leaves the list
				 * element in good order.
				 *
				 * \return pointer to node of the first element or 0
				 */
				Node *first()
				{
					Node_list_entry *e = _list.first();
					return e ? e->node : 0;
				}
		};


		/**
		 * This class implements the Process_entry functor class
		 *
		 * It is needed by the Trace_buffer_manager to process a entry
		 * from the Trace::Buffer.
		 */
		template <size_t CAPACITY>
		class Process_entry : public Followed_subject::Trace_buffer_manager::Process_entry
		{
			private:

				char   _buf[CAPACITY];
				size_t _length;


			public:

				Process_entry() : _length(0) { _buf[0] = 0; }

				/**
				 * Return capacity of the internal buffer
				 *
				 * \return capacity of the buffer in bytes
				 */
				size_t capacity() const { return CAPACITY; }

				/**
				 * Return data of the processed Trace::Buffer::Entry
				 *
				 * \return pointer to data
				 */
				char const *data() const { return _buf; }

				/**
				 * Functor for processing a Trace:Buffer::Entry
				 *
				 * \param entry reference of Trace::Buffer::Entry
				 *
				 * \return length of processed Trace::Buffer::Entry
				 */
				Genode::size_t operator()(Genode::Trace::Buffer::Entry &entry)
				{
					Genode::size_t len = Genode::min(entry.length() + 1, CAPACITY);
					Genode::memcpy(_buf, entry.data(), len);
					_buf[len - 1] = '\n';

					_length = len;

					return len;
				}
		};


		Genode::Allocator         &_alloc;
		Genode::Trace::Connection &_trace;
		Directory                 &_root_dir;

		size_t                     _buffer_size;
		size_t                     _buffer_size_max;

		Followed_subject_registry  _followed_subject_registry;


		/**
		 * Cast Node pointer to Directory pointer
		 *
		 * \param node pointer to node
		 */
		Directory *_node_to_directory(Node *node)
		{
			return dynamic_cast<Directory*>(node);
		}

		/**
		  * Gather recent trace events
		  *
		  * \param subject pointer to subject
		  */
		void _gather_events(Followed_subject *subject)
		{
			Followed_subject::Trace_buffer_manager *manager = subject->trace_buffer_manager();
			if (!manager)
				return;

			Process_entry<512> process_entry;

			while (!manager->last_entry()) {
				size_t len = manager->dump_entry(process_entry);

				if (len == 0)
					continue;

				try { subject->events_file.append(process_entry.data(), len); }
				catch (...) { Genode::error("could not write entry"); }
			}

			if (manager->last_entry()) {
				manager->rewind();
			}
		}

		/**
		 * Disable tracing of a followed subject
		 *
		 * \param subject pointer to subject
		 */
		void _disable_tracing(Followed_subject *subject)
		{
			subject->active_file.set_inactive();

			_trace.pause(subject->id());
			_gather_events(subject);
			try { subject->unmanage_trace_buffer(); }
			catch (...) { Genode::error("trace buffer was not managed"); }
			_trace.free(subject->id());
		}

		/**
		 * Enable tracing of a followed subject
		 *
		 * \param subject pointer to subject
		 */
		void _enable_tracing(Followed_subject *subject)
		{
			try {
				_trace.trace(subject->id().id, subject->policy_id().id,
				             subject->buffer_size_file.size());

				try { subject->manage_trace_buffer(_trace.buffer(subject->id())); }
				catch (...) { Genode::error("trace buffer is already managed"); }

				subject->active_file.set_active();
			} catch (...) { Genode::error("could not enable tracing"); }
		}

		/**
		 * Search recursively the parent node for the corresponding label
		 *
		 * \param list list of traversed nodes
		 * \param walker label walking object
		 * \param parent parent node of the current directory
		 *
		 * \return parent node for the given label
		 */
		Directory* _find_parent_node(Node_list &list, Util::Label_walker &walker,
		                                          Directory &parent)
		{
			char const *remainder = walker.next();

			Directory *child;

			try { child = _node_to_directory(parent.lookup(walker.element())); }
			catch (File_system::Lookup_failed) {
				try {
					child = new (&_alloc) Directory(walker.element());
					parent.adopt_unsynchronized(child);
				} catch (...) {
					Genode::error("could not create '", walker.element(), "'");
					return 0;
				}
			}

			list.push(child);

			if (!*remainder) {
				return child;
			} else {
				return _find_parent_node(list, walker, *child);
			}
		}

		/**
		 * Remove unsused nodes and free the memory
		 *
		 * All nodes are removed from the list and discarded from their
		 * parent until we hit a node or rather a parent that still has
		 * child nodes. In this case we stop. The remaining entries in
		 * the node list are freed when the Node_list is deleted.
		 *
		 * \param list reference to list
		 */
		void _remove_nodes(Node_list &list)
		{
			while (Node *child = list.pop()) {
				Node *parent = list.first();

				Directory *dir = dynamic_cast<Directory*>(child);
				if (dir->num_entries() == 0) {

					if (parent) {
						Directory *dir = dynamic_cast<Directory*>(parent);
						if (!(Genode::strcmp(dir->name(), child->name()) == 0))
							dir->discard_unsynchronized(child);
					}

					destroy(&_alloc, dir);
				} else {
					/* do not bother any further, node has other children */
					break;
				}
			}
		}


	public:

		/**
		 * Constructor
		 */
		Trace_file_system(Genode::Allocator &alloc,
		                  Trace             &trace,
		                  Directory         &root_dir,
		                  size_t             buffer_size,
		                  size_t             buffer_size_max)
		:
			_alloc(alloc), _trace(trace), _root_dir(root_dir),
			_buffer_size(buffer_size), _buffer_size_max(buffer_size_max),
			_followed_subject_registry(_alloc)
		{ }

		/**
		 * Handle the change of the content of a node
		 *
		 * XXX This method could be made much simpler if a Node would
		 *     store the subject_id and could be used to make the lookup.
		 */
		void handle_changed_node(Node *node)
		{
			Followed_subject *subject = 0;
			bool policy_changed       = false;

			using namespace File_system;

			/**
			 * It is enough to invoke acknowledge_change() on the Cleanup_file.
			 * Therefore here is nothing to be done.
			 */
			Cleanup_file *cleanup_file = dynamic_cast<Cleanup_file*>(node);
			if (cleanup_file) {
				return;
			}

			Policy_file *policy_file = dynamic_cast<Policy_file*>(node);
			if (policy_file) {
				try {
					subject = _followed_subject_registry.lookup(policy_file->id());
					size_t policy_length = policy_file->length();

					/* policy was changed, unload old one first */
					if (subject->policy_valid()) {
						_trace.unload_policy(subject->policy_id());

						subject->invalidate_policy();
					}

					/**
					 * Copy the new policy only if it may containg something useful.
					 * XXX: It might be better to check for a more reaseonable length.
					 */
					if (policy_length > 0) {
						try {
							Genode::Trace::Policy_id id = _trace.alloc_policy(policy_length);

							Genode::Dataspace_capability ds_cap = _trace.policy(id);
							if (ds_cap.valid()) {
								void *ram = Genode::env()->rm_session()->attach(ds_cap);
								size_t n = policy_file->read((char *)ram, policy_length, 0UL);

								if (n != policy_length) {
									Genode::error("error while copying policy content");
								} else { subject->policy_id(id); }

								Genode::env()->rm_session()->detach(ram);
							}
						} catch (...) { Genode::error("could not allocate policy"); }
					}

					policy_changed = true;

				} catch (Trace_fs::Followed_subject_registry::Invalid_subject) { }
			}

			Enable_file *enable_file = dynamic_cast<Enable_file*>(node);
			if (enable_file) {
				try { subject = _followed_subject_registry.lookup(enable_file->id()); }
				catch (Trace_fs::Followed_subject_registry::Invalid_subject) { }
			}

			/**
			 * Perform the action which was originally intended. This is actually
			 * safe because at this point we got invoked by either the Enable_file
			 * or Policy_file file.
			 */
			Subject_info info = _trace.subject_info(subject->id());
			Subject_info::State state = info.state();

			/* tracing already enabled but policy has changed */
			if (subject->enable_file.enabled() && policy_changed) {
				/* disable tracing first */
				if (state == Subject_info::State::TRACED)
					_disable_tracing(subject);

				/* reenable only if the policy is actually valid */
				if (subject->policy_valid())
					_enable_tracing(subject);
			}
			/* subject is untraced but tracing is now enabled */
			else if (subject->enable_file.enabled() &&
			           (state == Subject_info::State::UNTRACED)) {
				if (subject->policy_valid())
					_enable_tracing(subject);
			}
			/* subject is traced but tracing is now disabled */
			else if (!subject->enable_file.enabled() &&
			           (state == Subject_info::State::TRACED)) {
				_disable_tracing(subject);
			}
		}

		/**
		 * Update the trace subjects
		 *
		 * \param subject_limit limit the number of trace subjects
		 */
		void update(int subject_limit)
		{
			Genode::Trace::Subject_id  subjects[subject_limit];

			size_t num_subjects = _trace.subjects(subjects, subject_limit);

			/* traverse current trace subjects */
			for (size_t i = 0; i < num_subjects; i++) {
				Subject_info info = _trace.subject_info(subjects[i]);
				Subject_info::State state = info.state();

				/* opt-out early */
				switch (state) {
				case Subject_info::State::INVALID:
				case Subject_info::State::FOREIGN:
				case Subject_info::State::ERROR:
					continue;
					break; /* never reached */
				default:
					break;
				}

				Followed_subject *followed_subject;

				try {
					/* old subject found */
					followed_subject = _followed_subject_registry.lookup(subjects[i]);

					/**
					 * Update content of the corresponding events file
					 */
					if (state == Subject_info::State::TRACED) {
						_gather_events(followed_subject);
						continue;
					}

					/**
					 * The subject is either UNTRACED or DEAD in which case we want to remove
					 * the nodes if they are marked for deletion.
					 */
					if (followed_subject->marked_for_cleanup() ||
						(!followed_subject->was_traced() && state == Subject_info::State::DEAD)) {
						char const *label = info.session_label().string();

						Node_list list(_alloc);
						Util::Label_walker walker(label);

						Directory *parent = _find_parent_node(list, walker, _root_dir);
						if (!parent) {
							Genode::error("could not find parent node for label:'", label, "'");
							continue;
						}

						parent->discard_unsynchronized(followed_subject);
						_remove_nodes(list);
						_followed_subject_registry.free(followed_subject);
						continue;
					}
				} catch (Trace_fs::Followed_subject_registry::Invalid_subject) {
					/* ignore unknown but already dead subject */
					if (state == Subject_info::State::DEAD)
						continue;

					/* new subject */
					char const *label = info.session_label().string();
					char const *name  = info.thread_name().string();

					Util::Buffer<64> subject_dir_name;
					Genode::snprintf(subject_dir_name.data(), subject_dir_name.capacity(),
					                 "%s.%d", name, subjects[i].id);

					subject_dir_name.replace('/', '_');

					followed_subject = _followed_subject_registry.alloc(subject_dir_name.data(),
					                                                    subjects[i]);

					/* set trace buffer size */
					followed_subject->buffer_size_file.size_limit(_buffer_size_max);
					followed_subject->buffer_size_file.size(_buffer_size);

					Node_list list(_alloc);
					Util::Label_walker walker(label);
					Directory *parent = _find_parent_node(list, walker, _root_dir);
					if (!parent) {
						Genode::error("could not find parent node on creation");
						continue;
					}

					parent->adopt_unsynchronized(followed_subject);
				}
			}
		}
};


namespace File_system {
	struct Main;
	struct Session_component;
	struct Root;
}


class File_system::Session_component : public Session_rpc_object
{
	private:

		Server::Entrypoint   &_ep;
		Allocator            &_md_alloc;
		Directory            &_root_dir;
		Node_handle_registry  _handle_registry;
		bool                  _writeable;

		unsigned              _subject_limit;
		unsigned              _poll_interval;

		Timer::Connection    _fs_update_timer;

		Trace::Connection    *_trace;
		Trace_file_system    *_trace_fs;

		Signal_rpc_member<Session_component> _process_packet_dispatcher;
		Signal_rpc_member<Session_component> _fs_update_dispatcher;


		/**************************
		 ** File system updating **
		 **************************/

		/**
		 * Update the file system hierarchie and data of active trace subjects
		 */
		void _fs_update(unsigned)
		{
			_trace_fs->update(_subject_limit);
		}

		/******************************
		 ** Packet-stream processing **
		 ******************************/

		/**
		 * Perform packet operation
		 */
		void _process_packet_op(Packet_descriptor &packet, Node &node)
		{
			void     * const content = tx_sink()->packet_content(packet);
			size_t     const length  = packet.length();
			seek_off_t const offset  = packet.position();

			if (!content || (packet.length() > packet.size())) {
				packet.succeeded(false);
				return;
			}

			/* resulting length */
			size_t res_length = 0;

			switch (packet.operation()) {

			case Packet_descriptor::READ:
				res_length = node.read((char *)content, length, offset);
				break;

			case Packet_descriptor::WRITE:
				res_length = node.write((char const *)content, length, offset);
				break;
			}

			packet.length(res_length);
			packet.succeeded(res_length > 0);
		}

		void _process_packet()
		{
			Packet_descriptor packet = tx_sink()->get_packet();

			/* assume failure by default */
			packet.succeeded(false);

			try {
				Node *node = _handle_registry.lookup(packet.handle());

				_process_packet_op(packet, *node);
			}
			catch (Invalid_handle)     { Genode::error("Invalid_handle");     }

			/*
			 * The 'acknowledge_packet' function cannot block because we
			 * checked for 'ready_to_ack' in '_process_packets'.
			 */
			tx_sink()->acknowledge_packet(packet);
		}

		/**
		 * Called by signal dispatcher, executed in the context of the main
		 * thread (not serialized with the RPC functions)
		 */
		void _process_packets(unsigned)
		{
			while (tx_sink()->packet_avail()) {

				/*
				 * Make sure that the '_process_packet' function does not
				 * block.
				 *
				 * If the acknowledgement queue is full, we defer packet
				 * processing until the client processed pending
				 * acknowledgements and thereby emitted a ready-to-ack
				 * signal. Otherwise, the call of 'acknowledge_packet()'
				 * in '_process_packet' would infinitely block the context
				 * of the main thread. The main thread is however needed
				 * for receiving any subsequent 'ready-to-ack' signals.
				 */
				if (!tx_sink()->ready_to_ack())
					return;

				_process_packet();
			}
		}

		/**
		 * Check if string represents a valid path (must start with '/')
		 */
		static void _assert_valid_path(char const *path)
		{
			if (!path || path[0] != '/') {
				Genode::warning("malformed path '", path, "'");
				throw Lookup_failed();
			}
		}


	public:

		/**
		 * Constructor
		 */
		Session_component(size_t                 tx_buf_size,
		                  Server::Entrypoint     &ep,
		                  File_system::Directory &root_dir,
		                  Allocator              &md_alloc,
		                  unsigned                subject_limit,
		                  unsigned                poll_interval,
		                  size_t                  trace_quota,
		                  size_t                  trace_meta_quota,
		                  size_t                  trace_parent_levels,
		                  size_t                  buffer_size,
		                  size_t                  buffer_size_max)
		:
			Session_rpc_object(env()->ram_session()->alloc(tx_buf_size), ep.rpc_ep()),
			_ep(ep),
			_md_alloc(md_alloc),
			_root_dir(root_dir),
			_subject_limit(subject_limit),
			_poll_interval(poll_interval),
			_trace(new (&_md_alloc) Genode::Trace::Connection(trace_quota, trace_meta_quota, trace_parent_levels)),
			_trace_fs(new (&_md_alloc) Trace_file_system(_md_alloc, *_trace, _root_dir, buffer_size, buffer_size_max)),
			_process_packet_dispatcher(_ep, *this, &Session_component::_process_packets),
			_fs_update_dispatcher(_ep, *this, &Session_component::_fs_update)
		{
			_tx.sigh_packet_avail(_process_packet_dispatcher);
			_tx.sigh_ready_to_ack(_process_packet_dispatcher);

			/**
			 * Register '_fs_update' dispatch function as signal handler
			 * for polling the trace session.
			 */
			_fs_update_timer.sigh(_fs_update_dispatcher);

			/**
			 * We need to scale _poll_interval because trigger_periodic()
			 * uses usec.
			 */
			_fs_update_timer.trigger_periodic(_poll_interval * 1000);
		}

		/**
		 * Destructor
		 */
		~Session_component()
		{
			destroy(&_md_alloc, _trace_fs);
			destroy(&_md_alloc, _trace);

			Dataspace_capability ds = tx_sink()->dataspace();
			env()->ram_session()->free(static_cap_cast<Ram_dataspace>(ds));
		}


		/***************************
		 ** File_system interface **
		 ***************************/

		File_handle file(Dir_handle dir_handle, Name const &name, Mode mode, bool create)
		{
			if (!valid_filename(name.string()))
				throw Invalid_name();

			Directory *dir = _handle_registry.lookup(dir_handle);

			if (create)
				throw Permission_denied();

			File *file = dynamic_cast<File*>(dir->lookup(name.string()));
			if (!file)
				throw Invalid_name();

			return _handle_registry.alloc(file);
		}

		Symlink_handle symlink(Dir_handle dir_handle, Name const &name, bool create)
		{
			Genode::warning("symlinks not supported");
			return Symlink_handle();
		}

		Dir_handle dir(Path const &path, bool create)
		{
			char const *path_str = path.string();

			_assert_valid_path(path_str);

			if (create)
				throw Permission_denied();

			if (!path.valid_string())
				throw Name_too_long();

			Directory *dir = dynamic_cast<Directory*>(_root_dir.lookup(path_str + 1));
			if (!dir)
				throw Invalid_name();

			return _handle_registry.alloc(dir);
		}

		Node_handle node(Path const &path)
		{
			char const *path_str = path.string();

			_assert_valid_path(path_str);

			Node *node = _root_dir.lookup(path_str + 1);

			return _handle_registry.alloc(node);
		}

		void close(Node_handle handle)
		{
			Node *node;

			try { node = _handle_registry.lookup(handle); }
			catch (Invalid_handle) {
				Genode::error("close() called with invalid handle");
				return;
			}

			/**
			 * Acknowledge the change of the content of files which may be
			 * modified by the user of the file system.
			 */
			Changeable_content *changeable = dynamic_cast<Changeable_content*>(node);
			if (changeable) {
				if (changeable->changed()) {
					changeable->acknowledge_change();

					/* let the trace fs perform the provoked actions */
					_trace_fs->handle_changed_node(node);
				}
			}

			_handle_registry.free(handle);
		}

		Status status(Node_handle node_handle)
		{
			Node *node = _handle_registry.lookup(node_handle);
			return node->status();
		}

		void control(Node_handle, Control) { }
		void unlink(Dir_handle dir_handle, Name const &name) { }

		void truncate(File_handle handle, file_size_t size)
		{
			Node *node;

			try {
				node = _handle_registry.lookup(handle);

				File *file = dynamic_cast<File*>(node);
				if (file) { file->truncate(size); }
			} catch (Invalid_handle) { }
		}

		void move(Dir_handle, Name const &, Dir_handle, Name const &) { }

		void sigh(Node_handle node_handle, Signal_context_capability sigh) {
			_handle_registry.sigh(node_handle, sigh); }
};


class File_system::Root : public Root_component<Session_component>
{
	private:

		Server::Entrypoint &_ep;

		Directory          &_root_dir;

	protected:

		Session_component *_create_session(const char *args)
		{
			/*
			 * Determine client-specific policy defined implicitly by
			 * the client's label.
			 */

			enum { ROOT_MAX_LEN = 256 };
			char root[ROOT_MAX_LEN];
			root[0] = 0;

			/* default settings */
			unsigned interval                        = 1000; /* 1 sec */
			unsigned subject_limit                   = 128;

			Genode::Number_of_bytes trace_quota      =  32 * (1 << 20); /*  32 MiB */
			Genode::Number_of_bytes trace_meta_quota = 256 * (1 << 10); /* 256 KiB */
			Genode::Number_of_bytes buffer_size      =  32 * (1 << 10); /*  32 KiB */
			Genode::Number_of_bytes buffer_size_max  =   1 * (1 << 20); /*   1 MiB */
			unsigned trace_parent_levels             = 0;

			Session_label const label = label_from_args(args);
			try {
				Session_policy policy(label);

				/*
				 * Override default settings with specific session settings by
				 * evaluating the policy.
				 */
				try { policy.attribute("interval").value(&interval);
				} catch (...) { }
				try { policy.attribute("subject_limit").value(&subject_limit);
				} catch (...) { }
				try { policy.attribute("trace_quota").value(&trace_quota);
				} catch (...) { }
				try { policy.attribute("trace_meta_quota").value(&trace_meta_quota);
				} catch (...) { }
				try { policy.attribute("parent_levels").value(&trace_parent_levels);
				} catch (...) { }
				try { policy.attribute("buffer_size").value(&buffer_size);
				} catch (...) { }
				try { policy.attribute("buffer_size_max").value(&buffer_size_max);
				} catch (...) { }

				/*
				 * Determine directory that is used as root directory of
				 * the session.
				 */
				try {
					policy.attribute("root").value(root, sizeof(root));

					/*
					 * Make sure the root path is specified with a
					 * leading path delimiter. For performing the
					 * lookup, we skip the first character.
					 */
					if (root[0] != '/')
						throw Lookup_failed();
				} catch (Xml_node::Nonexistent_attribute) {
					Genode::error("Missing \"root\" attribute in policy definition");
					throw Root::Unavailable();
				} catch (Lookup_failed) {
					Genode::error("session root directory "
					              "\"", Genode::Cstring(root), "\" does not exist");
					throw Root::Unavailable();
				}

			} catch (Session_policy::No_policy_defined) {
				Genode::error("Invalid session request, no matching policy");
				throw Root::Unavailable();
			}

			size_t ram_quota =
				Arg_string::find_arg(args, "ram_quota"  ).ulong_value(0);
			size_t tx_buf_size =
				Arg_string::find_arg(args, "tx_buf_size").ulong_value(0);

			if (!tx_buf_size) {
				Genode::error(label, " requested a session with a zero length transmission buffer");
				throw Root::Invalid_args();
			}

			/*
			 * Check if donated ram quota suffices for session data,
			 * and communication buffer.
			 */
			size_t session_size = sizeof(Session_component) + tx_buf_size;
			if (max((size_t)4096, session_size) > ram_quota) {
				Genode::error("insufficient 'ram_quota', got ", ram_quota, ", "
				              "need ", session_size);
				throw Root::Quota_exceeded();
			}
			return new (md_alloc())
				Session_component(tx_buf_size, _ep, _root_dir, *md_alloc(),
				                  subject_limit, interval, trace_quota,
				                  trace_meta_quota, trace_parent_levels,
				                  buffer_size, buffer_size_max);
		}

	public:

		/**
		 * Constructor
		 *
		 * \param ep          entrypoint
		 * \param sig_rec     signal receiver used for handling the
		 *                    data-flow signals of packet streams
		 * \param md_alloc    meta-data allocator
		 */
		Root(Server::Entrypoint &ep, Allocator &md_alloc, Directory &root_dir)
		:
			Root_component<Session_component>(&ep.rpc_ep(), &md_alloc),
			_ep(ep),
			_root_dir(root_dir)
		{ }
};


struct File_system::Main
{
	Server::Entrypoint &ep;

	Directory           root_dir = { "/" };

	/*
	 * Initialize root interface
	 */
	Sliced_heap sliced_heap = { env()->ram_session(), env()->rm_session() };

	Root fs_root = { ep, sliced_heap, root_dir };

	Main(Server::Entrypoint &ep) : ep(ep)
	{
		env()->parent()->announce(ep.manage(fs_root));
	}
};


/**********************
 ** Server framework **
 **********************/

char const *   Server::name()                            { return "trace_fs_ep"; }
Genode::size_t Server::stack_size()                      { return 32 * 2048 * sizeof(long); }
void           Server::construct(Server::Entrypoint &ep) { static File_system::Main inst(ep); }
