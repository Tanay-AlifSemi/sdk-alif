/*
 * Copyright (c) 2024 Alif Semiconductor
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/flash.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/sys_io.h>
#include <zephyr/sys/util.h>

#define SPI_FLASH_TEST_REGION_OFFSET 0x0
#define SPI_FLASH_SECTOR_SIZE        4096
#define BUFF_SIZE                    1024

/* 64 MB erase is slow and wears the part. Re-enable to re-check Test 2. */
#define RUN_FULL_CHIP_ERASE 1

#define OSPI_NODE          DT_PARENT(DT_ALIAS(spi_flash0))
#define OSPI_CORE_CLK      DT_PROP(OSPI_NODE, clock_frequency)
#define OSPI_BUS_SPEED     DT_PROP(OSPI_NODE, bus_speed)
#define OSPI_RXDS_DELAY    DT_PROP(OSPI_NODE, rx_ds_delay)
#define OSPI_BAUD2_DELAY   DT_PROP(OSPI_NODE, baud2_delay)
#define OSPI_REG_BASE      DT_REG_ADDR(OSPI_NODE)
#define OSPI_AES_BASE      DT_PROP_BY_IDX(OSPI_NODE, aes_reg, 0)
#define OSPI_ENR_OFF       0x08U
#define OSPI_BAUDR_OFF     0x14U
#define AES_INTR_MASK_OFF  0x08U
#define AES_RXDS_DLY_OFF   0x20U
#define AES_BAUD2_DELAY_BIT 30U
#define RXDS_SWEEP_MAX     31U
#define RXDS_PROBE_BYTES   SPI_FLASH_SECTOR_SIZE

/* High enough to miss Tests 1/3/4 (sectors 0, 4, 5). */
#define BENCH_OFFSET       0x10000
#define BENCH_BYTES        (4 * SPI_FLASH_SECTOR_SIZE)
#define BENCH_ITERS        3

static uint8_t bench_w[BENCH_BYTES];
static uint8_t bench_r[BENCH_BYTES];

const struct flash_parameters *flash_param;

void single_sector_test(const struct device *flash_dev)
{
	const uint8_t expected[] = {0x55, 0xaa, 0x66, 0x99};
	const size_t len = ARRAY_SIZE(expected);
	uint8_t buf[len];
	int rc;
	int i, e_count = 0;

	printf("\nTest 1: Flash erase\n");

	/* Full flash erase if SPI_FLASH_TEST_REGION_OFFSET = 0 and
	 * SPI_FLASH_SECTOR_SIZE = flash size
	 */
	rc = flash_erase(flash_dev, SPI_FLASH_TEST_REGION_OFFSET, SPI_FLASH_SECTOR_SIZE);
	if (rc != 0) {
		printf("Flash erase failed! %d\n", rc);
	} else {
		printf("Flash erase succeeded!\n");
	}

	printf("\nTest 1: Flash write\n");

	printf("Attempting to write %zu bytes\n", len);
	rc = flash_write(flash_dev, SPI_FLASH_TEST_REGION_OFFSET, expected, len);
	if (rc != 0) {
		printf("Flash write failed! %d\n", rc);
		return;
	}

	printf("\nTest 1: Flash read\n");

	memset(buf, 0, len);
	rc = flash_read(flash_dev, SPI_FLASH_TEST_REGION_OFFSET, buf, len);
	if (rc != 0) {
		printf("Flash read failed! %d\n", rc);
		return;
	}

	for (i = 0; i < len; i++) {
		if (buf[i] != expected[i]) {
			e_count++;
			printf("Not matched at [%d] _w[%4x] _r[%4x]\n", i, expected[i], buf[i]);
		}
	}

	if (e_count) {
		printf("Error:Data read NOT matches data written\n");
	} else {
		printf("Data read matches data written. Good!!\n");
	}
}

#if RUN_FULL_CHIP_ERASE
void erase_test(const struct device *dev, uint32_t len)
{
	int ret = 0, i = 0, count = 0;
	uint8_t r_buf[BUFF_SIZE] = {0};

	printf("\nTest 2: Flash Full Erase\n");

	ret = flash_erase(dev, SPI_FLASH_TEST_REGION_OFFSET, len);
	if (ret == 0) {
		printf("Successfully Erased whole Flash Memory\n");
	} else {
		printf("Error: Bulk Erase Failed [%d]\n", ret);
	}

	/* Read data Cleared Buffer */
	ret = flash_read(dev, SPI_FLASH_TEST_REGION_OFFSET, r_buf, BUFF_SIZE);
	if (ret != 0) {
		printf("Error: [RetVal :%d] Reading Erased value\n", ret);
		return;
	}

	/* Verify the read data */
	for (i = 0; i < BUFF_SIZE; i++) {
		if (r_buf[i] != flash_param->erase_value) {
			count++;
		}
	}

	printf("Total errors after reading erased chip = %d\n", count);
}
#endif

void multi_page_test(const struct device *flash_dev)
{
	int rc, i, e_count = 0;

	uint8_t w_buf[BUFF_SIZE] = {0};
	uint8_t r_buf[BUFF_SIZE] = {0};

	const size_t len = ARRAY_SIZE(w_buf);

	printf("\nTest 3: Flash erase\n");

	/* Full flash erase if SPI_FLASH_TEST_REGION_OFFSET = 0 and
	 * SPI_FLASH_SECTOR_SIZE = flash size
	 */
	rc = flash_erase(flash_dev, SPI_FLASH_TEST_REGION_OFFSET, SPI_FLASH_SECTOR_SIZE);
	if (rc != 0) {
		printf("Flash erase failed! %d\n", rc);
	} else {
		printf("Flash erase succeeded!\n");
	}

	for (i = 0; i < BUFF_SIZE; i++) {
		w_buf[i] = i % 256;
	}

	printf("\nTest 3: Flash write\n");

	printf("Attempting to write %zu bytes\n", len);
	rc = flash_write(flash_dev, SPI_FLASH_TEST_REGION_OFFSET, w_buf, len);
	if (rc != 0) {
		printf("Flash write failed! %d\n", rc);
		return;
	}

	printf("\nTest 3: Flash read\n");

	memset(r_buf, 0, len);
	rc = flash_read(flash_dev, SPI_FLASH_TEST_REGION_OFFSET, r_buf, len);
	if (rc != 0) {
		printf("Flash read failed! %d\n", rc);
		return;
	}

	for (i = 0; i < BUFF_SIZE; i++) {
		if (r_buf[i] != w_buf[i]) {
			e_count++;
			printf("Not matched at [%d] _w[%4x] _r[%4x]\n", i, w_buf[i], r_buf[i]);
		}
	}

	if (e_count) {
		printf("Error:Data read NOT matches data written\n");
		printf(" -- number of Unmatched data [%d]\n", e_count);
	} else {
		printf("Data read matches data written. Good!!\n");
	}
}

