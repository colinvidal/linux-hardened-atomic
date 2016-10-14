/*
 *  arch/arm/include/asm/atomic.h
 *
 *  Copyright (C) 1996 Russell King.
 *  Copyright (C) 2002 Deep Blue Solutions Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef __ASM_ARM_ATOMIC_H
#define __ASM_ARM_ATOMIC_H

#include <linux/compiler.h>
#include <linux/prefetch.h>
#include <linux/types.h>
#include <linux/irqflags.h>
#include <asm/barrier.h>
#include <asm/cmpxchg.h>
#include <linux/bug.h>

#define ATOMIC_INIT(i)	{ (i) }

#ifdef __KERNEL__

#ifdef CONFIG_HARDENED_ATOMIC
#ifdef CONFIG_THUMB2_KERNEL
#define HARDENED_ATOMIC_INSN "bkpt 0xf1"
#else
#define HARDENED_ATOMIC_INSN "bkpt 0xf103"
#endif /* CONFIG_THUMB2_KERNEL */
#define _ASM_EXTABLE(from, to)			\
	".pushsection __ex_table,\"a\"\n"	\
	".align 3\n"				\
	".long "#from","#to"\n"			\
	".popsection"
#define __OVERFLOW_POST				\
	"bvc 3f\n"				\
	"2: "HARDENED_ATOMIC_INSN"\n"		\
	"3:\n"
#define __OVERFLOW_POST_RETURN			\
	"bvc 3f\n"				\
	"mov %0,%1\n"				\
	"2: "HARDENED_ATOMIC_INSN"\n"		\
	"3:\n"
#define __OVERFLOW_EXTABLE			\
	"4:\n"					\
	_ASM_EXTABLE(2b, 4b)
#else
#define __OVERFLOW_POST
#define __OVERFLOW_POST_RETURN
#define __OVERFLOW_EXTABLE
#endif /* CONFIG_HARDENED_ATOMIC */

/*
 * On ARM, ordinary assignment (str instruction) doesn't clear the local
 * strex/ldrex monitor on some implementations. The reason we can use it for
 * atomic_set() is the clrex or dummy strex done on every exception return.
 */
#define atomic_read(v)	READ_ONCE((v)->counter)
static inline int atomic_read_wrap(const atomic_wrap_t *v)
{
	return atomic_read(v);
}
#define atomic_set(v,i)	WRITE_ONCE(((v)->counter), (i))
static inline void atomic_set_wrap(atomic_wrap_t *v, int i)
{
	atomic_set(v, i);
}

#if __LINUX_ARM_ARCH__ >= 6

/*
 * ARMv6 UP and SMP safe atomic ops.  We use load exclusive and
 * store exclusive to ensure that these are atomic.  We may loop
 * to ensure that the update happens.
 */

#define __ATOMIC_OP(op, suffix, c_op, asm_op, post_op, extable)		\
static inline void atomic_##op##suffix(int i, atomic##suffix##_t *v)	\
{									\
	unsigned long tmp;						\
	int result;							\
									\
	prefetchw(&v->counter);						\
	__asm__ __volatile__("@ atomic_" #op #suffix "\n"		\
"1:	ldrex	%0, [%3]\n"						\
"	" #asm_op "	%0, %0, %4\n"					\
        post_op                 					\
"	strex	%1, %0, [%3]\n"						\
"	teq	%1, #0\n"						\
"	bne	1b\n"							\
        extable                 					\
	: "=&r" (result), "=&r" (tmp), "+Qo" (v->counter)		\
	: "r" (&v->counter), "Ir" (i)					\
	: "cc");							\
}									\

