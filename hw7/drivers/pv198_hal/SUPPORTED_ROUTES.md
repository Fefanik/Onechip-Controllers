# Implemented HAL routes

I2C and SPI port numbers name the corresponding hardware peripheral. Each port
has one fixed board pin route, documented in the public enum; pins are therefore
not part of either configuration structure. The public facade selects and binds
an internal backend driver for the requested peripheral.

UART retains its pin-configured routing because that API has not yet been
converted to fixed peripheral ports.

LP_FLEXCOMM4 is intentionally omitted because the board debug console owns it.
LP_FLEXCOMM8 is omitted because its useful pin group is wired to the on-board
external memory.
An LP_FLEXCOMM instance may be used by only one of UART, I2C, or SPI at a time,
even when different pins expose that instance.

## UART and I2C pins

| Instance | RX / SDA | TX / SCL |
|---|---|---|
| 0 | P0_12 | P0_13 |
| 1 | P0_24 | P0_25 |
| 2 | P4_0 | P4_1 |
| 3 | P1_0 | P1_1 |
| 5 | P1_16 | P1_17 |
| 6 | P3_20 | P3_21 |
| 7 | P3_2 | P3_3 |
| 9 | P2_4 | P2_3 |

For UART, the first pin is RX and the second is TX. For I2C, the first is SDA
and the second is SCL; the instance number is the `I2C_PORT_n` value.

## SPI

| Instance | MISO | MOSI | SCK | slave CS |
|---|---|---|---|---|
| 0 | P0_14 | P0_5 | P0_4 | P0_15 |
| 1 | P0_26 | P0_24 | P0_25 | P0_27 |
| 2 | P4_2 | P4_0 | P4_1 | P4_3 |
| 3 | P1_2 | P1_0 | P1_1 | P1_3 |
| 5 | P1_18 | P1_16 | P1_17 | P1_19 |
| 6 | P3_22 | P3_20 | P3_21 | P3_23 |
| 7 | P3_4 | P3_2 | P3_3 | P3_5 |
| 9 | P2_5 | P2_4 | P2_3 | P2_2 |

The instance number is the `SPI_PORT_n` value. SPI master chip select remains
an ordinary GPIO so one controller can address multiple devices. The PCS column
is used by slave mode. LPSPI6 slave mode 3 is rejected because that swapped-pin
route cannot launch its first bit reliably; modes 0–2 are supported.

## ADC, DAC, PWM, and capture

- LPADC accepts 69 package pins across ADC0 and ADC1, including both A-side and
  B-side channels. Up to eight pins may be initialized simultaneously.
- The 12-bit DAC outputs are P4_2/J1[4] for DAC0 and P4_3/J1[2] for DAC1.
  DAC2 uses the analog-only ANA_6/J3[2] pad and is outside the current digital
  `pin_num_t` model.
- PWM accepts an explicit TIMER0–TIMER4 and the corresponding MAT pins in
  timer.h (ALT4). MAT2 on every timer is reserved for the period;
  the other three channels are available, with one alternative pin per channel.
  All 39 output routes across the five timers are supported. Outputs on one
  timer share the tick rate and period configured by timer_init. Pulse widths
  are specified in ticks. PWM/capture channel setup does not initialize or start
  the counter; call timer_start explicitly.
  Pin mux routes follow [NXP's pinmux table](https://www.nxp.com/docs/en/data-sheet/MCXNP184M150F70.pdf)
  (Table 93); new routes still require electrical bench verification.
- Hardware edge capture supports up to four simultaneous inputs per selected timer,
  TIMER0–TIMER4. Input n is fixed to TIMER(n / 4), CAP(n % 4), with alternative
  pins sharing that channel; unsupported timer/pin combinations are rejected.
  Supported pins are P0_0, P0_1, P0_6-P0_9, P0_14, P0_15, P0_20-P0_23,
  P1_0, P1_1, P1_6-P1_9, P1_14-P1_17, P1_30, P1_31, P3_0, P3_1, P3_4,
  P3_5, P3_8, P3_9, P3_14-P3_17, P3_22, P3_23, and P4_0-P4_7. Pins
  exposing the same internal CT_INP signal cannot be captured simultaneously.
  PWM, capture and callback timers cannot use the same timer simultaneously.
  Callback timers accept all five hardware IDs, `TIMER0`–`TIMER4`.

## Bench-tested routes

- UART5: P1_16/P1_17
- I2C3: P1_0/P1_1
- SPI6 master and slave: P3_20–P3_23
- ADC0: P0_23 (SW2)
- PWM: the three on-board RGB LED channels

The additional routes are derived from the MCXN947 pin functions and NXP's
generated FRDM-MCXN947 configurations, but require wiring to bench-test each
physical header route. The hardware-test harness targets capture on P3_22 (TIMER2 CAP2) over
the existing ESP32-S3 SPI MISO wire, but that CTIMER route has not yet been
bench-tested.
