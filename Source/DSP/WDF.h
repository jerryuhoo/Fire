//==============================================================================
#ifndef __WDF_H_974CDD6D__
#define __WDF_H_974CDD6D__
//==============================================================================

#include <juce_core/juce_core.h>
#include <cmath>

//==============================================================================
// reference from https://forum.juce.com/t/wave-digital-filter-wdf-with-juce/11227
class WDF
{
public:
    virtual ~WDF() = default;

    explicit WDF(float portResistance)
        : R(sanitiseResistance(portResistance)), G(1.0f / R), a(0.0f), b(0.0f)
    {
    }

    WDF(const WDF&) = delete;
    WDF& operator=(const WDF&) = delete;
    WDF(WDF&&) = delete;
    WDF& operator=(WDF&&) = delete;
    //----------------------------------------------------------------------
    virtual juce::String getLabel() const = 0;
    //----------------------------------------------------------------------
    virtual float reflected() = 0;
    virtual void incident(float value) = 0;
    //----------------------------------------------------------------------
    float voltage() // v
    {
        return (a + b) / 2.f;
    }
    //----------------------------------------------------------------------
    float current() // i
    {
        return (a - b) / (2.0f * R);
    }
    //----------------------------------------------------------------------
    float R; // the WDF port resistance
    float G; // the inverse port resistance
    float a; // incident wave (incoming wave)
    float b; // reflected wave (outgoing wave)
    //----------------------------------------------------------------------

private:
    static float sanitiseResistance(float resistance) noexcept
    {
        return std::isfinite(resistance) && resistance > 1.0e-12f ? resistance : 1.0e-12f;
    }
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
    Adaptor(float portResistance, WDF *leftElement, WDF *rightElement)
        : WDF(portResistance), left(leftElement), right(rightElement)
    {
    }
    //----------------------------------------------------------------------
    juce::String getLabel() const override = 0;
    //----------------------------------------------------------------------
    float reflected() override = 0;
    //----------------------------------------------------------------------
    void incident(float wave) override
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
    Serie(WDF *leftElement, WDF *rightElement)
        : Adaptor((leftElement->R + rightElement->R), leftElement, rightElement)
    {
    }
    //----------------------------------------------------------------------
    juce::String getLabel() const override { return "Serie"; }
    //----------------------------------------------------------------------
    float reflected() override
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
    Parallel(WDF *leftElement, WDF *rightElement)
        : Adaptor((leftElement->R * rightElement->R / (leftElement->R + rightElement->R)),
                  leftElement, rightElement)
    {
    }
    //----------------------------------------------------------------------
    juce::String getLabel() const override { return "Parallel"; }
    //----------------------------------------------------------------------
    float reflected() override
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
    explicit Resistor(float resistance)
        : WDF(resistance)
    {
    }
    //----------------------------------------------------------------------
    juce::String getLabel() const override { return "R"; }
    //----------------------------------------------------------------------
    float reflected() override
    {
        b = 0;
        return b;
    }
    //----------------------------------------------------------------------
    void incident(float value) override
    {
        a = value;
    }
    //----------------------------------------------------------------------
};
//==============================================================================
class Capacitor : public WDF
{
public:
    Capacitor(float capacitance, float sampleInterval)
        : WDF(sampleInterval / (2.0f * capacitance)),
          state(0.0f)
    {
    }
    //----------------------------------------------------------------------
    juce::String getLabel() const override { return "C"; }
    //----------------------------------------------------------------------
    float reflected() override
    {
        b = state;
        return b;
    }
    //----------------------------------------------------------------------
    void incident(float value) override
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
    Inductor(float inductance, float sampleInterval)
        : WDF(2.0f * inductance / sampleInterval),
          state(0.0f)
    {
    }
    //----------------------------------------------------------------------
    juce::String getLabel() const override { return "L"; }
    //----------------------------------------------------------------------
    float reflected() override
    {
        b = -state;
        return b;
    }
    //----------------------------------------------------------------------
    void incident(float value) override
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
    explicit ShortCircuit(float resistance)
        : WDF(resistance)
    {
    }
    //----------------------------------------------------------------------
    juce::String getLabel() const override { return "Short Circuit"; }
    //----------------------------------------------------------------------
    float reflected() override
    {
        b = -a;
        return b;
    }
    //----------------------------------------------------------------------
    void incident(float value) override
    {
        a = value;
    }
    //----------------------------------------------------------------------
};
//==============================================================================
class OpenCircuit : public WDF
{
public:
    explicit OpenCircuit(float resistance)
        : WDF(resistance)
    {
    }
    //----------------------------------------------------------------------
    juce::String getLabel() const override { return "Open Circuit"; }
    //----------------------------------------------------------------------
    float reflected() override
    {
        b = a;
        return b;
    }
    //----------------------------------------------------------------------
    void incident(float value) override
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
    VoltageSource(float resistance, float sourceVoltage)
        : WDF(resistance), Vs(sourceVoltage)
    {
    }
    //----------------------------------------------------------------------
    juce::String getLabel() const override { return "Vs"; }
    //----------------------------------------------------------------------
    float reflected() override
    {
        b = -a + 2.0f * Vs;
        return b;
    }
    //----------------------------------------------------------------------
    void incident(float value) override
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
    CurrentSource(float resistance, float sourceCurrent)
        : WDF(resistance), Is(sourceCurrent)
    {
    }
    //----------------------------------------------------------------------
    juce::String getLabel() const override { return "Is"; }
    //----------------------------------------------------------------------
    float reflected() override
    {
        b = a + 2.0f * R * Is;
        return b;
    }
    //----------------------------------------------------------------------
    void incident(float value) override
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
