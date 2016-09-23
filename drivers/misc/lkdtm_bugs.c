/*
 * This is for all the tests related to logic bugs (e.g. bad dereferences,
 * bad alignment, bad loops, bad locking, bad scheduling, deep stacks, and
 * lockups) along with other things that don't fit well into existing LKDTM
 * test source files.
 */
#include "lkdtm.h"
#include <linux/list.h>
#include <linux/sched.h>

struct lkdtm_list {
	struct list_head node;
};

/*
 * Make sure our attempts to over run the kernel stack doesn't trigger
 * a compiler warning when CONFIG_FRAME_WARN is set. Then make sure we
 * recurse past the end of THREAD_SIZE by default.
 */
#if defined(CONFIG_FRAME_WARN) && (CONFIG_FRAME_WARN > 0)
#define REC_STACK_SIZE (CONFIG_FRAME_WARN / 2)
#else
#define REC_STACK_SIZE (THREAD_SIZE / 8)
#endif
#define REC_NUM_DEFAULT ((THREAD_SIZE / REC_STACK_SIZE) * 2)

static int recur_count = REC_NUM_DEFAULT;

static DEFINE_SPINLOCK(lock_me_up);

static int recursive_loop(int remaining)
{
	char buf[REC_STACK_SIZE];

	/* Make sure compiler does not optimize this away. */
	memset(buf, (remaining & 0xff) | 0x1, REC_STACK_SIZE);
	if (!remaining)
		return 0;
	else
		return recursive_loop(remaining - 1);
}

/* If the depth is negative, use the default, otherwise keep parameter. */
void __init lkdtm_bugs_init(int *recur_param)
{
	if (*recur_param < 0)
		*recur_param = recur_count;
	else
		recur_count = *recur_param;
}

void lkdtm_PANIC(void)
{
	panic("dumptest");
}

void lkdtm_BUG(void)
{
	BUG();
}

void lkdtm_WARNING(void)
{
	WARN_ON(1);
}

void lkdtm_EXCEPTION(void)
{
	*((int *) 0) = 0;
}

void lkdtm_LOOP(void)
{
	for (;;)
		;
}

void lkdtm_OVERFLOW(void)
{
	(void) recursive_loop(recur_count);
}

noinline void lkdtm_CORRUPT_STACK(void)
{
	/* Use default char array length that triggers stack protection. */
	char data[8];

	memset((void *)data, 0, 64);
}

void lkdtm_UNALIGNED_LOAD_STORE_WRITE(void)
{
	static u8 data[5] __attribute__((aligned(4))) = {1, 2, 3, 4, 5};
	u32 *p;
	u32 val = 0x12345678;

	p = (u32 *)(data + 1);
	if (*p == 0)
		val = 0x87654321;
	*p = val;
}

void lkdtm_SOFTLOCKUP(void)
{
	preempt_disable();
	for (;;)
		cpu_relax();
}

void lkdtm_HARDLOCKUP(void)
{
	local_irq_disable();
	for (;;)
		cpu_relax();
}

void lkdtm_SPINLOCKUP(void)
{
	/* Must be called twice to trigger. */
	spin_lock(&lock_me_up);
	/* Let sparse know we intended to exit holding the lock. */
	__release(&lock_me_up);
}

void lkdtm_HUNG_TASK(void)
{
	set_current_state(TASK_UNINTERRUPTIBLE);
	schedule();
}

