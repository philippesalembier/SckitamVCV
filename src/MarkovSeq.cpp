#include "plugin.hpp"
#include "widgets.hpp"

struct MarkovSeq : Module {
	enum ParamIds {
		STEP_PARAM,
		SCALE_PARAM,
		SLEW_PARAM,
		ENUMS(VAL_PARAMS, 8),
		ENUMS(FORCE_PARAMS, 8),
		ENUMS(P0_PARAMS, 8),
		ENUMS(P1_PARAMS, 8),
		ENUMS(P2_PARAMS, 8),
		ENUMS(P3_PARAMS, 8),
		ENUMS(P4_PARAMS, 8),
		ENUMS(P5_PARAMS, 8),
		ENUMS(P6_PARAMS, 8),
		ENUMS(P7_PARAMS, 8),
		ENUMS(TO_S_ON_PARAMS, 	8),
		ENUMS(TO_S_OFF_PARAMS, 	8),
		ENUMS(FROM_S_ON_PARAMS,	8),
		ENUMS(FROM_S_OFF_PARAMS,8),
		NUM_PARAMS
	};
	enum InputIds {
		CLOCK_INPUT,
		ENUMS(IN_INPUTS, 8),
		ENUMS(GFORCE_INPUTS, 8),
		NUM_INPUTS
	};
	enum OutputIds {
		OUT_OUTPUT,
		STATE_OUTPUT,
		ENUMS(TRIGGER_STATE, 8),
		NUM_OUTPUTS
	};
	enum LightIds {
		STEP_LIGHT,
		ENUMS(CUR_LIGHTS, 8),
		ENUMS(NEXT_LIGHTS, 8),
		NUM_LIGHTS
	};

	enum ZeroSum {
		DELTA,
		UNIFORM,
		NUM_ZERO_SUM
	};
	ZeroSum zeroSum;
	
	int 	lastChannels = 1;
	float 	lastGains[16] = {};
	int	CurrentState = 0, NextState = 0;
	float 	valout = 0.f, valout1, valout2, Gain, slewval = 0, slewout = 0, delta;
	float 	Prob[8] = {0.f}, SumP = 0.f, pval = 0.f;
	dsp::BooleanTrigger stepTrigger, state0Trigger, state1Trigger, state2Trigger, state3Trigger, state4Trigger, state5Trigger, state6Trigger, state7Trigger;
	dsp::SlewLimiter clickFilters[8];
	dsp::PulseGenerator pulseGenerators[8];
	
	void onReset() override {
		zeroSum = DELTA;
	}
	
	MarkovSeq() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(STEP_PARAM,  0.f, 10.f, 0.f, "Manual Step");
		configParam(SCALE_PARAM, 0.f,  1.f, 1.f, "Output Gain");
		configParam(SLEW_PARAM, 0.f,  0.999f, 0.f, "Slew");
		for (int i = 0; i < 8; i++)
			configParam(VAL_PARAMS + i, -10.f, 10.f, 0.f, string::f("Val %d", i + 1));
		for (int i = 0; i < 8; i++)
			configParam(FORCE_PARAMS + i, 0.f, 10.f, 0.f, string::f("Force State %d", i));
		for (int i = 0; i < 8; i++){
			configParam(TO_S_OFF_PARAMS + i, 0.f, 1.f, 0.f, string::f("Remove transition to S%d", i));
			configParam(TO_S_ON_PARAMS + i, 0.f, 1.f, 0.f, string::f("Set transition to S%d to 0.5", i));
			configParam(FROM_S_OFF_PARAMS + i, 0.f, 1.f, 0.f, string::f("Remove transition from S%d", i));
			configParam(FROM_S_ON_PARAMS + i, 0.f, 1.f, 0.f, string::f("Set transition from S%d to 0.5", i));
		}	
		for (int i = 0; i < 8; i++) {
			configParam(P0_PARAMS + i, 0.f, 1.f, 0.f, string::f("P0->%d", i));
			configParam(P1_PARAMS + i, 0.f, 1.f, 0.f, string::f("P1->%d", i));
			configParam(P2_PARAMS + i, 0.f, 1.f, 0.f, string::f("P2->%d", i));
			configParam(P3_PARAMS + i, 0.f, 1.f, 0.f, string::f("P3->%d", i));
			configParam(P4_PARAMS + i, 0.f, 1.f, 0.f, string::f("P4->%d", i));
			configParam(P5_PARAMS + i, 0.f, 1.f, 0.f, string::f("P5->%d", i));
			configParam(P6_PARAMS + i, 0.f, 1.f, 0.f, string::f("P6->%d", i));
			configParam(P7_PARAMS + i, 0.f, 1.f, 0.f, string::f("P7->%d", i));
		}
		// Intialize such that the sequencer moves as a regular sequencer in the forward direction
		configParam(P0_PARAMS + 1, 0.f, 1.f, 0.5f, string::f("P0->%d", 1));
		configParam(P1_PARAMS + 2, 0.f, 1.f, 0.5f, string::f("P0->%d", 2));
		configParam(P2_PARAMS + 3, 0.f, 1.f, 0.5f, string::f("P0->%d", 3));
		configParam(P3_PARAMS + 4, 0.f, 1.f, 0.5f, string::f("P0->%d", 4));
		configParam(P4_PARAMS + 5, 0.f, 1.f, 0.5f, string::f("P0->%d", 5));
		configParam(P5_PARAMS + 6, 0.f, 1.f, 0.5f, string::f("P0->%d", 6));
		configParam(P6_PARAMS + 7, 0.f, 1.f, 0.5f, string::f("P0->%d", 7));
		configParam(P7_PARAMS + 0, 0.f, 1.f, 0.5f, string::f("P0->%d", 0));