#define ATOMIC_OP(op, c_op, asm_op)		                        \
	__ATOMIC_OP(op, _wrap, c_op, asm_op, , )			\
	__ATOMIC_OP(op, , c_op, asm_op##s, __OVERFLOW_POST, __OVERFLOW_EXTABLE)

#define __ATOMIC_OP_RETURN(op, suffix, c_op, asm_op, post_op, extable)	\
static inline int atomic_##op##_return##suffix##_relaxed(int i, atomic##suffix##_t *v) \
{									\
	unsigned long tmp;						\
	int result;							\
									\
	prefetchw(&v->counter);						\
									\
	__asm__ __volatile__("@ atomic_" #op "_return" #suffix "\n"	\
"1:	ldrex	%0, [%3]\n"						\
"	" #asm_op "	%0, %0, %4\n"					\
        post_op                 					\
"	strex	%1, %0, [%3]\n"						\
"	teq	%1, #0\n"						\
"	bne	1b\n"							\
        extable                 					\
	: "=&r" (result), "=&r" (tmp), "+Qo" (v->counter)		\
	: "r" (&v->counter), "Ir" (i)					\
	: "cc");							\
									\
	return result;							\
}

#define ATOMIC_OP_RETURN(op, c_op, asm_op)	                        \
	__ATOMIC_OP_RETURN(op, _wrap, c_op, asm_op, , )			\
	__ATOMIC_OP_RETURN(op, , c_op, asm_op##s,			\
			   __OVERFLOW_POST_RETURN, __OVERFLOW_EXTABLE)

#define ATOMIC_FETCH_OP(op, c_op, asm_op)				\
static inline int atomic_fetch_##op##_relaxed(int i, atomic_t *v)	\
{									\
	unsigned long tmp;						\
	int result, val;						\
									\
	prefetchw(&v->counter);						\
									\
	__asm__ __volatile__("@ atomic_fetch_" #op "\n"			\
"1:	ldrex	%0, [%4]\n"						\
"	" #asm_op "	%1, %0, %5\n"					\
"	strex	%2, %1, [%4]\n"						\
"	teq	%2, #0\n"						\
"	bne	1b"							\
	: "=&r" (result), "=&r" (val), "=&r" (tmp), "+Qo" (v->counter)	\
	: "r" (&v->counter), "Ir" (i)					\
	: "cc");							\
									\
	return result;							\
}

#define atomic_add_return_relaxed	atomic_add_return_relaxed
#define atomic_add_return_wrap_relaxed  atomoc_add_return_wrap_relaxed
#define atomic_sub_return_relaxed	atomic_sub_return_relaxed
#define atomic_sub_return_wrap_relaxed  atomic_sub_return_wrap_relaxed
#define atomic_fetch_add_relaxed	atomic_fetch_add_relaxed
#define atomic_fetch_sub_relaxed	atomic_fetch_sub_relaxed

#define atomic_fetch_and_relaxed	atomic_fetch_and_relaxed
#define atomic_fetch_andnot_relaxed	atomic_fetch_andnot_relaxed
#define atomic_fetch_or_relaxed		atomic_fetch_or_relaxed
#define atomic_fetch_xor_relaxed	atomic_fetch_xor_relaxed

#define __ATOMIC_CMPXCHG_RELAXED(suffix)			       	\
static inline int atomic_cmpxchg##suffix##_relaxed(atomic##suffix##_t *ptr, \
						   int old, int new)	\
{									\
	int oldval;                                                     \
	unsigned long res;                                              \
									\
	prefetchw(&ptr->counter);					\
									\
	do {								\
	        __asm__ __volatile__("@ atomic_cmpxchg" #suffix "\n"	\
		"ldrex	%1, [%3]\n"					\
		"mov	%0, #0\n"					\
		"teq	%1, %4\n"					\
		"strexeq %0, %5, [%3]\n"				\
		    : "=&r" (res), "=&r" (oldval), "+Qo" (ptr->counter) \
		    : "r" (&ptr->counter), "Ir" (old), "r" (new)        \
		    : "cc");                                            \
	} while (res);							\
									\
	return oldval;							\
}

__ATOMIC_CMPXCHG_RELAXED()
__ATOMIC_CMPXCHG_RELAXED(_wrap)

#undef __ATOMIC_CMPXCHG_RELAXED

#define atomic_cmpxchg_relaxed		atomic_cmpxchg_relaxed
#define atomic_cmpxchg_wrap_relaxed     atomic_cmpxchg_wrap_relaxed

