# Pairing 2 — SPI1 master + LPSPI slave (160 vs 240)

**Board:** `alif_b1_dk` / `ab1c1f4m51820ph0` / `rtss_he`  
**SW:** Zephyr `ba68cc2b03ab`, DesignWare SPI + PL330 DMA (no `CONFIG_NO_OPTIMIZATIONS`)  
**Roles:** SPI1 master (HCLK 80/120), LPSPI0 slave (SSI = HE 160/240), full duplex TX+RX  
**Jumpers:** SCK, MOSI, MISO, SS (same DK)

**Quote logs:**  
`2026-09-08_b1_he_spi1_mst_lpspi_slv_normal160_10mhz.txt`  
`2026-09-08_b1_he_spi1_mst_lpspi_slv_turbo240_10mhz.txt`

Extra 10-iter loops: `..._normal160_loop_10_20mhz.txt` / `..._turbo240_loop_10_20_30mhz.txt`

---

## Clock / BAUDR

Master SCK = SSI / BAUDR. BAUDR is even (2 … 65534). Slave has no BAUDR.

| | Normal 160 | Turbo 240 |
|---|---|---|
| SPI1 master SSI (HCLK) | 80 MHz | 120 MHz |
| LPSPI slave SSI | 160 MHz | 240 MHz |
| 10 MHz BAUDR | 8 | 12 |
| actual SCK @ 10 MHz | **10.00 MHz** both | **10.00 MHz** both |
| LPSPI TX+RX TRM 12× max | **13.3 MHz** | **20 MHz** |

Fair sweep (same actual SCK on HCLK 80 and 120): **5, 10, 20 MHz**.  
8 MHz is **not** fair (80→8.00, 120→8.57).

Even-BAUDR trap (same class as OSPI): 80 MHz from 240 is impossible (`240/80 = 3` odd → HW 2 → 120 MHz). Not used as a product SCK here.

---

## Bench — 10 × 800 B @ 10 MHz (same SCK)

`k_cycle_get_32()` around master `spi_transceive()` only. 1 warmup dropped.

| | Normal 160 | Turbo 240 | Turbo vs normal |
|---|---|---|---|
| Master SSI / BAUDR | 80 / 8 | 120 / 12 | actual **10.00 MHz** |
| Slave SSI / 12× max | 160 / 13.3 MHz | 240 / 20 MHz | |
| CPU cycles | 1 101 562 | 1 617 839 | ~1.47× |
| **Wall time** | **6884 µs** | **6740 µs** | **−2.1%** |
| Wire (ideal) | 6400 µs | 6400 µs | same |
| Overhead | 484 µs | 340 µs | ~1.5× |
| Payload (one pin) | **9296 kbit/s** | **9495 kbit/s** | **+2.1%** |
| Full-duplex (2 pins) | 18 592 kbit/s | 18 990 kbit/s | +2.1% |
| Result | **PASS** | **PASS** | |

Same baud is **not** 1.5× Mbit/s.

---

## One-shot sweep (fair rates)

| SCK | Normal 160 | Turbo 240 | Both-dir data |
|---|---|---|---|
| 5 MHz | 1347 µs (wire 1280) | 1325 µs | **PASS** |
| 10 MHz | 707 µs (wire 640) | 685 µs | **PASS** |
| 20 MHz | 385 µs (wire 320) | 364 µs | **160 FAIL MISO** `5A→B4`; **240 PASS** |

| | Normal | Turbo |
|---|---|---|
| **Max PASS (fair sweep)** | **10 MHz** | **20 MHz** |
| 20 MHz vs LPSPI SSI | 160/20 = **8×** (< 12×) | 240/20 = **12×** (TRM min) |

20 MHz @ 160 still finishes. Data is a **1-bit left shift** (`5A→B4`). MOSI (master TX) still matches. Loop log: 10/10 iters same FAIL.

`-116` (`-ETIMEDOUT`) after turbo sweep `SPI tests completed` = slave still armed. **Not** a sweep fail.

---

## Extra — 30 MHz turbo (loop, not fair vs 160)

10-iter full `memcmp` at `freq 30000000Hz (div = 4)`, HCLK 120.

| Request | Actual SCK | LPSPI ratio | Result |
|---|---|---|---|
| 30 MHz | 30.00 MHz | 240/30 = **8×** | **PASS** both dirs, 10 loops |
| 10 / 20 MHz turbo | 10 / 20 MHz | 24× / 12× | PASS (loop agrees with sweep) |

30 MHz is the TRM **RX-only** 8× line, not TX+RX 12×. Quote as **observed on this DK**, not a spec claim.

Do **not** put 30 MHz in the 160-vs-240 table: SPI1 @ 80 MHz cannot make even-BAUDR 30 MHz (`80/30` is not an even integer).

---

## Lockup — 30 / 60 MHz LPSPI slave (bus / DMA stall)

Turbo HE = **240 MHz**. This sample is full duplex, so TRM is **TX+RX 12×**, not the RX-only 8× line.

| SCK | LPSPI ratio | vs TRM TX+RX 12× | What was seen |
|---|---|---|---|
| 20 MHz | 12× | min | **PASS** (fair sweep + loops) |
| 30 MHz | **8×** (RX-only line) | below 12× | 10-iter loop **PASS**; DMA 800 B can still **lock** |
| 60 MHz | **4×** | far below | **LOCKUP** (AHB/DMA stall). Expected even on a clean SS pin |

60 MHz into an LPSPI slave is enough to lock AHB/DMA. Debugger: `0xEFFFFFFE` / cannot halt M55. That is a **bus/DMA stall**, not a `memcmp` FAIL and not the LPUART P2_0 RX ISR storm (different mux: SS is `LPSPI_SS_A`, UART storm needs `LPUART_RX_A` + RX IRQ).

30 MHz is on the edge: small loops can PASS and a long DMA xfer can still hang. Do not quote 30/60 MHz as supported TX+RX. Spec ceiling stays **20 MHz** (12×).

P2_0 is the LPSPI SS pad and is analog-loaded (ANA_S8). That can make high-SCK CS worse. It is **not** required to explain 60 MHz lockup.

---

## What turbo does here

| Item | Result |
|---|---|
| Same 10 MHz throughput | **+2%** |
| CPU / HCLK / DMA | **1.5×** overhead only |
| **Max passing SCK (fair)** | **10 → 20 MHz** (real SCLK win) |
| 30 MHz | Loop PASS @ 8×; DMA 800 B can still lock. TRM TX+RX stays **20 MHz** |
| 60 MHz | **LOCKUP** (4×). Bus/DMA stall, not a data FAIL |

---

## Build

```bash
# Turbo
rm -rf build/; west build -p always -b alif_b1_dk/ab1c1f4m51820ph0/rtss_he \
  -S alif-dk -S spark-turbo ../alif/samples/drivers/spi_dw/ \
  -DCONFIG_FLASH_BASE_ADDRESS=0 -DCONFIG_FLASH_LOAD_OFFSET=0 -DCONFIG_FLASH_SIZE=256

# Normal: omit -S spark-turbo
```

Overlay: SPI1 = `master-spi`, LPSPI = `slave-spi` + `serial-target`.
