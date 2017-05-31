
#pragma once

#include <base/component.h>
#include <framebuffer_session/framebuffer_session.h>
#include <base/attached_io_mem_dataspace.h>
#include <base/attached_ram_dataspace.h>
#include <nova/syscalls.h>
#include <base/signal.h>
#include <util/reconstructible.h>
#include <timer_session/connection.h>

class Missing_multiboot2_fb : Genode::Exception {};

namespace Framebuffer {
    class Session_component;
}

namespace Pixel {
    struct RGB565;
    struct RGBA8888;
}

struct Pixel::RGB565
{
    Genode::uint8_t red : 5;
    Genode::uint8_t green : 6;
    Genode::uint8_t blue : 5;
} __attribute__((packed));

struct Pixel::RGBA8888
{
    Genode::uint8_t red;
    Genode::uint8_t green;
    Genode::uint8_t blue;
    Genode::uint8_t alpha;
} __attribute__((packed));

class Framebuffer::Session_component : public Genode::Rpc_object<Framebuffer::Session>
{
    private:

        Genode::Env &env;

        struct fb_desc {
            Genode::uint64_t addr;
            Genode::uint32_t pitch;
            Genode::uint32_t width;
            Genode::uint32_t height;
            Genode::uint8_t bpp;
            Genode::uint8_t type;
        } mbi_fb;
        
        Mode fb_mode;

        Genode::Constructible<Genode::Attached_io_mem_dataspace> fb_mem;
        Genode::Constructible<Genode::Attached_ram_dataspace> fb_ram;

        Timer::Connection timer { env };

        static inline void rgb565_to_rgba8888(Pixel::RGB565 *src, Pixel::RGBA8888 *dst)
        {
            dst->alpha = 0;
            dst->red = (src->red * 527 + 23) >> 6;
            dst->green = (src->green * 259 + 33) >> 6;
            dst->blue = (src->blue * 527 + 23) >> 6;
        }

    public:
        Session_component(Genode::Env &, void*); 
        Mode mode() const override;
        void mode_sigh(Genode::Signal_context_capability) override;
        void sync_sigh(Genode::Signal_context_capability) override;
        void refresh(int, int, int, int) override;
        Genode::Dataspace_capability dataspace() override;
};
