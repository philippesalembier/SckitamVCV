## Sckitam plugin for VCV Rack

This page provides some information about the following plugins:

* **[2DRotation](#2DRotation)**: Utility, 2D Rotation of 2 input signals
* **[2DAffine](#2DAffine)**: Utility, 2D Affine Transform of 2 intput signals
* **[MarkovSeq](#MarkovSeq)**: Sequencer & Switch, 8 steps sequencers based on Markov chain
* **[PolygonalVCO](#PolygonalVCO)**: VCO based on the paper: C. Hohnerlein, M. Rest, and J. O. Smith III, “Continuousorder polygonal waveform synthesis,” in Proceedings of theInternational Computer Music Conference, Utrecht, Netherlands, 2016.


![](doc/SckitamVCV.png)

![](doc/SckitamVCV_dark.png)

The context menu allows one to choose between light or dark panel. 

## 2DRotation <a id="2DRotation"> </a>
![](doc/2DRotation.png)

The two input signals, X and Y, are considered as the horizontal and vertical displacements on a 2D plane. They form a curve. The plugin mix them following a rotation rule. The two output signals are defined by:

Out1 = cos(\theta) X - sin(\theta) Y

Out2 = sin(\theta) X + cos(\theta) Y

The **Angle** value in the range +/-pi is defined by the knob on the left. The angle can be CV modulated. The range of CV from +/-5V corresponds to +/-pi. 

The **Velocity** of the rotation can be specified by the knob on the right. The range of rotation values has two modes. In the low (L) velocity mode, the velocity can reach +/-90 rotations per second. In the high (H) velocity mode, the rotation is specified by a 1V/Oct rule. If the knob is at 0, the velocity corresponds to 261.63 rotations per second (C4). Each increment of +/-1 will mutiply/divide the number of rotations per second by a factor of 2. The velocity parameter can also be CV modulated.  

The two sliders below \Delta X (\Delta Y) respectively define a horizontal (vertical) translation before and after the rotation. This allows one to precisely position the curve in the 2D space if necessary. 

X & Y input and output are polyphonic. The actual number of channels is defined by the X input port.

## 2DAffine <a id="2DAffine"> </a>
![](doc/2DAffine.png)

Same as the [2DRotation](#2DRotation) without the velocity parameter but with two additional shearing parameters, Sx and Sy. The shearing is applied before the rotation:

Out1 = cos(\theta) (X + Sx Y) - sin(\theta) (Sy X + Y)

Out2 = sin(\theta) (X + Sx Y) + cos(\theta) (Sy X + Y)

The shearing creates horizontal and vertical deformations of the curve in the 2D plane. Note that the shearing can modify very significantly the signal dinamic. As a result, a (parabolic) saturator has been introduced at the output.  

X & Y input and output are polyphonic. The actual number of channels is defined by the X input port.

## MarkovSeq <a id="MarkovSeq"> </a>
![](doc/MarkovSeq.png)

**8 steps sequencer or 8 to 1 switch**: This plugin defines 8 states and each incoming clock generates a **new state** which is defined by probabilities associated to the **current state**. This is essentially a first order 
[Markov Chain](https://en.wikipedia.org/wiki/Markov_chain) and allows one to define a graph of events with associated transition probabilities. The transition probabilites from each current state, S0,...,S7, are defined by the central part of the plugin. 

![](doc/MarkovSeq_State_Transition.png)

Each slider specifies the relative probability to go from the current state to one of the next state. Note that the probabilities are relative in the sense that they are normalized by the sum of the 8 values associated to the possible transitions. As a result, if all sliders have the same value, all possible next states are equiprobable (independently of the actual value the sliders may have). As a special case, if all slider values are equal to 0, the context menu allows one to choose either to remain in the current state or to assume that transitions to any states are equiprobable. 

As the number of probabilities to specify is 64, their definition can be cumbersome. Presets can be used. However beside presets, "-" and "+" buttons have been introduced to support the probability definition task. Above each set of sliders, 4 buttons are provided. The example below shows the case of state S2:  
![](doc/MarkovSeq_Reset_probability.png)

The two buttons on the left of S2 modifies the probabilities of all states (Si) that may come to state 2. The "-" button sets these probabilities to 0. This means that S2 will not be reached any more. The "+" button sets the probabilities to 0.5. 

The two buttons on the right of S2 modifies the probabilities of S2 to go to all possible states (Si). The "-" button sets these probabilities to 0. The "+" button sets these probabilities to 0.5. 

The probabilities initialization is done such that the sequencer moves as a regular sequencer in the forward direction: 

![](doc/MarkovSeq_Initialization.png)

The states are visualized on the right side of the plugin: 

![](doc/MarkovSeq_States.png)

The current state is shown in red and the next state (the one that will be triggered by the next clock gate) in blue. The next state can also be forced either manually with the associated button or by a gate signal. 

The left part of the plugin defines the values that are used to generate the output value for each current state. 

![](doc/MarkovSeq_input.png)

All input ports and the output port are polyphonic (the actual number of channels is defined by the input port of state 0). For each current state, the output is the sum of the incoming signal at the corresponding input port and the value of the knob (the transitions between values defined by the knobs can be smoothed by a Slew knob).
If the input is polyphonic, the value of the knob value is added to all channels. The plugin can therefore be used as a sequencer, an 8 to 1 switch or a combination of both. The final output can also be scaled by a Gain parameter. 

The current state value (value between 0 and 7) is also available at one output port. Finally, for each transition,
a trigger is sent to the Trig output port of the current state: 
 
![](doc/MarkovSeq_States_output_triggers.png)

## PolygonalVCO <a id="PolygonalVCO"> </a>
![](doc/PolygonalVCO.png)

This is an oscillator based on the sampling of a 2D curve, here a polygon of order N, by a rotating phasor. It generates two output signals X and Y corresponding to the projection of the sampling on the horizontal and vertical axes. 

The VCO was proposed in C. Hohnerlein, M. Rest, and J. O. Smith III, “Continuous order polygonal waveform synthesis,” in Proceedings of theInternational Computer Music Conference, Utrecht, Netherlands, 2016; and further discussed in C. Hohnerlein, M. Rest and J. Parker, "Efficient anti-aliasing of a complex polygonal oscillator", 20th International Conference on Digital Audio Effects (DAFx17), Edinburgh, Scotland, 2017.

The oscillator has essentially two parameters: 

1. **N: Order of the polygon**. Its value ranges from 2.1 up to 20 and can be CV modulated. High values of N create polygons that converge towards a circle and the resulting waveforms are cosine (X) and sine (Y) waves. Lower N values generate signals with richer harmonic content. If one wants to restrict the possible set of harmonic variations, the N parameter can be quantized. The context menu provides several quantization step options: "None", "0.25", "0.33", "0.50" or "1.00"
2. **T: Teeth**: This parameter creates "teeth" shapes on the corners of the polygon. These shapes increase the harmonic content of the signals.    

The oscillator is polyphonic. The actual number of channels is defined by the pitch signal. The FM signal is assumed to have as many channels as the pitch signal. The N and T CV ports *may* also be polyphonic. If they are monophonic, their value is applied on all channels (otherwise individual values are used for each channel).
 
In order to minimize the CPU load, no visual representation of the curve is included in the module itself. However, as shown in the figure above, the use of an additional scope is highly recommended, at least for sound design.

 