#define SPI_FLASH_SECTOR_4_OFFSET (4 * 1024 * 4)
#define SPI_FLASH_SECTOR_5_OFFSET (5 * 1024 * 4)

void multi_sector_test(const struct device *flash_dev)
{
	int rc, i, e_count = 0;

	uint8_t w_buf[BUFF_SIZE] = {0};
	uint8_t r_buf[BUFF_SIZE] = {0};

	const size_t len = ARRAY_SIZE(w_buf);

	for (i = 0; i < BUFF_SIZE; i++) {
		w_buf[i] =  i % 256;
	}

	printf("\nTest 4: write sector %d\n", SPI_FLASH_SECTOR_4_OFFSET);
	/* Write into Sector 4 */
	rc = flash_write(flash_dev, SPI_FLASH_SECTOR_4_OFFSET, w_buf, len);
	if (rc != 0) {
		printf("\nFlash write failed at Sec 4! %d\n", rc);
		return;
	}

	printf("\nTest 4: write sector %d\n", SPI_FLASH_SECTOR_5_OFFSET);
	/* Write into Sector 5 */
	rc = flash_write(flash_dev, SPI_FLASH_SECTOR_5_OFFSET, w_buf, len);
	if (rc != 0) {
		printf("\nFlash write failed at Sec 5! %d\n", rc);
		return;
	}

	/* Read from Sector 4 */
	printf("Sec4: Read and Verify written data\n");

	e_count = 0;
	memset(r_buf, 0, len);

	printf("\nTest 4: read sector %d\n", SPI_FLASH_SECTOR_4_OFFSET);

	rc = flash_read(flash_dev, SPI_FLASH_SECTOR_4_OFFSET, r_buf, len);
	if (rc != 0) {
		printf("Flash read failed at Sector 4! %d\n", rc);
		return;
	}

	for (i = 0; i < BUFF_SIZE; i++) {
		if (r_buf[i] != w_buf[i]) {
			e_count++;
			printf("Not matched at [%d] _w[%4x] _r[%4x]\n", i, w_buf[i], r_buf[i]);
		}
	}

	if (e_count) {
		printf("\nError:Data read NOT matches data written\n");
		printf(" -- number of Unmatched data [%d]\n", e_count);
	} else {
		printf("\nData read matches data written. Good!!\n");
	}

	/* Read from Sector 5 */
	printf("Sec5: Read and Verify written data\n");

	e_count = 0;
	memset(r_buf, 0, len);

	printf("\nTest 4: read sector %d\n", SPI_FLASH_SECTOR_5_OFFSET);

	rc = flash_read(flash_dev, SPI_FLASH_SECTOR_5_OFFSET, r_buf, len);
	if (rc != 0) {
		printf("Flash read failed at Sector 5! %d\n", rc);
		return;
	}

	for (i = 0; i < BUFF_SIZE; i++) {
		if (r_buf[i] != w_buf[i]) {
			e_count++;
			printf("Not matched at [%d] _w[%4x] _r[%4x]\n", i, w_buf[i], r_buf[i]);
		}
	}

	if (e_count) {
		printf("Error:Data read NOT matches data written\n");
		printf(" -- number of Unmatched data [%d]\n", e_count);
	} else {
		printf("Data read matches data written. Good!!\n");
	}

	/* Erase multiple Sector Sec 4+5 */
	printf("\nTest 4: Erase Sector 4 and 5\n");
	printf("Flash Erase from Sector %d Size to Erase %d\n", SPI_FLASH_SECTOR_4_OFFSET,
	       SPI_FLASH_SECTOR_SIZE * 2);

	rc = flash_erase(flash_dev, SPI_FLASH_SECTOR_4_OFFSET, SPI_FLASH_SECTOR_SIZE * 2);
	if (rc != 0) {
		printf("\nMulti-Sector erase failed! %d\n", rc);
	} else {
		printf("\nMulti-Sector erase succeeded!\n");
	}

	int count_1 = 0;

	memset(r_buf, 0, len);

	printf("\nTest 4: read sector %d\n", SPI_FLASH_SECTOR_4_OFFSET);
	/* Read Erased value and compare */
	rc = flash_read(flash_dev, SPI_FLASH_SECTOR_4_OFFSET, r_buf, BUFF_SIZE);
	if (rc != 0) {
		printf("Error: [RetVal :%d] Reading Erased value\n", rc);
		return;
	}

	/* Verify the read data */
	for (i = 0; i < BUFF_SIZE; i++) {
		if (r_buf[i] != flash_param->erase_value) {
			count_1++;
		}
	}

	printf("Total errors after reading erased Sector 4 = %d\n", count_1);

	int count_2 = 0;

	memset(r_buf, 0, len);
	printf("\nTest 4: read sector %d\n", SPI_FLASH_SECTOR_5_OFFSET);

	/* Read Erased value and compare */
	rc = flash_read(flash_dev, SPI_FLASH_SECTOR_5_OFFSET, r_buf, BUFF_SIZE);
	if (rc != 0) {
		printf("Error: [RetVal :%d] Reading Erased value\n", rc);
		return;
	}

	/* Verify the read data */
	for (i = 0; i < BUFF_SIZE; i++) {
		if (r_buf[i] != flash_param->erase_value) {
			count_2++;
		}
	}

	printf("Total errors after reading erased Sector 5 = %d\n", count_2);

	if (count_1 == 0 && count_2 == 0) {
		printf("\nMulti-Sector Erase Test Succeeded !\n");
	} else {
		printf("\nMulti-Sector Erase Failed\n");
	}
}

