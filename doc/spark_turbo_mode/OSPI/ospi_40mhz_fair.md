# OSPI — fair 40 MHz (160 vs 240)

**Board:** `alif_b1_dk` / `ab1c1f4m51820ph0` / `rtss_he`  
**Flash:** ISSI IS25WX 64 MB on OSPI0  
**SW:** Zephyr `e4aeaddf268f`, `flash_ospi_is25wx.c` + Alif OSPI HAL (no `CONFIG_NO_OPTIMIZATIONS`)  
**Pins:** P4_2 SCLK, P4_1 RXDS, P2_1 SS1, P3_0–7 data  
**Roles:** on-board octal DDR flash (not loopback)

This is the **quote** 160-vs-240 column. Same actual SCK **40.00 MHz** both modes (even BAUDR).

**Quote log (turbo):** `2026-09-15_b1_he_ospi_turbo240_40mhz_sweep60.txt`  
**Quote matrix (turbo):** `2026-09-15_b1_he_ospi_turbo240_matrix_40_60_120.txt` (40 MHz 43/43)

---

## Clock / BAUDR

SCK = SSI / BAUDR. BAUDR even (2 … 65534). Driver uses DTS `clock-frequency / bus-speed` (no `get_rate()`).

| | Normal 160 | Turbo 240 |
|---|---|---|
| OSPI SSI (ACLK) | 160 MHz | 240 MHz |
| `clock-frequency` | 160e6 | **240e6** (overlay required) |
| `bus-speed` (fair) | **40e6** | **40e6** |
| BAUDR | 4 | 6 |
| actual SCK | **40.00 MHz** | **40.00 MHz** |
| `rx-ds-delay` tap 11 | 68.8 ns | 45.8 ns |
| `baud2-delay` | off (BAUDR ≠ 2) | off (BAUDR ≠ 2) |

Default `b1.dtsi` is **160/80** (BAUDR=2, 80 MHz). That is **not** the fair column. See `ospi_60_80_120mhz.md`.

---

## Payload — size matrix @ 40 MHz

Test1 pattern `55 aa 66 99` + lengths 2…16384 B × 3 repeats, different seeds.

| | Normal 160 | Turbo 240 |
|---|---|---|
| SCK / BAUDR / tap | 40 / 4 / 11 | 40 / 6 / 11 |
| Result | **43 PASS / 0 FAIL** | **43 PASS / 0 FAIL** |

Functional Tests 1–5 at 40 MHz turbo: **PASS**. Same pattern on 160 @ 40 MHz: **PASS**.

---

## Bench — 3 × 16 KB `flash_read` (same SCK)

`k_cycle_get_32()` around `flash_read` only. 49 152 B.

| | Normal 160 | Turbo 240 | Turbo vs normal |
|---|---|---|---|
| actual SCK | 40.00 MHz | 40.00 MHz | same |
| CPU cycles | 1 271 691 | 1 271 691 | **1.00×** (same work) |
| **Wall time** | **7948 µs** | **5298 µs** | **÷1.50** |
| Payload | **49.5 Mbit/s** | **74.2 Mbit/s** (console 74219 kbit/s) | **+50%** |
| Octal DDR ideal @ 40 MHz | 640 Mbit/s | 640 Mbit/s | software-bound |
| Efficiency vs ideal | ~8% | ~12% | 512 B IRQ beats |

Same cycle count at 20 MHz as at 40 MHz → not wire-bound. Turbo gain is **HE 1.5×**, not a faster OSPI SCK.

Erase ~1.35 s (35 KiB/s) and write ~9 Mbit/s: IS25WX **tSE / tPP**. Same both clocks.

---

## What turbo does here

| Item | Result |
|---|---|
| Same 40 MHz payload | **43/43 both** |
| Cmd-path read time | **1.5× faster** (CPU) |
| Max passing SCK | **None** — 40 MHz both (60/80/120 not reliable) |
| 1.5× wire 640 Mbit/s | **No** — driver ceiling ~50 / 74 Mbit/s |

---

## Build

```bash
# Turbo 240, OSPI DTS 240/40
west build -p always -b alif_b1_dk/ab1c1f4m51820ph0/rtss_he \
  -S ospi-flash -S spark-turbo ../alif/samples/drivers/spi_flash/

# Normal 160 — omit -S spark-turbo
# For a fair 40 MHz payload on 160, set bus-speed = <40000000> (default DTS is 80 MHz).
west build -p always -b alif_b1_dk/ab1c1f4m51820ph0/rtss_he \
  -S ospi-flash ../alif/samples/drivers/spi_flash/
```
