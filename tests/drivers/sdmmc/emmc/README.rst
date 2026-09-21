eMMC 8-bit Tests (DevKit-E8)
############################

Overview
********

This suite validates onboard eMMC in **8-bit** mode on Ensemble DevKit-E8.
It lives under ``alif/tests`` so ``samples/subsys/fs/fs_sample`` is not modified.

Checks:

* Devicetree ``bus-width = <8>`` on the host and ``mmc`` child
* Pinmux: D0–D3 / CMD / CLK pad ``0x23``; **P8_4–P8_7** ``SD_Dx_C`` pad ``0x41``
* Host CAP1 8-bit, ``HOST_CTRL1`` EXT 8-bit + HS, idle PSTATE DAT[7:4]
* Disk geometry and single / multi-block reads
* FAT mount, list, 4 KiB file write/read/rewrite (``E8TEST.DAT``), mkdir, unmount/remount

The test overlay stays **8-bit** but uses Linux's proven write point:
``no-1-8-v`` and ``max-bus-freq = <25000000>``. MMC HS clamps to that
frequency. For a 400 kHz write experiment use
``alif/samples/subsys/fs/fs_sample`` with ``-S alif-emmc-8bit``. HOST_CTRL1 HISPD follows the requested **timing** (MMC HS),
not ``freq >= 50 MHz`` — otherwise the card is in HS and the host uses
default output delay, which CRC-fails 8-bit CMD24 writes.

The FATFS object and the file I/O buffer are placed in
``CONFIG_SD_BUFFER_SECTION`` (``.alif_ns``), matching ``fs_sample``.
SDHC DMA writes from DTCM ``.bss`` also CRC-fail. Aligned FatFS writes DMA
directly from the caller buffer, so that buffer must live in ``.alif_ns``.

Destructive raw-sector writes are **not** used. File tests create and delete
``/SD2:/E8TEST.DAT`` only. ``CONFIG_FS_FATFS_MOUNT_MKFS`` is disabled so a
failed mount will not format the eMMC.

Supported Boards
****************

* ``alif_e8_dk/ae822fa0e5597xx0/rtss_hp``
* ``alif_e8_dk/ae822fa0e5597xx0/rtss_he``

Building and Running
********************

The board overlay enables 8-bit eMMC (same mapping as ``-S alif-emmc-8bit``).
Do not also pass ``-S alif-emmc``.

.. code-block:: console

   west build -p always \
     -b alif_e8_dk/ae822fa0e5597xx0/rtss_hp \
     alif/tests/drivers/sdmmc/emmc/ \
     -DCONFIG_FLASH_BASE_ADDRESS=0 \
     -DCONFIG_FLASH_LOAD_OFFSET=0 \
     -DCONFIG_FLASH_SIZE=256

Pad and flash as for ``fs_sample``. On success ztest prints ``PROJECT EXECUTION SUCCESSFUL``.

Test sources
************

* ``src/main.c`` — disk init, FAT mount, teardown
* ``src/test_emmc_hw.c`` — DT, pinmux, host registers, disk reads
* ``src/test_emmc_fs.c`` — FAT directory and file I/O
