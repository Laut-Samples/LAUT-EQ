/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

 // ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"


ResponseCurveComponent::ResponseCurveComponent(LAUTEQAudioProcessor& p) : audioProcessor(p),
//                                                                            leftChannelFifo(&audioProcessor.leftChannelFifo)
                                                                        leftPathProducer(audioProcessor.leftChannelFifo),
                                                                        rightPathProducer(audioProcessor.rightChannelFifo)

{
    const auto& params = audioProcessor.getParameters();
    for ( auto param : params )
    {
        param ->addListener(this);
    }
    
//    leftChannelFFTDataGenerator.changeOrder(FFTOrder::order2048);
//    monoBuffer.setSize(1, leftChannelFFTDataGenerator.getFFTSize());
    
    /*
     44100 khz / 2048 Bins = 23Hz pro Band
     */
    
    updateChain();
    
    startTimerHz(60);
}


 // ==============================================================================


ResponseCurveComponent::~ResponseCurveComponent()
{
    const auto& params = audioProcessor.getParameters();
    for ( auto param : params )
    {
        param->removeListener(this);
    }
}


 // ==============================================================================

void ResponseCurveComponent::parameterValueChanged (int parameterIndex, float newValue)
{
    parametersChanged.set(true);
}


 // ==============================================================================


void PathProducer::process(juce::Rectangle<float> fftBounds, double sampleRate)
{
    juce::AudioBuffer<float> tempIncomingBuffer;
    
    while ( leftChannelFifo->getNumCompleteBuffersAvailable() > 0)
    {
        if (leftChannelFifo->getAudioBuffer(tempIncomingBuffer) )
        {
            auto size = tempIncomingBuffer.getNumSamples();
            
            // Shifting data
            juce::FloatVectorOperations::copy(monoBuffer.getWritePointer(0,0),
                                              monoBuffer.getReadPointer(0, size),
                                              monoBuffer.getNumSamples() - size);
            
            // Copying data
            juce::FloatVectorOperations::copy(monoBuffer.getWritePointer(0, monoBuffer.getNumSamples() - size),
                                              tempIncomingBuffer.getReadPointer(0, 0),
                                              size);
            
            
            
            leftChannelFFTDataGenerator.produceFFTDataForRendering(monoBuffer, -48.f);
            
        }
    }
    
    
    
//    const auto fftBounds = getAnalysisArea().toFloat();
    const auto fftSize = leftChannelFFTDataGenerator.getFFTSize();
    const auto binWidth = sampleRate / (double)fftSize;
    
    while (leftChannelFFTDataGenerator.getNumAvailableFFTDataBlocks() > 0)
    {
        std::vector<float> fftdata;
        if ( leftChannelFFTDataGenerator.getFFTData(fftdata) )
        {
            pathProducer.generatePath(fftdata, fftBounds, fftSize, binWidth, -48.f);
        }
    }
    
    while (pathProducer.getNumPathsAvailable() )
    {
        pathProducer.getPath(leftChannelFFTPath);
    }
}


 // ==============================================================================

void ResponseCurveComponent::timerCallback()
{

    
    auto fftBounds = getAnalysisArea().toFloat();
    auto samplerate = audioProcessor.getSampleRate();
    
    leftPathProducer.process(fftBounds, samplerate);
    rightPathProducer.process(fftBounds, samplerate);
    
    startTimer(1000/20);
    
    
    if (parametersChanged.compareAndSetBool(false, true) )
    {
        updateChain();
     //   repaint();
    }
    
    repaint();
}

 // ==============================================================================

void ResponseCurveComponent::updateChain()
{
    auto chainSettings = getChainSettings(audioProcessor.apvts);
    auto peakCoefficiens = makePeakFilter(chainSettings, audioProcessor.getSampleRate());
    updateCoefficients(monoChain.get<ChainPositions::Peak>().coefficients, peakCoefficiens);
    
    auto lowCutCoefficients = makeLowCutFilter(chainSettings, audioProcessor.getSampleRate());
    auto highCutCoefficients = makeHighCutFilter(chainSettings, audioProcessor.getSampleRate());
    
    
    updateCutFilter(monoChain.get<ChainPositions::LowCut>(), lowCutCoefficients, chainSettings.lowCutSlope);
    updateCutFilter(monoChain.get<ChainPositions::HighCut>(), highCutCoefficients, chainSettings.lowCutSlope);
    
}
 // ==============================================================================
