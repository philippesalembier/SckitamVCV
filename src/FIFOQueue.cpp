#include "plugin.hpp"
#include "widgets.hpp"

#define	BufferLength  128

struct FIFOQueue : Module {
	enum ParamIds {
		NUM_PARAMS
	};
	enum InputIds {
		TRIGW_INPUT,
		TRIGR_INPUT,
		LOOP_INPUT,
		CLEAR_INPUT,
		IN1_INPUT,
		IN2_INPUT,
		IN3_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		TRIGOUT_OUTPUT,
		QEMPTY_OUTPUT,
		OUT1_OUTPUT,
		OUT2_OUTPUT,
		OUT3_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		ENUMS(QSTATUS_LIGHT, 10),
		NUM_LIGHTS
	};

	float 	Buffer1[BufferLength] = {0.0},	Buffer2[BufferLength] = {0.0}, 	Buffer3[BufferLength] = {0.0};
      	float	in1, in2, in3, out1, out2, out3, outQFill; 
	int	IWrite=0, IRead=0, QFill=0, Q;
	dsp::BooleanTrigger WriteTrigger, ReadTrigger, ClearTrigger;
	dsp::PulseGenerator pulseGeneratorOut;


	FIFOQueue() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		
		configInput(TRIGW_INPUT, "Trigger to write in the queue,");
		configInput(TRIGR_INPUT, "Trigger to retrieve from the queue,");
		configInput(LOOP_INPUT, "Loop on/off,");
		configInput(CLEAR_INPUT, "Empty the queue,");
		configInput(IN1_INPUT, "Value to store in the first queue,");
		configInput(IN2_INPUT, "Value to store in the second queue,");
		configInput(IN3_INPUT, "Value to store in the third queue,");
	
		configOutput(TRIGOUT_OUTPUT, "Trigger each time a value is actually retrieved form the queue,");
		configOutput(QEMPTY_OUTPUT, "Indicate whether the queue is empty (10V) or not (0V),");
		configOutput(OUT1_OUTPUT, "Value retrieved from the first queue,");
		configOutput(OUT2_OUTPUT, "Value retrieved from the second queue,");
		configOutput(OUT3_OUTPUT, "Value retrieved from the third queue,");
	}

	void process(const ProcessArgs& args) override {

		// Process a new Clear event
		bool stepC = (inputs[CLEAR_INPUT].getVoltage()) > 1.0f;
		if (ClearTrigger.process(stepC)) {
			IWrite=0, IRead=0, QFill=0;
		}
		
		// Process a new Write event
		bool stepW = (inputs[TRIGW_INPUT].getVoltage()) > 1.0f;
		if (WriteTrigger.process(stepW)) {
		if (QFill<BufferLength) {
			in1 = inputs[IN1_INPUT].getVoltage();
			in2 = inputs[IN2_INPUT].getVoltage();
			in3 = inputs[IN3_INPUT].getVoltage();

			// Write in the buffer
			Buffer1[IWrite] = in1;
			Buffer2[IWrite] = in2;
			Buffer3[IWrite] = in3;

			QFill  +=1;
			IWrite +=1;
			if (IWrite>=BufferLength) IWrite = 0;

		}}

		// Process a new Read event
		bool stepR = (inputs[TRIGR_INPUT].getVoltage()) > 1.0f;
		if (ReadTrigger.process(stepR)) {
		if (QFill>0) {
			// Readin the buffer
			out1 = Buffer1[IRead];
			out2 = Buffer2[IRead];
			out3 = Buffer3[IRead];

			QFill -=1;
			IRead +=1;
			if (IRead>=BufferLength) IRead= 0;

			// Generate an output pulse
			pulseGeneratorOut.trigger(1e-3f);

			// Loop the sample if necesary
			if  (inputs[LOOP_INPUT].getVoltage() > 0.000001f) {
				// Write in the buffer
				Buffer1[IWrite] = out1;
				Buffer2[IWrite] = out2;
				Buffer3[IWrite] = out3;

				QFill  +=1;
				IWrite +=1;
				if (IWrite>=BufferLength) IWrite = 0;	
			}

		}}
		
		// Ouput
		outputs[OUT1_OUTPUT].setVoltage(out1);
		outputs[OUT2_OUTPUT].setVoltage(out2);
		outputs[OUT3_OUTPUT].setVoltage(out3);

		// Generate an output pulse
		bool pulse = pulseGeneratorOut.process(args.sampleTime);
		outputs[TRIGOUT_OUTPUT].setVoltage(pulse ? 10.f : 0.f);

		outQFill = 0.0f;
		if (QFill==0) {
			outQFill = 10.0f;
		}
		outputs[QEMPTY_OUTPUT].setVoltage(outQFill);

		// Lights
		lights[QSTATUS_LIGHT + 0].setBrightness(1.f);
		Q = std::ceil(8*float(QFill)/float(BufferLength));		
		for (int i=1; i<=8; i++){
			if (i<=Q){
				lights[QSTATUS_LIGHT + i].setBrightness(1.f);
			} 
			else {
				lights[QSTATUS_LIGHT + i].setBrightness(0.f);
			}

		}
		if (QFill==BufferLength){
			lights[QSTATUS_LIGHT + 9].setBrightness(1.f);
		}
		else {
			lights[QSTATUS_LIGHT + 9].setBrightness(0.f);
		}

	}
};

