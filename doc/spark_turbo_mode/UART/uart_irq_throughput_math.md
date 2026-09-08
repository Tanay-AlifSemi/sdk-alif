# UART IRQ bench math (internal)

This explains the numbers printed by `alif_uart_check.c` after:

```c
us = cyc_sum * 1000000ULL / hz
```

All examples below are from your real log
`2026-09-07_b1_he_uart_lpuart_normal160_p7_0.txt`
UART0 @ 115200, HE = 160 MHz.

---

## 0. What the bench actually does

One **scored** transfer = send 1024 bytes and receive them back (IRQ + FIFO).

There is **1 warmup** transfer first (not timed). Then the loop runs **10** timed transfers.

```
t0 = k_cycle_get_32()     // CPU cycle counter NOW
dut_irq_xfer(1024 bytes)
t1 = k_cycle_get_32()     // CPU cycle counter AFTER
cyc_sum += (t1 - t0)
```

If all 10 succeed:

| Name | Value |
|---|---|
| `BENCH_BYTES` | 1024 |
| `BENCH_ITERS` / `scored` | 10 |
| `bytes` | 1024 × 10 = **10240** payload bytes |
| `hz` | `sys_clock_hw_cycles_per_sec()` = HE = **160000000** (normal) or **240000000** (turbo) |
| `cyc_sum` | sum of 10 intervals, e.g. **142294097** |
| `actual` | programmed baud, e.g. **115273** |

`cyc_sum` is **not** one transfer. It is 10 transfers added together. `us` is also for those 10 transfers together.

---

## 1. What `hz` and a “cycle” are

The Cortex-M55 SysTick / cycle counter ticks once per CPU clock.

- Normal: CPU = 160 MHz → **160 000 000 ticks in 1 second**
- Turbo: CPU = 240 MHz → **240 000 000 ticks in 1 second**

So:

```
1 second  =  hz  cycles
1 cycle    =  1 / hz   seconds
```

If the counter increased by `N` ticks, the wall time is:

```
time_seconds = N / hz
```

Example: N = 160000000, hz = 160000000 → 1.000 second.

Your bench: N = `cyc_sum` = 142294097, hz = 160000000

```
time_seconds = 142294097 / 160000000 = 0.88933810625 seconds
```

That is already the full answer. The rest of the `us = ...` line only **changes units** from seconds to microseconds, and does it in **integer C**.

---

## 2. Why multiply by 1 000 000? (seconds → microseconds)

We want to print **microseconds**, not seconds.

```
1 second        = 1 000 000 microseconds
1 millisecond   =     1 000 microseconds
```

So:

```
time_us = time_seconds × 1 000 000
        = (cyc_sum / hz) × 1 000 000
        =  cyc_sum × 1 000 000 / hz
```

That last form is the C line:

```c
us = cyc_sum * 1000000ULL / hz
```

**The `1000000` is not a baud thing and not a UART thing.**
It is only: “there are 1 000 000 µs in one second.”

### Same formula with units (so it cannot be mysterious)

```
cyc_sum          cycles
hz               cycles / second
cyc_sum / hz     seconds

× 1 000 000      microseconds / second

(cyc_sum / hz) × 1e6     microseconds
```

---

## 3. Why write `cyc_sum * 1e6 / hz` and not `(cyc_sum / hz) * 1e6`

On the CPU this is **integer** math. There are no fractions until you print.

**Wrong order:**

```c
us = (cyc_sum / hz) * 1000000
```

```
142294097 / 160000000  =  0     because 142294097 < 160000000
0 * 1000000            =  0     WRONG
```

Integer divide truncates toward zero. Any transfer shorter than 1 second would print `us = 0`.

**Right order:** multiply first so the number stays big, then divide.

```c
us = cyc_sum * 1000000 / hz
```

```
142294097 × 1000000 = 142294097000000
142294097000000 / 160000000 = 889338
```

Printed `us = 889338`. That is 0.889338 seconds, in µs.

Check by hand:

```
0.88933810625 s × 1 000 000 = 889338.10625
integer C keeps 889338      (drops the 0.10625 µs)
```

---

## 4. Why `1000000ULL` and `uint64_t cyc_sum`

`uint32_t` max is about 4.29e9.

```
142294097 × 1000000 = 1.42e14     ← does not fit in 32 bits
```

If you multiply in 32-bit, it wraps and `us` is garbage.

So the code uses:

- `cyc_sum` as `uint64_t`
- `1000000ULL` so the multiply is 64-bit

Then it casts back to `uint32_t` for printk. 889338 fits in 32 bits.

---

## 5. Turbo uses a different `hz` — same wall time can look like more cycles

Same 0.89 s of UART traffic:

| Mode | hz | cycles for the same 0.89 s |
|---|---|---|
| 160 | 160e6 | ~142e6 |
| 240 | 240e6 | ~213e6  (1.5× more ticks) |

That is why UART0 115200 in your logs is:

| | cycles | us |
|---|---|---|
| 160 | 142294097 | 889338 |
| 240 | 213568005 | 889866 |

Wall time stayed ~890 ms (the **wire** is the same 115200). Cycles went up 1.5× because the CPU tick is faster. `us = cyc_sum * 1e6 / hz` is what makes 160 and 240 comparable.

Check turbo:

```
213568005 × 1000000 / 240000000
= 213568005000000 / 240000000
= 889866.6875  →  889866 µs
```

---

## 6. `wire_us` — different clock, different meaning

