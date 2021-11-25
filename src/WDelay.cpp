#include "plugin.hpp"
#include "widgets.hpp"

#define	BufferLength  (1<<21) 

struct WDelay : Module {
	enum ParamIds {
		DADJUST_PARAM,
		PITCH_PARAM,
		TIME_PARAM,
		PICKUP_PARAM,
		PICKUPMOD_PARAM,
		FBACK_PARAM,
		PM_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		PITCH_INPUT,
		PICKUPMOD_INPUT,
		IN_INPUT,
		FBACK_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		PICKUP_OUTPUT,
		OUT_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		P_LIGHT,
		M_LIGHT,
		NUM_LIGHTS
	};

	enum DelaySpec {
		PITCH,
		TIME,
		NUM_ZERO_SUM
	};

	DelaySpec delaySpec=PITCH;
	
	float 	Buffer[BufferLength] = {0.0}, x, y, ypickup, pitch, freq, delay, fDelay, IReadf, IPickupposf, PickupOut;
	float	tm1, t0, t1, t2, a, b, tmp, feedbackSign;
	int	IWrite=0, IRead0, IRead1, IRead2, IRead3, IPickuppos0, IPickuppos1;
	
	WDelay() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(DADJUST_PARAM, -16.f, 0.f, 0.f, "Delay adjustment (samples)");
		configParam(PITCH_PARAM, -4.f, 4.f, 0.f, "Pitch");
		configParam(TIME_PARAM, 0.f, 1.f, 0.f, "Time", " ms", 10.f / 1e-3, 1e-1);
		configParam(PICKUP_PARAM, 0.f, 1.f, 0.5f, "Pickup position");
		configParam(PICKUPMOD_PARAM, -1.f, 1.f, 0.f, "Pickup modulation amount");
		configParam(FBACK_PARAM, 0.f, 0.999f, 0.f, "Feedback amount");
		//configParam(PM_PARAM, 0.f, 1.f, 1.f, "Plus or Minus");
		configButton(PM_PARAM, "Plus or Minus");

		configInput(PITCH_INPUT, "Delay value");
		configInput(PICKUPMOD_INPUT, "Pickup position CV");
		configInput(IN_INPUT, "Signal");
		configInput(FBACK_INPUT, "Feedback");
		configOutput(OUT_OUTPUT, "Delay");
		configOutput(PICKUP_OUTPUT, "Pickup");


	}

	void process(const ProcessArgs& args) override {
		if (params[PM_PARAM].getValue() == 1){
			feedbackSign = 1.0f; 
			lights[P_LIGHT].setBrightness(1.f);
			lights[M_LIGHT].setBrightness(0.f);
		}
		else {
			feedbackSign = -1.0f;
			lights[P_LIGHT].setBrightness(0.f);
			lights[M_LIGHT].setBrightness(1.f);
		}

		x = inputs[IN_INPUT].getVoltage() 
	   	  + feedbackSign * params[FBACK_PARAM].getValue() * inputs[FBACK_INPUT].getVoltage();	

		// Write in the buffer
		Buffer[IWrite] = x;
		
		// Compute the delay
		if (delaySpec == 0) {
			pitch  = params[PITCH_PARAM].getValue() + inputs[PITCH_INPUT].getVoltage();
			pitch  = clamp(pitch, -4.f,4.f);
        		freq   = dsp::FREQ_C4 * std::pow(2.f, pitch);
			delay  = 1.0f / freq;
		}
		else{
			delay = params[TIME_PARAM].getValue();
			delay = 1e-4 * std::pow(10.f / 1e-3, delay);	
	 		delay =	delay * (1.0f+inputs[PITCH_INPUT].getVoltage()/5.0f);
			delay = clamp(delay,0.0001f, 1.0f);
		}

		// Read Buffer (Lagrange interpolation)
		IReadf = IWrite - delay * args.sampleRate - params[DADJUST_PARAM].getValue();
		IReadf = std::fmin(IReadf, IWrite-3.0f);
		IRead0 = floor(IReadf)-1;
    		IRead1 = IRead0 + 1;
    		IRead2 = IRead0 + 2;
    		IRead3 = IRead0 + 3;
    		fDelay = IReadf - floor(IReadf) + 1.0f;

		if (IRead0<0) IRead0 += BufferLength;
		if (IRead1<0) IRead1 += BufferLength;
		if (IRead2<0) IRead2 += BufferLength;
		if (IRead3<0) IRead3 += BufferLength;
	    	
		y = Buffer[IRead3] *  fDelay       * (fDelay-1.0f) * (fDelay-2.0f) / 6.0f 
	          - Buffer[IRead2] *  fDelay       * (fDelay-1.0f) * (fDelay-3.0f) / 2.0f 
        	  + Buffer[IRead1] *  fDelay       * (fDelay-2.0f) * (fDelay-3.0f) / 2.0f 
        	  - Buffer[IRead0] * (fDelay-1.0f) * (fDelay-2.0f) * (fDelay-3.0f) / 6.0f;

		// Alternative interpolation
    		//	IRead0 = floor(IReadf)-1;
    		//	IRead1 = IRead0 + 1;
    		//	IRead2 = IRead0 + 2;
    		//	IRead3 = IRead0 + 3;
    		//	fDelay = IReadf - floor(IReadf);

		//	if (IRead0<0) IRead0 += BufferLength;
		//	if (IRead1<0) IRead1 += BufferLength;
		//	if (IRead2<0) IRead2 += BufferLength;
		//	if (IRead3<0) IRead3 += BufferLength;
    		//
   		//	tm1 = Buffer[IRead0]; 
		//	t0  = Buffer[IRead1]; 
		//	t1  = Buffer[IRead2]; 
		//	t2  = Buffer[IRead3]; 
    		//	a = (t2-t0)/2.0f + (t1-tm1)/2.0f + (t0-t1) + (t0 - t1);
    		//	b = (t1-tm1)/2.0f + (t0-t1);
    		//	y = t0 + fDelay * ((t1-tm1)/2.0f + fDelay * (fDelay*a - (a+b)));

		y = clamp(y, -20.0f, 20.0f);

		// Pickup output (linear interoolation)
		tmp = params[PICKUP_PARAM].getValue() 
      		    + params[PICKUPMOD_PARAM].getValue() * inputs[PICKUPMOD_INPUT].getVoltage()/10.0f;	
		tmp = clamp(tmp, 0.f, 1.f);
		IPickupposf = IWrite - tmp*(IWrite-IReadf);
		IPickuppos0 = floor(IPickupposf);
		IPickuppos1 = IPickuppos0 + 1;
		fDelay = IPickupposf - IPickuppos0;
		if (IPickuppos0<0) IPickuppos0 += BufferLength;
		if (IPickuppos1<0) IPickuppos1 += BufferLength;
		PickupOut = (1.0f - fDelay) * Buffer[IPickuppos0] + fDelay * Buffer[IPickuppos1];

		// Ouput
		outputs[OUT_OUTPUT].setVoltage(y);
		outputs[PICKUP_OUTPUT].setVoltage(PickupOut);

		// Increment IWrite
		IWrite += 1;
		if (IWrite>=BufferLength) IWrite -= BufferLength;
	}
		
	void setDelaySpec(DelaySpec delaySpec) {
		if (delaySpec == this->delaySpec)
			return;
		this->delaySpec = delaySpec;
		onReset();
	}

	json_t* dataToJson() override {
		json_t* rootJ = json_object();
		json_object_set_new(rootJ, "delaySpec", json_integer(delaySpec));
		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		json_t* delaySpecJ = json_object_get(rootJ, "delaySpec");
		if (delaySpecJ)
			delaySpec = (DelaySpec) json_integer_value(delaySpecJ);
	}
};

