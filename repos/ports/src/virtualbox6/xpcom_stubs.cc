/*
 * \brief  Dummy implementations of symbols needed by XPCOM
 * \author Norman Feske
 * \date   2020-10-09
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <stub_macros.h>

static bool const debug = true;


#include <SpecialSystemDirectory.h>

nsresult GetSpecialSystemDirectory(SystemDirectories, nsILocalFile**) STOP
void StartupSpecialSystemDirectory() { }


#include <nsFastLoadService.h>

nsresult nsFastLoadService::Create(nsISupports*, nsID const&, void**) STOP


#include <nsMultiplexInputStream.h>

nsresult nsMultiplexInputStreamConstructor(nsISupports*, nsID const&, void**) STOP


#include <nsPersistentProperties.h>

nsresult nsPersistentProperties::Create(nsISupports*, nsID const&, void**) STOP


#include <nsProxyEventPrivate.h>

nsresult nsProxyObjectManager::Create(nsISupports*, nsID const&, void**) STOP


#include <nsScriptableInputStream.h>

nsresult nsScriptableInputStream::Create(nsISupports*, nsID const&, void**) STOP


#include <nsStringStream.h>

nsresult nsStringInputStreamConstructor(nsISupports*, nsID const&, void**) STOP


#include <xptinfo.h>

nsIInterfaceInfoManager *XPTI_GetInterfaceInfoManager() { return nullptr; }


extern "C" {
#include <_freebsd.h>
#include <primpl.h>

void _MD_EarlyInit(void)      { }
void _PR_InitCPUs(void)       { }
void _pr_init_ipv6(void)      { }
void _PR_InitLayerCache(void) { }
void _PR_InitLinker(void)     { }
void _PR_InitSegs(void)       { }
void _PR_InitStacks(void)     { }
}