struct QueueLDisplay : TransparentWidget {
	FIFOQueue * module;
	NVGcolor txtCol;
	std::string fontPath;
	const int fh = 14; // font height


	QueueLDisplay(Vec pos) {
		box.pos = pos;
		box.size.y = fh;
		box.size.x = fh;
		setColor(0xBA, 0x00, 0x00, 0xFF);
		fontPath = asset::plugin(pluginInstance, "res/DejaVuSansMono.ttf");
	}

	QueueLDisplay(Vec pos, unsigned char r, unsigned char g, unsigned char b, unsigned char a) {
		box.pos = pos;
		box.size.y = fh;
		box.size.x = fh;
		setColor(r, g, b, a);
		fontPath = asset::plugin(pluginInstance, "res/DejaVuSansMono.ttf");
	}

	void setColor(unsigned char r, unsigned char g, unsigned char b, unsigned char a) {
		txtCol.r = r;
		txtCol.g = g;
		txtCol.b = b;
		txtCol.a = a;
	}


	void draw(const DrawArgs &args) override {
		char tbuf[4];
//		std::shared_ptr<Font> font = APP->window->loadFont(fontPath);

		if (module == NULL) return;
		snprintf(tbuf, sizeof(tbuf), "%d", module->QFill);

		TransparentWidget::draw(args);
		drawValue(args, tbuf);

	}

	void drawValue(const DrawArgs &args, const char * txt) {
		std::shared_ptr<Font> font = APP->window->loadFont(fontPath);
		Vec c = Vec(box.size.x/2, box.size.y);

		nvgFontFaceId(args.vg, font->handle);
		nvgFontSize(args.vg, fh);
		nvgTextLetterSpacing(args.vg, -2);
		nvgTextAlign(args.vg, NVG_ALIGN_CENTER);
		nvgFillColor(args.vg, nvgRGBA(txtCol.r, txtCol.g, txtCol.b, txtCol.a));
		nvgText(args.vg, c.x, c.y+fh-1, txt, NULL);
	}
};

struct FIFOQueueWidget : ModuleWidget {
	FIFOQueueWidget(FIFOQueue* module) {
		setModule(module);
		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/FIFOQueue.svg")));

		addChild(createWidget<ScrewSilver>(Vec(0, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 1 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(0, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 1 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		float offsetv = 2.0f;
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(6.445, 17.023+offsetv)), module, FIFOQueue::TRIGW_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(6.445, 32.741+offsetv)), module, FIFOQueue::TRIGR_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(6.445, 48.459+offsetv)), module, FIFOQueue::LOOP_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(6.445, 66.293+offsetv)), module, FIFOQueue::CLEAR_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(6.445, 87.0)), module, FIFOQueue::IN1_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(6.445, 97.5)), module, FIFOQueue::IN2_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(6.445, 108.0)), module, FIFOQueue::IN3_INPUT));

		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(18.734, 48.459+offsetv)), module, FIFOQueue::TRIGOUT_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(18.734, 66.293+offsetv)), module, FIFOQueue::QEMPTY_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(18.734, 87.0)), module, FIFOQueue::OUT1_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(18.734, 97.5)), module, FIFOQueue::OUT2_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(18.734, 108.0)), module, FIFOQueue::OUT3_OUTPUT));

		float delta  = 2.2f;
		float center = 30.09;
		addChild(createLightCentered<MediumLight<RedLight>>(mm2px(Vec(18.734, center - 4*delta)), module, FIFOQueue::QSTATUS_LIGHT + 9));
		addChild(createLightCentered<MediumLight<YellowLight>>(mm2px(Vec(18.734, center - 3*delta)), module, FIFOQueue::QSTATUS_LIGHT + 8));
		addChild(createLightCentered<MediumLight<YellowLight>>(mm2px(Vec(18.734, center - 2*delta)), module, FIFOQueue::QSTATUS_LIGHT + 7));
		addChild(createLightCentered<MediumLight<GreenLight>>(mm2px(Vec(18.734, center - 1*delta)), module, FIFOQueue::QSTATUS_LIGHT + 6));
		addChild(createLightCentered<MediumLight<GreenLight>>(mm2px(Vec(18.734, center + 0*delta)), module, FIFOQueue::QSTATUS_LIGHT + 5));
		addChild(createLightCentered<MediumLight<GreenLight>>(mm2px(Vec(18.734, center + 1*delta)), module, FIFOQueue::QSTATUS_LIGHT + 4));
		addChild(createLightCentered<MediumLight<GreenLight>>(mm2px(Vec(18.734, center + 2*delta)), module, FIFOQueue::QSTATUS_LIGHT + 3));
		addChild(createLightCentered<MediumLight<GreenLight>>(mm2px(Vec(18.734, center + 3*delta)), module, FIFOQueue::QSTATUS_LIGHT + 2));
		addChild(createLightCentered<MediumLight<GreenLight>>(mm2px(Vec(18.734, center + 4*delta)), module, FIFOQueue::QSTATUS_LIGHT + 1));
		addChild(createLightCentered<MediumLight<BlueLight>>(mm2px(Vec(18.734, center + 5*delta)), module, FIFOQueue::QSTATUS_LIGHT + 0));

		{
			QueueLDisplay * sd = new QueueLDisplay(Vec(48,27));
			sd->module = module;
			addChild(sd);
		}

	}
};


Model* modelFIFOQueue = createModel<FIFOQueue, FIFOQueueWidget>("FIFOQueue");
