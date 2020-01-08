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

	float 	pitch, PPhi=0.f, Phi=0.f, modPhi=0.f, modPhi1=0.f, P=0.f, freq, NPoly, x, y, tmp1, tmp2;
	float	Teeth=0.f, fracDelay, PhiTr, fxprime, fyprime, fx, fy, Correction = 0.f, Gain = 1.f;
	float	xout0=0.f, xout1=0.f, xout2=0.f, xout3=0.f;
	float	yout0=0.f, yout1=0.f, yout2=0.f, yout3=0.f;
	float	h0, h1, h2, h3, d1, d2, d3, d4, d5;

	PolygonalVCO() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(PITCH_PARAM, -3.f, 3.f, 0.f, "");
		configParam(FMAMOUNT_PARAM, 0.f, 1.f, 0.f, "");
		configParam(NPOLY_PARAM, 2.1f, 20.f, 4.f, "");
		configParam(TEETH_PARAM, 0.f, 1.f, 0.f, "");
		configParam(NPOLYAMOUNT_PARAM, -1.f, 1.f, 0.f, "");
		configParam(TEETHAMOUNT_PARAM, -1.f, 1.f, 0.f, "");
	}

	void process(const ProcessArgs& args) override {

		// Compute the frequency from the pitch parameter and input
        	pitch  = params[PITCH_PARAM].getValue() + inputs[PITCH_INPUT].getVoltage();
        	pitch  = clamp(pitch, -4.f, 4.f);
        	freq   = dsp::FREQ_C4 * std::pow(2.f, pitch);
		freq  += 300.f * params[FMAMOUNT_PARAM].getValue() * inputs[FM_INPUT].getVoltage(); 
	
		// Compute NPoly value
		NPoly   = params[NPOLY_PARAM].getValue() 
			+ params[NPOLYAMOUNT_PARAM].getValue() * inputs[NPOLYCV_INPUT].getVoltage(); 
		NPoly   = clamp(NPoly, 2.1f, 20.f);

		// Compute the Teeth value
		Teeth = params[TEETH_PARAM].getValue() 
			+ 0.1f * params[TEETHAMOUNT_PARAM].getValue() * inputs[TEETHCV_INPUT].getVoltage(); 
		Teeth = clamp(Teeth,0.f, 1.f);
        	// Adapt the Teeth value to NPoly
		if (NPoly < 5.f){
			Teeth = Teeth * (-0.03f*NPoly*NPoly+0.4f*NPoly-0.66f);
		}
		else{
			Teeth = Teeth * (-0.0019f*NPoly*NPoly+0.07f*NPoly+0.2875f);
		}	
		
		// Accumulate the phase
        	PPhi += freq * args.sampleTime;
        	if (PPhi >= 0.5f)
           		PPhi -= 1.f;
		else if (PPhi < -0.5f)
	 	 	PPhi += 1.f;
		Phi = 2.f * M_PI * PPhi;

		// Compute modPhi
		modPhi += NPoly * freq * args.sampleTime;
		
		// Increment modPhi and Check if there is a discontinuity, if necessary compute data for PolyBlep and PolyBlam
		Correction = 0.f;
        	if (modPhi >= 1.0f){
           		fracDelay = (modPhi - 1.f)/(modPhi - modPhi1);
			tmp1 	  = 2.f * M_PI * freq * args.sampleTime;
			PhiTr 	  = Phi - fracDelay * tmp1;
			if (Teeth > 0.f){
				tmp2 = std::cos(M_PI/NPoly) * (1.f/std::cos(-1.f*M_PI/NPoly+Teeth)-1.f/std::cos(M_PI/NPoly+Teeth));
				fx   = std::cos(Phi) * tmp2;
				fy   = std::sin(Phi) * tmp2;
			}
			fxprime    = -2.f * std::tan(M_PI/NPoly) * std::cos(PhiTr) * tmp1;
			fyprime    = -2.f * std::tan(M_PI/NPoly) * std::sin(PhiTr) * tmp1;
			Correction = 1.0;
			modPhi 	  -= 1.f;
		}
		else if (modPhi < 0.0f){
			fracDelay = (modPhi)/(modPhi - modPhi1);
			tmp1 	  = 2.f * M_PI * freq *  args.sampleTime;
			PhiTr 	  = Phi - fracDelay * tmp1;
			if (Teeth > 0.f){
				tmp2 = std::cos(M_PI/NPoly) * (1.f/std::cos(-M_PI/NPoly+Teeth)-1.f/std::cos(M_PI/NPoly+Teeth));
				fx   = - std::cos(Phi) * tmp2;
				fy   = - std::sin(Phi) * tmp2;
			}			
			fxprime    = 2.f * std::tan(M_PI/NPoly) * std::cos(PhiTr) * tmp1;
			fyprime    = 2.f * std::tan(M_PI/NPoly) * std::sin(PhiTr) * tmp1;
			Correction = 1.0;
			modPhi 	  += 1.f;
		}

		// Create Polygon
		P = std::cos(M_PI/NPoly)/std::cos((2*modPhi-1)*M_PI/NPoly+Teeth);
		x = std::cos(Phi)*P;
		y = std::sin(Phi)*P;

		xout0 = 0.f; yout0 = 0.f;
		xout1 = x;   yout1 = y;
		if (Correction == 1.f){
			d1 = fracDelay; d2 = d1*d1; d3 = d2*d1; d4=d3*d1; d5=d4*d1;

			// PolyBlep correction (if Teeth is not zero)
			if (Teeth > 0.f){
				h3 =  0.03871f * d4 + 0.00617f * d3 + 0.00737f * d2 + 0.00029f * d1 ;
				h2 = -0.11656f * d4 + 0.14955f * d3 + 0.22663f * d2 + 0.18783f * d1 + 0.05254f;
            			h1 =  0.11656f * d4 - 0.31670f * d3 + 0.02409f * d2 + 0.62351f * d1 - 0.5f;
            			h0 = -0.03871f * d4 + 0.16102f * d3 - 0.25816f * d2 + 0.18839f * d1 - 0.05254f;

				xout0 = xout0 + fx*h0;
            			xout1 = xout1 + fx*h1;
            			xout2 = xout2 + fx*h2;
            			xout3 = xout3 + fx*h3;
            
            			yout0 = yout0 + fy*h0;
            			yout1 = yout1 + fy*h1;
            			yout2 = yout2 + fy*h2;
            			yout3 = yout3 + fy*h3;
			}

           		// PolyBlam Correction
			h0 = -0.008333 * d5 + 0.041667 * d4 - 0.083333 * d3 + 0.083333 * d2 - 0.041667 * d1 + 0.008333;
            		h1 =  0.025    * d5 - 0.083333 * d4 +                 0.333333 * d2 - 0.5      * d1 + 0.233333;
            		h2 = -0.025    * d5 + 0.041667 * d4 + 0.083333 * d3 + 0.083333 * d2 + 0.041667 * d1 + 0.008333;
            		h3 =  0.008333 * d5;

			xout0 = xout0 + fxprime*h0;
        		xout1 = xout1 + fxprime*h1;
        		xout2 = xout2 + fxprime*h2;
        		xout3 = xout3 + fxprime*h3;
        
        		yout0 = yout0 + fyprime*h0;
        		yout1 = yout1 + fyprime*h1;
        		yout2 = yout2 + fyprime*h2;
        		yout3 = yout3 + fyprime*h3;
		}
	
		// Output
		Gain = 1.f - 0.5f * Teeth;
		outputs[X_OUTPUT].setVoltage(clamp(2.5f * Gain * xout3, -10.f, 10.f));
		outputs[Y_OUTPUT].setVoltage(clamp(2.5f * Gain * yout3, -10.f, 10.f));
		
		// Keep values for the next iteration
		modPhi1 = modPhi;
		xout3 = xout2; xout2 = xout1; xout1 = xout0;
    		yout3 = yout2; yout2 = yout1; yout1 = yout0;
	}
};


struct PolygonalVCOWidget : ModuleWidget {
	PolygonalVCOWidget(PolygonalVCO* module) {
		setModule(module);
		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/PolygonalVCO.svg")));

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
};


Model* modelPolygonalVCO = createModel<PolygonalVCO, PolygonalVCOWidget>("PolygonalVCO");
