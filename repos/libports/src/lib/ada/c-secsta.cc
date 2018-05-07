
#include <base/log.h>
#include <base/thread.h>

/*
 * The Mark_Id type defined in libports/src/lib/ada/libsrc/s-secsta.ads
 * equal to the type defined in GCCs own implementation of s-secsta.ads.
 * If the implementation used by GCC changes it needs to be changed in
 * this library as well.
 */

#if !(__GNUC__ == 6 && __GNUC_MINOR__ == 3)
    #warning Unsupported compiler version, check s-secsta.ads
#endif

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
