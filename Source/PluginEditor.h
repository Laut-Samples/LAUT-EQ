/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

struct CustomRotarySlider : juce::Slider
{
    CustomRotarySlider() : juce::Slider(juce::Slider::SliderStyle::RotaryHorizontalDrag,
                                        juce::Slider::TextEntryBoxPosition::NoTextBox)
    {
        
    }
};

struct ResponseCurveComponent : juce::Component,
juce::AudioProcessorParameter::Listener,
juce::Timer
{
    ResponseCurveComponent(LAUTEQAudioProcessor&);
    ~ResponseCurveComponent();
    
    void parameterValueChanged (int parameterIndex, float newValue) override;
    
    void parameterGestureChanged (int parameterIndex, bool gestureIsStarting) override { } ;
    
    void timerCallback() override;
    
    void paint(juce::Graphics& g) override;
    

private:
        LAUTEQAudioProcessor& audioProcessor;
        juce::Atomic<bool> parametersChanged { false };
    
    MonoChain monoChain;
};

//==============================================================================
/**
*/
class LAUTEQAudioProcessorEditor  : public  juce::AudioProcessorEditor,
                                            

                                    private juce::ComboBox::Listener,
                                            juce::Slider::Listener
{
public:
    LAUTEQAudioProcessorEditor (LAUTEQAudioProcessor&);
    ~LAUTEQAudioProcessorEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;


    
private:
    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    
    LAUTEQAudioProcessor& audioProcessor;
    
    //juce::Atomic<bool> parametersChanged { false };

    CustomRotarySlider  peakFreqSlider,
                        peakGainSlider,
                        peakQualitySlider,
                        lowCutFreqSlider,
                        highCutFreqSlider,
                        lowCutSlopeSlider,
                        highCutSlopeSlider;
    
    ResponseCurveComponent responseCurveComponent;
    
    using APVTS = juce::AudioProcessorValueTreeState;
    using Attachment = APVTS::SliderAttachment;
    
    Attachment          peakFreqAttachment,
                        peakGainAttachment,
                        peakQualityAttachment,
                        lowCutFreqAttachment,
                        highCutFreqAttachment,
                        lowCutSlopeAttachment,
                        highCutSlopeAttachment;

    
    std::vector<juce::Component*> getComps();
    
    //MonoChain monoChain;
    
    // Dist
    
    void comboBoxChanged(juce::ComboBox* comboBoxThatHasChanged) override;
    juce::ComboBox disChoice;
    
    void sliderValueChanged(juce::Slider* sliderThatHasChanged) override;
    
    juce::Slider Threshold;
    juce::Slider Mix;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LAUTEQAudioProcessorEditor)
};
