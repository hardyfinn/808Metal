// 808 Metal sound engine. Plain C++ (no Rack headers) so it can be tested on
// its own: see test/engine_test.cpp.
//
// The circuit model comes from mjsyts' "TR808 Cymbal Hardware Accurate
// Modeling" post on scsynth.org (2026-09-11), written in SuperCollider:
// https://scsynth.org/t/tr808-cymbal-hardware-accurate-modeling/13607
// mjsyts derived the s-domain poles, zeros and gains below from the TR-808
// service-manual schematic. This file is a fresh C++ implementation of that
// model, with extra controls around it.
//
// Signal flow (as in the 808):
//   six square oscillators -> two bandpass filters in parallel (about 3.4 kHz
//   and 7.1 kHz), summed -> three "swing" VCAs, each with its own envelope
//   and a soft diode-style clip -> three highpass filters (one after each VCA),
//   summed -> out.
// The post used one envelope for all three paths; here each path can decay at
// its own rate (TONE and MODE), which is what the 808's cymbal circuit does.
#pragma once
#include <cmath>
#include <cstdint>
#include <algorithm>

namespace metal {

static const double PI = 3.14159265358979323846;

// ---- model data (rad/s), from mjsyts' post ----
struct BpfData { double zero, realPole, re, im, gain; };
static const BpfData BPF_DATA[2] = {
	{-131302.5210084, -9182.73645546, -1793.40028694, 21627.29000861, -8912.6559714795},     // ~3440 Hz
	{-270562.77056277, -45454.54545455, -3695.49150037, 44565.32486623, -27548.20936639118}, // ~7100 Hz
};
struct HpfData { double re, im, realPole, gain; };  // realPole 0 = 2nd order
static const HpfData HPF_DATA[3] = {
	{-8130.08130081, 13426.38737959, 0.0, 1.0},
	{-27777.77777778, 48112.52243247, 0.0, 2.0},
	{-5749.67889223, 64618.05691577, -32681.30805396, 2.7727272727272725},
};
static const float OSC_HZ[6] = {205.3f, 369.6f, 304.4f, 522.7f, 800.f, 540.f};
static const float PULSE_WIDTH = 0.4798f;
static const float OSC_SCALE = 0.15f;
static const float ATTACK = 1.0244e-4f;
static const float OUT_GAIN = 2.f;  // model units -> volts (about 5-6 V peaks)

// ---- filter sections ----
// Biquad / first-order section, transposed direct form II, double precision
// (the poles sit close to the unit circle at high sample rates).
struct Biquad {
	double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
	double z1 = 0, z2 = 0;
	inline double process(double x) {
		double y = b0 * x + z1;
		z1 = b1 * x - a1 * y + z2;
		z2 = b2 * x - a2 * y;
		return y;
	}
	void reset() { z1 = z2 = 0; }
	// Analog H(s) = (n2 s^2 + n1 s + n0) / (s^2 + d1 s + d0) via the bilinear
	// transform with s = K (1 - z^-1) / (1 + z^-1).
	void setAnalog2(double n2, double n1, double n0, double d1, double d0, double K) {
		double K2 = K * K;
		double a0 = K2 + d1 * K + d0;
		b0 = (n2 * K2 + n1 * K + n0) / a0;
		b1 = (2 * n0 - 2 * n2 * K2) / a0;
		b2 = (n2 * K2 - n1 * K + n0) / a0;
		a1 = (2 * d0 - 2 * K2) / a0;
		a2 = (K2 - d1 * K + d0) / a0;
	}
	// Analog H(s) = (n1 s + n0) / (s + d0)
	void setAnalog1(double n1, double n0, double d0, double K) {
		double a0 = K + d0;
		b0 = (n1 * K + n0) / a0;
		b1 = (n0 - n1 * K) / a0;
		b2 = 0;
		a1 = (d0 - K) / a0;
		a2 = 0;
	}
};

// Bilinear constant, prewarped so that angular frequency w lands exactly
// where it should. (The original uses plain 2*fs; prewarping keeps the
// 7.1 kHz band in place at 44.1/48 kHz.) w is kept below Nyquist.
static inline double warpK(double w, double fs) {
	double wmax = 0.95 * PI * fs;
	w = std::min(std::max(w, 1.0), wmax);
	return w / std::tan(w / (2.0 * fs));
}

struct Bpf3 {
	Biquad fos, sos;
	// k: frequency scale (COLOUR), q: real-part scale of the resonant pair (RES)
	void set(const BpfData& d, double k, double q, double fs) {
		double pr = d.realPole * k, z = d.zero * k;
		double re = d.re * k * q, im = d.im * k;
		double w0sq = re * re + im * im;
		double gain = d.gain * k * std::pow(q, 0.75);  // H0(s/k) keeps its peak level
		// s / (s - pr)
		fos.setAnalog1(1.0, 0.0, -pr, warpK(-pr, fs));
		// gain (s - z) / (s^2 - 2 re s + |p|^2)
		sos.setAnalog2(0.0, gain, -gain * z, -2 * re, w0sq, warpK(std::sqrt(w0sq), fs));
	}
	inline double process(double x) { return sos.process(fos.process(x)); }
	void reset() { fos.reset(); sos.reset(); }
};

struct Hpf {
	Biquad sos, fos;
	bool third = false;
	void set(const HpfData& d, double c, double fs) {
		double re = d.re * c, im = d.im * c;
		double w0sq = re * re + im * im;
		sos.setAnalog2(d.gain, 0.0, 0.0, -2 * re, w0sq, warpK(std::sqrt(w0sq), fs));
		third = d.realPole != 0.0;
		if (third) {
			double pr = d.realPole * c;
			fos.setAnalog1(1.0, 0.0, -pr, warpK(-pr, fs));
		}
	}
	inline double process(double x) {
		double y = sos.process(x);
		return third ? fos.process(y) : y;
	}
	void reset() { sos.reset(); fos.reset(); }
};

// ---- band-limited pulse oscillator (polyBLEP) ----
static inline float polyblep(float t, float dt) {
	if (t < dt) { t /= dt; return t + t - t * t - 1.f; }
	if (t > 1.f - dt) { t = (t - 1.f) / dt; return t * t + t + t + 1.f; }
	return 0.f;
}

struct PulseOsc {
	float phase = 0.f;
	inline float process(float freq, float pw, float sampleTime) {
		float dt = std::min(freq * sampleTime, 0.45f);
		float y = phase < pw ? 1.f : -1.f;
		y += polyblep(phase, dt);
		float t2 = phase - pw;
		if (t2 < 0.f) t2 += 1.f;
		y -= polyblep(t2, dt);
		phase += dt;
		if (phase >= 1.f) phase -= 1.f;
		return y;
	}
};

// small fast random generator for DRIFT
struct Rng {
	uint32_t s = 0x9E3779B9u;
	inline float uniform() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return (s >> 8) * (1.f / 16777216.f); }
	inline float normal() { float a = 0; for (int i = 0; i < 4; i++) a += uniform(); return (a - 2.f) * 1.7320508f; }
};

