//
//  Distortion.h
//  Fire
//
//  Created by 羽翼深蓝Wings on 2/11/20.
//

#ifndef Distortion_h
#define Distortion_h

#include "../JuceLibraryCode/JuceHeader.h"
#include <cmath>
#include "DiodeWDF.h"

class Distortion
{
public:
    struct Controls
    {
        // Distortion mode, 0 = bypass, 1 = soft-clip, 2 = hard-clip
        int mode;
        // Drive, [1., ?), the amount of gain prior to the non-linearity
        float drive;
        // Threshold, [0., 1], the amplitude at which to start fold-back distortion
        float thresh;
        // output
        float output;
        // Mix, [0., 1.] ratio between a dry and wet signal
        float mix;
        // Rectification mode 0 = bypass, 1 = half, 2 = full
        int rectification;
    } controls;

    Distortion();
    ~Distortion();
    float distortionProcess(float input);
    float rectificationProcess(float input);

private:
    // Intermediate values
    float input, cleanOut = 0.f;

    // Nonlinearities
    float arctanSoftClipping(float input);
    float expSoftClipping(float input);
    float tanhSoftClipping(float input);
    float cubicSoftClipping(float input);
    float hardClipping(float input);
    float sinFoldback(float input);
    float linFoldback(float input);
    float halfRectification(float input);
    float fullRectification(float input);
    // SmoothedValue<float> driverSmoother;
};

#endif /* Distortion_h */
