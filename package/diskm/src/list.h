#ifndef _LIST_H
#define _LIST_H                     1

#include <stdlib.h>

/*-----------------------------------------------------------------------------
 * Simple doubly linked list implementation.
 -----------------------------------------------------------------------------*/

struct list_head {
	struct list_head *next, *prev;
};

#define LIST_HEAD_INIT(name) { &(name), &(name) }

static inline void INIT_LIST_HEAD(struct list_head *list)
{
	list->next = list;
	list->prev = list;
}

static inline void prefetch(const void *x) {;}

/*
 * Purpose: Insert a new entry between two known consecutive entries.
 *
 * This is only for internal list manipulation where we know
 * the prev/next entries already!
 */
static inline void __list_add(struct list_head *new,
			      struct list_head *prev,
			      struct list_head *next)
{
	next->prev = new;
	new->next = next;
	new->prev = prev;
	prev->next = new;
}

/* Purpose: list_add - add a new entry
 * In     : new: new entry to be added
 *          head: list head to add it after
 * Return : void
 * Note   : Insert a new entry after the specified head.
 *          This is good for implementing stacks.
 */
static inline void list_add(struct list_head *new, struct list_head *head)
{
	__list_add(new, head, head->next);
}

/* Purpose: list_add_tail - add a new entry
 * In     : new: new entry to be added
 *          head: list head to add it before
 * Return : void
 * Note   : Insert a new entry before the specified head.
 *          This is useful for implementing queues.
 */
static inline void list_add_tail(struct list_head *new, struct list_head *head)
{
	__list_add(new, head->prev, head);
}

/* Purpose: Delete a list entry by making the prev/next entries
 *          point to each other.
 * In     : prev: the previous list head
 *          next: the next list head
 * Return : void
 * Note   : This is only for internal list manipulation where we know
 *          the prev/next entries already!
 */
static inline void __list_del(struct list_head * prev, struct list_head * next)
{
	next->prev = prev;
	prev->next = next;
}

/* Purpose: list_del - deletes entry from list.
 * In     : entry: the element to delete from the list.
 * Return : void
 * Note   : list_empty on entry does not return true after this, the entry is
 *          in an undefined state.
 */
static inline void list_del(struct list_head *entry)
{
	__list_del(entry->prev, entry->next);
	entry->next = NULL;
	entry->prev = NULL;
}

/* Purpose: list_replace - replace old entry by new one
 * In     : old: the element to be replaced
 *          new: the new element to insert
 * Return : void
 * Note   : if 'old' was empty, it will be overwritten.
 */
static inline void list_replace(struct list_head *old,
				struct list_head *new)
{
	new->next = old->next;
	new->next->prev = new;
	new->prev = old->prev;
	new->prev->next = new;
}

static inline void list_replace_init(struct list_head *old,
					struct list_head *new)
{
	list_replace(old, new);
	INIT_LIST_HEAD(old);
}

/* Purpose: list_del_init - deletes entry from list and reinitialize it.
 * In     : entry: the element to delete from the list.
 * Return : void
 */
static inline void list_del_init(struct list_head *entry)
{
	__list_del(entry->prev, entry->next);
	INIT_LIST_HEAD(entry);
}

/* Purpose: list_move - delete from one list and add as another's head
 * In     : list: the entry to move
 *          head: the head that will precede our entry
 * Return : void
 */
static inline void list_move(struct list_head *list, struct list_head *head)
{
        __list_del(list->prev, list->next);
        list_add(list, head);
}

/* Purpose: list_move_tail - delete from one list and add as another's tail
 * In     : list: the entry to move
 *          head: the head that will follow our entry
 * Return : void
 */
static inline void list_move_tail(struct list_head *list,
				  struct list_head *head)
{
        __list_del(list->prev, list->next);
        list_add_tail(list, head);
}

/* Purpose: list_is_last - tests whether @list is the last entry in list @head
 * In     : list: the entry to test
 *          head: the head of the list
 * Return : void
 */
static inline int list_is_last(const struct list_head *list,
				const struct list_head *head)
{
	return list->next == head;
}

