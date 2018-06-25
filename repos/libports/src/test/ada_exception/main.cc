
#include <base/log.h>
#include <base/component.h>

extern "C" void except__raise_task();

void Component::construct(Genode::Env &env)
{
    Genode::log("Ada exception test");

    except__raise_task();

    env.parent().exit(0);
}
