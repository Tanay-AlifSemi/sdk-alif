# Balletto B1 HE — UART Spark turbo (240 MHz) vs normal (160 MHz)

**Board:** `alif_b1_dk` / `ab1c1f4m51820ph0` / `rtss_he`  
**SW:** Zephyr `d58e56f5ef71`, NS16550 (`uart_ns16550.c`).  
**Sample:** `samples/basic/blinky` + B1 UART check (`alif_uart_check.c`) — driver APIs only (`uart_configure`, `uart_poll_*`, `uart_fifo_fill`/`read` in ISR, `uart_line_ctrl_set(LOOPBACK)`).  
**Turbo:** `-S spark-turbo` (HE/ACLK 240, HCLK 120, PCLK 60)  
**Normal:** omit the snippet (160/80/40)

This note is **observation on this DK + this driver**

**UART0–5 and LPUART 160 vs 240 test**

---

## 1. What was tested

| Experiment | Port / pins | What it measures |
|---|---|---|
| **115200 check** | UART2 console `uart@4901a000` (P5_3 TX → USB-COM) | Driver retunes DL/DLF from `clock_control_get_rate()`. Host stays 115200 in both modes. |
| **MCR loopback sweep** | UART0, `uart_line_ctrl_set(LOOPBACK)` | On-chip TX→RX via ns16550. 64-byte poll. Console stays UART2. |
| **LA TX burst** | UART0 TX **P8_1** | `uart_poll_out(0x55)`. Measure bit width, do not UART-decode. |
| **External loopback** | UART0 **P8_1 TX ↔ P8_0 RX** jumper | Same sweep, LOOP off, `uart_poll_*`. |
| **Throughput (IRQ)** | Same jumper | `uart_fifo_fill`/`fifo_read` in ISR (same APIs as `alif/tests/drivers/uart`). |

USB-COM echo (`uart_echo_test.py` on UART2) PASSed 3×500 blocks at **115200** after handshake sync. It is **not** a high-baud SoC test: the USB bridge never sees `ECHO` at 2.5 / 3.75 M. Script left unused.

Rates: 9600, 115200, 230400, 460800, 921600, 1.5M, 2.0M, 2.5M, 3.0M, 3.75M, 4.0M.  
`SKIP (DL=0)` = not programmable (`SCLK / (16 × baud) = 0`).

---

## 2. Clock map (B1 HE)

| Clock | Normal | Turbo | Ratio |
|---|---|---|---|
| HE core / ACLK / SysTick | 160 MHz | 240 MHz | 1.50× |
| HCLK | 80 MHz | 120 MHz | 1.50× |
| **SYST_PCLK** (UART0–5 SCLK) | **40 MHz** | **60 MHz** | 1.50× |
| LPUART SCLK | HE 160 | HE 240 | 1.50× |
| **HFOSC / HFXO** | **38.4 MHz** | **38.4 MHz** | **1.00×** |

UART0–5 DTS clocks are `ALIF_UARTn_SYST_PCLK` (PLL PCLK), not `ALIF_UARTn_38M4_CLK`. LPUART is `ALIF_LPUART_CLK` → HE (PLL). Spark-turbo only retunes `pll_clk1` / ACLK / HCLK / PCLK; it does **not** change `hfxo`. HFOSC is the crystal / PLL *reference*; the PLL output is what moved 160→240.

If UART were muxed to 38.4 MHz, turbo would **not** change baud. This board is not on that mux — measured SCLK is 40/60 (UART0–5) and 160/240 (LPUART).

This report is UART0–5 (`SYST_PCLK`) **and LPUART (`HE`)**. See §8 for LPUART.

---

## 3. Baud equation

\[
\mathrm{baud} = \frac{\mathrm{SCLK}}{16\cdot\mathrm{DL} + \mathrm{DLF}}
\]

Driver path: DTS has `clocks`, no `clock-frequency` → `clock_control_get_rate()`, then DL / DLF. Overlay `dlf=<11>` is **not** written on that path.

\[
\mathrm{DL} = \lfloor \mathrm{SCLK}/(16\cdot\mathrm{baud}) \rfloor, \quad
\mathrm{DLF} = \mathrm{round}(\mathrm{SCLK}/\mathrm{baud}) \bmod 16
\]

Max programmable rate = **SCLK / 16** (DL = 1, DLF = 0).

