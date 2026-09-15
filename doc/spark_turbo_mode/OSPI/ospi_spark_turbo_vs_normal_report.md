# Balletto B1 HE — OSPI Spark turbo (240 MHz) vs normal (160 MHz)

**Board:** `alif_b1_dk` / `ab1c1f4m51820ph0` / `rtss_he`  
**Flash:** ISSI IS25WX, 64 MB, OSPI0 (P4_2 SCLK, P4_1 RXDS, P2_1 SS1, P3_0–7)  
**SW:** Zephyr `e4aeaddf268f`, `flash_ospi_is25wx.c` + Alif OSPI HAL, octal DDR, DFS 16 (`write_block_size=2`)  
**Sample:** `alif/samples/drivers/spi_flash` with `-S ospi-flash`  
**Turbo:** `-S spark-turbo` (HE/ACLK 240, HCLK 120, PCLK 60; OSPI overlay `clock-frequency=240e6`, `bus-speed=40e6`)  
**Normal:** omit snippet (ACLK 160, DTS `clock-frequency=160e6`, `bus-speed=80e6`, `rx-ds-delay=11`)

Quote 40 MHz: `ospi_40mhz_fair.md` · Extra 60/80/120: `ospi_60_80_120mhz.md`  
JIRA paste: `JIRA_ZRTSS-854_OSPI.md` · Logs: `README.txt`  
Turbo consoles: `2026-09-15_b1_he_ospi_turbo240_*.txt` (Zephyr `e4aeaddf268f`).

This note is **observation on this DK + this driver**, not a flash datasheet or TRM change.

**OSPI testing for 160 vs 240 is complete.** No further matrix is required for the turbo report.

---

## 1. What was tested

| Experiment | What it measures |
|---|---|
| **Functional Tests 1–5** | 4 B, 1 KB, two sectors, optional full-chip erase, XiP vs `flash_read` at offset 0. Runs at **DTS** SCK. |
| **Timed bench** | 3 × 16 KB at `0x10000`. `k_cycle_get_32()` around `flash_erase` / `flash_write` / `flash_read` / XiP `memcpy` only. |
| **SCK sweep** | Exact **even BAUDR** only. 20 / 40 / 60 / 80 / 120 MHz; skip if HW SCK ≠ request (e.g. 80 from 240 → 120). |
| **RXDS tap sweep** | 120 MHz only: write at 40 MHz, read-only while walking `AES_RXDS_DLY` 0…31. A **tap** is one ACLK tick of RXDS delay. |
| **Size matrix** | Test1 pattern `55 aa 66 99` + lengths 2…16384 B × 3, different seeds. 40 / 60 / 80 / 120 MHz. 120 MHz skips ≥1024 B (`flash_read` can hang). |

A **tap** is `rx-ds-delay` = N × (1 / ACLK). Tap 11 @ 160 MHz = 68.8 ns (DTS default for 80 MHz). Tap 11 @ 240 MHz = 45.8 ns. At 120 MHz the passing window was taps **6–7** only.

---

## 2. Clock map (B1 HE)

| Clock | Normal | Turbo | Ratio |
|---|---|---|---|
| HE core / ACLK / SysTick / `ALIF_OSPI_CLK` | 160 MHz | 240 MHz | **1.50×** |
| HCLK | 80 MHz | 120 MHz | 1.50× |
| PCLK | 40 MHz | 60 MHz | 1.50× |
| HFOSC | 38.4 MHz | 38.4 MHz | 1.00× |

OSPI SSI is **ACLK**. The flash driver does **not** call `clock_control_get_rate()`; it programs `BAUDR = DTS clock-frequency / bus-speed`.

---

## 3. TRM OSPI_BAUDR

Same rule as DW SPI:

SCK = SSI / BAUDR,  BAUDR = 2, 4, …, 65534

LSB of BAUDR is stuck 0. Write 3 → HW stores 2.

| SSI | Requested SCK | Software `clk/speed` | HW BAUDR | Actual SCK |
|---|---|---|---|---|
| 160 | 40 | 4 | 4 | **40 MHz** |
| 160 | 60 | 2 | 2 | 80 MHz (not 60) — skipped |
| 160 | 80 | 2 | 2 | **80 MHz** |
| 160 | 120 | 1 → 2 | 2 | 80 MHz — skipped |
| 240 | 40 | 6 | 6 | **40 MHz** |
| 240 | 60 | 4 | 4 | **60 MHz** |
| 240 | 80 | 3 | 2 | **120 MHz** — skipped (do not label as 80) |
| 240 | 120 | 2 | 2 | **120 MHz** |

**80 MHz is impossible from 240 MHz** with even BAUDR. Turbo overlay must use 40 MHz (fair) or 60 MHz (turbo-only), not 80.

`baud2-delay`: 0 = off, 1 = on, 2 = auto (on iff `BAUDR==2`). Needed only at SSI/2.

---

## 4. Theoretical vs practical throughput

Octal DDR: 8 pins × 2 edges = **16 bits per SCK**.

ideal Mbit/s = SCK_MHz × 16,   ideal MB/s = SCK_MHz × 2

