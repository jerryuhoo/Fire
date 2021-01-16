//==============================================================================
#ifndef __DIODE_H_4BF269BF__
#define __DIODE_H_4BF269BF__
//==============================================================================
#include "WDF.h"
//==============================================================================
/**
 *  reference from https://forum.juce.com/t/wave-digital-filter-wdf-with-juce/11227
	The main simulation based on the DAFX-11 matlab example.

	The electronic circuit is write in five lines :

		- 3 WDF Elements (Resistor/Capacitor/Voltage)
		- 2 WDF Adaptors (Serie)

	For a simply use, all the classes overrides the WDF 
	base class. Basically, a WDF Element is a leaf, a WDF
	Adaptor is a node.

	The WDF class provide two global methods to get the 
	voltage and the current on each point of the circuit.

	The simulation produce same result than LTSPICE, 
	so the WDF class can be use to write simple electronic
	circuits with serie/parallel 3-ports adaptors (T-Shape).

	The limitation of a WDF binary tree :

		- only one nonlinear element by tree
		- nonlinear element at top of the tree (root)
		- simple serie/parallel circuits

	But it's a perfect way to produce cool blackbox with 
	correct electronic parameters. A WDF binary tree use
	the Kirchhoff's circuit laws (KCL).

	For more informations about this example :
		
		DAFX: Digital Audio Effects, Second Edition
		ISBN : 978-0-470-66599-2
		Chapter : 12.3.4 / Virtual analog effects
			    : Diode circuit model using wave digital filters
*/
//==============================================================================
/**
    diodeClipper
    ------------
    
    This is a blackbox of the GZ34 Vaccuum-Diode clipper. The simulation use
    is own internal WDF tree. For a better integration in other circuit, this
    code need to be rewrite as a class that inherit of WDF base class, allowing
    to connect a WDF Diode Element (this subcircuit) in other circuit.
    
    In this case, the some precomputed LUT (LookUp Table) can be used in place.
    This method is a simple theorical example without optimization, but you can
    easily replace the Array<double> in input by a AudioSampleBuffer and test
    this code with a real audio sample.
    
    The Fs parameter is the sampling rate of the datas inside the input buffer.
    
	Return a buffer with the output voltage of the circuit with the same size 
	of the input buffer.

*/
//==============================================================================
static inline float diodeClipper(juce::Array<float> &input, float Fs,
								 float Vdiode, VoltageSource &Vin, Serie &root, Resistor &R1)
{
	/*
    // 1 ohm is the Ri voltage source resistor
    float Ri = 1; 

    // Internal circuit parameters 
    VoltageSource Vin (0.0, Ri); // initialize voltage at 0V
    Resistor R1 (80.0); 
    Capacitor C1 (3.5e-5, Fs); // Capacitor & Inductor need sampling rate (Fs) in constructor
    
    // create WDF circuit
	Serie RC (&R1, &C1);
    Serie root (&Vin, &RC); 
	*/

	// Resistor R1(80.0);

	// accurate simulation of GZ34 valve diode.
	float Is = 125.56f; // reverse saturation current
	float Vt = 0.036f;  // thermal voltage

	// initial value for the voltage over the diode (n-1 memory)
	// float Vdiode = 0.0f;

	// for simulation
	float b, r, Rdiode;
	juce::Array<float> output;

	// the simulation loop
	int n = 0;
	int max = input.size();

	for (; n < max; ++n)
	{
		Vin.Vs = input[n] * 13;			 // read the input signal for the voltage source
		b = root.reflected();			 // get the waves up to the root
										 // ** VALVE RESISTOR **
		Rdiode = Is * exp(-Vt * Vdiode); // the nonlinear resistance of the diode
		r = (Rdiode - root.R)			 // update scattering coefficient (KCL)
			/ (Rdiode + root.R);
		root.incident(r * b);	  // evaluate the wave leaving the diode (root element)
								  // ** UPDATE **
		Vdiode = root.voltage();  // update the diode voltage for next time sample
		output.add(R1.voltage()); // the output is the voltage over the resistor R1
	}
	input = output;
	return Vdiode;
}

#endif // __DIODE_H_4BF269BF__
//==============================================================================