| Mode | SCLK | 115200 DL / DLF | Actual 115200 | Max |
|---|---|---|---|---|
| Normal 160 | 40 MHz | 21 / 11 | 115273 (+633 ppm) | **2.5 Mbps** |
| Turbo 240 | 60 MHz | 32 / 9 | 115163 (−321 ppm) | **3.75 Mbps** |

Host terminal stays **115200** in both modes. A stale divisor (turbo still using 21/11) would run ~1.5× fast (bit ~5.76 µs).

80 MHz from 240 is an OSPI/SPI even-BAUDR issue. It does **not** apply to UART.

---

## 4. Results

### 4.1 Console 115200

| Mode | SCLK | HW DL / DLF | Result |
|---|---|---|---|
| Normal | 40 MHz | 21 / 11 | **MATCH** |
| Turbo | 60 MHz | 32 / 9 | **MATCH** |

USB-COM readable at 115200 both builds. Driver retunes.

### 4.2 MCR on-chip (UART2) and wire loopback (UART0 P8_1↔P8_0)

Same table both experiments. External used a 64-byte `0xA5^i` pattern. No FAIL (−2 missing RX) or FAIL (−3 mismatch).

**Normal (PCLK 40 MHz)**

| baud | DL | DLF | actual | ppm | result |
|---|---|---|---|---|---|
| 9600 | 260 | 7 | 9599 | −104 | PASS |
| 115200 | 21 | 11 | 115273 | +633 | PASS |
| 230400 | 10 | 14 | 229885 | −2235 | PASS |
| 460800 | 5 | 7 | 459770 | −2235 | PASS |
| 921600 | 2 | 11 | 930232 | +45 | PASS |
| 1500000 | 1 | 11 | 1481481 | −892 | PASS |
| 2000000 | 1 | 4 | 2000000 | 0 | PASS |
| **2500000** | 1 | 0 | 2500000 | 0 | **PASS** |
| 3000000 | — | — | — | — | SKIP (DL=0) |
| 3750000 | — | — | — | — | SKIP (DL=0) |
| 4000000 | — | — | — | — | SKIP (DL=0) |

MAX PASS = theor max = **2.5 Mbps**.

**Turbo (PCLK 60 MHz)**

| baud | DL | DLF | actual | ppm | result |
|---|---|---|---|---|---|
| 9600 | 390 | 10 | 9600 | 0 | PASS |
| 115200 | 32 | 9 | 115163 | −321 | PASS |
| 230400 | 16 | 4 | 230769 | +1601 | PASS |
| 460800 | 8 | 2 | 461538 | +1601 | PASS |
| 921600 | 4 | 1 | 923076 | +1601 | PASS |
| 1500000 | 2 | 8 | 1500000 | 0 | PASS |
| 2000000 | 1 | 14 | 2000000 | 0 | PASS |
| 2500000 | 1 | 8 | 2500000 | 0 | PASS |
| 3000000 | 1 | 4 | 3000000 | 0 | PASS |
| **3750000** | 1 | 0 | 3750000 | 0 | **PASS** |
| 4000000 | — | — | — | — | SKIP (DL=0) |

MAX PASS = theor max = **3.75 Mbps**.

### 4.3 Logic analyzer (UART0 TX P8_1)

Expected bit widths (decode off):

| Requested | Bit time | Mode |
|---|---|---|
| 115200 | ~8.68 µs | both |
| 2.5 M | 400 ns | both (normal max) |
| 3.75 M | 267 ns | turbo only |

Reverse checks: \(\mathrm{baud}=1/T_{\mathrm{bit}}\), \(\mathrm{PCLK}=\mathrm{baud}\times(16\cdot\mathrm{DL}+\mathrm{DLF})\).

On normal the firmware prints `SKIP baud: 3750000 dl: 0` (not programmable). Trust MCR / external tables for 3.75 M on 160.

### 4.4 USB-COM echo (not a high-baud result)

UART2 @ 115200, PC `uart_echo_test.py`: **3/3 PASS**, 0 byte errors, 0 timeouts (500 × 256 B each) after marker sync. ~6.9 kB/s (host poll, not the wire).  
2.5 M / 3.75 M on the same USB-COM: no `ECHO` (bridge / host UART). **Do not treat as SoC fail.**

### 4.5 Throughput (`k_cycle_get_32`, P8_1→P8_0)

8N1 → 10 bits/byte on the wire. Payload Mbit/s uses 8 data bits. Ideal payload = `baud × 0.8`.  
Payload **10 240 B** (1024 × 10). Timed around one transfer only.

