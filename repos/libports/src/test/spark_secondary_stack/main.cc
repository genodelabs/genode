
#include <base/log.h>
#include <base/component.h>

extern "C" void stack__calloc(int);
extern "C" void stack__ralloc();
extern "C" void stack__salloc();
extern "C" void adainit();
extern "C" void adafinal();

extern "C" void print_stack(char* data)
{
    Genode::log(Genode::Cstring(data));
}

extern "C" void print_recursion(int r)
{
    Genode::log("recursion: ", r);
}

extern "C" void print_stage(int s)
{
    Genode::log("stage: ", s);
}

void Component::construct(Genode::Env &env)
{
    adainit();
    Genode::log("running iteration test");
    stack__calloc(32);
    stack__calloc(128);
    stack__calloc(512);
    stack__calloc(1024);

    Genode::log("running recursion test");
    stack__ralloc();

    Genode::log("running stage test");
    stack__salloc();

    Genode::log("secondary stack test successful");
    adafinal();

    env.parent().exit(0);
}
