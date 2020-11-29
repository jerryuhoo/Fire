/*
  ==============================================================================

   This file is part of the JUCE tutorials.
   Copyright (c) 2017 - ROLI Ltd.

   The code included in this file is provided under the terms of the ISC license
   http://www.isc.org/downloads/software-support-policy/isc-license. Permission
   To use, copy, modify, and/or distribute this software for any purpose with or
   without fee is hereby granted provided that the above copyright notice and
   this permission notice appear in all copies.

   THE SOFTWARE IS PROVIDED "AS IS" WITHOUT ANY WARRANTY, AND ALL WARRANTIES,
   WHETHER EXPRESSED OR IMPLIED, INCLUDING MERCHANTABILITY AND FITNESS FOR
   PURPOSE, ARE DISCLAIMED.

  ==============================================================================
*/

/*******************************************************************************
 The block below describes the properties of this PIP. A PIP is a short snippet
 of code that can be read by the Projucer and used to generate a JUCE project.

 BEGIN_JUCE_PIP_METADATA

 name:             SpectrumAnalyserTutorial
 version:          2.0.0
 vendor:           JUCE
 website:          http://juce.com
 description:      Displays an FFT spectrum analyser.

 dependencies:     juce_audio_basics, juce_audio_devices, juce_audio_formats,
                   juce_audio_processors, juce_audio_utils, juce_core,
                   juce_data_structures, juce_dsp, juce_events, juce_graphics,
                   juce_gui_basics, juce_gui_extra
 exporters:        xcode_mac, vs2019, linux_make

 type:             Component
 mainClass:        AnalyserComponent

 useLocalCopy:     1

 END_JUCE_PIP_METADATA

*******************************************************************************/

#include "PluginProcessor.h"

#pragma once
const int frequenciesForLines[] = { 20, 30, 40, 50, 60, 70, 80, 90, 100, 200, 300, 400, 500, 600, 700, 800, 900, 1000, 2000, 3000, 4000, 5000, 6000, 7000, 8000, 9000, 10000, 20000 };
const int numberOfLines = 28;