Ideal payload: 115200 → 0.092 Mbit/s; 2.5 M → **2.00**; 3.75 M → **3.00**.

**Do not quote poll** (`uart_poll_out` / `uart_poll_in`). Byte-at-a-time is slower than the IRQ+FIFO path.

**IRQ + FIFO** (`uart_fifo_fill` / `uart_fifo_read` in ISR — quote this for the driver):

| Baud | Mode | Cycles | µs | Payload Mbit/s | Wire µs | vs wire | vs ideal |
|---|---|---|---|---|---|---|---|
| 115200 | Normal 160 | 142 294 097 | 889 338 | **0.09** | 888 325 | 1.00× | ~100% |
| 115200 | Turbo 240 | 213 568 005 | 889 866 | **0.09** | 889 174 | 1.00× | ~100% |
| 2.5 M | Normal 160 | 7 709 001 | 48 181 | **1.70** | 40 960 | 1.18× | 85% |
| 2.5 M | Turbo 240 | 11 272 081 | 46 967 | **1.74** | 40 960 | 1.15× | 87% |
| 3.75 M | Turbo 240 | 7 709 001 | 32 120 | **2.55** | 27 306 | 1.18× | 85% |

**Same 115200 IRQ:** wire-bound both modes (~890 ms, 0.09 Mbit/s). Cycles **1.50×** (142 M → 214 M) = HE 160→240. Wall time does not move.

**Same 2.5 M IRQ:** a bit above wire (47–48 ms vs 41 ms). Wall time **the same class**. Turbo does **not** raise Mbit/s at the same baud (1.70 vs 1.74). Neither reaches 2.00 — ISR + 16-byte FIFO watermark, not PCLK.

**3.75 M turbo IRQ:** **2.55 Mbit/s** (85% of 3.00). That is the real turbo throughput win: higher **baud**, not 1.5× at 2.5 M.

No DMA in this bench.

---

## 5. Takeaways

1. **PCLK tracks 1.5×** (40 → 60 MHz). Driver recomputes DL/DLF. Console stays 115200.
2. **Fair max is 2.5 Mbps** (both modes). **Turbo-only max is 3.75 Mbps.** 4.0 M is impossible from 60 MHz (`SCLK/16`).
3. **External jumper matches MCR** at every programmable rate. Pads work at those bauds on this DK.
4. USB-COM is only good at 115200. High-baud proof is MCR + P8_1/P8_0 + LA.
5. Unlike OSPI/SPI, UART has **no even-BAUDR trap**. 80 MHz from 240 is not a UART issue.
6. **HFOSC 38.4 MHz does not move with turbo** and is **not** UART0–5 / LPUART SCLK on this DTS. Baud follows PLL PCLK / HE. See §2.
7. **Throughput — quote IRQ, not poll.** 115200 IRQ is wire (**0.09** both). Same baud is **not** 1.5× Mbit/s. Turbo win is a higher max baud. See §4 / §8.4.
8. **LPUART** SCLK is HE (160→240). Pins **P7_1 TX_B / P7_0 RX_B** (not P2_0). Correct-data **10 Mbps** both, **15 Mbps** turbo. IRQ 10 M is **7.19 / 7.00** (not 8); 15 M turbo **10.79** (not 12). See §8.

---

## 6. Build

```bash
# Turbo (ITCM)
rm -rf build/; west build -p always -b alif_b1_dk/ab1c1f4m51820ph0/rtss_he samples/basic/blinky -S spark-turbo -DCONFIG_FLASH_BASE_ADDRESS=0 -DCONFIG_FLASH_LOAD_OFFSET=0 -DCONFIG_FLASH_SIZE=256;

# Normal 160 (ITCM, omit -S spark-turbo)
rm -rf build/; west build -p always -b alif_b1_dk/ab1c1f4m51820ph0/rtss_he samples/basic/blinky -DCONFIG_FLASH_BASE_ADDRESS=0 -DCONFIG_FLASH_LOAD_OFFSET=0 -DCONFIG_FLASH_SIZE=256;
```

Console: USB-COM `/dev/ttyUSB2` @ **115200**. UART0 jumper **P8_1 ↔ P8_0**. LPUART jumper **P7_1 ↔ P7_0**. Do not jumper UART2 (P5_3) or LPUART P2_0.

Raw console: `2026-09-07_b1_he_uart_lpuart_normal160_p7_0.txt` and `..._turbo240_p7_0.txt` next to this report.

