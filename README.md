# 808 Metal

A VCV Rack 2 module: the TR-808 cymbal and hi-hat circuit, modelled stage by stage, with extra controls around the model.

**Credit.** The circuit model is by **mjsyts**, from the post
["TR808 Cymbal Hardware Accurate Modeling"](https://scsynth.org/t/tr808-cymbal-hardware-accurate-modeling/13607)
on scsynth.org (September 2026, SuperCollider). mjsyts worked out the filter poles, zeros and gains from the
TR-808 service-manual schematic. This plugin is a new C++ implementation of that model. The post gives no licence
for its code, so none of it is copied here: only the published circuit values are used.

## Signal flow

six square oscillators -> two bandpass filters (about 3.4 kHz and 7.1 kHz) -> three swing VCAs with their own decays and a soft clip -> three highpass filters -> out

The engine runs at twice the host sample rate so the high band and the resonant 10 kHz highpass stay true to the model.

## Controls

| Section | Control | What it does |
|---|---|---|
| Oscillators | TUNE (+ V/OCT) | moves all six together, ratios kept (±2 octaves) |
| | TRIM A / TRIM B | the 808's two trimmer-tuned oscillators (800 Hz, 540 Hz), over the hardware's ranges |
| | SPREAD | squeezes (left) or stretches (right) the gaps between the six |
| | WIDTH | pulse width, 47.98 % as built |
| | DRIFT | slow random detune per oscillator, up to about ±40 cents |
| Filters | COLOUR (+ CV, 1 V/oct) | shifts both bandpass filters up or down |
| | RES | narrower (right) or wider (left) bandpass; centre = as built |
| | BAND | balance of the 3.4 kHz and 7.1 kHz filters |
| | CUT (+ CV, 1 V/oct) | moves the highpass filters |
| | EXT IN | any audio here replaces the oscillators |
| | OSC OUT | the six oscillators, raw |
| Envelope | DECAY (+ CV) | decay time, range set by MODE |
| | MODE | CH 20-150 ms, OH 0.09-0.6 s, CY 0.35-1.2 s (808 ranges) |
| | TONE (+ CV) | low path (long decay) vs the two higher paths (shorter decays) |
| | DRIVE (+ CV) | pushes the VCAs into their soft clip |
| | LEVEL, HIT | output level, manual trigger |
| Gates | TRIG, ACCENT, CHOKE | trigger; accent voltage sampled at each trigger (10 V = +3 dB and more clip); a trigger at CHOKE cuts the sound in 3 ms |

Right-click: *One envelope for all three paths* (the original post's behaviour) and *Reset to the pure model*.

## Building

Builds for Apple Silicon Macs run on GitHub Actions on every push (`.github/workflows/build.yml`). Branch builds go to the
"preview" release, `main` to "latest". `make test` runs the engine checks without the Rack SDK.
Panel: `python3 res/make_panel.py` (needs fonttools and Inter), preview picture: `python3 tools/preview.py out.png`.

Licence: GPL-3.0-or-later (full text in `LICENSE.txt`). Copyright (C) 2026 Haer. It stays GPL until mjsyts, whose circuit model this follows, is asked about a more permissive licence.
