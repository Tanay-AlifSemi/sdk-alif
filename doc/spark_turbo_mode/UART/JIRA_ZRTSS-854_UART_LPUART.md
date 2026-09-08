# ZRTSS-854 — Spark turbo UART / LPUART (Balletto B1 HE)

**Summary (title):** Spark turbo 160→240: UART0–5 PCLK 40→60 (max 2.5 / 3.75 Mbps); LPUART HE 160→240 (max 10 / 15 Mbps). Do not use P2_0 as LPUART RX.

**Board:** `alif_b1_dk` / `ab1c1f4m51820ph0` / `rtss_he`  
**SW:** Zephyr `d58e56f5ef71`, `uart_ns16550`  
**Sample:** `samples/basic/blinky` + UART check. Turbo: `-S spark-turbo`.  
**Evidence:** `alif/doc/spark_turbo_mode/UART/`  
**Status:** UART0–5 and LPUART **complete** on P7_1 / P7_0. P2_0 is a **board/pad** issue, not an IP or PLL fail.

Full tables: `uart_spark_turbo_vs_normal_report.md`. P2_0: `lpuart_p2_0_hw_report.md`.

---

## 1. Clock change

HFOSC / HFXO stays **38.4 MHz** (PLL reference). Spark-turbo does not retune it. UART baud on this DTS does **not** use 38.4 MHz.

| Clock | Normal 160 | Turbo 240 | UART use |
|---|---|---|---|
| HFOSC | 38.4 MHz | 38.4 MHz (unchanged) | Not UART SCLK |
| HE / ACLK | 160 MHz | 240 MHz | **LPUART SCLK** |
| HCLK | 80 MHz | 120 MHz | — |
| **PCLK** | **40 MHz** | **60 MHz** | **UART0–5 SCLK** |

UART0–5 DTS: `ALIF_UARTn_SYST_PCLK` (PCLK). LPUART: `ALIF_LPUART_CLK` (HE).  
There is an unused mux `ALIF_UARTn_38M4_CLK`; this board is **not** on it. If UART were on 38.4, turbo would not change baud.

\[
\mathrm{baud} = \mathrm{SCLK} / (16\cdot\mathrm{DL} + \mathrm{DLF})
\]

Max programmable = **SCLK / 16**. Driver retunes DL/DLF from `clock_control_get_rate()`. Host console stays **115200**.

| Port | Mode | SCLK | 115200 DL/DLF | Console |
|---|---|---|---|---|
| UART2 | 160 | 40 MHz | 21 / 11 (115273) | **MATCH** |
| UART2 | 240 | 60 MHz | 32 / 9 (115163) | **MATCH** |
| LPUART | 160 | 160 MHz | 86 / 13 | — |
| LPUART | 240 | 240 MHz | 130 / 3 | — |

No even-BAUDR trap (unlike SPI/OSPI 80 MHz from 240).

---

## 2. Max speed (correct data on the wire)

MCR on-chip and external jumper match at every programmable rate.

**UART0** jumper **P8_1 TX ↔ P8_0 RX**

| Mode | Max PASS | Theor SCLK/16 | Next rate |
|---|---|---|---|
| Normal 160 | **2.5 Mbps** | 2.5 Mbps | 3.75 M SKIP (DL=0) |
| Turbo 240 | **3.75 Mbps** | 3.75 Mbps | 4.0 M SKIP (DL=0) |

**LPUART** jumper **P7_1 TX_B ↔ P7_0 RX_B** (same mux group B)

| Mode | Max PASS | Theor HE/16 | Next rate |
|---|---|---|---|
| Normal 160 | **10 Mbps** | 10 Mbps | 15 M SKIP (DL=0) |
| Turbo 240 | **15 Mbps** | 15 Mbps | at SCLK/16 |

USB-COM is only valid at 115200 (bridge). High-baud proof is MCR + jumper + LA bit width (not UART decode).

---

## 3. Throughput (quote IRQ + FIFO)

