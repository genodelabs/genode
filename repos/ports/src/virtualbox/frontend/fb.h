/*
 * \brief  Virtualbox framebuffer implementation for Genode
 * \author Alexander Boettcher
 * \date   2013-10-16
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

/* Genode includes */
#define Framebuffer Fb_Genode
#include <framebuffer_session/connection.h>
#undef Framebuffer

/* VirtualBox includes */

#include "Global.h"
#include "VirtualBoxBase.h"

class Genodefb :
	VBOX_SCRIPTABLE_IMPL(IFramebuffer)
{
	private:

		Genode::Env           &_env;
		Fb_Genode::Connection  _fb;

		/* The mode matching the currently attached dataspace */
		Fb_Genode::Mode        _fb_mode;

		/* The mode at the time when the mode change signal was received */
		Fb_Genode::Mode        _next_fb_mode;

		/*
		 * The mode currently used by the VM. Can be smaller than the
		 * framebuffer mode.
		 */
		Fb_Genode::Mode        _virtual_fb_mode;

		void                  *_fb_base;
		RTCRITSECT             _fb_lock;

		void _clear_screen()
		{
			size_t const num_pixels = _fb_mode.width() * _virtual_fb_mode.height();
			memset(_fb_base, 0, num_pixels * _fb_mode.bytes_per_pixel());
			_fb.refresh(0, 0, _virtual_fb_mode.width(), _virtual_fb_mode.height());
		}

	public:

		Genodefb (Genode::Env &env)
		:
			_env(env),
			_fb(env, Fb_Genode::Mode(0, 0, Fb_Genode::Mode::INVALID)),
			_fb_mode(_fb.mode()),
			_next_fb_mode(_fb_mode),
			_virtual_fb_mode(_fb_mode),
			_fb_base(env.rm().attach(_fb.dataspace()))
		{
			int rc = RTCritSectInit(&_fb_lock);
			Assert(rc == VINF_SUCCESS);
		}

		/* Return the next mode of the framebuffer */ 
		int w() const { return _next_fb_mode.width(); }
		int h() const { return _next_fb_mode.height(); }

		void mode_sigh(Genode::Signal_context_capability sigh)
		{
			_fb.mode_sigh(sigh);
		}

		void update_mode()
		{
			Lock();
			_next_fb_mode = _fb.mode();
			Unlock();
		}

		STDMETHODIMP COMGETTER(Width)(ULONG *width)
		{
			if (!width)
				return E_INVALIDARG;

			*width = _virtual_fb_mode.width();

			return S_OK;
		}

		STDMETHODIMP COMGETTER(Height)(ULONG *height)
		{
			if (!height)
				return E_INVALIDARG;

			*height = _virtual_fb_mode.height();

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

			*bits = _virtual_fb_mode.bytes_per_pixel() * 8;

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
			HRESULT result = E_FAIL;

			Lock();

			bool ok = (w <= (ULONG)_next_fb_mode.width()) &&
			          (h <= (ULONG)_next_fb_mode.height());

			if (ok) {
				Genode::log("fb resize : ", _virtual_fb_mode.width(),
				            "x", _virtual_fb_mode.height(), "@",
				            _virtual_fb_mode.bytes_per_pixel() * 8, " -> ",
				            w, "x", h, "@", bitsPerPixel);

				if ((w < (ULONG)_next_fb_mode.width()) ||
				    (h < (ULONG)_next_fb_mode.height())) {
					/* clear the old content around the new, smaller area. */
				    _clear_screen();
				}

				_fb_mode = _next_fb_mode;

				_virtual_fb_mode = Fb_Genode::Mode(w, h, Fb_Genode::Mode::RGB565);

				_env.rm().detach(_fb_base);

				_fb_base = _env.rm().attach(_fb.dataspace());

				result = S_OK;

			} else
				Genode::warning("fb resize : ", _virtual_fb_mode.width(),
				                "x", _virtual_fb_mode.height(), "@",
				                _virtual_fb_mode.bytes_per_pixel() * 8, " -> ",
				                w, "x", h, "@", bitsPerPixel, " ignored");

			*finished = true;

			Unlock();

			return result;
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

		STDMETHODIMP COMGETTER(WinId) (PRInt64 *winId)
		{
			Assert(!"FixMe");
			return S_OK;
		}

		STDMETHODIMP VideoModeSupported(ULONG width, ULONG height, ULONG bpp, BOOL *supported)
		{
			if (!supported)
				return E_POINTER;

			*supported = ((width <= (ULONG)_next_fb_mode.width()) &&
			              (height <= (ULONG)_next_fb_mode.height()));

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

		STDMETHODIMP Notify3DEvent(PRUint32, PRUint8*)
		{
			return E_NOTIMPL;
		}
};
