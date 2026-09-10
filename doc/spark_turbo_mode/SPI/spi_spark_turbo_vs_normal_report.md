# Balletto B1 HE — SPI Spark turbo (240 MHz) vs normal (160 MHz)

**Board:** `alif_b1_dk` / `ab1c1f4m51820ph0` / `rtss_he`  
**Loopback:** LPSPI0 ↔ SPI1 on the same DK (jumper wires: SCK, MOSI, MISO, SS)  
**SW:** Zephyr `ba68cc2b03ab`, DesignWare SPI + PL330 DMA (no `CONFIG_NO_OPTIMIZATIONS`)  
**Turbo:** `-S alif-dk -S spark-turbo` (clocks 240/120/60)  
**Normal:** omit `-S spark-turbo` (160/80/40)

Pairing reports (full tables + extra 16/30 MHz):  
`lpspi_master_spi1_slave.md` · `spi1_master_lpspi_slave.md`  
JIRA : `JIRA_ZRTSS-854_SPI.md` · Logs: `README.txt`

Jumpers limit analog margin. TRM ratios are **guarantees**, not hard clamps.

---

## 1. What was tested

| Experiment | What it measures |
|---|---|
| **Bench** | 10 × 800 bytes full duplex at a **fixed 10 MHz SCK** (same actual SCK in both modes). `k_cycle_get_32()` around master `spi_transceive()` only (1 warmup dropped). |
| **One-shot sweep** | One transfer per rate. Only rates whose **actual SCK is identical** at 160 and 240 (even BAUDR). Both MOSI and MISO `memcmp` must match. |

**10 MHz** is the fair bench rate: it divides both master SSI clocks with even BAUDR.

Payload one-way kbit/s = `8000 bytes × 8 / measured_us`. Full-duplex figure is 2× (both pins at once).

---

## 2. Clock map (B1 HE)

| Clock | Normal | Turbo | Ratio |
|---|---|---|---|
| HE core / ACLK / SysTick | 160 MHz | 240 MHz | 1.50× |
| HCLK (SPI0/1, DMA) | 80 MHz | 120 MHz | 1.50× |
| PCLK | 40 MHz | 60 MHz | 1.50× |
| LPSPI SSI (`ALIF_LPSPI_CLK` = HE) | 160 MHz | 240 MHz | 1.50× |
| SPI1 SSI (`ALIF_SPI_CLK` = HCLK) | 80 MHz | 120 MHz | 1.50× |
| HFOSC | 38.4 MHz | 38.4 MHz | 1.00× |

---

## 3. TRM clock equations

### 3.1 Master (LPSPI or HS SPI)

SCK = SSI / BAUDR

- BAUDR even, **2 … 65534** (SCKDV × 2; LSB hard-wired 0).
- Software writes `floor(SSI / f_req)`; hardware keeps it even.
- Max master SCK: SSI ≥ 2 × SCK → SCK max = SSI / 2.

| Master IP | SSI normal / turbo | Max SCK |
|---|---|---|
| LPSPI | 160 / 240 MHz | **80 / 120 MHz** |
| SPI1 | 80 / 120 MHz | **40 / 60 MHz** |

Even-BAUDR trap: 80 MHz from 240 is impossible (`240/80 = 3` odd → HW 2 → **120 MHz**). Same class as OSPI.

### 3.2 Slave (no BAUDR — master’s SCK is the bit clock)

| Slave type | TRM ratio | Meaning |
|---|---|---|
| **HS SPI** (SPI0/1/2) | SSI ≥ 4 × SCK | Sync delay ~1 `SPI_CLK` |
| **LPSPI RX-only** | SSI ≥ 8 × SCK | Double-sync RX |
| **LPSPI TX+RX** (this sample) | SSI ≥ 12 × SCK | Drive MISO ≥ 3 `SPI_CLK` before master sample |

Also: first SCK edge ≥ **4 × SSI** after SS.

