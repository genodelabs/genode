/*
 * \brief  Regulator driver interface
 * \author Alexander Tarasikov <tarasikov@ksyslabs.org>
 * \date   2013-02-18
 */

/*
 * Copyright (C) 2013 Ksys Labs LLC
 * Copyright (C) 2011-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _REGULATOR_DRIVER_H_
#define _REGULATOR_DRIVER_H_

#include <util/list.h>
#include <base/env.h>
#include <base/allocator.h>

namespace Regulator {
	typedef Genode::List<Genode::List_element<char> > NameList;
	typedef Genode::List_element<char> NameListElt;

	class AbstractRegulator {
	protected:
		unsigned mMin;
		unsigned mMax;
		unsigned mNumLevelSteps;
		unsigned mDelay;
		unsigned mRefCount;

	public:
		const unsigned int id;
		const char *name;

		virtual bool raw_enable(void) = 0;
		virtual bool raw_disable(void) = 0;
		virtual bool is_enabled(void) = 0;
		virtual enum regulator_state get_state(void) = 0;
		virtual bool set_state(enum regulator_state state) = 0;
		
		virtual bool enable(void) {
			mRefCount++;
		
			if (!is_enabled()) {
				return raw_enable();
			}

			return true;
		}

		virtual bool disable(void) {
			if (mRefCount) {
				mRefCount--;
			}
			else if (!is_enabled()) {
				PERR("trying to disable \'%s\' when already off", name);
				return false;
			}

			if (!mRefCount) {
				return raw_disable();
			}
			return true;
		}
		
		virtual unsigned min_level(void) {
			return mMin;
		}

		virtual unsigned num_level_steps(void) {
			return mNumLevelSteps;
		}

		/* default to fixed level level */
		virtual int get_level(void) {
			return mMin;
		}

		virtual bool set_level(unsigned min, unsigned max) {
			return false;
		}
		
		AbstractRegulator(unsigned id, const char* name) :
			mMin(0), mMax(0), mNumLevelSteps(1), mDelay(0), mRefCount(0),
			id(id), name(name) {}
		virtual ~AbstractRegulator() {}
	};

	class Driver
	{
	protected:
		AbstractRegulator **mRegulators;
		unsigned mNumRegulators;
		NameList *mAllowedRegulators;
		
		AbstractRegulator *lookup(unsigned regulator_id) {
			if (!mRegulators) {
				return NULL;
			}

			for (unsigned i = 0; i < mNumRegulators; i++) {
				if (!mRegulators[i]) {
					continue;
				}

				if (mRegulators[i]->id == regulator_id) {
					return mRegulators[i];
				}
			}
			
			return NULL;
		}

	public:
		Driver() : mRegulators(0), mNumRegulators(0), mAllowedRegulators(0) {}
		Driver(NameList *allowed_regulators,
			AbstractRegulator **regulators, unsigned num_regulators)
			: mAllowedRegulators(allowed_regulators)
		{
			int num_new_regulators = 0;
			NameListElt *elt;

			//first, find out how many entries we need to allocate
			for (elt = allowed_regulators->first(); elt; elt = elt->next()) {
				for (unsigned i = 0; i < num_regulators; i++) {
					if (!Genode::strcmp(elt->object(), regulators[i]->name)) {
						num_new_regulators++;
					}
				}
			}

			mRegulators = new (Genode::env()->heap())
				AbstractRegulator*[num_new_regulators];
			mNumRegulators = num_new_regulators;

			num_new_regulators = 0;
			//now, fill the data
			for (elt = allowed_regulators->first(); elt; elt = elt->next()) {
				for (unsigned i = 0; i < num_regulators; i++) {
					if (!Genode::strcmp(elt->object(), regulators[i]->name)) {
						mRegulators[num_new_regulators++] = regulators[i];
					}
				}
			}
		}

		virtual ~Driver() {
			Genode::destroy(Genode::env()->heap(), mRegulators);
			if (mAllowedRegulators) {
				NameListElt *elt;
				for (elt = mAllowedRegulators->first(); elt; elt = elt->next()) {
					Genode::destroy(Genode::env()->heap(), elt->object());
				}
				Genode::destroy(Genode::env()->heap(), mAllowedRegulators);
			}
		};
	
		virtual bool enable(unsigned regulator_id) {
			AbstractRegulator *vreg = lookup(regulator_id);
			if (!vreg) {
				return false;
			}
			
			return vreg->enable();
		}

		virtual bool disable(unsigned regulator_id) {
			AbstractRegulator *vreg = lookup(regulator_id);
			if (!vreg) {
				return false;
			}

			return vreg->disable();
		}
			
		bool is_enabled(unsigned regulator_id) {
			AbstractRegulator *vreg = lookup(regulator_id);
			if (!vreg) {
				return false;
			}
			return vreg->is_enabled();
		}

		enum regulator_state get_state(unsigned regulator_id) {
			AbstractRegulator *vreg = lookup(regulator_id);
			if (!vreg) {
				return STATE_ERROR;
			}

			return vreg->get_state();
		}

		bool set_state(unsigned regulator_id, enum regulator_state state) {
			AbstractRegulator *vreg = lookup(regulator_id);
			if (!vreg) {
				return false;
			}

			return vreg->set_state(state);
		}

		unsigned min_level(unsigned regulator_id) {
			AbstractRegulator *vreg = lookup(regulator_id);
			if (!vreg) {
				return 0;
			}

			return vreg->min_level();
		}

		unsigned num_level_steps(unsigned regulator_id) {
			AbstractRegulator *vreg = lookup(regulator_id);
			if (!vreg) {
				return 1;
			}

			return vreg->num_level_steps();
		}

		int get_level(unsigned regulator_id) {
			AbstractRegulator *vreg = lookup(regulator_id);
			if (!vreg) {
				return -1;
			}

			return vreg->get_level();
		}

		bool set_level(unsigned regulator_id,
			unsigned min_uV, unsigned max_uV)
		{
			AbstractRegulator *vreg = lookup(regulator_id);
			if (!vreg) {
				return false;
			}

			return vreg->set_level(min_uV, max_uV);
		}
	};

	/**
	 * Interface for constructing the driver objects
	 */
	struct Driver_factory
	{
		struct Not_available { };

		virtual Driver *create(NameList *allowed_regulators) = 0;

		/**
		 * Destroy driver
		 */
		virtual void destroy(Driver *driver) = 0;
	};

}

#endif /* _REGULATOR_DRIVER_H_ */
