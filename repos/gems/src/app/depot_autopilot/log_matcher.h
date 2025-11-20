/*
 * \brief  Utility for matching log output against pattern
 * \author Martin Stein
 * \author Norman Feske
 * \date   2025-11-12
 */

/*
 * Copyright (C) 2025 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _LOG_MATCHER_H_
#define _LOG_MATCHER_H_

/* local includes */
#include <string_chain.h>

namespace Depot_autopilot {

	struct Log_buffer : String_chain { using String_chain::String_chain; };

	struct Log_matcher;
}


struct Depot_autopilot::Log_matcher : Noncopyable
{
	/*
	 * Defines a point inside the log buffer, up to which the log has been
	 * processed already.
	 */
	size_t _log_processed = 0;

	/*
	 * Defines a point inside the pattern buffer, up to which the pattern was
	 * successfully matched against the log so far.
	 */
	size_t _pattern_processed = 0;

	String_chain _pattern;

	Log_matcher(Allocator &alloc, Span const &pattern)
	:
		_pattern(alloc)
	{
		/* strip off leading/trailing tabs and spaces */
		auto trimmed = [&] (Span const &span, auto const &fn)
		{
			auto space = [] (char c) { return c == ' ' || c == '\t'; };

			char const *s = span.start; size_t n = span.num_bytes;
			for (; n && space(s[0]); s++, n--);
			for (; n && space(s[n - 1]); n--);

			return fn(Span(s, n));
		};

		auto prepend_newline = [] (Span const &span, auto const &fn)
		{
			char buf[1 + span.num_bytes] { };
			buf[0] = '\n';
			memcpy(buf + 1, span.start, span.num_bytes);
			fn(Span { buf, sizeof(buf) });
		};

		pattern.split('\n', [&] (Span const &line) {
			trimmed(line, [&] (Span const &trimmed_line) {
				if (!trimmed_line.num_bytes)
					return;

				bool first = true;
				trimmed_line.split('*', [&] (Span const &fragment) {
					if (fragment.num_bytes) {

						/* imprint newline at the begin of each pattern line */
						if (first)
							prepend_newline(fragment, [&] (Span const &nl_fragment) {
								_pattern.append(nl_fragment); });
						else
							_pattern.append(fragment);
					}
					first = false;
				});
			});
		});
	}

	/**
	 * Incorporate added log-buffer content and evaluate the new state
	 *
	 * \return true if log-buffer content matches the pattern
	 */
	inline bool track_and_match(Log_buffer const &);
};


bool Depot_autopilot::Log_matcher::track_and_match(Log_buffer const &log_buffer)
{
	if (_pattern.num_bytes() == 0)
		return false;

	struct Result { bool matched, done; } result { };

	while (!result.done) {

		/*
		 * Determine the pattern element that covers the point defined by
		 * _pattern_processed. I.e., the first fragment of the pattern that
		 * could not be fully matched against the log yet.
		 */
		_pattern.with_span_at(_pattern_processed,
			[&] (Span const &pattern_part, size_t const pattern_elem_offset) {
				log_buffer.with_span_at(_log_processed,
					[&] (Span const &log_part, size_t) {

						/*
						 * Compare the yet unmatched pattern bytes to the yet
						 * unprocessed log bytes.
						 */
						size_t const n = min(log_part    .num_bytes,
						                     pattern_part.num_bytes);

						if (!memcmp(pattern_part.start, log_part.start, n)) {

							/* advance */
							_pattern_processed += n;
							_log_processed     += n;
							return;
						}

						/*
						 * Rewind previous partial match
						 *
						 * If the offset into the pattern element is > 0, this
						 * means that the element could be matched partially
						 * against the less advanced log buffer during the last
						 * update. If the remaining bytes now fail to match
						 * against the just arrived subsequent log bytes, we
						 * must discard the partial match and try to match the
						 * whole pattern element again.
						 */
						_pattern_processed -= pattern_elem_offset;
						_log_processed     -= pattern_elem_offset;

						_log_processed += 1; /* skip mismatching log byte */
					},
					[&] {
						/* no log left to process -> no match yet */
						result = { .matched = false, .done = true };
					});
			},
			[&] {
				/* no pattern left to compare -> match */
				result = { .matched = true, .done = true };
			});
	}
	return result.matched;
}

#endif /* _LOG_MATCHER_H_ */