---

## 7. Conclusion

1. **CPU / PCLK:** Spark turbo is **1.5×** (160→240, PCLK 40→60). UART0–5 SCLK is PCLK.
2. **115200:** DL/DLF MATCH both (21/11 vs 32/9). Host stays 115200.
3. **Correct-data max:** **2.5 Mbps** both modes (MCR + P8_1↔P8_0). Turbo also **3.75 Mbps**. 4.0 M not programmable.
4. **Throughput (ns16550 IRQ + FIFO, 10 240 B):** 115200 **0.09 Mbit/s** both (wire). Same baud is not 1.5×. UART0 2.5 M **1.70 / 1.74**, 3.75 M turbo **2.55**. Do not quote poll.
5. USB-COM TX/RX PASS at 115200 only. 2.5 / 3.75 M on USB-COM is a bridge limit, not SoC.
6. No even-BAUDR issue (unlike SPI/OSPI 80 MHz from 240).
7. **HFOSC 38.4 MHz unchanged** (PLL reference, not UART SCLK). UART0–5 = PCLK 40→60; LPUART = HE 160→240.
8. **LPUART (HE, not PCLK):** P7_1 TX_B / P7_0 RX_B. P2_0 RX_A: IRQ storm (jumper off) or TAD9 APB lock (jumper on). Correct-data **10 Mbps** both, **15 Mbps** turbo. IRQ: 2.5 M **1.95 / 1.96**, 10 M **7.19 / 7.00** (not 8), 15 M turbo **10.79** (not 12).

---

## 8. LPUART (HE clock, `uart@43008000`)

Same ns16550 driver as UART0–5. SCLK is **HE**, not PCLK. This sample muxes **P7_1 `LPUART_TX_B`** and **P7_0 `LPUART_RX_B`**. Board default **P2_0 `LPUART_RX_A`** is not used. P2_0 has two failures (see `lpuart_p2_0_hw_report.md`): jumper **off** → RX IRQ storm (`uart_irq_rx_ready` with empty FIFO, ISR never returns); jumper **P7_1→P2_0** → APB lock, debugger `TAD9-NAL64`. P7_0 does not storm. Mixed A/B group; P2_0 is also ANA_S8 / LPSPI_SS.

Wire and throughput: IRQ `uart_fifo_fill` / `fifo_read` (same as `alif/tests/drivers/uart`). MCR: poll + `UART_LINE_CTRL_LOOPBACK`.

### 8.1 Clock / baud

\[
\mathrm{baud} = \frac{\mathrm{HE}}{16\cdot\mathrm{DL} + \mathrm{DLF}}
\]

Max = **HE / 16**.

| Mode | HE / LPUART SCLK | 115200 DL / DLF | Actual 115200 | Max |
|---|---|---|---|---|
| Normal 160 | 160 MHz | 86 / 13 | 115190 (−86 ppm) | **10 Mbps** |
| Turbo 240 | 240 MHz | 130 / 3 | 115218 (+156 ppm) | **15 Mbps** |

HE 1.5× → max baud 1.5×. Console stays UART2 @ 115200.

### 8.2 MCR on-chip and wire (P7_1→P7_0)

Same PASS list for MCR and the jumper (IRQ 64-byte pattern on the wire).

**Normal (HE 160 MHz)**

| baud | DL | DLF | actual | ppm | result |
|---|---|---|---|---|---|
| 9600 | 1041 | 11 | 9599 | −104 | PASS |
| 115200 | 86 | 13 | 115190 | −86 | PASS |
| 230400 | 43 | 6 | 230547 | +638 | PASS |
| 460800 | 21 | 11 | 461095 | +640 | PASS |
| 921600 | 10 | 14 | 919540 | −2235 | PASS |
| 1500000 | 6 | 11 | 1495327 | −252 | PASS |
| 2000000 | 5 | 0 | 2000000 | 0 | PASS |
| 2500000 | 4 | 0 | 2500000 | 0 | PASS |
| 3000000 | 3 | 5 | 3018867 | +562 | PASS |
| 3750000 | 2 | 11 | 3720930 | +265 | PASS |
| 4000000 | 2 | 8 | 4000000 | 0 | PASS |
| 5000000 | 2 | 0 | 5000000 | 0 | PASS |
| **10000000** | 1 | 0 | 10000000 | 0 | **PASS** |
| 15000000 | — | — | — | — | SKIP (DL=0) |

MAX PASS = theor max = **10 Mbps**.

