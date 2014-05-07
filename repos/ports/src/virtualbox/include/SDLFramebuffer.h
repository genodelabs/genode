/*
 * \brief  Framebuffer implementation of VirtualBox for Genode
 * \author Alexander Boettcher
 * \date   2013-10-16
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

/* Genode includes */
#define Framebuffer FramebufferGenode
#include <framebuffer_session/connection.h>
#undef Framebuffer

/* VirtualBox includes */
#include "Framebuffer.h"

#include <base/printf.h>

class SDLFramebuffer : public Framebuffer
{
	private:

		FramebufferGenode::Connection _fb;
		FramebufferGenode::Mode const _fb_mode;
		void * _fb_base;
		RTCRITSECT mUpdateLock;

	public:

		SDLFramebuffer ()
		:
			_fb_mode(_fb.mode()),
			_fb_base(Genode::env()->rm_session()->attach(_fb.dataspace()))
		{
			int rc = RTCritSectInit(&mUpdateLock);
			if (rc != VINF_SUCCESS)
				PERR("Lock could not be initalized");
		}

		HRESULT getWidth(ULONG * width)
		{
			*width = _fb_mode.width();
			return S_OK;
		}
		
		HRESULT getHeight(ULONG * height)
		{
			*height = _fb_mode.height();
			return S_OK;
		}

		HRESULT Lock() { return RTCritSectEnter(&mUpdateLock); }
	
		HRESULT Unlock() { return RTCritSectLeave(&mUpdateLock); }

		HRESULT getAddress(uintptr_t * addr)
		{
			*addr = reinterpret_cast<uintptr_t>(_fb_base);
			return S_OK;
		}

		HRESULT getBitsPerPixel(ULONG * bits)
		{
			*bits = _fb_mode.bytes_per_pixel() * 8;
			return S_OK;
		}

		HRESULT getLineSize(ULONG * line)
		{
			*line = _fb_mode.width() * _fb_mode.bytes_per_pixel();
			return S_OK;
		}

		HRESULT NotifyUpdate(ULONG x, ULONG y, ULONG w, ULONG h)
		{
			_fb.refresh(x, y, w, h);
			return S_OK;
		}

		HRESULT RequestResize(ULONG x, ULONG y, BOOL * finished)
		{
			PERR("ignore resize request to %lux%lu", x, y);
			Genode::size_t const num_pixels = _fb_mode.width()*_fb_mode.height();
			Genode::memset(_fb_base, 0, num_pixels*_fb_mode.bytes_per_pixel());
			_fb.refresh(0, 0, _fb_mode.width(), _fb_mode.height());
			*finished = true;
			return S_OK;
		}

		HRESULT GetVisibleRegion(BYTE *, ULONG, ULONG *) { PERR("%s:%s called", __FILE__, __FUNCTION__); return E_NOTIMPL; }
		HRESULT SetVisibleRegion(BYTE *, ULONG) { PERR("%s:%s called", __FILE__, __FUNCTION__); return E_NOTIMPL; }

		HRESULT ProcessVHWACommand(BYTE *) { PERR("%s:%s called", __FILE__, __FUNCTION__); return E_NOTIMPL; }

		void repaint() { PERR("%s:%s called", __FILE__, __FUNCTION__); }
		void resize() { PERR("%s:%s called", __FILE__, __FUNCTION__); }

		void update(int, int, int, int) { PERR("%s:%s called", __FILE__, __FUNCTION__); }
		bool getFullscreen() { PERR("%s:%s called", __FILE__, __FUNCTION__); }
		void setFullscreen(bool) { PERR("%s:%s called", __FILE__, __FUNCTION__); }
		int  getYOffset() { PERR("%s:%s called", __FILE__, __FUNCTION__); }
		int  getHostXres() { PERR("%s:%s called", __FILE__, __FUNCTION__); }
		int  getHostYres() { PERR("%s:%s called", __FILE__, __FUNCTION__); }
		int  getHostBitsPerPixel() { PERR("%s:%s called", __FILE__, __FUNCTION__); }
};
