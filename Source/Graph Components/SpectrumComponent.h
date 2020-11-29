/*
  ==============================================================================

    SpectrumComponent.h
    Created: 11 Nov 2018 9:40:21am
    Author:  lenovo

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "../LookAndFeel.h"


//==============================================================================
/*
*/
class SpectrumComponent : public juce::Component
{
public:
	SpectrumComponent();
    ~SpectrumComponent();

	void paint(juce::Graphics& g) override;
	void prepareToPaintSpectrum(int numberOfBins, float * spectrumData);
	float transformToLog(float valueToTransform); 
	void resized() override;
	void paintSpectrum(juce::Graphics & g);





private:
	float last_x;
	float last_y;
	int number_of_bins;
	float* spectrum_data;
	static const int frequenciesForLines[];
	static const int numberOfLines;    


    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpectrumComponent)
};
