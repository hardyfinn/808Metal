#!/usr/bin/env python3
# Builds the 808 Metal guide PDF:  python3 docs/make_guide.py out.pdf
# (needs Playwright + Chromium; uses panel-preview.png from the repo root)
import os, sys, base64, subprocess
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
out = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else "808Metal-Guide.pdf")
img = base64.b64encode(open(os.path.join(ROOT, "panel-preview.png"), "rb").read()).decode()

html = f"""<!doctype html><html><head><meta charset="utf-8"><style>
@page {{ size: A4; margin: 16mm 16mm 16mm 16mm; }}
body {{ font-family: Inter, 'Helvetica Neue', Arial, sans-serif; color: #1d1d1f; font-size: 10.5pt; line-height: 1.45; }}
h1 {{ font-size: 26pt; margin: 0 0 2mm; letter-spacing: 0.04em; }}
h1 span {{ color: #b8902f; }}
h2 {{ font-size: 13.5pt; margin: 7mm 0 2mm; border-bottom: 1.5px solid #d9b45a; padding-bottom: 1mm; }}
h3 {{ font-size: 11pt; margin: 4mm 0 1mm; }}
.lede {{ font-size: 11.5pt; color: #444; }}
.cols {{ display: flex; gap: 8mm; align-items: flex-start; }}
.cols img {{ width: 92mm; border-radius: 2mm; }}
table {{ border-collapse: collapse; width: 100%; font-size: 9.6pt; }}
td, th {{ border-bottom: 1px solid #e3e0d8; padding: 1.3mm 2mm; text-align: left; vertical-align: top; }}
th {{ background: #f4f1ea; }}
td:first-child {{ font-weight: 700; white-space: nowrap; }}
.recipe {{ background: #f7f5ef; border-left: 3px solid #d9b45a; padding: 2.5mm 4mm; margin: 3mm 0; page-break-inside: avoid; }}
.recipe b.t {{ display: block; font-size: 11pt; margin-bottom: 1mm; }}
.flow {{ font-weight: 700; letter-spacing: 0.02em; background: #1f2023; color: #ece6d8; padding: 2.5mm 4mm; border-radius: 1.5mm; }}
.small {{ font-size: 9pt; color: #555; }}
.break {{ page-break-before: always; }}
</style></head><body>

<h1>808 <span>METAL</span></h1>
<p class="lede">The TR-808's cymbal and hi-hat circuit as a VCV Rack module, built stage by stage from the schematic,
with extra controls so you can push it well past the 808.</p>

<div class="cols">
<img src="data:image/png;base64,{img}">
<div>
<h3>What's inside</h3>
<p class="flow">6 square oscillators &rarr; 2 bandpass filters &rarr; 3 VCAs &rarr; 3 highpass filters &rarr; OUT</p>
<p>The 808 makes all its metal sounds from six square-wave oscillators at clashing, unrelated pitches
(205, 304, 370, 523, 540 and 800 Hz). Two bandpass filters (around 3.4 kHz and 7.1 kHz) pick the bright, clangy part out of
that mix. Three VCAs then shape it, each with its own decay. Last, three highpass filters take away the low end.</p>
<p>Turn every knob to its default (right-click &rarr; <i>Reset to the pure model</i>) and you hear the circuit as built.</p>
<h3>Credit</h3>
<p class="small">The circuit model is by <b>mjsyts</b>, from the post <i>"TR808 Cymbal Hardware Accurate Modeling"</i>
on scsynth.org (September 2026). mjsyts worked out every filter value from the TR-808 service-manual schematic and shared it as
SuperCollider code. 808 Metal is a new implementation of that model with added controls. The post gives no licence for its
code, so none of it is copied: only the published circuit values are used.</p>
</div></div>

<h2>Quick start</h2>
<p>Patch a clock or a sequencer gate into <b>TRIG</b>, and <b>OUT</b> to your mixer. Press <b>HIT</b> to audition.
Flick <b>MODE</b> to choose the kind of sound: <b>CH</b> closed hat, <b>OH</b> open hat, <b>CY</b> cymbal. MODE sets the range of
the <b>DECAY</b> knob to match the 808's own ranges.</p>

<h2>The controls</h2>
<table>
<tr><th>Control</th><th>What you hear</th></tr>
<tr><td>TUNE + V/OCT</td><td>All six oscillators up or down together (±2 octaves). The ratios stay the same, so it is the same metal, just bigger or smaller.</td></tr>
<tr><td>TRIM A / TRIM B</td><td>The two oscillators the real 808 has trimmers for (800 Hz and 540 Hz), over the hardware's own ranges. This is how 808s drift apart from each other.</td></tr>
<tr><td>SPREAD</td><td>Left squeezes the six pitches together (towards a cowbell or a chord); right pushes them apart (gong, bell, broken glass).</td></tr>
<tr><td>WIDTH</td><td>Pulse width. 47.98 % is the circuit. Away from the middle it gets hollow and nasal.</td></tr>
<tr><td>DRIFT</td><td>Each oscillator wanders slowly and randomly in pitch. Small amounts keep repeated hats from sounding identical.</td></tr>
<tr><td>COLOUR + CV</td><td>Slides both bandpass filters up or down (1 V/oct on the CV). Down: darker, gong-like. Up: thin and fizzy.</td></tr>
<tr><td>RES</td><td>Centre is the circuit. Right makes the bandpass filters narrow and ringing (whistling, pitched); left makes them wider and noisier.</td></tr>
<tr><td>BAND</td><td>Balance between the 3.4 kHz filter (left, body) and the 7.1 kHz filter (right, sizzle). Turn it right for a classic 808 hat.</td></tr>
<tr><td>CUT + CV</td><td>Moves the highpass filters. Down lets more body through; up leaves only the air.</td></tr>
<tr><td>EXT IN / OSC OUT</td><td>Any audio patched to EXT IN replaces the six oscillators, so the filters and VCAs work on your sound. OSC OUT gives you the raw six-oscillator chord.</td></tr>
<tr><td>DECAY + CV</td><td>How long the hit rings. CH 20-150 ms, OH 0.09-0.6 s, CY 0.35-1.2 s. ±5 V on the CV covers the whole range. As on the 808, in OH and CY the knob only lengthens the first of the three paths (the body); the two brighter paths always die away quickly.</td></tr>
<tr><td>TONE + CV</td><td>As on the 808, this mostly turns the top path up or down (the short, bright tick at the start of each hit). Left is darker and smoother; right is brighter with a sharper attack.</td></tr>
<tr><td>DRIVE + CV</td><td>Pushes the VCAs into their soft clipping, which the real circuit does too. Thicker, rougher, louder transient. At zero the VCAs are completely clean.</td></tr>
<tr><td>LEVEL / HIT</td><td>Output level, and a button that plays a hit (it lights up with the envelope).</td></tr>
<tr><td>ACCENT</td><td>Read at each trigger: 0 V plays normally, 10 V plays about 3 dB louder and hits the clipping harder, like the 808's accent.</td></tr>
<tr><td>CHOKE</td><td>A trigger here cuts the sound off in 3 ms. Use it so a closed hat stops an open hat.</td></tr>
</table>
<p class="small">Right-click menu: <i>Classic voicing</i> brings back how versions 2.0 to 2.2 sounded: DECAY stretches all three paths,
TONE crossfades the low path against the high ones, and there is no output brightening. Patches saved with those versions switch it on by themselves.
<i>One envelope for all three paths</i> plays it exactly as in mjsyts' post (one shared decay).
<i>Reset to the pure model</i> puts every sound-shaping knob back to the circuit.</p>

<h2 class="break">Recipes</h2>

<div class="recipe"><b class="t">1. The 808 hat pair (with choke)</b>
Use two 808 Metal modules. Module 1: MODE <b>CH</b>, BAND about 2 o'clock. Module 2: MODE <b>OH</b>, DECAY half way, BAND the same.
Patch your closed-hat gate to module 1's TRIG <i>and</i> to module 2's CHOKE. Patch the open-hat gate to module 2's TRIG.
Now every closed hat cuts off the open hat, just like the 808.
<br><i>Try:</i> a slow LFO into module 2's DECAY CV, so the open hats breathe over 8 bars.</div>

<div class="recipe"><b class="t">2. Detroit ride</b>
MODE <b>CY</b>, DECAY at 9 o'clock, TONE at 2 o'clock, DRIVE a little. Trigger on every off-beat 8th.
Patch an accent gate (every 4th hit, or a random gate from Marbles' t-section) to ACCENT.
<br><i>Try:</i> DRIFT at 10 o'clock so no two rides are quite the same.</div>

<div class="recipe"><b class="t">3. Tuned metal percussion</b>
Turn SPREAD left (about 9 o'clock) and RES to 3 o'clock. The six oscillators pull towards one pitch and the filters ring.
Send a sequencer's pitch to V/OCT and a slower gate to TRIG: you get pitched bells and cowbell-like hits.
<br><i>Try:</i> sequence COLOUR CV from the same sequencer, so higher notes are also brighter.</div>

<div class="recipe"><b class="t">4. Self-mutating hats (Autechre direction)</b>
Patch a fast clock to TRIG. Feed three slow, unrelated random voltages (Marbles X outputs, or a VCV Random module) to
DECAY CV, TONE CV and COLOUR CV. Use a probability gate on ACCENT. Every hit is slightly different and the pattern
evolves without repeating.</div>

<div class="recipe"><b class="t">5. 808 filters on anything</b>
Patch a drum loop, a noise source or a synth into EXT IN, trigger on every 16th, MODE <b>CH</b>.
The 808's filters and VCAs chop it into metallic hats. With a loop on EXT IN and CUT turned down, it becomes a gated
"808-ified" version of the loop.</div>

<h2>Notes</h2>
<p class="small">How close is it to the real thing? The oscillators and all the filters use values worked out from the schematic.
The VCA order, the DECAY and TONE behaviour and the brightening output stage follow Werner, Abel and Smith's paper
<i>"The TR-808 Cymbal: a Physically-Informed, Circuit-Bendable, Digital Model"</i> (ICMC 2014). That paper gives no part values for
the two fixed decays or the output stage, so those are set by estimate.</p>
<p class="small">Levels: peaks reach about 7.5 V at default settings. Above 8 V a gentle limiter rounds the peaks off.
OSC OUT is about ±4.5 V. The engine runs at twice your sample rate (oversampled), so the highest filters stay accurate.
CPU use is tiny (well under 1 % of one core).</p>
</body></html>"""

tmp = out + ".html"
open(tmp, "w").write(html)
js = f"""const {{chromium}} = require('playwright');
(async () => {{ const b = await chromium.launch(); const p = await b.newPage();
await p.goto('file://{tmp}'); await p.pdf({{path: '{out}', format: 'A4', printBackground: true}}); await b.close(); }})();"""
open(out + ".js", "w").write(js)
subprocess.run(["node", out + ".js"], check=True, cwd="/tmp")
os.remove(out + ".js"); os.remove(tmp)
print("wrote", out)