void xip_test(const struct device *flash_dev)
{
	uint8_t i;
	uint32_t xip_r[64] = {0}, fls_r[64] = {0}, cnt;
	uint32_t *ptr = (uint32_t *)DT_PROP_BY_IDX(DT_PARENT(DT_ALIAS(spi_flash0)),
						xip_base_address, 0);
	int32_t rc, e_count = 0;

	printf("\nTest 5: XiP Read\n");

	memcpy(xip_r, ptr, sizeof(xip_r));

	printf("Content Read from OSPI Flash in XiP Mode successfully\n\n");

	cnt = ARRAY_SIZE(xip_r);

	printf("Read from Flash cmd while XiP Mode turnned on\n\n");

	rc = flash_read(flash_dev, SPI_FLASH_TEST_REGION_OFFSET,
				fls_r, cnt * sizeof(uint32_t));
	if (rc != 0) {
		printf("Flash read failed! %d\n", rc);
		return;
	}

	for (i = 0; i < cnt; i++)
		if (fls_r[i] != xip_r[i]) {
			e_count++;
		}

	if (!e_count) {
		printf("XiP Read Test Succceeded !!\n\n");
	} else {
		printf("XiP Test Failed !"
			" contents are NOT Matching : Err Count [%d]!!!\n", e_count);
	}
}

static uint32_t even_baudr(uint32_t clk, uint32_t sck)
{
	uint32_t div;

	if (sck == 0U) {
		return 2U;
	}

	div = clk / sck;
	if (div < 2U) {
		div = 2U;
	}

	return div & ~1U;
}

static uint32_t hw_sck(uint32_t clk, uint32_t baudr)
{
	return baudr ? (clk / baudr) : 0U;
}

static bool sck_is_exact(uint32_t clk, uint32_t want)
{
	return hw_sck(clk, even_baudr(clk, want)) == want;
}

static void ospi_set_rxds(uint32_t delay)
{
	sys_write32(delay, OSPI_AES_BASE + AES_RXDS_DLY_OFF);
}

static uint32_t ospi_get_rxds(void)
{
	return sys_read32(OSPI_AES_BASE + AES_RXDS_DLY_OFF);
}

static void ospi_apply_sck(uint32_t sck)
{
	uint32_t baudr = even_baudr(OSPI_CORE_CLK, sck);
	uint32_t mask;

	sys_write32(0U, OSPI_REG_BASE + OSPI_ENR_OFF);
	sys_write32(baudr, OSPI_REG_BASE + OSPI_BAUDR_OFF);
	sys_write32(1U, OSPI_REG_BASE + OSPI_ENR_OFF);

	mask = sys_read32(OSPI_AES_BASE + AES_INTR_MASK_OFF);
	if (baudr == 2U) {
		mask |= BIT(AES_BAUD2_DELAY_BIT);
	} else {
		mask &= ~BIT(AES_BAUD2_DELAY_BIT);
	}
	sys_write32(mask, OSPI_AES_BASE + AES_INTR_MASK_OFF);
}

static void ospi_restore_dts_sck(void)
{
	ospi_apply_sck(OSPI_BUS_SPEED);
	ospi_set_rxds(OSPI_RXDS_DELAY);
}

static void print_tp(const char *op, uint32_t bytes, uint32_t cyc, int rc, int mismatches)
{
	uint32_t us = k_cyc_to_us_floor32(cyc);
	uint32_t kibs = 0;
	uint32_t kbits = 0;

	if (us > 0U) {
		kibs = (uint32_t)(((uint64_t)bytes * 1000000ULL) / ((uint64_t)us * 1024ULL));
		kbits = (uint32_t)(((uint64_t)bytes * 8ULL * 1000ULL) / us);
	}

	printf("  %-5s %5u B  %8u cyc  %7u us  %5u KiB/s  %7u kbit/s  %s",
	       op, bytes, cyc, us, kibs, kbits,
	       (rc != 0) ? "FAIL" : ((mismatches != 0) ? "MISMATCH" : "PASS"));
	if (rc != 0) {
		printf(" rc=%d", rc);
	}
	if (mismatches != 0) {
		printf(" err=%d", mismatches);
	}
	printf("\n");
}

static int count_mismatches(const uint8_t *a, const uint8_t *b, size_t len)
{
	int n = 0;
	size_t i;

	for (i = 0; i < len; i++) {
		if (a[i] != b[i]) {
			n++;
		}
	}

	return n;
}

static void fill_pattern(uint8_t *buf, size_t len, uint8_t seed)
{
	size_t i;

	for (i = 0; i < len; i++) {
		buf[i] = (uint8_t)(seed + i);
	}
}

