/*
 * \brief  Interface used for invoking GnuPG code
 * \author Norman Feske
 * \date   2018-01-06
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _GNUPG_H_
#define _GNUPG_H_

#ifdef __cplusplus
extern "C" {
#endif

enum Gnupg_verify_result { GNUPG_VERIFY_OK,
                           GNUPG_VERIFY_PUBKEY_UNAVAILABLE,
                           GNUPG_VERIFY_PUBKEY_INVALID,
                           GNUPG_VERIFY_SIGNATURE_INVALID };

enum Gnupg_verify_result gnupg_verify_detached_signature(char const *pubkey_path,
                                                         char const *data_path,
                                                         char const *sig_path);
#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* _GNUPG_H_ */
