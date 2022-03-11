/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

//==============================================================================
/**
*/
class Test_Overlapping_FFTAudioProcessorEditor  : public juce::AudioProcessorEditor
{
public:
    Test_Overlapping_FFTAudioProcessorEditor (Test_Overlapping_FFTAudioProcessor&);
    ~Test_Overlapping_FFTAudioProcessorEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    Test_Overlapping_FFTAudioProcessor& audioProcessor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Test_Overlapping_FFTAudioProcessorEditor)
};