static inline int __atomic_add_unless(atomic_t *v, int a, int u)
{
	int oldval, newval;
	unsigned long tmp;

	smp_mb();
	prefetchw(&v->counter);

	__asm__ __volatile__ ("@ atomic_add_unless\n"
"1:	ldrex	%0, [%4]\n"
"	teq	%0, %5\n"
"	beq	4f\n"
"	adds	%1, %0, %6\n"

#ifdef CONFIG_HARDENED_ATOMIC
"       bvc 3f\n"
"2:     "HARDENED_ATOMIC_INSN"\n"
"3:\n"
#endif
"	strex	%2, %1, [%4]\n"
"	teq	%2, #0\n"
"	bne	1b\n"
"4:"
#ifdef CONFIG_HARDENED_ATOMIC
        _ASM_EXTABLE(2b, 4b)
#endif
	: "=&r" (oldval), "=&r" (newval), "=&r" (tmp), "+Qo" (v->counter)
	: "r" (&v->counter), "r" (u), "r" (a)
	: "cc");

	if (oldval != u)
		smp_mb();

	return oldval;
}

#else /* ARM_ARCH_6 */

#ifdef CONFIG_SMP
#error SMP not supported on pre-ARMv6 CPUs
#endif

#define __ATOMIC_OP(op, suffix, c_op, asm_op)				\
static inline void atomic_##op##suffix(int i, atomic##suffix##_t *v)	\
{									\
	unsigned long flags;						\
									\
	raw_local_irq_save(flags);					\
	v->counter c_op i;						\
	raw_local_irq_restore(flags);					\
}									\

#define ATOMIC_OP(op, c_op, asm_op)		                        \
	__ATOMIC_OP(op, _wrap, c_op, asm_op)				\
	__ATOMIC_OP(op, , c_op, asm_op)

#define __ATOMIC_OP_RETURN(op, suffix, c_op, asm_op)			\
static inline int atomic_##op##_return##suffix(int i, atomic##suffix##_t *v) \
{									\
	unsigned long flags;						\
	int val;							\
									\
	raw_local_irq_save(flags);					\
	v->counter c_op i;						\
	val = v->counter;						\
	raw_local_irq_restore(flags);					\
									\
	return val;							\
}

#define ATOMIC_OP_RETURN(op, c_op, asm_op)	                        \
	__ATOMIC_OP_RETURN(op, wrap, c_op, asm_op)			\
	__ATOMIC_OP_RETURN(op, , c_op, asm_op)

#define ATOMIC_FETCH_OP(op, c_op, asm_op)				\
static inline int atomic_fetch_##op(int i, atomic_t *v)			\
{									\
	unsigned long flags;						\
	int val;							\
									\
	raw_local_irq_save(flags);					\
	val = v->counter;						\
	v->counter c_op i;						\
	raw_local_irq_restore(flags);					\
									\
	return val;							\
}

static inline int atomic_cmpxchg(atomic_t *v, int old, int new)
{
	int ret;
	unsigned long flags;

	raw_local_irq_save(flags);
	ret = v->counter;
	if (likely(ret == old))
		v->counter = new;
	raw_local_irq_restore(flags);

	return ret;
}

static inline int atomic_cmpxchg_wrap(atomic_wrap_t *v, int old, int new)
{
	return atomic_cmpxchg((atomic_t *)v, old, new);
}

static inline int __atomic_add_unless(atomic_t *v, int a, int u)
{
	int c, old;

	c = atomic_read(v);
	while (c != u && (old = atomic_cmpxchg((v), c, c + a)) != c)
		c = old;
	return c;
}

#endif /* __LINUX_ARM_ARCH__ */

static inline int __atomic_add_unless_wrap(atomic_wrap_t *v, int a, int u)
{
	return __atomic_add_unless((atomic_t *)v, a, u);
}

#define ATOMIC_OPS(op, c_op, asm_op)					\
	ATOMIC_OP(op, c_op, asm_op)					\
	ATOMIC_OP_RETURN(op, c_op, asm_op)				\
	ATOMIC_FETCH_OP(op, c_op, asm_op)

ATOMIC_OPS(add, +=, add)
ATOMIC_OPS(sub, -=, sub)

#define atomic_andnot atomic_andnot

#undef ATOMIC_OPS
#define ATOMIC_OPS(op, c_op, asm_op)					\
	ATOMIC_OP(op, c_op, asm_op)					\
	ATOMIC_FETCH_OP(op, c_op, asm_op)