/* Purpose: list_empty - tests whether a list is empty
 * In     : head: the list to test.
 * Return : void
 */
static inline int list_empty(const struct list_head *head)
{
	return head->next == head;
}

/* Purpose: list_empty_careful - tests whether a list is empty 
 *          and not being modified
 * In     : head: the list to test
 * Return : 1: empty;  0: isn't empty
 * Description:
 *         tests whether a list is empty _and_ checks that 
 *         no other CPU might be
 *         in the process of modifying either member (next or prev)
 *
 * NOTE   : using list_empty_careful() without synchronization
 *          can only be safe if the only activity that can happen
 *          to the list entry is list_del_init(). Eg. it cannot be used
 *          if another CPU could re-list_add() it.
 */
static inline int list_empty_careful(const struct list_head *head)
{
	struct list_head *next = head->next;
	return (next == head) && (next == head->prev);
}

static inline void __list_splice(struct list_head *list,
				 struct list_head *head)
{
	struct list_head *first = list->next;
	struct list_head *last = list->prev;
	struct list_head *at = head->next;

	first->prev = head;
	head->next = first;

	last->next = at;
	at->prev = last;
}

/* Purpose: list_splice - join two lists
 * In     : list: the new list to add.
 *          head: the place to add it in the first list.
 * Return : void
 */
static inline void list_splice(struct list_head *list, struct list_head *head)
{
	if (!list_empty(list))
		__list_splice(list, head);
}

/* Purpose: list_splice_init - join two lists and reinitialise the emptied list.
 * In     : list: the new list to add.
 *          head: the place to add it in the first list.
 * Return : The list at @list is reinitialised
 */
static inline void list_splice_init(struct list_head *list,
				    struct list_head *head)
{
	if (!list_empty(list)) {
		__list_splice(list, head);
		INIT_LIST_HEAD(list);
	}
}

#define sys_offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)

/* Purpose: container_of - cast a member of a structure out to the containing structure
 * In     : ptr   : the pointer to the member.
 *          type  : the type of the container struct this is embedded in.
 *          member: the name of the member within the struct.
 */
#define container_of(ptr, type, member) ({                      \
        const typeof( ((type *)0)->member ) *__mptr = (ptr);    \
        (type *)( (char *)__mptr - sys_offsetof(type,member) );})

/* Purpose: list_entry - get the struct for this entry
 * In     : ptr   : the &struct list_head pointer.
 *          type  : the type of the struct this is embedded in.
 *          member: the name of the list_struct within the struct.
 */
#define list_entry(ptr, type, member) \
	container_of(ptr, type, member)

/* Purpose: list_for_each - iterate over a list
 * In     : pos  : the &struct list_head to use as a loop cursor.
 *          head : the head for your list.
 */
#define list_for_each(pos, head) \
	for (pos = (head)->next; prefetch(pos->next), pos != (head); \
        	pos = pos->next)

/* Purpose: __list_for_each - iterate over a list
 * In     : pos : the &struct list_head to use as a loop cursor.
 *          head: the head for your list.
 * Note   :
 * This variant differs from list_for_each() in that it's the
 * simplest possible list iteration code, no prefetching is done.
 * Use this for code that knows the list to be very short (empty
 * or 1 entry) most of the time.
 */
#define __list_for_each(pos, head) \
	for (pos = (head)->next; pos != (head); pos = pos->next)

/* Purpose : list_for_each_prev	- iterate over a list backwards
 * In      : pos : the &struct list_head to use as a loop cursor.
 *           head: the head for your list.
 */
#define list_for_each_prev(pos, head) \
	for (pos = (head)->prev; prefetch(pos->prev), pos != (head); \
        	pos = pos->prev)

/* Purpose: list_for_each_safe - iterate over a list safe against 
 *          removal of list entry
 * In     : pos : the &struct list_head to use as a loop cursor.
 *          n   : another &struct list_head to use as temporary storage
 *          head: the head for your list.
 */
