//==============================================================================
#ifndef __WDF_H_974CDD6D__
#define __WDF_H_974CDD6D__
//==============================================================================
#include <JuceHeader.h>
//==============================================================================
// reference from https://forum.juce.com/t/wave-digital-filter-wdf-with-juce/11227
class WDF
{
public:
    virtual ~WDF() {}

    WDF(float Rp) : R(Rp), G(1 / Rp), a(0), b(0) {}
    //----------------------------------------------------------------------
    virtual juce::String getLabel() const = 0;
    //----------------------------------------------------------------------
    virtual inline float reflected() = 0;
    virtual inline void incident(float value) = 0;
    //----------------------------------------------------------------------
    float voltage() // v
    {
        return (a + b) / 2.f;
    }
    //----------------------------------------------------------------------
    float current() // i
    {
        return (a - b) / 2.f * R;
    }
    //----------------------------------------------------------------------
    float R; // the WDF port resistance
    float G; // the inverse port resistance
    float a; // incident wave (incoming wave)
    float b; // reflected wave (outgoing wave)
    //----------------------------------------------------------------------
};
//==============================================================================
class Adaptor : public WDF
{
public:
    WDF *left;  // WDF element connected at the left port
    WDF *right; // WDF element connected at the right port
    //----------------------------------------------------------------------
    //    Adaptor (WDF *l, WDF *r, float R)
    //        : left (l), right (r), WDF (R)
    Adaptor(float R, WDF *l, WDF *r)
        : WDF(R), left(l), right(r)
    {
    }
    //----------------------------------------------------------------------
    virtual juce::String getLabel() const = 0;
    //----------------------------------------------------------------------
    virtual inline float reflected() = 0;
    //----------------------------------------------------------------------
    virtual inline void incident(float wave)
    {
        // set the waves to the children according to the scattering rules
        left->incident(left->b - (left->R / R) * (wave + left->b + right->b));
        right->incident(right->b - (right->R / R) * (wave + left->b + right->b));
        a = wave;
    }
    //----------------------------------------------------------------------
};
//==============================================================================
class Serie : public Adaptor
{
public:
    //    Serie (WDF *l, WDF *r)
    //        : Adaptor (l, r, (l->R + r->R))
    Serie(WDF *l, WDF *r)
        : Adaptor((l->R + r->R), l, r)
    {
    }
    //----------------------------------------------------------------------
    virtual juce::String getLabel() const { return "Serie"; }
    //----------------------------------------------------------------------
    virtual inline float reflected()
    {
        b = -(left->reflected() + right->reflected());
        return b;
    }
    //----------------------------------------------------------------------
};
//==============================================================================
class Parallel : public Adaptor
{
public:
    //    Parallel (WDF *l, WDF *r)
    //        : Adaptor (l, r, (l->R * r->R / (l->R + r->R)))
    Parallel(WDF *l, WDF *r)
        : Adaptor((l->R * r->R / (l->R + r->R)), l, r)
    {
    }
    //----------------------------------------------------------------------
    virtual juce::String getLabel() const { return "Parallel"; }
    //----------------------------------------------------------------------
    virtual inline float reflected()
    {
        b = (left->G / G) * left->reflected() + (right->G / G) * right->reflected();
        return b;
    }
    //----------------------------------------------------------------------
};
//==============================================================================
class Resistor : public WDF
{
public:
    Resistor(float R)
        : WDF(R)
    {
    }
    //----------------------------------------------------------------------
    virtual juce::String getLabel() const { return "R"; }
    //----------------------------------------------------------------------
    virtual inline float reflected()
    {
        b = 0;
        return b;
    }
    //----------------------------------------------------------------------
    virtual inline void incident(float value)
    {
        a = value;
    }
    //----------------------------------------------------------------------
};
//==============================================================================
class Capacitor : public WDF
{
public:
    Capacitor(float C, float T)
        : WDF(T / (2.0f * C)),
          state(0)
    {
    }
    //----------------------------------------------------------------------
    virtual juce::String getLabel() const { return "C"; }
    //----------------------------------------------------------------------
    virtual inline float reflected()
    {
        b = state;
        return b;
    }
    //----------------------------------------------------------------------
    virtual inline void incident(float value)
    {
        a = value;
        state = value;
    }
    //----------------------------------------------------------------------
    float state;
    //----------------------------------------------------------------------
};
//==============================================================================
class Inductor : public WDF
{
public:
    Inductor(float L, float T)
        : WDF(2.0f * L / T),
          state(0)
    {
    }
    //----------------------------------------------------------------------
    virtual juce::String getLabel() const { return "L"; }
    //----------------------------------------------------------------------
    virtual inline float reflected()
    {
        b = -state;
        return b;
    }
    //----------------------------------------------------------------------
    virtual inline void incident(float value)
    {
        a = value;
        state = value;
    }
    //----------------------------------------------------------------------
    float state;
    //----------------------------------------------------------------------
};
//==============================================================================
class ShortCircuit : public WDF
{
public:
    ShortCircuit(float R)
        : WDF(R)
    {
    }
    //----------------------------------------------------------------------
    virtual juce::String getLabel() const { return "Short Circuit"; }
    //----------------------------------------------------------------------
    virtual inline float reflected()
    {
        b = -a;
        return b;
    }
    //----------------------------------------------------------------------
    virtual inline void incident(float value)
    {
        a = value;
    }
    //----------------------------------------------------------------------
};
//==============================================================================
class OpenCircuit : public WDF
{
public:
    OpenCircuit(float R)
        : WDF(R)
    {
    }
    //----------------------------------------------------------------------
    virtual juce::String getLabel() const { return "Open Circuit"; }
    //----------------------------------------------------------------------
    virtual inline float reflected()
    {
        b = a;
        return b;
    }
    //----------------------------------------------------------------------
    virtual inline void incident(float value)
    {
        a = value;
    }
    //----------------------------------------------------------------------
};
//==============================================================================
class VoltageSource : public WDF
{
public:
    //    VoltageSource (float V, float R)
    //        : Vs (V),
    //          WDF (R)
    VoltageSource(float R, float V)
        : WDF(R), Vs(V)
    {
    }
    //----------------------------------------------------------------------
    virtual juce::String getLabel() const { return "Vs"; }
    //----------------------------------------------------------------------
    virtual inline float reflected()
    {
        b = -a + 2.0f * Vs;
        return b;
    }
    //----------------------------------------------------------------------
    virtual inline void incident(float value)
    {
        a = value;
    }
    //----------------------------------------------------------------------
    float Vs;
    //----------------------------------------------------------------------
};
//==============================================================================
class CurrentSource : public WDF
{
public:
    //    CurrentSource (float I, float R)
    //        : Is (I),
    //          WDF (R)
    CurrentSource(float R, float I)
        : WDF(R), Is(I)
    {
    }
    //----------------------------------------------------------------------
    virtual juce::String getLabel() const { return "Is"; }
    //----------------------------------------------------------------------
    virtual inline float reflected()
    {
        b = a + 2.0f * R * Is;
        return b;
    }
    //----------------------------------------------------------------------
    virtual inline void incident(float value)
    {
        a = value;
    }
    //----------------------------------------------------------------------
    float Is;
    //----------------------------------------------------------------------
};
//==============================================================================
#endif // __WDF_H_974CDD6D__
//==============================================================================
