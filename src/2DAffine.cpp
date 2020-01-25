#include "plugin.hpp"


struct _2DAffine : Module {
	enum ParamIds {
		ANGLE_PARAM,
		AMOUNT_PARAM,
		SHEARX_PARAM,
		SHEARXAMOUNT_PARAM,
		SHEARY_PARAM,
		SHEARYAMOUNT_PARAM,
		XOFF_PRE_PARAM,
		YOFF_PRE_PARAM,
		XOFF_POST_PARAM,
		YOFF_POST_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		CV_INPUT,
		CVSHEARX_INPUT,
		CVSHEARY_INPUT,
		IN1_INPUT,
		IN2_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		OUT1_OUTPUT,
		OUT2_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		XOFF_PRE_LIGHT,
		YOFF_PRE_LIGHT,
		XOFF_POST_LIGHT,
		YOFF_POST_LIGHT,
		NUM_LIGHTS
	};

	float x1 = 0.f;
	float x2 = 0.f;
	float x10 = 0.f;
	float x20 = 0.f;
	float y1[16] = {};
	float y2[16] = {};
	float Theta = 0.f;
	float DeltaTheta = 0.f;
	float xoff_pre = 0.f;
	float yoff_pre = 0.f;
	float xoff_post = 0.f;
	float yoff_post = 0.f;
	int   panelTheme;

	_2DAffine() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(ANGLE_PARAM, -180.f, 180.f, 0.f, "Deg.");
		configParam(AMOUNT_PARAM, -1.f, 1.f, 0.f, "");
		configParam(SHEARX_PARAM, -3.f, 3.f, 0.f, "Scale");
		configParam(SHEARXAMOUNT_PARAM, -1.f, 1.f, 0.f, "");
		configParam(SHEARY_PARAM, -3.f, 3.f, 0.f, "Shear");
		configParam(SHEARYAMOUNT_PARAM, -1.f, 1.f, 0.f, "");

		configParam(XOFF_PRE_PARAM, -10.f, 10.f, 0.0f, "X_Offset");
		configParam(YOFF_PRE_PARAM, -10.f, 10.f, 0.0f, "Y_Offset");
		configParam(XOFF_POST_PARAM, -10.f, 10.f, 0.0f, "X_Offset");
		configParam(YOFF_POST_PARAM, -10.f, 10.f, 0.0f, "Y_Offset");
		panelTheme = 0;
	}

	void process(const ProcessArgs& args) override {

		// Use the first input to get number of channels
		int channels = std::max(inputs[IN1_INPUT].getChannels(), 1);

		// Define rotation angle
		Theta = params[ANGLE_PARAM].getValue() / 180 * M_PI;
		DeltaTheta = M_PI / 5.0 * inputs[CV_INPUT].getVoltage() * params[AMOUNT_PARAM].getValue();
		Theta += DeltaTheta;

		// Define translation parameters 
		xoff_pre = params[XOFF_PRE_PARAM].getValue();
		yoff_pre = params[YOFF_PRE_PARAM].getValue();
		xoff_post = params[XOFF_POST_PARAM].getValue();
		yoff_post = params[YOFF_POST_PARAM].getValue();
		

		for (int c = 0; c < channels; c++) {		
			// Read input values
			x1   = inputs[IN1_INPUT].getVoltage(c);
			x2   = inputs[IN2_INPUT].getVoltage(c);
			x1 = x1 + xoff_pre;
			x2 = x2 + yoff_pre;

			// Scale x & y
			x10 = x1 + x2 * (params[SHEARX_PARAM].getValue() + inputs[CVSHEARX_INPUT].getVoltage() * params[SHEARXAMOUNT_PARAM].getValue());
			x20 = x2 + x1 * (params[SHEARY_PARAM].getValue() + inputs[CVSHEARY_INPUT].getVoltage() * params[SHEARYAMOUNT_PARAM].getValue());

			// Rotation
			y1[c] = (   std::cos(Theta) * x10 + std::sin(Theta) * x20 + xoff_post)/10.f;
			y2[c] = ( - std::sin(Theta) * x10 + std::cos(Theta) * x20 + yoff_post)/10.f;	
		
			// Parabolic saturation
			y1[c] = 10.f * clamp(y1[c],-2.f,2.f) * (1.f-0.25*std::abs(y1[c]));
			y2[c] = 10.f * clamp(y2[c],-2.f,2.f) * (1.f-0.25*std::abs(y2[c]));
		}

		// Set output values
		outputs[OUT1_OUTPUT].setChannels(channels);
		outputs[OUT2_OUTPUT].setChannels(channels);
		outputs[OUT1_OUTPUT].writeVoltages(y1);
		outputs[OUT2_OUTPUT].writeVoltages(y2);

		//Light
		lights[XOFF_PRE_LIGHT].setBrightness(std::abs(xoff_pre)/10);
		lights[YOFF_PRE_LIGHT].setBrightness(std::abs(yoff_pre)/10);
		lights[XOFF_POST_LIGHT].setBrightness(std::abs(xoff_post)/10);
		lights[YOFF_POST_LIGHT].setBrightness(std::abs(yoff_post)/10);
	}


	json_t *dataToJson() override {
		json_t *rootJ = json_object();
		// panelTheme
		json_object_set_new(rootJ, "panelTheme", json_integer(panelTheme));
		return rootJ;
	}

	
	void dataFromJson(json_t *rootJ) override {
		// panelTheme
		json_t *panelThemeJ = json_object_get(rootJ, "panelTheme");
		if (panelThemeJ)
			panelTheme = json_integer_value(panelThemeJ);
		//resetNonJson();
	}

};


