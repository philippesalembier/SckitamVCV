#pragma once
#include <rack.hpp>


using namespace rack;

// Declare the Plugin, defined in plugin.cpp
extern Plugin* pluginInstance;

// Declare each Model, defined in each module source file
// extern Model* modelMyModule;
extern Model* model_2DRotation;
extern Model* model_2DAffine;
extern Model* modelMarkovSeq;
extern Model* modelPolygonalVCO;
extern Model* modelWDelay;
extern Model* modelFIFOQueue;
