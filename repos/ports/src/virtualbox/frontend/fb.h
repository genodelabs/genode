/*
 * \brief  Virtualbox framebuffer implementation for Genode
 * \author Alexander Boettcher
 * \date   2013-10-16
 */

/*
 * Copyright (C) 2013-2014 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

/* Genode includes */
#define Framebuffer Fb_Genode
#include <framebuffer_session/connection.h>
#undef Framebuffer

/* VirtualBox includes */

class Genodefb : public Framebuffer
{
	private:

		Fb_Genode::Connection _fb;
		Fb_Genode::Mode const _fb_mode;
		void            *     _fb_base;
		RTCRITSECT            _fb_lock;

		unsigned long         _width;
		unsigned long         _height;

	public:

		Genodefb ()
		:
			_fb_mode(_fb.mode()),
			_fb_base(Genode::env()->rm_session()->attach(_fb.dataspace())),
			_width(_fb_mode.width()),
			_height(_fb_mode.height())
		{
			int rc = RTCritSectInit(&_fb_lock);
			Assert(rc == VINF_SUCCESS);
		}

		STDMETHODIMP COMGETTER(Width)(ULONG *width)
		{
			if (!width)
				return E_INVALIDARG;

			*width = (int)_width > _fb_mode.width() ? _fb_mode.width() : _width;
			return S_OK;
		}
		
		STDMETHODIMP COMGETTER(Height)(ULONG *height)
		{
			if (!height)
				return E_INVALIDARG;

			*height = (int)_height > _fb_mode.height() ? _fb_mode.height() : _height;
			return S_OK;
		}

		STDMETHODIMP Lock()
		{
			return Global::vboxStatusCodeToCOM(RTCritSectEnter(&_fb_lock));
		}
	
		STDMETHODIMP Unlock()
		{
			return Global::vboxStatusCodeToCOM(RTCritSectLeave(&_fb_lock));
		}

		STDMETHODIMP COMGETTER(Address)(BYTE **addr)
		{
			*addr = reinterpret_cast<BYTE*>(_fb_base);
			return S_OK;
		}

		STDMETHODIMP COMGETTER(BitsPerPixel)(ULONG *bits)
		{
			if (!bits)
				return E_INVALIDARG;

			*bits = _fb_mode.bytes_per_pixel() * 8;
			return S_OK;
		}

		STDMETHODIMP COMGETTER(BytesPerLine)(ULONG *line)
		{
			*line = _fb_mode.width() * _fb_mode.bytes_per_pixel();
			return S_OK;
		}

		HRESULT NotifyUpdate(ULONG x, ULONG y, ULONG w, ULONG h)
		{
			_fb.refresh(x, y, w, h);
			return S_OK;
		}

		STDMETHODIMP RequestResize(ULONG aScreenId, ULONG pixelFormat,
		                           BYTE *vram, ULONG bitsPerPixel,
		                           ULONG bytesPerLine, ULONG w, ULONG h,
		                           BOOL *finished)
		{
			/* clear screen to avoid artefacts during resize */
			size_t const num_pixels = _fb_mode.width() * _fb_mode.height();
			memset(_fb_base, 0, num_pixels * _fb_mode.bytes_per_pixel());

			_fb.refresh(0, 0, _fb_mode.width(), _fb_mode.height());

			/* bitsPerPixel == 0 is set by DevVGA when in text mode */
			bool ok = ((bitsPerPixel == 16) || (bitsPerPixel == 0)) &&
			          (w <= (ULONG)_fb_mode.width()) &&
			          (h <= (ULONG)_fb_mode.height());
			if (ok)
				PINF("fb resize : %lux%lu@%zu -> %ux%u@%u", _width, _height,
				     _fb_mode.bytes_per_pixel() * 8, w, h, bitsPerPixel);
			else
				PWRN("fb resize : %lux%lu@%zu -> %ux%u@%u ignored", _width, _height,
				     _fb_mode.bytes_per_pixel() * 8, w, h, bitsPerPixel);

			_width  = w;
			_height = h;

			*finished = true;

			return S_OK;
		}

		STDMETHODIMP COMGETTER(PixelFormat) (ULONG *format)
		{
			if (!format)
				return E_POINTER;

			*format = FramebufferPixelFormat_Opaque;
			return S_OK;
		}

		STDMETHODIMP COMGETTER(UsesGuestVRAM) (BOOL *usesGuestVRAM)
		{
			if (!usesGuestVRAM)
				return E_POINTER;

			*usesGuestVRAM = FALSE;
			return S_OK;
		}

		STDMETHODIMP COMGETTER(HeightReduction) (ULONG *reduce)
		{
			*reduce = 0;
			return S_OK;
		}

		STDMETHODIMP COMGETTER(Overlay) (IFramebufferOverlay **aOverlay)
		{
			Assert(!"FixMe");
			return S_OK;
		}

		STDMETHODIMP COMGETTER(WinId) (ULONG64 *winId)
		{
			Assert(!"FixMe");
			return S_OK;
		}

		STDMETHODIMP VideoModeSupported(ULONG width, ULONG height, ULONG bpp, BOOL *supported)
		{
			if (!supported)
				return E_POINTER;

			*supported = ((width <= (ULONG)_fb_mode.width()) &&
			              (height <= (ULONG)_fb_mode.height()) &&
			              (bpp == _fb_mode.bytes_per_pixel() * 8));

			return S_OK;
		}

		STDMETHODIMP GetVisibleRegion(BYTE *aRectangles, ULONG aCount,
		                              ULONG *aCountCopied)
		{
			Assert(!"FixMe");
			return S_OK;
		}
		
		STDMETHODIMP SetVisibleRegion(BYTE *aRectangles, ULONG aCount)
		{
			Assert(!"FixMe");
			return S_OK;
		}

		STDMETHODIMP ProcessVHWACommand(BYTE *pCommand)
		{
		    return E_NOTIMPL;
		}
};