ATOMIC_OPS(and, &=, and)
ATOMIC_OPS(andnot, &= ~, bic)
ATOMIC_OPS(or,  |=, orr)
ATOMIC_OPS(xor, ^=, eor)

#undef ATOMIC_OPS
#undef ATOMIC_FETCH_OP
#undef ATOMIC_OP_RETURN
#undef __ATOMIC_OP_RETURN
#undef ATOMIC_OP
#undef __ATOMIC_OP

#define atomic_xchg(v, new) xchg(&((v)->counter), new)
#define atomic_xchg_wrap(v, new) atomic_xchg(v, new)
#define atomic_inc(v) atomic_add(1, v)
#define atomic_dec(v) atomic_sub(1, v)
#define atomic_inc_and_test(v) (atomic_inc_return(v) == 0)
#define atomic_dec_and_test(v) (atomic_dec_return(v) == 0)
#define atomic_inc_return_relaxed(v) (atomic_add_return_relaxed(1, v))
#define atomic_inc_return_wrap_relaxed(v) (atomic_add_return_wrap_relaxed(1, v))
#define atomic_dec_return_wrap_relaxed(v) (atomic_sub_return_wrap_relaxed(1, v))
#define atomic_dec_return_relaxed(v) atomic_sub_return_relaxed(1, v)
#define atomic_sub_and_test(i, v) (atomic_sub_return(i, v) == 0)
#define atomic_sub_and_test_wrap(i, v) (atomic_sub_return_wrap(i, v) == 0)
#define atomic_add_negative(i, v) (atomic_add_return(i, v) < 0)
#define atomic_add_negative_wrap(i, v) (atomic_add_return_wrap(i, v) < 0)

#ifdef CONFIG_HARDENED_ATOMIC
#define atomic_inc_and_test_wrap(v) (atomic_add_return_wrap(1, v) == 0)
#define atomic_dec_and_test_wrap(v) (atomic_sub_return_wrap(1, v) == 0)
#define atomic_add_and_test_wrap(i, v) (atomic_add_return_wrap(i, v) == 0)
#define atomic_sub_and_test_wrap(i, v) (atomic_sub_return_wrap(i, v) == 0)
#endif /* CONFIG_HARDENED_ATOMIC */

static inline void atomic_inc_wrap(atomic_wrap_t *v)
{
	atomic_add_wrap(1, v);
}

static inline void atomic_dec_wrap(atomic_wrap_t *v)
{
	atomic_sub_wrap(1, v);
}


#ifndef CONFIG_GENERIC_ATOMIC64
typedef struct {
	long long counter;
} atomic64_t;

#ifdef CONFIG_HARDENED_ATOMIC
typedef struct {
	long long counter;
} atomic64_wrap_t;
#else
typedef atomic64_t atomic64_wrap_t;
#endif /* CONFIG_HARDENED_ATOMIC */

#define ATOMIC64_INIT(i) { (i) }

#define __ATOMIC64_READ(suffix, asm_op)					\
static inline long long						        \
atomic64_read##suffix(const atomic64##suffix##_t *v)			\
{						                        \
	long long result;                                               \
									\
	__asm__ __volatile__("@ atomic64_read" #suffix "\n"		\
"	" #asm_op " %0, %H0, [%1]"					        \
	: "=&r" (result)						\
	: "r" (&v->counter), "Qo" (v->counter)	                        \
	);         							\
									\
	return result;							\
}

#ifdef CONFIG_ARM_LPAE
__ATOMIC64_READ(, ldrd)
__ATOMIC64_READ(wrap, ldrd)

#define __ATOMIC64_SET(suffix)					        \
static inline void atomic64_set##suffix(atomic64##suffix##_t *v, long long i) \
{									\
        __asm__ __volatile__("@ atomic64_set" #suffix "\n"		\
"	strd	%2, %H2, [%1]"					        \
	: "=Qo" (v->counter)						\
	: "r" (&v->counter), "r" (i)		                        \
	);							        \
}

__ATOMIC64_SET()
__ATOMIC64_SET(_wrap)

#undef __ATOMIC64

#else
__ATOMIC64_READ(, ldrexd)
__ATOMIC64_READ(_wrap, ldrexd)

