/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
LAUTEQAudioProcessorEditor::LAUTEQAudioProcessorEditor (LAUTEQAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p),

peakFreqAttachment(audioProcessor.apvts, "Peak Freq", peakFreqSlider),
peakGainAttachment(audioProcessor.apvts, "Peak Gain", peakGainSlider),
peakQualityAttachment(audioProcessor.apvts, "Peak Quality", peakQualitySlider),
lowCutFreqAttachment(audioProcessor.apvts, "LowCut Freq", lowCutFreqSlider),
highCutFreqAttachment(audioProcessor.apvts, "HighCut Freq", highCutFreqSlider),
lowCutSlopeAttachment(audioProcessor.apvts, "LowCut Slope", lowCutSlopeSlider),
highCutSlopeAttachment(audioProcessor.apvts, "HighCut Slope", highCutSlopeSlider)

{
    for (auto* comp : getComps() )
    {
        addAndMakeVisible(comp);
    }
    // Make sure that before the constructor has finished, you've set the
    // editor's size to whatever you need it to be.
    setSize (600, 400);
    
    
    // Dist
    addAndMakeVisible(&disChoice);
    disChoice.addItem("Hard Clip", 1);
    disChoice.addItem("Soft Clip", 2);
    disChoice.addItem("Half-Wave Rect", 3);
    disChoice.setSelectedId(1);
    disChoice.addListener(this);
    
    addAndMakeVisible(&Threshold);
    Threshold.setRange(0.0f, 1.0f, 0.001);
    Threshold.addListener(this);
    
    addAndMakeVisible(&Mix);
    Mix.setRange(0.0f, 1.0f, 0.001);
    Mix.addListener(this);
    
    
}

LAUTEQAudioProcessorEditor::~LAUTEQAudioProcessorEditor()
{
}

//==============================================================================
void LAUTEQAudioProcessorEditor::paint (juce::Graphics& g)
{
    // (Our component is opaque, so we must completely fill the background with a solid colour)
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));

    g.setColour (juce::Colours::white);
    g.setFont (15.0f);
    // g.drawFittedText ("Hello World!", getLocalBounds(), juce::Justification::centred, 1);
}

void LAUTEQAudioProcessorEditor::resized()
{
    
    // This is generally where you'll want to lay out the positions of any
    // subcomponents in your editor..
    
    disChoice.setBounds(50, 50, 200, 50);
    Threshold.setBounds(300, 25, 200, 50);
    Mix.setBounds(300, 75, 200, 50);
    
    
    auto bounds = getLocalBounds();
    auto responseArea = bounds.removeFromTop(bounds.getHeight() * 0.33);

    auto lowCutArea = bounds.removeFromLeft(bounds.getWidth() * 0.33);
    auto highCutArea = bounds.removeFromRight(bounds.getWidth() * 0.5);

    lowCutFreqSlider.setBounds(lowCutArea.removeFromTop(lowCutArea.getHeight() * 0.5));
    lowCutSlopeSlider.setBounds(lowCutArea);

    highCutFreqSlider.setBounds(highCutArea.removeFromTop(highCutArea.getHeight() * 0.5));
    highCutSlopeSlider.setBounds(highCutArea);

    peakFreqSlider.setBounds(bounds.removeFromTop(bounds.getHeight()* 0.33));
    peakGainSlider.setBounds(bounds.removeFromTop(bounds.getHeight()* 0.5));
    peakQualitySlider.setBounds(bounds);
    
    
}

std::vector<juce::Component*> LAUTEQAudioProcessorEditor::getComps()
{
    return
    {
        &peakFreqSlider,
        &peakGainSlider,
        &peakQualitySlider,
        &lowCutFreqSlider,
        &highCutFreqSlider,
        &highCutSlopeSlider,
        &lowCutSlopeSlider,
    };
}

void LAUTEQAudioProcessorEditor::comboBoxChanged(juce::ComboBox * comboBoxThatHasChanged)
{
    audioProcessor.menuChoice = comboBoxThatHasChanged->getSelectedId();
}

void LAUTEQAudioProcessorEditor::sliderValueChanged(juce::Slider * sliderThatHasChanged)
{
    if (&Mix == sliderThatHasChanged)
    {
        audioProcessor.mix = sliderThatHasChanged->getValue();
    }
    if (&Threshold == sliderThatHasChanged)
    {
        audioProcessor.thresh = sliderThatHasChanged->getValue();
    }
}
