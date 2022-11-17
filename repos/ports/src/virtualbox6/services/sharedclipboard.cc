/*
 * \brief  Shared-clipboard service backend
 * \author Christian Helmuth
 * \date   2021-09-01
 *
 * Note, the text strings exchanged with the upper-layers (and therefore the
 * guest) must be null-terminated and sizes have to include the terminator.
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

/* Genode includes */
#include <base/env.h>
#include <base/attached_rom_dataspace.h>
#include <os/reporter.h>
#include <libc/component.h>

/* VirtualBox includes */
#include <iprt/utf16.h>
#include <VBoxSharedClipboardSvc-internal.h>
#include <VBox/GuestHost/clipboard-helper.h>

/* local includes */
#include "services.h"


using namespace Genode;

namespace {

class Clipboard
{
	private:

		Env &_env;

		Attached_rom_dataspace _rom    { _env, "clipboard" };
		Expanding_reporter     _report { _env, "clipboard", "clipboard" };

		SHCLCLIENT *_client { nullptr };

		Signal_handler<Clipboard> _rom_sigh {
			_env.ep(), *this, &Clipboard::_handle_rom_changed };

		void _handle_rom_changed()
		{
			Libc::with_libc([&] () { ShClSvcImplSync(_client); });
		}

	public:

		struct Guard;

		Clipboard(Env &env) : _env(env)
		{
			_rom.sigh(_rom_sigh);
		}

		SHCLCLIENT * client() const { return _client; }

		int connect(SHCLCLIENT *client)
		{
			if (_client) {
				warning("shared clipboard: only one client supported");
				return VERR_NOT_SUPPORTED;
			}

			_client = client;

			return VINF_SUCCESS;
		}

		void disconnect(SHCLCLIENT *client)
		{
			if (client != _client) {
				warning("shared clipboard: unknown client on disconnect");
				return;
			}

			_client = nullptr;
		}

		void report(char const *content)
		{
			try {
				_report.generate([&] (Reporter::Xml_generator &xml) {
					xml.append_sanitized(content); });
			} catch (...) {
				error("shared clipboard: could not report new content");
			}
		}

		template <typename FN>
		void with_content(FN const &fn)
		{
			_rom.update();

			if (!_rom.valid())
				return;

			size_t  size    = _rom.xml().content_size();
			char   *content = (char *)RTMemAlloc(size + 1);

			size = _rom.xml().decoded_content(content, size);

			/* add null terminator */
			content[size] = 0;

			fn(content, size + 1);

			RTMemFree(content);
		}
};

struct Clipboard::Guard
{
	Guard()  { ShClSvcLock(); }
	~Guard() { ShClSvcUnlock(); }
};

Constructible<Clipboard> clipboard;

} /* unnamed namespace */


int ShClSvcImplReadData(PSHCLCLIENT, PSHCLCLIENTCMDCTX, SHCLFORMAT fFormat, void *pv, uint32_t cb, unsigned int *cb_out)
{
	if (!(fFormat & VBOX_SHCL_FMT_UNICODETEXT))
		return VERR_NOT_IMPLEMENTED;

	Clipboard::Guard guard;

	int rc  = VINF_SUCCESS;
	*cb_out = 0;

	clipboard->with_content([&] (char const *utf8_string, size_t utf8_size) {

		PRTUTF16 utf16_string = (PRTUTF16)pv;
		size_t   utf16_chars  = cb/sizeof(RTUTF16);

		rc = RTStrToUtf16Ex(utf8_string, utf8_size, &utf16_string, utf16_chars, &utf16_chars);

		/* VERR_BUFFER_OVERFLOW is handled by the guest if cb < cb_out */
		if (rc == VERR_BUFFER_OVERFLOW)
			rc = VINF_SUCCESS;

		if (RT_SUCCESS(rc)) {
			/* the protocol requires cb_out to include the null terminator */
			*cb_out = (utf16_chars + 1)*sizeof(RTUTF16);
		}
	});

	return rc;
}


int ShClSvcImplWriteData(PSHCLCLIENT, PSHCLCLIENTCMDCTX, SHCLFORMAT fFormat, void *pv, uint32_t cb)
{
	if (!(fFormat & VBOX_SHCL_FMT_UNICODETEXT))
		return VERR_NOT_IMPLEMENTED;

	if (pv == nullptr)
		return VERR_INVALID_POINTER;

	Clipboard::Guard guard;

	PCRTUTF16 const utf16_string = (PCRTUTF16)pv;

	char *utf8_string;

	/* allocates buffer and converts string (incl. null terminator) */
	int rc = RTUtf16ToUtf8(utf16_string, &utf8_string);
	if (RT_FAILURE(rc))
		return VINF_SUCCESS;

	clipboard->report(utf8_string);

	RTStrFree(utf8_string);

	/*
	 * We send a format report to guest as the global clipboard was changed by
	 * this operation. This generates a feedback loop to keep the host and
	 * guest clipboards in sync.
	 */
	return ShClSvcHostReportFormats(clipboard->client(), VBOX_SHCL_FMT_UNICODETEXT);
}


/**
 * The guest is taking possession of the shared clipboard
 */
int ShClSvcImplFormatAnnounce(PSHCLCLIENT pClient, SHCLFORMATS fFormats)
{
	/* eagerly request data from the guest */
	return ShClSvcDataReadRequest(pClient, fFormats, NULL /* pidEvent */);
}


/**
 * Synchronize contents of the host clipboard with the guest
 *
 * Called by HGCM svc layer on svcConnect() and svcLoadState() (after resume)
 * as well as on clipboard ROM update.
 */
int ShClSvcImplSync(PSHCLCLIENT pClient)
{
	Clipboard::Guard guard;

	if (!clipboard->client())
		return VINF_NO_CHANGE;

	if (pClient != clipboard->client()) {
		warning("shared clipboard: client mismatch on sync");
		return VINF_NO_CHANGE;
	}

	return ShClSvcHostReportFormats(clipboard->client(), VBOX_SHCL_FMT_UNICODETEXT);
}


int ShClSvcImplDisconnect(PSHCLCLIENT pClient)
{
	Clipboard::Guard guard;

	clipboard->disconnect(pClient);

	return VINF_SUCCESS;
}


int ShClSvcImplConnect(PSHCLCLIENT pClient, bool /* fHeadless */)
{
	Clipboard::Guard guard;

	int const rc = clipboard->connect(pClient);
	if (RT_FAILURE(rc))
		return rc;

	/* send initial format report to guest */
	return ShClSvcHostReportFormats(clipboard->client(), VBOX_SHCL_FMT_UNICODETEXT);
}


int ShClSvcImplInit(VBOXHGCMSVCFNTABLE *)
{
	try {
		clipboard.construct(Services::env());

		return VINF_SUCCESS;
	} catch (...) {
		return VERR_NOT_SUPPORTED;
	}
}


void ShClSvcImplDestroy()
{
	clipboard.destruct();
}
