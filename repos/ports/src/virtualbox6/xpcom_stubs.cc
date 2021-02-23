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
void StartupSpecialSystemDirectory() TRACE()


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

nsIInterfaceInfoManager *XPTI_GetInterfaceInfoManager() TRACE(nullptr)


extern "C" {
#include <_freebsd.h>
#include <primpl.h>

void _MD_EarlyInit(void)      TRACE()
void _PR_InitCPUs(void)       TRACE()
void _pr_init_ipv6(void)      TRACE()
void _PR_InitLayerCache(void) TRACE()
void _PR_InitLinker(void)     TRACE()
void _PR_InitSegs(void)       TRACE()
void _PR_InitStacks(void)     TRACE()
}