**Turbo (HE 240 MHz)**

| baud | DL | DLF | actual | ppm | result |
|---|---|---|---|---|---|
| 9600 | 1562 | 8 | 9600 | 0 | PASS |
| 115200 | 130 | 3 | 115218 | +156 | PASS |
| 230400 | 65 | 2 | 230326 | −321 | PASS |
| 460800 | 32 | 9 | 460652 | −321 | PASS |
| 921600 | 16 | 4 | 923076 | +1601 | PASS |
| 1500000 | 10 | 0 | 1500000 | 0 | PASS |
| 2000000 | 7 | 8 | 2000000 | 0 | PASS |
| 2500000 | 6 | 0 | 2500000 | 0 | PASS |
| 3000000 | 5 | 0 | 3000000 | 0 | PASS |
| 3750000 | 4 | 0 | 3750000 | 0 | PASS |
| 4000000 | 3 | 12 | 4000000 | 0 | PASS |
| 5000000 | 3 | 0 | 5000000 | 0 | PASS |
| 10000000 | 1 | 8 | 10000000 | 0 | PASS |
| **15000000** | 1 | 0 | 15000000 | 0 | **PASS** |

MAX PASS = theor max = **15 Mbps**.

### 8.3 Logic analyzer (P7_1)

`uart_poll_out(0x55)`, jumper off. Measure one bit, no UART decode.

| Requested | Bit time | Mode |
|---|---|---|
| 115200 | ~8.68 µs | both |
| 2.5 M | 400 ns | both |
| 3.75 M | 267 ns | both |
| 10 M | 100 ns | both (normal max) |
| 15 M | 66.7 ns | turbo only |

### 8.4 Throughput (IRQ, P7_1→P7_0)

1024 B × 10, `k_cycle_get_32()`, 8N1. Quote IRQ. Fair vs UART0: **115200 and 2.5 M**. Extra: 3.75 M and 10 M both; **15 M turbo only**.

Ideal payload = `baud × 0.8`.

| Baud | Mode | Cycles | µs | Payload Mbit/s | Wire µs | vs wire | vs ideal |
|---|---|---|---|---|---|---|---|
| 115200 | Normal 160 | 142 383 630 | 889 897 | **0.09** | 888 966 | 1.00× | ~100% |
| 115200 | Turbo 240 | 213 453 627 | 889 390 | **0.09** | 888 750 | 1.00× | ~100% |
| 2.5 M | Normal 160 | 6 713 979 | 41 962 | **1.95** | 40 960 | 1.02× | 98% |
| 2.5 M | Turbo 240 | 10 011 843 | 41 716 | **1.96** | 40 960 | 1.02× | 98% |
| 3.75 M | Normal 160 | 4 560 025 | 28 500 | **2.87** | 27 520 | 1.04× | 96% |
| 3.75 M | Turbo 240 | 6 713 940 | 27 974 | **2.92** | 27 306 | 1.02× | 97% |
| 10 M | Normal 160 | 1 822 199 | 11 388 | **7.19** | 10 240 | 1.11× | 90% |
| 10 M | Turbo 240 | 2 808 616 | 11 702 | **7.00** | 10 240 | 1.14× | 88% |
| 15 M | Turbo 240 | 1 822 231 | 7 592 | **10.79** | 6 826 | 1.11× | 90% |

**115200:** wire (**0.09** both). Cycles **1.50×** with HE.

**2.5 M:** **1.95 / 1.96** — not 1.5× Mbit/s.

**10 M / 15 M:** correct data on the wire. Quote **line max 10 / 15 Mbps**. IRQ **7.19 / 7.00 / 10.79** is not 8 / 12. Same 10 M is **not** faster at 240 (7.19 vs 7.00).

### 8.5 LPUART vs UART0 (IRQ)

| | UART0–5 | LPUART |
|---|---|---|
| SCLK | PCLK 40 / 60 | HE 160 / 240 |
| Pins | P8_1 / P8_0 | P7_1 TX_B / P7_0 RX_B |
| Correct-data max | 2.5 M both, 3.75 M turbo | **10 M both, 15 M turbo** |
| 115200 IRQ | 0.09 / 0.09 | 0.09 / 0.09 |
| 2.5 M IRQ | **1.70 / 1.74** | **1.95 / 1.96** |
| High-baud IRQ | 3.75 M turbo **2.55** | 10 M **7.19 / 7.00**, 15 M turbo **10.79** |