		for (int i = 0; i < 8; i++) {
			clickFilters[i].rise = 400.f; // Hz
			clickFilters[i].fall = 400.f; // Hz
		}
		onReset();
	}

	void process(const ProcessArgs& args) override {
		bool FState0 = (params[FORCE_PARAMS].getValue() + inputs[GFORCE_INPUTS].getVoltage()) > 1.0f;
		if (state0Trigger.process(FState0)) {
			lights[NEXT_LIGHTS + NextState   ].setBrightness(0.f);
 			NextState = 0;
			lights[NEXT_LIGHTS + NextState   ].setBrightness(1.f);
		}
		bool FState1 = (params[FORCE_PARAMS+1].getValue() + inputs[GFORCE_INPUTS+1].getVoltage()) > 1.0f;
		if (state1Trigger.process(FState1)) {
			lights[NEXT_LIGHTS + NextState   ].setBrightness(0.f);
 			NextState = 1;
			lights[NEXT_LIGHTS + NextState   ].setBrightness(1.f);
		}
		bool FState2 = (params[FORCE_PARAMS+2].getValue() + inputs[GFORCE_INPUTS+2].getVoltage()) > 1.0f;
		if (state2Trigger.process(FState2)) {
			lights[NEXT_LIGHTS + NextState   ].setBrightness(0.f);
 			NextState = 2;
			lights[NEXT_LIGHTS + NextState   ].setBrightness(1.f);
		}
		bool FState3 = (params[FORCE_PARAMS+3].getValue() + inputs[GFORCE_INPUTS+3].getVoltage()) > 1.0f;
		if (state3Trigger.process(FState3)) {
			lights[NEXT_LIGHTS + NextState   ].setBrightness(0.f);
 			NextState = 3;
			lights[NEXT_LIGHTS + NextState   ].setBrightness(1.f);
		}
		bool FState4 = (params[FORCE_PARAMS+4].getValue() + inputs[GFORCE_INPUTS+4].getVoltage()) > 1.0f;
		if (state4Trigger.process(FState4)) {
			lights[NEXT_LIGHTS + NextState   ].setBrightness(0.f);
 			NextState = 4;
			lights[NEXT_LIGHTS + NextState   ].setBrightness(1.f);
		}
		bool FState5 = (params[FORCE_PARAMS+5].getValue() + inputs[GFORCE_INPUTS+5].getVoltage()) > 1.0f;
		if (state5Trigger.process(FState5)) {
			lights[NEXT_LIGHTS + NextState   ].setBrightness(0.f);
 			NextState = 5;
			lights[NEXT_LIGHTS + NextState   ].setBrightness(1.f);
		}
		bool FState6 = (params[FORCE_PARAMS+6].getValue() + inputs[GFORCE_INPUTS+6].getVoltage()) > 1.0f;
		if (state6Trigger.process(FState6)) {
			lights[NEXT_LIGHTS + NextState   ].setBrightness(0.f);
 			NextState = 6;
			lights[NEXT_LIGHTS + NextState   ].setBrightness(1.f);
		}
		bool FState7 = (params[FORCE_PARAMS+7].getValue() + inputs[GFORCE_INPUTS+7].getVoltage()) > 1.0f;
		if (state7Trigger.process(FState7)) {
			lights[NEXT_LIGHTS + NextState   ].setBrightness(0.f);
 			NextState = 7;
			lights[NEXT_LIGHTS + NextState   ].setBrightness(1.f);
		}

		// Update probabilites if necessary
		for (int i = 0; i < 8; i++) { 
			if (params[TO_S_OFF_PARAMS + i].getValue() > 0) {
				params[TO_S_OFF_PARAMS + i].setValue(0.f);
				params[P0_PARAMS + i].setValue(0.f);
				params[P1_PARAMS + i].setValue(0.f);
				params[P2_PARAMS + i].setValue(0.f);
				params[P3_PARAMS + i].setValue(0.f);
				params[P4_PARAMS + i].setValue(0.f);
				params[P5_PARAMS + i].setValue(0.f);
				params[P6_PARAMS + i].setValue(0.f);
				params[P7_PARAMS + i].setValue(0.f);
			}
			if (params[TO_S_ON_PARAMS + i].getValue() > 0) {
				params[TO_S_ON_PARAMS + i].setValue(0.f);
				params[P0_PARAMS + i].setValue(0.5f);
				params[P1_PARAMS + i].setValue(0.5f);
				params[P2_PARAMS + i].setValue(0.5f);
				params[P3_PARAMS + i].setValue(0.5f);
				params[P4_PARAMS + i].setValue(0.5f);
				params[P5_PARAMS + i].setValue(0.5f);
				params[P6_PARAMS + i].setValue(0.5f);
				params[P7_PARAMS + i].setValue(0.5f);
			}
			if (params[FROM_S_OFF_PARAMS + i].getValue() > 0) {
				params[FROM_S_OFF_PARAMS + i].setValue(0.f);
				if (i == 0){
					for (int j = 0; j < 8; j++) { params[P0_PARAMS + j].setValue(0.f);}
				}
				else if (i == 1){	
					for (int j = 0; j < 8; j++) { params[P1_PARAMS + j].setValue(0.f);}
				}
				else if (i == 2){	
					for (int j = 0; j < 8; j++) { params[P2_PARAMS + j].setValue(0.f);}
				}
				else if (i == 3){	
					for (int j = 0; j < 8; j++) { params[P3_PARAMS + j].setValue(0.f);}
				}
				else if (i == 4){	
					for (int j = 0; j < 8; j++) { params[P4_PARAMS + j].setValue(0.f);}
				}
				else if (i == 5){	
					for (int j = 0; j < 8; j++) { params[P5_PARAMS + j].setValue(0.f);}
				}
				else if (i == 6){	
					for (int j = 0; j < 8; j++) { params[P6_PARAMS + j].setValue(0.f);}
				}
				else if (i == 7){	
					for (int j = 0; j < 8; j++) { params[P7_PARAMS + j].setValue(0.f);}
				}
			}
			if (params[FROM_S_ON_PARAMS + i].getValue() > 0) {
				params[FROM_S_ON_PARAMS + i].setValue(0.f);
				if (i == 0){
					for (int j = 0; j < 8; j++) { params[P0_PARAMS + j].setValue(0.5f);}
				}
				else if (i == 1){	
					for (int j = 0; j < 8; j++) { params[P1_PARAMS + j].setValue(0.5f);}
				}
				else if (i == 2){	
					for (int j = 0; j < 8; j++) { params[P2_PARAMS + j].setValue(0.5f);}
				}
				else if (i == 3){	
					for (int j = 0; j < 8; j++) { params[P3_PARAMS + j].setValue(0.5f);}
				}
				else if (i == 4){	
					for (int j = 0; j < 8; j++) { params[P4_PARAMS + j].setValue(0.5f);}
				}
				else if (i == 5){	
					for (int j = 0; j < 8; j++) { params[P5_PARAMS + j].setValue(0.5f);}
				}
				else if (i == 6){	
					for (int j = 0; j < 8; j++) { params[P6_PARAMS + j].setValue(0.5f);}
				}
				else if (i == 7){	
					for (int j = 0; j < 8; j++) { params[P7_PARAMS + j].setValue(0.5f);}
				}
			}

		}

		// Process new clock event
		bool step = (params[STEP_PARAM].getValue() + inputs[CLOCK_INPUT].getVoltage()) > 1.0f;
		lights[STEP_PARAM].setBrightness(step);

		if (stepTrigger.process(step)) {
			// Lights off
			lights[CUR_LIGHTS  + CurrentState].setBrightness(0.f);
			lights[NEXT_LIGHTS + NextState   ].setBrightness(0.f);

			// Define new CurrentState & Generate a trigger
			CurrentState = NextState;
			pulseGenerators[CurrentState].trigger(1e-3f);
		
			// Compute the transition probabilities
			switch(CurrentState){
				case 0:	
					Prob[0] = params[P0_PARAMS].getValue();
					Prob[1] = params[P0_PARAMS+1].getValue();
					Prob[2] = params[P0_PARAMS+2].getValue();
					Prob[3] = params[P0_PARAMS+3].getValue();
					Prob[4] = params[P0_PARAMS+4].getValue();
					Prob[5] = params[P0_PARAMS+5].getValue();
					Prob[6] = params[P0_PARAMS+6].getValue();
					Prob[7] = params[P0_PARAMS+7].getValue();
					break;
				case 1:
					Prob[0] = params[P1_PARAMS].getValue();
					Prob[1] = params[P1_PARAMS+1].getValue();
					Prob[2] = params[P1_PARAMS+2].getValue();
					Prob[3] = params[P1_PARAMS+3].getValue();
					Prob[4] = params[P1_PARAMS+4].getValue();
					Prob[5] = params[P1_PARAMS+5].getValue();
					Prob[6] = params[P1_PARAMS+6].getValue();
					Prob[7] = params[P1_PARAMS+7].getValue();
					break;
				case 2:	
					Prob[0] = params[P2_PARAMS].getValue();
					Prob[1] = params[P2_PARAMS+1].getValue();
					Prob[2] = params[P2_PARAMS+2].getValue();
					Prob[3] = params[P2_PARAMS+3].getValue();
					Prob[4] = params[P2_PARAMS+4].getValue();
					Prob[5] = params[P2_PARAMS+5].getValue();
					Prob[6] = params[P2_PARAMS+6].getValue();
					Prob[7] = params[P2_PARAMS+7].getValue();
					break;
				case 3:
					Prob[0] = params[P3_PARAMS].getValue();
					Prob[1] = params[P3_PARAMS+1].getValue();
					Prob[2] = params[P3_PARAMS+2].getValue();
					Prob[3] = params[P3_PARAMS+3].getValue();
					Prob[4] = params[P3_PARAMS+4].getValue();
					Prob[5] = params[P3_PARAMS+5].getValue();
					Prob[6] = params[P3_PARAMS+6].getValue();
					Prob[7] = params[P3_PARAMS+7].getValue();
					break;
				case 4:	
					Prob[0] = params[P4_PARAMS].getValue();
					Prob[1] = params[P4_PARAMS+1].getValue();
					Prob[2] = params[P4_PARAMS+2].getValue();
					Prob[3] = params[P4_PARAMS+3].getValue();
					Prob[4] = params[P4_PARAMS+4].getValue();
					Prob[5] = params[P4_PARAMS+5].getValue();
					Prob[6] = params[P4_PARAMS+6].getValue();
					Prob[7] = params[P4_PARAMS+7].getValue();
					break;
				case 5:
					Prob[0] = params[P5_PARAMS].getValue();
					Prob[1] = params[P5_PARAMS+1].getValue();
					Prob[2] = params[P5_PARAMS+2].getValue();
					Prob[3] = params[P5_PARAMS+3].getValue();
					Prob[4] = params[P5_PARAMS+4].getValue();
					Prob[5] = params[P5_PARAMS+5].getValue();
					Prob[6] = params[P5_PARAMS+6].getValue();
					Prob[7] = params[P5_PARAMS+7].getValue();
					break;
				case 6:	
					Prob[0] = params[P6_PARAMS].getValue();
					Prob[1] = params[P6_PARAMS+1].getValue();
					Prob[2] = params[P6_PARAMS+2].getValue();
					Prob[3] = params[P6_PARAMS+3].getValue();
					Prob[4] = params[P6_PARAMS+4].getValue();
					Prob[5] = params[P6_PARAMS+5].getValue();
					Prob[6] = params[P6_PARAMS+6].getValue();
					Prob[7] = params[P6_PARAMS+7].getValue();
					break;
				case 7:
					Prob[0] = params[P7_PARAMS].getValue();
					Prob[1] = params[P7_PARAMS+1].getValue();
					Prob[2] = params[P7_PARAMS+2].getValue();
					Prob[3] = params[P7_PARAMS+3].getValue();
					Prob[4] = params[P7_PARAMS+4].getValue();
					Prob[5] = params[P7_PARAMS+5].getValue();
					Prob[6] = params[P7_PARAMS+6].getValue();
					Prob[7] = params[P7_PARAMS+7].getValue();
					break;
			}
			SumP = Prob[0]+Prob[1]+Prob[2]+Prob[3]+Prob[4]+Prob[5]+Prob[6]+Prob[7];
			if (SumP > 0.0){
				for (int i=0; i<8; i++) Prob[i] = Prob[i] / SumP; 
			}
			else {
				//DEBUG("zeroSum %d", zeroSum);
				// Depending on the context menu, define the df as a delta or as uniform 
				if (zeroSum == 0) {
					Prob[CurrentState]=1.f;
				}
				else {
					for (int i=0; i<8; i++) Prob[i] = 1.0f/8.0f; 
				}
			}

			// Define new NextState
			pval =  random::uniform();
			if 	(pval<  Prob[0] ) NextState = 0;
			else if (pval<( Prob[0]+Prob[1] )) NextState = 1;
			else if (pval<( Prob[0]+Prob[1]+Prob[2] )) NextState = 2;
			else if (pval<( Prob[0]+Prob[1]+Prob[2]+Prob[3] )) NextState = 3;
			else if (pval<( Prob[0]+Prob[1]+Prob[2]+Prob[3]+Prob[4] )) NextState = 4;
			else if (pval<( Prob[0]+Prob[1]+Prob[2]+Prob[3]+Prob[4]+Prob[5] )) NextState = 5;
			else if (pval<( Prob[0]+Prob[1]+Prob[2]+Prob[3]+Prob[4]+Prob[5]+Prob[6] )) NextState = 6;
			else 	NextState = 7;


			// Lights on
			lights[CUR_LIGHTS  + CurrentState].setBrightness(1.f);
			lights[NEXT_LIGHTS + NextState   ].setBrightness(1.f);
		}


	      	// OUTPUT
		// Slew de parameter evolution
		valout1 = params[VAL_PARAMS + CurrentState].getValue();
		slewval =  log10(199*params[SLEW_PARAM].getValue()+1)/log10(200);
		delta   = valout1 - slewout;
        	if (delta > 0){
            		slewout = slewout + (1.0 - slewval);
            		if (slewout > valout1){ slewout = valout1;}
        	} else {
           		slewout = slewout - (1.0 - slewval);
            		if (slewout < valout1){ slewout = valout1;}
		}
		
		// Prevent clicks on inputs
		valout2 = 0;
		float G0 = 0;
		for (int i = 0; i < 8; i++) {
			float gaininput = clickFilters[i].process(args.sampleTime, CurrentState == i);
			if (i==0){G0 = gaininput;}
			if (gaininput != 0.f) {
				float in = inputs[IN_INPUTS  + i].getVoltage();
				valout2 += in * gaininput;
			}
		}

		Gain = params[SCALE_PARAM].getValue();
		valout = clamp( (slewout+valout2) * Gain, -10.f, 10.f);
		// Output values 
		outputs[OUT_OUTPUT].setVoltage(valout);

		outputs[STATE_OUTPUT].setVoltage(CurrentState);	

		bool pulse = pulseGenerators[CurrentState].process(args.sampleTime);
		outputs[TRIGGER_STATE + CurrentState].setVoltage(pulse ? 10.f : 0.f);
	}

	void setZeroSum(ZeroSum zeroSum) {
		if (zeroSum == this->zeroSum)
			return;
		this->zeroSum = zeroSum;
	}

	json_t* dataToJson() override {
		json_t* rootJ = json_object();
		json_object_set_new(rootJ, "zeroSum", json_integer(zeroSum));
		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		json_t* zeroSumJ = json_object_get(rootJ, "zeroSum");
		if (zeroSumJ)
			zeroSum = (ZeroSum) json_integer_value(zeroSumJ);
	}

};

