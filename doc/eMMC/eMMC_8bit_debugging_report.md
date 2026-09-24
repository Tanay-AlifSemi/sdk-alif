# SD / eMMC — Ensemble DevKit-E8

Personal bring-up notes. One file: hardware, pinmux, Zephyr, Linux commands, test cases, and every pad experiment.

**Board:** `alif_e8_dk` / AE822FA0E5597xx0, RTSS-HP
**Host:** Synopsys DWC SDHC `sdhc@48102000` (Linux `48102000.mmc`)
**Card:** SD-to-eMMC converter using SwissBit eMMC on SD card slot; 122159104 × 512 B ≈ 59648 MB
**Date:** Sep 2026

---

## Summary

DevKit-E8 eMMC: 4-bit is stable at 50 MHz (3.3 V and 1.8 V). 8-bit is not stable. D4–D7 have no level translator on the SD-slot converter, and the same software does not pass every boot.

4-bit (`-S alif-emmc`) works at 50 MHz at both 3.3 V and 1.8 V. D0–D3, CMD, and CLK go through the translator on the SD slot.

8-bit is a lab tested result only. The SD-to-eMMC converter in the SD slot level-shifts D0–D3, CMD, and CLK. D4–D7 (P8_4–P8_7) have no translator on that board. The SoC pads are 1.8 V and the card is 3.3 V, so those four lines are the limit. The same image does not pass every run.

What we tried:

1. **E8, no extra translator on D4–D7**
   - 3.3 V: 400 kHz and 12.5 MHz (mostly) pass. 25 MHz does not. At 12.5 MHz constant bytes often match, but toggling data and FatFS hit DATA CRC. 25 MHz fails `0xF0`.
   - 1.8 V: 400 kHz passes. 12.5 MHz was not tested. 25 MHz and above do not pass. On the logic analyzer, D0–D3 were 1.8 V and D4–D7 were about 2.2 V.
2. **E7 board used as the D4–D7 translator.** Not stable. Dropped the test. The translator on E7 is not a good hardware design to continue with.
3. **External TI TXB0108 on D4–D7**, short soldered wires, 3.3 V at the card. Best point is pad `0x29` (pull-up, 4 mA, slow slew) at 25 MHz: 4 of 5 boots passed. The same image does not pass every run. One boot failed a single `0xF0` write. 50 MHz failed. The link is still flaky. The hardware setup is still not proper for 8-bit.

There is a Kingston eMMC, 8-bit, mounted on an Eagle Engineering board. Not tried. Others reported that it is not even detecting properly.

As of now, the supported path remains 4-bit through the slot translator.

---

## About