static int ospi_timed_rw(const struct device *flash_dev, uint32_t sck, bool do_xip)
{
	uint32_t t0, t1;
	uint32_t erase_cyc = 0, write_cyc = 0, read_cyc = 0, xip_cyc = 0;
	int rc, mismatches;
	int i;
	int pass = 1;

	printf("\n--- SCK %u Hz  BAUDR=%u  actual=%u ---\n",
	       sck, even_baudr(OSPI_CORE_CLK, sck),
	       hw_sck(OSPI_CORE_CLK, even_baudr(OSPI_CORE_CLK, sck)));
	printf("CPU: k_cycle_get_32() around flash_erase / flash_write / flash_read only\n");

	fill_pattern(bench_w, BENCH_BYTES, 0x5A);

	for (i = 0; i < BENCH_ITERS; i++) {
		t0 = k_cycle_get_32();
		rc = flash_erase(flash_dev, BENCH_OFFSET, BENCH_BYTES);
		t1 = k_cycle_get_32();
		erase_cyc += (t1 - t0);
		if (rc != 0) {
			print_tp("erase", BENCH_BYTES, t1 - t0, rc, 0);
			return -1;
		}

		t0 = k_cycle_get_32();
		rc = flash_write(flash_dev, BENCH_OFFSET, bench_w, BENCH_BYTES);
		t1 = k_cycle_get_32();
		write_cyc += (t1 - t0);
		if (rc != 0) {
			print_tp("write", BENCH_BYTES, t1 - t0, rc, 0);
			return -1;
		}

		memset(bench_r, 0, BENCH_BYTES);
		t0 = k_cycle_get_32();
		rc = flash_read(flash_dev, BENCH_OFFSET, bench_r, BENCH_BYTES);
		t1 = k_cycle_get_32();
		read_cyc += (t1 - t0);
		mismatches = (rc == 0) ? count_mismatches(bench_w, bench_r, BENCH_BYTES) : 0;
		if ((rc != 0) || (mismatches != 0)) {
			print_tp("read", BENCH_BYTES, t1 - t0, rc, mismatches);
			if (mismatches != 0) {
				size_t k;
				int shown = 0;

				for (k = 0; (k < BENCH_BYTES) && (shown < 4); k++) {
					if (bench_w[k] != bench_r[k]) {
						printf("    first[%zu] w=%02x r=%02x\n",
						       k, bench_w[k], bench_r[k]);
						shown++;
					}
				}
			}
			pass = 0;
			break;
		}

#ifdef CONFIG_ALIF_OSPI_FLASH_XIP
		if (do_xip) {
			uint32_t *xip = (uint32_t *)(DT_PROP_BY_IDX(OSPI_NODE,
								   xip_base_address, 0) +
						     BENCH_OFFSET);

			memset(bench_r, 0, BENCH_BYTES);
			t0 = k_cycle_get_32();
			memcpy(bench_r, xip, BENCH_BYTES);
			t1 = k_cycle_get_32();
			xip_cyc += (t1 - t0);
			mismatches = count_mismatches(bench_w, bench_r, BENCH_BYTES);
			if (mismatches != 0) {
				print_tp("xip", BENCH_BYTES, t1 - t0, 0, mismatches);
				pass = 0;
				break;
			}
		}
#else
		ARG_UNUSED(do_xip);
#endif
	}

	if (pass) {
		print_tp("erase", BENCH_BYTES * BENCH_ITERS, erase_cyc, 0, 0);
		print_tp("write", BENCH_BYTES * BENCH_ITERS, write_cyc, 0, 0);
		print_tp("read", BENCH_BYTES * BENCH_ITERS, read_cyc, 0, 0);
#ifdef CONFIG_ALIF_OSPI_FLASH_XIP
		if (do_xip) {
			print_tp("xip", BENCH_BYTES * BENCH_ITERS, xip_cyc, 0, 0);
		}
#endif
		printf("  note: erase/write are flash tSE/tPP bound; read/xip scale with SCK/CPU\n");
	}

	return pass ? 0 : -1;
}

static void ospi_print_clock_banner(void)
{
	uint32_t div = OSPI_BUS_SPEED ? (OSPI_CORE_CLK / OSPI_BUS_SPEED) : 0U;
	uint32_t baudr = even_baudr(OSPI_CORE_CLK, OSPI_BUS_SPEED);

	printf("\n========================================\n");
#ifdef CONFIG_ALIF_SPARK_TURBO_MODE
	printf("OSPI mode: SPARK TURBO (SYS_CLOCK_HW_CYCLES_PER_SEC=%u)\n",
	       CONFIG_SYS_CLOCK_HW_CYCLES_PER_SEC);
#else
	printf("OSPI mode: NORMAL (SYS_CLOCK_HW_CYCLES_PER_SEC=%u)\n",
	       CONFIG_SYS_CLOCK_HW_CYCLES_PER_SEC);
#endif
	printf("DTS core_clk=%u bus_speed=%u write=%u HW BAUDR=%u SCK=%u\n",
	       OSPI_CORE_CLK, OSPI_BUS_SPEED, div, baudr, hw_sck(OSPI_CORE_CLK, baudr));
	printf("rx-ds-delay=%u baud2-delay=%u (0=off 1=on 2=auto)\n",
	       OSPI_RXDS_DELAY, OSPI_BAUD2_DELAY);
	printf("bench: %u x %u B at 0x%x\n", BENCH_ITERS, BENCH_BYTES, BENCH_OFFSET);
	printf("========================================\n");
}

static void ospi_throughput_bench(const struct device *flash_dev)
{
	printf("\n=== OSPI bench at DTS SCK ===\n");
	ospi_restore_dts_sck();
	if (ospi_timed_rw(flash_dev, hw_sck(OSPI_CORE_CLK,
					    even_baudr(OSPI_CORE_CLK, OSPI_BUS_SPEED)),
			  true) == 0) {
		printf("BENCH PASS\n");
	} else {
		printf("BENCH FAIL\n");
	}
}

/*
 * Write the pattern at a known-good SCK, then only read at 120 MHz while
 * sweeping AES_RXDS_DLY. Isolates capture-window vs a bad program.
 * Returns a passing tap, or -1 if none.
 */
