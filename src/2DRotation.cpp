#include "plugin.hpp"


struct _2DRotation : Module {
	enum ParamIds {
		ANGLE_PARAM,
		AMOUNT_PARAM,
		XOFF_PRE_PARAM,
		YOFF_PRE_PARAM,
		XOFF_POST_PARAM,
		YOFF_POST_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		CV_INPUT,
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
	float y1 = 0.f;
	float y2 = 0.f;
	float Theta = 0.f;
	float DeltaTheta = 0.f;
	float xoff_pre = 0.f;
	float yoff_pre = 0.f;
	float xoff_post = 0.f;
	float yoff_post = 0.f;

	_2DRotation() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(ANGLE_PARAM, -180.f, 180.f, 0.f, "Deg.");
		configParam(AMOUNT_PARAM, -1.f, 1.f, 0.f, "");
		configParam(XOFF_PRE_PARAM, -10.f, 10.f, 0.0f, "X_Offset");
		configParam(YOFF_PRE_PARAM, -10.f, 10.f, 0.0f, "Y_Offset");
		configParam(XOFF_POST_PARAM, -10.f, 10.f, 0.0f, "X_Offset");
		configParam(YOFF_POST_PARAM, -10.f, 10.f, 0.0f, "Y_Offset");
	}

	void process(const ProcessArgs& args) override {
		// Read input values
		x1   = inputs[IN1_INPUT].getVoltage();
		x2   = inputs[IN2_INPUT].getVoltage();
		xoff_pre = params[XOFF_PRE_PARAM].getValue();
		yoff_pre = params[YOFF_PRE_PARAM].getValue();
		x1 = x1 + xoff_pre;
		x2 = x2 + yoff_pre;

		// Define rotation angle
		Theta = params[ANGLE_PARAM].getValue() / 180 * M_PI;
		DeltaTheta = M_PI / 5.0 * inputs[CV_INPUT].getVoltage() * params[AMOUNT_PARAM].getValue();
		Theta += DeltaTheta;

		// Rotation
		y1 = std::cos(Theta) * x1 + std::sin(Theta) * x2;
		y2 = - std::sin(Theta) * x1 + std::cos(Theta) * x2;	
		xoff_post = params[XOFF_POST_PARAM].getValue();
		yoff_post = params[YOFF_POST_PARAM].getValue();
	
		outputs[OUT1_OUTPUT].setVoltage(y1+xoff_post);
		outputs[OUT2_OUTPUT].setVoltage(y2+yoff_post);

		//Light
		lights[XOFF_PRE_LIGHT].setBrightness(std::abs(xoff_pre)/10);
		lights[YOFF_PRE_LIGHT].setBrightness(std::abs(yoff_pre)/10);
		lights[XOFF_POST_LIGHT].setBrightness(std::abs(xoff_post)/10);
		lights[YOFF_POST_LIGHT].setBrightness(std::abs(yoff_post)/10);
	}
};


struct _2DRotationWidget : ModuleWidget {
	_2DRotationWidget(_2DRotation* module) {
		setModule(module);
		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/2DRotation.svg")));

		addChild(createWidget<ScrewSilver>(Vec(0, 0)));
		//addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		//addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 1 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addParam(createParamCentered<RoundLargeBlackKnob>(mm2px(Vec(10.124, 31.317)), module, _2DRotation::ANGLE_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(10.124, 44.828)), module, _2DRotation::AMOUNT_PARAM));
		addParam(createLightParamCentered<LEDLightSlider<GreenLight>>(mm2px(Vec(2.714, 80.858)),  module, _2DRotation::XOFF_PRE_PARAM, _2DRotation::XOFF_PRE_LIGHT));
		addParam(createLightParamCentered<LEDLightSlider<GreenLight>>(mm2px(Vec(12.714, 80.858)), module, _2DRotation::YOFF_PRE_PARAM, _2DRotation::YOFF_PRE_LIGHT));
		addParam(createLightParamCentered<LEDLightSlider<BlueLight>>(mm2px(Vec(7.714, 80.858)),   module, _2DRotation::XOFF_POST_PARAM, _2DRotation::XOFF_POST_LIGHT));
		addParam(createLightParamCentered<LEDLightSlider<BlueLight>>(mm2px(Vec(17.714, 80.858)),  module, _2DRotation::YOFF_POST_PARAM, _2DRotation::YOFF_POST_LIGHT));

		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(10.124, 56.587)), module, _2DRotation::CV_INPUT));

		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(5.493, 108.24)),  module, _2DRotation::IN1_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(14.755, 108.24)), module, _2DRotation::IN2_INPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(5.493, 118.046)),  module, _2DRotation::OUT1_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(14.755, 118.046)), module, _2DRotation::OUT2_OUTPUT));
	}
};


Model* model_2DRotation = createModel<_2DRotation, _2DRotationWidget>("2DRotation");
