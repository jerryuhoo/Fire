//
//  Distortion.h
//  Blood
//
//  Created by 俞轶风 on 2/11/20.
//

#ifndef Distortion_h
#define Distortion_h
#include <cmath>

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
        // Mix, [0., 1.] ratio between a dry and wet signal
        float mix;
    } controls;

    Distortion();
    ~Distortion();
    float distortionProcess(float sample);

private:
    // Intermediate values
    float input, cleanOut = 0.f;

    // Nonlinearities
    float arctanSoftClipping(float input);
    float expSoftClipping(float input);
    float tanhSoftClipping(float input);
    float cubicSoftClipping(float input);
    float hardClipping(float input);
    float squareWaveClipping(float input);
};

#endif /* Distortion_h */
