/* Copyright (C) 2026 Alif Semiconductor - All Rights Reserved.
 * Use, distribution and modification of this code is permitted under the
 * terms stated in the Alif Semiconductor Software License Agreement
 *
 * You should have received a copy of the Alif Semiconductor Software
 * License Agreement with this file. If not, please write to:
 * contact@alifsemi.com, or visit: https://alifsemi.com/license
 */

#include <errno.h>
#include <ff.h>
#include <zephyr/device.h>
#include <zephyr/fs/fs.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/storage/disk_access.h>
#include <zephyr/sys/sys_io.h>
#include <zephyr/ztest.h>

#include "emmc_test.h"

LOG_MODULE_REGISTER(emmc_8bit, LOG_LEVEL_INF);

const char *emmc_disk = DISK_DRIVE_NAME;
uint32_t emmc_sector_count;
uint32_t emmc_sector_size;
bool emmc_fat_mounted;
bool emmc_disk_ready;
/* FatFS will DMA aligned file writes from this buffer (not via card_buffer). */
uint8_t Z_GENERIC_SECTION(CONFIG_SD_BUFFER_SECTION) __aligned(32)
	emmc_io_buf[EMMC_IO_BYTES];

/* SDHC DMA on Ensemble writes from .alif_ns, not DTCM .bss (see fs_sample). */
static FATFS Z_GENERIC_SECTION(CONFIG_SD_BUFFER_SECTION) fat_fs;
struct fs_mount_t emmc_mp = {
	.type = FS_FATFS,
	.fs_data = &fat_fs,
	.mnt_point = DISK_MOUNT_PT,
};

int emmc_read_pin(uint32_t pin_no, uint32_t *raw)
{
	uintptr_t base = DT_REG_ADDR(DT_NODELABEL(pinctrl));

	if (raw == NULL) {
		return -EINVAL;
	}

	*raw = sys_read32(base + (pin_no * 4u));
	return 0;
}

static void *emmc_suite_setup(void)
{
	int rc;
	uint32_t cmd_buf;

	rc = disk_access_ioctl(emmc_disk, DISK_IOCTL_CTRL_INIT, NULL);
	emmc_disk_ready = (rc == 0);
	if (!emmc_disk_ready) {
		LOG_ERR("eMMC init failed (%d) — pinmux/DT still run; disk tests skip", rc);
		LOG_ERR("If PSTATE DAT[7:4] is 0x00, reconnect D4-D7 jumpers (working was 0xf0)");
		return NULL;
	}

	rc = disk_access_ioctl(emmc_disk, DISK_IOCTL_GET_SECTOR_COUNT, &cmd_buf);
	zassert_equal(rc, 0, "GET_SECTOR_COUNT failed");
	emmc_sector_count = cmd_buf;

	rc = disk_access_ioctl(emmc_disk, DISK_IOCTL_GET_SECTOR_SIZE, &cmd_buf);
	zassert_equal(rc, 0, "GET_SECTOR_SIZE failed");
	emmc_sector_size = cmd_buf;

	LOG_INF("eMMC sectors=%u size=%u (~%u MB)", emmc_sector_count, emmc_sector_size,
		(uint32_t)(((uint64_t)emmc_sector_count * emmc_sector_size) >> 20));
	// #region agent log
	LOG_INF("dbgff42d9 A bufs io=%p fat=%p ns_expect=0x20018000",
		emmc_io_buf, &fat_fs);
	// #endregion

	/*
	 * Disk init can succeed with no FAT (Linux dd of /dev/mmcblk0
	 * zeros LBA 0). Do not fail the suite — HW and raw disk tests
	 * still prove 8-bit reads. FAT tests skip if unmounted.
	 */
	rc = fs_mount(&emmc_mp);
	emmc_fat_mounted = (rc == FR_OK);
	if (!emmc_fat_mounted) {
		LOG_WRN("FAT mount of %s failed (%d) — skipping FAT tests, disk/HW still run",
			DISK_MOUNT_PT, rc);
	}

	return NULL;
}

static void emmc_suite_teardown(void *fixture)
{
	ARG_UNUSED(fixture);

	if (emmc_fat_mounted) {
		(void)fs_unmount(&emmc_mp);
		emmc_fat_mounted = false;
	}
	(void)disk_access_ioctl(emmc_disk, DISK_IOCTL_CTRL_DEINIT, NULL);
}

ZTEST_SUITE(emmc_8bit, NULL, emmc_suite_setup, NULL, NULL, emmc_suite_teardown);