| Section | What it is |
|---------|------------|
| [Summary](#summary) | JIRA result: 4-bit supported, 8-bit not stable |
| [1. Status](#1-status) | What works today |
| [2. Hardware](#2-hardware) | Slot, converter, D0–D3 vs D4–D7, voltage |
| [3. Pinmux](#3-pinmux) | P8_4–P8_7, pad byte, why not P5_4–P5_7 |
| [4. Zephyr](#4-zephyr) | Snippets, software path, build, how to read a log |
| [5. Test cases](#5-test-cases) | Every FAT and RAW pattern, and what a fail means |
| [6. What we tried](#6-what-we-tried) | Timeline, side-by-side pad comparison, TXB0108 |
| [7. Linux](#7-linux) | Iota commands, 4-bit vs 8-bit proof |
| [8. CMSIS](#8-cmsis) | Why 8-bit is still blocked there |
| [9. Files](#9-files) | Where the code lives |

Customer docs stay **4-bit only** (`-S alif-emmc`). This file is the 8-bit lab record. Do not copy the 8-bit section into the product app note.

---

## 1. Status

| Stack | Width | Clock | I/O | Result |
|-------|-------|-------|-----|--------|
| Zephyr `-S alif-emmc` | 4-bit | HS (up to 50 MHz) | 3.3 V with `no-1-8-v`, else Zephyr may switch to 1.8 V | Supported path. D0–D3 go through the board SD translator. |
| Zephyr `-S alif-emmc-8bit`, **no** extra shifter on D4–D7, pad **`0x41`** | 8-bit | **400 kHz** | 3.3 V (`no-1-8-v`, `PWR=0x0f`) | FAT + RAW **full pass**. |
| Same, 12.5 MHz or 25 MHz | 8-bit | 12.5 / 25 MHz | 3.3 V | Init OK. Constant bytes often pass. Mixed data (`0xA5`, walk, FatFS metadata) **DATA CRC** `0x00200000`. |
| Same + **TI TXB0108** on D4–D7, pad **`0x41`**, **short soldered wires** | 8-bit | **400 kHz** | 3.3 V card, 1.8 V SoC A-side | **FAT + RAW full pass**, including `0xA5`, walkbit, and RAW `0xA5+i`. |
| Same soldered TXB, pad `0x41` | 8-bit | **12.5 MHz** (`CLK=0x080f`) | same | **RAW full pass**, including `0xA5+i`. FAT `0x00` / `0xFF` / `0x0F` / `0xF0` / `0xA5` MATCH. FAT `0x5A`, inc, walkbit, `a5+i` DATA CRC. Some unlinks CRC and are ignored by the test. |
| Same soldered TXB, pad **`0x4f`** (pull-up, 8 mA, fast slew, Schmitt, REN) | 8-bit | 12.5 MHz | same | Not better. Same five FAT MATCH, same four FAT CRC. `0x5A` failed at `fs_write` instead of `fs_sync`. RAW still full pass. |
| Same soldered TXB, pad **`0x49`** (pull-up, 8 mA, **slow** slew, no Schmitt, REN) | 8-bit | 12.5 MHz | same | **FAT WRITE/READ PASSED** and **RAW WRITE/READ PASSED**, including `0x5A`, inc, walkbit, `a5+i`. One unlink CRC before `0xA5` was ignored by the test; that file still read back. |
| Same soldered TXB, pad **`0x49`** | 8-bit | **25 MHz** (`CLK=0x040f`) | same | RAW full pass, including `0xA5+i`. FAT `0x00` `0xFF` `0x0F` `0xF0` `0xA5` walkbit `a5+i` MATCH. FAT `0x5A` and inc fail at `fs_sync`. Unlink CRCs before `0xA5`, `0x5A`, and `a5+i` are ignored. FAT banner FAILED. |
| Same soldered TXB, pad **`0x41`** (High-Z, 8 mA, slow, no Schmitt) | 8-bit | 25 MHz | same | **Marginal, two boots.** Boot 1: RAW pass; FAT `0x00`, `0xFF`, `a5+i` fail at `fs_sync`. Boot 2: **FAT + RAW banners PASSED**, including `0x5A`, inc, walkbit, `a5+i`. One unlink CRC before `0xA5` ignored. Not repeatable yet. |
| Same soldered TXB, pad **`0x21`** (High-Z, **4 mA**, slow, no Schmitt) | 8-bit | 25 MHz | same | RAW full pass. FAT `0x00` `0xFF` `0x0F` `0xF0` `0xA5` inc walkbit `a5+i` MATCH. FAT `0x5A` fails at **`fs_write`** (`p5a.dat` left size 0). Worse than the `0x41` pass. |
| Same soldered TXB, pad **`0x69`** (pull-up, **12 mA**, slow, no Schmitt) | 8-bit | 25 MHz | same | RAW full pass. FAT only `0xFF` and `0x0F` MATCH. `0x00`, `0xF0`, `0x5A`, inc, `a5+i` fail at `fs_write`. `0xA5` and walkbit fail at `fs_sync`. Worst 25 MHz pad so far. |
| Same soldered TXB, pad **`0x29`** (pull-up, **4 mA**, slow, no Schmitt) | 8-bit | 25 MHz | same | **Five boots, same image.** Boot 1: FAT + RAW passed, one ignored unlink CRC. Boot 2: RAW passed; FAT `0xF0` failed at `fs_write`. Boots 3, 4, and 5: **FAT + RAW passed with no DATA CRC**. 4 of 5 banner passes, 3 of 5 fully clean. |
| Same soldered TXB, pad **`0x29`** | 8-bit | **50 MHz** (`CLK=0x020f`) | same | RAW full pass, including `0xF0` and `0xA5+i`. FAT only `a5+i` MATCH. `0x00` `0xFF` `0x0F` `0xF0` `0xA5` `0x5A` inc walkbit fail at **`fs_write`**. Those files left at size 0. |
| Same soldered TXB, pad **`0x49`** (pull-up, **8 mA**, slow, no Schmitt) | 8-bit | 50 MHz | same | Worse than `0x29`. FAT only `0x00` and `0xFF` MATCH. `0x0F` `0xF0` `0xA5` `0x5A` inc walkbit `a5+i` fail at `fs_write`. RAW `0x0F` also fails `disk_write`; `0xF0` `0xFF` `0xA5+i` MATCH. |
| Same soldered TXB, pad **`0x69`** (pull-up, **12 mA**, slow, no Schmitt) | 8-bit | 50 MHz | same | RAW full pass again. FAT `0x00` `0xFF` `0x0F` MATCH. `0xF0` fails at `fs_sync` (file size 512, not read back). `0xA5` `0x5A` inc walkbit `a5+i` fail at `fs_write`. Better than 8 mA, still not a pass. |
| Same TXB, **long jumpers**, pad `0x41` | 8-bit | 400 kHz | same | RAW constants MATCH. RAW `0xA5+i` and all FatFS writes CRC. |
| TXB + internal pull-up pad **`0x2b`** | 8-bit | 400 kHz | same | Worse: FAT and RAW all CRC. `dat47=0x00`. |
| Linux (Iota) | 8-bit in `ios` | 25 MHz HS | 3.3 V | `ios` says 8-bit. Sector `0xF0` did **not** stick. Same pattern **did** stick in 4-bit after D4–D7 were removed. |
| CMSIS MCI | 4-bit OK | ~25 MHz HS | 3.3 V | 8-bit still fails (pinmux / bus-width). |

**Conclusion (soldered TXB0108 on D4–D7):** keep **pad `0x29`** (pull-up, 4 mA, slow slew, no Schmitt) and **25 MHz** (`CLK=0x040f`, `no-1-8-v`). Five boots: 4 passed the banners, 3 had no DATA CRC, 1 failed a single `0xF0` write. 50 MHz on this pad failed most FatFS writes. 12 mA and 8 mA did not fix 50 MHz. Confirm UART prints `pad=0x29` and `max_hz=25000000`.

---

## 2. Hardware

DevKit-E8 has an **SD slot**, not a soldered eMMC. The lab card is an **SD-to-eMMC converter** in that slot.

| Net | SoC pin | On the board | Notes |
|-----|---------|--------------|--------|
| DAT0–DAT3, CMD, CLK | P5_0–P5_3, P7_0, P7_1 | Through the SD slot **NVT4857** (or equivalent) | Level-shifted. This is why 4-bit works at speed. |
| DAT4–DAT7 | **P8_4–P8_7** `SD_D4_C`–`SD_D7_C` | **No onboard translator.** 1.8 V pad, flying wires to the converter. | eMMC DAT Vih at 3.3 V is about **2.3 V**. A 1.8 V “1” is marginal. Fine at 400 kHz, CRC at 12.5 MHz and above. |
| Reset / VMMC | GPIO **P14_7** | `regulator-fixed` `sd_pwr`, active high | |
| VQMMC select | GPIO **P6_3** | `regulator-gpio` `sd_vsel`: 1.8 V when 0, 3.3 V when 1 | **P6_3 is also UART2 RTS.** Do not use UART2 with SD voltage switching. |
| Card detect | none for eMMC | Overlay deletes `cd-gpios` | Host uses PSTATE. |

`no-1-8-v` keeps host signaling at 3.3 V (`PWR=0x0f`, log `sig_v=1`). Without it, Zephyr switches to 1.8 V when CMD1 OCR says the card can do 1.8 V. Linux stayed at 3.3 V.

**Do not** use an E7 board as a D4–D7 translator. That path CRC’d EXT_CSD and left D6 around 1.5 V.

**TXB0108** on D4–D7 only (soldered, short wires, Sep 23):

- Auto-direction, push-pull. DC output is about **4 kΩ**, plus a ~10 ns one-shot. TI does not recommend it for SD/MMC (they point at TXS0206 / NVT4857). It still passed the 400 kHz pattern suite once the long jumpers were replaced with short soldered wires.
- VCCA = SoC **1.8 V**, VCCB = eMMC **3.3 V**, VCCA ≤ VCCB, OE tied to VCCA.
- **No pull-ups** on either side (or only > 50 kΩ). An SoC pull-up fights the 4 kΩ driver. Pad `0x2b` CRC-failed every sector.

---

## 3. Pinmux

8-bit failed first because D4–D7 were not muxed to the pins that are actually wired. The DWC block supports 8-bit (`CAP1` 8-bit bit is set).

### 3.1 Pins that match the board (and the Linux image that enumerated 8-bit)

```
pinctrl_sdhci: sdhci0grp {
    pinmux = <
        PIN_P5_0__SD_D0_A       0x23
        PIN_P5_1__SD_D1_A       0x23
        PIN_P5_2__SD_D2_A       0x23
        PIN_P5_3__SD_D3_A       0x23

        PIN_P8_4__SD_D4_C       0x29 // check other pinpad combinations also
        PIN_P8_5__SD_D5_C       0x29
        PIN_P8_6__SD_D6_C       0x29
        PIN_P8_7__SD_D7_C       0x29

        PIN_P7_0__SD_CMD_A      0x23
        PIN_P7_1__SD_CLK_A      0x23
    >;
};
```

Zephyr splits that:

- Board `pinctrl_sdmmc` group0: D0–D3 / CMD / CLK, pad **`0x23`**.
- `-S alif-emmc-8bit` group1: D4–D7, pad **`0x29`** (the lab point to keep, with a soldered TXB0108, at 25 MHz).

Stock `linux_alif` DevKit-E8 DTS often lists only D0–D3. Trust the **wired** mux above, not that incomplete DTS.

### 3.2 Do not use P5_4–P5_7

Group-A `SD_D4_A`…`SD_D7_A` exist on the SoC. They are a **different** ball group. On this kit DAT4–DAT7 are port 8, group C.

| Signal | Correct | Wrong |
|--------|---------|--------|
| DAT0 | P5_0 `SD_D0_A` func 7 | |
| DAT1 | P5_1 `SD_D1_A` func 7 | |
| DAT2 | P5_2 `SD_D2_A` func 7 | |
| DAT3 | P5_3 `SD_D3_A` func 6 | |
| DAT4 | **P8_4 `SD_D4_C` func 5** | P5_4 `SD_D4_A` func 6 |
| DAT5 | **P8_5 `SD_D5_C` func 5** | P5_5 `SD_D5_A` func 5 |
| DAT6 | **P8_6 `SD_D6_C` func 5** | P5_6 `SD_D6_A` func 4 |
| DAT7 | **P8_7 `SD_D7_C` func 5** | P5_7 `SD_D7_A` func 5 |
| CMD | P7_0 `SD_CMD_A` func 6 | |
| CLK | P7_1 `SD_CLK_A` func 6 | |

P5_4–P5_7 also alias OSPI1 / PDM / CDC / ETH. CMSIS Conductor left those pads on PDM/OSPI, so an 8-bit CMD6 had no DAT[7:4] on the device.

Unused SD groups on this SoC (do not use unless the schematic says so): P6 `SD_Dx_D`, P13 `SD_Dx_B`, P8_0–P8_3 `SD_D0_C`–`SD_D3_C`.

### 3.3 Pad byte (bits 23:16)

From `pinctrl_alif.c`, one word per pin at `0x1A603000 + pin_index * 4`. The pad lives in bits 23:16. UART prints that byte as `pad=0xNN`.

```
bit:  23  22  21  20  19  18  17  16
name: DR  E2  E1  P2  P1  SR  ST  REN
```

| Field | Values |
|-------|--------|
| REN (16) | 1 = input enabled. Required on DAT and CMD. |
| ST (17) | Schmitt. 1 = on. |
| SR (18) | 0 = slow slew, 1 = fast slew. |
| P2:P1 (20:19) | Driver state when the output is off: `00` High-Z, `01` pull-up, `10` pull-down, `11` keeper. Zephyr `driver-state-control`. |
| E2:E1 (22:21) | Drive: `00` 2 mA, `01` 4 mA, `10` 8 mA, `11` 12 mA. Zephyr `drive-strength` (mA). |
| DR (23) | Driver type. 0 on all of these runs. |

| Pad | REN | SMT | Slew | Pull | Drive | Zephyr properties |
|-----|-----|-----|------|------|-------|-------------------|
| **`0x23`** | 1 | 1 | slow | High-Z | **4 mA** | `read-enable=1`, `schmitt-enable=1`, `drive-strength=4` |
| `0x41` | 1 | 0 | slow | High-Z | **8 mA** | `read-enable=1`, `schmitt-enable=0`, `slew-rate=0`, `driver-state-control=0`, `drive-strength=8` |
| `0x21` | 1 | 0 | slow | High-Z | **4 mA** | `driver-state-control=0`, `drive-strength=4` |
| **`0x29`** | 1 | 0 | slow | **pull-up** | **4 mA** | `driver-state-control=1`, `drive-strength=4`. Lab point to keep. |
| `0x49` | 1 | 0 | slow | **pull-up** | 8 mA | `driver-state-control=1`, `drive-strength=8` |
| `0x69` | 1 | 0 | slow | pull-up | **12 mA** | `driver-state-control=1`, `drive-strength=12` |
| `0x6d` | 1 | 0 | **fast** | pull-up | 12 mA | `slew-rate=1`, `driver-state-control=1`, `drive-strength=12` |
| `0x2b` | 1 | **1** | slow | pull-up | **4 mA** | `schmitt-enable=1`, `driver-state-control=1`, `drive-strength=4` |

`0x23` and `0x41` are **not** the same electrical setting. D0–D3 / CMD / CLK stay `0x23` (matches Linux, and those nets already have the NVT4857). Only D4–D7 were swept.

Verified dump when the 8-bit overlay is actually flashed:

```
P5_0 raw=0x00230007  func=7  pad=0x23   SD_D0_A
P5_1 raw=0x00230007  func=7  pad=0x23   SD_D1_A
P5_2 raw=0x00230007  func=7  pad=0x23   SD_D2_A
P5_3 raw=0x00230006  func=6  pad=0x23   SD_D3_A
P7_0 raw=0x00230006  func=6  pad=0x23   SD_CMD_A
P7_1 raw=0x00230006  func=6  pad=0x23   SD_CLK_A
P8_4 raw=0x00290005  func=5  pad=0x29   SD_D4_C
P8_5 raw=0x00290005  func=5  pad=0x29   SD_D5_C
P8_6 raw=0x00290005  func=5  pad=0x29   SD_D6_C
P8_7 raw=0x00290005  func=5  pad=0x29   SD_D7_C
```

If P8_4–P8_7 are not `func=5`, the host can still SWITCH to 8-bit and then CRC every data phase.

### 3.4 PSTATE

After 8-bit is up and DAT[7:4] idle high: **`PSTATE 0x03ff00f0`**. The `0xf0` is DAT[7:4].
Before width switch, or if those pins are not SD: **`0x03ff0000`**, log `dat47=0x00`.

`dat47=0x00` **at pre-init** is normal (1-bit, D4–D7 not in the transfer yet). It is a problem only if it stays `0x00` **after** `set_io width=8`. With TXB and a pull-up, pre-init stayed `0x00` and writes all CRC’d — the A-side was held low.

---

## 4. Zephyr

### 4.1 Snippets

| Snippet | Disk | Compatible | Width | Mount |
|---------|------|------------|-------|-------|
| `-S alif-emmc` | `SD2` | `zephyr,mmc-disk` | 4 | `/SD2:` |
| `-S alif-emmc-8bit` | `SD2` | `zephyr,mmc-disk` | 8 (includes the 4-bit overlay, then D4–D7) | `/SD2:` |
| `-S alif-sdmmc` | `SD` | `zephyr,sdmmc-disk` | 4, **SD card** path | `/SD:` |

Pass **one** snippet. `alif-emmc-8bit` is DevKit-E8 only. E7 stays 4-bit.

`fs_sample` uses `"SD2"` when `CONFIG_DISK_DRIVER_MMC`. Building with `-S alif-sdmmc` never runs the MMC 8-bit path.

### 4.2 What was wrong before pinmux was fixed

1. Host and `mmc` child `bus-width` stayed 4, so `mmc_set_bus_width()` never sent an 8-bit CMD6. Both `host_caps.bus_8_bit_support` and `card->bus_width == 8` are required.
2. `pinctrl_sdmmc` only muxed D0–D3 / CMD / CLK. The wrong guess was P5_4–P5_7.

### 4.3 Software path

```
fs_sample
  → disk_access_init("SD2")
  → drivers/disk/mmc_subsys.c
  → sd_init()                      # SDIO, then SD, then MMC
      SD CMD8 times out on eMMC    # expected
      → mmc_card_init()
          CMD1, optional 1.8 V
          CMD2 / CMD3 / CMD9 / CMD7
          mmc_set_bus_width()      # CMD6 EXT_CSD[183]
          MMC CMD8 SEND_EXT_CSD    # this one is a data read, not SD CMD8
          mmc_set_hs_timing()
  → sdhc_dwc_set_io()              # HOST_CTRL1, clock divider
```

CMD6 bus-width argument (seen in logs): **`0x03b70200`** or **`0x03b70201`**. Byte 183 value **2** is 8-bit SDR. 4-bit is value 1 (`0x03b70100`).

| `HOST_CTRL1` | Meaning |
|--------------|---------|
| `0x10` | ADMA, 1-bit (identify) |
| `0x16` | 4-bit + HS |
| `0x30` | ADMA + bit 5 EXT data width = **8-bit**, still slow clock |
| `0x34` | **8-bit + HS** |

8-bit uses EXT bit 5, not “4-bit bit plus EXT”.

### 4.4 Clock

SDCLK = 200 MHz / (2 × divider).

| `CLK_CTRL` | Clock |
|------------|--------|
| `0xfa0f` | 400 kHz (identify, and the only 8-bit rate that fully passed) |
| `0x080f` | 12.5 MHz |
| `0x040f` | 25 MHz |
| `0x020f` | 50 MHz (`MMC_CLOCK_52MHZ` request rounds here; DT max is 50 MHz) |

This is MMC High Speed, not HS200. `sdhc_dwc_get_host_props()` does not set `hs200_support`.

### 4.5 CMD8 errors

Two different CMD8s:

| Which | Log | Meaning |
|-------|-----|---------|
| **SD** `SEND_IF_COND` | `CMD 0x081a ARG 0x000001aa`, event **`0x00010000`**, repeated, then `Card does not support CMD8, assuming legacy card` | Expected on eMMC. Not an 8-bit failure. |
| **MMC** `SEND_EXT_CSD` | Later, succeeds, `Card block count is 122159104` | Real card data read. If this CRC’s, the bus is broken. |

`0x00010000` = command timeout.
`0x00200000` = **DATA CRC** on the transfer (`XFR complete error`). Writes then return `-5` (`EIO`).
`CMD: 0x371a ARG: 0x00020000` after a CRC is CMD13 status to RCA `0x00020000`. `RSP01: 0x00000900` is the card R1 with an error bit set. `XFER: 0x0003` was a write with DMA. `adma_err=0x00` means ADMA itself did not fault — the card rejected the CRC.

### 4.6 Build

Sample that prints the patterns: `alif/samples/subsys/fs/fs_sample/` (local copy; upstream `zephyr/samples/.../fs_sample` is the stock demo).

```bash
west build -p always -b alif_e8_dk/ae822fa0e5597xx0/rtss_hp \
  alif/samples/subsys/fs/fs_sample/ \
  -S alif-emmc-8bit \
  -DCONFIG_FLASH_BASE_ADDRESS=0 \
  -DCONFIG_FLASH_LOAD_OFFSET=0 \
  -DCONFIG_FLASH_SIZE=256
python3 /home/tanay/pad_16bytes.py -b build/zephyr/zephyr.bin
```

4-bit: same command with `-S alif-emmc`.

Always check the first pad lines. A stale image has shown up as `pad=0x69` when `0x49` was what the tree contained.

### 4.7 How to tell 4-bit vs 8-bit from a log

| Evidence | 4-bit | 8-bit |
|----------|-------|-------|
| CMD6 BUS_WIDTH | `0x03b701xx` or absent | **`0x03b702xx`** |
| `HOST_CTRL1` after switch | `0x16` class | **`0x34`** (or `0x30` before HS) |
| PSTATE after width | DAT[7:4] not high | **`0x03ff00f0`** |
| P8_4–P8_7 | not SD | **func 5**, pad whatever you programmed |
| Linux `ios` `bus width` | `2 (4 bits)` | **`3 (8 bits)`** |
| EXT_CSD `bus_width` | 1 | **2** |

Good 8-bit init at 400 kHz / 3.3 V looks like:

```
ext_csd bus_width=0 ... hc_bus_io=1     # still 1-bit
CMD6 width arg=0x03b70201 ret=0 r1=0x00000800 want_io=8
set_io width=8 HC1=0x34 PSTATE=0x03ff00f0
ext_csd bus_width=2 hs_timing=1 ... hc_bus_io=8
io-check HC1=0x34 PWR=0x0f CLK=0xfa0f dt_bw=8 max_hz=400000
```

Mount and a directory list can succeed on reads of old data even when **new writes** CRC. Trust `RESULT` lines, not “Disk mounted”.

---

## 5. Test cases

App: `alif/samples/subsys/fs/fs_sample/src/main.c`.
Buffers are in `CONFIG_SD_BUFFER_SECTION` (`.alif_ns`), 32-byte aligned, because the SDHC DMA cannot use every SRAM region.

Order on every boot:

1. Pad + PSTATE dump (`dbgff42d9 K`).
2. `disk_access` init (this is where SD CMD8 times out, then MMC init, 8-bit CMD6).
3. Mount `/SD2:`, unmount, remount, **list the directory** (read-only; can pass when writes are dead).
4. FatFS pattern files.
5. List again.
6. Unmount.
7. Raw sector tests at **LBA 131072** (64 MiB). Does not touch the FAT. Ends by writing zeros back to that LBA.

### 5.1 FatFS — 512-byte files

Each case: unlink if the name exists, `FS_O_CREATE|FS_O_RDWR`, write 512 bytes, `fs_sync`, close, reopen, read 512, compare. A DATA CRC on the **unlink** or the **directory update** fails the case before the payload is the interesting part. That is why a log can say `failed to unlink` / `file open error` / `fs_write got -5` on a pattern that would have been electrically easy.

| Result tag | File | Bytes | What it stresses |
|------------|------|-------|------------------|
| `FAT 0x00` | `p00.dat` | 512 × `0x00` | Almost no DAT edges. Write CRC here means the write path is broken for any data (pull-up fight, shifter stuck low, contact). |
| `FAT 0xFF` | `pff.dat` | 512 × `0xFF` | All eight DAT lines high for the payload. Few edges. |
| `FAT 0x0F D0-D3` | `p0f.dat` | 512 × `0x0F` | Only **DAT[3:0]** carry ones. DAT[7:4] stay 0 in the payload. If this passes and `0xF0` fails, D4–D7 are the bad nets. |
| `FAT 0xF0 D4-D7` | `pf0.dat` | 512 × `0xF0` | Only **DAT[7:4]** carry ones. The D4–D7 test. |
| `FAT 0xA5` | `pa5.dat` | 512 × `0xA5` (`10100101`) | Edge on every bit, all eight lines. |
| `FAT 0x5A` | `p5a.dat` | 512 × `0x5A` (`01011010`) | Same, inverted. |
| `FAT inc` | `pinc.dat` | `byte[i] = i` | Walking values, all lines, long run of transitions. |
| `FAT walkbit` | `pwalk.dat` | `byte[i] = 1 << (i % 8)` | One bit high per byte, cycles D0…D7. Shows a single bad line if the compare prints `first[n]`. |
| `FAT a5+i 512` | `rwtest.dat` | `byte[i] = 0xA5 + i` | Same idea as RAW `0xA5+i`, but through FatFS (extra FAT/dir writes). |

`MATCH` means all 512 bytes read back equal. `FAIL fs_write got -5` means the CMD24 never completed (almost always `0x00200000`).

Files left on the card from older passes (`some.dat` size 1024, `some/`, pattern files size 512) are **previous good writes**. After a CRC run those sizes go to **0** because unlink/create updated the directory entry and the data write died.

### 5.2 Raw — one sector, no FatFS

`disk_access_write` / `read` of **one** 512-byte sector at **LBA 131072**. Same LBA every pattern (each overwrites the last). Then the sector is zeroed. This is the electrical test: no FAT, no directory.

| Result tag | Payload | What a fail means |
|------------|---------|-------------------|
| `RAW 0x0F D0-D3` | 512 × `0x0F` | D0–D3 path (NVT4857 / slot) is bad, or the whole write machine is wedged. |
| `RAW 0xF0 D4-D7` | 512 × `0xF0` | D4–D7 did not deliver a stable 1. Classic no-shifter / bad-shifter symptom. |
| `RAW 0xFF` | 512 × `0xFF` | All lines high. Passes when levels are DC-ok and edges are rare. |
| `RAW 0xA5+i` | `byte[i] = 0xA5 + i` | Edges on every line. Failed on TXB0108 with long jumpers. **MATCH** after the TXB was soldered with short wires (400 kHz, pad `0x41`). |

Linux uses the same LBA (`dd ... seek=131072`). Do not use LBA 0. That is the FAT boot sector (`mount` then returns -19 / ENODEV).

### 5.3 How to read a mixed result

| RAW 0x0F | RAW 0xF0 | RAW 0xFF | RAW A5+i | FAT | Reading |
|----------|----------|----------|----------|-----|---------|
| MATCH | MATCH | MATCH | MATCH | MATCH | 8-bit data path is good **at this clock and pad**. Seen at 400 kHz pad `0x41` (no shifter, and again with soldered TXB), at 12.5 MHz pad `0x49` with soldered TXB, and at **25 MHz pad `0x29`** with soldered TXB (4 of 5 boots). |
| MATCH | FAIL | FAIL or MATCH | FAIL | CRC | DAT[7:4] not making a valid 1. Seen with no shifter at 12.5–25 MHz, and with TXB when the A-side was held low. |
| MATCH | MATCH | MATCH | **FAIL** | all CRC | Levels can sit at 0 and 1 (constants), edges fail. **TXB0108 + long jumpers**, 400 kHz, pad `0x41`. FatFS metadata toggles, so it fails even when `0xF0` matches. Short soldered wires cleared this row. |
| all FAIL | all FAIL | all FAIL | FAIL | all CRC | Pad or wiring is fighting the bus. **TXB + `0x2b` pull-up.** |

Constant patterns are a weak test. A pass on `0x00` / `0xFF` / `0x0F` / `0xF0` does **not** mean 8-bit is done. `0xA5+i` and a FatFS create/unlink are the bar.

---

## 6. What we tried

### 6.1 Timeline

1. **Linux `ios`:** clock 25 MHz, bus width `3 (8 bits)`, timing `mmc high-speed`, signal voltage 3.30 V. Throughput on large `dd` was in the right range for 8-bit × 25 MHz (order of 10–20 MB/s), which only proves the link can move a lot of data, not that every DAT line is clean.
2. **Zephyr pinmux** copied to P8_4–P8_7 `SD_Dx_C` pad `0x41`, D0–D3/CMD/CLK pad `0x23`, `bus-width = <8>`. Init reached `HC1=0x34`, EXT_CSD `bus_width=2`, FatFS mounted `/SD2:` and listed old files. Early boots also switched to **1.8 V** (`pwr_ctrl=0x0b`) and still mounted. That mount was not the pattern suite.
3. **Pattern tests (Linux and Zephyr)** split D0–D3 from D4–D7. `0x0F` could pass while `0xF0` did not stick (`od` stayed `0000`, or Zephyr DATA CRC `0x00200000`). Same card in **4-bit** (D4–D7 disconnected, `ios` bus width 2) stored `0xF0` (`od` all `f0f0`). So the controller, FAT, and D0–D3 translator were fine. DAT[7:4] were not.
4. **Forced 3.3 V** with `no-1-8-v` (`PWR=0x0f`). Did not fix D4–D7: those balls are still 1.8 V I/O with no translator. The slot translator only covers D0–D3/CMD/CLK.
5. **Clock sweep, pad `0x41`, no extra shifter.** 400 kHz: FAT + RAW full pass. 12.5 MHz: CRC on mixed / unlink / `A5+i`; constants often still MATCH. 25 MHz: `0xF0` CRC. Dropping back to 400 kHz passed again, so it was not a dead contact.
6. **Pad sweep on D4–D7** (table below), still no shifter, mostly at 12.5 MHz because 400 kHz already passed on `0x41`. Pull-up, 12 mA, and fast slew did not make 12.5 MHz reliable. Fast slew was worse.
7. **TI TXB0108** on D4–D7 only, 400 kHz, **long jumpers**. With pad `0x41`: RAW constants including `0xF0` MATCH, `A5+i` and FatFS CRC. With internal pull-up + Schmitt + 4 mA (`0x2b`): everything CRC, `dat47=0x00`. Pull-up removed; `0x41` restored the constant-RAW pass.
8. **Same TXB, short soldered wires**, still pad `0x41` / 400 kHz / `no-1-8-v`. **FAT + RAW full pass** (`0x00`, `0xFF`, `0x0F`, `0xF0`, `0xA5`, `0x5A`, inc, walkbit, `a5+i`, and RAW `0xA5+i`). The long jumpers were the miss, not the pad byte. Pre-init `dat47=0x00` is still normal; after `set_io width=8`, `PSTATE=0x03ff00f0`.
9. **Same soldered TXB at 12.5 MHz** (`max-bus-freq = <12500000>`, `CLK=0x080f`, `pad=0x41`, `HC1=0x34`, `PWR=0x0f`). RAW `0x0F` / `0xF0` / `0xFF` / `0xA5+i` all MATCH. FAT payloads `0x00`, `0xFF`, `0x0F`, `0xF0`, `0xA5` MATCH. FAT `0x5A` fails at `fs_sync`, `inc` and `a5+i` fail at `fs_write`, `walkbit` fails at `fs_sync`. `fat_one()` ignores `fs_unlink` errors, so a CRC on unlink can be followed by a MATCH on the next file. Bus recovers: RAW still passes after the FAT CRCs. Marginal at 12.5 MHz on `0x41`.
10. **Soldered TXB, pad sweep at 12.5 / 25 / 50 MHz.** Pull-up 8 mA (`0x49`) made 12.5 MHz a full pass. At 25 MHz the best pad was **`0x29`** (pull-up, 4 mA, slow, no Schmitt): 4 of 5 boots passed FAT + RAW, 3 with no DATA CRC. 8 mA and 12 mA were worse. 50 MHz failed on `0x29`, `0x49`, and `0x69`. **Keep `0x29` at 25 MHz.** 4-bit `-S alif-emmc` stays the product path.

### 6.2 Side-by-side results

Same layout as a two-pad compare: one row per pattern, one column per pad. `MATCH` is all 512 bytes. A fail names the call that returned the DATA CRC. `CRC` means the log recorded a DATA CRC without naming `fs_write` vs `fs_sync`. `—` means that pattern was not split out in the log.

Soldered TXB0108, short wires, `no-1-8-v`, slow slew, no Schmitt, unless the column says otherwise.

**50 MHz** (`CLK=0x020f`). None of these is a pass.

| | 0x29 4 mA pull-up | 0x49 8 mA pull-up | 0x69 12 mA pull-up |
|--|--|--|--|
| FAT 0x00 | fail fs_write | MATCH | MATCH |
| FAT 0xFF | fail fs_write | MATCH | MATCH |
| FAT 0x0F | fail fs_write | fail fs_write | MATCH |
| FAT 0xF0 | fail fs_write | fail fs_write | fail fs_sync |
| FAT 0xA5 | fail fs_write | fail fs_write | fail fs_write |
| FAT 0x5A | fail fs_write | fail fs_write | fail fs_write |
| FAT inc | fail fs_write | fail fs_write | fail fs_write |
| FAT walkbit | fail fs_write | fail fs_write | fail fs_write |
| FAT a5+i | MATCH | fail fs_write | fail fs_write |
| RAW 0x0F | MATCH | fail disk_write | MATCH |
| RAW 0xF0 | MATCH | MATCH | MATCH |
| RAW 0xFF | MATCH | MATCH | MATCH |
| RAW 0xA5+i | MATCH | MATCH | MATCH |

**25 MHz** (`CLK=0x040f`). `0x29` is the lab point: 4 of 5 boots passed every row. The `0xF0` cell is the one boot that missed.

| | 0x29 4 mA pull-up | 0x21 4 mA High-Z | 0x41 8 mA High-Z b1 | 0x41 8 mA High-Z b2 | 0x49 8 mA pull-up | 0x69 12 mA pull-up |
|--|--|--|--|--|--|--|
| FAT 0x00 | MATCH | MATCH | fail fs_sync | MATCH | MATCH | fail fs_write |
| FAT 0xFF | MATCH | MATCH | fail fs_sync | MATCH | MATCH | MATCH |
| FAT 0x0F | MATCH | MATCH | MATCH | MATCH | MATCH | MATCH |
| FAT 0xF0 | MATCH (1/5 fail fs_write) | MATCH | MATCH | MATCH | MATCH | fail fs_write |
| FAT 0xA5 | MATCH | MATCH | MATCH | MATCH | MATCH | fail fs_sync |
| FAT 0x5A | MATCH | fail fs_write | MATCH | MATCH | fail fs_sync | fail fs_write |
| FAT inc | MATCH | MATCH | MATCH | MATCH | fail fs_sync | fail fs_write |
| FAT walkbit | MATCH | MATCH | MATCH | MATCH | MATCH | fail fs_sync |
| FAT a5+i | MATCH | MATCH | fail fs_sync | MATCH | MATCH | fail fs_write |
| RAW 0x0F | MATCH | MATCH | MATCH | MATCH | MATCH | MATCH |
| RAW 0xF0 | MATCH | MATCH | MATCH | MATCH | MATCH | MATCH |
| RAW 0xFF | MATCH | MATCH | MATCH | MATCH | MATCH | MATCH |
| RAW 0xA5+i | MATCH | MATCH | MATCH | MATCH | MATCH | MATCH |

**12.5 MHz** (`CLK=0x080f`), soldered TXB. `0x4f` is pull-up, 8 mA, fast slew, Schmitt.

| | 0x41 8 mA High-Z | 0x4f 8 mA pull-up fast+Schmitt | 0x49 8 mA pull-up |
|--|--|--|--|
| FAT 0x00 | MATCH | MATCH | MATCH |
| FAT 0xFF | MATCH | MATCH | MATCH |
| FAT 0x0F | MATCH | MATCH | MATCH |
| FAT 0xF0 | MATCH | MATCH | MATCH |
| FAT 0xA5 | MATCH | MATCH | MATCH |
| FAT 0x5A | fail fs_sync | fail fs_write | MATCH |
| FAT inc | fail fs_write | CRC | MATCH |
| FAT walkbit | fail fs_sync | CRC | MATCH |
| FAT a5+i | fail fs_write | CRC | MATCH |
| RAW 0x0F | MATCH | MATCH | MATCH |
| RAW 0xF0 | MATCH | MATCH | MATCH |
| RAW 0xFF | MATCH | MATCH | MATCH |
| RAW 0xA5+i | MATCH | MATCH | MATCH |

**400 kHz** (`CLK=0xfa0f`).

| | 0x41 soldered TXB | 0x41 long jumpers | 0x2b long jumpers pull-up+Schmitt | 0x41 no shifter | 0x6d no shifter 12 mA fast |
|--|--|--|--|--|--|
| FAT 0x00 | MATCH | CRC | CRC | MATCH | MATCH |
| FAT 0xFF | MATCH | CRC | CRC | MATCH | MATCH |
| FAT 0x0F | MATCH | CRC | CRC | MATCH | MATCH |
| FAT 0xF0 | MATCH | CRC | CRC | MATCH | MATCH |
| FAT 0xA5 | MATCH | CRC | CRC | MATCH | MATCH |
| FAT 0x5A | MATCH | CRC | CRC | MATCH | MATCH |
| FAT inc | MATCH | CRC | CRC | MATCH | MATCH |
| FAT walkbit | MATCH | CRC | CRC | MATCH | MATCH |
| FAT a5+i | MATCH | CRC | CRC | MATCH | MATCH |
| RAW 0x0F | MATCH | MATCH | CRC | MATCH | MATCH |
| RAW 0xF0 | MATCH | MATCH | CRC | MATCH | MATCH |
| RAW 0xFF | MATCH | MATCH | CRC | MATCH | MATCH |
| RAW 0xA5+i | MATCH | CRC | CRC | MATCH | MATCH |

**No shifter, above 400 kHz.** These logs were not split per file the way the TXB runs were.

| | 0x41 12.5 MHz | 0x6d 12.5 MHz | 0x69 12.5 MHz | 0x49 12.5 MHz | 0x41 25 MHz |
|--|--|--|--|--|--|
| FAT 0x00 | often MATCH | CRC | often MATCH | — | — |
| FAT 0xFF | often MATCH | CRC | often MATCH | — | — |
| FAT 0x0F | often MATCH | CRC | often MATCH | — | — |
| FAT 0xF0 | often MATCH | CRC | often MATCH | — | CRC |
| FAT 0xA5 | CRC | CRC | CRC | CRC | — |
| FAT 0x5A | — | CRC | — | CRC | — |
| FAT inc | — | CRC | — | CRC | — |
| FAT walkbit | CRC | CRC | — | CRC | — |
| FAT a5+i | CRC | CRC | — | CRC | — |
| RAW 0x0F | often MATCH | often MATCH | — | — | — |
| RAW 0xF0 | often MATCH | often MATCH | — | — | CRC |
| RAW 0xFF | often MATCH | often MATCH | — | — | — |
| RAW 0xA5+i | CRC | CRC | CRC | — | — |

### 6.3 D4–D7 pad log

All rows are 8-bit, `no-1-8-v` (3.3 V at the card), P8_4–P8_7 func 5. D0–D3 / CMD / CLK stayed pad `0x23`.

| Pad | Pull-up | Drive | Slew | Schmitt | Clock | Extra HW | What happened |
|-----|---------|-------|------|---------|-------|----------|----------------|
| **`0x41`** | no (High-Z) | 8 mA | slow | no | **400 kHz** | none | **FAT + RAW full pass.** Best no-shifter result. |
| `0x41` | no | 8 mA | slow | no | 12.5 MHz | none | Init OK (`CLK=0x080f`, `HC1=0x34`). DATA CRC on mixed patterns, unlink, walkbit, RAW `A5+i`. Constants often still matched. One bad run looked like a loose wire; 400 kHz right after passed. |
| `0x41` | no | 8 mA | slow | no | 25 MHz | none | `0xF0` DATA CRC. |
| `0x6d` | **yes** | **12 mA** | **fast** | no | 400 kHz | none | FAT + RAW passed (same as `0x41` at this clock). |
| `0x6d` | yes | 12 mA | fast | no | 12.5 MHz | none | Worse than slow slew. FAT and `A5+i` CRC. Constant RAW (`0x0F` / `0xF0` / `0xFF`) often still matched. Ringing, not a weak driver. |
| `0x69` | yes | 12 mA | **slow** | no | 12.5 MHz | none | Better than `0x6d`, not clean. File payloads often MATCH (`0x00`, `0xFF`, `0x0F`, `0xF0`). Unlink, `0xA5`, RAW `A5+i` still CRC. |
| `0x49` | yes | **8 mA** | slow | no | 12.5 MHz | none | Still CRC on the high-transition cases. Pull-up did not replace a level shifter. (One UART dump was still `0x69` — that boot was the previous image.) |
| `0x2b` | yes | **4 mA** | slow | **yes** | 400 kHz | **TXB0108** | Regression. FAT and RAW **all** CRC, files left at size 0, pre-init `dat47=0x00`. SoC pull-up vs TXB ~4 kΩ. |
| `0x41` | no | 8 mA | slow | no | 400 kHz | **TXB0108, long jumpers** | RAW `0x0F`, `0xF0`, `0xFF` MATCH. RAW `0xA5+i` CRC. Every FatFS write CRC (`0x00200000` on CMD24). |
| **`0x41`** | **no** | 8 mA | slow | no | 400 kHz | **TXB0108, short soldered wires** | **FAT + RAW full pass**, including walkbit and `0xA5+i`. `HC1=0x34` `PWR=0x0f` `CLK=0xfa0f` `pad=0x41`. First clean soldered-TXB result. |
| `0x41` | no | 8 mA | slow | no | **12.5 MHz** | **TXB0108, short soldered wires** | RAW full pass (`0xA5+i` MATCH). FAT `0x00` `0xFF` `0x0F` `0xF0` `0xA5` MATCH. FAT `0x5A` / inc / walkbit / `a5+i` CRC (`0x00200000`, `adma_err=0x00`). |
| `0x4f` | yes | 8 mA | **fast** | **yes** | 12.5 MHz | **TXB0108, short soldered wires** | Same FAT split as `0x41` at 12.5 MHz. Fast slew + Schmitt did not help. |
| **`0x49`** | **yes** | 8 mA | slow | no | **12.5 MHz** | **TXB0108, short soldered wires** | **FAT + RAW passed**, including `0x5A`, inc, walkbit, `a5+i`. One ignored unlink CRC before `0xA5`. Best 12.5 MHz result. |
| `0x49` | yes | 8 mA | slow | no | **25 MHz** | **TXB0108, short soldered wires** | RAW full pass. FAT `0x5A` and inc fail at `fs_sync` (`CLK=0x040f`). walkbit and `a5+i` MATCH. Not a 25 MHz pass. |
| `0x41` | no | 8 mA | slow | no | **25 MHz** | **TXB0108, short soldered wires** | Two boots, same image. Boot 1: FAT `0x00` / `0xFF` / `a5+i` fail at `fs_sync`, RAW pass. Boot 2: FAT + RAW banners passed, one ignored unlink CRC before `0xA5`. Marginal. |
| `0x21` | no | **4 mA** | slow | no | **25 MHz** | **TXB0108, short soldered wires** | RAW pass. FAT `0x5A` fails at `fs_write`. |
| `0x69` | yes | **12 mA** | slow | no | **25 MHz** | **TXB0108, short soldered wires** | RAW pass. Most FAT writes CRC. Worst 25 MHz pad. |
| **`0x29`** | **yes** | **4 mA** | slow | no | **25 MHz** | **TXB0108, short soldered wires** | **Lab point to keep.** Five boots: 4 banner passes, 3 with no DATA CRC. One boot failed a single FAT `0xF0` write. `CLK=0x040f` `pad=0x29` `HC1=0x34` `PWR=0x0f`. |
| `0x29` | yes | 4 mA | slow | no | **50 MHz** | **TXB0108, short soldered wires** | RAW pass. FAT only `a5+i` MATCH. Most files left size 0. |
| `0x49` | yes | 8 mA | slow | no | **50 MHz** | **TXB0108, short soldered wires** | Worse. FAT mostly CRC. RAW `0x0F` also fails `disk_write`. |
| `0x69` | yes | 12 mA | slow | no | **50 MHz** | **TXB0108, short soldered wires** | RAW pass. FAT `0x00` `0xFF` `0x0F` MATCH. Rest CRC. More current did not reach the 25 MHz result. |

Zephyr knobs that produce those bytes (D4–D7 group only):

| Goal | `driver-state-control` | `drive-strength` | `slew-rate` | `schmitt-enable` | `read-enable` |
|------|------------------------|------------------|-------------|------------------|---------------|
| `0x41` | `<0>` High-Z | `<8>` | `<0>` slow | `<0>` | `<1>` |
| `0x21` | `<0>` | `<4>` | `<0>` | `<0>` | `<1>` |
| **`0x29` (keep this)** | `<1>` pull-up | `<4>` | `<0>` | `<0>` | `<1>` |
| `0x49` | `<1>` pull-up | `<8>` | `<0>` | `<0>` | `<1>` |
| `0x69` | `<1>` | `<12>` | `<0>` | `<0>` | `<1>` |
| `0x6d` | `<1>` | `<12>` | `<1>` fast | `<0>` | `<1>` |
| `0x2b` | `<1>` | `<4>` | `<0>` | `<1>` | `<1>` |

### 6.4 What the pad sweep ruled out

- **Not** a missing 8-bit CMD6. EXT_CSD comes back `bus_width=2`, `HC1=0x34`.
- **Not** FatFS. RAW `0xF0` fails or passes the same way with no filesystem.
- **Not** “need more current”. 12 mA and fast slew were worse at 12.5 MHz than 8 mA slow.
- **Not** “DAT[7:4] need a strong SoC pull-up”. At 400 kHz with no TXB, High-Z `0x41` already passed. With the TXB and **long jumpers**, pull-up + Schmitt (`0x2b`) stuck `dat47` at 0. With **short soldered wires**, a weak pull-up at 4 mA (`0x29`) was the best 25 MHz pad. 8 mA and 12 mA were worse.
- **Is** the D4–D7 wiring. With no shifter, 400 kHz passes and 12.5 MHz does not. With TXB0108, long jumpers passed only constant sectors; short soldered wires passed the full 400 kHz suite, including toggling data.

### 6.5 TXB0108 wiring

- A-side = P8_4–P8_7 (1.8 V). B-side = eMMC DAT4–DAT7 (3.3 V).
- OE to VCCA. **Short soldered wires** (the long-jumper build failed `0xA5+i` and FatFS).
- Overlay to keep: pad **`0x29`** (pull-up, 4 mA, slow slew, no Schmitt), `no-1-8-v`, **25 MHz**.
- **25 MHz is the lab rate** (`CLK=0x040f`): 4 of 5 boots passed FAT + RAW. One boot failed a single `0xF0` write.
- **50 MHz** (`CLK=0x020f`) on this pad failed most FatFS writes. 8 mA and 12 mA did not fix it.
- 4-bit through the slot NVT4857 (`-S alif-emmc`) stays the supported product path. Do not put 8-bit in customer docs.

### 6.6 Console output: E8 + external level translator

DevKit-E8, soldered TXB0108 on D4–D7, pad `0x29`, 25 MHz, `no-1-8-v`. This is a clean boot (FAT WRITE/READ PASSED and RAW WRITE/READ PASSED, no DATA CRC). The `CMD: 0x081a` errors are SD CMD8; eMMC does not answer them.

```
[00:00:00.011,000] <dbg> fs: fs_register: fs register 0: 0
*** Booting Zephyr OS build a738b2f3b23f ***
[00:00:00.020,000] <inf> main: dbgff42d9 K pre-init PSTATE=0x03ff0000 dat47=0x00
[00:00:00.028,000] <inf> main: dbgff42d9 K P8_4 raw=0x00290005 func=5 pad=0x29
[00:00:00.036,000] <inf> main: dbgff42d9 K P8_5 raw=0x00290005 func=5 pad=0x29
[00:00:00.044,000] <inf> main: dbgff42d9 K P8_6 raw=0x00290005 func=5 pad=0x29
[00:00:00.051,000] <inf> main: dbgff42d9 K P8_7 raw=0x00290005 func=5 pad=0x29
[00:00:00.059,000] <inf> sdhc_dwc: dbgff42d9 L set_io width=1 HC1=0x10 PSTATE=0x03ff0000
[00:00:01.080,000] <err> sdhc_dwc: CMD error event: 0x00010000
[00:00:01.087,000] <err> sdhc_dwc: CMD: 0x081a ARG: 0x000001aa XFER: 0x0000 RSP01: 0x00000000 PSTATE: 0x03ff0000, cc:0
[00:00:01.099,000] <err> sdhc_dwc: CMD error event: 0x00010000
[00:00:01.105,000] <err> sdhc_dwc: CMD: 0x081a ARG: 0x000001aa XFER: 0x0000 RSP01: 0x00000000 PSTATE: 0x03ff0000, cc:0
[00:00:01.117,000] <err> sdhc_dwc: CMD error event: 0x00010000
[00:00:01.123,000] <err> sdhc_dwc: CMD: 0x081a ARG: 0x000001aa XFER: 0x0000 RSP01: 0x00000000 PSTATE: 0x03ff0000, cc:0
[00:00:01.135,000] <err> sdhc_dwc: CMD error event: 0x00010000
[00:00:01.141,000] <err> sdhc_dwc: CMD: 0x081a ARG: 0x000001aa XFER: 0x0000 RSP01: 0x00000000 PSTATE: 0x03ff0000, cc:0
[00:00:01.153,000] <err> sdhc_dwc: CMD error event: 0x00010000
[00:00:01.160,000] <err> sdhc_dwc: CMD: 0x081a ARG: 0x000001aa XFER: 0x0000 RSP01: 0x00000000 PSTATE: 0x03ff0000, cc:0
[00:00:01.172,000] <err> sdhc_dwc: CMD error event: 0x00010000
[00:00:01.178,000] <err> sdhc_dwc: CMD: 0x081a ARG: 0x000001aa XFER: 0x0000 RSP01: 0x00000000 PSTATE: 0x03ff0000, cc:0
[00:00:01.190,000] <err> sdhc_dwc: CMD error event: 0x00010000
[00:00:01.196,000] <err> sdhc_dwc: CMD: 0x081a ARG: 0x000001aa XFER: 0x0000 RSP01: 0x00000000 PSTATE: 0x03ff0000, cc:0
[00:00:01.208,000] <err> sdhc_dwc: CMD error event: 0x00010000
[00:00:01.214,000] <err> sdhc_dwc: CMD: 0x081a ARG: 0x000001aa XFER: 0x0000 RSP01: 0x00000000 PSTATE: 0x03ff0000, cc:0
[00:00:01.226,000] <err> sdhc_dwc: CMD error event: 0x00010000
[00:00:01.233,000] <err> sdhc_dwc: CMD: 0x081a ARG: 0x000001aa XFER: 0x0000 RSP01: 0x00000000 PSTATE: 0x03ff0000, cc:0
[00:00:01.244,000] <err> sdhc_dwc: CMD error event: 0x00010000
[00:00:01.251,000] <err> sdhc_dwc: CMD: 0x081a ARG: 0x000001aa XFER: 0x0000 RSP01: 0x00000000 PSTATE: 0x03ff0000, cc:0
[00:00:01.263,000] <err> sdhc_dwc: CMD error event: 0x00010000
[00:00:01.269,000] <err> sdhc_dwc: CMD: 0x081a ARG: 0x000001aa XFER: 0x0000 RSP01: 0x00000000 PSTATE: 0x03ff0000, cc:0
[00:00:01.280,000] <inf> sd: Card does not support CMD8, assuming legacy card
[00:00:01.372,000] <inf> sd: CID decoding not supported for MMC
[00:00:01.379,000] <inf> sd: Using Legacy MMC will have slow initialization
[00:00:01.424,000] <inf> sd: dbgff42d9 C ext_csd bus_width=0 hs_timing=0 hs52=1 hs26=1 host_bw=8 hc_bus_io=1
[00:00:01.434,000] <inf> sd: Card block count is 122159104, block size is 512
[00:00:01.444,000] <inf> sd: dbgff42d9 C hs clock=25000000 Hz sig_v=1
[00:00:01.450,000] <inf> sd: dbgff42d9 L CMD6 width arg=0x03b70201 ret=0 r1=0x00000800 want_io=8
[00:00:01.459,000] <inf> sdhc_dwc: dbgff42d9 L set_io width=8 HC1=0x34 PSTATE=0x03ff00f0
[00:00:01.494,000] <inf> sd: dbgff42d9 C ext_csd bus_width=2 hs_timing=1 hs52=1 hs26=1 host_bw=8 hc_bus_io=8
[00:00:01.504,000] <inf> sd: Card block count is 122159104, block size is 512
[00:00:01.512,000] <inf> main: Block count 122159104
Sector size 512
Memory Size(MB) 59648
[00:00:01.521,000] <inf> main: dbgff42d9 C io-check HC1=0x34 PWR=0x0f CLK=0x040f dt_bw=8 max_hz=25000000
SDHC HC1=0x34 CLK=0x040f bus-width=8 max-bus-freq=25000000 Hz
[00:00:01.537,000] <dbg> fs: fs_mount: fs mounted at /SD2:
Disk mounted.
[00:00:01.544,000] <dbg> fs: fs_unmount: fs unmounted from /SD2:
[00:00:01.551,000] <dbg> fs: fs_mount: fs mounted at /SD2:

Listing dir /SD2: ...
[FILE] some.dat (size = 1024)
[DIR ] some
[FILE] p00.dat (size = 512)
[FILE] pff.dat (size = 512)
[FILE] p0f.dat (size = 512)
[FILE] pf0.dat (size = 512)
[FILE] pa5.dat (size = 0)
[FILE] p5a.dat (size = 0)
[FILE] pinc.dat (size = 0)
[FILE] pwalk.dat (size = 0)
[FILE] rwtest.dat (size = 512)

E8-only 8-bit tests (no E7 translator)

--- FatFS tests (512-byte files) ---
RESULT FAT 0x00 MATCH w[0]=0x00 r[0]=0x00 (512 bytes)
RESULT FAT 0xFF MATCH w[0]=0xff r[0]=0xff (512 bytes)
RESULT FAT 0x0F D0-D3 MATCH w[0]=0x0f r[0]=0x0f (512 bytes)
RESULT FAT 0xF0 D4-D7 MATCH w[0]=0xf0 r[0]=0xf0 (512 bytes)
RESULT FAT 0xA5 MATCH w[0]=0xa5 r[0]=0xa5 (512 bytes)
RESULT FAT 0x5A MATCH w[0]=0x5a r[0]=0x5a (512 bytes)
RESULT FAT inc MATCH w[0]=0x00 r[0]=0x00 (512 bytes)
RESULT FAT walkbit MATCH w[0]=0x01 r[0]=0x01 (512 bytes)
RESULT FAT a5+i 512 MATCH w[0]=0xa5 r[0]=0xa5 (512 bytes)
FAT WRITE/READ PASSED

Listing dir /SD2: ...
[FILE] some.dat (size = 1024)
[DIR ] some
[FILE] p00.dat (size = 512)
[FILE] pff.dat (size = 512)
[FILE] p0f.dat (size = 512)
[FILE] pf0.dat (size = 512)
[FILE] pa5.dat (size = 512)
[FILE] p5a.dat (size = 512)
[FILE] pinc.dat (size = 512)
[FILE] pwalk.dat (size = 512)
[FILE] rwtest.dat (size = 512)
[00:00:01.757,000] <dbg> fs: fs_unmount: fs unmounted from /SD2:

--- Raw disk tests: CMD24 512-byte sector at LBA 131072 (no FatFS) ---
RESULT RAW 0x0F D0-D3 MATCH w[0]=0x0f r[0]=0x0f (512 bytes)
RESULT RAW 0xF0 D4-D7 MATCH w[0]=0xf0 r[0]=0xf0 (512 bytes)
RESULT RAW 0xFF MATCH w[0]=0xff r[0]=0xff (512 bytes)
RESULT RAW 0xA5+i MATCH w[0]=0xa5 r[0]=0xa5 (512 bytes)
RAW WRITE/READ PASSED
```

---

## 7. Linux

Iota Tiny Linux on DevKit-E8. Device node `/dev/mmcblk0`. Safe write offset: sector **131072** = 64 MiB. Never write LBA 0.

Iota limits:

- Root is read-only. `/tmp` must be tmpfs (gone after reboot).
- No `cmp`, `hexdump`, or `mkfs.vfat`.
- BusyBox `od`: use `od -x` (not `-An` / `-tx1`).
- Type **one** line, wait for `root@devkit-e8:~#`, then the next.
- Do **not** `echo 1 > /proc/sys/kernel/printk` (that hides I/O errors). Use `4`.
- Do **not** format `/dev/sda` in GNOME Disks on the PC. That is the PC SSD.

### 7.1 After every reboot

```sh
echo 4 > /proc/sys/kernel/printk
mount -t tmpfs tmpfs /tmp
mkdir -p /tmp/debug
mount -t debugfs none /tmp/debug
cat /tmp/debug/mmc0/ios
```

`bus width` in that file:

- `3 (8 bits)` — 8-bit test (D4–D7 connected).
- `2 (4 bits)` — 4-bit test (D4–D7 removed, then reboot).

If `ios` is missing, the mounts above were skipped. Do not `mount -t debugfs … /sys` (that hides sysfs). If debugfs is not in the image, `/sys/class/mmc_host/mmc0/ios` is the other place (field names differ slightly: `bus width: 3 (8 bits)`, `signal voltage: 0 (3.30 V)`).

Example of a live 8-bit `ios`:

```
clock:          25000000 Hz
vdd:            21 (3.3 ~ 3.4 V)
bus width:      3 (8 bits)
timing spec:    1 (mmc high-speed)
signal voltage: 0 (3.30 V)
```

`SDHCI: ... ERR=0` means the **command** was accepted, not that the data CRC was good.

### 7.2 8-bit sector test (D4–D7 connected, ios must say 8 bits)

What we saw: zeros can land; **0xF0 does not** (after `sync` + drop_caches, `od` is still `0000`). `dd` can return 0 because of the page cache. The read after drop_caches is the real result.

```sh
dd if=/dev/zero of=/dev/mmcblk0 bs=512 seek=131072 count=1
echo $?

dd if=/dev/zero bs=512 count=1 | tr '\0' '\360' > /tmp/f0.bin
dd if=/tmp/f0.bin of=/dev/mmcblk0 bs=512 seek=131072 count=1
echo $?

sync
echo 3 > /proc/sys/vm/drop_caches
dd if=/dev/mmcblk0 of=/tmp/rd.bin bs=512 skip=131072 count=1
echo $?
od -x /tmp/rd.bin
```

8-bit fail we captured:

```
0000000     0000    0000    0000    ...
*
0001000
```

`dd: /dev/mmcblk0: I/O error` on that sector is the same failure as Zephyr `0x00200000`.

### 7.3 4-bit proof (D4–D7 removed, power off, boot)

`ios` **must** say `2 (4 bits)`. If it still says 3, Linux is still in 8-bit — stop.

Same `dd` / `sync` / `drop_caches` / `od` as above.

4-bit pass we captured: every `echo $?` is 0, and

```
0000000     f0f0    f0f0    f0f0    ...
*
0001000
```

### 7.4 Other patterns (Iota has no `cmp`)

```sh
# 0x0F = D0-D3
i=0; : > /tmp/p0f.bin
while [ $i -lt 512 ]; do printf '\017' >> /tmp/p0f.bin; i=$((i+1)); done

# 0xF0 = D4-D7
i=0; : > /tmp/pf0.bin
while [ $i -lt 512 ]; do printf '\360' >> /tmp/pf0.bin; i=$((i+1)); done

# 0xA5
i=0; : > /tmp/pa5.bin
while [ $i -lt 512 ]; do printf '\245' >> /tmp/pa5.bin; i=$((i+1)); done

dd if=/dev/urandom of=/tmp/prand.bin bs=512 count=1
```

Write one file the same way as `/tmp/f0.bin` (`dd` + `sync` + `drop_caches` + `od`).

### 7.5 Dynamic debug

Do not paste these while typing other commands. SDHCI spam overruns the UART.

```sh
echo 'module sdhci -p' > /proc/dynamic_debug/control
echo 'module mmc_core -p' > /proc/dynamic_debug/control
echo 'module mmc_block -p' > /proc/dynamic_debug/control
echo 'func mmc_mrq_pr_debug -p' > /proc/dynamic_debug/control
```

Iota `echo` keeps a literal `\n` if you write `-p\n`. `printf` into that file prints nothing on the UART; that is success. This kernel may still print SDHCI lines after reboot (`CONFIG_MMC_DEBUG`). Quiet errors-only: `echo 4 > /proc/sys/kernel/printk`.

### 7.6 FAT format — 4-bit only

Confirm `ios` is 4-bit first. Iota often has no `mkfs`:

```sh
which mkfs.vfat mkfs.fat mkfs.msdos
busybox --list | grep mkfs
```

If `mkfs.vfat` exists:

```sh
mkfs.vfat -F 32 -n EMMC /dev/mmcblk0
# or, if it says the device is partitioned:
mkfs.vfat -I -F 32 -n EMMC /dev/mmcblk0
sync
```

If `mkfs` is missing, format from Zephyr 4-bit with D4–D7 still unplugged:

```bash
west build -p always \
  -b alif_e8_dk/ae822fa0e5597xx0/rtss_hp \
  alif/samples/subsys/fs/fs_sample \
  -S alif-emmc \
  -DCONFIG_FS_FATFS_MOUNT_MKFS=y \
  -DCONFIG_FLASH_BASE_ADDRESS=0 \
  -DCONFIG_FLASH_LOAD_OFFSET=0 \
  -DCONFIG_FLASH_SIZE=256
python3 /home/tanay/pad_16bytes.py -b build/zephyr/zephyr.bin
```

Rebuild with `MKFS` off after FAT exists once, or the next boot wipes the card.

Do not `dd if=/dev/zero of=/dev/mmcblk0` (that zeros LBA 0 and breaks mount). Do not `mkfs` while `ios` says 8 bits.

### 7.7 Linux results already captured

| Mode | Result |
|------|--------|
| 8-bit, wires on, bus width 3 | Zeros write OK. `0xF0` did not stick (`od` all `0000`). |
| 4-bit, D4–D7 off, bus width 2 | Zeros OK, `0xF0` OK, `od` all `f0f0`. |

Same bug as Zephyr: 8-bit DAT[7:4] writes. Not FatFS.

---

## 8. CMSIS

DFP `Driver_MCI` + DevKit-E8 `pins.h` / Conductor pinmux. 4-bit works. 8-bit does not, for the same two reasons Zephyr had before the overlay:

1. D4–D7 not on `P8_4–P8_7 SD_Dx_C` pad `0x41` (left on PDM/OSPI, or pointed at P5_4–P5_7).
2. Host `HOST_CTRL1` bit 5 and CMD6 `EXT_CSD[183]=2` both have to happen.

Until that mux matches Linux, keep CMSIS at 4-bit / 3.3 V / ~25 MHz HS. Do not start with HS200.

---

## 9. Files

| Role | Path |
|------|------|
| 4-bit pinmux | `zephyr/boards/alif/common/ensemble-e4-e6-e8-pinctrl.dtsi` (`pinctrl_sdmmc` group0) |
| Pin macros | `zephyr/include/zephyr/dt-bindings/pinctrl/ensemble-e4-e6-e8-pinctrl.h` |
| Pad bit layout | `zephyr/drivers/pinctrl/pinctrl_alif.c` |
| E8 4-bit overlay | `alif/samples/subsys/fs/fs_sample/snippets/alif-emmc/alif_ensemble_e8_dk.overlay` |
| E8 8-bit overlay (the one we edit) | `alif/samples/subsys/fs/fs_sample/snippets/alif-emmc-8bit/alif_ensemble_e8_dk.overlay` |
| Twin under zephyr/samples | `zephyr/samples/subsys/fs/fs_sample/snippets/alif-emmc-8bit/` (keep in sync if that tree is built) |
| Host driver | `zephyr/drivers/sdhc/sdhc_dwc.c` |
| MMC protocol | `zephyr/subsys/sd/mmc.c` |
| Disk | `zephyr/drivers/disk/mmc_subsys.c` |
| SoC SDHC | `zephyr/dts/arm/alif/ensemble/common/e1.dtsi` |
| VSEL / reset | `zephyr/boards/alif/common/alif-sd-vsel.dtsi` |
| Pattern test app | `alif/samples/subsys/fs/fs_sample/src/main.c` |

**One line:** With a soldered TXB0108 on D4–D7, the 8-bit lab point to keep is **pad `0x29`** (pull-up, 4 mA, slow slew, no Schmitt) at **25 MHz**, 3.3 V, `no-1-8-v`. Five boots: 4 of 5 passed FatFS and raw. 50 MHz failed. 4-bit through the slot translator remains the supported product path.
