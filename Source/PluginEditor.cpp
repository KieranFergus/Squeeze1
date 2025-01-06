/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
Squeeze1AudioProcessorEditor::Squeeze1AudioProcessorEditor (Squeeze1AudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p), customLookAndFeel(thresholdKnob, ratioKnob, attackKnob, releaseKnob, gainKnob)
{
    juce::LookAndFeel::setDefaultLookAndFeel(&globalLookAndFeel);
    
    knobs.push_back(&thresholdKnob);
    knobs.push_back(&ratioKnob);
    knobs.push_back(&attackKnob);
    knobs.push_back(&releaseKnob);
    knobs.push_back(&gainKnob);
    
    for ( auto* knob : knobs ) {
        knob->setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        knob->setTextBoxStyle(juce::Slider::NoTextBox, false, 50, 20);
        knob->setLookAndFeel(&customLookAndFeel);
        addAndMakeVisible(knob);
        knob->addListener(this);
    }
    
    thresholdAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(audioProcessor.apvts, "THRESHOLD", thresholdKnob);
    
    ratioAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(audioProcessor.apvts, "RATIO", ratioKnob);
    
    attackAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(audioProcessor.apvts, "ATTACK", attackKnob);
    
    releaseAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(audioProcessor.apvts, "RELEASE", releaseKnob);
    
    gainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(audioProcessor.apvts, "GAIN", gainKnob);
    
    addAndMakeVisible(inputWaveform);
    addAndMakeVisible(outputWaveform);
    
    setSize (600, 400);
    startTimerHz(30);
    drawInputWaveform = true;
}

Squeeze1AudioProcessorEditor::~Squeeze1AudioProcessorEditor()
{
    for ( auto* knob : knobs ) {
        knob->setLookAndFeel(nullptr);
        knob->removeListener(this);
    }
    
    juce::LookAndFeel::setDefaultLookAndFeel(nullptr);
}

//==============================================================================
void Squeeze1AudioProcessorEditor::paint (juce::Graphics& g)
{
    // (Our component is opaque, so we must completely fill the background with a solid colour)
    g.fillAll (juce::Colours::white);
    //drawRects(g); Draw bounding rectangles
    
    drawStaticWindows(g);
    drawLabels(g);
    
    if ( drawInputWaveform ) {
        drawWaveform(g, audioProcessor.getInputBuffer(), inputWindow, juce::Colours::white);

    }
    else {
        g.setColour(juce::Colours::white);
        g.drawText("SHOW INPUT", inputWindow, juce::Justification::centred);
    }
    
    
    drawWaveform(g, audioProcessor.getOutputBuffer(), outputWindow, juce::Colours::orange);
    
    if ( isEnvelopeVisible ) {
        drawEnvelope(g, outputWindow.toNearestInt());
    }
    
}

void Squeeze1AudioProcessorEditor::resized()
{
   //======================= Rectangles ================================
    auto bounds = getLocalBounds().toFloat();
    
    
    topRect = bounds.removeFromTop(bounds.getHeight() * 0.67);
    
    inputRect = topRect.removeFromLeft(topRect.getWidth() * 0.5);
    outputRect = topRect;
    
    
    inputWindow = inputRect.reduced(inputRect.getWidth() * 0.05, inputRect.getHeight() * 0.05);
    inputLabel = inputWindow.removeFromTop(20.f);
    
    outputWindow = outputRect.reduced(outputRect.getWidth() * 0.05, outputRect.getHeight() * 0.05);
    outputLabel = outputWindow.removeFromTop(20.f);
    
    
    bottomRect = bounds;
    auto temp = bottomRect;
    bottomRect = bottomRect.reduced(bounds.getWidth() * 0.08, 0);
    
    float knobWidth = bottomRect.getWidth() * 0.2;
    
    threshRect = bottomRect.removeFromLeft(knobWidth);
    threshLabel = threshRect.removeFromTop(20.f);
    threshRange = threshRect.removeFromBottom(20.f);
    
    ratioRect = bottomRect.removeFromLeft(knobWidth);
    ratioLabel = ratioRect.removeFromTop(20.f);
    ratioRange = ratioRect.removeFromBottom(20.f);
    
    attackRect = bottomRect.removeFromLeft(knobWidth);
    attackLabel = attackRect.removeFromTop(20.f);
    attackRange = attackRect.removeFromBottom(20.f);
    
    releaseRect = bottomRect.removeFromLeft(knobWidth);
    releaseLabel = releaseRect.removeFromTop(20.f);
    releaseRange = releaseRect.removeFromBottom(20.f);
    
    gainRect = bottomRect;
    gainLabel = gainRect.removeFromTop(20.f);
    gainRange = gainRect.removeFromBottom(20.f);
    
    float windowMid = inputWindow.getHeight() / 2 + inputWindow.getY();
    
    inputZero.setStart(inputWindow.getX(), windowMid );
    inputZero.setEnd(inputWindow.getX() + inputWindow.getWidth(),
                     windowMid);
    
    outputZero.setStart(outputWindow.getX(),
                        windowMid);
    outputZero.setEnd(outputWindow.getX() + outputWindow.getWidth(),
                      windowMid);
    
    bottomRect = temp;
    
    //======================= Sliders/Knobs ================================
    
    thresholdKnob.setBounds(threshRect.toNearestInt());
    ratioKnob.setBounds(ratioRect.toNearestInt());
    attackKnob.setBounds(attackRect.toNearestInt());
    releaseKnob.setBounds(releaseRect.toNearestInt());
    gainKnob.setBounds(gainRect.toNearestInt());

}



