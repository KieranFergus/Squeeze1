/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
Squeeze1AudioProcessor::Squeeze1AudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       ),
            envelope(0.0f)
#endif
{
    
}

Squeeze1AudioProcessor::~Squeeze1AudioProcessor()
{
}



juce::AudioProcessorValueTreeState::ParameterLayout Squeeze1AudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout params;
    
    // Threshold Slider
    params.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"THRESHOLD", 1},
                                                                 "Threshold",
                                                         juce::NormalisableRange<float>(-24.0f, 0.0f, 0.1f), 0.0f));
    
    // Ratio Slider
    params.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"RATIO", 1}, "Ratio",
                                                                 juce::NormalisableRange<float>(1.0f, 20.0f, 0.1f, 0.5f), 1.0f));
    
    // Attack Slider
    params.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"ATTACK", 1}, "Attack",
                                                                 juce::NormalisableRange<float>(0.1f, 100.0f, 0.1f, 0.3f), 0.1f));
    
    // Release Slider
    params.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"RELEASE", 1}, "Release",
                                                                 juce::NormalisableRange<float>(10.0f, 1000.0f, 1.0f, 0.3f), 10.0f));
    
    // Gain Slider
    params.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"GAIN", 1}, "Gain",
                                                                 juce::NormalisableRange<float>(0.0f, 24.0f, 0.1f), 0.0f));
    
    return params;
}


//==============================================================================
const juce::String Squeeze1AudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool Squeeze1AudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool Squeeze1AudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool Squeeze1AudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double Squeeze1AudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int Squeeze1AudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int Squeeze1AudioProcessor::getCurrentProgram()
{
    return 0;
}

void Squeeze1AudioProcessor::setCurrentProgram (int index)
{
}

const juce::String Squeeze1AudioProcessor::getProgramName (int index)
{
    return {};
}

void Squeeze1AudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void Squeeze1AudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // Use this method as the place to do any pre-playback
    // initialisation that you need..
    inputBuffer.setSize(1, samplesPerBlock); // Mono buffer for input visualization
    outputBuffer.setSize(1, samplesPerBlock);
}

void Squeeze1AudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool Squeeze1AudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
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

void Squeeze1AudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    auto totalNumInputChannels = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // Clear unused output channels
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i) {
        buffer.clear(i, 0, buffer.getNumSamples());
    }
    
    
    auto gainDb = apvts.getRawParameterValue("GAIN")->load();
    auto linearGain = juce::Decibels::decibelsToGain(gainDb);
    
    auto thresholdDb = apvts.getRawParameterValue("THRESHOLD")->load();
    auto linearThreshold = juce::Decibels::decibelsToGain(thresholdDb);
    
    auto ratio = apvts.getRawParameterValue("RATIO")->load();
    
    auto attackMs = apvts.getRawParameterValue("ATTACK")->load();
    auto attackCoeff = calculateAttackCoefficient(attackMs, getSampleRate());
    
    auto releaseMs = apvts.getRawParameterValue("RELEASE")->load();
    auto releaseCoeff = calculateReleaseCoefficient(releaseMs, getSampleRate());
    
    // Copy input buffer for visualization
    inputBuffer.copyFrom(0, 0, buffer, 0, 0, buffer.getNumSamples());
    

    // Process audio (example gain reduction)
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
    {
        auto* channelData = buffer.getWritePointer(channel);
        
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            auto& sampleValue = channelData[sample];
            
            
            if (std::abs(sampleValue) > linearThreshold){
                            // Calculate the excess above the threshold
                float excess = std::abs(sampleValue) - linearThreshold;

                            // Smooth the gain reduction using the envelope
                float targetEnvelope = excess / ratio; // Amount of reduction based on ratio
                envelope = envelope + attackCoeff * (targetEnvelope - envelope); // Attack smoothing

                            // Apply compression
                float compressedSample = linearThreshold + envelope;
                sampleValue = (sampleValue > 0.0f ? compressedSample : -compressedSample); // Preserve polarity
            }
            else {
                // Release the envelope
                envelope = envelope - releaseCoeff * envelope;
                envelope = juce::jmax(envelope, 0.0f); // Ensure envelope doesn't go negative
            }
                // Apply final gain
                sampleValue *= linearGain;
        }
    }
   

    // Copy output buffer for visualization
    outputBuffer.copyFrom(0, 0, buffer, 0, 0, buffer.getNumSamples());
}

//==============================================================================
bool Squeeze1AudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* Squeeze1AudioProcessor::createEditor()
{
    return new Squeeze1AudioProcessorEditor (*this);
}

//==============================================================================
void Squeeze1AudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void Squeeze1AudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
        if (xmlState && xmlState->hasTagName(apvts.state.getType())) {
            apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
        }
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new Squeeze1AudioProcessor();
}
