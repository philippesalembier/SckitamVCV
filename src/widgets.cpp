#include "widgets.hpp"

ScButton::ScButton(const char* offSvgPath, const char* onSvgPath) {
	shadow = new CircularShadow();
	addChild(shadow);

	_svgWidget = new SvgWidget();
	addChild(_svgWidget);

	auto svg = APP->window->loadSvg(asset::plugin(pluginInstance, offSvgPath));
	_frames.push_back(svg);
	_frames.push_back(APP->window->loadSvg(asset::plugin(pluginInstance, onSvgPath)));

	_svgWidget->setSvg(svg);
	box.size = _svgWidget->box.size;
	shadow->box.size = _svgWidget->box.size;
	shadow->blurRadius = 1.0;
	shadow->box.pos = Vec(0.0, 1.0);
}

void ScButton::onDragStart(const event::DragStart& e) {
	if (paramQuantity) {
		_svgWidget->setSvg(_frames[1]);
		if (paramQuantity->getValue() >= paramQuantity->getMaxValue()) {
			paramQuantity->setValue(paramQuantity->getMinValue());
		}
		else {
			paramQuantity->setValue(paramQuantity->getValue() + 1.0);
		}
	}
}

void ScButton::onDragEnd(const event::DragEnd& e) { _svgWidget->setSvg(_frames[0]);}


ScButton1::ScButton1() : ScButton("res/button1-off.svg", "res/button1-on.svg") { }

