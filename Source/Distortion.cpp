//
//  Distortion.cpp
//  Blood
//
//  Created by 羽翼深蓝Wings on 2/11/20.
//

#include "Distortion.h"

Distortion::Distortion()
{
    controls.thresh = 1.f;
    controls.mode = 0;
    controls.drive = 1.f;
    controls.output = 1.f;
    controls.mix = 0.f;
}

Distortion::~Distortion() {}

float Distortion::distortionProcess(float sample)
{
    cleanOut = sample;
    
    input = cleanOut * controls.drive;

    switch (controls.mode)
    {
    case 1:
        input = arctanSoftClipping(input);
        break;
    case 2:
        input = expSoftClipping(input);
        break;
    case 3:
        input = tanhSoftClipping(input);
        break;
    case 4:
        input = cubicSoftClipping(input);
        break;
    case 5:
        input = hardClipping(input);
        break;
    case 6:
        input = sinFoldback(input);
        break;
    case 7:
        input = linFoldback(input);
        break;
    case 8:
        input = bitCrusher(input);
        break;
    
    }
    input = input * controls.output;
    return (1.f - controls.mix) * cleanOut + controls.mix * input;
}

float Distortion::arctanSoftClipping(float input)
{
    input = atan(input) / 2;
    return input;
}

float Distortion::expSoftClipping(float input)
{
    if (input > 0)
    {
        input = 1.0f - expf(-input);
    }
    else
    {
        input = -1.0f + expf(input);
    }
    return input;
}

float Distortion::tanhSoftClipping(float input)
{
    input = tanh(input);
    return input;
}

float Distortion::cubicSoftClipping(float input)
{
    if (input > controls.thresh)
    {
        input = controls.thresh * 2.0f / 3.0f;
    }
    else if (input < -controls.thresh)
    {
        input = -controls.thresh * 2.0f / 3.0f;
    }
    else
    {
        input = input - (pow(input, 3) / 3);
    }
    return input * 3.0f / 2.0f;
}

float Distortion::hardClipping(float input)
{
    if (input > controls.thresh)
    {
        input = controls.thresh;
    }
    else if (input < -controls.thresh)
    {
        input = -controls.thresh;
    }
    else
    {
        input = input;
    }
    return input;
}

float Distortion::sinFoldback(float input)
{
    input = std::sin(input);
    
    return input;
}

float Distortion::linFoldback(float input)
{
    if (input > controls.thresh || input < -controls.thresh) {
        input = fabs(fabs(fmod(input - controls.thresh, controls.thresh * 4))
                     - controls.thresh * 2) - controls.thresh;
    }
    return input;
}

float Distortion::bitCrusher(float input)
{
    input /= controls.drive;
    return input;
}


