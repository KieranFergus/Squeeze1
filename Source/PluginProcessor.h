/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>


//==============================================================================
/**
*/
class Squeeze1AudioProcessor  : public juce::AudioProcessor
{
public:
    //==============================================================================
    Squeeze1AudioProcessor();
    ~Squeeze1AudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;
    
    //==============================================================================
    juce::AudioProcessorValueTreeState apvts {*this, nullptr, "Parameters", createParameterLayout()};

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    juce::AudioBuffer<float>& getInputBuffer() { return inputBuffer; }
    juce::AudioBuffer<float>& getOutputBuffer() { return outputBuffer; }
    
    float calculateAttackCoefficient(float attackMs, double sampleRate)
    {
        float attackTimeInSeconds = attackMs / 1000.0f;
        return 1.0f - std::exp(-1.0f / (attackTimeInSeconds * sampleRate));
    }
    
    float calculateReleaseCoefficient(float releaseMs, double sampleRate)
    {
        float releaseTimeInSeconds = releaseMs / 1000.0f;
        return 1.0f - std::exp(-1.0f / (releaseTimeInSeconds * sampleRate));
    }


    
    
private:
    juce::AudioBuffer<float> inputBuffer;
    juce::AudioBuffer<float> outputBuffer;
    
    float envelope;
    
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Squeeze1AudioProcessor)
};