`uart_fifo_fill` / `uart_fifo_read` in ISR, 1024 B × 10, 8N1.  
**Do not quote poll.**

Same baud is **not** 1.5× Mbit/s. Turbo adds **3.75 M (UART0)** and **15 M (LPUART)**. At 10 M, 160 vs 240 is 7.19 vs 7.00 (CPU/ISR, not PCLK).

**UART0 P8_1↔P8_0**

| Baud | 160 | 240 | Note |
|---|---|---|---|
| 115200 | **0.09** | **0.09** | Wire |
| 2.5 M | **1.70** | **1.74** | Not 2.00, not 1.5× |
| 3.75 M | SKIP | **2.55** | Turbo-only max |

**LPUART P7_1↔P7_0**

| Baud | 160 | 240 | 8N1 ideal |
|---|---|---|---|
| 115200 | **0.09** | **0.09** | 0.09 |
| 2.5 M | **1.95** | **1.96** | 2.00 |
| 3.75 M | **2.87** | **2.92** | 3.00 |
| 10 M | **7.19** | **7.00** | 8.00 |
| 15 M | SKIP | **10.79** | 12.00 |

Quote **line max 10 / 15 Mbps**. IRQ 10 M is **~7.0 both clocks** (not 8); 15 M turbo **10.79** (not 12).

Logs: `..._normal160_p7_0.txt` / `..._turbo240_p7_0.txt`

---

## 4. LPUART P2_0 — pad issue (not IP / not turbo)

Board default mux is mixed groups: **P7_1 `LPUART_TX_B` + P2_0 `LPUART_RX_A`**. P2_0 is also **ANA_S8** / LPSPI_SS / OSPI SS.

**P2_0 is not usable as LPUART RX on this DK.** Two failures, same pad:

| | Setup | What happens | Debugger |
|---|---|---|---|
| **A** | P2_0 muxed RX_A, jumper **off**, RX IRQ on | **RX ISR storm.** `uart_irq_rx_ready()` stays true, `fifo_read` returns 0 (`LSR.DR=0`), IIR never clears, thread never times out. | Halt **works** (PC in ISR). |
| **B** | Jumper **P7_1 → P2_0** | **APB lock** on first `uart_poll_out` / THR. | Halt **fails:** `ERROR(TAD9-NAL64) Unable to stop Cortex-M55`. HW reset to recover. |

**P7_0 `LPUART_RX_B`:** no storm. Jumper P7_1↔P7_0 PASSes MCR + wire + IRQ to 10 Mbps (160) and 15 Mbps (240).

LPUART core is proven (MCR to SCLK/16 with any mux; P7_1 LA TX OK). Same driver as UART0.

**SW:** blinky overlay uses P7_0. Board DTS `pinctrl_lpuart` still has P2_0 — HW/board should change that.

Excerpt: `UART/2026-09-07_b1_he_lpuart_p2_0_rx_isr_storm_excerpt.txt`

**Ask HW:** analog/CS load on P2_0 at AF2? Mixed A/B allowed? Why floating RX_A keeps IIR RXRDY with empty FIFO? Why SOUT into P2_0 stalls LPUART APB?

---

## 5. One-line conclusions

1. UART0–5 SCLK = PCLK **40→60**. Console 115200 MATCH. Max **2.5 Mbps** both, **3.75 Mbps** turbo.
2. LPUART SCLK = HE **160→240**. Max **10 Mbps** both, **15 Mbps** turbo. Use **P7_1 + P7_0**, not P2_0.
3. IRQ 115200 = **0.09** (wire). LPUART 10 M **7.19 / 7.00**; 15 M turbo **10.79**. Line max still 10/15 Mbps.
4. HFOSC 38.4 MHz unchanged; not this UART’s baud clock.
5. P2_0 LPUART RX_A: ISR storm (jumper off) or TAD9 APB lock (jumper on). Pad/mux, not turbo PLL.
