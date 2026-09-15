OSPI flash logs / report index
======================================

OSPI log index — Balletto B1 HE  (alif_b1_dk / rtss_he)
Flash: ISSI IS25WX 64 MB, OSPI0
Zephyr e4aeaddf268f   sample: ../alif/samples/drivers/spi_flash/
No CONFIG_NO_OPTIMIZATIONS.

Pins: P4_2 SCLK, P4_1 RXDS, P2_1 SS1 (not P2_0), P3_0-7 data.

Turbo (ACLK 240, overlay clock-frequency=240e6, bus-speed=40e6):
west build -p always -b alif_b1_dk/ab1c1f4m51820ph0/rtss_he \
  -S ospi-flash -S spark-turbo ../alif/samples/drivers/spi_flash/

Normal 160 (DTS default 160/80 unless bus-speed overlay 40e6):
west build -p always -b alif_b1_dk/ab1c1f4m51820ph0/rtss_he \
  -S ospi-flash ../alif/samples/drivers/spi_flash/

Driver does not call clock_control_get_rate(). Overlay MUST set
clock-frequency=240e6 in turbo or actual SCK becomes 120 MHz.

Log validation (pasted consoles vs existing report)
------------------------------------------------
All turbo boots print Zephyr e4aeaddf268f. Numbers match the report.

SET A — quote (fair 40 MHz turbo)
  Tests 1-5 PASS. DTS BAUDR=6 actual 40.00 MHz.
  flash_read 1271691 cyc, 5298 us, 74219 kbit/s (74.2 Mbit/s).
  Size matrix 43 PASS / 0 FAIL.
  Same cycle count at 20/40/60/120 16 KB read (software-bound).

SET B — extra 16 KB sweep (do not quote as max SCK)
  Firmware prints "Max SCK read/write PASS: 60 MHz" or "120 MHz"
  after a 16 KB increment pattern. That is NOT the size matrix.
  60 MHz 16 KB PASS, then matrix 21/22 FAIL (err=length/4).
  120 MHz tap 11: 16 KB MISMATCH err=60.
  120 MHz RXDS window taps 6-7; tap 6 16 KB can PASS; matrix 6/3;
  skip >=1024 B (hang risk). SKIP 80 (would be actual 120).

SET C — normal 160
  No console in this folder yet. Report numbers (49.5 Mbit/s, 80 MHz 19/24)
  stay from the earlier 160 matrix. Drop ..._normal160_*.txt here when you have it.

File                                                            What
----                                                            ----
QUOTE — turbo 40 MHz
2026-09-15_b1_he_ospi_turbo240_40mhz_sweep60.txt                Tests 1-5 PASS, bench 5298 us / 74219 kbit/s
                                                                sweep 20/40 PASS; 60 16 KB PASS; SKIP 80
                                                                Max SCK line 60 MHz = 16 KB only

EXTRA — turbo 120 MHz 16 KB (tap 11)
2026-09-15_b1_he_ospi_turbo240_120mhz_16kb_fail.txt             sweep 20/40/60 PASS; 120 16 KB FAIL err=60
                                                                Max SCK line 60 MHz

QUOTE — turbo size matrix (payload limiter)
2026-09-15_b1_he_ospi_turbo240_matrix_40_60_120.txt             40 MHz 43/43 PASS
                                                                60 MHz 21/22 FAIL (55->54, err=length/4)
                                                                120 tap 6-7; 16 KB can PASS; matrix 6/3 of 14
                                                                skip >=1024 hang risk
                                                                60/120 FAIL cases kept; 40 MHz PASS iters collapsed

Reports
-------
  /ospi_40mhz_fair.md
  /ospi_60_80_120mhz.md
  /ospi_spark_turbo_vs_normal_report.md
  /JIRA_ZRTSS-854_OSPI.md
