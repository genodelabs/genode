
#include <framebuffer.h>

using namespace Framebuffer;

Session_component::Session_component(Genode::Env &i_env, void* hip_rom) : env(i_env)
{
    Genode::log("Initializing Multiboot2 framebuffer");
    
    Nova::Hip *hip = reinterpret_cast<Nova::Hip*>(hip_rom);

    mbi_fb = *(struct fb_desc*)(void*)&hip->fb_desc;

    if(!mbi_fb.addr){
        Genode::error("Invalid framebuffer address");
        throw Missing_multiboot2_fb();
    }

    Genode::log("Framebuffer with ", mbi_fb.width, "x", mbi_fb.height, "x", mbi_fb.bpp, " @ ", (void*)mbi_fb.addr);

    fb_mem.construct(
        env,
        mbi_fb.addr,
        mbi_fb.width * mbi_fb.height * mbi_fb.bpp / 4,
        true);

    fb_mode = Mode(mbi_fb.width, mbi_fb.height, Mode::RGB565);

    fb_ram.construct(
        env.ram(),
        env.rm(),
        mbi_fb.width * mbi_fb.height * fb_mode.bytes_per_pixel());
}

Mode Session_component::mode() const
{
    return fb_mode;
}

void Session_component::mode_sigh(Genode::Signal_context_capability _scc){};

void Session_component::sync_sigh(Genode::Signal_context_capability scc){
    timer.sigh(scc);
    timer.trigger_periodic(10*1000);
};

void Session_component::refresh(int x, int y, int w, int h){
    Genode::uint32_t u_x = (Genode::uint32_t)Genode::min(mbi_fb.width,  (Genode::uint32_t)Genode::max(x, 0));
    Genode::uint32_t u_y = (Genode::uint32_t)Genode::min(mbi_fb.height, (Genode::uint32_t)Genode::max(y, 0));
    Genode::uint32_t u_w = (Genode::uint32_t)Genode::min(mbi_fb.width,  (Genode::uint32_t)Genode::max(w, 0) + u_x);
    Genode::uint32_t u_h = (Genode::uint32_t)Genode::min(mbi_fb.height, (Genode::uint32_t)Genode::max(h, 0) + u_y);
    Pixel::RGBA8888 *pixel_32 = fb_mem->local_addr<Pixel::RGBA8888>();
    Pixel::RGB565 *pixel_16 = fb_ram->local_addr<Pixel::RGB565>();
    for(Genode::uint32_t r = u_y; r < u_h; ++r){
        for(Genode::uint32_t c = u_x; c < u_w; ++c){
            Session_component::rgb565_to_rgba8888(
                    &pixel_16[c + r * mbi_fb.width],
                    &pixel_32[c + r * mbi_fb.width]
                    );
        }
    }
}

Genode::Dataspace_capability Session_component::dataspace()
{
    return fb_ram->cap();
}
