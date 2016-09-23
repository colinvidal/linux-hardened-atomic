/*
 * This is for all the tests related to logic bugs (e.g. bad dereferences,
 * bad alignment, bad loops, bad locking, bad scheduling, deep stacks, and
 * lockups) along with other things that don't fit well into existing LKDTM
 * test source files.
 */
#include "lkdtm.h"
#include <linux/sched.h>

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

void lkdtm_ATOMIC_UNDERFLOW(void)
{
	atomic_t under = ATOMIC_INIT(INT_MIN);

	pr_info("attempting good atomic increment\n");
	atomic_inc(&under);
	atomic_dec(&under);

	pr_info("attempting bad atomic underflow\n");
	atomic_dec(&under);
}

void lkdtm_ATOMIC_DEC_RETURN_UNDERFLOW(void)
{
	atomic_t under = ATOMIC_INIT(INT_MIN);

	pr_info("attempting good atomic_dec_return\n");
	atomic_inc(&under);
	atomic_dec_return(&under);

	pr_info("attempting bad atomic_dec_return\n");
	atomic_dec_return(&under);
}

void lkdtm_ATOMIC_SUB_UNDERFLOW(void) {
	atomic_t under = ATOMIC_INIT(INT_MIN);

	pr_info("attempting good atomic subtract\n");
	atomic_add(10, &under);
	atomic_sub(10, &under);

	pr_info("attempting bad atomic subtract underflow\n");
	atomic_sub(10, &under);

}

void lkdtm_ATOMIC_SUB_RETURN_UNDERFLOW(void)
{
	atomic_t under = ATOMIC_INIT(INT_MIN);

	pr_info("attempting good atomic_sub_return\n");
	atomic_add(10, &under);
	atomic_sub_return(10, &under);

	pr_info("attempting bad atomic_sub_return underflow\n");
	atomic_sub_return(10, &under);

}

void lkdtm_ATOMIC_OVERFLOW(void)
{
	atomic_t over = ATOMIC_INIT(INT_MAX);

	pr_info("attempting good atomic decrement\n");
	atomic_dec(&over);
	atomic_inc(&over);

	pr_info("attempting bad atomic overflow\n");
	atomic_inc(&over);
}

void lkdtm_ATOMIC_INC_RETURN_OVERFLOW(void)
{
	atomic_t over = ATOMIC_INIT(INT_MAX);

	pr_info("attempting good atomic_inc_return\n");
	atomic_dec(&over);
	atomic_inc_return(&over);

	pr_info("attempting bad atomic_inc_return overflow\n");
	atomic_inc_return(&over);
}

void lkdtm_ATOMIC_ADD_OVERFLOW(void) {
	atomic_t over = ATOMIC_INIT(INT_MAX);

	pr_info("attempting good atomic add\n");
	atomic_sub(10, &over);
	atomic_add(10, &over);

	pr_info("attempting bad atomic add overflow\n");
	atomic_add(10, &over);
}

void lkdtm_ATOMIC_ADD_RETURN_OVERFLOW(void)
{
	atomic_t over = ATOMIC_INIT(INT_MAX);

	pr_info("attempting good atomic_add_return\n");
	atomic_sub(10, &over);
	atomic_add_return(10, &over);

	pr_info("attempting bad atomic_add_return overflow\n");
	atomic_add_return(10, &over);
}

void lkdtm_ATOMIC_ADD_UNLESS_OVERFLOW(void)
{
	atomic_t over = ATOMIC_INIT(INT_MAX);

	pr_info("attempting good atomic_add_unless\n");
	atomic_sub(10, &over);
	atomic_add_unless(&over, 10, 0);

	pr_info("attempting bad atomic_add_unless overflow\n");
	atomic_add_unless(&over, 10, 0);
}

void lkdtm_ATOMIC_INC_AND_TEST_OVERFLOW(void)
{
	atomic_t over = ATOMIC_INIT(INT_MAX);

	pr_info("attempting good atomic_inc_and_test\n");
	atomic_dec(&over);
	atomic_inc_and_test(&over);

	pr_info("attempting bad atomic_inc_and_test overflow\n");
	atomic_inc_and_test(&over);
}

void lkdtm_ATOMIC_LONG_UNDERFLOW(void)
{
	atomic_long_t under = ATOMIC_LONG_INIT(LONG_MIN);

	pr_info("attempting good atomic_long_dec\n");
	atomic_long_inc(&under);
	atomic_long_dec(&under);

	pr_info("attempting bad atomic_long_dec underflow\n");
	atomic_long_dec(&under);
}

void lkdtm_ATOMIC_LONG_DEC_RETURN_UNDERFLOW(void)
{
	atomic_long_t under = ATOMIC_LONG_INIT(LONG_MIN);

	pr_info("attempting good atomic_long_dec_return\n");
	atomic_long_inc(&under);
	atomic_long_dec_return(&under);

	pr_info("attempting bad atomic_long_dec_return underflow\n");
	atomic_long_dec_return(&under);
}

void lkdtm_ATOMIC_LONG_SUB_UNDERFLOW(void)
{
	atomic_long_t under = ATOMIC_INIT(LONG_MIN);

	pr_info("attempting good atomic_long_sub\n");
	atomic_long_add(10, &under);
	atomic_long_sub(10, &under);

	pr_info("attempting bad atomic_long_sub underflow\n");
	atomic_long_sub(10, &under);

}

void lkdtm_ATOMIC_LONG_SUB_RETURN_UNDERFLOW(void)
{
	atomic_long_t under = ATOMIC_INIT(LONG_MIN);

	pr_info("attempting good atomic_long_sub_return \n");
	atomic_long_add(10, &under);
	atomic_long_sub_return(10, &under);

	pr_info("attempting bad atomic_long_sub_return underflow\n");
	atomic_long_sub_return(10, &under);

}

void lkdtm_ATOMIC_LONG_OVERFLOW(void)
{
	atomic_long_t over = ATOMIC_LONG_INIT(LONG_MAX);

	pr_info("attempting good atomic_long_inc\n");
	atomic_long_dec(&over);
	atomic_long_inc(&over);

	pr_info("attempting bad atomic_long_inc overflow\n");
	atomic_long_inc(&over);
}

void lkdtm_ATOMIC_LONG_INC_RETURN_OVERFLOW(void)
{
	atomic_long_t over = ATOMIC_LONG_INIT(LONG_MAX);

	pr_info("attempting good atomic_ong_inc_return\n");
	atomic_long_dec(&over);
	atomic_long_inc_return(&over);

	pr_info("attempting bad atomic_long_inc_return overflow\n");
	atomic_long_inc_return(&over);
}

void lkdtm_ATOMIC_LONG_ADD_OVERFLOW(void)
{
	atomic_long_t over = ATOMIC_LONG_INIT(LONG_MAX);

	pr_info("attempting good atomic_long_add\n");
	atomic_long_sub(10, &over);
	atomic_long_add(10, &over);

	pr_info("attempting bad atomic_long_add overflow\n");
	atomic_long_add(10, &over);
}

void lkdtm_ATOMIC_LONG_ADD_RETURN_OVERFLOW(void)
{
	atomic_long_t over = ATOMIC_LONG_INIT(LONG_MAX);

	pr_info("attempting good atomic_long_add_return\n");
	atomic_long_sub(10, &over);
	atomic_long_add_return(10, &over);

	pr_info("attempting bad atomic_long_add_return overflow\n");
	atomic_long_add_return(10, &over);
}
