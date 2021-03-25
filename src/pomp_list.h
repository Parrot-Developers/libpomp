/**
 * @file pomp_list.h
 *
 * @brief linked list.
 *
 * @author yves-marie.morgan@parrot.com
 *
 * Copyright (c) 2021 Parrot Donres SAS.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of the Parrot Company nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE PARROT COMPANY BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef _POMP_LIST_H_
#define _POMP_LIST_H_

/**
 * POMP_CONTAINER_OF
 * cast a member of a structure out to the containing structure
 *
 * @param ptr the pointer to the member.
 * @param type the type of the container struct this is embedded in.
 * @param member the name of the member within the struct.
 * @return base address of member containing structure
 **/
#define POMP_CONTAINER_OF(ptr, type, member)			\
({								\
	const __typeof__(((type *)0)->member) * __mptr = (ptr);	\
	(type *)((uintptr_t)__mptr - offsetof(type, member));	\
})

/**
 * list node
 */
struct pomp_list_node {
	struct pomp_list_node *next, *prev;
};

static inline void pomp_list_init(struct pomp_list_node *list)
{
	list->next = list;
	list->prev = list;
}

static inline struct pomp_list_node *pomp_list_prev(
		const struct pomp_list_node *list,
		const struct pomp_list_node *item)
{
	return (item->prev != list) ? item->prev : NULL;
}

static inline struct pomp_list_node *pomp_list_next(
		const struct pomp_list_node *list,
		const struct pomp_list_node *item)
{
	return (item->next != list) ? item->next : NULL;
}

static inline struct pomp_list_node *pomp_list_first(
		const struct pomp_list_node *list)
{
	return list->next;
}

static inline struct pomp_list_node *pomp_list_last(
		const struct pomp_list_node *list)
{
	return list->prev;
}

static inline int pomp_list_is_empty(const struct pomp_list_node *list)
{
	return list->next == list;
}

static inline void pomp_list_add(struct pomp_list_node *novel,
		struct pomp_list_node *prev,
		struct pomp_list_node *next)
{
	next->prev = novel;
	novel->next = next;
	novel->prev = prev;
	prev->next = novel;
}

static inline void pomp_list_add_after(
		struct pomp_list_node *node,
		struct pomp_list_node *novel)
{
	pomp_list_add(novel, node, node->next);
}

static inline void pomp_list_add_before(
		struct pomp_list_node *node,
		struct pomp_list_node *novel)
{
	pomp_list_add(novel, node->prev, node);
}

static inline void pomp_list_add_tail(
		const struct pomp_list_node *list,
		struct pomp_list_node *novel)
{
	pomp_list_add_after(pomp_list_last(list), novel);
}

static inline void pomp_list_add_head(
		const struct pomp_list_node *list,
		struct pomp_list_node *novel)
{
	pomp_list_add_before(pomp_list_first(list), novel);
}

static inline void pomp_list_remove(struct pomp_list_node *node)
{
	node->next->prev = node->prev;
	node->prev->next = node->next;
	node->next = NULL;
	node->prev = NULL;
}

#define pomp_list_entry(ptr, type, member)\
	POMP_CONTAINER_OF(ptr, type, member)

#define pomp_list_walk_forward(list, pos) \
	for (pos = (list)->next; pos != (list); pos = pos->next)

#define pomp_list_walk_backward(list, pos) \
	for (pos = (list)->prev; pos != (list);	pos = pos->prev)

#define pomp_list_walk_forward_safe(list, pos, tmp)	\
	for (pos = (list)->next,			\
			tmp = pos->next; pos != (list);	\
			pos = tmp, tmp = pos->next)

#define pomp_list_walk_backward_safe(list, pos, tmp)	\
	for (pos = (list)->prev,			\
			tmp = pos->prev; pos != (list);	\
			pos = tmp, tmp = pos->prev)

#endif /* _POMP_LIST_H_ */
