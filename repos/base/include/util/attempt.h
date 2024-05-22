/*
 * \brief  Utility for passing return values
 * \author Norman Feske
 * \date   2021-11-09
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__UTIL__ATTEMPT_H_
#define _INCLUDE__UTIL__ATTEMPT_H_

namespace Genode { template <typename, typename> struct Attempt; }


/**
 * Option type for return values
 *
 * The 'Attempt' type addresses the C++ limitation to only a single return
 * value, which makes it difficult to propagate error information in addition
 * to an actual return value from a called function back to the caller. Hence,
 * errors have to be propagated as exceptions, results have to be returned via
 * out parameters, or error codes have to be encoded in the form of magic
 * values. Each of these approaches create its own set of robustness problems.
 *
 * An 'Attempt' represents the result of a function call that is either a
 * meaningful value or an error code, but never both. The result value and
 * error are distinct types. To consume the return value of a call, the caller
 * needs to specify two functors, one for handing the value if the value exists
 * (the call was successful), and one for handling the error value if the call
 * failed. Thereby the use of an 'Attempt' return type reinforces the explicit
 * handling of all possible error conditions at the caller site.
 */
template <typename RESULT, typename ERROR>
class Genode::Attempt
{
	private:

		bool   _ok;
		RESULT _result { };
		ERROR  _error  { };

	public:

		Attempt(RESULT result) : _ok(true),  _result(result) { }
		Attempt(ERROR  error)  : _ok(false), _error(error)   { }

		Attempt(Attempt const &)              = default;
		Attempt &operator = (Attempt const &) = default;

		template <typename RET>
		RET convert(auto const &access_fn, auto const &fail_fn) const
		{
			return _ok ? RET { access_fn(_result) }
			           : RET { fail_fn(_error) };
		}

		void with_result(auto const &access_fn, auto const &fail_fn) const
		{
			_ok ? access_fn(_result) : fail_fn(_error);
		}

		void with_error(auto const &fail_fn) const
		{
			if (!_ok)
				fail_fn(_error);
		}

		bool operator == (ERROR const &rhs) const {
			return failed() && (_error == rhs); }

		bool operator == (RESULT const &rhs) const {
			return ok()     && (_result == rhs); }

		bool ok()     const { return  _ok; }
		bool failed() const { return !_ok; }
};

#endif /* _INCLUDE__UTIL__ATTEMPT_H_ */