static inline float softClip(float x) {
	// tanh-like, smooth, exact enough for a diode clip
	if (x > 3.f) return 1.f;
	if (x < -3.f) return -1.f;
	float x2 = x * x;
	return x * (27.f + x2) / (27.f + 9.f * x2);
}

enum Mode { MODE_CH = 0, MODE_OH = 1, MODE_CY = 2 };

// Decay range (seconds) for the DECAY knob in each mode, from the 808
// service manual: closed hat ~50 ms, open hat 0.09-0.6 s, cymbal 0.35-1.2 s.
static const float DECAY_LO[3] = {0.02f, 0.09f, 0.35f};
static const float DECAY_HI[3] = {0.15f, 0.60f, 1.20f};
// how much shorter the middle and top paths decay than the lowest one
static const float BAND_RATIO[3][3] = {
	{1.f, 1.f, 1.f},
	{1.f, 0.7f, 0.5f},
	{1.f, 0.5f, 0.25f},
};

struct Controls {
	float tune = 0.f;        // octaves (TUNE knob + V/OCT)
	float trimA = 800.f;     // Hz, the 808's tunable 800 Hz oscillator
	float trimB = 540.f;     // Hz, the 808's tunable 540 Hz oscillator
	float spread = 0.f;      // -1..+1, 0 = as built
	float pw = PULSE_WIDTH;  // 0.05..0.95
	float drift = 0.f;       // 0..1
	float colour = 0.f;      // octaves, bandpass shift
	float res = 0.5f;        // 0..1, 0.5 = as built
	float band = 0.f;        // -1 (3.4 kHz only) .. +1 (7.1 kHz only)
	float cut = 0.f;         // octaves, highpass shift
	float decay = 0.5f;      // 0..1 within the mode's range
	int mode = MODE_CY;
	float tone = 0.f;        // -1 (low path only) .. +1 (top paths only)
	float drive = 0.f;       // 0..1
	float level = 1.f;
	bool oneEnvelope = false; // all three paths share one decay (as in the post)
	bool useExt = false;
	float ext = 0.f;          // external audio (V), replaces the oscillators
};

