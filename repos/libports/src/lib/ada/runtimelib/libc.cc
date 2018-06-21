#include <base/stdint.h>
#include <util/string.h>

extern "C" {

    int memcmp(const void *s1, const void *s2, Genode::size_t n)
    {
        return Genode::memcmp(s1, s2, n);
    }
}
