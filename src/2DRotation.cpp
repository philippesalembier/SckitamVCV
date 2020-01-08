#include "plugin.hpp"
#include "widgets.hpp"

struct _2DRotation : Module {
	enum ParamIds {
		ANGLE_PARAM,
		AMOUNT_PARAM,
		VANGLE_PARAM,
		VAMOUNT_PARAM,
		VHILO_PARAM,
		XOFF_PRE_PARAM,
		YOFF_PRE_PARAM,
		XOFF_POST_PARAM,
		YOFF_POST_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		CV1_INPUT,
		CV2_INPUT,
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
		HILOH_LIGHT,
		HILOL_LIGHT,
		NUM_LIGHTS
	};

	float x1 = 0.f;
	float x2 = 0.f;
	float y1 = 0.f;
	float y2 = 0.f;
	float Theta = 0.f, VTheta = 0.f, tmp;
	float DeltaTheta = 0.f;
	float xoff_pre = 0.f;
	float yoff_pre = 0.f;
	float xoff_post = 0.f;
	float yoff_post = 0.f;

	_2DRotation() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(ANGLE_PARAM, -180.f, 180.f, 0.f, "Deg.");
		configParam(AMOUNT_PARAM, -1.f, 1.f, 0.f, "Angle Modulation");
		configParam(VANGLE_PARAM, -3.f, 3.f, 0.f, "Velocity");
		configParam(VAMOUNT_PARAM, -1.f, 1.f, 0.f, "Velocity Modulation");
		configParam(VHILO_PARAM, 0.f, 1.f, 0.f, "High or Low Velocity");
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
		DeltaTheta = inputs[CV1_INPUT].getVoltage() * params[AMOUNT_PARAM].getValue() / 5.0f;
		tmp =  params[VANGLE_PARAM].getValue() + inputs[CV2_INPUT].getVoltage() * params[VAMOUNT_PARAM].getValue(); 

		if(params[VHILO_PARAM].getValue() == 0){
			VTheta += 60.0f * clamp(tmp,-3.f,3.f) * args.sampleTime;
			lights[HILOH_LIGHT].setBrightness(0.f);
			lights[HILOL_LIGHT].setBrightness(1.f);
		}
		else{
        		tmp  = clamp(tmp, -3.f, 3.f);
        		tmp  = dsp::FREQ_C4 * std::pow(2.f, tmp+1.0f);

			VTheta += tmp * args.sampleTime;
			lights[HILOH_LIGHT].setBrightness(1.f);
			lights[HILOL_LIGHT].setBrightness(0.f);
		}

		if (VTheta >= 1.0f) {
			VTheta -= 2.0f;
		}
		else if (VTheta < -1.0f) {
			VTheta += 2.0f;
		}

		Theta += M_PI * (DeltaTheta + VTheta);

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

		addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(5.493, 31.317)), module, _2DRotation::ANGLE_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(5.493, 46.828)), module, _2DRotation::AMOUNT_PARAM));
		addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(14.755, 31.317)), module, _2DRotation::VANGLE_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(14.755, 46.828)), module, _2DRotation::VAMOUNT_PARAM));
		addParam(createParam<ScButton1>(mm2px(Vec(11.7, 37.3)), module, _2DRotation::VHILO_PARAM));
		addChild(createLight<SmallLight<GreenLight>>(mm2px(Vec(15.5, 35.8)), module, _2DRotation::HILOH_LIGHT));
		addChild(createLight<SmallLight<GreenLight>>(mm2px(Vec(15.5, 38.8)), module, _2DRotation::HILOL_LIGHT));
		
		addParam(createLightParamCentered<LEDLightSlider<GreenLight>>(mm2px(Vec(2.714, 80.858)),  module, _2DRotation::XOFF_PRE_PARAM, _2DRotation::XOFF_PRE_LIGHT));
		addParam(createLightParamCentered<LEDLightSlider<GreenLight>>(mm2px(Vec(12.714, 80.858)), module, _2DRotation::YOFF_PRE_PARAM, _2DRotation::YOFF_PRE_LIGHT));
		addParam(createLightParamCentered<LEDLightSlider<BlueLight>>(mm2px(Vec(7.714, 80.858)),   module, _2DRotation::XOFF_POST_PARAM, _2DRotation::XOFF_POST_LIGHT));
		addParam(createLightParamCentered<LEDLightSlider<BlueLight>>(mm2px(Vec(17.714, 80.858)),  module, _2DRotation::YOFF_POST_PARAM, _2DRotation::YOFF_POST_LIGHT));

		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(5.493, 56.587)), module, _2DRotation::CV1_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(14.755, 56.587)), module, _2DRotation::CV2_INPUT));

		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(5.493, 108.24)),  module, _2DRotation::IN1_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(14.755, 108.24)), module, _2DRotation::IN2_INPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(5.493, 118.046)),  module, _2DRotation::OUT1_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(14.755, 118.046)), module, _2DRotation::OUT2_OUTPUT));
	}
};


Model* model_2DRotation = createModel<_2DRotation, _2DRotationWidget>("2DRotation");

