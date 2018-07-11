/*
 * \brief Implementation of Ada exception functions, propagating to C++
 * \author Alexander Senier
 * \author Johannes Kliemann
 * \date 2018-04-16
 *
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 * Copyright (C) 2018 Componolit GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/log.h>
#include <util/string.h>
#include <ada/exception.h>

extern "C" {

	/* Program Error */
	void __gnat_rcheck_PE_Explicit_Raise(char *file, int line)
	{
		Genode::error("Program Error in ", Genode::Cstring(file), " at line ", line);
		throw Ada::Exception::Program_Error();
	}

	/* Constraint Error */
	void constraint_error(char *file, int line)
	{
		Genode::error("Constraint Error in ", Genode::Cstring(file), " at line ", line);
		throw Ada::Exception::Constraint_Error();
	}

	void __gnat_rcheck_CE_Explicit_Raise(char *file, int line)
	{
		constraint_error(file, line);
	}

	/* Storage Error */
	void __gnat_rcheck_SE_Explicit_Raise(char *file, int line)
	{
		Genode::error("Storage Error in ", Genode::Cstring(file), " at line ", line);
		throw Ada::Exception::Storage_Error();
	}

	/* Constraint Error subtypes */

	/* Length check failed */
	void __gnat_rcheck_CE_Length_Check(char *file, int line)
	{
		Genode::error("Constraint Error: Length check failed in ",
		              Genode::Cstring(file), " at line ", line);
		throw Ada::Exception::Length_Check();
	}

	/* Overflow check failed */
	void __gnat_rcheck_CE_Overflow_Check(char *file, int line)
	{
		Genode::error("Constraint Error: Overflow check failed in ",
		              Genode::Cstring(file), " at line ", line);
		throw Ada::Exception::Overflow_Check();
	}

	/* Invalid data */
	void __gnat_rcheck_CE_Invalid_Data(char *file, int line)
	{
		Genode::error("Constraint Error: Invalid data in ",
		              Genode::Cstring(file), " at line ", line);
		throw Ada::Exception::Invalid_Data();
	}

	/* Range check failed */
	void __gnat_rcheck_CE_Range_Check(char *file, int line)
	{
		Genode::error("Constraint Error: Range check failed in ",
		              Genode::Cstring(file), " at line ", line);
		throw Ada::Exception::Range_Check();
	}

	/* Index check failed */
	void __gnat_rcheck_CE_Index_Check(char *file, int line)
	{
		Genode::error("Constraint Error: Index check failed in ",
		              Genode::Cstring(file), " at line ", line);
		throw Ada::Exception::Index_Check();
	}

	/* Discriminant check failed */
	void __gnat_rcheck_CE_Discriminant_Check(char *file, int line)
	{
		Genode::error("Constraint Error: Discriminant check failed in ",
		              Genode::Cstring(file), " at line ", line);
		throw Ada::Exception::Discriminant_Check();
	}

	/* Divide by 0 */
	void __gnat_rcheck_CE_Divide_By_Zero(char *file, int line)
	{
		Genode::error("Constraint Error: Divide by zero in ",
		              Genode::Cstring(file), " at line ", line);
		throw Ada::Exception::Divide_By_Zero();
	}

	void raise_ada_exception(char *name, char *message)
	{
		Genode::error(Genode::Cstring(name), " raised: ", Genode::Cstring(message));
	}

	void warn_unimplemented_function(char *func)
	{
		Genode::warning(Genode::Cstring(func), " unimplemented");
	}

}
