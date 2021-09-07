/*
  ==============================================================================

    FFTProcessor.h
    Created: 12 Nov 2018 2:16:02pm
    Author:  lenovo

  ==============================================================================
*/
#include <JuceHeader.h>
#pragma once

class SpectrumProcessor {
public:
	SpectrumProcessor(): forwardFFT(fftOrder), window(fftSize, juce::dsp::WindowingFunction<float>::hamming)
	{
        window.fillWindowingTables(fftSize, juce::dsp::WindowingFunction<float>::blackman);
	}

	enum // moze zamiast enuma to ustawic   a moze enum ok?
	{
		fftOrder = 11,
		fftSize = 1 << fftOrder, // 2^ ??
	};

	float fftData[2 * fftSize] = {0};
	bool nextFFTBlockReady = false;
	float fifo[fftSize];
    juce::dsp::FFT forwardFFT;
    juce::dsp::WindowingFunction<float> window;
	int fifoIndex = 0;

	void pushNextSampleIntoFifo(float sample) noexcept {
        if (fifoIndex == fftSize)
        {
            if (!nextFFTBlockReady)
            {
                juce::zeromem(fftData, sizeof(fftData));
                memmove(fftData, fifo, sizeof(fifo)); // memmove is safer, but memcpy is faster
				nextFFTBlockReady = true;
			}

			fifoIndex = 0;
		}

		fifo[fifoIndex++] = sample;
	}

	void doProcessing(float * tempFFTData)
	{
        window.multiplyWithWindowingTable(tempFFTData, fftSize);
        forwardFFT.performFrequencyOnlyForwardTransform(tempFFTData);
	}

private:
};
