UART / LPUART console logs (plain text)
======================================

UART log index — Balletto B1 HE  (alif_b1_dk / rtss_he)
Zephyr d58e56f5ef71   sample: samples/basic/blinky + alif_uart_check.c

Turbo (ITCM, -S spark-turbo):
rm -rf build/; west build -p always -b alif_b1_dk/ab1c1f4m51820ph0/rtss_he samples/basic/blinky -S spark-turbo -DCONFIG_FLASH_BASE_ADDRESS=0 -DCONFIG_FLASH_LOAD_OFFSET=0 -DCONFIG_FLASH_SIZE=256;

Normal 160 (ITCM, omit -S spark-turbo):
rm -rf build/; west build -p always -b alif_b1_dk/ab1c1f4m51820ph0/rtss_he samples/basic/blinky -DCONFIG_FLASH_BASE_ADDRESS=0 -DCONFIG_FLASH_LOAD_OFFSET=0 -DCONFIG_FLASH_SIZE=256;

Jumpers: UART0 P8_1<->P8_0, LPUART P7_1<->P7_0 (not P2_0)
Host must stay 115200. Do not jumper UART2 P5_3.

File                                                            What
----                                                            ----
2026-09-07_b1_he_uart_lpuart_normal160_p7_0.txt                 HE 160, jumpers on
                                                                PASS to 2.5 M (UART0) and 10 M (LPUART)
                                                                IRQ 2.5 / 3.75 / 10 M: UART0 1.70; LPUART 1.95 / 2.87 / 7.19

2026-09-07_b1_he_uart_lpuart_turbo240_p7_0.txt                  HE 240 (-S spark-turbo)
                                                                PASS to 3.75 M (UART0) and 15 M (LPUART)
                                                                IRQ 2.5 / 3.75 / 10 / 15 M: UART0 1.74 / 2.55; LPUART 1.96 / 2.92 / 7.00 / 10.79

2026-09-07_b1_he_lpuart_p2_0_rx_isr_storm_excerpt.txt           P2_0 RX_A, jumper OFF, RX IRQ storm
                                                                Trimmed; full million-line ISR dump not stored

Reports
-------
  /uart_spark_turbo_vs_normal_report.md
  /lpuart_p2_0_hw_report.md
