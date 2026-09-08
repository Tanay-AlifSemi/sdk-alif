# Balletto B1 DK — LPUART P2_0 RX_A pad issue (SW → HW)

**To:** Hardware / board / pinmux  
**From:** SW (Zephyr ns16550 LPUART on RTSS-HE)  
**Board:** `alif_b1_dk` AB1C1F4M51820PH0  
**Block:** LPUART `uart@43008000` (AON), SCLK = HE (160 / 240 MHz)  
**SW:** Zephyr `d58e56f5ef71`, `uart_ns16550`, blinky UART check  
**Status:** Closed on SW — use **P7_0 `LPUART_RX_B`**. Board default still muxes P2_0.

This is a **P2_0 pad / mux** finding. The LPUART core and **P7_1 TX_B** are proven. P7_0 RX_B does **not** show either failure below.

---

## 1. Conclusion

**P2_0 as `LPUART_RX_A` is not usable as LPUART RX on this DK.** Two distinct failures, same pad:

| # | Setup | What the CPU does | Debugger |
|---|---|---|---|
| **A** | P2_0 muxed RX_A, jumper **off**, `uart_irq_rx_enable` | **RX IRQ storm.** ISR never exits. `uart_irq_rx_ready()` stays true, `uart_fifo_read()` returns 0 (`LSR.DR=0`), RBR never read, IIR never clears. Thread (`k_sem_take` timeout) never runs. | Halt **works**. PC in `dut_irq_cb`. |
| **B** | Jumper **P7_1 TX → P2_0 RX** | **APB lock** on first LPUART register access (`uart_poll_out` / THR). | Halt **fails**: `ERROR(TAD9-NAL64) Unable to stop device Cortex-M55 / Target in wrong state/mode`. Hardware reset to recover. |

**P7_0 `LPUART_RX_B`:** idle mark, no IRQ storm; jumper P7_1↔P7_0 PASSes wire + IRQ through **10 Mbps (160) and 15 Mbps (240)** (reconfirmed 7 Sep 2026, both clocks, jumper on).

SW has moved the blinky overlay to P7_0. Board DTS `pinctrl_lpuart` still uses mixed **P7_1 TX_B + P2_0 RX_A**.

---

## 2. Ask HW

1. On this DK, is P2_0 still loaded as **ANA_S8 / LPSPI_SS / OSPI SS** when SW selects AF2 `LPUART_RX_A`?
2. Why does a **floating** P2_0 RX_A keep IIR RXRDY/CTI asserted with **empty FIFO** (failure A)?
3. Why does **driving SOUT into P2_0** stall the LPUART APB (failure B / TAD9)?
4. Schematic: anything else driving P2_0 (analog mux, CS, camera)?

---

## 3. Pinmux

| Pad | AF2 (UART) | Other relevant | Analog |
|---|---|---|---|
| **P2_0** | **LPUART_RX_A** | OSPI0_SS0_B, LPSPI_SS_A, I2S, LPCAM | **ANA_S8** |
| **P7_0** | **LPUART_RX_B** | OSPI0_SCLK_C, SPI2_MISO_B, I2C0_SDA_C | — |
| **P7_1** | **LPUART_TX_B** | OSPI0_SCLKN_C, SPI2_MOSI_B, I2C0_SCL_C | — |

Board default (`balletto-pinctrl.dtsi` `pinctrl_lpuart`): **P7_1 TX_B + P2_0 RX_A** (mixed groups).

Same-group pairs:

- Group A: P2_1 TX_A + P2_0 RX_A  
- Group B: P7_1 TX_B + P7_0 RX_B  ← **SW working pair**

---

## 4. Runtime evidence (failure A — IRQ storm)

Instrumenting `dut_irq_cb` (`uart_irq_rx_ready` / `uart_fifo_read` / `uart_fifo_fill`):

**UART0 P8_1↔P8_0 (healthy):** first ISRs `tx_i`/`rx_i` advance, `readp` matches, `sem_ok` with `tx_i=rx_i=1024`.

**LPUART P2_0 RX_A, jumper off, 9600 wire IRQ:**

```
irqs on  baud=9600 n=64
isr cb=1  tx_i=0 rx_i=0 txrdy=0 rxrdy=1 fillp=0 readp=0
isr cb=5  ...                         rxrdy=5         readp=0
isr cb=5000 ...                       rxrdy=5000      readp=0
...
isr cb=1140000 ...                    rxrdy=1140000   readp=0
```

No `sem_take`, no `wire_done`. `readp` stayed **0**: driver thought RX was ready, FIFO read returned **0**. CPU never left the ISR.

After SW disabled RX+TX IER on 8 consecutive empty reads, the same setup returned **FAIL(-2)** in ~200–400 ms (`cb=8…12`, `abort=1`, `rx_i=0`) and reached LED blink. That is a **test guard**, not a pad fix.

MCR on-chip with P2_0 still muxed: 115200 / 230400 / 460800 **FAIL(-3)** (dirty SIN into RBR). Other bauds PASS. Pad unused by LOOP=1 still leaks.

---

## 5. Runtime evidence (failure B — jumper on → TAD9)

Jumper **P7_1 → P2_0**, same mux:

- LA / first `uart_poll_out`: M55 lock, ULink `TAD9-NAL64`.
- Reproduced with jumper connected on the HW-repro image (user, 7 Sep 2026).
- Remove jumper: LA completes; debugger can pause again (then failure A if RX IRQ is enabled).

Same SoC, same driver, same first TX at 115200. Only RX pad / jumper changed.

Not turbo-only: seen at **normal 160** and turbo 240.

---

## 6. What SW ruled out

| Item | Why not |
|---|---|
| LPUART IP / clock | MCR loopback PASSes to SCLK/16 (10 M @ 160, 15 M @ 240) |
| P7_1 TX pad | LA bit width OK; P7_0 wire PASSes |
| Driver baud formula | Same ns16550 path as UART0 |
| “Forgot RX drain” | Storm is IIR RXRDY with **empty** FIFO (readp=0), not overrun of good data |
| IRQ vs poll | A is IRQ; B is poll APB. P7_0 works for both |
| Missing jumper as hang | FAIL(-2) is the correct no-RX result. Hang was ISR never returning |

---

## 7. SW workaround

Blinky overlay now muxes **P7_1 `LPUART_TX_B` + P7_0 `LPUART_RX_B`**.

Jumper for wire/throughput: **P7_1 ↔ P7_0**. Do not jumper UART2 P5_3. Do not jumper P2_0.

ISR still aborts after 8 empty `fifo_read`s so a noisy RX cannot lock the CPU again.

Board default `pinctrl_lpuart` should be changed to P7_0 when HW agrees.