#define ATOMIC_LKDTM_MIN(tag,fun) void lkdtm_ATOMIC_##tag(void)	\
{									\
	atomic_t atomic = ATOMIC_INIT(INT_MIN);				\
									\
	pr_info("attempting good atomic_" #fun "\n");			\
	atomic_inc(&atomic);						\
	TEST_FUNC(&atomic);						\
									\
	pr_info("attempting bad atomic_" #fun "\n");			\
	TEST_FUNC(&atomic);						\
}

#define ATOMIC_LKDTM_MAX(tag,fun,...)					\
void lkdtm_ATOMIC_##tag(void)						\
{									\
	atomic_t atomic = ATOMIC_INIT(INT_MAX);				\
									\
	pr_info("attempting good atomic_" #fun "\n");			\
	atomic_dec(&atomic);						\
	TEST_FUNC(&atomic);						\
									\
	pr_info("attempting bad atomic_" #fun "\n");			\
	TEST_FUNC(&atomic);						\
}

#define ATOMIC_LKDTM_LONG_MIN(tag,fun,...)				\
void lkdtm_ATOMIC_LONG_##tag(void)					\
{									\
	atomic_long_t atomic  = ATOMIC_LONG_INIT(LONG_MIN);		\
									\
	pr_info("attempting good atomic_long_" #fun "\n");		\
	atomic_long_inc(&atomic);					\
	TEST_FUNC(&atomic);						\
									\
	pr_info("attempting bad atomic_long_" #fun "\n");		\
	TEST_FUNC(&atomic);						\
}

#define ATOMIC_LKDTM_LONG_MAX(tag,fun,...)				\
void lkdtm_ATOMIC_LONG_##tag(void)					\
{									\
	atomic_long_t atomic = ATOMIC_LONG_INIT(LONG_MAX);		\
									\
	pr_info("attempting good atomic_long_" #fun "\n");		\
	atomic_long_dec(&atomic);					\
	TEST_FUNC(&atomic);						\
									\
	pr_info("attempting bad atomic_long_" #fun "\n");		\
	TEST_FUNC(&atomic);						\
}

#define TEST_FUNC(x) atomic_dec(x)
ATOMIC_LKDTM_MIN(UNDERFLOW,dec)
#undef TEST_FUNC
#define TEST_FUNC(x) atomic_dec_return(x)
ATOMIC_LKDTM_MIN(DEC_RETURN_UNDERFLOW,dec_return);
#undef TEST_FUNC
#define TEST_FUNC(x) atomic_sub(1,x)
ATOMIC_LKDTM_MIN(SUB_UNDERFLOW,sub);
#undef TEST_FUNC
#define TEST_FUNC(x) atomic_sub_return(1,x);
ATOMIC_LKDTM_MIN(SUB_RETURN_UNDERFLOW,sub_return);
#undef TEST_FUNC
#define TEST_FUNC(x) atomic_inc(x);
ATOMIC_LKDTM_MAX(OVERFLOW,inc);
#undef TEST_FUNC
#define TEST_FUNC(x) atomic_inc_return(x);
ATOMIC_LKDTM_MAX(INC_RETURN_OVERFLOW,inc_return);
#undef TEST_FUNC
#define TEST_FUNC(x) atomic_add(1,x);
ATOMIC_LKDTM_MAX(ADD_OVERFLOW,add);
#undef TEST_FUNC
#define TEST_FUNC(x) atomic_add_return(1,x);
ATOMIC_LKDTM_MAX(ADD_RETURN_OVERFLOW,add_return);
#undef TEST_FUNC
#define TEST_FUNC(x) atomic_add_unless(x,1,0);
ATOMIC_LKDTM_MAX(ADD_UNLESS_OVERFLOW,add_unless);
#undef TEST_FUNC
#define TEST_FUNC(x) atomic_inc_and_test(x);
ATOMIC_LKDTM_MAX(INC_AND_TEST_OVERFLOW,inc_and_test);
#undef TEST_FUNC

#define TEST_FUNC(x) atomic_long_dec(x);
ATOMIC_LKDTM_LONG_MIN(UNDERFLOW,dec);
#undef TEST_FUNC
#define TEST_FUNC(x) atomic_long_dec_return(x);
ATOMIC_LKDTM_LONG_MIN(DEC_RETURN_UNDERFLOW,dec_return);
#undef TEST_FUNC
#define TEST_FUNC(x) atomic_long_sub(1,x);
ATOMIC_LKDTM_LONG_MIN(SUB_UNDERFLOW,sub);
#undef TEST_FUNC
#define TEST_FUNC(x) atomic_long_sub_return(1,x);
ATOMIC_LKDTM_LONG_MIN(SUB_RETURN_UNDERFLOW,sub_return);
#undef TEST_FUNC
#define TEST_FUNC(x) atomic_long_inc(x);
ATOMIC_LKDTM_LONG_MAX(OVERFLOW,inc);
#undef TEST_FUNC
#define TEST_FUNC(x) atomic_long_inc_return(x);
ATOMIC_LKDTM_LONG_MAX(INC_RETURN_OVERFLOW,inc_return);
#undef TEST_FUNC
#define TEST_FUNC(x) atomic_long_add(1,x);
ATOMIC_LKDTM_LONG_MAX(ADD_OVERFLOW,add);
#undef TEST_FUNC
#define TEST_FUNC(x) atomic_long_add_return(1,x);
ATOMIC_LKDTM_LONG_MAX(ADD_RETURN_OVERFLOW,add_return);
#undef TEST_FUNC
#define TEST_FUNC(x) atomic_long_sub_and_test(1,x);
ATOMIC_LKDTM_LONG_MIN(SUB_AND_TEST,sub_and_test);
#undef TEST_FUNC

void lkdtm_CORRUPT_LIST_ADD(void)
{
	/*
	 * Initially, an empty list via LIST_HEAD:
	 *	test_head.next = &test_head
	 *	test_head.prev = &test_head
	 */
	LIST_HEAD(test_head);
	struct lkdtm_list good, bad;
	void *target[2] = { };
	void *redirection = &target;

	pr_info("attempting good list addition\n");

	/*
	 * Adding to the list performs these actions:
	 *	test_head.next->prev = &good.node
	 *	good.node.next = test_head.next
	 *	good.node.prev = test_head
	 *	test_head.next = good.node
	 */
	list_add(&good.node, &test_head);

	pr_info("attempting corrupted list addition\n");
	/*
	 * In simulating this "write what where" primitive, the "what" is
	 * the address of &bad.node, and the "where" is the address held
	 * by "redirection".
	 */
	test_head.next = redirection;
	list_add(&bad.node, &test_head);

	if (target[0] == NULL && target[1] == NULL)
		pr_err("Overwrite did not happen, but no BUG?!\n");
	else
		pr_err("list_add() corruption not detected!\n");
}

void lkdtm_CORRUPT_LIST_DEL(void)
{
	LIST_HEAD(test_head);
	struct lkdtm_list item;
	void *target[2] = { };
	void *redirection = &target;

	list_add(&item.node, &test_head);

	pr_info("attempting good list removal\n");
	list_del(&item.node);

	pr_info("attempting corrupted list removal\n");
	list_add(&item.node, &test_head);

	/* As with the list_add() test above, this corrupts "next". */
	item.node.next = redirection;
	list_del(&item.node);

	if (target[0] == NULL && target[1] == NULL)
		pr_err("Overwrite did not happen, but no BUG?!\n");
	else
		pr_err("list_del() corruption not detected!\n");
}