struct DelaySpecValueItem : MenuItem {
	WDelay* module;
	WDelay::DelaySpec delaySpec;
	void onAction(const event::Action& e) override {
		module->setDelaySpec(delaySpec);
	}
};


struct DelaySpecItem : MenuItem {
	WDelay* module;
	Menu* createChildMenu() override {
		Menu* menu = new Menu;
		std::vector<std::string> delaySpecNames = {
			"Pitch",
			"Time",
		};
		for (int i = 0; i < WDelay::NUM_ZERO_SUM; i++) {
			WDelay::DelaySpec delaySpec = (WDelay::DelaySpec) i;
			DelaySpecValueItem* item = new DelaySpecValueItem;
			item->text = delaySpecNames[i];
			item->rightText = CHECKMARK(module->delaySpec == delaySpec);
			item->module = module;
			item->delaySpec = delaySpec;
			menu->addChild(item);
		}
		return menu;
	}
};

struct WDelayWidget : ModuleWidget {
	ParamWidget* PitchParam;
	ParamWidget* TimeParam;

	WDelayWidget(WDelay* module) {
		setModule(module);
		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/WDelay.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		PitchParam = createParamCentered<RoundGreenKnob>(mm2px(Vec(22.738, 24.67)), module, WDelay::PITCH_PARAM);
		addParam(PitchParam);
		TimeParam = createParamCentered<RoundBlueKnob>(mm2px(Vec(22.738, 24.67)), module, WDelay::TIME_PARAM);
		addParam(TimeParam);

		addParam(createParamCentered<Trimpot>(mm2px(Vec(7.621, 24.67)), module, WDelay::DADJUST_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(7.621, 46.464)), module, WDelay::PICKUP_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(7.621, 57.034)), module, WDelay::PICKUPMOD_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(10.618, 112.767)), module, WDelay::FBACK_PARAM));
		addParam(createParam<ScButton1>(mm2px(Vec(1.2, 103.9)), module, WDelay::PM_PARAM));
		addChild(createLight<SmallLight<GreenLight>>(mm2px(Vec(6.9, 102.9)), module, WDelay::P_LIGHT));
		addChild(createLight<SmallLight<GreenLight>>(mm2px(Vec(6.9, 105.9)), module, WDelay::M_LIGHT));

		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(22.738, 39.446)), module, WDelay::PITCH_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.621, 65.604)), module, WDelay::PICKUPMOD_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.621, 82.843)), module, WDelay::IN_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(23.211, 112.459)), module, WDelay::FBACK_INPUT));

		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(22.732, 65.604)), module, WDelay::PICKUP_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(22.732, 82.843)), module, WDelay::OUT_OUTPUT));
	}

	void step() override {
		WDelay* module = dynamic_cast<WDelay*>(this->module);

		if (module) {
			PitchParam->visible = (module->delaySpec == 0);
			TimeParam->visible  = (module->delaySpec == 1);
		}
		ModuleWidget::step();
	}

	void appendContextMenu(Menu *menu) override {

		WDelay *module = dynamic_cast<WDelay*>(this->module);
		assert(module);

		// Delay specificaton 
		menu->addChild(new MenuEntry);

		DelaySpecItem* delaySpecItem = new DelaySpecItem;
		delaySpecItem->text = "Delay specification...";
		delaySpecItem->rightText = RIGHT_ARROW;
		delaySpecItem->module = module;
		menu->addChild(delaySpecItem);
	}	
	
};


Model* modelWDelay = createModel<WDelay, WDelayWidget>("WDelay");
