.. _spi-dw-sample:

SPI-DW Sample
####################

Overview
********

This sample demonstrates using the Designware SPI driver.
This sample uses 2 SPI instances one as master and other slave.
By default it uses LPSPI as master and SPI0 as slave.

It runs two checks:

* Timed bench: 10 full-duplex transfers of ``BUFF_SIZE`` words at 10 MHz
  (exact SCK on both 160 MHz and 240 MHz LPSPI). One warmup xfer is discarded.
  CPU time is ``k_cycle_get_32()`` around ``spi_transceive()`` only.
* SCLK sweep: one transfer each at 5/8/10/20 MHz only — these are the rates
  whose actual SCK is the same in both modes (BAUDR even). Both MOSI and
  MISO must match.

.. note::

Use LPUART port for console logs on E1C and B1 DevKits.

Building and Running
********************

The application will build only for a target that has a devicetree entry with :dt compatible:`snps,designware-spi` as a compatible.

B1 Spark turbo (240 MHz core / 120 MHz HCLK)::

   west build -p always -b alif_b1_dk/ab1c1f4m51820ph0/rtss_he \
     -S alif-dk -S spark-turbo ../alif/samples/drivers/spi_dw/ \
     -DCONFIG_FLASH_BASE_ADDRESS=0 -DCONFIG_FLASH_LOAD_OFFSET=0 \
     -DCONFIG_FLASH_SIZE=256

B1 normal (160 MHz) omit ``-S spark-turbo``.

.. zephyr-app-commands::
   :zephyr-app: ../alif/samples/drivers/spi_dw
   :board: alif_e7_dk/ae722f80f55d5xx/rtss_he
   :goals: build
   :gen-args: -S alif-dk

Sample Output
=============

::

   SPI mode: SPARK TURBO (SYS_CLOCK_HW_CYCLES_PER_SEC=240000000)

   === SPI bench: 10 x 800 bytes @ 10000000 Hz ===
     req 10000000 Hz -> BAUDR=16  actual SCK=10000000 Hz   (or BAUDR=24 @ turbo)
   BENCH PASS

   === SPI SCLK sweep (1 xfer; rates identical on 160 and 240 MHz SSI) ===
      5 MHz : PASS
      8 MHz : PASS
     10 MHz : PASS
     20 MHz : FAIL   (MISO)
   Max SCLK both directions PASS: 10 MHz
   SPI tests completed
