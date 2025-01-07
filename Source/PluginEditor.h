/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

//==============================================================================

class GlobalLookAndFeel : public juce::LookAndFeel_V4
{
public:
    GlobalLookAndFeel()
    {
        // Load your custom font (ensure the font file is added to Projucer Binary Resources)
        customTypeface = juce::Typeface::createSystemTypefaceFor(BinaryData::Jersey15Regular_ttf,
                                                                 BinaryData::Jersey15Regular_ttfSize);
    }

    // Override to use the custom font globally
    juce::Typeface::Ptr getTypefaceForFont(const juce::Font& font) override
    {
        return customTypeface;
    }

private:
    juce::Typeface::Ptr customTypeface;
};


class CustomLookAndFeel : public juce::LookAndFeel_V4
{
public:
    CustomLookAndFeel(juce::Slider& threshold, juce::Slider& ratio, juce::Slider& attack,
                          juce::Slider& release, juce::Slider& gain)
            : thresholdKnob(&threshold), ratioKnob(&ratio), attackKnob(&attack),
              releaseKnob(&release), gainKnob(&gain) {}
    
    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPosProportional, float rotaryStartAngle, float rotaryEndAngle, juce::Slider& slider) override
    {
        constexpr auto pi = juce::MathConstants<float>::pi;
        auto bounds = juce::Rectangle<float>(x, y, width, height).reduced(12.f, 8.f);
        auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) / 2.0f;
        auto center = bounds.getCentre();
        auto toAngle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);
        
        g.setColour(juce::Colours::black);
        g.fillEllipse(bounds);
        
        auto thumbRadius = 4.0f; // Size of the thumb
        auto thumbX = center.x + radius * 0.8f * std::cos(toAngle - pi * 0.5);
        auto thumbY = center.y + radius * 0.8f * std::sin(toAngle - pi * 0.5);
        g.setColour(juce::Colours::white);
        g.fillEllipse(thumbX - thumbRadius, thumbY - thumbRadius, thumbRadius * 2.0f, thumbRadius * 2.0f);
        
        juce::String text = getText(slider);
        g.drawFittedText(text, bounds.toNearestInt(), juce::Justification::centred, 1);

    }
    
    juce::String getText(juce::Slider& slider) {
        juce::String text = "";
        if (&slider == thresholdKnob){
            text << thresholdKnob->getValue() << "dB";
        }
        else if (&slider == ratioKnob){
            text << ratioKnob->getValue() << ":1";
        }
        else if (&slider == attackKnob){
            text << attackKnob->getValue() << "ms";
        }
        else if (&slider == releaseKnob){
            if (releaseKnob->getValue() < 1000.f) {
                text << releaseKnob->getValue() << "ms";
            } else {
                text << releaseKnob->getValue() / 1000 << "s";
            }
        }
        else if (&slider == gainKnob){
            text << gainKnob->getValue() << "db";
        }
        return text;
    }
    
private:
    juce::Slider* thresholdKnob;
    juce::Slider* ratioKnob;
    juce::Slider* attackKnob;
    juce::Slider* releaseKnob;
    juce::Slider* gainKnob;
};

//==============================================================================
/**
*/

class WaveformComponent : public juce::Component
{
public:
    WaveformComponent() {}

    void setBuffer(const juce::AudioBuffer<float>* newBuffer)
    {
        buffer = newBuffer;
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colours::black); // Background

        if (buffer == nullptr || buffer->getNumSamples() == 0)
            return;

        g.setColour(juce::Colours::white); // Waveform color

        auto width = getWidth();
        auto height = getHeight();
        auto numSamples = buffer->getNumSamples();
        auto* samples = buffer->getReadPointer(0);

        juce::Path path;
        path.startNewSubPath(0, height / 2);

        for (int i = 0; i < width; ++i)
        {
            auto sampleIndex = juce::jmap(i, 0, width, 0, numSamples);
            auto sampleValue = juce::jmap(samples[sampleIndex], -1.0f, 1.0f, static_cast<float>(height), 0.0f);
            path.lineTo(i, sampleValue);
        }

        g.strokePath(path, juce::PathStrokeType(2.0f)); // Draw waveform
    }

private:
    const juce::AudioBuffer<float>* buffer = nullptr;
};


