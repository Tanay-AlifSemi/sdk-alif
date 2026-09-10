# ZRTSS-854 — Spark turbo SPI (Balletto B1 HE)

**Summary (title):** Spark turbo 160→240: same 10 MHz SPI ~+2% throughput. LPSPI master + SPI1 slave max PASS 10/10 MHz. SPI1 master + LPSPI slave max PASS 10/20 MHz (TRM 12×). Even BAUDR: SCK = SSI / BAUDR.

**Board:** `alif_b1_dk` / `ab1c1f4m51820ph0` / `rtss_he`  
**SW:** Zephyr `ba68cc2b03ab`, DesignWare SPI + PL330 DMA  
**Sample:** `../alif/samples/drivers/spi_dw/`  Turbo: `-S alif-dk -S spark-turbo`. No `CONFIG_NO_OPTIMIZATIONS`.  
**Evidence:** `alif/doc/spark_turbo_mode/SPI/`  
**Status:** both pairings complete on DK jumpers (SCK, MOSI, MISO, SS).

Full tables: `lpspi_master_spi1_slave.md`, `spi1_master_lpspi_slave.md`. Combined: `spi_spark_turbo_vs_normal_report.md`.

---

## 1. Clock change

HFOSC / HFXO stays **38.4 MHz**. Spark-turbo does not retune it. SPI SSI is PLL (HE for LPSPI, HCLK for SPI1).

| Clock | Normal 160 | Turbo 240 | SPI use |
|---|---|---|---|
| HFOSC | 38.4 MHz | 38.4 MHz | Not SPI SSI |
| HE / ACLK | 160 MHz | 240 MHz | **LPSPI SSI** |
| **HCLK** | **80 MHz** | **120 MHz** | **SPI1 SSI**, DMA |
| PCLK | 40 MHz | 60 MHz | — |

Master SCK = SSI / BAUDR. BAUDR is even, 2 to 65534 (LSB hard-wired 0). Software writes floor(SSI / f_req); hardware keeps it even.

Fair same-SCK only if actual SCK matches on 160 and 240.

| Pairing | Fair sweep |
|---|---|
| LPSPI master + SPI1 slave | 5, 8, 10, 20 MHz |
| SPI1 master + LPSPI slave | 5, 10, 20 MHz (8 MHz is not fair: 80→8.00, 120→8.57) |

Even-BAUDR trap (same class as OSPI): 80 MHz from 240 is impossible. 240/80 = 3 (odd) → HW 2 → actual **120 MHz**. Not used as a product SCK here.

Slave has no BAUDR. TRM:

| Slave | Ratio | Max SCK 160 / 240 |
|---|---|---|
| SPI1 (HS) | 4× | 20 / 30 MHz |
| LPSPI TX+RX (this sample) | 12× | 13.3 / 20 MHz |
| LPSPI RX-only | 8× | 20 / 30 MHz |

---

## 2. Throughput (quote bench @ 10 MHz, same actual SCK)

`k_cycle_get_32()` around master `spi_transceive()` only. 10 × 800 B, 1 warmup dropped. Payload = 8000 bytes × 8 / measured_us.

Same SCK is **not** 1.5× Mbit/s. Both pairings are wire-bound at 10 MHz. Turbo only cuts DMA/CPU overhead.

**LPSPI master + SPI1 slave** (BAUDR 16 vs 24)

| | 160 | 240 |
|---|---|---|
| Wall time | **6850 µs** | **6705 µs** |
| Payload | **9343 kbit/s** | **9545 kbit/s** (+2.2%) |
| Result | PASS | PASS |

**SPI1 master + LPSPI slave** (BAUDR 8 vs 12)

| | 160 | 240 |
|---|---|---|
| Wall time | **6884 µs** | **6740 µs** |
| Payload | **9296 kbit/s** | **9495 kbit/s** (+2.1%) |
| Result | PASS | PASS |

Quote **actual SCK**, not the `spi_config` name.

---

## 3. Max SCK (both directions PASS)

**LPSPI master + SPI1 slave**

| Mode | Max PASS | 20 MHz |
|---|---|---|
| Normal 160 | **10 MHz** | FAIL MISO `5A → 7F` (still clocks, 365 µs) |
| Turbo 240 | **10 MHz** | FAIL MISO `5A → 7F` (351 µs) |

Turbo does **not** raise passing SCLK here. Same fail both clocks → pad/jumper/sample, not turbo HCLK.

Extra (not fair at 240): 16 MHz requested PASSes both modes; 20+ still FAIL. Do not replace the fair max (10 MHz) with 16 MHz.

**SPI1 master + LPSPI slave**

| Mode | Max PASS | 20 MHz |
|---|---|---|
| Normal 160 | **10 MHz** | FAIL MISO `5A → B4` (1-bit shift). MOSI still PASS. LPSPI 160/20 = 8× (< 12×) |
| Turbo 240 | **20 MHz** | PASS. LPSPI 240/20 = 12× (TRM min) |

This is the real SCLK win: **10 → 20 MHz**.

30 MHz turbo: 10-iter loop PASS (div=4, 240/30 = 8×, RX-only line). DMA 800 B can still lock. Characterization only — TRM TX+RX stays **20 MHz**. Do not put 30 MHz in the 160-vs-240 table (SPI1 @ 80 MHz cannot make even-BAUDR 30 MHz).

60 MHz turbo (4×): **LOCKUP** (AHB/DMA stall, `0xEFFFFFFE`, cannot halt). Expected below 12×. Not a `memcmp` FAIL. Not the LPUART P2_0 RX ISR storm (SPI mux is `LPSPI_SS_A` on that pad).

`-116` after turbo sweep `SPI tests completed` = slave still armed. Not a sweep fail.  
`Clock enable not supported` = `clock_control_on()` no-op; `get_rate()` is valid.

---

## 4. One-line conclusions

1. CPU / HCLK / DMA **1.5×**. Same 10 MHz SPI **~+2%** throughput both pairings.
2. LPSPI master + SPI1 slave: max both-dir PASS **10 MHz** both modes. 20 MHz MISO fail on jumpers.
3. SPI1 master + LPSPI slave: max both-dir PASS **10 MHz (160)** and **20 MHz (240)** — matches LPSPI TX+RX 12×.
4. 20 MHz @ 160 still clocks; MISO shifts `5A→B4`. Do not call that supported.
5. Even BAUDR: SCK = SSI / even(BAUDR). Fair rates only. 80 MHz from 240 is not a real SCK.
6. LPSPI slave 60 MHz: **LOCKUP** (4×, bus/DMA). 30 MHz loop can PASS; DMA 800 B can hang. Spec TX+RX stays **20 MHz**.