```c
wire_us = bytes * 10 * 1000000 / actual
```

This does **not** use `cyc_sum` or `hz`. It is “how long would 10240 bytes take **on the UART wire** if the line were 100% busy.”

### Why `× 10`

UART mode is **8N1**:

```
1 start bit
8 data bits     ← this is the payload byte
1 stop bit
────────────
10 bits on the wire per byte
```

So bits on the wire = `bytes × 10`.

Baud `actual` = bits per second on the wire.

```
time_seconds = (bytes × 10) / actual
time_us      = (bytes × 10 / actual) × 1 000 000
             = bytes × 10 × 1000000 / actual
```

Your numbers:

```
bytes  = 10240
actual = 115273
wire_us = 10240 × 10 × 1000000 / 115273
        = 102400000000 / 115273
        = 888325.28  →  888325
```

### Compare `us` vs `wire_us`

| | µs | meaning |
|---|---|---|
| `us` | 889338 | stopwatch around the ISR xfer (wire **plus** CPU/FIFO) |
| `wire_us` | 888325 | physics of 8N1 at 115273 baud, nothing else |

889338 / 888325 ≈ 1.001 → at 115200 the CPU is waiting on the UART. That is why we call it **wire-limited**.

At 2.5 Mbps UART0 160:

```
actual  = 2500000
bytes   = 10240
wire_us = 10240 × 10 × 1e6 / 2500000 = 40960

cyc_sum = 7709001
hz      = 160000000
us      = 7709001 × 1e6 / 160000000 = 48181
```

48181 > 40960 → CPU/ISR added extra time. That is why Mbit/s is 1.70, not 2.00.

---

## 7. `kB/s` — payload bytes per second

```c
kBps = bytes * 1000 / us
```

Want kilobytes per second of **payload** (10240 bytes, not 10-bit frames).

```
B/s   = bytes / time_seconds
      = bytes / (us / 1 000 000)
      = bytes × 1 000 000 / us

kB/s  = B/s / 1000
      = bytes × 1000 / us
```

The `1000` here is “convert B/s to kB/s”, not the same 1e6 as in `us`.

```
10240 × 1000 / 889338 = 10240000 / 889338 = 11.51  →  11
```

Integer divide → table shows **11**.

---

## 8. `kbit` and printed `Mbit/s` — payload bits, not line bits

```c
kbit = bytes * 8000 / us
```

Payload bits = `bytes × 8` (not × 10). Start/stop are ignored on purpose so this is **payload Mbit/s**, not UART line rate.

```
bit/s   = (bytes × 8) / time_seconds
        = bytes × 8 × 1 000 000 / us

kbit/s  = bit/s / 1000
        = bytes × 8000 / us
```

```
10240 × 8000 / 889338 = 81920000 / 889338 = 92.11  →  92
```

`kbit` is in **kbit/s**. The printk splits it into `Mbit/s` with two decimals **without floating point**:

```c
kbit / 1000U          // ones of Mbit/s     92/1000 = 0
(kbit / 100U) % 10    // tenths              92/100 = 0, %10 = 0
(kbit / 10U) % 10     // hundredths            92/10  = 9, %10 = 9
```

Prints `0.09`.

Ideal payload at this baud (8 of every 10 bits are data):

```
actual × 0.8 = 115273 × 0.8 = 92218 bit/s = 0.092 Mbit/s
```

Matches 0.09 after integer rounding.

At 2.5 M, ideal payload = 2.5e6 × 0.8 = **2.00 Mbit/s**. Measured 1.70 because `us` > `wire_us`.

---

## 9. One-page cheat sheet

Let

- `N` = `cyc_sum` (CPU ticks for 10 xfers)
- `f` = `hz` (CPU ticks per second)
- `B` = `bytes` (10240)
- `a` = `actual` baud (wire bits/s)

| Want | Formula | Your 115200 / 160 |
|---|---|---|
| time (s) | `N / f` | 0.889338 s |
| time (µs) | `N × 1e6 / f` | **889338** |
| ideal wire (µs) | `B × 10 × 1e6 / a` | **888325** |
| kB/s payload | `B × 1000 / us` | **11** |
| kbit/s payload | `B × 8000 / us` | 92 → **0.09 Mbit/s** |

Only the first two rows use `hz`. UART baud is **not** in `us`.

---

## 10. Common mix-ups

1. **`1000000` is not baud.** It is µs per second.
2. **`hz` is CPU, not UART SCLK.** UART0 SCLK is PCLK 40/60 MHz. `hz` is HE 160/240 MHz.
3. **`us` is measured. `wire_us` is theoretical.** Gap = CPU/ISR.
4. **`Mbit/s` uses ×8, `wire_us` uses ×10.** Different questions.
5. Integer C: always multiply before divide, and use 64-bit for `N × 1e6`.

---

## 11. Code map (`alif_uart_check.c`)

```
hz        = sys_clock_hw_cycles_per_sec()     // HE
actual    = sclk / (16*DL + DLF)               // UART baud
cyc_sum   = sum of (t1-t0) for 10 xfers
bytes     = 1024 * scored

us        = cyc_sum * 1000000 / hz             // stopwatch
wire_us   = bytes * 10 * 1000000 / actual      // 8N1 wire
kBps      = bytes * 1000 / us                  // payload kB/s
kbit      = bytes * 8000 / us                  // payload kbit/s
Mbit/s    = printed from kbit as X.YZ
```