struct Engine {
	PulseOsc osc[6];
	Bpf3 bpf[2];
	Hpf hpf[3];
	float env[3] = {0, 0, 0};
	bool attacking = false;
	bool choking = false;
	float accent = 1.f;
	float driftState[6] = {0, 0, 0, 0, 0, 0};
	Rng rng;
	double fs = 0;
	// cached filter settings so coefficients are only recomputed on change
	float lastColour = 1e9f, lastRes = -1, lastCut = 1e9f;
	int counter = 0;
	float freqs[6];
	float decayCoef[3] = {0, 0, 0};
	float oscOut = 0.f;

	Engine() {
		for (int i = 0; i < 6; i++) { freqs[i] = OSC_HZ[i]; osc[i].phase = 0.17f * i; }
	}

	// The engine runs at twice the host rate (oversampled): the 7.1 kHz
	// band and the resonant 10 kHz highpass then stay true to the model,
	// and the oscillators alias less. process() returns one host sample.
	Biquad down[2];
	void setSampleRate(double sr) {
		if (2.0 * sr == fs) return;
		fs = 2.0 * sr;
		// 4th-order Butterworth lowpass at 0.42 x host rate, for decimation
		double wc = 2 * PI * 0.42 * sr;
		double K = warpK(wc, fs);
		const double qs[2] = {0.5411961, 1.3065630};
		for (int i = 0; i < 2; i++) {
			down[i].setAnalog2(0, 0, wc * wc, wc / qs[i], wc * wc, K);
			down[i].reset();
		}
		lastColour = 1e9f;  // force a recompute
		for (auto& b : bpf) b.reset();
		for (auto& h : hpf) h.reset();
	}

	void trigger(float acc) {
		accent = acc;
		attacking = true;
		choking = false;
	}
	void choke() { choking = true; attacking = false; }

	static float resToQ(float r) {
		// 0.5 = as built; down to 2x wider, up to 8x narrower
		return r < 0.5f ? std::pow(2.f, 2.f * (0.5f - r)) : std::pow(2.f, -6.f * (r - 0.5f));
	}

	void updateFilters(const Controls& c) {
		if (c.colour != lastColour || c.res != lastRes) {
			double k = std::pow(2.0, (double) c.colour), q = resToQ(c.res);
			for (int i = 0; i < 2; i++) bpf[i].set(BPF_DATA[i], k, q, fs);
			lastColour = c.colour; lastRes = c.res;
		}
		if (c.cut != lastCut) {
			double k = std::pow(2.0, (double) c.cut);
			for (int i = 0; i < 3; i++) hpf[i].set(HPF_DATA[i], k, fs);
			lastCut = c.cut;
		}
	}