#define list_for_each_safe(pos, n, head) \
	for (pos = (head)->next, n = pos->next; pos != (head); \
		pos = n, n = pos->next)

/* Purpose: list_for_each_entry	- iterate over list of given type
 * In     : pos   : the type * to use as a loop cursor.
 *          head  : the head for your list.
 *          member: the name of the list_struct within the struct.
 */
#define list_for_each_entry(pos, head, member)				\
	for (pos = list_entry((head)->next, typeof(*pos), member);	\
	     prefetch(pos->member.next), &pos->member != (head); 	\
	     pos = list_entry(pos->member.next, typeof(*pos), member))

/* Purpose: list_for_each_entry_reverse - iterate backwards over list of given type.
 * In     : pos   : the type * to use as a loop cursor.
 *          head  : the head for your list.
 *          member: the name of the list_struct within the struct.
 */
#define list_for_each_entry_reverse(pos, head, member)			\
	for (pos = list_entry((head)->prev, typeof(*pos), member);	\
	     prefetch(pos->member.prev), &pos->member != (head); 	\
	     pos = list_entry(pos->member.prev, typeof(*pos), member))

/* Purpose: list_prepare_entry - prepare a pos entry for use in 
 *           list_for_each_entry_continue
 * In     : pos   : the type * to use as a start point
 *          head  : the head of the list
 *          member: the name of the list_struct within the struct.
 * Note   : Prepares a pos entry for use as a start point in 
 *          list_for_each_entry_continue.
 */
#define list_prepare_entry(pos, head, member) \
	((pos) ? : list_entry(head, typeof(*pos), member))

/* Purpose: list_for_each_entry_continue - continue iteration 
 *          over list of given type
 * In     : pos   : the type * to use as a loop cursor.
 *          head  : the head for your list.
 *          member: the name of the list_struct within the struct.
 * Note   : Continue to iterate over list of given type, continuing after
 *          the current position.
 */
#define list_for_each_entry_continue(pos, head, member) 		\
	for (pos = list_entry(pos->member.next, typeof(*pos), member);	\
	     prefetch(pos->member.next), &pos->member != (head);	\
	     pos = list_entry(pos->member.next, typeof(*pos), member))

/* Purpose: list_for_each_entry_from - iterate over list of given type from 
 *          the current point
 * In     : pos   : the type * to use as a loop cursor.
 *          head  : the head for your list.
 *          member: the name of the list_struct within the struct.
 * Note   : Iterate over list of given type, continuing from current position.
 */
#define list_for_each_entry_from(pos, head, member) 			\
	for (; prefetch(pos->member.next), &pos->member != (head);	\
	     pos = list_entry(pos->member.next, typeof(*pos), member))

/* Purpose: list_for_each_entry_safe - iterate over list of given type safe 
 *          against removal of list entry
 * In     : pos   : the type * to use as a loop cursor.
 *          n     : another type * to use as temporary storage
 *          head  : the head for your list.
 *          member: the name of the list_struct within the struct.
 */
#define list_for_each_entry_safe(pos, n, head, member)			\
	for (pos = list_entry((head)->next, typeof(*pos), member),	\
		n = list_entry(pos->member.next, typeof(*pos), member);	\
	     &pos->member != (head); 					\
	     pos = n, n = list_entry(n->member.next, typeof(*n), member))

/* Purpose: list_for_each_entry_safe_continue
 * In     : pos   : the type * to use as a loop cursor.
 *          n     : another type * to use as temporary storage
 *          head  : the head for your list.
 *          member: the name of the list_struct within the struct.
 * Note   : Iterate over list of given type, continuing after current point,
 *          safe against removal of list entry.
 */
#define list_for_each_entry_safe_continue(pos, n, head, member) 		\
	for (pos = list_entry(pos->member.next, typeof(*pos), member), 		\
		n = list_entry(pos->member.next, typeof(*pos), member);		\
	     &pos->member != (head);						\
	     pos = n, n = list_entry(n->member.next, typeof(*n), member))