#define __ATOMIC64_SET(suffix)					        \
static inline void atomic64_set##suffix(atomic64##suffix##_t *v, long long i) \
{									\
	long long tmp;                                                  \
									\
	prefetchw(&v->counter);						\
	__asm__ __volatile__("@ atomic64_set" #suffix"\n"               \
"1:	ldrexd	%0, %H0, [%2]\n"                                        \
"	strexd	%0, %3, %H3, [%2]\n"                                    \
"	teq	%0, #0\n"                                               \
"	bne	1b"                                                     \
	: "=&r" (tmp), "=Qo" (v->counter)				\
	: "r" (&v->counter), "r" (i)		                        \
	: "cc");                                                        \
}

__ATOMIC64_SET()
__ATOMIC64_SET(_wrap)

#undef __ATOMIC64_SET

#endif

#undef __ATOMIC64_READ

#define __ATOMIC64_OP(op, suffix, op1, op2, post_op, extable)		\
static inline void atomic64_##op##suffix(long long i, atomic64##suffix##_t *v) \
{									\
	long long result;						\
	unsigned long tmp;						\
									\
	prefetchw(&v->counter);						\
	__asm__ __volatile__("@ atomic64_" #op "\n"			\
"1:	ldrexd	%0, %H0, [%3]\n"					\
"	" #op1 " %Q0, %Q0, %Q4\n"					\
"	" #op2 " %R0, %R0, %R4\n"					\
        post_op					                        \
"	strexd	%1, %0, %H0, [%3]\n"					\
"	teq	%1, #0\n"						\
"	bne	1b\n"							\
	extable                                                         \
	: "=&r" (result), "=&r" (tmp), "+Qo" (v->counter)		\
	: "r" (&v->counter), "r" (i)					\
	: "cc");							\
}

