/*
 * \brief  Dummies needed to link the parts of GnuPG that we need
 * \author Norman Feske
 * \date   2018-01-06
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* libc includes */
#include <ctype.h>  /* isascii, needed by gnupg headers */

/* GnuPG headers */
#include <gpg.h>
#include <main.h>


/********************
 ** Silent dummies **
 ********************/

int check_special_filename(char const *fname, int for_write, int notranslate)
{
	return -1;
}

void show_notation(PKT_signature *sig,int indent,int mode,int which) { }
void show_keyserver_url(PKT_signature *sig,int indent,int mode) { }
void show_policy_url(PKT_signature *sig,int indent,int mode) { }
void register_mem_cleanup_func() { }
int  check_signatures_trust(ctrl_t ctrl, PKT_signature *sig) { return 0; }

const char *asctimestamp(u32 stamp) { return "<sometime>"; }

u32 make_timestamp() { return ~0; /* disarm key-creation time check */ }


/***********************************************************
 ** Dummies that print a message when unexpectedly called **
 ***********************************************************/

#define DUMMY(ret_type, ret_val, name, args) \
ret_type name args \
{ \
	printf("%s: not implemented\n", __func__); \
	return ret_val; \
}

#define DUMMY_NORETURN(name, args) \
void name args \
{ \
	printf("%s: not implemented\n", __func__); \
	for (;;); \
}

DUMMY(byte const *, NULL, get_session_marker, (size_t *rlen));
DUMMY(char const *, NULL, print_fname_stdin, (char const *s));

DUMMY_NORETURN(g10_exit, (int code));


/*
 * The following dummies are solely needed by mainproc.c
 */

DUMMY(char *, NULL, bin2hex, (const void *buffer, size_t length, char *stringbuf));
DUMMY(int, 0, decrypt_data, (ctrl_t ctrl, void *ctx, PKT_encrypted *ed, DEK *dek));
DUMMY(void, , dump_attribs, (const PKT_user_id *uid, PKT_public_key *pk));
DUMMY(void, , free_keyserver_spec, ());
DUMMY(void, , free_notation, (struct notation *notation));
DUMMY(char *, NULL, get_matching_datafile, (const char *sigfilename));
DUMMY(gpg_error_t, 0, get_override_session_key, (DEK *dek, const char *string));
DUMMY(void, , get_ownertrust_info, ());
DUMMY(gpg_error_t, 0, get_session_key, (ctrl_t ctrl, PKT_pubkey_enc *k, DEK *dek));
DUMMY(char*, NULL, get_user_id, (ctrl_t ctrl, u32 *keyid, size_t *rn));
DUMMY(char*, NULL, get_user_id_native, (ctrl_t ctrl, u32 *keyid));
DUMMY(void, , get_validity, ());
DUMMY(void, , get_validity_info, ());
DUMMY(void, , gpg_dirmngr_get_pka, ());
DUMMY(int, 0, handle_compressed, (ctrl_t ctrl, void *ctx, PKT_compressed *cd, int (*callback)(iobuf_t, void *), void *passthru));
DUMMY(int, 0, have_secret_key_with_kid, (u32 *keyid));
DUMMY(void, , is_valid_mailbox, ());
DUMMY(void, , keyserver_any_configured, ());
DUMMY(void, , keyserver_import_fprint, ());
DUMMY(void, , keyserver_import_keyid, ());
DUMMY(void, , keyserver_import_wkd, ());
DUMMY(void, , merge_keys_and_selfsig, (ctrl_t ctrl, kbnode_t keyblock));
DUMMY(void, , parse_keyserver_uri, ());
DUMMY(void, , parse_preferred_keyserver, ());
DUMMY(void, , passphrase_clear_cache, (const char *cacheid));
DUMMY(DEK *, NULL, passphrase_to_dek, (int cipher_algo, STRING2KEY *s2k, int create, int nocache, const char *tryagain_text, int *canceled));
DUMMY(void, , print_fingerprint, (ctrl_t ctrl, estream_t fp, PKT_public_key *pk, int mode));
DUMMY(void, , print_key_line, (ctrl_t ctrl, estream_t fp, PKT_public_key *pk, int secret));
DUMMY(void, , print_utf8_buffer, (estream_t fp, const void *p, size_t n));
DUMMY(void, , show_photos, ());
DUMMY(struct notation *, NULL, sig_to_notation, (PKT_signature *sig));
DUMMY(const char *, NULL, strtimestamp, (u32 stamp));
DUMMY(void, , trust_value_to_string, ());
DUMMY(char *, NULL, utf8_to_native, (const char *string, size_t length, int delim));
DUMMY(int, 0, get_pubkey_byfprint, (ctrl_t ctrl, PKT_public_key *pk, kbnode_t *r_keyblock, const byte *fprint, size_t fprint_len));
DUMMY(const char *, NULL, strtimevalue, (u32 stamp));
DUMMY(char *, NULL, try_make_printable_string, (const void *p, size_t n, int delim));
DUMMY(void, , zb32_encode, ());
DUMMY(void, , image_type_to_string, ());
DUMMY(int, 0, get_pubkey_fast, (PKT_public_key *pk, u32 *keyid));
DUMMY(void, , parse_image_header, ());
DUMMY(char *, NULL, make_outfile_name, (const char *iname));
DUMMY(char *, NULL, ask_outfile_name, (const char *name, size_t namelen));
DUMMY(int, 0, overwrite_filep, (const char *fname));
DUMMY(void, , tty_get, ());
DUMMY(void, , display_online_help, (const char *keyword));
DUMMY(void, , tty_kill_prompt, ());
DUMMY(int, 0, text_filter, (void *opaque, int control, iobuf_t chain, byte *buf, size_t *ret_len));
DUMMY(iobuf_t, NULL, open_sigfile, (const char *sigfilename, progress_filter_context_t *pfx));
DUMMY(void, , tty_printf, ());
DUMMY(char *, NULL, gnupg_getcwd, ());
