
#include <base/component.h>
#include <base/attached_rom_dataspace.h>
#include <dataspace/capability.h>
#include <os/static_root.h>

#include <framebuffer.h>

struct Main {

    Genode::Env &env;
    
    Genode::Attached_rom_dataspace rom_hip {
        env,
        "hypervisor_info_page"
    };

    Framebuffer::Session_component fb {
        env,
        rom_hip.local_addr<void>(),
    };

    Genode::Static_root<Framebuffer::Session> fb_root {env.ep().manage(fb)};

    Main(Genode::Env &env) : env(env)
    {
        env.parent().announce(env.ep().manage(fb_root));
    }
};

void Component::construct(Genode::Env &env){

    static Main inst(env);

}
