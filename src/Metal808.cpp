// 808 Metal: TR-808 cymbal / hi-hat circuit model.
// The sound engine is in MetalEngine.hpp (credit and model notes there).
#include "plugin.hpp"
#include "MetalEngine.hpp"

// ---- LAYOUT (same numbers in res/make_panel.py) ----
static const float COL[6] = {11.0f, 31.0f, 51.0f, 71.0f, 91.0f, 111.0f};
static const float ROW_OSC = 30.0f, ROW_FLT = 51.0f, ROW_ENV = 72.0f, ROW_CV = 94.5f, ROW_IO = 113.5f;

struct Metal808 : Module {
	enum ParamIds {
		TUNE_PARAM,
		TRIMA_PARAM,
		TRIMB_PARAM,
		SPREAD_PARAM,
		WIDTH_PARAM,
		DRIFT_PARAM,
		COLOUR_PARAM,
		RES_PARAM,
		BAND_PARAM,
		CUT_PARAM,
		DECAY_PARAM,
		MODE_PARAM,
		TONE_PARAM,
		DRIVE_PARAM,
		LEVEL_PARAM,
		HIT_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		VOCT_INPUT,
		COLOUR_INPUT,
		CUT_INPUT,
		DECAY_INPUT,
		TONE_INPUT,
		DRIVE_INPUT,
		TRIG_INPUT,
		ACCENT_INPUT,
		CHOKE_INPUT,
		EXT_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		OSC_OUTPUT,
		MAIN_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		HIT_LIGHT,
		NUM_LIGHTS
	};

	metal::Engine engine;
	metal::Controls ctl;
	dsp::SchmittTrigger trigIn, chokeIn, hitButton;
	dsp::ClockDivider lightDivider;
	bool oneEnvelope = false;

	struct ModeQuantity : ParamQuantity {
		std::string getDisplayValueString() override {
			static const char* names[3] = {"Closed hat (CH)", "Open hat (OH)", "Cymbal (CY)"};
			int m = clamp((int) std::round(getValue()), 0, 2);
			return names[m];
		}
	};

	Metal808() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(TUNE_PARAM, -24.f, 24.f, 0.f, "Tune (all six oscillators)", " semitones");
		configParam(TRIMA_PARAM, 359.4f, 1149.9f, 800.f, "Trim A (the 808's tunable 800 Hz oscillator)", " Hz");
		configParam(TRIMB_PARAM, 254.3f, 627.2f, 540.f, "Trim B (the 808's tunable 540 Hz oscillator)", " Hz");
		configParam(SPREAD_PARAM, -1.f, 1.f, 0.f, "Spread (gaps between the oscillators)", "x", 2.f);
		configParam(WIDTH_PARAM, 5.f, 95.f, 47.98f, "Pulse width", "%");
		configParam(DRIFT_PARAM, 0.f, 1.f, 0.f, "Drift (slow random detune)", "%", 0.f, 100.f);
		configParam(COLOUR_PARAM, -1.f, 1.f, 0.f, "Colour (bandpass shift)", " oct");
		configParam(RES_PARAM, 0.f, 1.f, 0.5f, "Resonance (0.5 = as built)", "%", 0.f, 100.f);
		configParam(BAND_PARAM, -1.f, 1.f, 0.f, "Band (3.4 kHz <-> 7.1 kHz)");
		configParam(CUT_PARAM, -2.f, 1.f, 0.f, "Cut (highpass shift)", " oct");
		configParam(DECAY_PARAM, 0.f, 1.f, 0.5f, "Decay (range set by MODE)", "%", 0.f, 100.f);
		configParam<ModeQuantity>(MODE_PARAM, 0.f, 2.f, 2.f, "Mode");
		getParamQuantity(MODE_PARAM)->snapEnabled = true;
		configParam(TONE_PARAM, -1.f, 1.f, 0.f, "Tone (low path <-> high paths)");
		configParam(DRIVE_PARAM, 0.f, 1.f, 0.f, "Drive (VCA clipping)", "%", 0.f, 100.f);
		configParam(LEVEL_PARAM, 0.f, 1.f, 1.f, "Level", "%", 0.f, 100.f)->randomizeEnabled = false;
		configButton(HIT_PARAM, "Hit");