void ResponseCurveComponent::paint (juce::Graphics& g)
{
    g.fillAll(Colours::black);
    
    
    auto responseArea = getLocalBounds();
//    auto responseArea = bounds.removeFromTop(bounds.getHeight() * 0.33);
    
    auto w = responseArea.getWidth();
    
    auto& lowcut = monoChain.get<ChainPositions::LowCut>();
    auto& peak = monoChain.get<ChainPositions::Peak>();
    auto& highcut = monoChain.get<ChainPositions::HighCut>();
    
    auto samplerate = audioProcessor.getSampleRate();
    
    std::vector<double> mags;
    
    mags.resize(w);
    
    for( int i=0; i<w; i++)
    {
        double mag = 1.f;
        auto freq = mapToLog10(double(i) / double(w), 20.0, 20000.0);
        
        if(! monoChain.isBypassed<ChainPositions::Peak>() )
            mag *= peak.coefficients->getMagnitudeForFrequency(freq, samplerate);
        
        if(! lowcut.isBypassed<0>() )
            mag *= lowcut.get<0>().coefficients->getMagnitudeForFrequency(freq, samplerate);
        if(! lowcut.isBypassed<1>() )
            mag *= lowcut.get<1>().coefficients->getMagnitudeForFrequency(freq, samplerate);
        if(! lowcut.isBypassed<2>() )
            mag *= lowcut.get<2>().coefficients->getMagnitudeForFrequency(freq, samplerate);
        if(! lowcut.isBypassed<3>() )
            mag *= lowcut.get<3>().coefficients->getMagnitudeForFrequency(freq, samplerate);
        
        if(! highcut.isBypassed<0>() )
            mag *= highcut.get<0>().coefficients->getMagnitudeForFrequency(freq, samplerate);
        if(! highcut.isBypassed<1>() )
            mag *= highcut.get<1>().coefficients->getMagnitudeForFrequency(freq, samplerate);
        if(! highcut.isBypassed<2>() )
            mag *= highcut.get<2>().coefficients->getMagnitudeForFrequency(freq, samplerate);
        if(! highcut.isBypassed<3>() )
            mag *= highcut.get<3>().coefficients->getMagnitudeForFrequency(freq, samplerate);
        
        
        mags[i] = Decibels::gainToDecibels(mag);
    }
    
    
     // ==============================================================================
    
    // Response Curve
    Path responseCurve;
    
//    juce::ColourGradient gradient;
 
    const double outputMin = responseArea.getBottom();
    const double outputMax = responseArea.getY();
    auto map = [outputMin, outputMax](double input)
    {
        return jmap(input, -24.0, 24.0, outputMin, outputMax);
    };
    
    
     // ==============================================================================
    
//    //Filter Curve DRAW
//
//    responseCurve.startNewSubPath(responseArea.getX(), map(mags.front()));
//
//
//    for ( size_t i =1; i < mags.size(); i++)
//    {
//        responseCurve.lineTo(responseArea.getX() + i, map(mags[i]));
//    }
//
        // ==============================================================================
    
    
    //Spectrum
    
    
//    auto leftChannelFFTPath = leftPathProducer.getPath();
//    leftChannelFFTPath.applyTransform(AffineTransform().translation(responseArea.getX(), responseArea.getY()));
//
    
    //
    for ( auto y = 0; y < 5; y++ )
    {
        
        juce::Array<juce::Colour> colours { juce::Colours::red, juce::Colours::green, juce::Colours::blue };
        
        
        auto rightChannelFFTPath = rightPathProducer.getPath();
        
        // First Spec
        rightChannelFFTPath.applyTransform(AffineTransform().translation(responseArea.getX()+1, responseArea.getY()-0));
        
        g.setColour(colours[0+y]);
        
        // Draw Specs wishing out
        g.fillPath(rightChannelFFTPath,(AffineTransform().translation(responseArea.getX()+1, responseArea.getY()-0)));
        
        
        //Draw Lines to Wish out
        for ( double y2 = 0.5; y2 >= 0.0; y2 -= 0.1)
        {
        rightChannelFFTPath.applyTransform(AffineTransform().translation(responseArea.getX()+5, responseArea.getY()-1*10));
        PathStrokeType pst(y2);
        g.setColour(colours[0+y]);
        
//        ColourGradient gradient(juce::Colours::blue, responseArea.getX()/2,responseArea.getY()/2,
//                             Colours::orange, responseArea.getX()/2,true);
//
//        g.setGradientFill(gradient);
            
            
        g.strokePath(rightChannelFFTPath, PathStrokeType(y2));
            
        
        }
        
        
        for ( double y3 = 0; y3 < samplerate/10000; y3++)
        {
            rightChannelFFTPath.applyTransform(AffineTransform().translation(responseArea.getX()+5, responseArea.getY()-1*10));
            g.setColour(Colours::white);
//            g.strokePath(rightChannelFFTPath(0);
        }
        
        
//        rightChannelFFTPath.applyTransform(AffineTransform().translation(responseArea.getX()+3, responseArea.getY()-2*10));
//        g.setColour(Colours::red);
//        g.strokePath(rightChannelFFTPath, PathStrokeType(0.25f));
//
//        rightChannelFFTPath.applyTransform(AffineTransform().translation(responseArea.getX()+4, responseArea.getY()-3*10));
//        g.setColour(Colours::red);
//        g.strokePath(rightChannelFFTPath, PathStrokeType(0.1f));
//
        
//                for ( auto i = 0; i < samplerate/10000; i++ )
//                {
//
//                    auto rightChannelFFTPath = rightPathProducer.getPath();
//                    rightChannelFFTPath.applyTransform(AffineTransform().translation(responseArea.getX()-y, responseArea.getY()-y*10));
//
////
////                    auto leftChannelFFTPath = leftPathProducer.getPath();
////                    leftChannelFFTPath.applyTransform(AffineTransform().translation(responseArea.getX()-i, responseArea.getY()-i*10));
//
//
////                        // Response curve Definition
////                        g.setColour(Colours::white);
////                        g.strokePath(leftChannelFFTPath, PathStrokeType(1.f));
////
//                    g.setColour(Colours::red);
//                    g.strokePath(rightChannelFFTPath, PathStrokeType(1.f));
//
//                }
//
//
    
    }
            
            // Response curve Definition
            g.setColour(Colours::whitesmoke);
    
    
    
    // g.strokePath(rightChannelFFTPath, PathStrokeType(1.f));
    
    // Filter curve
    g.setColour(Colours::orange);
    g.drawRoundedRectangle(responseArea.toFloat(), 4.f, 1.f);
    
    g.setColour(Colours::white);
    g.strokePath(responseCurve, PathStrokeType(2.));
    
    
    
    
}

