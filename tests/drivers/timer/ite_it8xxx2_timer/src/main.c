/*
 * SPDX-FileCopyrightText: Copyright The Zephyr Project Contributors
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <inttypes.h>
#include <stdint.h>

#include <zephyr/irq.h>
#include <zephyr/kernel.h>
#include <zephyr/ztest.h>

/* Exported from drivers/timer/ite_it8xxx2_timer.c via ZTESTABLE_STATIC. */
int sys_clock_driver_init(void);

#define REINIT_ITERS          16U
#define CNTOX_LATCH_WAIT_US   100U
#define MIN_TIMER_ADVANCE_US  2000U
#define SUBTICK_PHASE_STEP_US 2U
#define MIN_UPTIME_STEP_MS    (-100)
#define MAX_UPTIME_STEP_MS    1000

ZTEST(ite_it8xxx2_timer, test_reinit_while_running)
{
	uint32_t entry_cyc = k_cycle_get_32();

	for (uint32_t i = 0U; i < REINIT_ITERS; i++) {
		uint32_t phase_offset_us = i * SUBTICK_PHASE_STEP_US;

		k_busy_wait(MIN_TIMER_ADVANCE_US + phase_offset_us);

		unsigned int key = irq_lock();
		int64_t up_before = k_uptime_get();
		int ret = sys_clock_driver_init();

		k_busy_wait(CNTOX_LATCH_WAIT_US);
		uint32_t cyc = k_cycle_get_32();
		int64_t up_after = k_uptime_get();

		irq_unlock(key);

		int64_t up_delta = up_after - up_before;

		zassert_equal(ret, 0, "sys_clock_driver_init failed (%d) on iter %" PRIu32, ret, i);
		zassert_true(cyc < (uint32_t)CONFIG_SYS_CLOCK_HW_CYCLES_PER_SEC,
			     "iter %" PRIu32 " cycle %" PRIu32 " did not reset", i, cyc);
		/*
		 * Re-init resets FREE_RUN_TIMER and the timer core cycle baseline without
		 * announcing elapsed ticks, so k_uptime_get() can drop by the few ms spent since
		 * the last announce; an unlatched seed underflows by ~36.4 hours.
		 */
		zassert_true((up_delta >= MIN_UPTIME_STEP_MS) && (up_delta < MAX_UPTIME_STEP_MS),
			     "iter %" PRIu32 " uptime jump: before=%" PRId64 " after=%" PRId64
			     " (delta=%" PRId64 " ms)",
			     i, up_before, up_after, up_delta);
	}

	while (k_cycle_get_32() <= entry_cyc) {
		k_busy_wait(CNTOX_LATCH_WAIT_US);
	}
}

ZTEST_SUITE(ite_it8xxx2_timer, NULL, NULL, NULL, NULL, NULL);
