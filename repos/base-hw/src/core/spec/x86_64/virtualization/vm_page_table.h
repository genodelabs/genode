/*
 * \brief  VM page table abstraction between VMX and SVM for x86
 * \author Benjamin Lamowski
 * \date   2024-04-23
 */

/*
 * Copyright (C) 2024 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__SPEC__PC__VIRTUALIZATION__VM_PAGE_TABLE_H_
#define _CORE__SPEC__PC__VIRTUALIZATION__VM_PAGE_TABLE_H_

#include <base/log.h>
#include <util/construct_at.h>
#include <spec/x86_64/virtualization/ept.h>
#include <spec/x86_64/virtualization/hpt.h>

namespace Board {
	using namespace Genode;

	struct Vm_page_table
	{
		/* Both Ept and Hpt need to actually use this allocator */
		using Allocator = Genode::Page_table_allocator<1UL << SIZE_LOG2_4KB>;

		template <class T, class U>
		struct is_same {
			static const bool value = false;
		};

		template <class T>
		struct is_same <T, T> {
			static const bool value = true;
		};

		static_assert(is_same<Allocator, Hw::Ept::Allocator>::value,
			"Ept uses different allocator");
		static_assert(is_same<Allocator, Hw::Hpt::Allocator>::value,
			"Hpt uses different allocator");

		static constexpr size_t ALIGNM_LOG2 = Hw::SIZE_LOG2_4KB;

		enum Virt_type {
			VIRT_TYPE_NONE,
			VIRT_TYPE_VMX,
			VIRT_TYPE_SVM
		};

		union {
			Hw::Ept ept;
			Hw::Hpt hpt;
		};

		void insert_translation(addr_t vo,
				addr_t pa,
				size_t size,
				Page_flags const & flags,
				Allocator & alloc)
		{
			if (virt_type() == VIRT_TYPE_VMX)
				ept.insert_translation(vo, pa, size, flags, alloc);
			else if (virt_type() == VIRT_TYPE_SVM)
				hpt.insert_translation(vo, pa, size, flags, alloc);
		}

		void remove_translation(addr_t vo, size_t size, Allocator & alloc)
		{
			if (virt_type() == VIRT_TYPE_VMX)
				ept.remove_translation(vo, size, alloc);
			else if (virt_type() == VIRT_TYPE_SVM)
				hpt.remove_translation(vo, size, alloc);
		}

		static Virt_type virt_type() {
			static Virt_type virt_type { VIRT_TYPE_NONE };

			if (virt_type == VIRT_TYPE_NONE) {
				if (Hw::Virtualization_support::has_vmx())
					virt_type = VIRT_TYPE_VMX;
				else if (Hw::Virtualization_support::has_svm())
					virt_type = VIRT_TYPE_SVM;
				else
					error("Failed to detect Virtualization technology");
			}

			return virt_type;
		}

		Vm_page_table()
		{
			if (virt_type() == VIRT_TYPE_VMX)
				Genode::construct_at<Hw::Ept>(this);
			else if (virt_type() == VIRT_TYPE_SVM)
				Genode::construct_at<Hw::Hpt>(this);
		}
	};

	using Vm_page_table_array =
		Vm_page_table::Allocator::Array<Kernel::DEFAULT_TRANSLATION_TABLE_MAX>;
};

#endif /* _CORE__SPEC__PC__VIRTUALIZATION__VM_PAGE_TABLE_H_ */
