/* Copyright (C) 2026 Alif Semiconductor - All Rights Reserved.
 * Use, distribution and modification of this code is permitted under the
 * terms stated in the Alif Semiconductor Software License Agreement
 *
 * You should have received a copy of the Alif Semiconductor Software
 * License Agreement with this file. If not, please write to:
 * contact@alifsemi.com, or visit: https://alifsemi.com/license
 */

#include <string.h>
#include <zephyr/fs/fs.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/ztest.h>

#include "emmc_test.h"

LOG_MODULE_DECLARE(emmc_8bit, LOG_LEVEL_INF);

static void fill_pattern(uint8_t *buf, size_t len, uint8_t seed)
{
	for (size_t i = 0; i < len; i++) {
		buf[i] = (uint8_t)(seed + i);
	}
}

ZTEST(emmc_8bit, test_fat_listdir_root)
{
	struct fs_dir_t dir;

	if (!emmc_fat_mounted) {
		ztest_test_skip();
	}
	struct fs_dirent ent;
	int rc;
	int count = 0;

	fs_dir_t_init(&dir);
	rc = fs_opendir(&dir, DISK_MOUNT_PT);
	zassert_equal(rc, 0, "opendir %s failed (%d)", DISK_MOUNT_PT, rc);

	for (;;) {
		rc = fs_readdir(&dir, &ent);
		if (rc || ent.name[0] == 0) {
			break;
		}
		LOG_INF("root %s %s size=%zu",
			ent.type == FS_DIR_ENTRY_DIR ? "DIR " : "FILE", ent.name, ent.size);
		count++;
	}

	zassert_equal(rc, 0, "readdir failed (%d)", rc);
	fs_closedir(&dir);
	zassert_true(count >= 0, "listdir failed");
}

ZTEST(emmc_8bit, test_fat_mkdir_file_rw_unlink)
{
	uint8_t *wbuf = emmc_io_buf;

	if (!emmc_fat_mounted) {
		ztest_test_skip();
	}
	uint8_t *rbuf = emmc_io_buf + EMMC_FILE_BYTES;
	struct fs_file_t file;
	struct fs_dirent st;
	ssize_t n;
	int rc;

	BUILD_ASSERT(EMMC_FILE_BYTES * 2 <= EMMC_IO_BYTES);

	fill_pattern(wbuf, EMMC_FILE_BYTES, 0xa5);

	if (fs_stat(EMMC_TEST_FILE, &st) == 0) {
		zassert_equal(fs_unlink(EMMC_TEST_FILE), 0, "unlink leftover %s failed",
			      EMMC_TEST_FILE);
	}

	/* Root file first — same write path as fs_sample some.dat. */
	fs_file_t_init(&file);
	rc = fs_open(&file, EMMC_TEST_FILE, FS_O_CREATE | FS_O_RDWR);
	zassert_equal(rc, 0, "open/create %s failed (%d)", EMMC_TEST_FILE, rc);

	// #region agent log
	LOG_INF("dbgff42d9 A/D before fs_write wbuf=%p n=%u b0=0x%02x",
		wbuf, EMMC_FILE_BYTES, wbuf[0]);
	// #endregion
	n = fs_write(&file, wbuf, EMMC_FILE_BYTES);
	zassert_equal(n, EMMC_FILE_BYTES, "write %s got %zd", EMMC_TEST_FILE, n);
	zassert_equal(fs_sync(&file), 0, "fs_sync failed");
	zassert_equal(fs_close(&file), 0, "fs_close after write failed");

	zassert_equal(fs_stat(EMMC_TEST_FILE, &st), 0, "stat %s failed", EMMC_TEST_FILE);
	zassert_equal(st.type, FS_DIR_ENTRY_FILE, "stat type not file");
	zassert_equal(st.size, EMMC_FILE_BYTES, "size %zu want %u", st.size, EMMC_FILE_BYTES);

	fs_file_t_init(&file);
	rc = fs_open(&file, EMMC_TEST_FILE, FS_O_READ);
	zassert_equal(rc, 0, "reopen %s failed (%d)", EMMC_TEST_FILE, rc);
	memset(rbuf, 0, EMMC_FILE_BYTES);
	n = fs_read(&file, rbuf, EMMC_FILE_BYTES);
	zassert_equal(n, EMMC_FILE_BYTES, "read %s got %zd", EMMC_TEST_FILE, n);
	zassert_equal(fs_close(&file), 0, "fs_close after read failed");
	zassert_mem_equal(wbuf, rbuf, EMMC_FILE_BYTES, "file data mismatch (8-bit data path)");

	/* Rewrite with a different pattern and verify. */
	fill_pattern(wbuf, EMMC_FILE_BYTES, 0x5a);
	fs_file_t_init(&file);
	rc = fs_open(&file, EMMC_TEST_FILE, FS_O_RDWR);
	zassert_equal(rc, 0, "reopen for rewrite failed");
	zassert_equal(fs_seek(&file, 0, FS_SEEK_SET), 0, "seek failed");
	n = fs_write(&file, wbuf, EMMC_FILE_BYTES);
	zassert_equal(n, EMMC_FILE_BYTES, "rewrite got %zd", n);
	zassert_equal(fs_close(&file), 0, "close after rewrite failed");

	fs_file_t_init(&file);
	zassert_equal(fs_open(&file, EMMC_TEST_FILE, FS_O_READ), 0, "open after rewrite failed");
	memset(rbuf, 0, EMMC_FILE_BYTES);
	n = fs_read(&file, rbuf, EMMC_FILE_BYTES);
	zassert_equal(n, EMMC_FILE_BYTES, "reread got %zd", n);
	zassert_equal(fs_close(&file), 0, "final close failed");
	zassert_mem_equal(wbuf, rbuf, EMMC_FILE_BYTES, "rewrite data mismatch");

	zassert_equal(fs_unlink(EMMC_TEST_FILE), 0, "unlink file failed");

	rc = fs_mkdir(EMMC_TEST_DIR);
	zassert_true(rc == 0 || rc == -EEXIST, "mkdir %s failed (%d)", EMMC_TEST_DIR, rc);
	if (fs_stat(EMMC_TEST_DIR, &st) == 0) {
		zassert_equal(fs_unlink(EMMC_TEST_DIR), 0, "unlink dir failed");
	}
}

ZTEST(emmc_8bit, test_fat_unmount_remount)
{
	int rc;

	if (!emmc_fat_mounted) {
		ztest_test_skip();
	}

	rc = fs_unmount(&emmc_mp);
	zassert_equal(rc, 0, "unmount failed (%d)", rc);

	rc = fs_mount(&emmc_mp);
	zassert_equal(rc, 0, "remount failed (%d)", rc);
}