//==============================================================================
/**
*/
class Squeeze1AudioProcessorEditor  : public juce::AudioProcessorEditor,
                                      private juce::Timer,
                                      public juce::Slider::Listener
{
public:
    Squeeze1AudioProcessorEditor (Squeeze1AudioProcessor&);
    ~Squeeze1AudioProcessorEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;
    
    
    
    std::vector<juce::Slider*> knobs;
    
    void sliderValueChanged(juce::Slider* slider) override
        {
            // Check if the slider impacts the envelope
            if (slider == &thresholdKnob || slider == &attackKnob || slider == &releaseKnob || slider == &ratioKnob)
            {
                isEnvelopeVisible = true;
                repaint(); // Redraw to show the envelope
            }
        }
    
    void sliderDragEnded(juce::Slider* slider) override
        {
            if (slider == &thresholdKnob || slider == &attackKnob || slider == &releaseKnob || slider == &ratioKnob)
            {
                isEnvelopeVisible = false;
                repaint(); // Redraw to hide the envelope
            }
        }
    
    void timerCallback() override
    {
        inputWaveform.setBuffer(&audioProcessor.getInputBuffer());
        outputWaveform.setBuffer(&audioProcessor.getOutputBuffer());
        repaint();
    }
    
    void drawWaveform(juce::Graphics& g, const juce::AudioBuffer<float>& buffer, juce::Rectangle<float> bounds, juce::Colour colour);
    
    void drawEnvelope(juce::Graphics& g, const juce::Rectangle<int>& bounds)
    {
        juce::Path envelopePath;

        float midY = bounds.getCentreY();

            // Get slider values
            float threshold = thresholdKnob.getValue(); // dB
            float attack = attackKnob.getValue();       // ms
            float release = releaseKnob.getValue();     // ms
            float ratio = ratioKnob.getValue();         // ratio

        // Scale attack and release times relative to the bounds width
            float totalWidth = static_cast<float>(bounds.getWidth());
            float attackTime = juce::jmap(attack, 0.0f, 100.0f, 0.0f, totalWidth * 0.5f);
            float releaseTime = juce::jmap(release, 0.0f, 1000.0f, 0.0f, totalWidth * 0.5f);

            // Normalize threshold height
            float thresholdHeight = juce::jmap(threshold, -30.0f, 0.0f, midY, (float)bounds.getY());

            // Constrain threshold height to stay within bounds
            thresholdHeight = juce::jlimit<float>(bounds.getY(), midY, thresholdHeight);

            // Calculate compressed height based on ratio
            float compressedHeight = juce::jmap(ratio, 1.0f, 20.0f, thresholdHeight, midY);

            // Start the envelope at the midpoint (left side of the bounds)
            envelopePath.startNewSubPath(bounds.getX(), midY);

            // Attack: Line from the midpoint to the threshold height
            envelopePath.lineTo(bounds.getX() + attackTime, thresholdHeight);

            // Sustain: Line across the compressed height
            envelopePath.lineTo(bounds.getX() + attackTime + (totalWidth - attackTime - releaseTime), compressedHeight);

            // Release: Line from compressed height back to midpoint
            envelopePath.lineTo(bounds.getRight(), midY);

        // Reflect the path below the midpoint
        juce::Path mirroredPath;
        mirroredPath.addPath(envelopePath, juce::AffineTransform::scale(1.0f, -1.0f, bounds.getX(), midY));

        // Draw the envelope
        g.setColour(juce::Colours::lightskyblue);
        g.strokePath(envelopePath, juce::PathStrokeType(1.0f));
        g.setColour(juce::Colours::violet);
        g.strokePath(mirroredPath, juce::PathStrokeType(1.0f));
    }

    
    void mouseDown(const juce::MouseEvent& event) override
        {
            if (inputWindow.contains(event.getPosition().toFloat()))
            {
                drawInputWaveform = !drawInputWaveform;
                repaint(); 
            }
        }

private:
    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    Squeeze1AudioProcessor& audioProcessor;
    
    
    
    bool isEnvelopeVisible = false;
    bool drawInputWaveform;

    WaveformComponent inputWaveform;
    WaveformComponent outputWaveform;
    
    juce::Slider thresholdKnob;
    juce::Slider ratioKnob;
    juce::Slider attackKnob;
    juce::Slider releaseKnob;
    juce::Slider gainKnob;
    
    
    
    using Attachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> thresholdAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> ratioAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attackAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> releaseAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> gainAttachment;
    
    CustomLookAndFeel customLookAndFeel;
    
    GlobalLookAndFeel globalLookAndFeel;

    //==============================================================================
    
    
    void drawRects(juce::Graphics& g); //for debugging
    void drawStaticWindows(juce::Graphics& g);
    void drawLabels(juce::Graphics& g);
    
    juce::Rectangle<float> topRect;
        juce::Rectangle<float> inputRect;
            juce::Rectangle<float> inputLabel;
            juce::Rectangle<float> inputWindow; //(rounded)
                juce::Line<float> inputZero;
        juce::Rectangle<float> outputRect;
            juce::Rectangle<float> outputLabel;
            juce::Rectangle<float> outputWindow; //(rounded)
                juce::Line<float> outputZero;
    
    juce::Rectangle<float> bottomRect;
        juce::Rectangle<float> threshRect;
        juce::Rectangle<float> threshLabel;
        juce::Rectangle<float> threshRange;
    
        juce::Rectangle<float> ratioRect;
        juce::Rectangle<float> ratioLabel;
        juce::Rectangle<float> ratioRange;
    
        juce::Rectangle<float> attackRect;
        juce::Rectangle<float> attackLabel;
        juce::Rectangle<float> attackRange;
    
        juce::Rectangle<float> releaseRect;
        juce::Rectangle<float> releaseLabel;
        juce::Rectangle<float> releaseRange;
    
        juce::Rectangle<float> gainRect;
        juce::Rectangle<float> gainLabel;
        juce::Rectangle<float> gainRange;
    
    
    
    
    float cornerSize = 5.0f;
    float windowSill = 2.f;
    
    

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Squeeze1AudioProcessorEditor)
};
