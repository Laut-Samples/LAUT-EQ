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



//==============================================================================
/**
*/
class LAUTEQAudioProcessorEditor  : public  juce::AudioProcessorEditor,
                                            juce::AudioProcessorParameter::Listener,
                                            juce::Timer,

                                    private juce::ComboBox::Listener,
                                            juce::Slider::Listener
{
public:
    LAUTEQAudioProcessorEditor (LAUTEQAudioProcessor&);
    ~LAUTEQAudioProcessorEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

    void parameterValueChanged (int parameterIndex, float newValue) override;
    
    /** Indicates that a parameter change gesture has started.
     
     E.g. if the user is dragging a slider, this would be called with gestureIsStarting
     being true when they first press the mouse button, and it will be called again with
     gestureIsStarting being false when they release it.
     
     IMPORTANT NOTE: This will be called synchronously, and many audio processors will
     call it during their audio callback. This means that not only has your handler code
     got to be completely thread-safe, but it's also got to be VERY fast, and avoid
     blocking. If you need to handle this event on your message thread, use this callback
     to trigger an AsyncUpdater or ChangeBroadcaster which you can respond to later on the
     message thread.
     */
    void parameterGestureChanged (int parameterIndex, bool gestureIsStarting) override { } ;
    
    void timerCallback() override;
    
private:
    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    
    LAUTEQAudioProcessor& audioProcessor;
    
    juce::Atomic<bool> parametersChanged { false };

    CustomRotarySlider  peakFreqSlider,
                        peakGainSlider,
                        peakQualitySlider,
                        lowCutFreqSlider,
                        highCutFreqSlider,
                        lowCutSlopeSlider,
                        highCutSlopeSlider;
    
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
    
    MonoChain monoChain;
    
    // Dist
    
    void comboBoxChanged(juce::ComboBox* comboBoxThatHasChanged) override;
    juce::ComboBox disChoice;
    
    void sliderValueChanged(juce::Slider* sliderThatHasChanged) override;
    
    juce::Slider Threshold;
    juce::Slider Mix;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LAUTEQAudioProcessorEditor)
};