		configInput(VOCT_INPUT, "V/OCT (all six oscillators)");
		configInput(COLOUR_INPUT, "Colour CV (1 V/oct)");
		configInput(CUT_INPUT, "Cut CV (1 V/oct)");
		configInput(DECAY_INPUT, "Decay CV (+-5 V = full range)");
		configInput(TONE_INPUT, "Tone CV (+-5 V = full range)");
		configInput(DRIVE_INPUT, "Drive CV (0-10 V)");
		configInput(TRIG_INPUT, "Trigger");
		configInput(ACCENT_INPUT, "Accent (sampled at each trigger; 0 V normal, 10 V +3 dB and harder clip)");
		configInput(CHOKE_INPUT, "Choke (a trigger here cuts the sound, e.g. from a closed hat)");
		configInput(EXT_INPUT, "External audio (replaces the oscillators)");
		configOutput(OSC_OUTPUT, "Six oscillators, raw");
		configOutput(MAIN_OUTPUT, "Audio");

		lightDivider.setDivision(256);
	}

	void onSampleRateChange(const SampleRateChangeEvent& e) override {
		engine.setSampleRate(e.sampleRate);
	}

	void process(const ProcessArgs& args) override {
		engine.setSampleRate(args.sampleRate);

		ctl.tune = params[TUNE_PARAM].getValue() / 12.f + inputs[VOCT_INPUT].getVoltage();
		ctl.trimA = params[TRIMA_PARAM].getValue();
		ctl.trimB = params[TRIMB_PARAM].getValue();
		ctl.spread = params[SPREAD_PARAM].getValue();
		ctl.pw = params[WIDTH_PARAM].getValue() / 100.f;
		ctl.drift = params[DRIFT_PARAM].getValue();
		ctl.colour = clamp(params[COLOUR_PARAM].getValue() + inputs[COLOUR_INPUT].getVoltage(), -2.f, 1.5f);
		ctl.res = params[RES_PARAM].getValue();
		ctl.band = params[BAND_PARAM].getValue();
		ctl.cut = clamp(params[CUT_PARAM].getValue() + inputs[CUT_INPUT].getVoltage(), -3.f, 1.5f);
		ctl.decay = clamp(params[DECAY_PARAM].getValue() + inputs[DECAY_INPUT].getVoltage() * 0.1f, 0.f, 1.f);
		ctl.mode = clamp((int) std::round(params[MODE_PARAM].getValue()), 0, 2);
		ctl.tone = clamp(params[TONE_PARAM].getValue() + inputs[TONE_INPUT].getVoltage() * 0.2f, -1.f, 1.f);
		ctl.drive = clamp(params[DRIVE_PARAM].getValue() + inputs[DRIVE_INPUT].getVoltage() * 0.1f, 0.f, 1.f);
		ctl.level = params[LEVEL_PARAM].getValue();
		ctl.oneEnvelope = oneEnvelope;
		ctl.useExt = inputs[EXT_INPUT].isConnected();
		ctl.ext = inputs[EXT_INPUT].getVoltage();

		bool hit = hitButton.process(params[HIT_PARAM].getValue());
		if (trigIn.process(inputs[TRIG_INPUT].getVoltage(), 0.1f, 1.f) || hit) {
			float acc = 1.f;
			if (inputs[ACCENT_INPUT].isConnected())
				acc = 1.f + clamp(inputs[ACCENT_INPUT].getVoltage(), 0.f, 10.f) * 0.043f;
			engine.trigger(acc);
		}
		if (chokeIn.process(inputs[CHOKE_INPUT].getVoltage(), 0.1f, 1.f))
			engine.choke();

		float y = engine.process(ctl);
		outputs[MAIN_OUTPUT].setVoltage(y);
		outputs[OSC_OUTPUT].setVoltage(engine.oscOut * 5.f);

		if (lightDivider.process()) {
			float dt = args.sampleTime * lightDivider.getDivision();
			lights[HIT_LIGHT].setBrightnessSmooth(engine.env[0], dt, 30.f);
		}
	}

	json_t* dataToJson() override {
		json_t* root = json_object();
		json_object_set_new(root, "oneEnvelope", json_boolean(oneEnvelope));
		return root;
	}

	void dataFromJson(json_t* root) override {
		json_t* j;
		if ((j = json_object_get(root, "oneEnvelope"))) oneEnvelope = json_boolean_value(j);
	}
};

