# Pairing 1 — LPSPI master + SPI1 slave (160 vs 240)

**Board:** `alif_b1_dk` / `ab1c1f4m51820ph0` / `rtss_he`  
**SW:** Zephyr `ba68cc2b03ab`, DesignWare SPI + PL330 DMA 
**Roles:** LPSPI0 master (SSI = HE 160/240), SPI1 slave (HCLK 80/120)  
**Jumpers:** SCK, MOSI, MISO, SS (same DK)

**Quote logs:**  
`2026-09-08_b1_he_lpspi_mst_spi1_slv_normal160_10mhz.txt`  
`2026-09-08_b1_he_lpspi_mst_spi1_slv_turbo240_10mhz.txt`

Extra 16 MHz: `..._normal160_16mhz.txt` / `..._turbo240_16mhz.txt`

---

## Clock / BAUDR

Master SCK = SSI / BAUDR. BAUDR is even (2 … 65534).

| | Normal 160 | Turbo 240 |
|---|---|---|
| LPSPI SSI | 160 MHz | 240 MHz |
| SPI1 slave SSI | 80 MHz | 120 MHz |
| 10 MHz BAUDR | 16 | 24 |
| actual SCK @ 10 MHz | **10.00 MHz** both | **10.00 MHz** both |
| TRM SPI1 slave 4× cap | 20 MHz | 30 MHz |

Fair sweep (same actual SCK on 160 and 240): **5, 8, 10, 20 MHz**.

---

## Bench — 10 × 800 B @ 10 MHz (same SCK)

`k_cycle_get_32()` around master `spi_transceive()` only. 1 warmup dropped.

| | Normal 160 | Turbo 240 | Turbo vs normal |
|---|---|---|---|
| Master SSI / BAUDR | 160 / 16 | 240 / 24 | actual **10.00 MHz** |
| CPU cycles | 1 096 050 | 1 609 410 | 1.47× |
| **Wall time** | **6850 µs** | **6705 µs** | **−2.1%** |
| Wire (ideal) | 6400 µs | 6400 µs | same |
| Overhead | 450 µs | 305 µs | ~1.5× CPU/DMA |
| Payload (one pin) | **9343 kbit/s** | **9545 kbit/s** | **+2.2%** |
| Full-duplex (2 pins) | 18 686 kbit/s | 19 090 kbit/s | +2.2% |
| Result | **PASS** | **PASS** | |

Same baud is **not** 1.5× Mbit/s. Wire-bound at 10 MHz.

---

## One-shot sweep (fair rates)

| SCK | Normal 160 | Turbo 240 | Both-dir data |
|---|---|---|---|
| 5 MHz | 1328 µs (wire 1280) | 1312 µs | **PASS** |
| 8 MHz | 848 µs (wire 800) | 832 µs | **PASS** |
| 10 MHz | 688 µs (wire 640) | 672 µs | **PASS** |
| 20 MHz | 365 µs (wire 320) | 351 µs | **FAIL MISO** (`5A → 7F`) |

| | Normal | Turbo |
|---|---|---|
| **Max PASS (fair sweep)** | **10 MHz** | **10 MHz** |
| 20 MHz vs SPI1 SSI | 80/20 = 4× (spec edge) | 120/20 = 6× |

20 MHz still clocks (time ≈ wire + ~40 µs). MISO fails the same way both modes → **pad/jumper/sample**, not turbo HCLK.

---

## Extra — 16 MHz requested (not in the fair 5/8/10/20 list)

Firmware print is slightly different (no SSI/BAUDR header). 20 MHz still `5A→7F` so this is the same pairing.

| | Normal 160 | Turbo 240 |
|---|---|---|
| Bench 10×800 @ 16 MHz req | 5344 µs, **11976 kbit/s** PASS | 4641 µs, **13790 kbit/s** PASS |
| Sweep max PASS | **16 MHz** | **16 MHz** |
| 20 / 22 / 24 / 30 MHz | FAIL MISO | FAIL MISO |

Do **not** replace the fair-table max (10 MHz) with 16 MHz in the 160-vs-240 column: at 240 MHz, 16 MHz is not an even-BAUDR exact rate (`240/16 = 15`). At 160 MHz it is exact (`160/16 = 10`).

---

## What turbo does here

| Item | Result |
|---|---|
| Same 10 MHz throughput | **+2%** |
| CPU / HCLK / DMA | **1.5×** (overhead only) |
| Max passing SCK (fair) | **None** — 10 MHz both |
| 20 MHz | FAIL both |

---

## Build

```bash
# Turbo
rm -rf build/; west build -p always -b alif_b1_dk/ab1c1f4m51820ph0/rtss_he \
  -S alif-dk -S spark-turbo ../alif/samples/drivers/spi_dw/ \
  -DCONFIG_FLASH_BASE_ADDRESS=0 -DCONFIG_FLASH_LOAD_OFFSET=0 -DCONFIG_FLASH_SIZE=256

# Normal: omit -S spark-turbo
```

Overlay: LPSPI = `master-spi`, SPI1 = `slave-spi`.
