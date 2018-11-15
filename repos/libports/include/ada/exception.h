/*
 * \brief Ada exception declarations for C++
 * \author Johannes Kliemann
 * \date 2018-06-25
 *
 */

/*
 * Copyright (C) 2018 Componolit GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/exception.h>

namespace Ada {
	namespace Exception {
		class Undefined_Error                  : Genode::Exception {};

		class Program_Error                    : Genode::Exception {};
		class Constraint_Error                 : Genode::Exception {};
		class Storage_Error                    : Genode::Exception {};

		class Access_Check                     : Constraint_Error {};
		class Null_Access_Parameter            : Constraint_Error {};
		class Discriminant_Check               : Constraint_Error {};
		class Divide_By_Zero                   : Constraint_Error {};
		class Index_Check                      : Constraint_Error {};
		class Invalid_Data                     : Constraint_Error {};
		class Length_Check                     : Constraint_Error {};
		class Null_Exception_Id                : Constraint_Error {};
		class Null_Not_Allowed                 : Constraint_Error {};
		class Overflow_Check                   : Constraint_Error {};
		class Partition_Check                  : Constraint_Error {};
		class Range_Check                      : Constraint_Error {};
		class Tag_Check                        : Constraint_Error {};

		class Access_Before_Elaboration        : Program_Error {};
		class Accessibility_Check              : Program_Error {};
		class Address_Of_Intrinsic             : Program_Error {};
		class Aliased_Parameters               : Program_Error {};
		class All_Guards_Closed                : Program_Error {};
		class Bad_Predicated_Generic_Type      : Program_Error {};
		class Current_Task_In_Entry_Body       : Program_Error {};
		class Duplicated_Entry_Address         : Program_Error {};
		class Implicit_Return                  : Program_Error {};
		class Misaligned_Address_Value         : Program_Error {};
		class Missing_Return                   : Program_Error {};
		class Overlaid_Controlled_Object       : Program_Error {};
		class Non_Transportable_Actual         : Program_Error {};
		class Potentially_Blocking_Operation   : Program_Error {};
		class Stream_Operation_Not_Allowed     : Program_Error {};
		class Stubbed_Subprogram_Called        : Program_Error {};
		class Unchecked_Union_Restriction      : Program_Error {};
		class Finalize_Raised_Exception        : Program_Error {};

		class Empty_Storage_Pool               : Storage_Error {};
		class Infinite_Recursion               : Storage_Error {};
		class Object_Too_Large                 : Storage_Error {};
	}
}