struct Metal808Widget : ModuleWidget {
	Metal808Widget(Metal808* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/808Metal.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		const int oscKnobs[6] = {Metal808::TUNE_PARAM, Metal808::TRIMA_PARAM, Metal808::TRIMB_PARAM,
		                         Metal808::SPREAD_PARAM, Metal808::WIDTH_PARAM, Metal808::DRIFT_PARAM};
		for (int i = 0; i < 6; i++)
			addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(COL[i], ROW_OSC)), module, oscKnobs[i]));

		const int fltKnobs[4] = {Metal808::COLOUR_PARAM, Metal808::RES_PARAM, Metal808::BAND_PARAM, Metal808::CUT_PARAM};
		for (int i = 0; i < 4; i++)
			addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(COL[i], ROW_FLT)), module, fltKnobs[i]));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(COL[4], ROW_FLT)), module, Metal808::EXT_INPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(COL[5], ROW_FLT)), module, Metal808::OSC_OUTPUT));

		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(COL[0], ROW_ENV)), module, Metal808::DECAY_PARAM));
		addParam(createParamCentered<CKSSThree>(mm2px(Vec(COL[1], ROW_ENV)), module, Metal808::MODE_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(COL[2], ROW_ENV)), module, Metal808::TONE_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(COL[3], ROW_ENV)), module, Metal808::DRIVE_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(COL[4], ROW_ENV)), module, Metal808::LEVEL_PARAM));
		addParam(createLightParamCentered<VCVLightBezel<YellowLight>>(mm2px(Vec(COL[5], ROW_ENV)), module, Metal808::HIT_PARAM, Metal808::HIT_LIGHT));

		const int cvIns[6] = {Metal808::VOCT_INPUT, Metal808::COLOUR_INPUT, Metal808::CUT_INPUT,
		                      Metal808::DECAY_INPUT, Metal808::TONE_INPUT, Metal808::DRIVE_INPUT};
		for (int i = 0; i < 6; i++)
			addInput(createInputCentered<PJ301MPort>(mm2px(Vec(COL[i], ROW_CV)), module, cvIns[i]));

		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(COL[0], ROW_IO)), module, Metal808::TRIG_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(COL[1], ROW_IO)), module, Metal808::ACCENT_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(COL[2], ROW_IO)), module, Metal808::CHOKE_INPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(COL[5], ROW_IO)), module, Metal808::MAIN_OUTPUT));
	}

	void appendContextMenu(Menu* menu) override {
		Metal808* module = dynamic_cast<Metal808*>(this->module);
		if (!module)
			return;
		menu->addChild(new MenuSeparator);
		menu->addChild(createBoolPtrMenuItem("One envelope for all three paths (as in the original post)", "", &module->oneEnvelope));
		menu->addChild(createMenuItem("Reset to the pure model", "", [=]() {
			for (int p : {Metal808::TUNE_PARAM, Metal808::TRIMA_PARAM, Metal808::TRIMB_PARAM, Metal808::SPREAD_PARAM,
			              Metal808::WIDTH_PARAM, Metal808::DRIFT_PARAM, Metal808::COLOUR_PARAM, Metal808::RES_PARAM,
			              Metal808::BAND_PARAM, Metal808::CUT_PARAM, Metal808::TONE_PARAM, Metal808::DRIVE_PARAM})
				module->paramQuantities[p]->reset();
		}));
	}
};

Model* modelMetal808 = createModel<Metal808, Metal808Widget>("808Metal");