| Slave IP | SSI normal / turbo | TRM max SCK (this test = TX+RX) |
|---|---|---|
| SPI1 (HS) | 80 / 120 MHz | **20 / 30 MHz** (4×) |
| LPSPI (TX+RX) | 160 / 240 MHz | **13.3 / 20 MHz** (12×) |
| LPSPI (RX-only, not this test) | 160 / 240 MHz | 20 / 30 MHz (8×) |

BAUDR is programmed on the **master only**. Slave `get_rate()` is only for the ratio check.

---

## 4. Pairing 1 — LPSPI master + SPI1 slave

Full write-up: `lpspi_master_spi1_slave.md`  
Quote logs: `..._lpspi_mst_spi1_slv_*_10mhz.txt`

**Roles:** LPSPI0 master (SSI 160/240), SPI1 slave (HCLK 80/120).  
**Fair sweep:** 5, 8, 10, 20 MHz.  
**Limiter on paper:** SPI1 slave 4× → 20 / 30 MHz.

### 4.1 Bench — 10 × 800 B @ 10 MHz (same SCK)

| | Normal 160 | Turbo 240 | Turbo vs normal |
|---|---|---|---|
| Master SSI / BAUDR | 160 / 16 | 240 / 24 | actual SCK **10.00 MHz** both |
| CPU cycles | 1 096 050 | 1 609 410 | 1.47× (same wire wait) |
| **Wall time** | **6850 µs** | **6705 µs** | **−145 µs (−2.1%)** |
| Wire (ideal) | 6400 µs | 6400 µs | same |
| Overhead | 450 µs | 305 µs | ÷1.48 ≈ **1.5× CPU/DMA** |
| Payload (one pin) | **9343 kbit/s** | **9545 kbit/s** | **+2.2%** |
| Full-duplex (2 pins) | 18 686 kbit/s | 19 090 kbit/s | +2.2% |
| Result | **PASS** | **PASS** | |

### 4.2 One-shot sweep (same actual SCK)

| SCK | Normal 160 | Turbo 240 | Both-dir data |
|---|---|---|---|
| 5 MHz | 1328 µs (wire 1280) | 1312 µs | **PASS** |
| 8 MHz | 848 µs (wire 800) | 832 µs | **PASS** |
| 10 MHz | 688 µs (wire 640) | 672 µs | **PASS** |
| 20 MHz | 365 µs (wire 320) | 351 µs | **FAIL MISO both** (`5A → 7F`) |

| | Normal | Turbo |
|---|---|---|
| **Max SCLK both directions PASS (fair)** | **10 MHz** | **10 MHz** |
| TRM slave 4× cap | 20 MHz | 30 MHz |
| 20 MHz vs slave SSI | 80/20 = 4× (spec edge) | 120/20 = 6× |

20 MHz **did clock**. MISO failed the same way in both modes → **pad / jumper / sample**, not turbo HCLK.

### 4.3 Extra — 16 MHz (not fair at 240)

Same pairing, slightly different printk. Max PASS **16 MHz** both; 20+ still FAIL `5A→7F`. At 240 MHz, 16 MHz is not even-BAUDR exact (`240/16 = 15`). Do not replace the fair max (10 MHz).

At 240 MHz, a 16 MHz request is not 16.00 MHz on the wire:

SCK = SSI / even(BAUDR)
240 / 16 = 15 (odd) → HW 14 → actual ~17.14 MHz
At 160: 160 / 16 = 10 (even) → actual 16.00 MHz

---

## 5. Pairing 2 — SPI1 master + LPSPI slave

Full write-up: `spi1_master_lpspi_slave.md`  
Quote logs: `..._spi1_mst_lpspi_slv_*_10mhz.txt`

**Roles:** SPI1 master (HCLK 80/120), LPSPI0 slave (SSI 160/240).  
**Fair sweep:** 5, 10, 20 MHz. **8 MHz is not fair** (80→8.00, 120→8.57).  
**Limiter on paper:** LPSPI slave 12× → 13.3 / 20 MHz.

### 5.1 Bench — 10 × 800 B @ 10 MHz (same SCK)