//==============================================================================
class AnalyserComponent   : public juce::Component,
                            private juce::Timer
{
public:
    AnalyserComponent(FireAudioProcessor &p)
        : processor(p),
          forwardFFT (fftOrder),
          window (fftSize, juce::dsp::WindowingFunction<float>::blackman)
          
    {
        setOpaque (false);
//        setAudioChannels (2, 0);  // we want a couple of input channels but no outputs
        startTimerHz (30);
//        setSize (700, 500);
        
    }

    ~AnalyserComponent()
    {
//        shutdownAudio();
    }

    //==============================================================================
//    void prepareToPlay (int, double) override {}
//    void releaseResources() override          {}

    void getNextAudioBlock ()
    {
        juce::Array<float> historyL = processor.getHistoryArrayL();
        juce::Array<float> historyR = processor.getHistoryArrayR();
        
        for (auto i = 0; i < historyL.size(); ++i)
            pushNextSampleIntoFifo ((historyL[i] + historyR[i]) / 2.f);
        
    }

    //==============================================================================
    void paint (juce::Graphics& g) override
    {
        //g.fillAll (juce::Colours::red.withAlpha(0.2f));

        g.setOpacity (0.4f);
        g.setColour (juce::Colours::yellow);
        
        // paint lines and numbers
        g.setColour(COLOUR1.withAlpha(0.2f));
        for (int i = 0; i < numberOfLines; ++i)
        {
            const double proportion = frequenciesForLines[i] / (44100 / 2.0);
            int xPos = transformToLog(proportion) * (getWidth());
            g.drawVerticalLine(xPos, getHeight() / 5, getHeight());
            if (frequenciesForLines[i] == 10 || frequenciesForLines[i] == 100 || frequenciesForLines[i] == 20 || frequenciesForLines[i] == 200)
                g.drawFittedText(static_cast<juce::String>(frequenciesForLines[i]), xPos - 5, 0, 60, 30, juce::Justification::left, 2);
            else if ( frequenciesForLines[i] == 1000 || frequenciesForLines[i] == 10000 || frequenciesForLines[i] == 2000 || frequenciesForLines[i] == 20000)
                g.drawFittedText(static_cast<juce::String>(frequenciesForLines[i] / 1000) + "k", xPos - 20, 0, 60, 30, juce::Justification::left, 2);
        }
        
    
        
        g.setColour (COLOUR1);
                
        juce::ColourGradient grad(COLOUR2, 0, 0,
                                  COLOUR6, 0, getLocalBounds().getHeight(), false);
        g.setGradientFill(grad);
        
        getNextAudioBlock();
        
        drawFrame (g);
    }

    void timerCallback() override
    {
        if (nextFFTBlockReady)
        {
            drawNextFrameOfSpectrum();
            nextFFTBlockReady = false;
            repaint();
        }
    }

    void pushNextSampleIntoFifo (float sample) noexcept
    {
        // if the fifo contains enough data, set a flag to say
        // that the next frame should now be rendered..
        if (fifoIndex == fftSize)               // [11]
        {
            if (! nextFFTBlockReady)            // [12]
            {
                juce::zeromem (fftData, sizeof (fftData));
                memcpy (fftData, fifo, sizeof (fifo));
                nextFFTBlockReady = true;
            }

            fifoIndex = 0;
        }

        fifo[fifoIndex++] = sample;             // [12]
    }

    void drawNextFrameOfSpectrum()
    {
        // first apply a windowing function to our data
        window.multiplyWithWindowingTable (fftData, fftSize);       // [1]

        // then render our FFT data..
        forwardFFT.performFrequencyOnlyForwardTransform (fftData);  // [2]

        auto mindB = -100.0f;
        auto maxdB =    0.0f;

        for (int i = 0; i < scopeSize; ++i)                         // [3]
        {
            auto skewedProportionX = 1.0f - std::exp (std::log (1.0f - (float) i / (float) scopeSize) * 0.2f);
            auto fftDataIndex = juce::jlimit (0, fftSize / 2, (int) (skewedProportionX * (float) fftSize * 0.5f));
            auto level = juce::jmap (juce::jlimit (mindB, maxdB, juce::Decibels::gainToDecibels (fftData[fftDataIndex])
                                                               - juce::Decibels::gainToDecibels ((float) fftSize)),
                                     mindB, maxdB, 0.0f, 1.0f);

            scopeData[i] = level;                                   // [4]
        }
    }

    void drawFrame (juce::Graphics& g)
    {
        auto width = getLocalBounds().getWidth();
        auto height = getLocalBounds().getHeight();
        
        juce::Path p;
        p.startNewSubPath(0, height);
        //p.lineTo(0, juce::jmap (scopeData[0], 0.0f, 1.0f, (float) height, 0.0f));
        for (int i = 0; i < scopeSize; i++)
        {
            p.lineTo(transformToLog((float)i / (scopeSize - 1)) * width,
                             juce::jmap (scopeData[i], 0.0f, 1.0f, (float) height, 0.0f));
//            p.lineTo(transformToLog((float)i / scopeSize) * width,
//                             height - height * scopeData[i]);
            
        }
        p.lineTo(width, height);
        p.lineTo(0, height);
        g.fillPath(p);
    }

    enum
    {
        fftOrder  = 11,             // [1]
        fftSize   = 1 << fftOrder,  // [2]
        scopeSize = 512             // [3]
    };

private:
    
    FireAudioProcessor &processor;
    juce::Array<float> historyL;
    juce::Array<float> historyR;
    
    juce::dsp::FFT forwardFFT;                      // [4]
    juce::dsp::WindowingFunction<float> window;     // [5]

    float fifo [fftSize];                           // [6]
    float fftData [2 * fftSize];                    // [7]
    int fifoIndex = 0;                              // [8]
    bool nextFFTBlockReady = false;                 // [9]
    float scopeData [scopeSize] = {0};                    // [10]

    float transformToLog(float between0and1)
    {
        const float minimum = 1.0f;
        const float maximum = 1000.0f;
        return log10(minimum + ((maximum - minimum) * between0and1)) / log10(maximum);
    }
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AnalyserComponent)
};