struct _2DAffineWidget : ModuleWidget {

	SvgPanel* darkPanel;

	struct PanelThemeItem : MenuItem {
		_2DAffine *module;
		int theme;
		void onAction(const event::Action &e) override {
			module->panelTheme = theme;
		}
		void step() override {
			rightText = (module->panelTheme == theme) ? "✔" : "";
		}
	};	
	void appendContextMenu(Menu *menu) override {
		MenuLabel *spacerLabel = new MenuLabel();
		menu->addChild(spacerLabel);

		_2DAffine *module = dynamic_cast<_2DAffine*>(this->module);
		assert(module);

		MenuLabel *themeLabel = new MenuLabel();
		themeLabel->text = "Panel Theme";
		menu->addChild(themeLabel);

		PanelThemeItem *lightItem = new PanelThemeItem();
		lightItem->text = "Light panel";
		lightItem->module = module;
		lightItem->theme = 0;
		menu->addChild(lightItem);

		PanelThemeItem *darkItem = new PanelThemeItem();
		darkItem->text = "Dark panel";	
		darkItem->module = module;
		darkItem->theme = 1;
		menu->addChild(darkItem);
	}	

	_2DAffineWidget(_2DAffine* module) {
		setModule(module);
		if (module) {
			darkPanel = new SvgPanel();
			darkPanel->setBackground(APP->window->loadSvg(asset::plugin(pluginInstance, "res/2DAffine_dark.svg")));
			darkPanel->visible = false;
			addChild(darkPanel);
		}
		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/2DAffine.svg")));

		addChild(createWidget<ScrewSilver>(Vec(0, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 1 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(0, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 1 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(12.664, 29.317)), module, _2DAffine::ANGLE_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(12.664, 41.828)), module, _2DAffine::AMOUNT_PARAM));
		addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(5.664, 34.317)), module, _2DAffine::SHEARX_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(5.664, 46.828)), module, _2DAffine::SHEARXAMOUNT_PARAM));
		addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(19.664, 34.317)), module, _2DAffine::SHEARY_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(19.664, 46.828)), module, _2DAffine::SHEARYAMOUNT_PARAM));

		addParam(createLightParamCentered<LEDLightSlider<GreenLight>>(mm2px(Vec(5.254, 80.858)),  module, _2DAffine::XOFF_PRE_PARAM, _2DAffine::XOFF_PRE_LIGHT));
		addParam(createLightParamCentered<LEDLightSlider<GreenLight>>(mm2px(Vec(15.254, 80.858)), module, _2DAffine::YOFF_PRE_PARAM, _2DAffine::YOFF_PRE_LIGHT));
		addParam(createLightParamCentered<LEDLightSlider<BlueLight>>(mm2px(Vec(10.254, 80.858)),  module, _2DAffine::XOFF_POST_PARAM, _2DAffine::XOFF_POST_LIGHT));
		addParam(createLightParamCentered<LEDLightSlider<BlueLight>>(mm2px(Vec(20.254, 80.858)),  module, _2DAffine::YOFF_POST_PARAM, _2DAffine::YOFF_POST_LIGHT));

		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(12.664, 51.587)), module, _2DAffine::CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(5.664, 56.587)), module, _2DAffine::CVSHEARX_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(19.664, 56.587)), module, _2DAffine::CVSHEARY_INPUT));

		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.533, 108.24)),  module, _2DAffine::IN1_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(17.795, 108.24)), module, _2DAffine::IN2_INPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(7.533, 118.046)),  module, _2DAffine::OUT1_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(17.795, 118.046)), module, _2DAffine::OUT2_OUTPUT));
	}

	void step() override {
		if (module) {
			panel->visible = ((((_2DAffine*)module)->panelTheme) == 0);
			darkPanel->visible  = ((((_2DAffine*)module)->panelTheme) == 1);
		}
		Widget::step();
	}


};


Model* model_2DAffine = createModel<_2DAffine, _2DAffineWidget>("2DAffine");
