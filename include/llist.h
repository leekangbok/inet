#ifndef _LLIST_H
#define _LLIST_H

#include <stdio.h>

/**
 * @name from other kernel headers
 */
/*@{*/

/**
 * Get offset of a member
 */
#define __offsetof__(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)

/**
 * Casts a member of a structure out to the containing structure
 * @param ptr        the pointer to the member.
 * @param type       the type of the container struct this is embedded in.
 * @param member     the name of the member within the struct.
 *
 */
#define __container_of__(ptr, type, member) ({                      \
					     const typeof( ((type *)0)->member ) *__mptr = (ptr);    \
					     (type *)( (char *)__mptr - __offsetof__(type,member) );})
/*@}*/


/*
 * These are non-NULL pointers that will result in page faults
 * under normal circumstances, used to verify that nobody uses
 * non-initialized list entries.
 */
#define LL_POISON1  ((void *) 0x00100100)
#define LL_POISON2  ((void *) 0x00200200)

/**
 * Simple doubly linked list implementation.
 *
 * Some of the internal functions ("__xxx") are useful when
 * manipulating whole lists rather than single entries, as
 * sometimes we already know the next/prev entries and we can
 * generate better code by using them directly rather than
 * using the generic single-entry routines.
 */
struct ll_head {
	struct ll_head *next, *prev;
};

#define LL_HEAD_INIT(name) { &(name), &(name) }

#define LL_HEAD(name) \
	struct ll_head name = LL_HEAD_INIT(name)

#define INIT_LL_HEAD(ptr) do { \
	(ptr)->next = (ptr); (ptr)->prev = (ptr); \
} while (0)

/*
 * Insert a new entry between two known consecutive entries.
 *
 * This is only for internal list manipulation where we know
 * the prev/next entries already!
 */
static inline void __ll_add(struct ll_head *new,
			    struct ll_head *prev,
			    struct ll_head *next)
{
	next->prev = new;
	new->next = next;
	new->prev = prev;
	prev->next = new;
}

/**
 * ll_add - add a new entry
 * @new: new entry to be added
 * @head: list head to add it after
 *
 * Insert a new entry after the specified head.
 * This is good for implementing stacks.
 */
static inline void ll_add(struct ll_head *new, struct ll_head *head)
{
	__ll_add(new, head, head->next);
}

/**
 * ll_add_tail - add a new entry
 * @new: new entry to be added
 * @head: list head to add it before
 *
 * Insert a new entry before the specified head.
 * This is useful for implementing queues.
 */
static inline void ll_add_tail(struct ll_head *new, struct ll_head *head)
{
	__ll_add(new, head->prev, head);
}


/*
 * Delete a list entry by making the prev/next entries
 * point to each other.
 *
 * This is only for internal list manipulation where we know
 * the prev/next entries already!
 */
static inline void __ll_del(struct ll_head * prev, struct ll_head * next)
{
	next->prev = prev;
	prev->next = next;
}

/**
 * ll_del - deletes entry from list.
 * @entry: the element to delete from the list.
 * Note: ll_empty on entry does not return true after this, the entry is
 * in an undefined state.
 */
static inline void ll_del(struct ll_head *entry)
{
	__ll_del(entry->prev, entry->next);
	entry->next = LL_POISON1;
	entry->prev = LL_POISON2;
}



/**
 * ll_del_init - deletes entry from list and reinitialize it.
 * @entry: the element to delete from the list.
 */
static inline void ll_del_init(struct ll_head *entry)
{
	__ll_del(entry->prev, entry->next);
	INIT_LL_HEAD(entry);
}

/**
 * ll_move - delete from one list and add as another's head
 * @list: the entry to move
 * @head: the head that will precede our entry
 */
static inline void ll_move(struct ll_head *list, struct ll_head *head)
{
	__ll_del(list->prev, list->next);
	ll_add(list, head);
}

