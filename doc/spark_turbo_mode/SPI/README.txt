SPI console logs (plain text)
======================================

SPI log index — Balletto B1 HE  (alif_b1_dk / rtss_he)
Zephyr ba68cc2b03ab   sample: ../alif/samples/drivers/spi_dw/


Turbo (ITCM, -S alif-dk -S spark-turbo):
rm -rf build/; west build -p always -b alif_b1_dk/ab1c1f4m51820ph0/rtss_he \
  -S alif-dk -S spark-turbo ../alif/samples/drivers/spi_dw/ \
  -DCONFIG_FLASH_BASE_ADDRESS=0 -DCONFIG_FLASH_LOAD_OFFSET=0 -DCONFIG_FLASH_SIZE=256

Normal 160 (ITCM, omit -S spark-turbo):
rm -rf build/; west build -p always -b alif_b1_dk/ab1c1f4m51820ph0/rtss_he \
  -S alif-dk ../alif/samples/drivers/spi_dw/ \
  -DCONFIG_FLASH_BASE_ADDRESS=0 -DCONFIG_FLASH_LOAD_OFFSET=0 -DCONFIG_FLASH_SIZE=256

Jumpers: SCK, MOSI, MISO, SS (LPSPI0 <-> SPI1). Overlay aliases pick the pairing.

Log validation (pasted consoles vs existing report)
-----------------------------------------------
All boots print Zephyr ba68cc2b03ab.

SET A — quote (use for pairing reports)
  Prints "SPI mode:", SSI/BAUDR, 10 x 800 bench, fair sweep, "Max SCLK".

SET B — extra 16 MHz (pairing 1 only)
  Same hash, slightly older printk (no "LPSPI SSI (get_rate)=" header).
  Sweep list 10/15/16/20/22/24/30. Max PASS 16 MHz both modes; 20 MHz still 5A->7F.
  Keep as extra. Do not replace the fair 5/8/10/20 table.

SET C — 10-iter SUCCESS/FAIL loops (pairing 2)
  Same hash, older printk ("Slave Transceive Iter=", no kbit/s).
  Confirms: turbo 10/20/30 PASS both dirs; normal 10 PASS, 20 FAIL 5A->B4.
  Bench+sweep (SET A) remains the quote for throughput.
  60 MHz has no console (AHB/DMA LOCKUP). See pairing 2 report.

File                                                            What
----                                                            ----
QUOTE — LPSPI master + SPI1 slave
2026-09-08_b1_he_lpspi_mst_spi1_slv_normal160_10mhz.txt         HE 160, 10 MHz bench 6850 us / 9343 kbit/s
                                                                sweep PASS 5/8/10, FAIL 20 (5A->7F), max 10 MHz
2026-09-08_b1_he_lpspi_mst_spi1_slv_turbo240_10mhz.txt          HE 240, 10 MHz bench 6705 us / 9545 kbit/s
                                                                same sweep, max 10 MHz

EXTRA — LPSPI master + SPI1 slave (16 MHz firmware)
2026-09-08_b1_he_lpspi_mst_spi1_slv_normal160_16mhz.txt         16 MHz bench 5344 us / 11976 kbit/s, max PASS 16
                                                                20+ still FAIL (5A->7F); not fair at 240 (240/16 odd)
2026-09-08_b1_he_lpspi_mst_spi1_slv_turbo240_16mhz.txt          16 MHz bench 4641 us / 13790 kbit/s, max PASS 16
                                                                20+ still FAIL (5A->7F)
                                                                (20 MHz inconsistent, sometimes working,
                                                                 as per spec 30MHz also should work in turbo mode but
                                                                 it is not working due to long jumper cables?)

QUOTE — SPI1 master + LPSPI slave
2026-09-08_b1_he_spi1_mst_lpspi_slv_normal160_10mhz.txt         HCLK 80, 10 MHz bench 6884 us / 9296 kbit/s
                                                                sweep PASS 5/10, FAIL 20 (5A->B4), max 10 MHz
2026-09-08_b1_he_spi1_mst_lpspi_slv_turbo240_10mhz.txt          HCLK 120, 10 MHz bench 6740 us / 9495 kbit/s
                                                                sweep PASS 5/10/20, max 20 MHz;

EXTRA — SPI1 master + LPSPI slave (10-iter loops)
2026-09-08_b1_he_spi1_mst_lpspi_slv_normal160_loop_10_20mhz.txt   10 MHz PASS; 20 MHz MISO FAIL 5A->B4 (MOSI OK)
2026-09-08_b1_he_spi1_mst_lpspi_slv_turbo240_loop_10_20_30mhz.txt 10/20/30 MHz PASS both dirs (30 MHz = 8x, lab only)

No 60 MHz console: AHB/DMA LOCKUP (bus stall). See spi1_master_lpspi_slave.md.

Reports
-------
  /lpspi_master_spi1_slave.md     pairing 1 (normal + turbo)
  /spi1_master_lpspi_slave.md     pairing 2 (normal + turbo)
  /spi_spark_turbo_vs_normal_report.md   clocks + side-by-side
  /JIRA_ZRTSS-854_SPI.md
