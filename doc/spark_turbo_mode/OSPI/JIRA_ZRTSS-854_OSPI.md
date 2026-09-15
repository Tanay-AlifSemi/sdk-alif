# ZRTSS-854 — Spark turbo OSPI (Balletto B1 HE)

**Summary (title):** Spark turbo 160→240: OSPI SSI is ACLK. Even BAUDR: SCK = SSI / BAUDR. 80 MHz from 240 is impossible (→ 120 MHz). Reliable payload SCK is **40 MHz both**. Cmd-path read **49.5 → 74.2 Mbit/s** is 1.5× CPU, not faster flash.

**Board:** `alif_b1_dk` / `ab1c1f4m51820ph0` / `rtss_he`  
**Flash:** ISSI IS25WX, 64 MB, OSPI0 (P4_2 SCLK, P4_1 RXDS, **P2_1 SS1**, P3_0–7)  
**SW:** Zephyr `e4aeaddf268f`, `flash_ospi_is25wx.c` + Alif OSPI HAL, octal DDR, DFS 16. No `CONFIG_NO_OPTIMIZATIONS`.  
**Sample:** `../alif/samples/drivers/spi_flash/` with `-S ospi-flash`. Turbo: `-S spark-turbo`.  
**Evidence:** `alif/doc/spark_turbo_mode/OSPI/`  
**Status:** 160 vs 240 **complete**. No further matrix required for this ticket.

Full tables: `ospi_40mhz_fair.md`, `ospi_60_80_120mhz.md`. Combined: `ospi_spark_turbo_vs_normal_report.md`.

---

## 1. Clock change

HFOSC / HFXO stays **38.4 MHz**. Spark-turbo does not retune it. OSPI SSI is **ACLK** (PLL).

| Clock | Normal 160 | Turbo 240 | OSPI use |
|---|---|---|---|
| HFOSC | 38.4 MHz | 38.4 MHz | Not OSPI SSI |
| **HE / ACLK** | **160 MHz** | **240 MHz** | **OSPI SSI** (`ALIF_OSPI_CLK`) |
| HCLK | 80 MHz | 120 MHz | — |
| PCLK | 40 MHz | 60 MHz | — |

The flash driver does **not** call `clock_control_get_rate()`. It programs BAUDR from DTS: `BAUDR = clock-frequency / bus-speed`. Turbo overlay **must** set `clock-frequency = 240e6`. If left at 160 with ACLK 240, BAUDR=2 → **actual SCK 120 MHz**.

SCK = SSI / BAUDR. BAUDR even, 2 to 65534 (LSB hard-wired 0). Write 3 → HW stores 2.

| SSI | Requested SCK | SW clk/speed | HW BAUDR | Actual SCK |
|---|---|---|---|---|
| 160 | 40 | 4 | 4 | **40 MHz** (fair) |
| 160 | 80 | 2 | 2 | **80 MHz** (DTS default) |
| 160 | 60 / 120 | — | — | SKIP (not exact even BAUDR) |
| 240 | 40 | 6 | 6 | **40 MHz** (fair) |
| 240 | 60 | 4 | 4 | **60 MHz** (turbo-only) |
| 240 | 80 | 3 | 2 | **120 MHz** — do not label as 80 |
| 240 | 120 | 2 | 2 | **120 MHz** |

**80 MHz from 240 is impossible.** Same trap as SPI. Turbo DTS: **40 MHz** (fair) or **60 MHz** (characterization), not 80.

Fair same-SCK: **20 / 40 MHz**. Quote payload at **40 MHz**.

`baud2-delay`: needed only at SSI/2 (BAUDR=2). `rx-ds-delay` tap is N × (1/ACLK). Tap 11 = 68.8 ns @ 160, 45.8 ns @ 240.

Pins: CS is **P2_1 `OSPI0_SS1_B`**, not P2_0 (LPUART RX_A pad).

---

## 2. Throughput (quote cmd-path `flash_read`)

Octal DDR ideal = SCK_MHz × 16 bit/s. At 40 MHz that is **640 Mbit/s**.

