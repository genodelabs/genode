
#include <base/log.h>

extern "C" {

    void __gnat_rcheck_CE_Explicit_Raise()
    {
        Genode::warning("Unhandled Ada exception: Constraint_Error");
    }

    void __gnat_rcheck_SE_Explicit_Raise()
    {
        Genode::warning("Unhandled Ada exception: Storage_Error");
    }

    void __gnat_rcheck_PE_Explicit_Raise()
    {
        Genode::warning("Unhandled Ada exception: Program_Error");
    }

}
