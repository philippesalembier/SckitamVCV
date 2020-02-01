#include "plugin.hpp"


struct PolygonalVCO : Module {
	enum ParamIds {
		PITCH_PARAM,
		FMAMOUNT_PARAM,
		NPOLY_PARAM,
		TEETH_PARAM,
		NPOLYAMOUNT_PARAM,
		TEETHAMOUNT_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		PITCH_INPUT,
		FM_INPUT,
		NPOLYCV_INPUT,
		TEETHCV_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		X_OUTPUT,
		Y_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		NUM_LIGHTS
	};

	enum NPolyQuant{
		NONE,
		Q025,
		Q033,
		Q050,
		Q100,
		NUM_NPOLYQUANT
	};
	NPolyQuant	nPolyQuant;

	float 	pitch, PPhi[16]={}, Phi=0.f, modPhi[16]={}, modPhi1[16]={}, P=0.f, freq, NPoly, x, y, tmp1, tmp2;
	float	Teeth=0.f, fracDelay, PhiTr, fPprime, fP, Correction = 0.f, Gain = 1.f;
	float	Pout0[16]={}, Pout1[16]={}, Pout2[16]={}, Pout3[16]={}, Pout[16]={};
	float	xout[16]={}, yout[16]={};
	float	h0, h1, h2, h3, d1, d2, d3, d4, d5;
	int   	panelTheme;


	void onReset() override {
		nPolyQuant = NONE;
	}