| SCK | Ideal read | Measured `flash_read` 49 152 B |
|---|---|---|
| 20 MHz | 320 Mbit/s | ~71 Mbit/s (turbo) / ~49.5 Mbit/s (normal) |
| 40 MHz | **640 Mbit/s** | **74.2 / 49.5 Mbit/s** |
| 60 MHz | 960 Mbit/s | **74.2 Mbit/s** when 16 KB bench PASSes |
| 80 MHz | 1280 Mbit/s | **49.5 Mbit/s** (160 only) |
| 120 MHz | 1920 Mbit/s | **74.2 Mbit/s** when 16 KB bench PASSes |

`flash_read` of 49 152 B is **1 271 691 CPU cycles** at 20, 40, and 80 MHz on 160 **and** at 40 / 60 / 120 MHz on 240.

- Same cycles at every SCK → **software-bound**, not wire-bound.  
- 74.2 / 49.5 = **1.50** = 240/160. Turbo “faster read” is **HE 1.5×**, not 40 vs 80 MHz flash.  
- Efficiency vs 40 MHz ideal: ~8% (160) / ~12% (240).

**Why so far below ideal**

The driver reads in **512 B** beats (`OSPI_MAX_RX_COUNT` 256 × 2-byte DFS). Each beat: CS, command, 32-bit address, **16 dummy cycles**, IRQ, `k_event_wait`, CS off. 16 KB = 32 setups. ~96% of the 5.3 ms (240) / 7.9 ms (160) is that path.

Write ~9 Mbit/s and erase ~0.3 Mbit/s are IS25WX **tPP / tSE**, not OSPI.

**XiP `memcpy`** (different machine): ~10.3 Mbit/s @ 40 MHz turbo; ~13.9 Mbit/s @ 80 MHz normal (16 KB bench); ~20.9 Mbit/s @ 120 MHz when it PASSes. XiP *does* move with SCK a little. Functional XiP vs `flash_read` at offset 0 **failed** at default 80 MHz (33–38 words).

---

## 5. Payload correctness (the real limiter)

### 5.1 Size matrix (3 repeats + Test1 `55 aa 66 99`)

| Mode | SCK | BAUDR | Tap | Result |
|---|---|---|---|---|
| Normal 160 | **40 MHz** | 4 | 11 | **43 PASS / 0 FAIL** |
| Turbo 240 | **40 MHz** | 6 | 11 | **43 PASS / 0 FAIL** |
| Turbo 240 | 60 MHz | 4 | 11 | **21 PASS / 22 FAIL** |
| Normal 160 | 80 MHz (DTS default) | 2 | 11 | **19 PASS / 24 FAIL** |
| Turbo 240 | 120 MHz | 2 | 6–7 | **6 PASS / 3 FAIL** (of 14, < 1024 B); skip ≥1024 B hang risk |

**Only 40 MHz is payload-reliable** on this board + driver.

Sweep may print `Max SCK PASS: 60 MHz` or `120 MHz` after **16 KB**. Size matrix is the limiter (60 MHz **21/22 FAIL**).

### 5.2 How 60 / 80 / 120 fail

- **1-bit drops**, often bit 0: `55→54`, `aa→a8`, `81→80`, `a5→a4`.  
- **80 / 120:** often byte `[1]` or `[513]` (second byte of a 512 B beat).  
- **60 MHz:** **exactly 1 bit per 4 bytes** (`err = length/4`: 16→4, 512→128, 1024→256, 4096→1024, 8192→2048). Systematic DFS / 32-bit stride, every iteration.  
- Full-chip erase **0 errors** on `0xFF`. Same sector then **43/43 at 40 MHz**. Not worn cells or leftover 120 MHz data.  
- 16 KB increment (`0x5A+i` or `0xAA`) **often PASSes** at 60/80/120 while 16 B / 256 B / 4 KB **FAIL every repeat**. A 16 KB bench is **not** proof the rate is safe.

### 5.3 Hang / 120 MHz tap window

High taps (≈24+) and **multi-beat `flash_read` ≥1024 B** can wait forever on RXDS. Matrix skips those sizes. 16 KB at tap 11: `MISMATCH err=60`. RXDS walk: taps **6–7 PASS**, 8 closes. Tap 6 can PASS 16 KB + XiP 20.9 Mbit/s; matrix still 6/3.

Logic analyzer: **~120 MHz** on P4_2 at `BAUDR=2`. Clock is real.

---

## 6. Timed bench (3 × 16 KB)

| Op | Normal @ 80 MHz DTS | Turbo @ 40 MHz DTS | Notes |
|---|---|---|---|
| Erase | ~1.35 s, 35 KiB/s | ~1.35 s, 35 KiB/s | `tSE` — no turbo win |
| Write | ~45 ms, **8.7 Mbit/s** | ~43 ms, **9.1 Mbit/s** | `tPP` |
| Read | **7948 µs, 49.5 Mbit/s** | **5298 µs, 74.2 Mbit/s** | **1.50× CPU** |
| XiP | 28.3 ms, 13.9 Mbit/s | 38.1 ms, 10.3 Mbit/s | 80 vs 40 SCK; XiP waits on SCK |

