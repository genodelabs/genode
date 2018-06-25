
#include <base/exception.h>

namespace Ada {
    namespace Exception {
        class Program_Error : Genode::Exception {};
        class Constraint_Error : Genode::Exception {};
        class Storage_Error : Genode::Exception {};

        class Length_Check : Constraint_Error {};
        class Overflow_Check : Constraint_Error {};
        class Invalid_Data : Constraint_Error {};
        class Range_Check : Constraint_Error {};
        class Index_Check : Constraint_Error {};
        class Discriminant_Check : Constraint_Error {};
        class Divide_By_Zero : Constraint_Error {};
    };
};