//========================= Paint Helpers =========================

void Squeeze1AudioProcessorEditor::drawWaveform(juce::Graphics& g, const juce::AudioBuffer<float>& buffer, juce::Rectangle<float> bounds, juce::Colour colour)
{
    g.setColour(colour);
    
    // Center the waveform vertically
    auto midY = bounds.getCentreY();
    auto width = bounds.getWidth();
    auto height = bounds.getHeight();

    auto* samples = buffer.getReadPointer(0); // Assuming mono for simplicity
    auto numSamples = buffer.getNumSamples();

    if (numSamples == 0)
        return; // No data to render

    juce::Path path;
    path.startNewSubPath(bounds.getX(), midY);

    for (int x = 0; x < width; ++x)
    {
        // Map pixel position to sample index
        auto sampleIndex = juce::jmap<int>(x, 0, (int)width, 0, numSamples - 1);

        // Map sample value to vertical position
        auto sampleValue = juce::jlimit(bounds.getY(), bounds.getBottom(), juce::jmap<float>(samples[sampleIndex], -1.0f, 1.0f, midY + height / 2, midY - height / 2));


        path.lineTo(bounds.getX() + x, sampleValue);
    }

    // Draw the waveform path
    g.strokePath(path, juce::PathStrokeType(2.0f));
}




void Squeeze1AudioProcessorEditor::drawRects(juce::Graphics& g) {
        g.setColour(juce::Colours::green);
        g.drawRoundedRectangle(inputWindow, cornerSize, windowSill);
        g.drawRoundedRectangle(outputWindow, cornerSize, windowSill);
    
        g.drawRect(threshRect);
        g.drawRect(ratioRect);
        g.drawRect(attackRect);
        g.drawRect(releaseRect);
        g.drawRect(gainRect);
    
        g.setColour(juce::Colours::blue);
        g.drawRect(inputLabel);
        g.drawRect(outputLabel);
    
        g.drawRect(threshLabel);
        g.drawRect(ratioLabel);
        g.drawRect(attackLabel);
        g.drawRect(releaseLabel);
        g.drawRect(gainLabel);
}

void Squeeze1AudioProcessorEditor::drawStaticWindows(juce::Graphics& g) {
    //g.setColour(juce::Colours::whitesmoke);
    //g.fillRect(topRect);
    
    g.setColour(juce::Colours::black);
    g.fillRoundedRectangle(inputWindow, cornerSize);
    g.fillRoundedRectangle(outputWindow, cornerSize);
    
    g.setColour(juce::Colours::silver);
    g.drawRoundedRectangle(inputWindow, cornerSize, windowSill);
    g.drawRoundedRectangle(outputWindow, cornerSize, windowSill);
    
//    g.setColour(juce::Colours::white);
//    g.drawLine(inputZero, 0.4f);
//    g.drawLine(outputZero, 0.4f);
}

void Squeeze1AudioProcessorEditor::drawLabels(juce::Graphics& g) {
    g.setColour(juce::Colours::darkslategrey);
    g.drawFittedText("Input", inputLabel.toNearestInt(), juce::Justification::centred, 1);
    g.drawFittedText("Output", outputLabel.toNearestInt(), juce::Justification::centred, 1);
    
    g.drawFittedText("Threshold", threshLabel.toNearestInt(), juce::Justification::centredBottom, 1);
    g.drawFittedText("Ratio", ratioLabel.toNearestInt(), juce::Justification::centredBottom, 1);
    g.drawFittedText("Attack", attackLabel.toNearestInt(), juce::Justification::centredBottom, 1);
    g.drawFittedText("Release", releaseLabel.toNearestInt(), juce::Justification::centredBottom, 1);
    g.drawFittedText("Gain", gainLabel.toNearestInt(), juce::Justification::centredBottom, 1);
    
    g.drawFittedText("-24dB | 0dB", threshRange.toNearestInt(), juce::Justification::centredTop, 1);
    g.drawFittedText("1:1 | 20:1", ratioRange.toNearestInt(), juce::Justification::centredTop, 1);
    g.drawFittedText("0ms | 100ms", attackRange.toNearestInt(), juce::Justification::centredTop, 1);
    g.drawFittedText("0ms | 1s", releaseRange.toNearestInt(), juce::Justification::centredTop, 1);
    g.drawFittedText("0dB | 24dB", gainRange.toNearestInt(), juce::Justification::centredTop, 1);
}
