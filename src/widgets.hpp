#pragma once
#include "rack.hpp"

using namespace rack;
extern Plugin *pluginInstance;

struct ScButton : ParamWidget {
	std::vector<std::shared_ptr<Svg>> _frames;
	SvgWidget* _svgWidget;
	CircularShadow* shadow = NULL;

	ScButton(const char* offSvgPath, const char* onSvgPath);
	void onDragStart(const event::DragStart& e) override;
	void onDragEnd(const event::DragEnd& e) override;
};

struct RoundGreenKnob : RoundKnob {
	RoundGreenKnob() {
		setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/RoundGreenKnob.svg")));
	}
};
struct RoundBlueKnob : RoundKnob {
	RoundBlueKnob() {
		setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/RoundBlueKnob.svg")));
	}
};

struct ScButton1 : ScButton {ScButton1(); };
struct ScButtonMinus : ScButton {ScButtonMinus(); };
struct ScButtonPlus : ScButton {ScButtonPlus(); };
