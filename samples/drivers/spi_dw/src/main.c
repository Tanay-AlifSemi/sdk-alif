/* Copyright (C) 2025 Alif Semiconductor - All Rights Reserved.
 * Use, distribution and modification of this code is permitted under the
 * terms stated in the Alif Semiconductor Software License Agreement
 *
 * You should have received a copy of the Alif Semiconductor Software
 * License Agreement with this file. If not, please write to:
 * contact@alifsemi.com, or visit: https://alifsemi.com/license
 *
 */

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(app, LOG_LEVEL_INF);

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <string.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/drivers/clock_control.h>
#include <zephyr/dt-bindings/dma/alif_dma_event_router.h>
#include <soc_common.h>

#define Mhz		1000000
#define Khz		1000

/* master_spi and slave_spi aliases are defined in
 * overlay files to use different SPI instance if needed.
 */

#define SPIDW_NODE	DT_ALIAS(master_spi)

#define S_SPIDW_NODE	DT_ALIAS(slave_spi)

/* size of stack area used by each thread */
#define STACKSIZE 2048

/* default SPI master SS(slave select) is H/W controlled,
 * enable this to use as S/W controlled using gpio.
 */
#define SPI_MASTER_SS_SW_CONTROLLED_GPIO   0

#define SLAVE_PRIORITY  7

K_THREAD_STACK_DEFINE(SlaveT_stack, STACKSIZE);
static struct k_thread SlaveT_data;

static K_SEM_DEFINE(sem_slave_done, 0, 1);

/* Master and Slave buffer size */
#define BUFF_SIZE  200

/* Master and Slave buffer word size */
#define SPI_WORD_SIZE 8

/*
 * 10 MHz is an exact SCK on both LPSPI SSI clocks:
 *   160 MHz / 16 = 10 MHz,  240 MHz / 24 = 10 MHz  (BAUDR even).
 * 16 MHz is NOT: 160/10=16 MHz, 240/14≈17.14 MHz.
 */
#define SPI_BENCH_FREQ		(10 * Mhz)
#define SPI_NUM_TRANSFERS	10

/* Let the slave thread re-enter spi_transceive() after each master xfer. */
#define SPI_SLAVE_REARM_MS	50

/*
 * One-shot sweep: only rates whose actual SCK is identical at 160 and 240
 * (SSI / even(floor(SSI/f)) == f on both). 15/16/22/24/30 MHz are not.
 */
static const uint32_t sclk_sweep[] = {
	5 * Mhz, 8 * Mhz, 10 * Mhz, 20 * Mhz,
};

/* Master and Slave buffers */
static uint32_t master_txdata[BUFF_SIZE];
static uint32_t master_rxdata[BUFF_SIZE];
static uint32_t slave_txdata[BUFF_SIZE];
static uint32_t slave_rxdata[BUFF_SIZE];
static uint32_t slave_rx_snap[BUFF_SIZE];

/*
 * spi_dw_configure() skips work when ctx->config == config (pointer).
 * A stack spi_config has the same address every call, so BAUDR never
 * updates after the first transfer. Ping-pong two static objects.
 */
static struct spi_config master_cnfg[2];
static uint8_t master_cnfg_sel;
static struct spi_config slave_cnfg;
static uint32_t g_spi_freq = SPI_BENCH_FREQ;
static uint32_t g_ssi_hz;
static int g_slave_mosi_ret;
static bool g_slave_run = true;

static void prepare_data(uint32_t *data, uint16_t def_mask)
{
	for (uint32_t cnt = 0; cnt < BUFF_SIZE; cnt++) {
		data[cnt] = (def_mask << 16) | cnt;
	}
}

static int xfer_len(void)
{
	return BUFF_SIZE * (int)sizeof(uint32_t);
}

/* Same formula as spi_dw: BAUDR = SSI/f. DW SSI keeps BAUDR even (LSB=0). */
static uint32_t baudr_of(uint32_t ssi_hz, uint32_t freq)
{
	uint32_t div;

	if (!ssi_hz || !freq) {
		return 0;
	}
	div = ssi_hz / freq;
	if (div < 2U) {
		div = 2U;
	}
	return div & ~1U;
}

