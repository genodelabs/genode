/*
 * \brief  Integration of the Consistent Block Encrypter (CBE)
 * \author Martin Stein
 * \author Josef Soentgen
 * \date   2020-11-10
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CBE_SPARK_OBJECT_H_
#define _CBE_SPARK_OBJECT_H_

/* Genode includes */
#include <base/stdint.h>
#include <base/output.h>
#include <base/log.h>

namespace Cbe {

	/**
	 * Opaque object that contains the space needed to store a SPARK record.
	 *
	 * \param BYTES  size of the SPARK record in bytes
	 */
	template <Genode::uint32_t BYTES>
	struct Spark_object
	{
		/**
		 * Exception type
		 */
		struct Object_size_mismatch { };

		static constexpr Genode::uint32_t bytes() { return BYTES; }

		long _space[(BYTES + sizeof(long) - 1)/sizeof(long)] { };
	};

	template <typename T>
	static inline void assert_valid_object_size()
	{
		if (object_size(*(T *)nullptr) > T::bytes()) {
			Genode::error("need ", object_size(*(T *)nullptr),
			              " bytes, got ", T::bytes(), " bytes");
			throw typename T::Object_size_mismatch();
		}
	}

	template <typename T>
	static inline void assert_same_object_size()
	{
		if (object_size(*(T *)nullptr) != T::bytes()) {
			Genode::error("need ", object_size(*(T *)nullptr),
			              " bytes, got ", T::bytes(), " bytes");
			throw typename T::Object_size_mismatch();
		}
	}

} /* namespace Cbe */

#endif /* _CBE_SPARK_OBJECT_H_ */
