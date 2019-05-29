/*
 * \brief  Component providing a Terminal session via SSH
 * \author Josef Soentgen
 * \author Pirmin Duss
 * \date   2019-05-29
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 * Copyright (C) 2019 gapfruit AG
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SSH_TERMINAL_LOGIN_H_
#define _SSH_TERMINAL_LOGIN_H_

/* Genode includes */
#include <util/string.h>
#include <base/heap.h>

/* libssh includes */
#include <libssh/libssh.h>

/* local includes */
#include "util.h"


namespace Ssh {

	using namespace Genode;
	using namespace Util;

	using User     = String<32>;
	using Password = String<64>;
	using Hash     = String<65>;

	struct Login;
	struct Login_registry;
}


struct Ssh::Login : Genode::Registry<Ssh::Login>::Element
{
	Ssh::User     user         { };
	Ssh::Password password     { };
	Ssh::Hash     pub_key_hash { };
	ssh_key       pub_key      { nullptr };
	bool multi_login           { false };
	bool request_terminal      { false };

	/**
	 * Constructor
	 */
	Login(Genode::Registry<Login> &reg,
	      Ssh::User      const &user,
	      Ssh::Password  const &pw,
	      Filename       const &pk_file,
	      bool           const multi_login,
	      bool           const request_terminal)
	:
		Element(reg, *this),
		user(user), password(pw), multi_login(multi_login),
		request_terminal(request_terminal)
	{
		Libc::with_libc([&] {

			if (pk_file.valid() &&
			    ssh_pki_import_pubkey_file(pk_file.string(), &pub_key)) {
				Genode::error("could not import public key for user '",
				              user, "'");
			}

			if (pub_key) {
				unsigned char *h    = nullptr;
				size_t         hlen = 0;
				/* pray and assume both calls never fail */
				ssh_get_publickey_hash(pub_key, SSH_PUBLICKEY_HASH_SHA256,
				                       &h, &hlen);
				char const *p = ssh_get_fingerprint_hash(SSH_PUBLICKEY_HASH_SHA256,
				                                         h, hlen);
				if (p) {
					pub_key_hash = Ssh::Hash(p);
				}

				ssh_clean_pubkey_hash(&h);

				/* abuse function to free fingerprint */
				ssh_clean_pubkey_hash((unsigned char**)&p);
			}
		}); /* Libc::with_libc */
	}

	virtual ~Login() { ssh_key_free(pub_key); }

	bool auth_password()  const { return password.valid(); }
	bool auth_publickey() const { return pub_key != nullptr; }

	void print(Genode::Output &out) const
	{
		Genode::print(out, "user ", user, ": ");
		if (auth_password())  { Genode::print(out, "password "); }
		if (auth_publickey()) { Genode::print(out, "public-key"); }
	}
};


struct Ssh::Login_registry : Genode::Registry<Ssh::Login>
{
	Genode::Allocator &_alloc;
	Genode::Lock       _lock { };

	/**
	 * Import one login from node
	 */
	bool _import_single(Genode::Xml_node const &node)
	{
		User     user        = node.attribute_value("user", User());
		Password pw          = node.attribute_value("password", Password());
		Filename pub         = node.attribute_value("pub_key", Filename());
		bool     multi_login = node.attribute_value("multi_login", false);
		bool     req_term    = node.attribute_value("request_terminal", false);

		if (!user.valid() || (!pw.valid() && !pub.valid())) {
			Genode::warning("ignoring invalid policy");
			return false;
		}

		if (lookup(user.string())) {
			Genode::warning("ignoring already imported login ", user.string());
			return false;
		}

		try {
			new (&_alloc) Login(*this, user, pw, pub, multi_login, req_term);
			return true;
		} catch (...) { return false; }
	}

	void _remove_all()
	{
		for_each([&] (Login &login) {
			Genode::destroy(&_alloc, &login);
		});
	}

	/**
	 * Constructor
	 *
	 * \param  alloc   allocator for registry elements
	 */
	Login_registry(Genode::Allocator &alloc) : _alloc(alloc) { }

	/**
	 * Return registry lock
	 */
	Genode::Lock &lock() { return _lock; }

	/**
	 * Import all login information from config
	 */
	void import(Genode::Xml_node const &node)
	{
		_remove_all();

		try {
			node.for_each_sub_node("policy",
			[&] (Genode::Xml_node const &node) {
				_import_single(node);
			});
		} catch (...) { }
	}

	/**
	 * Look up login information by user name
	 */
	Ssh::Login const *lookup(char const *user) const
	{
		Login const *p = nullptr;
		auto lookup_user = [&] (Login const &login) {
			if (login.user == user) { p = &login; }
		};
		for_each(lookup_user);
		return p;
	}
};

#endif /* _SSH_TERMINAL_LOGIN_H_ */