| | Normal 160 | Turbo 240 | Turbo vs normal |
|---|---|---|---|
| Master SSI / BAUDR | 80 / 8 | 120 / 12 | actual SCK **10.00 MHz** both |
| Slave SSI / 12× max | 160 / **13.3 MHz** | 240 / **20 MHz** | |
| CPU cycles | 1 101 562 | 1 617 839 | ~1.47× |
| **Wall time** | **6884 µs** | **6740 µs** | **−144 µs (−2.1%)** |
| Wire (ideal) | 6400 µs | 6400 µs | same |
| Overhead | 484 µs | 340 µs | ~**1.5×** |
| Payload (one pin) | **9296 kbit/s** | **9495 kbit/s** | **+2.1%** |
| Full-duplex (2 pins) | 18 592 kbit/s | 18 990 kbit/s | +2.1% |
| Result | **PASS** | **PASS** | |

### 5.2 One-shot sweep (same actual SCK)

| SCK | Normal 160 | Turbo 240 | Both-dir data |
|---|---|---|---|
| 5 MHz | 1347 µs (wire 1280) | 1325 µs | **PASS** |
| 10 MHz | 707 µs (wire 640) | 685 µs | **PASS** |
| 20 MHz | 385 µs (wire 320) | 364 µs | **160 FAIL MISO** `5A→B4`; **240 PASS** |

| | Normal | Turbo |
|---|---|---|
| **Max SCLK both directions PASS (fair)** | **10 MHz** | **20 MHz** |
| 20 MHz vs LPSPI SSI | 160/20 = **8×** (< 12×) | 240/20 = **12×** (TRM min) |

20 MHz on 160 MHz **still finishes**. Data is a **1-bit left shift** (`5A→B4`). MOSI still matches. Loop log: 10/10 iters same FAIL.

`-116` after turbo `SPI tests completed` = slave still armed. **Not** a sweep fail.

### 5.3 Extra characterization (turbo only)

This sample is full duplex. TRM for LPSPI slave is **TX+RX 12×** (turbo max SCK **20 MHz**). 30/60 MHz are below that.

| SCK | Master BAUDR | LPSPI ratio | vs TRM 12× | Result |
|---|---|---|---|---|
| 20 MHz | 120/20 = 6 | **12×** | min | **PASS** (fair sweep) |
| 30 MHz | 120/30 = 4 | **8×** (RX-only line) | below | 10-iter loop **PASS**; DMA 800 B can still **lock** |
| 60 MHz | 120/60 = 2 | **4×** | far below | **LOCKUP** — AHB/DMA stall (`0xEFFFFFFE`, cannot halt M55) |

60 MHz into an LPSPI slave is enough to lock the bus even on a clean SS pin. 30 MHz is on the edge: small loops can PASS and a long DMA xfer can still hang. Do not quote 30/60 as supported TX+RX. Spec ceiling stays **20 MHz**.

This is **not** the LPUART P2_0 RX ISR storm (that mux is `LPUART_RX_A` + RX IRQ). SPI uses `LPSPI_SS_A` on the same pad. Analog load on P2_0 can make CS worse; it is not required to explain 60 MHz lockup.

Do **not** put 30 MHz in the 160-vs-240 table (SPI1 @ 80 MHz cannot make even-BAUDR 30 MHz).

---

## 6. Side-by-side summary

### 6.1 Max clock (one-shot, both directions PASS)

| Pairing | TRM slave cap 160 / 240 | **Observed max PASS 160** | **Observed max PASS 240** | Turbo SCLK gain |
|---|---|---|---|---|
| LPSPI master + SPI1 slave | 20 / 30 MHz (4×) | **10 MHz** | **10 MHz** | none (20 MHz MISO fail both) |
| SPI1 master + LPSPI slave | 13.3 / 20 MHz (12×) | **10 MHz** | **20 MHz** | **+10 MHz** |
| SPI1 master + LPSPI (turbo extra) | — | — | 30 MHz loop PASS (8×); DMA 800 B can lock. **60 MHz LOCKUP** (4×) | lab only; not TX+RX spec |