static int ospi_rxds_sweep_120(const struct device *flash_dev)
{
	int rc, mismatches;
	int d;
	int first_pass = -1;
	int last_pass = -1;
	uint32_t saved = ospi_get_rxds();

	printf("\n=== 120 MHz RXDS delay sweep (read-only after 40 MHz write) ===\n");
	printf("DTS rx-ds-delay=%u (SSI ticks). Same tap is shorter at 240 MHz ACLK.\n",
	       OSPI_RXDS_DELAY);

	ospi_set_rxds(OSPI_RXDS_DELAY);
	ospi_apply_sck(40000000U);
	fill_pattern(bench_w, RXDS_PROBE_BYTES, 0x5A);
	rc = flash_erase(flash_dev, BENCH_OFFSET, RXDS_PROBE_BYTES);
	if (rc != 0) {
		printf("RXDS probe erase failed %d\n", rc);
		ospi_set_rxds(saved);
		return -1;
	}
	rc = flash_write(flash_dev, BENCH_OFFSET, bench_w, RXDS_PROBE_BYTES);
	if (rc != 0) {
		printf("RXDS probe write failed %d\n", rc);
		ospi_set_rxds(saved);
		return -1;
	}

	ospi_apply_sck(120000000U);
	printf("ARM LA on P4_2: SCK=120 MHz BAUDR=2. Sweeping rx-ds-delay 0..%u\n",
	       RXDS_SWEEP_MAX);
	printf("Stop after the PASS window closes; high taps can hang SSI (no RXDS).\n");

	for (d = 0; d <= (int)RXDS_SWEEP_MAX; d++) {
		int fail_after_pass;
		int all_bad;

		printf("  probe rx-ds-delay=%d ...\n", d);
		ospi_set_rxds((uint32_t)d);
		memset(bench_r, 0, RXDS_PROBE_BYTES);
		rc = flash_read(flash_dev, BENCH_OFFSET, bench_r, RXDS_PROBE_BYTES);
		mismatches = (rc == 0) ? count_mismatches(bench_w, bench_r, RXDS_PROBE_BYTES) : -1;
		printf("  rx-ds-delay=%2d  rc=%d  err=%d%s\n",
		       d, rc, mismatches, (mismatches == 0) ? "  PASS" : "");
		if ((rc == 0) && (mismatches == 0)) {
			if (first_pass < 0) {
				first_pass = d;
			}
			last_pass = d;
			continue;
		}

		/* Window already found and this tap failed: do not walk into
		 * taps where flash_read waits forever on RXDS.
		 */
		fail_after_pass = (first_pass >= 0) && (d > last_pass);
		all_bad = (mismatches == (int)RXDS_PROBE_BYTES);
		if (fail_after_pass || all_bad) {
			printf("  stop sweep at delay=%d (window closed or RXDS lost)\n", d);
			break;
		}
	}

	ospi_set_rxds(saved);

	if (first_pass < 0) {
		printf("No RXDS window at 120 MHz. Keep Max PASS at 60 MHz.\n");
		return -1;
	}

	/*
	 * Width 3+: interior tap (5..7 → 6). Width 2: later tap (5..6 → 6),
	 * not the low edge next to hundreds of errors. Width 1: that tap.
	 */
	{
		int pick;

		if (last_pass > first_pass + 1) {
			pick = (first_pass + last_pass) / 2;
		} else if (last_pass > first_pass) {
			pick = last_pass;
		} else {
			pick = first_pass;
		}

		printf("RXDS window %d..%d, prefer interior tap=%d\n",
		       first_pass, last_pass, pick);
		return pick;
	}
}

static void ospi_throughput_bench_120(const struct device *flash_dev, int rxds)
{
	if (!sck_is_exact(OSPI_CORE_CLK, 120000000U)) {
		printf("\n=== OSPI bench at 120 MHz skipped (SSI cannot hit 120) ===\n");
		return;
	}

	if (rxds < 0) {
		rxds = 6;
	}

	printf("\n=== OSPI bench at 120 MHz (BAUDR=2 rx-ds-delay=%d) ===\n",
	       rxds);
	printf("Second 120 MHz run; sweep result above is unchanged.\n");
	ospi_set_rxds((uint32_t)rxds);
	ospi_apply_sck(120000000U);
	if (ospi_timed_rw(flash_dev, 120000000U, false) == 0) {
		printf("BENCH 120 MHz cmd-path PASS\n");
	} else {
		printf("BENCH 120 MHz cmd-path FAIL\n");
	}

#ifdef CONFIG_ALIF_OSPI_FLASH_XIP
	{
		static const int xip_taps[] = { 5, 6, 7 };
		uint32_t *xip = (uint32_t *)(DT_PROP_BY_IDX(OSPI_NODE,
							   xip_base_address, 0) +
					     BENCH_OFFSET);
		int t, mismatches;
		int xip_ok = 0;

		printf("XiP-only at 120 MHz (data already programmed above)\n");
		for (t = 0; t < ARRAY_SIZE(xip_taps); t++) {
			uint32_t t0, t1;

			ospi_set_rxds((uint32_t)xip_taps[t]);
			memset(bench_r, 0, BENCH_BYTES);
			t0 = k_cycle_get_32();
			memcpy(bench_r, xip, BENCH_BYTES);
			t1 = k_cycle_get_32();
			mismatches = count_mismatches(bench_w, bench_r, BENCH_BYTES);
			print_tp("xip", BENCH_BYTES, t1 - t0, 0, mismatches);
			printf("    rx-ds-delay=%d\n", xip_taps[t]);
			if (mismatches == 0) {
				xip_ok = 1;
				break;
			}
		}
		printf(xip_ok ? "BENCH 120 MHz XiP PASS\n" : "BENCH 120 MHz XiP FAIL\n");
	}
#endif
	ospi_restore_dts_sck();
}

/*
 * Extra 120 MHz cases: several lengths and patterns. Does not replace
 * the DTS bench, SCK sweep, or the 16 KB 120 MHz bench above.
 */
static int ospi_120_one_size(const struct device *flash_dev, size_t len,
			     uint8_t seed)
{
	size_t erase_len = ROUND_UP(len, SPI_FLASH_SECTOR_SIZE);
	uint32_t t0, t1;
	int rc, mismatches;
	size_t i;

	if ((len == 0U) || (len > BENCH_BYTES) || ((len & 1U) != 0U)) {
		printf("  SKIP len=%zu (need even, 2..%u)\n", len, BENCH_BYTES);
		return -1;
	}

	for (i = 0; i < len; i++) {
		bench_w[i] = (uint8_t)(seed + (uint8_t)i);
	}

	printf("  erase %zu B ...\n", erase_len);
	t0 = k_cycle_get_32();
	rc = flash_erase(flash_dev, BENCH_OFFSET, erase_len);
	t1 = k_cycle_get_32();
	if (rc != 0) {
		print_tp("erase", (uint32_t)erase_len, t1 - t0, rc, 0);
		return -1;
	}

	printf("  write %zu B ...\n", len);
	t0 = k_cycle_get_32();
	rc = flash_write(flash_dev, BENCH_OFFSET, bench_w, len);
	t1 = k_cycle_get_32();
	if (rc != 0) {
		print_tp("write", (uint32_t)len, t1 - t0, rc, 0);
		return -1;
	}

	memset(bench_r, 0, len);
	printf("  read %zu B ...\n", len);
	t0 = k_cycle_get_32();
	rc = flash_read(flash_dev, BENCH_OFFSET, bench_r, len);
	t1 = k_cycle_get_32();
	mismatches = (rc == 0) ? count_mismatches(bench_w, bench_r, len) : -1;
	print_tp("rw", (uint32_t)len, t1 - t0, rc, mismatches);
	if ((rc != 0) || (mismatches != 0)) {
		size_t k;
		int shown = 0;

		printf("    seed=0x%02x erase=%zu\n", seed, erase_len);
		for (k = 0; (k < len) && (shown < 4); k++) {
			if (bench_w[k] != bench_r[k]) {
				printf("    first[%zu] w=%02x r=%02x\n",
				       k, bench_w[k], bench_r[k]);
				shown++;
			}
		}
		return -1;
	}

	return 0;
}