	// control-rate work: oscillator frequencies, drift, filters, decay rates
	void updateControl(const Controls& c, float dt) {
		updateFilters(c);
		float base[6];
		for (int i = 0; i < 6; i++) base[i] = OSC_HZ[i];
		base[4] = c.trimA;
		base[5] = c.trimB;
		// SPREAD: stretch the six around their geometric mean
		float lg = 0.f;
		for (int i = 0; i < 6; i++) lg += std::log2(base[i]);
		lg /= 6.f;
		float stretch = std::pow(2.f, c.spread);
		for (int i = 0; i < 6; i++) {
			// DRIFT: slow wander (Ornstein-Uhlenbeck), up to about +-40 cents
			float& d = driftState[i];
			d += -d * 0.6f * dt + rng.normal() * 0.9f * std::sqrt(dt);
			float cents = c.drift * 40.f * d;
			float l = lg + (std::log2(base[i]) - lg) * stretch + c.tune + cents / 1200.f;
			freqs[i] = std::min(std::pow(2.f, l), 0.4f * (float) fs);
		}
		// envelope decay coefficients (exponential, -35 dB at the decay time)
		int m = std::min(std::max(c.mode, 0), 2);
		float d = DECAY_LO[m] * std::pow(DECAY_HI[m] / DECAY_LO[m], std::min(std::max(c.decay, 0.f), 1.f));
		for (int b = 0; b < 3; b++) {
			float ratio = c.oneEnvelope ? 1.f : BAND_RATIO[m][b];
			float tau = d * ratio / 4.f;
			decayCoef[b] = std::exp(-1.f / (tau * (float) fs));
		}
	}

	float process(const Controls& c) {
		double y = 0;
		for (int i = 0; i < 2; i++)
			y = down[1].process(down[0].process(processInner(c)));
		return (float) std::min(std::max(y, -12.0), 12.0);
	}

	float processInner(const Controls& c) {
		float st = 1.f / (float) fs;
		if (counter-- <= 0) {
			counter = 31;
			updateControl(c, 32.f * st);
		}
		// oscillators
		float pw = std::min(std::max(c.pw, 0.05f), 0.95f);
		float s = 0.f;
		for (int i = 0; i < 6; i++) s += osc[i].process(freqs[i], pw, st);
		s *= OSC_SCALE;
		oscOut = s;
		if (c.useExt) s = c.ext * 0.1f;

		// bandpass pair, BAND balances them
		float gLow = c.band > 0.f ? 1.f - c.band : 1.f;
		float gHigh = c.band < 0.f ? 1.f + c.band : 1.f;
		double bp = gLow * bpf[0].process(s) + gHigh * bpf[1].process(s);

		// envelopes
		const float atkStep = st / ATTACK;
		float chokeCoef = std::exp(-1.f / (0.003f * (float) fs));
		for (int b = 0; b < 3; b++) {
			if (attacking) {
				env[b] = std::min(env[b] + atkStep, 1.f);
			}
			else {
				env[b] *= choking ? chokeCoef : decayCoef[b];
			}
		}
		if (attacking && env[0] >= 1.f && env[1] >= 1.f && env[2] >= 1.f) attacking = false;

		// three VCAs with a soft clip, DRIVE pushes into it
		float pre = 1.f + 15.f * c.drive * c.drive;
		const float S = 5.f;  // clip headroom in model units
		float post = S / std::sqrt(pre);
		float gL = c.tone > 0.f ? 1.f - c.tone : 1.f;
		float gH = c.tone < 0.f ? 1.f + c.tone : 1.f;
		float out = 0.f;
		for (int b = 0; b < 3; b++) {
			float v = (float) bp * env[b] * accent;
			v = softClip(v * pre / S) * post;
			out += (float) hpf[b].process(v) * (b == 0 ? gL : gH);
		}
		return out * c.level * OUT_GAIN;
	}
};

}  // namespace metal
