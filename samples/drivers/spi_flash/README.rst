
.. _spi-flash-test:

SPI-Flash Test
###############

Overview
********

This is the test application to test and verify the Flash Read,Write and Erase opearations over OSPI interface.
The OSPI driver placed under *modules/hal/alif/drivers*.

As of current h/w support 16bit Data Frame Size been applied for R/W.

Flash driver will be chosen based on configuration in ospi-flash node.

Currently supported Flash devices are :
       1. ISSI IS25WX Flash
       2. Macronix MX66UW Flash


Building and Running
********************

Functional tests, then a timed throughput bench and an even-BAUDR SCK sweep.
``k_cycle_get_32()`` wraps only ``flash_erase`` / ``flash_write`` / ``flash_read``
(and XiP ``memcpy`` when ``CONFIG_ALIF_OSPI_FLASH_XIP=y``).

Erase and program time are flash ``tSE`` / ``tPP`` (not SCK). Read and XiP
are the numbers that can move with turbo. TRM BAUDR is even, so 80 MHz SCK
from 240 MHz SSI is skipped (``240/80=3`` becomes 2 → 120 MHz). Turbo lists
**120 MHz** (``240/2``, BAUDR=2, baud2-delay on) as its own step; hang or FAIL
is a valid result. Probe OSPI0 SCLK on P4_2. The sweep waits 3 s before that
step so the logic analyzer can be armed.

Fair same-SCK compare is **40 MHz** (``160/4`` and ``240/6``). Turbo-only
tries are **60 MHz** (``240/4``) and **120 MHz** (``240/2``). Normal-only
try is **80 MHz** (``160/2``).

Full-chip erase (Test 2) is off by default (``RUN_FULL_CHIP_ERASE``).

Example command to build:

.. code-block:: console

   west build -p always -b alif_b1_dk/ab1c1f4m51820ph0/rtss_he \
     -S ospi-flash ../alif/samples/drivers/spi_flash/

   west build -p always -b alif_b1_dk/ab1c1f4m51820ph0/rtss_he \
     -S ospi-flash -S spark-turbo ../alif/samples/drivers/spi_flash/

Sample Output
=============

.. code-block:: console

	ospi1@83002000 OSPI flash testing
	========================================

	Test 1: Flash erase
	Flash erase succeeded!

	Test 1: Flash write
	Attempting to write 4 bytes

	Test 1: Flash read
	Data read matches data written. Good!!

	Test 2: Flash Full Erase
	Successfully Erased whole Flash Memory
	Total errors after reading erased chip = 0

	Test 3: Flash erase
	Flash erase succeeded!

	Test 3: Flash write
	Attempting to write 1024 bytes

	Test 3: Flash read
	Data read matches data written. Good!!

	Test 4: write sector 16384
	Test 4: write sector 20480

	Sec4: Read and Verify written data

	Test 4: read sector 16384

	Data read matches data written. Good!!
	Sec5: Read and Verify written data

	Test 4: read sector 20480
	Data read matches data written. Good!!

	Test 4: Erase Sector 4 and 5
	Flash Erase from Sector 16384 Size to Erase 8192

	Multi-Sector erase succeeded!

	Test 4: read sector 16384
	Total errors after reading erased Sector 4 = 0

	Test 4: read sector 20480
	Total errors after reading erased Sector 5 = 0

	Multi-Sector Erase Test Succeeded !