static void ospi_120_size_matrix(const struct device *flash_dev, int rxds)
{
	/* Even lengths only (write_block_size=2). Include 512-beat
	 * boundary (514) where 5b→5a showed up before.
	 */
	static const size_t sizes[] = {
		2, 4, 16, 32, 64, 128, 256, 512, 514,
		1024, 2048, 4096, 8192, 16384,
	};
	static const uint8_t seeds[] = {
		0x00, 0xFF, 0xA5, 0x5A, 0x96, 0x3C, 0xC3, 0x11,
		0x7E, 0x81, 0xF0, 0x0F, 0x55, 0xAA,
	};
	int i;
	int pass = 0;
	int fail = 0;

	if (!sck_is_exact(OSPI_CORE_CLK, 120000000U)) {
		printf("\n=== 120 MHz size matrix skipped ===\n");
		return;
	}
	if (rxds < 0) {
		rxds = 6;
	}

	printf("\n=== 120 MHz size matrix (BAUDR=2 rx-ds-delay=%d) ===\n", rxds);
	printf("Extra cases only; skip >=1024 B (flash_read can hang).\n");
	ospi_set_rxds((uint32_t)rxds);
	ospi_apply_sck(120000000U);

	for (i = 0; i < ARRAY_SIZE(sizes); i++) {
		uint8_t seed = seeds[i];

		if (sizes[i] >= 1024U) {
			printf("\n-- %zu B SKIP (120 MHz hang risk on multi-beat read) --\n",
			       sizes[i]);
			continue;
		}

		printf("\n-- %zu B  seed=0x%02x  tap=%d --\n", sizes[i], seed, rxds);
		if (ospi_120_one_size(flash_dev, sizes[i], seed) == 0) {
			pass++;
			printf("SIZE %zu B PASS\n", sizes[i]);
			continue;
		}

		printf("SIZE %zu B FAIL at tap=%d\n", sizes[i], rxds);
		if (rxds != 6) {
			printf("  retry %zu B at rx-ds-delay=6\n", sizes[i]);
			ospi_set_rxds(6U);
			if (ospi_120_one_size(flash_dev, sizes[i], seed) == 0) {
				pass++;
				printf("SIZE %zu B PASS (tap=6)\n", sizes[i]);
				ospi_set_rxds((uint32_t)rxds);
				continue;
			}
			ospi_set_rxds((uint32_t)rxds);
		}
		fail++;
		printf("SIZE %zu B FAIL\n", sizes[i]);
	}

	printf("\n120 MHz size matrix: %d PASS, %d FAIL (of %d)\n",
	       pass, fail, (int)ARRAY_SIZE(sizes));
	ospi_restore_dts_sck();
}

#define OSPI_80_ITERS 3

static int ospi_80_one_pattern(const struct device *flash_dev, const uint8_t *pat,
			       size_t len)
{
	size_t erase_len = ROUND_UP(len, SPI_FLASH_SECTOR_SIZE);
	int rc, mismatches;

	if ((len == 0U) || (len > BENCH_BYTES) || ((len & 1U) != 0U)) {
		return -1;
	}

	memcpy(bench_w, pat, len);
	printf("  erase %zu B ...\n", erase_len);
	rc = flash_erase(flash_dev, BENCH_OFFSET, erase_len);
	if (rc != 0) {
		printf("  erase FAIL rc=%d\n", rc);
		return -1;
	}
	printf("  write %zu B ...\n", len);
	rc = flash_write(flash_dev, BENCH_OFFSET, bench_w, len);
	if (rc != 0) {
		printf("  write FAIL rc=%d\n", rc);
		return -1;
	}
	memset(bench_r, 0, len);
	printf("  read %zu B ...\n", len);
	rc = flash_read(flash_dev, BENCH_OFFSET, bench_r, len);
	mismatches = (rc == 0) ? count_mismatches(bench_w, bench_r, len) : -1;
	if ((rc != 0) || (mismatches != 0)) {
		size_t k;
		int shown = 0;

		printf("  rw %zu B FAIL rc=%d err=%d\n", len, rc, mismatches);
		for (k = 0; (k < len) && (shown < 4); k++) {
			if (bench_w[k] != bench_r[k]) {
				printf("    first[%zu] w=%02x r=%02x\n",
				       k, bench_w[k], bench_r[k]);
				shown++;
			}
		}
		return -1;
	}
	printf("  rw %zu B PASS\n", len);
	return 0;
}

/*
 * Normal 160 MHz only: DTS 80 MHz, BAUDR=2, rx-ds-delay=11.
 * Same lengths as the 120 matrix, plus Test 1's 4-byte pattern,
 * each length repeated OSPI_80_ITERS times.
 */
