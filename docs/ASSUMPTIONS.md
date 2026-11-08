# Assumptions and open questions (spec §10)

No reference unit has been measured yet. Everything below is a modelling decision taken from the
published documents, the parts list, and BBD circuit practice. Each one is a runtime tunable
(`src/dsp/Tunables.h`; override with `sbf_render --set key=value` or the plugin's
`~/Library/Application Support/StereoBucketFlanger/tunables.txt`), so a measurement can replace it
without touching code. When hardware captures exist, re-run `measure/run_all.py --reference <dir>`.

| §10 question | Decision | Tunable(s) | Evidence / reasoning |
|---|---|---|---|
| Exact BBD part, one or two per channel | One 1024-stage MN3007 per channel (two per unit) | `bbdStages` | Parts list OCR reads "MN5007" (no such part); reviewers report "dual MN3007". Service notes: clock period 1 µs (0.5 ms) to ~30 µs (15 ms) ⇒ 1024/(2·f_clk) matches 0.5–15 ms exactly. |
| Clock VCO law | Linear f_clk vs control voltage (delay ∝ 1/V) | `vcoLaw` (0 lin / 1 exp) | LF555 timer used as a current-controlled VCO; LFO modulation therefore sweeps delay as 1/x. The MANUAL knob is mapped in ms, so the law only affects how the LFO sweeps around it. |
| DEPTH scaling | Crossfade: control = (1−D)·MANUAL + D·(½ + ½·LFO) | `depthLaw` | Owner's manual: "at 10, MANUAL has no effect (modulation spans the full range)". Additive-and-clamp variant selectable. |
| LFO waveform | Bipolar triangle, 0.02–5 Hz | — | LF555-based integrator; ±peak LEDs. Measured at CP‑2 on hardware to confirm (§8.6). |
| Reconstruction filter | Fixed 18 kHz 4th-order Butterworth; anti-alias 18 kHz 3rd-order | `reconCutoffHz`, `reconOrder`, `aaCutoffHz`, `aaOrder`, `reconTrack` | Fixed filters are the norm in Roland BBD designs of the era; a reviewer notes aliasing at the longest delays, consistent with a fixed filter. `reconTrack > 0` makes the cutoff clock-tracking if the schematic shows it. Cutoff chosen so the wet path is −3 dB at 14 kHz at the fastest clock (see below). |
| Published 75 Hz–14 kHz spec | Applies at the short-delay (fast-clock) end | — | At 15 ms the clock is 34 kHz; the bucket hold (sinc) alone costs 2.6 dB at 14 kHz and the 1:2 expander doubles any delay-line loss, so 14 kHz is unreachable there with any filter. The spec PDF's assumption ("the 14 kHz spec is the long-delay end") is therefore replaced; hardware captures decide. Model prediction: 14.1 kHz at 0.5 ms, 10.2 kHz at 15 ms. |
| NE570 time constants | τ = 22 ms (10 kΩ × 2.2 µF) for attack and release; same for both halves | `compTauMs` | 2.2 µF is the NE570 datasheet's audio reference value; the scan fragment "10/1S NP" could instead mean 10 µF (τ = 100 ms). Measure §8.5. |
| Companding inside the feedback loop | Yes: feedback taps after the expander/phase switch and re-enters before the compressor | — | Spec §3 ("Loop includes the BBD bandwidth and the compander"). |
| Compander unity level | Sine peak 0.158 of full scale (−16 dB re +20 dBm) | `compVref` | Nominal input −16 dBm (H) / +4 dBm (L), 16 dB below the maximum. |
| FLANGER III mix | out A = dry A + wet A − 0.5·wet B (and vice versa) | `fl3Own`, `fl3Cross` | Spec: cross-panning of wet signals; the Dimension-D lineage suggests inverted cross terms. Coefficients and sign must be read from the mix resistors (§8.3). |
| FLANGER I input | A + B summed into line A; both outputs identical | `fl1InputBMix` | Owner's manual: "Monaural … A's output feeds both outputs"; reviewer: "sums the inputs". |
| Feedback ceiling | Loop gain 0.95 at FEEDBACK = 100 % | `fbMax` | Factory trim VR7/VR107 "just before oscillation" over the whole MANUAL range. Peak comb gain is then ≈ +26 dB; hardware value sets this (§8.4). |
| Wet/dry balance | Unity dry + unity wet in all modes | `flangerDry/Wet`, `chorusDry/Wet` | Chorus "different wet/dry balance" is unknown; set from the mix resistors. |
| Clipping | Soft limit at full scale (+20 dBm), knee exponent 4 | `clipLevel`, `clipHardness` | BBD input stage / output op-amp "near +20 dBm". THD sweep informational until hardware THD exists. |
| Noise | White −90 dBFS (−70 dBm) per bucket + clock feedthrough 2e‑5 | `bbdNoiseRms`, `clockFeedthrough` | Published "< −70 dBm" with 0 dBFS ≡ +20 dBm. |
| Level switches | Modelled as 0 / −20 dB pads plus ±6 dB fine trims | — | Spec §2.3; H‑in/L‑out and L‑in/H‑out are unity. |

## Deliberate deviations from the spec text

* **Latency.** The delay-line island runs at 2× the host rate through polyphase IIR half-bands whose
  round trip is 3.97 samples. The dry path is delayed by 4 samples so the comb notches sit exactly where
  the hardware's would (0.5 ms minimum delay is only 24 samples at 48 kHz, so 4 samples would otherwise
  be a 17 % error). The plugin reports 4 samples of latency to the host; algorithmic latency is zero.
  `oversample=0` runs at host rate with zero latency at the cost of the input-side interpolation images.
* **Aliasing gate.** The −80 dBFS alias bar is applied at −12 dBFS input. At 0 dBFS the modelled input
  clipping makes harmonics that the BBD then aliases; the number is reported, not gated, until hardware
  captures show whether the unit does the same.
* **Chorus RATE description.** "CCW lessens effect, CW adds vibrato" is reproduced only through the rate
  itself; no additional wet/dry law is applied (unknown).

## Things that are not modelled

Power supply, footswitch, panel artwork, input/output impedance, BBD charge-transfer tilt beyond a
scalar loss (`bbdLoss`), temperature drift of the clock trimmers.