	PolygonalVCO() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(PITCH_PARAM, -3.f, 3.f, 0.f, "Pitch (Octave)");
		configParam(FMAMOUNT_PARAM, 0.f, 1.f, 0.f, "FM amount");
		configParam(NPOLY_PARAM, 2.1f, 20.f, 4.f, "Number of polygon sides");
		configParam(TEETH_PARAM, 0.f, 1.f, 0.f, "Teeth value");
		configParam(NPOLYAMOUNT_PARAM, -1.f, 1.f, 0.f, "CV amount for N");
		configParam(TEETHAMOUNT_PARAM, -1.f, 1.f, 0.f, "CV amount for T");
		onReset();
		panelTheme = 0;
	}

	void process(const ProcessArgs& args) override {
		
		// Use the pitch input to get number of channels
		int channels  = std::max(inputs[PITCH_INPUT].getChannels(), 1);
		int channelsN = std::max(inputs[NPOLYCV_INPUT].getChannels(), 1);
		int channelsT = std::max(inputs[TEETHCV_INPUT].getChannels(), 1);

		for (int c = 0; c < channels; c++) {
			// Compute the frequency from the pitch parameter and input
        		pitch  = params[PITCH_PARAM].getValue() + inputs[PITCH_INPUT].getVoltage(c);

			pitch  = clamp(pitch, -4.f, 4.f);
        		freq   = dsp::FREQ_C4 * std::pow(2.f, pitch);
			freq  += 300.f * params[FMAMOUNT_PARAM].getValue() * inputs[FM_INPUT].getVoltage(c); 
	
			// Compute NPoly value
			if (channelsN == 1){
				NPoly   = params[NPOLY_PARAM].getValue() 
					+ params[NPOLYAMOUNT_PARAM].getValue() * inputs[NPOLYCV_INPUT].getVoltage(); 
			}
			else {
				NPoly   = params[NPOLY_PARAM].getValue() 
					+ params[NPOLYAMOUNT_PARAM].getValue() * inputs[NPOLYCV_INPUT].getVoltage(c); 
			}
			NPoly = fmin(NPoly,1.0f/(freq * args.sampleTime)); 
				
			switch(nPolyQuant) {
				case 0:
            				NPoly   = clamp(NPoly, 2.1f, 20.f);
            				break;
				case 1:
					NPoly   = round(4.f*NPoly)/4.f;
					NPoly   = clamp(NPoly, 2.25f, 20.f);
            				break;
				case 2:
					NPoly   = round(3.f*NPoly)/3.f;
					NPoly   = clamp(NPoly, 2.33333f, 20.f);
            				break;
				case 3:
            				NPoly   = round(2.f*NPoly)/2.f;
					NPoly   = clamp(NPoly, 2.5f, 20.f);
            				break;
				case 4:
					NPoly   = round(NPoly);
					NPoly   = clamp(NPoly, 3.f, 20.f);
            				break;
             			default: break;
			}

			// Compute the Teeth value
			if (channelsT == 1){
				Teeth = params[TEETH_PARAM].getValue() 
					+ 0.1f * params[TEETHAMOUNT_PARAM].getValue() * inputs[TEETHCV_INPUT].getVoltage(); 
			}
			else {
				Teeth = params[TEETH_PARAM].getValue() 
					+ 0.1f * params[TEETHAMOUNT_PARAM].getValue() * inputs[TEETHCV_INPUT].getVoltage(c); 
			}
			Teeth = clamp(Teeth,0.f, 1.f);

        		// Adapt the Teeth value to NPoly
			if (NPoly < 5.f){
				Teeth = Teeth * (-0.03f*NPoly*NPoly+0.4f*NPoly-0.66f);
			}
			else{
				Teeth = Teeth * (-0.0019f*NPoly*NPoly+0.07f*NPoly+0.2875f);
			}	

			// Accumulate the phase
        		PPhi[c] += freq * args.sampleTime;
        		if (PPhi[c] >= 0.5f)
           			PPhi[c] -= 1.f;
			else if (PPhi[c] < -0.5f)
	 	 		PPhi[c] += 1.f;
			Phi = 2.f * M_PI * PPhi[c];

			// Compute modPhi
			modPhi[c] += NPoly * freq * args.sampleTime;
		
			// Increment modPhi and Check if there is a discontinuity, if necessary compute data for PolyBlep and PolyBlam
			Correction = 0.f;
			if (modPhi[c] >= 1.0f){
				fracDelay = (modPhi[c] - 1.f)/(modPhi[c] - modPhi1[c]);
				tmp1 	  = 2.f * M_PI * freq * args.sampleTime;
				PhiTr 	  = Phi - fracDelay * tmp1;
				if (Teeth > 0.f){
					fP = std::cos(M_PI/NPoly) * (1.f/std::cos(-1.f*M_PI/NPoly+Teeth)-1.f/std::cos(M_PI/NPoly+Teeth));
				}
				fPprime   = -2.f * std::tan(M_PI/NPoly) * tmp1;
				fPprime   = std::cos(M_PI/NPoly) * (std::tan(Teeth-(M_PI/NPoly))/std::cos(Teeth-(M_PI/NPoly)) 
						                 -  std::tan(Teeth+(M_PI/NPoly))/std::cos(Teeth+(M_PI/NPoly))) * tmp1;
				Correction = 1.0;
				modPhi[c] -= 1.f;
			}
			else if (modPhi[c] < 0.0f){
				fracDelay = (modPhi[c])/(modPhi[c] - modPhi1[c]);
				tmp1 	  = 2.f * M_PI * freq *  args.sampleTime;
				PhiTr 	  = Phi - fracDelay * tmp1;
				if (Teeth > 0.f){
					fP = std::cos(M_PI/NPoly) * (1.f/std::cos(-M_PI/NPoly+Teeth)-1.f/std::cos(M_PI/NPoly+Teeth));
				}			
				fPprime    = 2.f * std::tan(M_PI/NPoly) * tmp1;
				fPprime    = -1.0f * std::cos(M_PI/NPoly) * (std::tan(Teeth-(M_PI/NPoly))/std::cos(Teeth-(M_PI/NPoly)) 
						                 -  std::tan(Teeth+(M_PI/NPoly))/std::cos(Teeth+(M_PI/NPoly))) * tmp1;
				Correction = 1.0;
				modPhi[c] += 1.f;
			}

			// Create Polygon
			P = std::cos(M_PI/NPoly)/std::cos((2*modPhi[c]-1)*M_PI/NPoly+Teeth);

			Pout0[c] = 0.f; 
			Pout1[c] = P;   
			if (Correction == 1.f){
				d1 = fracDelay; d2 = d1*d1; d3 = d2*d1; d4=d3*d1; d5=d4*d1;

				// PolyBlep correction (if Teeth is not zero)
				if (Teeth > 0.f){
					h3 =  0.03871f * d4 + 0.00617f * d3 + 0.00737f * d2 + 0.00029f * d1 ;
					h2 = -0.11656f * d4 + 0.14955f * d3 + 0.22663f * d2 + 0.18783f * d1 + 0.05254f;
					h1 =  0.11656f * d4 - 0.31670f * d3 + 0.02409f * d2 + 0.62351f * d1 - 0.5f;
					h0 = -0.03871f * d4 + 0.16102f * d3 - 0.25816f * d2 + 0.18839f * d1 - 0.05254f;

					Pout0[c] = Pout0[c] + fP*h0;
					Pout1[c] = Pout1[c] + fP*h1;
					Pout2[c] = Pout2[c] + fP*h2;
					Pout3[c] = Pout3[c] + fP*h3;
				}

				// PolyBlam Correction
				h0 = -0.008333 * d5 + 0.041667 * d4 - 0.083333 * d3 + 0.083333 * d2 - 0.041667 * d1 + 0.008333;
				h1 =  0.025    * d5 - 0.083333 * d4 +                 0.333333 * d2 - 0.5      * d1 + 0.233333;
				h2 = -0.025    * d5 + 0.041667 * d4 + 0.083333 * d3 + 0.083333 * d2 + 0.041667 * d1 + 0.008333;
				h3 =  0.008333 * d5;

				Pout0[c] = Pout0[c] + fPprime*h0;
				Pout1[c] = Pout1[c] + fPprime*h1;
				Pout2[c] = Pout2[c] + fPprime*h2;
				Pout3[c] = Pout3[c] + fPprime*h3;
			}

			// Output
			Gain = 1.f - 0.5f * Teeth;
			xout[c] = clamp(2.5f * Gain * Pout3[c] * std::cos(Phi), -10.f, 10.f);
			yout[c] = clamp(2.5f * Gain * Pout3[c] * std::sin(Phi), -10.f, 10.f);
	
			// Keep values for the next iteration
			modPhi1[c] = modPhi[c];
			Pout3[c] = Pout2[c]; Pout2[c] = Pout1[c]; Pout1[c] = Pout0[c];
		}

		// Set output values
		outputs[X_OUTPUT].setChannels(channels);
		outputs[Y_OUTPUT].setChannels(channels);
		outputs[X_OUTPUT].writeVoltages(xout);
		outputs[Y_OUTPUT].writeVoltages(yout);
	}	

	void setNPolyQuant(NPolyQuant nPolyQuant) {
		if (nPolyQuant == this->nPolyQuant)
			return;
		this->nPolyQuant = nPolyQuant;
	}

	json_t* dataToJson() override {
		json_t* rootJ = json_object();
		// N Quantization
		json_object_set_new(rootJ, "nPolyQuant", json_integer(nPolyQuant));
		// panelTheme
		json_object_set_new(rootJ, "panelTheme", json_integer(panelTheme));
		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		json_t* nPolyQuantJ = json_object_get(rootJ, "nPolyQuant");
		// N Quantization
		if (nPolyQuantJ)
			nPolyQuant = (NPolyQuant) json_integer_value(nPolyQuantJ);
		// panelTheme
		json_t *panelThemeJ = json_object_get(rootJ, "panelTheme");
		if (panelThemeJ)
			panelTheme = json_integer_value(panelThemeJ);	
	}

};