`k_cycle_get_32()` around `flash_read` only. 3 × 16 KB = 49 152 B.

| | Normal 160 | Turbo 240 |
|---|---|---|
| Fair SCK | **40 MHz** (BAUDR 4) | **40 MHz** (BAUDR 6) |
| CPU cycles | **1 271 691** (same at every SCK that runs) | **1 271 691** |
| Wall time | **7948 µs** | **5298 µs** |
| Payload | **49.5 Mbit/s** | **74.2 Mbit/s** (**1.50×**) |

Same cycle count at 20 / 40 / 80 (160) and 40 / 60 / 120 (240) → **software-bound**, not wire-bound. Turbo “faster read” is **HE 1.5×**, not 40 vs 80 MHz flash.

Driver reads in **512 B** IRQ beats (16 dummy cycles, CS, command, address each beat). ~8% (160) / ~12% (240) of 40 MHz ideal.

Write ~9 Mbit/s and erase ~35 KiB/s are IS25WX **tPP / tSE**. No turbo win.

XiP `memcpy` moves a little with SCK (~10.3 Mbit/s @ 40 turbo, ~13.9 @ 80 normal). Do not quote as cmd-path throughput.

---

## 3. Max SCK (payload PASS, 3 repeats)

Size matrix: Test1 `55 aa 66 99` + lengths 2…16384 B × 3.

| Mode | SCK | BAUDR | Result |
|---|---|---|---|
| Normal 160 | **40 MHz** | 4 | **43/43 PASS** |
| Turbo 240 | **40 MHz** | 6 | **43/43 PASS** |
| Turbo 240 | 60 MHz | 4 | 21 PASS / 22 FAIL (`err = length/4`) |
| Normal 160 | 80 MHz (DTS default) | 2 | 19 PASS / 24 FAIL. Test 1 `aa→a8` |
| Turbo 240 | 120 MHz | 2 | small sizes intermittent; **`flash_read` 1024 B HANG** |

**Only 40 MHz is payload-reliable** on this DK + this driver. Turbo does **not** raise working OSPI SCK.

Sweep firmware may print `Max SCK read/write PASS: 60 MHz` or `120 MHz` after **16 KB**. That is not the size matrix. 60 MHz matrix is **21/22 FAIL**. 120 MHz 16 KB can PASS at tap 6 and still FAIL 16/256/512 B.

A passing **16 KB** bench at 60/80/120 does **not** mean the rate is safe (16 B / 256 B / 4 KB still FAIL).

60 MHz: systematic 1 bit per 4 bytes. 80/120: 1-bit drops, often byte `[1]` or `[513]`. Full-chip erase 0 errors on `0xFF` then 43/43 at 40 MHz → not worn cells.

120 MHz hang: erase/write OK, stuck on RXDS wait. Reset required. LA on P4_2: ~120 MHz at BAUDR=2 (clock is real).

Default `b1.dtsi` **80 MHz + tap 11 + baud2-delay=1** fails Test 1 on this unit.

---

## 4. One-line conclusions

1. CPU / ACLK **1.5×**. OSPI SSI = ACLK. HFOSC 38.4 unchanged.
2. Even BAUDR: SCK = SSI / even(BAUDR). **80 MHz from 240 is not a real SCK** (HW 120 MHz). Overlay `clock-frequency` must be 240.
3. Reliable SCK (all payloads): **40 MHz both**. 43/43.
4. Cmd-path read **49.5 → 74.2 Mbit/s** at the same 1.27 M cycles. Not 640 Mbit/s. Not 1.5× because SCK went up.
5. 60 / 80 / 120 MHz: clock real, data not consistent. 120 MHz can hang `flash_read` ≥1024 B. Characterization only.
6. Write / erase: `tPP` / `tSE`. No turbo claim.
7. Do **not** claim 80 or 120 MHz OSPI as a turbo (or normal) product benefit.
8. Do **not** quote sweep `Max SCK PASS: 60/120 MHz` — that is 16 KB only. Size matrix stays **40 MHz**.
