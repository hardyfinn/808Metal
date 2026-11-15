// Engine checks without Rack:  g++ -O2 -std=c++17 -Isrc test/engine_test.cpp -o test/engine_test && test/engine_test
// 1. digital filter responses match mjsyts' analog model (magnitude, dB)
// 2. renders hits in each mode and checks level, silence after decay, no NaN
// 3. writes test/out/*.wav for listening
#include "MetalEngine.hpp"
#include <complex>
#include <cstdio>
#include <vector>
#include <string>
#include <chrono>
using namespace metal;
typedef std::complex<double> cd;
static int fails = 0;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL: " __VA_ARGS__); printf("\n"); } } while (0)

cd biq(const Biquad& b, double w) {
	cd z1 = std::exp(cd(0, -w)), z2 = z1 * z1;
	return (b.b0 + b.b1 * z1 + b.b2 * z2) / (1.0 + b.a1 * z1 + b.a2 * z2);
}
cd analogBpf(const BpfData& d, double w) {
	cd s(0, w);
	return d.gain * s * (s - d.zero) / ((s - d.realPole) * (s - cd(d.re, d.im)) * (s - cd(d.re, -d.im)));
}
cd analogHpf(const HpfData& d, double w) {
	cd s(0, w);
	cd h = d.gain * s * s / ((s - cd(d.re, d.im)) * (s - cd(d.re, -d.im)));
	if (d.realPole != 0) h *= s / (s - d.realPole);
	return h;
}
void writeWav(const std::string& path, const std::vector<float>& x, int sr) {
	FILE* f = fopen(path.c_str(), "wb");
	if (!f) return;
	int n = x.size(), bytes = n * 2;
	auto w32 = [&](uint32_t v) { fwrite(&v, 4, 1, f); };
	auto w16 = [&](uint16_t v) { fwrite(&v, 2, 1, f); };
	fwrite("RIFF", 1, 4, f); w32(36 + bytes); fwrite("WAVEfmt ", 1, 8, f);
	w32(16); w16(1); w16(1); w32(sr); w32(sr * 2); w16(2); w16(16);
	fwrite("data", 1, 4, f); w32(bytes);
	for (float v : x) { float s = std::max(-1.f, std::min(1.f, v / 10.f)); w16((uint16_t)(int16_t)(s * 32767)); }
	fclose(f);
}

int main() {
	for (double fs : {44100.0, 48000.0, 96000.0}) {
		Engine e; e.setSampleRate(fs);
		Controls c; e.updateFilters(c);
		double worst = 0, worstF = 0;
		for (double f : {500.0, 1000.0, 2000.0, 3440.0, 5000.0, 7100.0, 9000.0, 10300.0, 12000.0}) {
			if (f > 0.27 * fs) continue;
			double w = 2 * PI * f, wd = w / e.fs;
			cd dig = 0, ana = 0;
			for (int i = 0; i < 2; i++) {
				dig += biq(e.bpf[i].fos, wd) * biq(e.bpf[i].sos, wd);
				ana += analogBpf(BPF_DATA[i], w);
			}
			cd dh = 0, ah = 0;
			for (int i = 0; i < 3; i++) {
				cd h = biq(e.hpf[i].sos, wd);
				if (e.hpf[i].third) h *= biq(e.hpf[i].fos, wd);
				dh += h; ah += analogHpf(HPF_DATA[i], w);
			}
			double err = std::fabs(20 * std::log10(std::abs(dig * dh) / std::abs(ana * ah)));
			if (err > worst) { worst = err; worstF = f; }
		}
		printf("fs %.0f: worst model-vs-digital error %.2f dB at %.0f Hz\n", fs, worst, worstF);
		CHECK(worst < 1.5, "filter response off by %.2f dB at fs %.0f", worst, fs);
	}

	if (system("mkdir -p test/out")) return 1;
	const int sr = 48000;
	const char* names[3] = {"closed-hat", "open-hat", "cymbal"};
	for (int m = 0; m < 3; m++) {
		Engine e; e.setSampleRate(sr);
		Controls c; c.mode = m; c.decay = 0.5f;
		std::vector<float> out;
		double peak = 0, sumsq = 0; bool nan = false;
		for (int hit = 0; hit < 4; hit++) {
			e.trigger(hit % 2 ? 0.6f : 1.f);
			for (int i = 0; i < sr * 3 / 2; i++) {
				float y = e.process(c);
				if (!std::isfinite(y)) nan = true;
				out.push_back(y);
				peak = std::max(peak, (double) std::fabs(y));
				sumsq += y * y;
			}
		}
		float tail = 0;
		for (int i = out.size() - 2000; i < (int) out.size(); i++) tail = std::max(tail, std::fabs(out[i]));
		printf("%-10s peak %.2f V, tail %.5f\n", names[m], peak, tail);
		CHECK(!nan, "NaN in %s", names[m]);
		CHECK(peak > 3.0 && peak < 10.0, "%s peak %.2f out of range", names[m], peak);
		CHECK(tail < 0.01, "%s does not die away (%.4f)", names[m], tail);
		writeWav(std::string("test/out/") + names[m] + ".wav", out, sr);
	}

	// extreme settings stay finite and bounded
	{
		Engine e; e.setSampleRate(44100);
		Controls c; c.colour = 1.f; c.res = 1.f; c.cut = 1.f; c.drive = 1.f; c.spread = 1.f; c.tune = 2.f; c.drift = 1.f; c.pw = 0.05f;
		double peak = 0; bool nan = false;
		e.trigger(1.f);
		for (int i = 0; i < 44100; i++) { float y = e.process(c); nan |= !std::isfinite(y); peak = std::max(peak, (double) std::fabs(y)); }
		c.colour = -2.f; c.res = 0.f; c.cut = -2.f; c.spread = -1.f; c.tune = -2.f;
		e.trigger(1.f);
		for (int i = 0; i < 44100; i++) { float y = e.process(c); nan |= !std::isfinite(y); peak = std::max(peak, (double) std::fabs(y)); }
		printf("extremes peak %.2f\n", peak);
		CHECK(!nan && peak <= 12, "extreme settings misbehave (peak %.2f)", peak);
	}
	// choke
	{
		Engine e; e.setSampleRate(48000); Controls c; c.mode = MODE_CY; c.decay = 1.f;
		e.trigger(1.f);
		for (int i = 0; i < 4800; i++) e.process(c);
		e.choke();
		float m = 0;
		for (int i = 0; i < 4800; i++) { float y = e.process(c); if (i > 2400) m = std::max(m, std::fabs(y)); }
		printf("choke tail %.5f\n", m);
		CHECK(m < 0.01, "choke too slow");
	}
	// cpu
	{
		Engine e; e.setSampleRate(48000); Controls c; float acc = 0;
		auto t0 = std::chrono::steady_clock::now();
		for (int i = 0; i < 48000 * 10; i++) { if (i % 12000 == 0) e.trigger(1); acc += e.process(c); }
		double s = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
		printf("cpu: %.2f%% of one core at 48 kHz (%g)\n", s / 10 * 100, acc * 0);
	}
	printf(fails ? "%d FAILED\n" : "all passed\n", fails);
	return fails ? 1 : 0;
}