static uint32_t actual_sclk(uint32_t ssi_hz, uint32_t freq)
{
	uint32_t div = baudr_of(ssi_hz, freq);

	return div ? (ssi_hz / div) : 0;
}

static uint32_t read_lpspi_ssi(void)
{
	const struct device *clkdev = DEVICE_DT_GET(DT_CLOCKS_CTLR(SPIDW_NODE));
	clock_control_subsys_t subsys =
		(clock_control_subsys_t)DT_CLOCKS_CELL(SPIDW_NODE, clkid);
	uint32_t rate = 0;

	if (clock_control_get_rate(clkdev, subsys, &rate) != 0 || rate == 0) {
		rate = DT_PROP(DT_PATH(clocks, pll_clk1), clock_frequency);
	}
	return rate;
}

static void dump_mismatch(const char *tag, const uint32_t *exp, const uint32_t *got)
{
	printk("%s mismatch: exp %08x %08x %08x  got %08x %08x %08x\n",
	       tag,
	       exp[0], exp[1], exp[2],
	       got[0], got[1], got[2]);
}

static struct spi_config *master_cfg(uint32_t freq, struct spi_cs_control *cs)
{
	/* New pointer whenever frequency changes so the driver rewrites BAUDR. */
	if (master_cnfg[master_cnfg_sel].frequency != freq) {
		master_cnfg_sel ^= 1U;
	}

	struct spi_config *c = &master_cnfg[master_cnfg_sel];

	c->frequency = freq;
	c->operation = SPI_OP_MODE_MASTER | SPI_WORD_SET(SPI_WORD_SIZE);
	c->slave = 0;
	c->cs = *cs;
	return c;
}

static uint32_t cyc_to_us(uint32_t cyc)
{
	return (uint32_t)(k_cyc_to_ns_floor64(cyc) / 1000U);
}

static void print_sclk_line(uint32_t freq)
{
	uint32_t div = baudr_of(g_ssi_hz, freq);
	uint32_t act = actual_sclk(g_ssi_hz, freq);

	printk("  req %u Hz -> BAUDR=%u  actual SCK=%u Hz%s\n",
	       freq, div, act,
	       (act == freq) ? "" : "  (NOT equal to request)");
}

/*
 * Send/Receive data through slave spi
 */
static int slave_spi_transceive(const struct device *dev)
{
	int ret;
	int length = xfer_len();

	memset(slave_rxdata, 0, sizeof(slave_rxdata));

	slave_cnfg.frequency = g_spi_freq;
	slave_cnfg.operation = SPI_OP_MODE_SLAVE | SPI_WORD_SET(SPI_WORD_SIZE);
	slave_cnfg.slave = 0;

	struct spi_buf rx_buf = {
		.buf = slave_rxdata,
		.len = length
	};
	struct spi_buf_set rx_bufset = {
		.buffers = &rx_buf,
		.count = 1
	};
	struct spi_buf tx_buf = {
		.buf = slave_txdata,
		.len = length
	};
	struct spi_buf_set tx_bufset = {
		.buffers = &tx_buf,
		.count = 1
	};

	ret = spi_transceive(dev, &slave_cnfg, &tx_bufset, &rx_bufset);
	if (ret < 0) {
		printk("ERROR: Slave SPI Transceive: %d\n", ret);
		return ret;
	}

	ret = memcmp(master_txdata, slave_rxdata, length);
	g_slave_mosi_ret = ret;
	memcpy(slave_rx_snap, slave_rxdata, length);
	k_sem_give(&sem_slave_done);
	return ret;
}

/*
 * Send/Receive data through master spi.
 * cycles_out is only spi_transceive() — not memset/memcmp/sleep.
 */
static int master_spi_transceive(const struct device *dev,
				 struct spi_cs_control *cs,
				 uint32_t freq,
				 uint32_t *cycles_out)
{
	int ret;
	int length = xfer_len();
	struct spi_config *cnfg = master_cfg(freq, cs);
	uint32_t t0, t1;

	memset(master_rxdata, 0, sizeof(master_rxdata));

	struct spi_buf tx_buf = {
		.buf = master_txdata,
		.len = length
	};
	struct spi_buf_set tx_bufset = {
		.buffers = &tx_buf,
		.count = 1
	};
	struct spi_buf rx_buf = {
		.buf = master_rxdata,
		.len = length
	};
	struct spi_buf_set rx_bufset = {
		.buffers = &rx_buf,
		.count = 1
	};

	t0 = k_cycle_get_32();
	ret = spi_transceive(dev, cnfg, &tx_bufset, &rx_bufset);
	t1 = k_cycle_get_32();
	if (cycles_out) {
		*cycles_out = t1 - t0;
	}

	if (ret) {
		printk("ERROR: SPI=%p transceive: %d\n", dev, ret);
		return ret;
	}

	ret = memcmp(master_rxdata, slave_txdata, length);
	return ret;
}