struct MarkovSeqVUKnob : SliderKnob {
	MarkovSeq* module = NULL;

	MarkovSeqVUKnob() {
		box.size = mm2px(Vec(5, 20));
	}

	void draw(const DrawArgs& args) override {
		nvgBeginPath(args.vg);
		nvgRoundedRect(args.vg, 0, 0, box.size.x, box.size.y, 2.0);
		nvgFillColor(args.vg, nvgRGB(0, 0, 0));
		nvgFill(args.vg);

		const Vec margin = Vec(3, 3);
		Rect r = box.zeroPos().grow(margin.neg());

		//int channels = module ? module->lastChannels : 1;
		float value = paramQuantity ? paramQuantity->getValue() : 1.f;

		// Segment value
		nvgBeginPath(args.vg);
		nvgRect(args.vg,
		        r.pos.x,
		        r.pos.y + r.size.y * (1 - value),
		        r.size.x,
		        r.size.y * value);
		if (value>0.0){
			nvgFillColor(args.vg, color::mult(color::GREEN, 1.00));}
		else{
			nvgFillColor(args.vg, color::mult(color::GREEN, 0.00));}
		nvgFill(args.vg);

		// Segment gain
		//nvgBeginPath(args.vg);
		//for (int c = 0; c < channels; c++) {
		//	float gain = module ? module->lastGains[c] : 1.f;
		//	if (gain >= 0.005f) {
		//		nvgRect(args.vg,
		//		        r.pos.x + r.size.x * c / channels,
		//		        r.pos.y + r.size.y * (1 - gain),
		//		        r.size.x / channels,
		//		        r.size.y * gain);
		//	}
		//}
		//nvgFillColor(args.vg, SCHEME_GREEN);
		//nvgFill(args.vg);

		// Invisible separators
		//const int segs = 25;
		//nvgBeginPath(args.vg);
		//for (int i = 1; i <= segs; i++) {
		//	nvgRect(args.vg,
		//	        r.pos.x - 1.0,
		//	        r.pos.y + r.size.y * i / segs,
		//	        r.size.x + 2.0,
		//	        1.0);
		//}
		//nvgFillColor(args.vg, color::BLACK);
		//nvgFill(args.vg);
	}
};

