/*
 * \brief  Example of implementing an object in Ada/SPARK
 * \author Norman Feske
 * \date   2018-12-18
 */

#ifndef _MACHINERY_H_
#define _MACHINERY_H_

/* Genode includes */
#include <base/log.h>

namespace Spark {

	/**
	 * Opaque object that contains the space needed to store a SPARK record.
	 *
	 * \param BYTES  size of the SPARK record in bytes
	 */
	template <Genode::size_t BYTES>
	struct Object
	{
		/**
		 * Exception type
		 */
		struct Object_size_mismatch { };

		static constexpr Genode::size_t bytes() { return BYTES; }

		long _space[(BYTES + sizeof(long) - 1)/sizeof(long)] { };
	};

	struct Machinery : Object<4>
	{
		Machinery();

		void heat_up();

		Genode::size_t temperature() const;
	};

	Genode::size_t number_of_bits(Machinery const &);

	template <typename T>
	static inline void assert_valid_object_size()
	{
		if (number_of_bits(*(T *)nullptr) > T::bytes()*8)
			throw typename T::Object_size_mismatch();
	}
}


static inline void test_spark_object_construction()
{
	using namespace Genode;

	Spark::assert_valid_object_size<Spark::Machinery>();

	Spark::Machinery machinery { };

	auto check = [&] (char const *msg, Genode::uint32_t expected)
	{
		Genode::uint32_t const value = machinery.temperature();
		log("machinery temperature ", msg, " is ", value);

		class Spark_object_construction_failed { };
		if (value != expected)
			throw Spark_object_construction_failed();
	};

	check("after construction", 25);

	machinery.heat_up();

	check("after heating up", 77);
}

#endif /* _MACHINERY_H_ */