#define ATOMIC64_OP(op, op1, op2)		                        \
	__ATOMIC64_OP(op, _wrap, op1, op2, , )				\
	__ATOMIC64_OP(op, , op1, op2##s, __OVERFLOW_POST, __OVERFLOW_EXTABLE)

#ifdef CONFIG_HARDENED_ATOMIC
#undef __OVERFLOW_POST_RETURN
#define __OVERFLOW_POST_RETURN			\
	"bvc 3f\n"				\
	"mov %0, %1\n"				\
	"mov %H0, %H1\n"			\
	"2: "HARDENED_ATOMIC_INSN"\n"		\
	"3:\n"
#endif /* CONFIG_HARDENED_ATOMIC */

#define __ATOMIC64_OP_RETURN(op, suffix, op1, op2, post_op, extable)	\
static inline long long							\
atomic64_##op##_return##suffix##_relaxed(long long i, atomic64##suffix##_t *v) \
{									\
	long long result;						\
	unsigned long tmp;						\
									\
	prefetchw(&v->counter);						\
									\
	__asm__ __volatile__("@ atomic64_" #op "_return\n"		\
"1:	ldrexd	%0, %H0, [%3]\n"					\
"	" #op1 " %Q0, %Q0, %Q4\n"					\
"	" #op2 " %R0, %R0, %R4\n"					\
	post_op                                                         \
"	strexd	%1, %0, %H0, [%3]\n"					\
"	teq	%1, #0\n"						\
"	bne	1b\n"							\
	extable                                                         \
	: "=&r" (result), "=&r" (tmp), "+Qo" (v->counter)		\
	: "r" (&v->counter), "r" (i)					\
	: "cc");							\
									\
	return result;							\
}

#define ATOMIC64_OP_RETURN(op, op1, op2)	                        \
	__ATOMIC64_OP_RETURN(op, _wrap, op1, op2, , )			\
	__ATOMIC64_OP_RETURN(op, , op1, op2##s, __OVERFLOW_POST_RETURN, \
			     __OVERFLOW_EXTABLE)

#define ATOMIC64_FETCH_OP(op, op1, op2)					\
static inline long long							\
atomic64_fetch_##op##_relaxed(long long i, atomic64_t *v)		\
{									\
	long long result, val;						\
	unsigned long tmp;						\
									\
	prefetchw(&v->counter);						\
									\
	__asm__ __volatile__("@ atomic64_fetch_" #op "\n"		\
"1:	ldrexd	%0, %H0, [%4]\n"					\
"	" #op1 " %Q1, %Q0, %Q5\n"					\
"	" #op2 " %R1, %R0, %R5\n"					\
"	strexd	%2, %1, %H1, [%4]\n"					\
"	teq	%2, #0\n"						\
"	bne	1b"							\
	: "=&r" (result), "=&r" (val), "=&r" (tmp), "+Qo" (v->counter)	\
	: "r" (&v->counter), "r" (i)					\
	: "cc");							\
									\
	return result;							\
}

#define ATOMIC64_OPS(op, op1, op2)					\
	ATOMIC64_OP(op, op1, op2)					\
	ATOMIC64_OP_RETURN(op, op1, op2)				\
	ATOMIC64_FETCH_OP(op, op1, op2)

ATOMIC64_OPS(add, adds, adc)
ATOMIC64_OPS(sub, subs, sbc)

#define atomic64_add_return_relaxed	atomic64_add_return_relaxed
#define atomic64_sub_return_relaxed	atomic64_sub_return_relaxed
#define atomic64_fetch_add_relaxed	atomic64_fetch_add_relaxed
#define atomic64_fetch_sub_relaxed	atomic64_fetch_sub_relaxed

#undef ATOMIC64_OPS
#define ATOMIC64_OPS(op, op1, op2)					\
	ATOMIC64_OP(op, op1, op2)					\
	ATOMIC64_FETCH_OP(op, op1, op2)

#define atomic64_andnot atomic64_andnot

ATOMIC64_OPS(and, and, and)
ATOMIC64_OPS(andnot, bic, bic)
ATOMIC64_OPS(or,  orr, orr)
ATOMIC64_OPS(xor, eor, eor)

#define atomic64_fetch_and_relaxed	atomic64_fetch_and_relaxed
#define atomic64_fetch_andnot_relaxed	atomic64_fetch_andnot_relaxed
#define atomic64_fetch_or_relaxed	atomic64_fetch_or_relaxed
#define atomic64_fetch_xor_relaxed	atomic64_fetch_xor_relaxed

#undef ATOMIC64_OPS
#undef ATOMIC64_FETCH_OP
#undef ATOMIC64_OP_RETURN
#undef __ATOMIC64_OP_RETURN
#undef ATOMIC64_OP
#undef __ATOMIC64_OP
#undef __OVERFLOW_EXTABLE
#undef __OVERFLOW_POST_RETURN
#undef __OVERFLOW_RETURN

#define __ATOMIC64_CMPXCHG_RELAXED(suffix)	                        \
static inline long long	atomic64_cmpxchg##suffix##_relaxed(             \
	atomic64##suffix##_t *ptr, long long old, long long new)	\
{									\
	long long oldval;                                               \
	unsigned long res;						\
									\
	prefetchw(&ptr->counter);					\
									\
	do {								\
		__asm__ __volatile__("@ atomic64_cmpxchg" #suffix "\n"  \
		"ldrexd		%1, %H1, [%3]\n"			\
		"mov		%0, #0\n"				\
		"teq		%1, %4\n"				\
		"teqeq		%H1, %H4\n"				\
		"strexdeq	%0, %5, %H5, [%3]"			\
		: "=&r" (res), "=&r" (oldval), "+Qo" (ptr->counter)     \
		: "r" (&ptr->counter), "r" (old), "r" (new)             \
		: "cc");                                                \
	} while (res);							\
									\
	return oldval;							\
}

__ATOMIC64_CMPXCHG_RELAXED()
__ATOMIC64_CMPXCHG_RELAXED(_wrap)
#define atomic64_cmpxchg_relaxed	atomic64_cmpxchg_relaxed

#undef __ATOMIC64_CMPXCHG_RELAXED

#define __ATOMIC64_XCHG_RELAXED(suffix)					\
static inline long long atomic64_xchg##suffix##_relaxed(                \
	atomic64##suffix##_t *ptr, long long new)			\
{									\
	long long result;                                               \
	unsigned long tmp;						\
									\
	prefetchw(&ptr->counter);					\
									\
	__asm__ __volatile__("@ atomic64_xchg" #suffix "\n"		\
"1:	ldrexd	%0, %H0, [%3]\n"                                        \
"	strexd	%1, %4, %H4, [%3]\n"                                    \
"	teq	%1, #0\n"                                               \
"	bne	1b"                                                     \
	: "=&r" (result), "=&r" (tmp), "+Qo" (ptr->counter)             \
	: "r" (&ptr->counter), "r" (new)                                \
	: "cc");                                                        \
									\
	return result;							\
}

__ATOMIC64_XCHG_RELAXED()
__ATOMIC64_XCHG_RELAXED(_wrap)
#define atomic64_xchg_relaxed		atomic64_xchg_relaxed

#undef __ATOMIC64_XCHG_RELAXED

static inline long long atomic64_dec_if_positive(atomic64_t *v)
{
	long long result;
	u64 tmp;

	smp_mb();
	prefetchw(&v->counter);

	__asm__ __volatile__("@ atomic64_dec_if_positive\n"
"1:	ldrexd	%1, %H1, [%3]\n"
"	subs	%Q0, %Q1, #1\n"
"	sbcs	%R0, %R1, #0\n"
#ifdef CONFIG_HARDENED_ATOMIC
"	bvc	3f\n"
"	mov	%Q0, %Q1\n"
"	mov	%R0, %R1\n"
"2:	"HARDENED_ATOMIC_INSN"\n"
"3:\n"
#endif
"	teq	%R0, #0\n"
"	bmi	4f\n"
"	strexd	%1, %0, %H0, [%3]\n"
"	teq	%1, #0\n"
"	bne	1b\n"
"4:\n"
#ifdef CONFIG_HARDENED_ATOMIC
       _ASM_EXTABLE(2b, 4b)
#endif
	: "=&r" (result), "=&r" (tmp), "+Qo" (v->counter)
	: "r" (&v->counter)
	: "cc");

	smp_mb();

	return result;
}

static inline int atomic64_add_unless(atomic64_t *v, long long a, long long u)
{
	long long val;
	unsigned long tmp;
	int ret = 1;

	smp_mb();
	prefetchw(&v->counter);

	__asm__ __volatile__("@ atomic64_add_unless\n"
"1:	ldrexd	%0, %H0, [%4]\n"
"	teq	%0, %5\n"
"	teqeq	%H0, %H5\n"
"	moveq	%1, #0\n"
"	beq	4f\n"
"	adds	%Q0, %Q0, %Q6\n"
"	adcs	%R0, %R0, %R6\n"
#ifdef CONFIG_HARDENED_ATOMIC
"       bvc     3f\n"
"2:     "HARDENED_ATOMIC_INSN"\n"
"3:\n"
#endif
"	strexd	%2, %0, %H0, [%4]\n"
"	teq	%2, #0\n"
"	bne	1b\n"
"4:\n"
#ifdef CONFIG_HARDENED_ATOMIC
       _ASM_EXTABLE(2b, 4b)
#endif
	: "=&r" (val), "+r" (ret), "=&r" (tmp), "+Qo" (v->counter)
	: "r" (&v->counter), "r" (u), "r" (a)
	: "cc");

	if (ret)
		smp_mb();

	return ret;
}

#define atomic64_add_negative(a, v)	(atomic64_add_return((a), (v)) < 0)
#define atomic64_inc(v)			atomic64_add(1LL, (v))
#define atomic64_inc_return_relaxed(v)	atomic64_add_return_relaxed(1LL, (v))
#define atomic64_inc_return_wrap_relaxed(v) atomic64_add_return_wrap_relaxed(1LL, v)
#define atomic64_inc_and_test(v)	(atomic64_inc_return(v) == 0)
#define atomic64_sub_and_test(a, v)	(atomic64_sub_return((a), (v)) == 0)
#define atomic64_dec(v)			atomic64_sub(1LL, (v))
#define atomic64_dec_return_relaxed(v)	atomic64_sub_return_relaxed(1LL, (v))
#define atomic64_dec_and_test(v)	(atomic64_dec_return((v)) == 0)
#define atomic64_inc_not_zero(v)	atomic64_add_unless((v), 1LL, 0LL)

#define atomic64_inc_wrap(v) atomic64_add_wrap(1LL, v)
#define atomic64_dec_wrap(v) atomic64_sub_wrap(1LL, v)

#endif /* !CONFIG_GENERIC_ATOMIC64 */
#endif
#endif