static void slave_spi(void *p1, void *p2, void *p3)
{
	const struct device *const slave_dev = DEVICE_DT_GET(S_SPIDW_NODE);

	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	if (!device_is_ready(slave_dev)) {
		printk("%s: Slave device not ready\n", slave_dev->name);
		return;
	}

	/* Stay armed in spi_transceive() — same model as the original sample. */
	while (g_slave_run) {
		slave_spi_transceive(slave_dev);
	}
}

static int run_one_xfer(const struct device *dev, struct spi_cs_control *cs,
			uint32_t freq, uint32_t *cycles_out)
{
	uint32_t cyc = 0;
	int mret;

	g_spi_freq = freq;
	g_slave_mosi_ret = -1;

	mret = master_spi_transceive(dev, cs, freq, &cyc);
	if (cycles_out) {
		*cycles_out = cyc;
	}

	if (k_sem_take(&sem_slave_done, K_MSEC(500))) {
		printk("ERROR: slave did not finish this transfer\n");
		k_msleep(SPI_SLAVE_REARM_MS);
		return -EIO;
	}

	/* Slave is looping back into spi_transceive(); give it time to arm. */
	k_msleep(SPI_SLAVE_REARM_MS);

	if (mret) {
		dump_mismatch("MISO (master RX / slave TX)", slave_txdata, master_rxdata);
	}
	if (g_slave_mosi_ret) {
		dump_mismatch("MOSI (master TX / slave RX)", master_txdata, slave_rx_snap);
	}

	if (mret || g_slave_mosi_ret) {
		return -EIO;
	}
	return 0;
}

static void print_mode_banner(void)
{
#ifdef CONFIG_ALIF_SPARK_TURBO_MODE
	printk("SPI mode: SPARK TURBO (SYS_CLOCK_HW_CYCLES_PER_SEC=%u)\n",
	       CONFIG_SYS_CLOCK_HW_CYCLES_PER_SEC);
#else
	printk("SPI mode: normal (SYS_CLOCK_HW_CYCLES_PER_SEC=%u)\n",
	       CONFIG_SYS_CLOCK_HW_CYCLES_PER_SEC);
#endif
	printk("LPSPI SSI (get_rate)=%u  BAUDR(10MHz)=%u  actual SCK=%u\n",
	       g_ssi_hz, baudr_of(g_ssi_hz, SPI_BENCH_FREQ),
	       actual_sclk(g_ssi_hz, SPI_BENCH_FREQ));
}