/**
 * ll_move_tail - delete from one list and add as another's tail
 * @list: the entry to move
 * @head: the head that will follow our entry
 */
static inline void ll_move_tail(struct ll_head *list,
				struct ll_head *head)
{
	__ll_del(list->prev, list->next);
	ll_add_tail(list, head);
}

/**
 * ll_empty - tests whether a list is empty
 * @head: the list to test.
 */
static inline int ll_empty(const struct ll_head *head)
{
	return head->next == head;
}

static inline void __ll_splice(struct ll_head *list,
			       struct ll_head *head)
{
	struct ll_head *first = list->next;
	struct ll_head *last = list->prev;
	struct ll_head *at = head->next;

	first->prev = head;
	head->next = first;

	last->next = at;
	at->prev = last;
}

/**
 * ll_splice - join two lists
 * @list: the new list to add.
 * @head: the place to add it in the first list.
 */
static inline void ll_splice(struct ll_head *list, struct ll_head *head)
{
	if (!ll_empty(list))
		__ll_splice(list, head);
}

/**
 * ll_splice_init - join two lists and reinitialise the emptied list.
 * @list: the new list to add.
 * @head: the place to add it in the first list.
 *
 * The list at @list is reinitialised
 */
static inline void ll_splice_init(struct ll_head *list,
				  struct ll_head *head)
{
	if (!ll_empty(list)) {
		__ll_splice(list, head);
		INIT_LL_HEAD(list);
	}
}

/**
 * ll_entry - get the struct for this entry
 * @ptr:	the &struct ll_head pointer.
 * @type:	the type of the struct this is embedded in.
 * @member:	the name of the ll_struct within the struct.
 */
#define ll_entry(ptr, type, member) \
	__container_of__(ptr, type, member)

/**
 * ll_for_each	-	iterate over a list
 * @pos:	the &struct ll_head to use as a loop counter.
 * @head:	the head for your list.
 */

#define ll_for_each(pos, head) \
	for (pos = (head)->next; pos != (head);	\
	     pos = pos->next)

/**
 * __ll_for_each	-	iterate over a list
 * @pos:	the &struct ll_head to use as a loop counter.
 * @head:	the head for your list.
 *
 * This variant differs from ll_for_each() in that it's the
 * simplest possible list iteration code, no prefetching is done.
 * Use this for code that knows the list to be very short (empty
 * or 1 entry) most of the time.
 */
#define __ll_for_each(pos, head) \
	for (pos = (head)->next; pos != (head); pos = pos->next)

/**
 * ll_for_each_prev	-	iterate over a list backwards
 * @pos:	the &struct ll_head to use as a loop counter.
 * @head:	the head for your list.
 */
#define ll_for_each_prev(pos, head) \
	for (pos = (head)->prev; prefetch(pos->prev), pos != (head); \
	     pos = pos->prev)

/**
 * ll_for_each_safe	-	iterate over a list safe against removal of list entry
 * @pos:	the &struct ll_head to use as a loop counter.
 * @n:		another &struct ll_head to use as temporary storage
 * @head:	the head for your list.
 */
#define ll_for_each_safe(pos, n, head) \
	for (pos = (head)->next, n = pos->next; pos != (head); \
	     pos = n, n = pos->next)

/**
 * ll_for_each_entry	-	iterate over list of given type
 * @pos:	the type * to use as a loop counter.
 * @head:	the head for your list.
 * @member:	the name of the ll_struct within the struct.
 */
#define ll_for_each_entry(pos, head, member)				\
	for (pos = ll_entry((head)->next, typeof(*pos), member);	\
	     &pos->member != (head);					\
	     pos = ll_entry(pos->member.next, typeof(*pos), member))

/**
 * ll_for_each_entry_reverse - iterate backwards over list of given type.
 * @pos:	the type * to use as a loop counter.
 * @head:	the head for your list.
 * @member:	the name of the ll_struct within the struct.
 */
