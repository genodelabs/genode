
#include <base/log.h>
#include <base/thread.h>

extern "C" {

    void *get_thread()
    {
        return static_cast<void *>(Genode::Thread::myself());
    }

    void *allocate_secondary_stack(void *thread, Genode::size_t size)
    {
        return static_cast<Genode::Thread *>(thread)->alloc_secondary_stack("ada thread", size);
    }

}
