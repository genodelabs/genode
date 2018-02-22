/*
 * \brief  Wrapper for safely calling into GnuPG from C++ code
 * \author Norman Feske
 * \date   2018-01-06
 *
 * We cannot directly include GnuPG's 'main.h' from C++ code. E.g., because
 * the header uses C++ keywords as variable names. By using this wrapper,
 * we keep C++ and C nicely separated.
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* local includes */
#include "gnupg.h"

/* libc includes */
#include <ctype.h>  /* isascii, needed by gnupg headers */

/* GnuPG headers */
#include <gpg.h>
#include <main.h>
#include <options.h>


/*
 * Global variable that is incremented by GnuPG whenever a signature check
 * failed.
 */
int g10_errors_seen = 0;


enum Read_pubkey_result { READ_PUBKEY_OK,
                          READ_PUBKEY_MISSING_FILE,
                          READ_PUBKEY_INVALID_FORMAT };

/**
 * Read public key into new packet
 *
 * The packet is allocated by this function and returned as 'packet_out_ptr'
 * whenever it returns with 'READ_PUBKEY_OK'.
 */
static enum Read_pubkey_result
read_pubkey_from_file(char const *pubkey_path, PACKET **packet_out_ptr)
{
	*packet_out_ptr = NULL;

	/* set up parser context for parsing the public key data */
	struct parse_packet_ctx_s parse_ctx;
	memset(&parse_ctx, 0, sizeof(parse_ctx));

	parse_ctx.inp = iobuf_open(pubkey_path);
	if (!parse_ctx.inp)
		return READ_PUBKEY_MISSING_FILE;

	/* convert public key from ASCII-armored to binary representation */
	armor_filter_context_t *afx = new_armor_context ();
	push_armor_filter(afx, parse_ctx.inp);

	/* parse public key data */
	PACKET * const packet = xmalloc(sizeof(*packet));
	memset(packet, 0, sizeof(*packet));
	int const parse_ok = (parse_packet(&parse_ctx, packet) == 0);

	release_armor_context(afx);
	iobuf_close(parse_ctx.inp);

	if (parse_ok && packet->pkt.public_key) {
		*packet_out_ptr = packet;
		return READ_PUBKEY_OK;
	}

	xfree(packet);
	return READ_PUBKEY_INVALID_FORMAT;
}


/*
 * Emulation of a key ring with only one public key.
 */
static PACKET *_pubkey_packet;



/********************************************
 ** Implementation of the public interface **
 ********************************************/

enum Gnupg_verify_result gnupg_verify_detached_signature(char const *pubkey_path,
                                                         char const *data_path,
                                                         char const *sig_path)
{
	/*
	 * Obtain pointer to public-key packet. The packet is allocated by
	 * 'read_pubkey_from_file' and freed by 'verify_signatures'.
	 */
	switch (read_pubkey_from_file(pubkey_path, &_pubkey_packet)) {
	case READ_PUBKEY_OK:              break;
	case READ_PUBKEY_MISSING_FILE:    return GNUPG_VERIFY_PUBKEY_UNAVAILABLE;
	case READ_PUBKEY_INVALID_FORMAT:  return GNUPG_VERIFY_PUBKEY_INVALID;
	};

	/*
	 * Set up the GnuPG control context, which is normally the job of
	 * 'gpg_init_default_ctrl'.
	 */
	struct server_control_s control;
	memset(&control, 0, sizeof(control));
	ctrl_t ctrl = &control;
	ctrl->magic = SERVER_CONTROL_MAGIC;

	opt.quiet = 1; /* prevent disclaimer about key compliance */

	/*
	 * Remember 'g10_errors_seen' before calling into GnuPG to obtain the
	 * feedback about the success of the signature verification.
	 */
	int const orig_errors_seen = g10_errors_seen;


	/*
	 * Call into GnuPG to verify the data with a detached signature. The
	 * 'verify_signatures' function indirectly calls 'get_pubkey' and
	 * 'get_pubkeyblock', which hand out our '_pubkey_packet'.
	 */
	char *file_names[2] = { strdup(sig_path), strdup(data_path) };
	int const err = verify_signatures(ctrl, 2, file_names);
	for (unsigned i = 0; i < 2; i++)
		free(file_names[i]);

	return !err && (orig_errors_seen == g10_errors_seen) ? GNUPG_VERIFY_OK
	                                                     : GNUPG_VERIFY_SIGNATURE_INVALID;
}


int get_pubkey(ctrl_t ctrl, PKT_public_key *pk, u32 *keyid)
{
	copy_public_key(pk, _pubkey_packet->pkt.public_key);
	pk->flags.valid = 1;
	return 0;
}


kbnode_t get_pubkeyblock(ctrl_t ctrl, u32 *keyid)
{
	return new_kbnode(_pubkey_packet);
}