#define ll_for_each_entry_reverse(pos, head, member)			\
	for (pos = ll_entry((head)->prev, typeof(*pos), member);	\
	     &pos->member != (head);	\
	     pos = ll_entry(pos->member.prev, typeof(*pos), member))

/**
 * ll_prepare_entry - prepare a pos entry for use as a start point in
 *			ll_for_each_entry_continue
 * @pos:	the type * to use as a start point
 * @head:	the head of the list
 * @member:	the name of the ll_struct within the struct.
 */
#define ll_prepare_entry(pos, head, member) \
	((pos) ? : ll_entry(head, typeof(*pos), member))

/**
 * ll_for_each_entry_continue -	iterate over list of given type
 *			continuing after existing point
 * @pos:	the type * to use as a loop counter.
 * @head:	the head for your list.
 * @member:	the name of the ll_struct within the struct.
 */
#define ll_for_each_entry_continue(pos, head, member)		\
	for (pos = ll_entry(pos->member.next, typeof(*pos), member);	\
	     &pos->member != (head);	\
	     pos = ll_entry(pos->member.next, typeof(*pos), member))

/**
 * ll_for_each_entry_safe - iterate over list of given type safe against removal of list entry
 * @pos:	the type * to use as a loop counter.
 * @n:		another type * to use as temporary storage
 * @head:	the head for your list.
 * @member:	the name of the ll_struct within the struct.
 */
#define ll_for_each_entry_safe(pos, n, head, member)			\
	for (pos = ll_entry((head)->next, typeof(*pos), member),	\
	     n = ll_entry(pos->member.next, typeof(*pos), member);	\
	     &pos->member != (head);					\
	     pos = n, n = ll_entry(n->member.next, typeof(*n), member))

/**
 * ll_for_each_entry_safe_continue -	iterate over list of given type
 *			continuing after existing point safe against removal of list entry
 * @pos:	the type * to use as a loop counter.
 * @n:		another type * to use as temporary storage
 * @head:	the head for your list.
 * @member:	the name of the ll_struct within the struct.
 */
#define ll_for_each_entry_safe_continue(pos, n, head, member)		\
	for (pos = ll_entry(pos->member.next, typeof(*pos), member),		\
	     n = ll_entry(pos->member.next, typeof(*pos), member);		\
	     &pos->member != (head);						\
	     pos = n, n = ll_entry(n->member.next, typeof(*n), member))

/**
 * ll_for_each_entry_safe_reverse - iterate backwards over list of given type safe against
 *				      removal of list entry
 * @pos:	the type * to use as a loop counter.
 * @n:		another type * to use as temporary storage
 * @head:	the head for your list.
 * @member:	the name of the ll_struct within the struct.
 */
#define ll_for_each_entry_safe_reverse(pos, n, head, member)		\
	for (pos = ll_entry((head)->prev, typeof(*pos), member),	\
	     n = ll_entry(pos->member.prev, typeof(*pos), member);	\
	     &pos->member != (head);					\
	     pos = n, n = ll_entry(n->member.prev, typeof(*n), member))




/*
 * Double linked lists with a single pointer list head.
 * Mostly useful for hash tables where the two pointer list head is
 * too wasteful.
 * You lose the ability to access the tail in O(1).
 */

struct hll_head {
	struct hll_node *first;
};

struct hll_node {
	struct hll_node *next, **pprev;
};

#define HLL_HEAD_INIT { .first = NULL }
#define HLL_HEAD(name) struct hll_head name = {  .first = NULL }
#define INIT_HLL_HEAD(ptr) ((ptr)->first = NULL)
#define INIT_HLL_NODE(ptr) ((ptr)->next = NULL, (ptr)->pprev = NULL)

static inline int hll_unhashed(const struct hll_node *h)
{
	return !h->pprev;
}

static inline int hll_empty(const struct hll_head *h)
{
	return !h->first;
}

