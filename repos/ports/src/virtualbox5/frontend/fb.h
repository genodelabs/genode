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
#include <nitpicker_session/connection.h>
#undef Framebuffer

#include <os/texture_rgb565.h>
#include <os/texture_rgb888.h>
#include <os/dither_painter.h>

/* VirtualBox includes */

#include "Global.h"
#include "VirtualBoxBase.h"

typedef Nitpicker::Session::View_handle View_handle;

class Genodefb :
	VBOX_SCRIPTABLE_IMPL(IFramebuffer)
{
	private:

		Genode::Env           &_env;
		Nitpicker::Connection &_nitpicker;
		Fb_Genode::Session    &_fb;
		View_handle            _view;
		Fb_Genode::Mode        _fb_mode { 1024, 768, Fb_Genode::Mode::RGB565 };

		/*
		 * The mode currently used by the VM. Can be smaller than the
		 * framebuffer mode.
		 */
		Fb_Genode::Mode        _virtual_fb_mode;

		void                  *_fb_base;
		RTCRITSECT             _fb_lock;

		void _clear_screen()
		{
			if (!_fb_base) return;

			size_t const max_h = Genode::min(_fb_mode.height(), _virtual_fb_mode.height());
			size_t const num_pixels = _fb_mode.width() * max_h;
			memset(_fb_base, 0, num_pixels * _fb_mode.bytes_per_pixel());
			_fb.refresh(0, 0, _virtual_fb_mode.width(), _virtual_fb_mode.height());
		}

		void _adjust_buffer()
		{
			_nitpicker.buffer(Fb_Genode::Mode(_fb_mode.width(), _fb_mode.height(),
			                                  Fb_Genode::Mode::RGB565), false);

			typedef Nitpicker::Session::Command Command;

			Nitpicker::Rect rect(Nitpicker::Point(0, 0),
			                     Nitpicker::Area(_fb_mode.width(), _fb_mode.height()));

			_nitpicker.enqueue<Command::Geometry>(_view, rect);
			_nitpicker.execute();
		}

		Fb_Genode::Mode _initial_setup()
		{
			typedef Nitpicker::Session::Command Command;

			_view = _nitpicker.create_view();

			_adjust_buffer();

			_nitpicker.enqueue<Command::To_front>(_view, View_handle());
			_nitpicker.execute();

			return _fb_mode;
		}

	public:

		Genodefb (Genode::Env &env, Nitpicker::Connection &nitpicker)
		:
			_env(env),
			_nitpicker(nitpicker),
			_fb(*nitpicker.framebuffer()),
			_virtual_fb_mode(_initial_setup()),
			_fb_base(env.rm().attach(_fb.dataspace()))
		{
			int rc = RTCritSectInit(&_fb_lock);
			Assert(rc == VINF_SUCCESS);
		}

		int w() const { return _fb_mode.width(); }
		int h() const { return _fb_mode.height(); }

		void update_mode(Fb_Genode::Mode mode)
		{
			Lock();

			_fb_mode = mode;

			if (_fb_base)
				_env.rm().detach(_fb_base);

			_adjust_buffer();

			try {
				_fb_base = _env.rm().attach(_fb.dataspace());
			} catch (...) {
				_fb_base = nullptr;
			}

			Unlock();
		}

		STDMETHODIMP Lock()
		{
			return Global::vboxStatusCodeToCOM(RTCritSectEnter(&_fb_lock));
		}
	
		STDMETHODIMP Unlock()
		{
			return Global::vboxStatusCodeToCOM(RTCritSectLeave(&_fb_lock));
		}

		STDMETHODIMP NotifyChange(PRUint32 screen, PRUint32, PRUint32,
		                          PRUint32 w, PRUint32 h) override
		{
			HRESULT result = E_FAIL;

			Lock();

			bool ok = (w <= (ULONG)_fb_mode.width()) &&
			          (h <= (ULONG)_fb_mode.height());

			if (ok) {
				Genode::log("fb resize : [", screen, "] ",
				            _virtual_fb_mode.width(), "x",
				            _virtual_fb_mode.height(), " -> ",
				            w, "x", h,
				            " (host: ", _fb_mode.width(), "x",
				             _fb_mode.height(), ")");

				if ((w < (ULONG)_fb_mode.width()) ||
				    (h < (ULONG)_fb_mode.height())) {
					/* clear the old content around the new, smaller area. */
				    _clear_screen();
				}

				_virtual_fb_mode = Fb_Genode::Mode(w, h, Fb_Genode::Mode::RGB565);

				result = S_OK;
			} else
				Genode::log("fb resize : [", screen, "] ",
				            _virtual_fb_mode.width(), "x",
				            _virtual_fb_mode.height(), " -> ",
				            w, "x", h, " ignored"
				            " (host: ", _fb_mode.width(), "x",
				             _fb_mode.height(), ")");

			Unlock();

			return result;
		}

		STDMETHODIMP COMGETTER(Capabilities)(ComSafeArrayOut(FramebufferCapabilities_T, enmCapabilities)) override
		{
			if (ComSafeArrayOutIsNull(enmCapabilities))
				return E_POINTER;

			com::SafeArray<FramebufferCapabilities_T> caps;
			caps.resize(1);
			caps[0] = FramebufferCapabilities_UpdateImage;
			//caps[0] = FramebufferCapabilities_VHWA;
			caps.detachTo(ComSafeArrayOutArg(enmCapabilities));

			return S_OK;
		}

		STDMETHODIMP COMGETTER(HeightReduction) (ULONG *reduce) override
		{
			if (!reduce)
				return E_POINTER;

			*reduce = 0;
			return S_OK;
		}

		STDMETHODIMP NotifyUpdateImage(PRUint32 o_x, PRUint32 o_y,
		                               PRUint32 width, PRUint32 height,
		                               PRUint32 imageSize,
		                               PRUint8 *image) override
		{
			if (!_fb_base) return S_OK;

			Lock();

			Nitpicker::Area const area_fb = Nitpicker::Area(_fb_mode.width(),
			                                                _fb_mode.height());
			Nitpicker::Area const area_vm = Nitpicker::Area(width, height);

			using namespace Genode;

			typedef Pixel_rgb888 Pixel_src;
			typedef Pixel_rgb565 Pixel_dst;

			Texture<Pixel_src> texture((Pixel_src *)image, nullptr, area_vm);
			Surface<Pixel_dst> surface((Pixel_dst *)_fb_base, area_fb);

			Dither_painter::paint(surface, texture, Surface_base::Point(o_x, o_y));

			_fb.refresh(o_x, o_y, area_vm.w(), area_vm.h());

			Unlock();

			return S_OK;
		}

		STDMETHODIMP COMGETTER(Overlay) (IFramebufferOverlay **) override {
			Assert(!"FixMe");
			return E_NOTIMPL; }

		STDMETHODIMP COMGETTER(WinId) (PRInt64 *winId) override {
			Assert(!"FixMe");
			return E_NOTIMPL; }

		STDMETHODIMP VideoModeSupported(ULONG width, ULONG height,
		                                ULONG bpp, BOOL *supported) override
		{
			if (!supported)
				return E_POINTER;

			*supported = ((width <= (ULONG)_fb_mode.width()) &&
			              (height <= (ULONG)_fb_mode.height()));

			return S_OK;
		}

		STDMETHODIMP Notify3DEvent(PRUint32, PRUint32, PRUint8 *) override {
			Assert(!"FixMe");
			return E_NOTIMPL; }

		STDMETHODIMP ProcessVHWACommand(BYTE *, LONG, BOOL) override {
			Assert(!"FixMe");
		    return E_NOTIMPL; }

		STDMETHODIMP GetVisibleRegion(BYTE *, ULONG, ULONG *) override {
			Assert(!"FixMe");
			return E_NOTIMPL; }
		
		STDMETHODIMP SetVisibleRegion(BYTE *, ULONG) override {
			Assert(!"FixMe");
			return E_NOTIMPL; }

		STDMETHODIMP COMGETTER(PixelFormat) (ULONG *format) override {
			Assert(!"FixMe");
			return E_NOTIMPL; }

		HRESULT NotifyUpdate(ULONG x, ULONG y, ULONG w, ULONG h) override {
			Assert(!"FixMe");
			return E_NOTIMPL; }

		STDMETHODIMP COMGETTER(BitsPerPixel)(ULONG *bits) override {
			Assert(!"FixMe");
			return E_NOTIMPL; }

		STDMETHODIMP COMGETTER(BytesPerLine)(ULONG *line) override {
			Assert(!"FixMe");
			return E_NOTIMPL; }

		STDMETHODIMP COMGETTER(Width)(ULONG *width) override {
			Assert(!"FixMe");
			return E_NOTIMPL; }

		STDMETHODIMP COMGETTER(Height)(ULONG *height) override {
			Assert(!"FixMe");
			return E_NOTIMPL; }
};