### 6.2 Throughput (bench, same 10 MHz SCK)

| Pairing | 160 MHz | 240 MHz | Delta |
|---|---|---|---|
| LPSPI master + SPI1 slave | 9343 kbit/s (6850 µs) | 9545 kbit/s (6705 µs) | **+2.2%** |
| SPI1 master + LPSPI slave | 9296 kbit/s (6884 µs) | 9495 kbit/s (6740 µs) | **+2.1%** |

Both pairings are **wire-bound** at 10 MHz. Turbo cannot give 1.5× Mbit/s unless SCK goes up.

### 6.3 Turbo vs normal (what to claim)

| Benefit | Always? | Pairing 1 | Pairing 2 |
|---|---|---|---|
| 1.5× CPU / HCLK / PCLK | Yes | Yes | Yes |
| ~2% faster at **same** 10 MHz SCK | Yes | 6850→6705 µs | 6884→6740 µs |
| Higher **passing** SCLK | Only if slave ratio + board allow | **No** (10 MHz cap) | **Yes: 20 MHz** (TRM 12×) |
| 1.5× SPI wire rate | Only if SCK ×1.5 and data PASS | Not shown | 10→20 MHz is **2× SCK**, not 1.5× |

---

## 7. How to read FAIL vs “it still transferred”

| Symptom | Meaning |
|---|---|
| Time ≈ wire + ~40–70 µs, `memcmp` 0 | PASS — SCK and data good |
| Time scales with SCK, `5A→7F` / `5A→B4` / `5A→2D` | SCK is real; **sample/hold** wrong (below TRM ratio or jumpers) |
| `0xEFFFFFFE` / cannot halt M55 | AHB/DMA **LOCKUP** (bus stall). LPSPI slave 60 MHz = 4×; 30 MHz DMA 800 B can also lock. Not a `memcmp` FAIL, not LPUART ISR storm |
| `-116` (`-ETIMEDOUT`) after `SPI tests completed` | Slave still armed; no more master clocks. **Not** a sweep fail |
| `Clock enable not supported` | `clock_control_on()` no-op; `get_rate()` is valid |

TRM ratios = **guaranteed correct data**. Silicon may still toggle SCK faster; data or the bus may fail.

---

## 8. Build

```bash
# Turbo 240 / HCLK 120
rm -rf build/; west build -p always -b alif_b1_dk/ab1c1f4m51820ph0/rtss_he \
  -S alif-dk -S spark-turbo ../alif/samples/drivers/spi_dw/ \
  -DCONFIG_FLASH_BASE_ADDRESS=0 -DCONFIG_FLASH_LOAD_OFFSET=0 -DCONFIG_FLASH_SIZE=256

# Normal 160 / HCLK 80 — omit -S spark-turbo
```

Overlay aliases select the pairing (`master-spi` / `slave-spi` + `serial-target` on the slave).

---

## 9. Suggested report wording

1. **CPU:** Spark turbo is **1.5×** (160→240, HCLK 80→120).  
2. **Same 10 MHz SPI:** about **+2%** throughput on both pairings.  
3. **LPSPI master + SPI1 slave:** max both-dir PASS **10 MHz** in both modes; 20 MHz MISO fail on jumpers.  
4. **SPI1 master + LPSPI slave:** max both-dir PASS **10 MHz (160)** and **20 MHz (240)** — matches LPSPI TX+RX **12×**. 20 MHz @ 160 clocks but MISO shifts (`5A→B4`).  
5. **30 MHz turbo** (SPI1→LPSPI): loop **PASS** at 8×; DMA 800 B can still lock. TRM TX+RX stays **20 MHz**.  
6. **60 MHz LPSPI slave:** **LOCKUP** (4×, AHB/DMA stall). Expected. Do not call supported.  
7. Quote **actual SCK** = SSI / even(BAUDR), not the `spi_config` name.
