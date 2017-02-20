/*
 * \brief  Genode-specific configuration
 * \author Christian Prochaska
 * \date   2011-05-06
 */

/*
 * Copyright (C) 2011-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#define HAVE_ARPA_INET_H 1
#define HAVE_ERRNO_H 1
#define HAVE_FCNTL_H 1
#define HAVE_NETINET_IN_H 1
#define HAVE_NETINET_TCP_H 1
#define HAVE_SIGNAL_H 1
#define HAVE_SOCKLEN_T 1
#define HAVE_STRING_H 1
#define HAVE_SYS_SOCKET_H 1
#define HAVE_UNISTD_H 1

#define HAVE_LINUX_REGSETS 1
#define HAVE_LINUX_USRREGS 1

#define __GENODE__

/* first process id */
#define GENODE_MAIN_LWPID 1
