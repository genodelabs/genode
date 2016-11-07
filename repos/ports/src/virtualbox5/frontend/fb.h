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
#include <nitpicker_session/nitpicker_session.h>
#undef Framebuffer

#include <os/texture_rgb565.h>
#include <os/texture_rgb888.h>
#include <os/dither_painter.h>

/* VirtualBox includes */

#include "Global.h"
#include "VirtualBoxBase.h"

class Genodefb :
	VBOX_SCRIPTABLE_IMPL(IFramebuffer)
{
	private:

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

		Genodefb ()
		:
			_fb_mode(_fb.mode()),
			_next_fb_mode(_fb_mode),
			_virtual_fb_mode(_fb_mode),
			_fb_base(Genode::env()->rm_session()->attach(_fb.dataspace()))
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

			bool ok = (w <= (ULONG)_next_fb_mode.width()) &&
			          (h <= (ULONG)_next_fb_mode.height());

			if (ok) {
				Genode::log("fb resize : [", screen, "] ",
				            _virtual_fb_mode.width(), "x",
				            _virtual_fb_mode.height(), " -> ",
				            w, "x", h);

				if ((w < (ULONG)_next_fb_mode.width()) ||
				    (h < (ULONG)_next_fb_mode.height())) {
					/* clear the old content around the new, smaller area. */
				    _clear_screen();
				}

				_fb_mode = _next_fb_mode;

				_virtual_fb_mode = Fb_Genode::Mode(w, h, Fb_Genode::Mode::RGB565);

				Genode::env()->rm_session()->detach(_fb_base);

				_fb_base = Genode::env()->rm_session()->attach(_fb.dataspace());

				result = S_OK;

			} else
				Genode::log("fb resize : [", screen, "] ",
				            _virtual_fb_mode.width(), "x",
				            _virtual_fb_mode.height(), " -> ",
				            w, "x", h, " ignored");

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
			Nitpicker::Area area_fb = Nitpicker::Area(_fb_mode.width(),
			                                          _fb_mode.height());
			Nitpicker::Area area_vm = Nitpicker::Area(width, height);

			using namespace Genode;

			typedef Pixel_rgb888 Pixel_src;
			typedef Pixel_rgb565 Pixel_dst;

			Texture<Pixel_src> texture((Pixel_src *)image, nullptr, area_vm);
			Surface<Pixel_dst> surface((Pixel_dst *)_fb_base, area_fb);

			Dither_painter::paint(surface, texture, Surface_base::Point(o_x, o_y));

			_fb.refresh(o_x, o_y, area_vm.w(), area_vm.h());

			return S_OK;
		}

		STDMETHODIMP COMGETTER(Overlay) (IFramebufferOverlay **) override {
			return E_NOTIMPL; }

		STDMETHODIMP COMGETTER(WinId) (PRInt64 *winId) override {
			return E_NOTIMPL; }

		STDMETHODIMP VideoModeSupported(ULONG, ULONG, ULONG, BOOL *) override {
			return E_NOTIMPL; }

		STDMETHODIMP Notify3DEvent(PRUint32, PRUint32, PRUint8 *) override {
			return E_NOTIMPL; }

		STDMETHODIMP ProcessVHWACommand(BYTE *pCommand) override {
		    return E_NOTIMPL; }

		STDMETHODIMP GetVisibleRegion(BYTE *, ULONG, ULONG *) override {
			return E_NOTIMPL; }
		
		STDMETHODIMP SetVisibleRegion(BYTE *, ULONG) override {
			return E_NOTIMPL; }

		STDMETHODIMP COMGETTER(PixelFormat) (ULONG *format) override {
			return E_NOTIMPL; }

		HRESULT NotifyUpdate(ULONG x, ULONG y, ULONG w, ULONG h) override {
			return E_NOTIMPL; }

		STDMETHODIMP COMGETTER(BitsPerPixel)(ULONG *bits) override {
			return E_NOTIMPL; }

		STDMETHODIMP COMGETTER(BytesPerLine)(ULONG *line) override {
			return E_NOTIMPL; }

		STDMETHODIMP COMGETTER(Width)(ULONG *width) override {
			return E_NOTIMPL; }

		STDMETHODIMP COMGETTER(Height)(ULONG *height) override {
			return E_NOTIMPL; }
};
