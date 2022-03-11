/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
Test_Overlapping_FFTAudioProcessor::Test_Overlapping_FFTAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       )
#endif
{
}

Test_Overlapping_FFTAudioProcessor::~Test_Overlapping_FFTAudioProcessor()
{
}

//==============================================================================
const juce::String Test_Overlapping_FFTAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool Test_Overlapping_FFTAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool Test_Overlapping_FFTAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool Test_Overlapping_FFTAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double Test_Overlapping_FFTAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int Test_Overlapping_FFTAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int Test_Overlapping_FFTAudioProcessor::getCurrentProgram()
{
    return 0;
}

void Test_Overlapping_FFTAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String Test_Overlapping_FFTAudioProcessor::getProgramName (int index)
{
    return {};
}

void Test_Overlapping_FFTAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void Test_Overlapping_FFTAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    spectralDynamicProcessor.prepare(sampleRate, samplesPerBlock, 2, 2);
}

void Test_Overlapping_FFTAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool Test_Overlapping_FFTAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    // Some plugin hosts, such as certain GarageBand versions, will only
    // load plugins that support stereo bus layouts.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
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

void Test_Overlapping_FFTAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

	dsp::AudioBlock<float> audioBlock (buffer);
	dsp::ProcessContextReplacing<float> context (audioBlock);
	spectralDynamicProcessor.process (context);
	// buffer.applyGain(1.0f / (1024 / 128 / 2));
}

//==============================================================================
bool Test_Overlapping_FFTAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* Test_Overlapping_FFTAudioProcessor::createEditor()
{
    return new Test_Overlapping_FFTAudioProcessorEditor (*this);
}

//==============================================================================
void Test_Overlapping_FFTAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
}

void Test_Overlapping_FFTAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new Test_Overlapping_FFTAudioProcessor();
}