static void run_bench(const struct device *dev, struct spi_cs_control *cs)
{
	uint32_t cyc, cyc_sum = 0;
	uint32_t bytes = (uint32_t)xfer_len();
	uint32_t total_bytes = bytes * SPI_NUM_TRANSFERS;
	uint32_t act = actual_sclk(g_ssi_hz, SPI_BENCH_FREQ);
	int fail = 0;

	printk("\n=== SPI bench: %u x %u bytes ===\n",
	       SPI_NUM_TRANSFERS, bytes);
	print_sclk_line(SPI_BENCH_FREQ);

	/* Drop first xfer so BAUDR/DMA configure is not in the sum. */
	(void)run_one_xfer(dev, cs, SPI_BENCH_FREQ, NULL);

	for (uint32_t i = 0; i < SPI_NUM_TRANSFERS; i++) {
		if (run_one_xfer(dev, cs, SPI_BENCH_FREQ, &cyc)) {
			fail++;
			printk("  xfer %u FAIL (master_cyc=%u)\n", i + 1, cyc);
		} else {
			cyc_sum += cyc;
		}
	}

	if (fail) {
		printk("BENCH FAIL: %d / %u transfers failed\n", fail, SPI_NUM_TRANSFERS);
		return;
	}

	uint32_t us = cyc_to_us(cyc_sum);
	uint32_t us_from_hz = (uint32_t)((cyc_sum * 1000000ULL) /
					 CONFIG_SYS_CLOCK_HW_CYCLES_PER_SEC);
	uint32_t kbps = us ? (uint32_t)((total_bytes * 8000ULL) / us) : 0;
	uint32_t wire_us = act ? (uint32_t)((total_bytes * 8ULL * 1000000ULL) / act) : 0;

	printk("CPU: k_cycle_get_32() around spi_transceive() only (no memset/memcmp/sleep)\n");
	printk("  transfers     : %u (after 1 warmup)\n", SPI_NUM_TRANSFERS);
	printk("  bytes/xfer    : %u (one direction)\n", bytes);
	printk("  total bytes   : %u (one direction)\n", total_bytes);
	printk("  CPU cycles    : %u\n", cyc_sum);
	printk("  time          : %u us  (k_cyc_to_ns)  check %u us (cyc*1e6/SYS_CLOCK)\n",
	       us, us_from_hz);
	printk("  wire (ideal)  : %u us at actual SCK %u Hz\n", wire_us, act);
	printk("  us/transfer   : %u\n", us / SPI_NUM_TRANSFERS);
	printk("  throughput    : %u kbit/s (payload, one direction)\n", kbps);
	printk("  full-duplex   : %u kbit/s (both directions)\n", kbps * 2U);
	printk("BENCH PASS\n");
}

static void run_sclk_sweep(const struct device *dev, struct spi_cs_control *cs)
{
	uint32_t max_pass = 0;

	printk("\n=== SPI SCLK sweep (1 xfer; rates identical on 160 and 240 MHz SSI) ===\n");

	for (size_t i = 0; i < ARRAY_SIZE(sclk_sweep); i++) {
		uint32_t freq = sclk_sweep[i];
		uint32_t cyc = 0;
		uint32_t bytes = (uint32_t)xfer_len();
		uint32_t act = actual_sclk(g_ssi_hz, freq);
		uint32_t wire_us = act ? (uint32_t)((bytes * 8ULL * 1000000ULL) / act) : 0;
		int ok = (run_one_xfer(dev, cs, freq, &cyc) == 0);
		uint32_t meas_us = cyc_to_us(cyc);

		printk("  %2u MHz : %s  cyc=%u  %u us  wire=%u us  BAUDR=%u actual=%u Hz\n",
		       freq / Mhz, ok ? "PASS" : "FAIL", cyc, meas_us, wire_us,
		       baudr_of(g_ssi_hz, freq), act);
		if (ok) {
			max_pass = freq;
		}
	}

	if (max_pass) {
		printk("Max SCLK both directions PASS: %u MHz\n", max_pass / Mhz);
	} else {
		printk("Max SCLK both directions PASS: none\n");
	}
}

int main(void)
{
	const struct device *const dev = DEVICE_DT_GET(SPIDW_NODE);

	g_ssi_hz = read_lpspi_ssi();
	print_mode_banner();

	if (!device_is_ready(dev)) {
		printk("%s: Master device not ready.\n", dev->name);
		return 0;
	}

	prepare_data(master_txdata, 0xA5A5);
	prepare_data(slave_txdata, 0x5A5A);

	k_tid_t tids = k_thread_create(&SlaveT_data, SlaveT_stack, STACKSIZE,
			&slave_spi, NULL, NULL, NULL,
			SLAVE_PRIORITY, 0, K_NO_WAIT);
	if (tids == NULL) {
		printk("Error creating Slave Thread\n");
		return 0;
	}
	k_thread_start(&SlaveT_data);
	/* First slave transceive must be waiting before the master clocks. */
	k_msleep(100);

#if SPI_MASTER_SS_SW_CONTROLLED_GPIO
	struct spi_cs_control cs_ctrl = (struct spi_cs_control){
		.gpio  = GPIO_DT_SPEC_GET(SPIDW_NODE, cs_gpios),
		.delay = 100u,
	};
#else
	struct spi_cs_control cs_ctrl = {0};
#endif

	run_bench(dev, &cs_ctrl);
	run_sclk_sweep(dev, &cs_ctrl);

	g_slave_run = false;
	printk("SPI tests completed\n");
	return 0;
}
