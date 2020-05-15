/*
 *  Resonant Amp tube amplifier simulation
 *  Copyright (C) 2020  Garrin McGoldrick
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <algorithm>

#include "PluginProcessor.h"
#include "PluginEditor.h"

// add parameter to the VTS with default range -1 to +1
#define MAKE_PARAMETER_UNIT(n) std::make_unique<AudioParameterFloat>("id"#n, #n, -1.0f, 1.0f, 0.0f)
// add parameter to the VTS with custom range
#define MAKE_PARAMETER(n, l, h, d) std::make_unique<AudioParameterFloat>("id"#n, #n, l, h, d)
// assign a VTS parameter to an object member of the same name
#define ASSIGN_PARAMETER(n) par##n = parameters.getRawParameterValue("id"#n);

//==============================================================================
ResonantAmpAudioProcessor::ResonantAmpAudioProcessor() :
#ifndef JucePlugin_PreferredChannelConfigurations
	 AudioProcessor(BusesProperties()
#if ! JucePlugin_IsMidiEffect
#if ! JucePlugin_IsSynth
		.withInput("Input", AudioChannelSet::stereo(), true)
#endif
		.withOutput("Output", AudioChannelSet::stereo(), true)
#endif
	),
#endif
	parameters(
		*this,
		nullptr,
		Identifier("APVTSResonantAmp"), {
			MAKE_PARAMETER_UNIT(InputLevel),
			MAKE_PARAMETER_UNIT(OutputLevel),
			MAKE_PARAMETER_UNIT(PreDrive),
			MAKE_PARAMETER_UNIT(PowerDrive),

			MAKE_PARAMETER_UNIT(TsLow),
			MAKE_PARAMETER_UNIT(TsMid),
			MAKE_PARAMETER_UNIT(TsHigh),

			MAKE_PARAMETER(GainStages, 1.0f, 4.0f, 2.0f),
			MAKE_PARAMETER_UNIT(GainSlope),

			MAKE_PARAMETER_UNIT(LowCut),
			MAKE_PARAMETER(CabMix, 0.0f, 1.0f, 1.0f),

			MAKE_PARAMETER_UNIT(TriodeDynamic),
			MAKE_PARAMETER_UNIT(TriodeDistort),

			MAKE_PARAMETER_UNIT(TetrodeDynamic),
			MAKE_PARAMETER_UNIT(TetrodeDistort),
		}
	)
{
	ASSIGN_PARAMETER(InputLevel)
	ASSIGN_PARAMETER(OutputLevel)
	ASSIGN_PARAMETER(PreDrive)
	ASSIGN_PARAMETER(PowerDrive)

	ASSIGN_PARAMETER(TsLow)
	ASSIGN_PARAMETER(TsMid)
	ASSIGN_PARAMETER(TsHigh)

	ASSIGN_PARAMETER(GainStages)
	ASSIGN_PARAMETER(GainSlope)

	ASSIGN_PARAMETER(LowCut)
	ASSIGN_PARAMETER(CabMix)

	ASSIGN_PARAMETER(TriodeDynamic)
	ASSIGN_PARAMETER(TriodeDistort)

	ASSIGN_PARAMETER(TetrodeDynamic)
	ASSIGN_PARAMETER(TetrodeDistort)
}

#undef MAKE_PARAMETER_UNIT
#undef MAKE_PARAMETER
#undef ASSIGN_PARAMETER

ResonantAmpAudioProcessor::~ResonantAmpAudioProcessor()
{
}

void ResonantAmpAudioProcessor::addMeterListener(LevelMeter::Listener& listener)
{
	meterListeners.add(&listener);
}

void ResonantAmpAudioProcessor::removeMeterListener(LevelMeter::Listener& listener)
{
	meterListeners.remove(&listener);
}

float remap_unit(float unit, float lower, float upper) {
	return (unit + 1.0f) / 2.0f * (upper - lower) + lower;
}

// set the amp object user parameters from the VTS values
void ResonantAmpAudioProcessor::setAmpParameters() {
	for (int i = 0; i < 2; i++) {
		amp_channel[i].set_input_level(*parInputLevel);
		amp_channel[i].set_output_level(*parOutputLevel);
		amp_channel[i].set_pre_drive(*parPreDrive);
		amp_channel[i].set_power_drive(*parPowerDrive);

		amp_channel[i].set_ts_low(*parTsLow);
		amp_channel[i].set_ts_mid(*parTsMid);
		amp_channel[i].set_ts_high(*parTsHigh);

		amp_channel[i].set_gain_stages(*parGainStages);
		amp_channel[i].set_gain_slope(*parGainSlope);
		amp_channel[i].set_cab_mix(*parCabMix);

		amp_channel[i].set_triode_hp_freq(remap_unit(*parLowCut, -1, 0.75)); 
		amp_channel[i].set_tetrode_hp_freq(remap_unit(*parLowCut, -1, 0.75)); 

		amp_channel[i].set_triode_grid_tau(remap_unit(*parTriodeDistort * -1, -1.0, 0));
		amp_channel[i].set_triode_grid_ratio(remap_unit(*parTriodeDynamic, -2, 0));

		amp_channel[i].set_triode_plate_power(remap_unit(*parTriodeDistort * -1, 0, 0.5));
		amp_channel[i].set_triode_plate_corner(remap_unit(*parTriodeDynamic, 0, 0.1));

		amp_channel[i].set_tetrode_grid_tau(remap_unit(*parTetrodeDistort, -2, 0));
		amp_channel[i].set_tetrode_grid_ratio(remap_unit(*parTetrodeDistort, -2, 0));

		amp_channel[i].set_tetrode_plate_ratio(remap_unit(*parTetrodeDynamic, 0, 0.5));
		amp_channel[i].set_tetrode_plate_smooth(remap_unit(*parTetrodeDistort * -1, 0, 2));
	}
}

//==============================================================================
const String ResonantAmpAudioProcessor::getName() const
{
	return JucePlugin_Name;
}

bool ResonantAmpAudioProcessor::acceptsMidi() const
{
#if JucePlugin_WantsMidiInput
	return true;
#else
	return false;
#endif
}

bool ResonantAmpAudioProcessor::producesMidi() const
{
#if JucePlugin_ProducesMidiOutput
	return true;
#else
	return false;
#endif
}

bool ResonantAmpAudioProcessor::isMidiEffect() const
{
#if JucePlugin_IsMidiEffect
	return true;
#else
	return false;
#endif
}

double ResonantAmpAudioProcessor::getTailLengthSeconds() const
{
	return 0.0;
}

int ResonantAmpAudioProcessor::getNumPrograms()
{
	return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
				// so this should be at least 1, even if you're not really implementing programs.
}

int ResonantAmpAudioProcessor::getCurrentProgram()
{
	return 0;
}

void ResonantAmpAudioProcessor::setCurrentProgram(int index)
{
}

const String ResonantAmpAudioProcessor::getProgramName(int index)
{
	return {};
}

void ResonantAmpAudioProcessor::changeProgramName(int index, const String& newName)
{
}

//==============================================================================
void ResonantAmpAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
	// Use this method as the place to do any pre-playback
	// initialisation that you need..
	for (int i = 0; i < 2; i++) amp_channel[i].init(sampleRate);
	setAmpParameters();
}

void ResonantAmpAudioProcessor::releaseResources()
{
	// When playback stops, you can use this as an opportunity to free up any
	// spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool ResonantAmpAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
#if JucePlugin_IsMidiEffect
	ignoreUnused(layouts);
	return true;

 #else
	// outputs must be mono or stereo
	if (layouts.getMainOutputChannelSet() != AudioChannelSet::mono()
		&& layouts.getMainOutputChannelSet() != AudioChannelSet::stereo())
		return false;
	// inputs must be mono or stereo
	if (layouts.getMainInputChannelSet() != AudioChannelSet::mono()
		&& layouts.getMainInputChannelSet() != AudioChannelSet::stereo())
		return false;
	// if input is stereo, output must be stereo
	if (layouts.getMainInputChannelSet() == AudioChannelSet::stereo()
		&& layouts.getMainOutputChannelSet() != AudioChannelSet::stereo())
		return false;

	// This checks if the input layout matches the output layout
 #if ! JucePlugin_IsSynth
	if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
		return false;
 #endif

	return true;
 #endif
}
#endif

void ResonantAmpAudioProcessor::processBlock(AudioBuffer<float>& buffer, MidiBuffer& midiMessages)
{
	ScopedNoDenormals noDenormals;
	auto totalNumInputChannels = getTotalNumInputChannels();
	auto totalNumOutputChannels = getTotalNumOutputChannels();

	// copy plugin parameter values into the amp object
	setAmpParameters();

	// In case we have more outputs than inputs, this code clears any output
	// channels that didn't contain input data, (because these aren't
	// guaranteed to be empty - they may contain garbage).
	// This is here to avoid people getting screaming feedback
	// when they first compile a plugin, but obviously you don't need to keep
	// this code if your algorithm always overwrites all the output channels.
	for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
		buffer.clear (i, 0, buffer.getNumSamples());

	const auto numSamples = buffer.getNumSamples();

	// TODO: stereo meter if input is stereo
	auto newLevel = buffer.getMagnitude(0, 0, numSamples);
	// convert to decibels and add the input level which ranges from -35 to +35
	newLevel = 20 * log10f(newLevel) + (*parInputLevel * 35);
	meterListeners.call ([=] (LevelMeter::Listener& l) { l.handleNewValue(0, newLevel); });

	// mono to mono: run the amp once
	if (totalNumInputChannels == 1 && totalNumOutputChannels == 1) {
		float* amp_buffer = buffer.getWritePointer(0);
		amp_channel[0].compute(buffer.getNumSamples(), &amp_buffer, &amp_buffer);
	}
	// mono to stereo: run the amp once, copy the result
	else if (totalNumInputChannels == 1 && totalNumOutputChannels == 2) {
		float* amp_buffer = buffer.getWritePointer(0);
		amp_channel[0].compute(buffer.getNumSamples(), &amp_buffer, &amp_buffer);
		float* amp_buffer_other = buffer.getWritePointer(1);
		std::memcpy((void*)amp_buffer_other, (void*)amp_buffer, numSamples * sizeof(float));
	}
	// stereo to stereo: run the amp twice
	else if (totalNumInputChannels == 2 && totalNumOutputChannels == 2) {
		for (int i = 0; i < 2; i++) {
			float* amp_buffer = buffer.getWritePointer(i);
			amp_channel[i].compute(buffer.getNumSamples(), &amp_buffer, &amp_buffer);
		}
	}
}

//==============================================================================
bool ResonantAmpAudioProcessor::hasEditor() const
{
	return true; // (change this to false if you choose to not supply an editor)
}

AudioProcessorEditor* ResonantAmpAudioProcessor::createEditor()
{
	return new ResonantAmpAudioProcessorEditor(*this, parameters);
}

//==============================================================================
void ResonantAmpAudioProcessor::getStateInformation(MemoryBlock& destData)
{
	auto state = parameters.copyState();
	std::unique_ptr<XmlElement> xml (state.createXml());
	copyXmlToBinary(*xml, destData);
}

void ResonantAmpAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
	std::unique_ptr<XmlElement> xmlState(getXmlFromBinary (data, sizeInBytes));
	if (xmlState.get() != nullptr)
		if (xmlState->hasTagName(parameters.state.getType()))
			parameters.replaceState(ValueTree::fromXml(*xmlState));
}

//==============================================================================
// This creates new instances of the plugin..
AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
	return new ResonantAmpAudioProcessor();
}