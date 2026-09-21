/* Copyright (C) 2026 Alif Semiconductor - All Rights Reserved.
 * Use, distribution and modification of this code is permitted under the
 * terms stated in the Alif Semiconductor Software License Agreement
 *
 * You should have received a copy of the Alif Semiconductor Software
 * License Agreement with this file. If not, please write to:
 * contact@alifsemi.com, or visit: https://alifsemi.com/license
 */

#ifndef ALIF_EMMC_TEST_H
#define ALIF_EMMC_TEST_H

#include <stdbool.h>
#include <stdint.h>
#include <zephyr/fs/fs.h>
#include <zephyr/ztest.h>

#define DISK_DRIVE_NAME "SD2"
#define DISK_MOUNT_PT   "/" DISK_DRIVE_NAME ":"

#define SDHC_NODE DT_NODELABEL(sdhc)
#define MMC_NODE  DT_CHILD(SDHC_NODE, mmc)

/* SDHCI offsets matching DWC SDHC (sdhc_dwc.h). */
#define SDHC_PSTATE_OFF  0x24
#define SDHC_HC1_OFF     0x28
#define SDHC_CLK_OFF     0x2c
#define SDHC_CAP1_OFF    0x40

#define SDHC_HC1_HS      BIT(2)
#define SDHC_HC1_EXT8    BIT(5)
#define SDHC_CAP1_8BIT   BIT(18)
#define SDHC_PSTATE_DAT47 0xf0

#define PIN_PAD(raw)  (((raw) >> 16) & 0xffu)
#define PIN_FUNC(raw) ((raw) & 0x7u)

#define EMMC_TEST_DIR  DISK_MOUNT_PT "/E8TEST"
#define EMMC_TEST_FILE DISK_MOUNT_PT "/E8TEST.DAT"

#define EMMC_FILE_BYTES 4096
#define EMMC_IO_SECS    16
#define EMMC_IO_BYTES   (EMMC_IO_SECS * 512)

extern const char *emmc_disk;
extern struct fs_mount_t emmc_mp;
extern uint32_t emmc_sector_count;
extern uint32_t emmc_sector_size;
extern uint8_t emmc_io_buf[EMMC_IO_BYTES] __aligned(32);
extern bool emmc_fat_mounted;
extern bool emmc_disk_ready;

int emmc_read_pin(uint32_t pin_no, uint32_t *raw);

static inline void emmc_skip_if_no_disk(void)
{
	if (!emmc_disk_ready) {
		ztest_test_skip();
	}
}

#endif /* ALIF_EMMC_TEST_H */