Sweep 20/40/60/80/120 (when it PASSes) keeps the **same 1.27 M read cycles**. Raising SCK does not raise cmd-path Mbit/s.

---

## 7. Functional tests at DTS SCK

| | Normal (80 MHz) | Turbo (40 MHz) |
|---|---|---|
| Test 1 (4 B `55 aa`) | **FAIL** `aa→a8` | **PASS** |
| Test 2 full erase | 0 errors (when enabled) | 0 errors |
| Test 3 (1 KB) | PASS or 1-bit FAIL (intermittent) | **PASS** |
| Test 4 sectors | Mixed `[1]` / `[513]` | **PASS** |
| Test 5 XiP vs cmd | **FAIL** 33–38 | **PASS** |

Default `b1.dtsi` **80 MHz + tap 11 + baud2-delay=1** fails Test 1 on this unit. QA/old sample runs that only check 16 KB or “erase succeeded” would not catch it.

---

## 8. Why this was not filed before

1. Happy-path sizes (32–128 B, 2 KB, **16 KB**, file/LFS/XIP images) match the **PASS** columns at 80/120.  
2. Official sample Test 1 is 4 bytes at **80 MHz** — it **does** fail here (`aa→a8`); easy to miss if Test 3/4 are the only glance.  
3. Other Alif boards (E7 OSPI1, other `bus-speed`) are a different analog.  
4. Nobody swept 2 B…16 KB × 3 at BAUDR=2 before this turbo work.

---

## 9. Side-by-side — what to claim

| Benefit | Always? | This board |
|---|---|---|
| 1.5× HE / ACLK / SysTick | Yes | Yes |
| 1.5× cmd-path **read time** at same software path | Yes | 7948 → 5298 µs (49.5 → 74.2 Mbit/s) |
| Higher **working** OSPI SCK | **No** | Reliable SCK stays **40 MHz** both modes |
| 1.5× wire / ideal 640 Mbit/s | **No** | Driver ceiling ~50 / 74 Mbit/s |
| Erase / program | No | `tSE` / `tPP` |
| 80 or 120 MHz as product SCK | **No** | Intermittent + 120 hang |

**Fair same-SCK column is 40 MHz.** Turbo does not make 80 MHz “work better”; 80 is not available from 240 with even BAUDR.

---

## 10. Build

```bash
# Turbo 240, OSPI DTS 240/40
west build -p always -b alif_b1_dk/ab1c1f4m51820ph0/rtss_he \
  -S ospi-flash -S spark-turbo ../alif/samples/drivers/spi_flash/

# Normal 160, DTS 160/80 — omit -S spark-turbo
west build -p always -b alif_b1_dk/ab1c1f4m51820ph0/rtss_he \
  -S ospi-flash ../alif/samples/drivers/spi_flash/
```

Optional later (not required for this report): `bus-speed = <40000000>` on normal so Tests 1–5 are clean every boot.

---

## 11. Suggested report wording

1. **CPU / ACLK:** Spark turbo is **1.5×** (160→240). OSPI SSI is ACLK.  
2. **BAUDR** is even. **80 MHz from 240 is impossible** (`240/80=3` → HW 2 → 120 MHz). Turbo DTS must be 40 or 60 MHz, not 80.  
3. **Reliable SCK (all payloads, 3 repeats):** **40 MHz** on both 160 and 240 (**43/43**).  
4. **60 MHz (turbo), 80 MHz (normal default), 120 MHz (turbo):** clocks are real; data is **not** consistent. 60 MHz drops bit 0 every 4 bytes (`err=length/4`). 80 MHz fails Test 1 and 24/43 matrix cases. 120 MHz can PASS 16 KB and **hang** `flash_read` at 1024 B. Quote as characterization only.  
5. **Cmd-path read:** **49.5 Mbit/s @ 160** vs **74.2 Mbit/s @ 240** at any SCK that works — **1.5× CPU**, same 1.27 M cycles. Ideal octal DDR at 40 MHz is 640 Mbit/s; this driver is ~8–12% of that (512 B IRQ beats + 16 dummies).  
6. **Write / erase:** no meaningful turbo gain (`tPP` / `tSE`).  
7. Do **not** claim 80 or 120 MHz OSPI as a turbo (or normal) product benefit.  
8. A passing 16 KB bench does **not** mean the rate is safe.

---

## 12. Testing status

| Item | Status |
|---|---|
| Normal 160, 40 MHz matrix | Done — 43/43 |
| Normal 160, 80 MHz matrix + functional | Done — 19/24, Test 1 FAIL |
| Turbo 240, 40 MHz matrix + functional | Done — 43/43, Tests 1–5 PASS |
| Turbo 240, 60 MHz matrix | Done — 21/22 |
| Turbo 240, 120 MHz + LA | Done — intermittent; hang at 1024 B; LA ~120 MHz |
| 20 MHz sweep both modes | Done — PASS, still CPU-bound |
| Further OSPI for this report | **None required** |