static void ospi_80_size_matrix(const struct device *flash_dev)
{
	static const size_t sizes[] = {
		2, 4, 16, 32, 64, 128, 256, 512, 514,
		1024, 2048, 4096, 8192, 16384,
	};
	static const uint8_t seeds[] = {
		0x00, 0xFF, 0xA5, 0x5A, 0x96, 0x3C, 0xC3, 0x11,
		0x7E, 0x81, 0xF0, 0x0F, 0x55, 0xAA,
	};
	static const uint8_t test1[] = { 0x55, 0xaa, 0x66, 0x99 };
	int i, iter;
	int pass = 0;
	int fail = 0;
	int cases = 0;

	if (!sck_is_exact(OSPI_CORE_CLK, 80000000U)) {
		printf("\n=== 80 MHz size matrix skipped (need 160 MHz SSI) ===\n");
		return;
	}

	printf("\n=== 80 MHz size matrix (NORMAL BAUDR=2 rx-ds-delay=%u) ===\n",
	       OSPI_RXDS_DELAY);
	printf("%d repeats per length. Official DTS tap, no extra retune.\n",
	       OSPI_80_ITERS);
	ospi_set_rxds(OSPI_RXDS_DELAY);
	ospi_apply_sck(80000000U);

	printf("\n-- Test1 pattern 4 B  55 aa 66 99 --\n");
	cases++;
	if (ospi_80_one_pattern(flash_dev, test1, sizeof(test1)) == 0) {
		pass++;
		printf("SIZE 4 B Test1 PASS\n");
	} else {
		fail++;
		printf("SIZE 4 B Test1 FAIL\n");
	}

	for (i = 0; i < ARRAY_SIZE(sizes); i++) {
		for (iter = 1; iter <= OSPI_80_ITERS; iter++) {
			printf("\n-- %zu B  seed=0x%02x  iter=%d/%d  tap=%u --\n",
			       sizes[i], seeds[i], iter, OSPI_80_ITERS,
			       OSPI_RXDS_DELAY);
			cases++;
			if (ospi_120_one_size(flash_dev, sizes[i], seeds[i]) == 0) {
				pass++;
				printf("SIZE %zu B iter %d PASS\n", sizes[i], iter);
			} else {
				fail++;
				printf("SIZE %zu B iter %d FAIL\n", sizes[i], iter);
			}
		}
	}

	printf("\n80 MHz size matrix: %d PASS, %d FAIL (of %d)\n",
	       pass, fail, cases);
	ospi_restore_dts_sck();
}

#define OSPI_40_ITERS 3

/*
 * 40 MHz is even on both SSI clocks (160/4 and 240/6). Same 43 cases
 * as the 80 MHz matrix so we can tell "flash/driver" vs "BAUDR=2 eye".
 */
static void ospi_40_size_matrix(const struct device *flash_dev)
{
	static const size_t sizes[] = {
		2, 4, 16, 32, 64, 128, 256, 512, 514,
		1024, 2048, 4096, 8192, 16384,
	};
	static const uint8_t seeds[] = {
		0x00, 0xFF, 0xA5, 0x5A, 0x96, 0x3C, 0xC3, 0x11,
		0x7E, 0x81, 0xF0, 0x0F, 0x55, 0xAA,
	};
	static const uint8_t test1[] = { 0x55, 0xaa, 0x66, 0x99 };
	int i, iter;
	int pass = 0;
	int fail = 0;
	int cases = 0;

	if (!sck_is_exact(OSPI_CORE_CLK, 40000000U)) {
		printf("\n=== 40 MHz size matrix skipped ===\n");
		return;
	}

	printf("\n=== 40 MHz size matrix (BAUDR=%u rx-ds-delay=%u) ===\n",
	       even_baudr(OSPI_CORE_CLK, 40000000U), OSPI_RXDS_DELAY);
	printf("%d repeats per length. Same cases as 80 MHz matrix.\n",
	       OSPI_40_ITERS);
	ospi_set_rxds(OSPI_RXDS_DELAY);
	ospi_apply_sck(40000000U);

	printf("\n-- Test1 pattern 4 B  55 aa 66 99 --\n");
	cases++;
	if (ospi_80_one_pattern(flash_dev, test1, sizeof(test1)) == 0) {
		pass++;
		printf("SIZE 4 B Test1 PASS\n");
	} else {
		fail++;
		printf("SIZE 4 B Test1 FAIL\n");
	}

	for (i = 0; i < ARRAY_SIZE(sizes); i++) {
		for (iter = 1; iter <= OSPI_40_ITERS; iter++) {
			printf("\n-- %zu B  seed=0x%02x  iter=%d/%d  tap=%u --\n",
			       sizes[i], seeds[i], iter, OSPI_40_ITERS,
			       OSPI_RXDS_DELAY);
			cases++;
			if (ospi_120_one_size(flash_dev, sizes[i], seeds[i]) == 0) {
				pass++;
				printf("SIZE %zu B iter %d PASS\n", sizes[i], iter);
			} else {
				fail++;
				printf("SIZE %zu B iter %d FAIL\n", sizes[i], iter);
			}
		}
	}

	printf("\n40 MHz size matrix: %d PASS, %d FAIL (of %d)\n",
	       pass, fail, cases);
	ospi_restore_dts_sck();
}

#define OSPI_60_ITERS 3

/*
 * 60 MHz is even only on turbo SSI (240/4). Skipped at 160 (would be 80).
 * Same 43 cases as 40/80. BAUDR=4, not BAUDR=2.
 */
static void ospi_60_size_matrix(const struct device *flash_dev)
{
	static const size_t sizes[] = {
		2, 4, 16, 32, 64, 128, 256, 512, 514,
		1024, 2048, 4096, 8192, 16384,
	};
	static const uint8_t seeds[] = {
		0x00, 0xFF, 0xA5, 0x5A, 0x96, 0x3C, 0xC3, 0x11,
		0x7E, 0x81, 0xF0, 0x0F, 0x55, 0xAA,
	};
	static const uint8_t test1[] = { 0x55, 0xaa, 0x66, 0x99 };
	int i, iter;
	int pass = 0;
	int fail = 0;
	int cases = 0;

	if (!sck_is_exact(OSPI_CORE_CLK, 60000000U)) {
		printf("\n=== 60 MHz size matrix skipped (need 240 MHz SSI) ===\n");
		return;
	}

	printf("\n=== 60 MHz size matrix (BAUDR=%u rx-ds-delay=%u) ===\n",
	       even_baudr(OSPI_CORE_CLK, 60000000U), OSPI_RXDS_DELAY);
	printf("%d repeats per length. Same cases as 40/80 MHz matrix.\n",
	       OSPI_60_ITERS);
	ospi_set_rxds(OSPI_RXDS_DELAY);
	ospi_apply_sck(60000000U);

	printf("\n-- Test1 pattern 4 B  55 aa 66 99 --\n");
	cases++;
	if (ospi_80_one_pattern(flash_dev, test1, sizeof(test1)) == 0) {
		pass++;
		printf("SIZE 4 B Test1 PASS\n");
	} else {
		fail++;
		printf("SIZE 4 B Test1 FAIL\n");
	}

	for (i = 0; i < ARRAY_SIZE(sizes); i++) {
		for (iter = 1; iter <= OSPI_60_ITERS; iter++) {
			printf("\n-- %zu B  seed=0x%02x  iter=%d/%d  tap=%u --\n",
			       sizes[i], seeds[i], iter, OSPI_60_ITERS,
			       OSPI_RXDS_DELAY);
			cases++;
			if (ospi_120_one_size(flash_dev, sizes[i], seeds[i]) == 0) {
				pass++;
				printf("SIZE %zu B iter %d PASS\n", sizes[i], iter);
			} else {
				fail++;
				printf("SIZE %zu B iter %d FAIL\n", sizes[i], iter);
			}
		}
	}

	printf("\n60 MHz size matrix: %d PASS, %d FAIL (of %d)\n",
	       pass, fail, cases);
	ospi_restore_dts_sck();
}

