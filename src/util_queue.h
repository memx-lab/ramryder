#ifndef UTIL_QUEUE_H
#define UTIL_QUEUE_H

#include <stddef.h>

#define	TAILQ_EMPTY(head)	((head)->tqh_first == NULL)
#define	TAILQ_FIRST(head)	((head)->tqh_first)

#define TAILQ_HEAD(name, type)                      \
struct name {                                      \
    struct type *tqh_first; /* first element */    \
    struct type **tqh_last; /* addr of last next */\
}

#define TAILQ_ENTRY(type)                       \
struct {                                       \
    struct type *tqe_next;  /* next element */ \
    struct type **tqe_prev; /* prev's next */  \
}

#define TAILQ_INIT(head) do {               \
    (head)->tqh_first = NULL;              \
    (head)->tqh_last = &(head)->tqh_first; \
} while (0)

#define TAILQ_INSERT_TAIL(head, elm, field) do {           \
    (elm)->field.tqe_next = NULL;                          \
    (elm)->field.tqe_prev = (head)->tqh_last;              \
    *(head)->tqh_last = (elm);                             \
    (head)->tqh_last = &(elm)->field.tqe_next;             \
} while (0)

#define TAILQ_REMOVE(head, elm, field) do {                         \
    if (((elm)->field.tqe_next) != NULL)                           \
        (elm)->field.tqe_next->field.tqe_prev =                    \
            (elm)->field.tqe_prev;                                 \
    else                                                           \
        (head)->tqh_last = (elm)->field.tqe_prev;                  \
    *(elm)->field.tqe_prev = (elm)->field.tqe_next;                \
} while (0)

#define TAILQ_FOREACH(var, head, field) \
    for ((var) = ((head)->tqh_first); (var); (var) = ((var)->field.tqe_next))

#endif // UTIL_QUEUE_H