juce::Rectangle<int> ResponseCurveComponent::getRenderArea()
{
    auto bounds = getLocalBounds();
    
    bounds.removeFromTop(12);
    bounds.removeFromBottom(0);
    bounds.removeFromLeft(20);
    bounds.removeFromRight(20);
    
    return bounds;
}

juce::Rectangle<int> ResponseCurveComponent::getAnalysisArea()
{
    auto bounds = getRenderArea();
    bounds.removeFromTop(4);
    bounds.removeFromBottom(0);
    return bounds;
}

//==============================================================================
LAUTEQAudioProcessorEditor::LAUTEQAudioProcessorEditor (LAUTEQAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p),

responseCurveComponent(audioProcessor),
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
    
//    const auto& params = audioProcessor.getParameters();
//    for ( auto param : params )
//    {
//        param ->addListener(this);
//    }
    
//    startTimerHz(60);
    
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
//    const auto& params = audioProcessor.getParameters();
//    for ( auto param : params )
//    {
//        param->removeListener(this);
//    }
}

//==============================================================================
void LAUTEQAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll(Colours::black);
    
    
    
    
    
}

void LAUTEQAudioProcessorEditor::resized()
{
    
    // This is generally where you'll want to lay out the positions of any
    // subcomponents in your editor..
    
//    //dist
//    disChoice.setBounds(50, 50, 200, 50);
//    Threshold.setBounds(300, 25, 200, 50);
//    Mix.setBounds(300, 75, 200, 50);
    
    
    auto bounds = getLocalBounds();
    auto responseArea = bounds.removeFromTop(bounds.getHeight() * 0.33);
    
    responseCurveComponent.setBounds(responseArea);

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




void LAUTEQAudioProcessorEditorparameterGestureChanged (int parameterIndex, bool gestureIsStarting)
{
    
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
        &responseCurveComponent,
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