static inline void __hll_del(struct hll_node *n)
{
	struct hll_node *next = n->next;
	struct hll_node **pprev = n->pprev;
	*pprev = next;
	if (next)
		next->pprev = pprev;
}

static inline void hll_del(struct hll_node *n)
{
	__hll_del(n);
	n->next = LL_POISON1;
	n->pprev = LL_POISON2;
}


static inline void hll_del_init(struct hll_node *n)
{
	if (n->pprev)  {
		__hll_del(n);
		INIT_HLL_NODE(n);
	}
}

static inline void hll_add_head(struct hll_node *n, struct hll_head *h)
{
	struct hll_node *first = h->first;
	n->next = first;
	if (first)
		first->pprev = &n->next;
	h->first = n;
	n->pprev = &h->first;
}



/* next must be != NULL */
static inline void hll_add_before(struct hll_node *n,
				  struct hll_node *next)
{
	n->pprev = next->pprev;
	n->next = next;
	next->pprev = &n->next;
	*(n->pprev) = n;
}

static inline void hll_add_after(struct hll_node *n,
				 struct hll_node *next)
{
	next->next = n->next;
	n->next = next;
	next->pprev = &n->next;

	if(next->next)
		next->next->pprev  = &next->next;
}



#define hll_entry(ptr, type, member) __container_of__(ptr,type,member)

#define hll_for_each(pos, head) \
	for (pos = (head)->first; pos && ({ prefetch(pos->next); 1; }); \
	     pos = pos->next)

#define hll_for_each_safe(pos, n, head) \
	for (pos = (head)->first; pos && ({ n = pos->next; 1; }); \
	     pos = n)

/**
 * hll_for_each_entry	- iterate over list of given type
 * @tpos:	the type * to use as a loop counter.
 * @pos:	the &struct hll_node to use as a loop counter.
 * @head:	the head for your list.
 * @member:	the name of the hll_node within the struct.
 */
#define hll_for_each_entry(tpos, pos, head, member)			 \
	for (pos = (head)->first;					 \
	     pos && ({ prefetch(pos->next); 1;}) &&			 \
	     ({ tpos = hll_entry(pos, typeof(*tpos), member); 1;}); \
	     pos = pos->next)

/**
 * hll_for_each_entry_continue - iterate over a hlist continuing after existing point
 * @tpos:	the type * to use as a loop counter.
 * @pos:	the &struct hll_node to use as a loop counter.
 * @member:	the name of the hll_node within the struct.
 */
#define hll_for_each_entry_continue(tpos, pos, member)		 \
	for (pos = (pos)->next;						 \
	     pos && ({ prefetch(pos->next); 1;}) &&			 \
	     ({ tpos = hll_entry(pos, typeof(*tpos), member); 1;}); \
	     pos = pos->next)

/**
 * hll_for_each_entry_from - iterate over a hlist continuing from existing point
 * @tpos:	the type * to use as a loop counter.
 * @pos:	the &struct hll_node to use as a loop counter.
 * @member:	the name of the hll_node within the struct.
 */
#define hll_for_each_entry_from(tpos, pos, member)			 \
	for (; pos && ({ prefetch(pos->next); 1;}) &&			 \
	     ({ tpos = hll_entry(pos, typeof(*tpos), member); 1;}); \
	     pos = pos->next)

/**
 * hll_for_each_entry_safe - iterate over list of given type safe against removal of list entry
 * @tpos:	the type * to use as a loop counter.
 * @pos:	the &struct hll_node to use as a loop counter.
 * @n:		another &struct hll_node to use as temporary storage
 * @head:	the head for your list.
 * @member:	the name of the hll_node within the struct.
 */
#define hll_for_each_entry_safe(tpos, pos, n, head, member)		 \
	for (pos = (head)->first;					 \
	     pos && ({ n = pos->next; 1; }) &&				 \
	     ({ tpos = hll_entry(pos, typeof(*tpos), member); 1;}); \
	     pos = n)


#endif