struct NPolyQuantValueItem : MenuItem {
	PolygonalVCO* module;
	PolygonalVCO::NPolyQuant nPolyQuant;
	void onAction(const event::Action& e) override {
		module->setNPolyQuant(nPolyQuant);
	}
};


struct NPolyQuantItem : MenuItem {
	PolygonalVCO* module;
	Menu* createChildMenu() override {
		Menu* menu = new Menu;
		std::vector<std::string> nPolyQuantNames = {
			"None",
			"0.25",
			"0.33",
			"0.50",
			"1.00",
		};
		for (int i = 0; i < PolygonalVCO::NUM_NPOLYQUANT; i++) {
			PolygonalVCO::NPolyQuant nPolyQuant = (PolygonalVCO::NPolyQuant) i;
			NPolyQuantValueItem* item = new NPolyQuantValueItem;
			item->text = nPolyQuantNames[i];
			item->rightText = CHECKMARK(module->nPolyQuant == nPolyQuant);
			item->module = module;
			item->nPolyQuant = nPolyQuant;
			menu->addChild(item);
		}
		return menu;
	}
};


struct PolygonalVCOWidget : ModuleWidget {

	SvgPanel* darkPanel;

	struct PanelThemeItem : MenuItem {
		PolygonalVCO *module;
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

		PolygonalVCO *module = dynamic_cast<PolygonalVCO*>(this->module);
		assert(module);

		// N Quantization
		menu->addChild(new MenuEntry);
		NPolyQuantItem* nPolyQuantItem = new NPolyQuantItem;
		nPolyQuantItem->text = "N: Quantization step";
		nPolyQuantItem->rightText = RIGHT_ARROW;
		nPolyQuantItem->module = module;
		menu->addChild(nPolyQuantItem);

		menu->addChild(spacerLabel);
		
		// Panel
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

	PolygonalVCOWidget(PolygonalVCO* module) {
		setModule(module);

		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/PolygonalVCO.svg")));
        	if (module) {
			darkPanel = new SvgPanel();
			darkPanel->setBackground(APP->window->loadSvg(asset::plugin(pluginInstance, "res/PolygonalVCO_dark.svg")));
			darkPanel->visible = false;
			addChild(darkPanel);
		}



		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(8.677, 27.007)), module, PolygonalVCO::PITCH_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(22.457,27.629)), module, PolygonalVCO::FMAMOUNT_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(8.677, 73.221)), module, PolygonalVCO::NPOLY_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(22.457, 73.221)), module, PolygonalVCO::TEETH_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(8.677, 85.505)), module, PolygonalVCO::NPOLYAMOUNT_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(22.457, 85.505)), module, PolygonalVCO::TEETHAMOUNT_PARAM));

		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(8.677, 42.657)), module, PolygonalVCO::PITCH_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(22.457, 42.657)), module, PolygonalVCO::FM_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(8.677, 95.829)), module, PolygonalVCO::NPOLYCV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(22.457, 95.829)), module, PolygonalVCO::TEETHCV_INPUT));

		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(8.677, 115.516)), module, PolygonalVCO::X_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(22.457, 115.516)), module, PolygonalVCO::Y_OUTPUT));
	}

	void step() override {
		if (module) {
			panel->visible = ((((PolygonalVCO*)module)->panelTheme) == 0);
			darkPanel->visible  = ((((PolygonalVCO*)module)->panelTheme) == 1);
		}
		Widget::step();
	}
};


Model* modelPolygonalVCO = createModel<PolygonalVCO, PolygonalVCOWidget>("PolygonalVCO");
