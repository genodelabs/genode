/*
 * \brief  Linux kernel API
 * \author Sebastian Sumpf
 * \date   2016-03-31
 *
 * Based on the prototypes found in the Linux kernel's 'include/'.
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */


/************************
 ** linux/completion.h **
 ************************/

struct completion;

void complete(struct completion *);
void complete_all(struct completion *);
void init_completion(struct completion *c);
bool try_wait_for_completion(struct completion *);

void          wait_for_completion(struct completion *c);
unsigned long wait_for_completion_timeout(struct completion *c,
                                          unsigned long timeout);
int           wait_for_completion_interruptible(struct completion *c);
long          wait_for_completion_interruptible_timeout(struct completion *c,
                                                        unsigned long timeout);
