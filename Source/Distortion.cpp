//
//  Distortion.cpp
//  Blood
//
//  Created by 俞轶风 on 2/11/20.
//

#include "Distortion.h"

Distortion::Distortion()
{
    controls.mode = 0;
    controls.drive = 1.f;
    controls.mix = 0.f;
    controls.thresh = 1.f;
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
        input = squareWaveClipping(input);
        break;
    }

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
    //input = 2.0f/3.0f*input;
    return input;
}

float Distortion::tanhSoftClipping(float input)
{
    input = tanh(input);
    //input = 2.0f/3.0f*input;
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
    //input = 2.0f/3.0f*input;
    return input;
}

float Distortion::squareWaveClipping(float input)
{
    input = round(input * 10) / 10;
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

    //input = 2.0f/3.0f*input;
    return input;
}
