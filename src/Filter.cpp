#include "RJModules.hpp"
#include <iostream>
#include <cmath>
#include <random>
#include "VAStateVariableFilter.h"

struct Filter: Module {
    enum ParamIds {
        FREQ_PARAM,
        RES_PARAM,
        MIX_PARAM,
        NUM_PARAMS
    };
    enum InputIds {
        CH1_INPUT,
        FREQ_CV_INPUT,
        RES_CV_INPUT,
        MIX_CV_INPUT,
        NUM_INPUTS
    };
    enum OutputIds {
        CH1_OUTPUT,
        NUM_OUTPUTS
    };

    VAStateVariableFilter *lpFilter = new VAStateVariableFilter() ; // create a lpFilter;
    VAStateVariableFilter *hpFilter = new VAStateVariableFilter() ; // create a hpFilter;

    Filter() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS);
        configParam(Filter::FREQ_PARAM, 0.0, 1.0, 0.5, "");
        configParam(Filter::RES_PARAM,  0.0, 1.0, .8, "");
        configParam(Filter::MIX_PARAM, 0.0, 1.0, 1.0, "");

    }
    void step() override;
};

void Filter::step() {

    float dry = inputs[CH1_INPUT].value;
    float wet = 0.0;
    dry += 1.0e-6 * (2.0*random::uniform() - 1.0)*1000;
    float mixed = 1.0;

    float param = params[FREQ_PARAM].value * clamp(inputs[FREQ_CV_INPUT].normalize(10.0f) / 10.0f, 0.0f, 10.0f);

    // if param < .5
    //      LPF, 30:8000
    // if param > .5
    //      HPF, 30:8000
    // if param == .5, wet = dry

    lpFilter->setFilterType(0);
    hpFilter->setFilterType(2);

    // todo get from param
    lpFilter->setResonance(params[RES_PARAM].value * clamp(inputs[RES_CV_INPUT].normalize(10.0f) / 10.0f, 0.0f, 1.0f));
    hpFilter->setResonance(params[RES_PARAM].value * clamp(inputs[RES_CV_INPUT].normalize(10.0f) / 10.0f, 0.0f, 1.0f));

    lpFilter->setSampleRate(APP->engine->getSampleRate());
    hpFilter->setSampleRate(APP->engine->getSampleRate());

    if(param < .5){

        // new_value = ( (old_value - old_min) / (old_max - old_min) ) * (new_max - new_min) + new_min
        float lp_cutoff = ( (param - 0) / (.5 - 0.0)) * (8000.0 - 30.0) + 30.0;
        lpFilter->setCutoffFreq(lp_cutoff);
        wet = lpFilter->processAudioSample(dry, 1);
    }
    if(param > .5){

        // new_value = ( (old_value - old_min) / (old_max - old_min) ) * (new_max - new_min) + new_min
        float hp_cutoff = ( (param - .5) / (1.0 - 0.5)) * (8000.0 - 200.0) + 200.0;
        hpFilter->setCutoffFreq(hp_cutoff);
        wet = hpFilter->processAudioSample(dry, 1);
    }
    if(param == .5){
        wet = dry;
    }

    float mix_cv = clamp(inputs[MIX_CV_INPUT].normalize(10.0f) / 10.0f, 0.0f, 1.0f);
    mixed = (wet * (params[MIX_PARAM].value * mix_cv)) + (dry * ((1-params[MIX_PARAM].value) * mix_cv));

    outputs[CH1_OUTPUT].value = mixed;
}


struct FilterWidget: ModuleWidget {
    FilterWidget(Filter *module);
};

FilterWidget::FilterWidget(Filter *module) {
		setModule(module);
    box.size = Vec(15*10, 380);

    {
        SVGPanel *panel = new SVGPanel();
        panel->box.size = box.size;
        panel->setBackground(APP->window->loadSvg(asset::plugin(pluginInstance, "res/Filter.svg")));
        addChild(panel);
    }

    addChild(createWidget<ScrewSilver>(Vec(15, 0)));
    addChild(createWidget<ScrewSilver>(Vec(box.size.x-30, 0)));
    addChild(createWidget<ScrewSilver>(Vec(15, 365)));
    addChild(createWidget<ScrewSilver>(Vec(box.size.x-30, 365)));

    addParam(createParam<RoundHugeBlackKnob>(Vec(47, 61), module, Filter::FREQ_PARAM));
    addParam(createParam<RoundHugeBlackKnob>(Vec(47, 143), module, Filter::RES_PARAM));
    addParam(createParam<RoundHugeBlackKnob>(Vec(47, 228), module, Filter::MIX_PARAM));

    addInput(createInput<PJ301MPort>(Vec(22, 100), module, Filter::FREQ_CV_INPUT));
    addInput(createInput<PJ301MPort>(Vec(22, 180), module, Filter::RES_CV_INPUT));
    addInput(createInput<PJ301MPort>(Vec(22, 260), module, Filter::MIX_CV_INPUT));
    addInput(createInput<PJ301MPort>(Vec(22, 310), module, Filter::CH1_INPUT));

    addOutput(createOutput<PJ301MPort>(Vec(110, 310), module, Filter::CH1_OUTPUT));
}

Model *modelFilter = createModel<Filter, FilterWidget>("Filter");
