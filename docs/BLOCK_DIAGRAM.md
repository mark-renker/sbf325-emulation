# Block diagram

## Audio path (per channel; A and B identical, FLANGER I uses A only)

```
 in ──▶ input trim ──▶ (FL I: + B input) ──▶ ⊕ ──▶ NE570 compressor (2:1) ──▶ 2× up-sample
                                             ▲                                   │
                                   feedback  │                                   ▼
                                   (FLANGER  │                 anti-alias LPF (analog, modal)
                                    modes)   │                                   │
                                             │                                   ▼
                                             │        BBD: 1024 stages, variable clock f_clk
                                             │        (sample at ticks, hold between ticks)
                                             │                                   │
                                             │                                   ▼
                                             │               reconstruction LPF (analog, modal)
                                             │                                   │
                                             │                                   ▼
                                             │                  NE570 expander (1:2) ──▶ ± PHASE
                                             │                                   │
                                             └────────── × FEEDBACK (≤ 0.95) ◀───┤
                                                                                 ▼
                                                                       2× down-sample  = wet
 dry (delayed 4 samples) ───────────────────────────────────────────────▶ ⊕ ◀── wet
                                                                          │
                                                   FLANGER III cross-mix: │ out A += −0.5·wet B
                                                                          │ out B += −0.5·wet A
                                                                          ▼
                                                            soft clip ──▶ output trim ──▶ out
```

## Control path

```
 RATE ──▶ triangle LFO (0.02–5 Hz) ──┐
 EXT CV (sidechain / parameter) ─────┤ replaces LFO when enabled
                                     ▼
 DEPTH ──▶ control = (1−D)·MANUAL + D·(½ + ½·mod)      (CH‑B MOD = INV negates mod for channel B)
                                     ▼
 MANUAL (ms → control via the VCO law) ──▶ clock VCO: f_clk = f_min + control·(f_max − f_min)
                                     ▼
                       f_clk ∈ [34.1 kHz … 1.024 MHz] (FLANGER) / [34.1 … 102.4 kHz] (CHORUS)
                       delay = 1024 / (2·f_clk)
```

## Modes

| Mode | Lines | Output A | Output B |
|---|---|---|---|
| OFF | — | dry A | dry B |
| FLANGER I | A (input A + B) | dry + wet A | same as A |
| FLANGER II | A, B | dry A + wet A | dry B + wet B |
| FLANGER III | A, B | dry A + wet A + cross·wet B | dry B + wet B + cross·wet A |
| CHORUS | A, B (5–15 ms, no feedback) | dry A + wet A | dry B + wet B |

## Implementation notes

* The BBD is a 512-sample ring sampled at the clock; the anti-alias and reconstruction filters are
  continuous-time Butterworth sections decomposed into first-order modes and advanced analytically
  between clock ticks (Raffel & Smith, DAFx 2010). No host-rate resampling of the clock takes place.
* The island between the up- and down-samplers runs at 2× host rate so the linear interpolation used to
  drive the anti-alias filter has its images ≥ 80 kHz away (then −58 dB from the filter), and so the
  compander and clipping nonlinearities are oversampled.
* MANUAL, DEPTH, FEEDBACK and the trims are smoothed with a 20 ms one-pole; the LFO is not smoothed.
