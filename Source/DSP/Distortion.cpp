//
//  Distortion.cpp
//  Fire
//
//  Created by 羽翼深蓝Wings on 2/11/20.
//

#include "Distortion.h"

Distortion::Distortion()
{
    controls.thresh = 1.f;
    controls.mode = 0;
    controls.drive = 1.f;
    controls.color = 0;
    //controls.output = 1.f;
    controls.rectification = 0.f;
    controls.protection = false;
    controls.bias = 0;
}

Distortion::~Distortion() {}

float Distortion::distortionProcess(float input)
{
    // sausage max is 4.3 * input
    //    if (controls.mode == 6) {
    //        controls.drive = 1 + (controls.drive - 1)* 3.3f / 31.f;
    //    }

    input += controls.bias;

    input = input * controls.drive;

    switch (controls.mode)
    {
        // controls.mode + 1 = the ith mode in menu
    case 0:
        break;
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
        input = sausageFattener(input);
        break;
    case 7:
        input = sinFoldback(input);
        break;
    case 8:
        input = linFoldback(input);
        break;
    }
    //input = input * controls.output;

    return input;
}

float Distortion::rectificationProcess(float input)
{
    //    switch (controls.rectification)
    //    {
    //    case 0:
    //        break;
    //    case 1:
    //        input = halfRectification(input);
    //        break;
    //    case 2:
    //        input = fullRectification(input);
    //        break;
    //    }
    if (input < 0)
    {
        input *= (0.5 - controls.rectification) * 2;
    }
    return input;
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
    return input;
}

float Distortion::sausageFattener(float input)
{
    // 0.8 - 1.2 smooth
    //    if (input >= 1.2f)
    //    {
    //        input = controls.thresh;
    //    }
    //    else if (input <= -1.2f)
    //    {
    //        input = -controls.thresh;
    //    }
    //    else if (input > 0.8f && input < 1.2f)
    //    {
    //        input = -1.25 * input * input + 3 * input - 0.8;
    //    }
    //    else if (input < -0.8f && input > -1.2f)
    //    {
    //        input = 1.25 * input * input + 3 * input + 0.8;
    //    }

    // 0.9 -1.1 smooth

    input = input * 1.1;

    //    if (controls.protection == true)
    //    {
    //        if (input > 1)
    //        {
    //            input = 1 + input * 0.05;
    //        }
    //    }

    float cleanInput = input;
    if (input >= 1.1f)
    {
        input = controls.thresh;
    }
    else if (input <= -1.1f)
    {
        input = -controls.thresh;
    }
    else if (input > 0.9f && input < 1.1f)
    {
        input = -2.5 * input * input + 5.5 * input - 2.025;
    }
    else if (input < -0.9f && input > -1.1f)
    {
        input = 2.5 * input * input + 5.5 * input + 2.025;
    }
    input = (1.f - controls.color) * input + controls.color * hardClipping(cleanInput);
    return input;
}

float Distortion::sinFoldback(float input)
{
    input = std::sin(input);

    return input;
}

float Distortion::linFoldback(float input)
{
    if (input > controls.thresh || input < -controls.thresh)
    {
        input = fabs(fabs(fmod(input - controls.thresh, controls.thresh * 4)) - controls.thresh * 2) - controls.thresh;
    }
    return input;
}

float Distortion::halfRectification(float input)
{

    if (input < 0)
    {
        input = 0;
    }
    return input;

    /*
    // x - input
    // Q - work point.Controls the linearity of the transfer 
    //      function for low input levels, more negative = more linear 
    // dist - controls the distortion’s character, a higher number gives 
    //      a harder distortion, > 0
    float x = input;
    float Q = 0.2f;
    float dist = 8.0f;
    if (x != 0 || x != Q)
    {
        input = (x - Q) / (1 - expf(-dist * (x - Q))) + Q / (1 - expf(dist * Q));
    }
    else
    {
        input = 1 / dist + Q / Q / (1 - expf(dist * Q));
    }
    return input;*/
}

float Distortion::fullRectification(float input)
{
    if (input < 0)
    {
        input = -input;
    }
    return input;
}
