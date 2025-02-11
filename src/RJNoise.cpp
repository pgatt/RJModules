#include <iostream>
#include <stdlib.h>
#include <random>
#include <cmath>

#include "RJModules.hpp"
#include "VAStateVariableFilter.h"

/*
    Voss pink noise algorithm from: http://www.firstpr.com.au/dsp/pink-noise/#Voss
*/

class PinkNumber
{

// goes from 0 to 118;
private:
  int max_key;
  int key;
  unsigned int white_values[5];
  unsigned int range;
public:
  PinkNumber(unsigned int range = 128)
    {
      max_key = 0x1f; // Five bits set
      this->range = range;
      key = 0;
      for (int i = 0; i < 5; i++){
        white_values[i] = rand() % (range/5);
      }
    }
  float GetNextValue()
    {
      int last_key = key;
      unsigned int sum;

      key++;
      if (key > max_key){
        key = 0;
      }
      // Exclusive-Or previous value with current value. This gives
      // a list of bits that have changed.
      int diff = last_key ^ key;
      sum = 0;
      for (int i = 0; i < 5; i++)
 {
   // If bit changed get new random number for corresponding
   // white_value
   if (diff & (1 << i))
     white_values[i] = rand() % (range/5);
   sum += white_values[i];
 }
      return 1.0 * (float)sum;
    }
};

struct Noise : Module {
    enum ParamIds {
        COLOR_PARAM,
        LPF_PARAM,
        HPF_PARAM,
        VOL_PARAM,
        NUM_PARAMS
    };
    enum InputIds {
        COLOR_CV_INPUT,
        LPF_CV_INPUT,
        HPF_CV_INPUT,
        NUM_INPUTS
    };
    enum OutputIds {
        NOISE_OUTPUT,
        NUM_OUTPUTS
    };
    enum LightIds {
        NUM_LIGHTS
    };

    PinkNumber pink = PinkNumber();

    float low = 99;
    float high = 0;
    float next;
    float mapped_pink = 0.0;
    float white = 0.0;
    float mixed = 0.0;
    float mix_value = 1.0;

    std::random_device rd; // obtain a random number from hardware

    float outLP;
    float outHP;

    VAStateVariableFilter *lpFilter = new VAStateVariableFilter() ; // create a lpFilter;
    VAStateVariableFilter *hpFilter = new VAStateVariableFilter() ; // create a hpFilter;

    Noise() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
configParam(Noise::COLOR_PARAM, 0.0, 1.0, 1.0, "");
configParam(Noise::LPF_PARAM, 0.0, 8000.0, 8000.0, "");
configParam(Noise::HPF_PARAM, 30.0, 8000.0, 30.0, "");
configParam(Noise::VOL_PARAM, 0.0, 2.0, 1.0, "");
  }
    void step() override;
};

void Noise::step(){

    next = pink.GetNextValue();
    mapped_pink = (next - 0) / (118 ) * 10 - 5;

    std::mt19937 eng(rd()); // seed the generator
    std::uniform_real_distribution<> distr1(-5, 5); // define the range
    white =  distr1(eng);

    mix_value = params[COLOR_PARAM].value * clamp(inputs[COLOR_CV_INPUT].normalize(10.0f) / 10.0f, 0.0f, 1.0f);
    mixed = ( (mapped_pink * mix_value) + (white * (1.0 - mix_value)) )/ 2;

    // filtration
    mixed += 1.0e-6 * (2.0*random::uniform() - 1.0)*1000;

    //float cutoffcv =  400;//*params[LPF_PARAM].value * inputs[FREQ_INPUT].value+ 400*inputs[FREQ_INPUT2].value *params[FREQ_CV_PARAM2].value ;
    float lp_cutoff = params[LPF_PARAM].value * clamp(inputs[LPF_CV_INPUT].normalize(10.0f) / 10.0f, 0.0f, 1.0f);;
    float hp_cutoff = params[HPF_PARAM].value * clamp(inputs[HPF_CV_INPUT].normalize(10.0f) / 10.0f, 0.0f, 1.0f);;  // + cutoffcv;

    lpFilter->setFilterType(0);
    hpFilter->setFilterType(2);

    lpFilter->setCutoffFreq(lp_cutoff);
    hpFilter->setCutoffFreq(hp_cutoff);

    lpFilter->setResonance(.6);
    hpFilter->setResonance(.6);

    lpFilter->setSampleRate(APP->engine->getSampleRate());
    hpFilter->setSampleRate(APP->engine->getSampleRate());

    mixed = lpFilter->processAudioSample(mixed, 1);
    mixed = hpFilter->processAudioSample(mixed, 1);

    // if you don't map to whatever, it just sounds like weird kinda cool crackles
    //outputs[NOISE_OUTPUT].value = pink.GetNextValue();
    outputs[NOISE_OUTPUT].value = mixed*2*params[VOL_PARAM].value;

}

struct NoiseWidget: ModuleWidget {
    NoiseWidget(Noise *module);
};

NoiseWidget::NoiseWidget(Noise *module) {
		setModule(module);
    box.size = Vec(15*10, 380);

    {
        SVGPanel *panel = new SVGPanel();
        panel->box.size = box.size;
        panel->setBackground(APP->window->loadSvg(asset::plugin(pluginInstance, "res/Noise.svg")));
        addChild(panel);
    }

    addChild(createWidget<ScrewSilver>(Vec(15, 0)));
    addChild(createWidget<ScrewSilver>(Vec(box.size.x-30, 0)));
    addChild(createWidget<ScrewSilver>(Vec(15, 365)));
    addChild(createWidget<ScrewSilver>(Vec(box.size.x-30, 365)));

    addParam(createParam<RoundHugeBlackKnob>(Vec(47, 61), module, Noise::COLOR_PARAM));
    addParam(createParam<RoundHugeBlackKnob>(Vec(47, 143), module, Noise::LPF_PARAM));
    addParam(createParam<RoundHugeBlackKnob>(Vec(47, 228), module, Noise::HPF_PARAM));

    addInput(createInput<PJ301MPort>(Vec(22, 100), module, Noise::COLOR_CV_INPUT));
    addInput(createInput<PJ301MPort>(Vec(22, 190), module, Noise::LPF_CV_INPUT));
    addInput(createInput<PJ301MPort>(Vec(22, 270), module, Noise::HPF_CV_INPUT));
    addParam(createParam<RoundSmallBlackKnob>(Vec(20, 310), module, Noise::VOL_PARAM));

    addOutput(createOutput<PJ301MPort>(Vec(100, 310), module, Noise::NOISE_OUTPUT));
}
Model *modelNoise = createModel<Noise, NoiseWidget>("Noise");