struct ZeroSumValueItem : MenuItem {
	MarkovSeq* module;
	MarkovSeq::ZeroSum zeroSum;
	void onAction(const event::Action& e) override {
		module->setZeroSum(zeroSum);
	}
};


struct ZeroSumItem : MenuItem {
	MarkovSeq* module;
	Menu* createChildMenu() override {
		Menu* menu = new Menu;
		std::vector<std::string> zeroSumNames = {
			"Remain on the current state",
			"Transitions to all states are equiprobable",
		};
		for (int i = 0; i < MarkovSeq::NUM_ZERO_SUM; i++) {
			MarkovSeq::ZeroSum zeroSum = (MarkovSeq::ZeroSum) i;
			ZeroSumValueItem* item = new ZeroSumValueItem;
			item->text = zeroSumNames[i];
			item->rightText = CHECKMARK(module->zeroSum == zeroSum);
			item->module = module;
			item->zeroSum = zeroSum;
			menu->addChild(item);
		}
		return menu;
	}
};


struct MarkovSeqWidget : ModuleWidget {
	MarkovSeqWidget(MarkovSeq* module) {
		setModule(module);
		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/MarkovSeq.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addParam(createParamCentered<LEDBezel>(mm2px(Vec(27.697, 9.782)), module,  MarkovSeq::STEP_PARAM));
		
		addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(20.811, 22.603)), module, MarkovSeq::VAL_PARAMS + 0));
		addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(20.811, 36.238)), module, MarkovSeq::VAL_PARAMS + 1));	
		addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(20.811, 49.872)), module, MarkovSeq::VAL_PARAMS + 2));
		addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(20.811, 63.507)), module, MarkovSeq::VAL_PARAMS + 3));
		addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(20.811, 77.142)), module, MarkovSeq::VAL_PARAMS + 4));
		addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(20.811, 90.776)), module, MarkovSeq::VAL_PARAMS + 5));
		addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(20.811, 104.411)), module, MarkovSeq::VAL_PARAMS + 6));
		addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(20.811, 118.046)), module, MarkovSeq::VAL_PARAMS + 7));

		addParam(createParamCentered<TL1105>(mm2px(Vec(129.356, 27.803)), module, MarkovSeq::FORCE_PARAMS + 0));
		addParam(createParamCentered<TL1105>(mm2px(Vec(129.45, 41.438)), module, MarkovSeq::FORCE_PARAMS + 1));
		addParam(createParamCentered<TL1105>(mm2px(Vec(129.45, 55.072)), module, MarkovSeq::FORCE_PARAMS + 2));
		addParam(createParamCentered<TL1105>(mm2px(Vec(129.45, 68.707)), module, MarkovSeq::FORCE_PARAMS + 3));
		addParam(createParamCentered<TL1105>(mm2px(Vec(129.45, 82.342)), module, MarkovSeq::FORCE_PARAMS + 4));
		addParam(createParamCentered<TL1105>(mm2px(Vec(129.45, 95.976)), module, MarkovSeq::FORCE_PARAMS + 5));
		addParam(createParamCentered<TL1105>(mm2px(Vec(129.45, 109.611)), module, MarkovSeq::FORCE_PARAMS + 6));
		addParam(createParamCentered<TL1105>(mm2px(Vec(129.45, 123.246)), module, MarkovSeq::FORCE_PARAMS + 7));
		
		MarkovSeqVUKnob* levelParamP11 = createParam<MarkovSeqVUKnob>(mm2px(Vec(27.699, 19.34)), module, MarkovSeq::P0_PARAMS + 0);
		levelParamP11->module = module; addParam(levelParamP11);
		MarkovSeqVUKnob* levelParamP12 = createParam<MarkovSeqVUKnob>(mm2px(Vec(32.708, 19.34)), module, MarkovSeq::P0_PARAMS + 1);
		levelParamP12->module = module; addParam(levelParamP12);
		MarkovSeqVUKnob* levelParamP13 = createParam<MarkovSeqVUKnob>(mm2px(Vec(37.716, 19.34)), module, MarkovSeq::P0_PARAMS + 2);
		levelParamP13->module = module; addParam(levelParamP13);
		MarkovSeqVUKnob* levelParamP14 = createParam<MarkovSeqVUKnob>(mm2px(Vec(42.725, 19.34)), module, MarkovSeq::P0_PARAMS + 3);
		levelParamP14->module = module; addParam(levelParamP14);
		MarkovSeqVUKnob* levelParamP15 = createParam<MarkovSeqVUKnob>(mm2px(Vec(47.733, 19.34)), module, MarkovSeq::P0_PARAMS + 4);
		levelParamP15->module = module; addParam(levelParamP15);
		MarkovSeqVUKnob* levelParamP16 = createParam<MarkovSeqVUKnob>(mm2px(Vec(52.742, 19.34)), module, MarkovSeq::P0_PARAMS + 5);
		levelParamP16->module = module; addParam(levelParamP16);
		MarkovSeqVUKnob* levelParamP17 = createParam<MarkovSeqVUKnob>(mm2px(Vec(57.75, 19.34)), module, MarkovSeq::P0_PARAMS + 6);
		levelParamP17->module = module; addParam(levelParamP17);
		MarkovSeqVUKnob* levelParamP18 = createParam<MarkovSeqVUKnob>(mm2px(Vec(62.759, 19.34)), module, MarkovSeq::P0_PARAMS + 7);
		levelParamP18->module = module; addParam(levelParamP18);

		MarkovSeqVUKnob* levelParamP21 = createParam<MarkovSeqVUKnob>(mm2px(Vec(69.965, 19.34)), module, MarkovSeq::P1_PARAMS + 0);
		levelParamP21->module = module; addParam(levelParamP21);
		MarkovSeqVUKnob* levelParamP22 = createParam<MarkovSeqVUKnob>(mm2px(Vec(74.973, 19.34)), module, MarkovSeq::P1_PARAMS + 1);
		levelParamP22->module = module; addParam(levelParamP22);
		MarkovSeqVUKnob* levelParamP23 = createParam<MarkovSeqVUKnob>(mm2px(Vec(79.982, 19.34)), module, MarkovSeq::P1_PARAMS + 2);
		levelParamP23->module = module; addParam(levelParamP23);
		MarkovSeqVUKnob* levelParamP24 = createParam<MarkovSeqVUKnob>(mm2px(Vec(84.99, 19.34)), module, MarkovSeq::P1_PARAMS + 3);
		levelParamP24->module = module; addParam(levelParamP24);
		MarkovSeqVUKnob* levelParamP25 = createParam<MarkovSeqVUKnob>(mm2px(Vec(89.999, 19.34)), module, MarkovSeq::P1_PARAMS + 4);
		levelParamP25->module = module; addParam(levelParamP25);
		MarkovSeqVUKnob* levelParamP26 = createParam<MarkovSeqVUKnob>(mm2px(Vec(95.007, 19.34)), module, MarkovSeq::P1_PARAMS + 5);
		levelParamP26->module = module; addParam(levelParamP26);
		MarkovSeqVUKnob* levelParamP27 = createParam<MarkovSeqVUKnob>(mm2px(Vec(100.016, 19.34)), module, MarkovSeq::P1_PARAMS + 6);
		levelParamP27->module = module; addParam(levelParamP27);
		MarkovSeqVUKnob* levelParamP28 = createParam<MarkovSeqVUKnob>(mm2px(Vec(105.024, 19.34)), module, MarkovSeq::P1_PARAMS + 7);
		levelParamP28->module = module; addParam(levelParamP28);

		MarkovSeqVUKnob* levelParamP31 = createParam<MarkovSeqVUKnob>(mm2px(Vec(27.699, 46.592)), module, MarkovSeq::P2_PARAMS + 0);
		levelParamP31->module = module; addParam(levelParamP31);
		MarkovSeqVUKnob* levelParamP32 = createParam<MarkovSeqVUKnob>(mm2px(Vec(32.708, 46.592)), module, MarkovSeq::P2_PARAMS + 1);
		levelParamP32->module = module; addParam(levelParamP32);
		MarkovSeqVUKnob* levelParamP33 = createParam<MarkovSeqVUKnob>(mm2px(Vec(37.716, 46.592)), module, MarkovSeq::P2_PARAMS + 2);
		levelParamP33->module = module; addParam(levelParamP33);
		MarkovSeqVUKnob* levelParamP34 = createParam<MarkovSeqVUKnob>(mm2px(Vec(42.725, 46.592)), module, MarkovSeq::P2_PARAMS + 3);
		levelParamP34->module = module; addParam(levelParamP34);
		MarkovSeqVUKnob* levelParamP35 = createParam<MarkovSeqVUKnob>(mm2px(Vec(47.733, 46.592)), module, MarkovSeq::P2_PARAMS + 4);
		levelParamP35->module = module; addParam(levelParamP35);
		MarkovSeqVUKnob* levelParamP36 = createParam<MarkovSeqVUKnob>(mm2px(Vec(52.742, 46.592)), module, MarkovSeq::P2_PARAMS + 5);
		levelParamP36->module = module; addParam(levelParamP36);
		MarkovSeqVUKnob* levelParamP37 = createParam<MarkovSeqVUKnob>(mm2px(Vec(57.75, 46.592)), module, MarkovSeq::P2_PARAMS + 6);
		levelParamP37->module = module; addParam(levelParamP37);
		MarkovSeqVUKnob* levelParamP38 = createParam<MarkovSeqVUKnob>(mm2px(Vec(62.759, 46.592)), module, MarkovSeq::P2_PARAMS + 7);
		levelParamP38->module = module; addParam(levelParamP38);

		MarkovSeqVUKnob* levelParamP41 = createParam<MarkovSeqVUKnob>(mm2px(Vec(69.965, 46.592)), module, MarkovSeq::P3_PARAMS + 0);
		levelParamP41->module = module; addParam(levelParamP41);
		MarkovSeqVUKnob* levelParamP42 = createParam<MarkovSeqVUKnob>(mm2px(Vec(74.973, 46.592)), module, MarkovSeq::P3_PARAMS + 1);
		levelParamP42->module = module; addParam(levelParamP42);
		MarkovSeqVUKnob* levelParamP43 = createParam<MarkovSeqVUKnob>(mm2px(Vec(79.982, 46.592)), module, MarkovSeq::P3_PARAMS + 2);
		levelParamP43->module = module; addParam(levelParamP43);
		MarkovSeqVUKnob* levelParamP44 = createParam<MarkovSeqVUKnob>(mm2px(Vec(84.99, 46.592)), module, MarkovSeq::P3_PARAMS + 3);
		levelParamP44->module = module; addParam(levelParamP44);
		MarkovSeqVUKnob* levelParamP45 = createParam<MarkovSeqVUKnob>(mm2px(Vec(89.999, 46.592)), module, MarkovSeq::P3_PARAMS + 4);
		levelParamP45->module = module; addParam(levelParamP45);
		MarkovSeqVUKnob* levelParamP46 = createParam<MarkovSeqVUKnob>(mm2px(Vec(95.007, 46.592)), module, MarkovSeq::P3_PARAMS + 5);
		levelParamP46->module = module; addParam(levelParamP46);
		MarkovSeqVUKnob* levelParamP47 = createParam<MarkovSeqVUKnob>(mm2px(Vec(100.016, 46.592)), module, MarkovSeq::P3_PARAMS + 6);
		levelParamP47->module = module; addParam(levelParamP47);
		MarkovSeqVUKnob* levelParamP48 = createParam<MarkovSeqVUKnob>(mm2px(Vec(105.024, 46.592)), module, MarkovSeq::P3_PARAMS + 7);
		levelParamP48->module = module; addParam(levelParamP48);

		MarkovSeqVUKnob* levelParamP51 = createParam<MarkovSeqVUKnob>(mm2px(Vec(27.699, 73.845)), module, MarkovSeq::P4_PARAMS + 0);
		levelParamP51->module = module; addParam(levelParamP51);
		MarkovSeqVUKnob* levelParamP52 = createParam<MarkovSeqVUKnob>(mm2px(Vec(32.708, 73.845)), module, MarkovSeq::P4_PARAMS + 1);
		levelParamP52->module = module; addParam(levelParamP52);
		MarkovSeqVUKnob* levelParamP53 = createParam<MarkovSeqVUKnob>(mm2px(Vec(37.716, 73.845)), module, MarkovSeq::P4_PARAMS + 2);
		levelParamP53->module = module; addParam(levelParamP53);
		MarkovSeqVUKnob* levelParamP54 = createParam<MarkovSeqVUKnob>(mm2px(Vec(42.725, 73.845)), module, MarkovSeq::P4_PARAMS + 3);
		levelParamP54->module = module; addParam(levelParamP54);
		MarkovSeqVUKnob* levelParamP55 = createParam<MarkovSeqVUKnob>(mm2px(Vec(47.733, 73.845)), module, MarkovSeq::P4_PARAMS + 4);
		levelParamP55->module = module; addParam(levelParamP55);
		MarkovSeqVUKnob* levelParamP56 = createParam<MarkovSeqVUKnob>(mm2px(Vec(52.742, 73.845)), module, MarkovSeq::P4_PARAMS + 5);
		levelParamP56->module = module; addParam(levelParamP56);
		MarkovSeqVUKnob* levelParamP57 = createParam<MarkovSeqVUKnob>(mm2px(Vec(57.75, 73.845)), module, MarkovSeq::P4_PARAMS + 6);
		levelParamP57->module = module; addParam(levelParamP57);
		MarkovSeqVUKnob* levelParamP58 = createParam<MarkovSeqVUKnob>(mm2px(Vec(62.759, 73.845)), module, MarkovSeq::P4_PARAMS + 7);
		levelParamP58->module = module; addParam(levelParamP58);

		MarkovSeqVUKnob* levelParamP61 = createParam<MarkovSeqVUKnob>(mm2px(Vec(69.965, 73.845)), module, MarkovSeq::P5_PARAMS + 0);
		levelParamP61->module = module; addParam(levelParamP61);
		MarkovSeqVUKnob* levelParamP62 = createParam<MarkovSeqVUKnob>(mm2px(Vec(74.973, 73.845)), module, MarkovSeq::P5_PARAMS + 1);
		levelParamP62->module = module; addParam(levelParamP62);
		MarkovSeqVUKnob* levelParamP63 = createParam<MarkovSeqVUKnob>(mm2px(Vec(79.982, 73.845)), module, MarkovSeq::P5_PARAMS + 2);
		levelParamP63->module = module; addParam(levelParamP63);
		MarkovSeqVUKnob* levelParamP64 = createParam<MarkovSeqVUKnob>(mm2px(Vec(84.99, 73.845)), module, MarkovSeq::P5_PARAMS + 3);
		levelParamP64->module = module; addParam(levelParamP64);
		MarkovSeqVUKnob* levelParamP65 = createParam<MarkovSeqVUKnob>(mm2px(Vec(89.999, 73.845)), module, MarkovSeq::P5_PARAMS + 4);
		levelParamP65->module = module; addParam(levelParamP65);
		MarkovSeqVUKnob* levelParamP66 = createParam<MarkovSeqVUKnob>(mm2px(Vec(95.007, 73.845)), module, MarkovSeq::P5_PARAMS + 5);
		levelParamP66->module = module; addParam(levelParamP66);
		MarkovSeqVUKnob* levelParamP67 = createParam<MarkovSeqVUKnob>(mm2px(Vec(100.016, 73.845)), module, MarkovSeq::P5_PARAMS + 6);
		levelParamP67->module = module; addParam(levelParamP67);
		MarkovSeqVUKnob* levelParamP68 = createParam<MarkovSeqVUKnob>(mm2px(Vec(105.024, 73.845)), module, MarkovSeq::P5_PARAMS + 7);
		levelParamP68->module = module; addParam(levelParamP68);

		MarkovSeqVUKnob* levelParamP71 = createParam<MarkovSeqVUKnob>(mm2px(Vec(27.699, 101.098)), module, MarkovSeq::P6_PARAMS + 0);
		levelParamP71->module = module; addParam(levelParamP71);
		MarkovSeqVUKnob* levelParamP72 = createParam<MarkovSeqVUKnob>(mm2px(Vec(32.708, 101.098)), module, MarkovSeq::P6_PARAMS + 1);
		levelParamP72->module = module; addParam(levelParamP72);
		MarkovSeqVUKnob* levelParamP73 = createParam<MarkovSeqVUKnob>(mm2px(Vec(37.716, 101.098)), module, MarkovSeq::P6_PARAMS + 2);
		levelParamP73->module = module; addParam(levelParamP73);
		MarkovSeqVUKnob* levelParamP74 = createParam<MarkovSeqVUKnob>(mm2px(Vec(42.725, 101.098)), module, MarkovSeq::P6_PARAMS + 3);
		levelParamP74->module = module; addParam(levelParamP74);
		MarkovSeqVUKnob* levelParamP75 = createParam<MarkovSeqVUKnob>(mm2px(Vec(47.733, 101.098)), module, MarkovSeq::P6_PARAMS + 4);
		levelParamP75->module = module; addParam(levelParamP75);
		MarkovSeqVUKnob* levelParamP76 = createParam<MarkovSeqVUKnob>(mm2px(Vec(52.742, 101.098)), module, MarkovSeq::P6_PARAMS + 5);
		levelParamP76->module = module; addParam(levelParamP76);
		MarkovSeqVUKnob* levelParamP77 = createParam<MarkovSeqVUKnob>(mm2px(Vec(57.75, 101.098)), module, MarkovSeq::P6_PARAMS + 6);
		levelParamP77->module = module; addParam(levelParamP77);
		MarkovSeqVUKnob* levelParamP78 = createParam<MarkovSeqVUKnob>(mm2px(Vec(62.759, 101.098)), module, MarkovSeq::P6_PARAMS + 7);
		levelParamP78->module = module; addParam(levelParamP78);

		MarkovSeqVUKnob* levelParamP81 = createParam<MarkovSeqVUKnob>(mm2px(Vec(69.965, 101.098)), module, MarkovSeq::P7_PARAMS + 0);
		levelParamP81->module = module; addParam(levelParamP81);
		MarkovSeqVUKnob* levelParamP82 = createParam<MarkovSeqVUKnob>(mm2px(Vec(74.973, 101.098)), module, MarkovSeq::P7_PARAMS + 1);
		levelParamP82->module = module; addParam(levelParamP82);
		MarkovSeqVUKnob* levelParamP83 = createParam<MarkovSeqVUKnob>(mm2px(Vec(79.982, 101.098)), module, MarkovSeq::P7_PARAMS + 2);
		levelParamP83->module = module; addParam(levelParamP83);
		MarkovSeqVUKnob* levelParamP84 = createParam<MarkovSeqVUKnob>(mm2px(Vec(84.99, 101.098)), module, MarkovSeq::P7_PARAMS + 3);
		levelParamP84->module = module; addParam(levelParamP84);
		MarkovSeqVUKnob* levelParamP85 = createParam<MarkovSeqVUKnob>(mm2px(Vec(89.999, 101.098)), module, MarkovSeq::P7_PARAMS + 4);
		levelParamP85->module = module; addParam(levelParamP85);
		MarkovSeqVUKnob* levelParamP86 = createParam<MarkovSeqVUKnob>(mm2px(Vec(95.007, 101.098)), module, MarkovSeq::P7_PARAMS + 5);
		levelParamP86->module = module; addParam(levelParamP86);
		MarkovSeqVUKnob* levelParamP87 = createParam<MarkovSeqVUKnob>(mm2px(Vec(100.016, 101.098)), module, MarkovSeq::P7_PARAMS + 6);
		levelParamP87->module = module; addParam(levelParamP87);
		MarkovSeqVUKnob* levelParamP88 = createParam<MarkovSeqVUKnob>(mm2px(Vec(105.024, 101.098)), module, MarkovSeq::P7_PARAMS + 7);
		levelParamP88->module = module; addParam(levelParamP88);

		addParam(createParam<ScButtonMinus>(mm2px(Vec(35.0, 16.5)), module, MarkovSeq::TO_S_OFF_PARAMS + 0));
		addParam(createParam<ScButtonPlus>(mm2px(Vec(38.0, 16.5)), module, MarkovSeq::TO_S_ON_PARAMS + 0));
		addParam(createParam<ScButtonMinus>(mm2px(Vec(77.5, 16.5)), module, MarkovSeq::TO_S_OFF_PARAMS + 1));
		addParam(createParam<ScButtonPlus>(mm2px(Vec(80.5, 16.5)), module, MarkovSeq::TO_S_ON_PARAMS + 1));
		addParam(createParam<ScButtonMinus>(mm2px(Vec(35.0, 43.6)), module, MarkovSeq::TO_S_OFF_PARAMS + 2));
		addParam(createParam<ScButtonPlus>(mm2px(Vec(38.0, 43.6)), module, MarkovSeq::TO_S_ON_PARAMS + 2));
		addParam(createParam<ScButtonMinus>(mm2px(Vec(77.5, 43.6)), module, MarkovSeq::TO_S_OFF_PARAMS + 3));
		addParam(createParam<ScButtonPlus>(mm2px(Vec(80.5, 43.6)), module, MarkovSeq::TO_S_ON_PARAMS + 3));
		addParam(createParam<ScButtonMinus>(mm2px(Vec(35.0, 70.9)), module, MarkovSeq::TO_S_OFF_PARAMS + 4));
		addParam(createParam<ScButtonPlus>(mm2px(Vec(38.0, 70.9)), module, MarkovSeq::TO_S_ON_PARAMS + 4));
		addParam(createParam<ScButtonMinus>(mm2px(Vec(77.5, 70.9)), module, MarkovSeq::TO_S_OFF_PARAMS + 5));
		addParam(createParam<ScButtonPlus>(mm2px(Vec(80.5, 70.9)), module, MarkovSeq::TO_S_ON_PARAMS + 5));
		addParam(createParam<ScButtonMinus>(mm2px(Vec(35.0, 98.3)), module, MarkovSeq::TO_S_OFF_PARAMS + 6));
		addParam(createParam<ScButtonPlus>(mm2px(Vec(38.0, 98.3)), module, MarkovSeq::TO_S_ON_PARAMS + 6));
		addParam(createParam<ScButtonMinus>(mm2px(Vec(77.5, 98.3)), module, MarkovSeq::TO_S_OFF_PARAMS + 7));
		addParam(createParam<ScButtonPlus>(mm2px(Vec(80.5, 98.3)), module, MarkovSeq::TO_S_ON_PARAMS + 7));

		addParam(createParam<ScButtonMinus>(mm2px(Vec(54.5, 16.5)), module, MarkovSeq::FROM_S_OFF_PARAMS + 0));
		addParam(createParam<ScButtonPlus>(mm2px(Vec(57.5, 16.5)), module, MarkovSeq::FROM_S_ON_PARAMS + 0));
		addParam(createParam<ScButtonMinus>(mm2px(Vec(97.0, 16.5)), module, MarkovSeq::FROM_S_OFF_PARAMS + 1));
		addParam(createParam<ScButtonPlus>(mm2px(Vec(100.0, 16.5)), module, MarkovSeq::FROM_S_ON_PARAMS + 1));
		addParam(createParam<ScButtonMinus>(mm2px(Vec(54.5, 43.6)), module, MarkovSeq::FROM_S_OFF_PARAMS + 2));
		addParam(createParam<ScButtonPlus>(mm2px(Vec(57.5, 43.6)), module, MarkovSeq::FROM_S_ON_PARAMS + 2));
		addParam(createParam<ScButtonMinus>(mm2px(Vec(97.0, 43.6)), module, MarkovSeq::FROM_S_OFF_PARAMS + 3));
		addParam(createParam<ScButtonPlus>(mm2px(Vec(100.0, 43.6)), module, MarkovSeq::FROM_S_ON_PARAMS + 3));
		addParam(createParam<ScButtonMinus>(mm2px(Vec(54.5, 70.9)), module, MarkovSeq::FROM_S_OFF_PARAMS + 4));
		addParam(createParam<ScButtonPlus>(mm2px(Vec(57.5, 70.9)), module, MarkovSeq::FROM_S_ON_PARAMS + 4));
		addParam(createParam<ScButtonMinus>(mm2px(Vec(97.0, 70.9)), module, MarkovSeq::FROM_S_OFF_PARAMS + 5));
		addParam(createParam<ScButtonPlus>(mm2px(Vec(100.0, 70.9)), module, MarkovSeq::FROM_S_ON_PARAMS + 5));
		addParam(createParam<ScButtonMinus>(mm2px(Vec(54.5, 98.3)), module, MarkovSeq::FROM_S_OFF_PARAMS + 6));
		addParam(createParam<ScButtonPlus>(mm2px(Vec(57.5, 98.3)), module, MarkovSeq::FROM_S_ON_PARAMS + 6));
		addParam(createParam<ScButtonMinus>(mm2px(Vec(97.0, 98.3)), module, MarkovSeq::FROM_S_OFF_PARAMS + 7));
		addParam(createParam<ScButtonPlus>(mm2px(Vec(100.0, 98.3)), module, MarkovSeq::FROM_S_ON_PARAMS + 7));

		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(8.305, 9.782)), module, MarkovSeq::CLOCK_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(8.305, 22.363)), module, MarkovSeq::IN_INPUTS + 0));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(8.305, 36.032)), module, MarkovSeq::IN_INPUTS + 1));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(8.305, 49.701)), module, MarkovSeq::IN_INPUTS + 2));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(8.305, 63.37)), module, MarkovSeq::IN_INPUTS + 3));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(8.305, 77.039)), module, MarkovSeq::IN_INPUTS + 4));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(8.305, 90.708)), module, MarkovSeq::IN_INPUTS + 5));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(8.305, 104.377)), module, MarkovSeq::IN_INPUTS + 6));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(8.305, 118.046)), module, MarkovSeq::IN_INPUTS + 7));
		
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(135.356, 22.363)), module, MarkovSeq::GFORCE_INPUTS + 0));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(135.356, 36.032)), module, MarkovSeq::GFORCE_INPUTS + 1));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(135.356, 49.701)), module, MarkovSeq::GFORCE_INPUTS + 2));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(135.356, 63.37)), module, MarkovSeq::GFORCE_INPUTS + 3));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(135.356, 77.039)), module, MarkovSeq::GFORCE_INPUTS + 4));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(135.356, 90.708)), module, MarkovSeq::GFORCE_INPUTS + 5));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(135.356, 104.377)), module, MarkovSeq::GFORCE_INPUTS + 6));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(135.356, 118.046)), module, MarkovSeq::GFORCE_INPUTS + 7));
	
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(106.738, 9.782)), module, MarkovSeq::OUT_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(125.676, 9.782)), module, MarkovSeq::STATE_OUTPUT));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(96.697, 9.782)), module,  MarkovSeq::SCALE_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(44.20, 9.782)), module,  MarkovSeq::SLEW_PARAM));

		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(146.2, 22.363)), module, MarkovSeq::TRIGGER_STATE + 0));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(146.2, 36.032)), module, MarkovSeq::TRIGGER_STATE + 1));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(146.2, 49.701)), module, MarkovSeq::TRIGGER_STATE + 2));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(146.2, 63.37)),  module, MarkovSeq::TRIGGER_STATE + 3));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(146.2, 77.039)), module, MarkovSeq::TRIGGER_STATE + 4));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(146.2, 90.708)), module, MarkovSeq::TRIGGER_STATE + 5));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(146.2, 104.377)), module, MarkovSeq::TRIGGER_STATE + 6));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(146.2, 118.046)), module, MarkovSeq::TRIGGER_STATE + 7));
		
		addChild(createLightCentered<LEDBezelLight<GreenLight>>(mm2px(Vec(27.697, 9.782)), module, MarkovSeq::STEP_LIGHT));
		addChild(createLightCentered<LargeLight<RedLight>>(mm2px(Vec(117.15, 22.521)), module, MarkovSeq::CUR_LIGHTS + 0));
		addChild(createLightCentered<LargeLight<BlueLight>>(mm2px(Vec(125.45, 22.521)), module, MarkovSeq::NEXT_LIGHTS + 0));
		addChild(createLightCentered<LargeLight<RedLight>>(mm2px(Vec(117.15, 36.188)), module, MarkovSeq::CUR_LIGHTS + 1));
		addChild(createLightCentered<LargeLight<BlueLight>>(mm2px(Vec(125.45, 36.188)), module, MarkovSeq::NEXT_LIGHTS + 1));
		addChild(createLightCentered<LargeLight<RedLight>>(mm2px(Vec(117.15, 49.855)), module, MarkovSeq::CUR_LIGHTS + 2));
		addChild(createLightCentered<LargeLight<BlueLight>>(mm2px(Vec(125.45, 49.855)), module, MarkovSeq::NEXT_LIGHTS + 2));
		addChild(createLightCentered<LargeLight<RedLight>>(mm2px(Vec(117.15, 63.522)), module, MarkovSeq::CUR_LIGHTS + 3));
		addChild(createLightCentered<LargeLight<BlueLight>>(mm2px(Vec(125.45, 63.522)), module, MarkovSeq::NEXT_LIGHTS + 3));
		addChild(createLightCentered<LargeLight<RedLight>>(mm2px(Vec(117.15, 77.189)), module, MarkovSeq::CUR_LIGHTS + 4));
		addChild(createLightCentered<LargeLight<BlueLight>>(mm2px(Vec(125.45, 77.189)), module, MarkovSeq::NEXT_LIGHTS + 4));
		addChild(createLightCentered<LargeLight<RedLight>>(mm2px(Vec(117.15, 90.856)), module, MarkovSeq::CUR_LIGHTS + 5));
		addChild(createLightCentered<LargeLight<BlueLight>>(mm2px(Vec(125.45, 90.856)), module, MarkovSeq::NEXT_LIGHTS + 5));
		addChild(createLightCentered<LargeLight<RedLight>>(mm2px(Vec(117.15, 104.523)), module, MarkovSeq::CUR_LIGHTS + 6));
		addChild(createLightCentered<LargeLight<BlueLight>>(mm2px(Vec(125.45, 104.523)), module, MarkovSeq::NEXT_LIGHTS + 6));
		addChild(createLightCentered<LargeLight<RedLight>>(mm2px(Vec(117.15, 118.19)), module, MarkovSeq::CUR_LIGHTS + 7));
		addChild(createLightCentered<LargeLight<BlueLight>>(mm2px(Vec(125.45, 118.19)), module, MarkovSeq::NEXT_LIGHTS + 7));
	}

	void appendContextMenu(Menu* menu) override {
		MarkovSeq* module = dynamic_cast<MarkovSeq*>(this->module);

		menu->addChild(new MenuEntry);

		ZeroSumItem* zeroSumItem = new ZeroSumItem;
		zeroSumItem->text = "If all probabilities are zero....";
		zeroSumItem->rightText = RIGHT_ARROW;
		zeroSumItem->module = module;
		menu->addChild(zeroSumItem);
	}

};


Model* modelMarkovSeq = createModel<MarkovSeq, MarkovSeqWidget>("MarkovSeq");
