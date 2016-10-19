#ifndef _LINUX_LOCAL_WRAP_H
#define _LINUX_LOCAL_WRAP_H

#include <asm/local.h>

/*
 * A signed long type for operations which are atomic for a single CPU. Usually
 * used in combination with per-cpu variables. This is a safeguard header that
 * ensures that local_wrap_* is available regardless of whether platform support
 * for HARDENED_ATOMIC is available.
 */

#ifndef CONFIG_HARDENED_ATOMIC

typedef struct {
    atomic_long_wrap_t a;
} local_wrap_t;

#ifndef local_read_wrap
#define local_read_wrap(l)	atomic_long_read_wrap(&(l)->a)
#endif
#ifndef local_set_wrap
#define local_set_wrap(l,i)	atomic_long_set_wrap((&(l)->a),(i))
#endif
#ifndef local_inc_wrap
#define local_inc_wrap(l)	atomic_long_inc_wrap(&(l)->a)
#endif
#ifndef local_dec_wrap
#define local_dec_wrap(l)	atomic_long_dec_wrap(&(l)->a)
#endif
#ifndef local_add_wrap
#define local_add_wrap(i,l)	atomic_long_add_wrap((i),(&(l)->a))
#endif
#ifndef local_sub_wrap
#define local_sub_wrap(i,l)	atomic_long_sub_wrap((i),(&(l)->a))
#endif
#ifndef local_sub_and_test_wrap
#define local_sub_and_test_wrap(i, l) atomic_long_sub_and_test_wrap((i), (&(l)->a))
#endif
#ifndef local_add_return_wrap
#define local_add_return_wrap(i, l) atomic_long_add_return_wrap((i), (&(l)->a))
#endif
#ifndef local_cmpxchg_wrap
#define local_cmpxchg_wrap(l, o, n) atomic_long_cmpxchg_wrap((&(l)->a), (o), (n))
#endif
#endif /* CONFIG_HARDENED_ATOMIC */

#endif /* _LINUX_LOCAL_WRAP_H */