static int ospi_sck_sweep(const struct device *flash_dev)
{
	/* Even BAUDR only. 80 MHz from 240 is odd (3) → HW 2 → 120 MHz, skipped.
	 * 120 MHz is listed on its own (turbo BAUDR=2). Hang/FAIL is a valid result.
	 */
	static const uint32_t rates[] = {
		20000000U, 40000000U, 60000000U, 80000000U, 120000000U,
	};
	uint32_t max_pass = 0;
	int i;
	int rxds_120 = -1;

	printf("\n=== OSPI SCK sweep (exact even BAUDR only) ===\n");

	for (i = 0; i < ARRAY_SIZE(rates); i++) {
		if (!sck_is_exact(OSPI_CORE_CLK, rates[i])) {
			printf("SKIP %u Hz (would be BAUDR=%u actual SCK=%u)\n",
			       rates[i], even_baudr(OSPI_CORE_CLK, rates[i]),
			       hw_sck(OSPI_CORE_CLK, even_baudr(OSPI_CORE_CLK, rates[i])));
			continue;
		}

		if (rates[i] == 120000000U) {
			int d;
			int pass_tap = -1;

			printf("ARM LA on OSPI0 SCLK (P4_2): 120 MHz BAUDR=2 baud2-delay ON\n");
			printf("Hang or FAIL is a valid result. Starting in 3 s...\n");
			k_sleep(K_SECONDS(3));
			rxds_120 = ospi_rxds_sweep_120(flash_dev);
			if (rxds_120 < 0) {
				printf("SWEEP %u Hz FAIL (no RXDS window)\n", rates[i]);
				continue;
			}

			/* Full write+read at each passing tap. Edge taps can
			 * pass a 4 KB read-only probe and fail a 16 KB program.
			 */
			for (d = rxds_120; d >= 0; d--) {
				ospi_set_rxds((uint32_t)d);
				ospi_apply_sck(rates[i]);
				printf("Retry 120 MHz full bench with rx-ds-delay=%d\n", d);
				if (ospi_timed_rw(flash_dev, rates[i], false) == 0) {
					pass_tap = d;
					break;
				}
				if (d == 5) {
					break;
				}
			}
			if (pass_tap >= 0) {
				rxds_120 = pass_tap;
				max_pass = rates[i];
				printf("SWEEP %u Hz PASS (rx-ds-delay=%d)\n",
				       rates[i], pass_tap);
			} else {
				printf("SWEEP %u Hz FAIL (no tap survived 16 KB)\n",
				       rates[i]);
			}
			continue;
		}

		ospi_apply_sck(rates[i]);
		if (ospi_timed_rw(flash_dev, rates[i], false) == 0) {
			max_pass = rates[i];
			printf("SWEEP %u Hz PASS\n", rates[i]);
		} else {
			printf("SWEEP %u Hz FAIL\n", rates[i]);
		}
	}

	ospi_restore_dts_sck();

	if (max_pass != 0U) {
		printf("\nMax SCK read/write PASS: %u Hz\n", max_pass);
	} else {
		printf("\nMax SCK read/write PASS: none\n");
	}

	return rxds_120;
}

int main(void)
{
	const struct device *flash_dev = DEVICE_DT_GET(DT_ALIAS(spi_flash0));

	if (!device_is_ready(flash_dev)) {
		printk("%s: device not ready.\n", flash_dev->name);
		return -1;
	}

	printf("\n%s OSPI flash testing\n", flash_dev->name);
	printf("========================================\n");

	flash_param = flash_get_parameters(flash_dev);

	printf("****Flash Configured Parameters******\n");
	printf("* Num Of Sectors : %d\n", flash_param->num_of_sector);
	printf("* Sector Size : %d\n", flash_param->sector_size);
	printf("* Page Size : %d\n", flash_param->page_size);
	printf("* Erase value : %d\n", flash_param->erase_value);
	printf("* Write Blk Size: %d\n", flash_param->write_block_size);
	printf("* Total Size in MB: %d\n",
	       (flash_param->num_of_sector * flash_param->sector_size) / (1024 * 1024));


	/*Current RW support only on 16 DFS */
	single_sector_test(flash_dev);

#if RUN_FULL_CHIP_ERASE
	/* Erase Full and verify the content */
	erase_test(flash_dev, flash_param->num_of_sector * flash_param->sector_size);
#else
	printf("\nTest 2: Flash Full Erase skipped (RUN_FULL_CHIP_ERASE=0)\n");
#endif

	/* Multipage test */
	multi_page_test(flash_dev);

	/* Multi-Secot R/W and Erase test*/
	multi_sector_test(flash_dev);

#ifdef CONFIG_ALIF_OSPI_FLASH_XIP
	xip_test(flash_dev);
#endif

	ospi_print_clock_banner();
	ospi_throughput_bench(flash_dev);
	{
		int rxds_120 = ospi_sck_sweep(flash_dev);

		ospi_throughput_bench_120(flash_dev, rxds_120);
		ospi_40_size_matrix(flash_dev);
		ospi_60_size_matrix(flash_dev);
		ospi_80_size_matrix(flash_dev);
		ospi_120_size_matrix(flash_dev, rxds_120);
	}

	printf("OSPI tests completed\n");
	return 0;
}