/* Purpose: list_for_each_entry_safe_from
 * In     : pos   : the type * to use as a loop cursor.
 *          n     : another type * to use as temporary storage
 *          head  : the head for your list.
 *          member: the name of the list_struct within the struct.
 * Note   : Iterate over list of given type from current point, safe against
 *          removal of list entry.
 */
#define list_for_each_entry_safe_from(pos, n, head, member) 			\
	for (n = list_entry(pos->member.next, typeof(*pos), member);		\
	     &pos->member != (head);						\
	     pos = n, n = list_entry(n->member.next, typeof(*n), member))

/* Purpose: list_for_each_entry_safe_reverse
 * In     : pos   : the type * to use as a loop cursor.
 *          n     : another type * to use as temporary storage
 *          head  : the head for your list.
 *          member: the name of the list_struct within the struct.
 * Note   : Iterate backwards over list of given type, safe against removal
 *          of list entry.
 */
#define list_for_each_entry_safe_reverse(pos, n, head, member)		\
	for (pos = list_entry((head)->prev, typeof(*pos), member),	\
		n = list_entry(pos->member.prev, typeof(*pos), member);	\
	     &pos->member != (head); 					\
	     pos = n, n = list_entry(n->member.prev, typeof(*n), member))


/*-----------------------------------------------------------------------------
 * Single linked list implementation.
 -----------------------------------------------------------------------------*/
/* Purpose: Initialize the new entry
 * In     : ptr   : the new entry pointer
 *          member: the name of single link node in the struct
 */
#define slist_init(ptr, member)			\
	((ptr)->member = NULL)

/* Purpose: Insert new entry before the specified head. 
 * In     : head  : the single link list head
 *          ptr   : the new entry pointer
 *          member: the name of single link node in the struct
 */
#define slist_add(head, ptr, member)		\
	{					\
		if (!(head)) {			\
			(head) = (ptr);		\
		} else {			\
			(ptr)->member = (head);	\
			(head) = (ptr);		\
		}				\
	}

/* Purpose: Insert new entry end of the specified list
 * In     : head  : the single link list head
 *          ptr   : the new entry pointer
 *          type  : the entry type
 *          member: the name of single link node in the struct
 */
#define slist_add_tail(head, ptr, type, member)		\
	{						\
		if (!(head)) {				\
			(head) = (ptr);			\
		} else {				\
			type *__tmp = (head);		\
			while (__tmp->member != NULL)	\
				__tmp = __tmp->member;	\
			__tmp->member = (ptr);		\
			(ptr)->member = NULL;		\
		}					\
	}

/* Purpose: Delete the entry from the specified list
 * In     : head  : the single link list head
 *          ptr   : the deleting entry pointer
 *          type  : the entry type
 *          member: the name of single link node in the struct
 */	
#define slist_del(head, ptr, type, member)				\
	{								\
		if ((head) == (ptr)) {					\
			(head) = (ptr)->member;				\
		} else {						\
			type *__tmp = (head);				\
			while ((__tmp != NULL) && (__tmp->next != (ptr))) \
				__tmp = __tmp->member;			\
			if (__tmp->next == (ptr))			\
				__tmp->member = (ptr)->member;		\
		}							\
		(ptr)->member = NULL;					\
	}								\

/* Purpose: Iterate over the single link list
 * In     : head  : the single link list head
 *          pos   : the entry pointer to use as a loop cursor.
 *          member: the name of single link node in the struct
 */
#define slist_for_each(head, pos, member)			\
	for ((pos) = (head); (pos) != NULL; (pos) = (pos)->member)

/* Purpose: Iterate over the single link list safe against
 *          removal of list entry
 * In     : head  : the single link list head
 *          pos   : the entry pointer to use as a loop cursor.
 *          tmp   : another entry pointer to use as temporary storage
 *          member: the name of single link node in the struct
 */
#define slist_for_each_safe(head, pos, tmp, member) \
	for ((tmp) = (head), (pos) = (head)->member; (tmp) != NULL; \
	     ((tmp) = (pos)) ? ((pos) = (pos)->member) : ((pos) = NULL))

#endif
