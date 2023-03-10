/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

 // ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"


ResponseCurveComponent::ResponseCurveComponent(LAUTEQAudioProcessor& p) : audioProcessor(p),
//                                                                            leftChannelFifo(&audioProcessor.leftChannelFifo)

//==============================================================================  Initialize FIFO

                                                                        leftPathProducer(audioProcessor.leftChannelFifo),
                                                                        rightPathProducer(audioProcessor.rightChannelFifo)

//==============================================================================

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
    
    startTimerHz(60000);
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


 // ============================================================================== PATH PRODUCER // ==============================================================================


void PathProducer::process(juce::Rectangle<float> fftBounds, double sampleRate)
{
    /// Temporary Buffer
    juce::AudioBuffer<float> tempIncomingBuffer;
    
    // while more than 0 buffer available
    while ( leftChannelFifo->getNumCompleteBuffersAvailable() > 0)
    {
        // pull get audio buffer
        if (leftChannelFifo->getAudioBuffer(tempIncomingBuffer) )
        {
            // get size of incoming buffer
            auto size = tempIncomingBuffer.getNumSamples();
            
            
            // Shifting data
            juce::FloatVectorOperations::copy(monoBuffer.getWritePointer(0,0),          // init with index 0
                                              monoBuffer.getReadPointer(0, size/2),       // start with first block and size // select every second
                                              monoBuffer.getNumSamples() - size);       // shift next block size to Gui
            
            // Copying from temp buffer to mono buffer data
            juce::FloatVectorOperations::copy(monoBuffer.getWritePointer(0, monoBuffer.getNumSamples() - size), // copy to mono buffer with sample index shift next block 
                                              tempIncomingBuffer.getReadPointer(0, 0),      // source temp incoming Buffer
                                              size);                                        // Copy samples in Size data
            
            
            // Sending Buffers to FFT Data Generator //Producing FFT Data Blocks 
            leftChannelFFTDataGenerator.produceFFTDataForRendering(monoBuffer, -48.f);
            
        }
    }
    
    // If there are fft data buffers to pull
        // if we can pull a buffer
            // generate a path
    
//    const auto fftBounds = getAnalysisArea().toFloat();                   // Bounding box
    const auto fftSize = leftChannelFFTDataGenerator.getFFTSize();          // get fftsize
    
    /*
     44100 khz / 2048 Bins = 23Hz pro Band
     */
    const auto binWidth = sampleRate / (double)fftSize;                     //
    
    // while it has fft blocks
    while (leftChannelFFTDataGenerator.getNumAvailableFFTDataBlocks() > 0)
    {
        // get fft blocks
        std::vector<float> fftdata;     // History
        if ( leftChannelFFTDataGenerator.getFFTData(fftdata) )
        {
            //if we able to pull fft blocks
            pathProducer.generatePath(fftdata, fftBounds, fftSize, binWidth, -48.f);
        }
    }
    
    
    // while there are paths that can be pull
    // pull as many as we can
    // display the most recent
    
    while (pathProducer.getNumPathsAvailable() )
    {
        pathProducer.getPath(leftChannelFFTPath);
    }
}


 // ============================================================================== RESPONSECURVE // ==============================================================================

void ResponseCurveComponent::timerCallback()
{

    
    auto fftBounds = getAnalysisArea().toFloat();
    auto samplerate = audioProcessor.getSampleRate();
    
    leftPathProducer.process(fftBounds, samplerate);
    rightPathProducer.process(fftBounds, samplerate);
    
    startTimer(100/20);
    
    
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
    
    
    //
    
    
    
    for ( auto y = 0; y < 5; y++ )
    {
        
        juce::Array<juce::Colour> colours { juce::Colours::red, juce::Colours::green, juce::Colours::blue };
        
        
        auto rightChannelFFTPath = rightPathProducer.getPath();
        auto leftChannelFFTPath = leftPathProducer.getPath();
        
        // First Spec
        rightChannelFFTPath.applyTransform(AffineTransform().translation(responseArea.getX()+1, responseArea.getY()*y));
        g.setColour(colours[0+y]);
        
        
        
        
        leftChannelFFTPath.applyTransform(AffineTransform().translation(responseArea.getX()+1, responseArea.getY()*y));
        g.setColour(colours[0+y]);
        

        
        // Draw Specs
        g.strokePath(rightChannelFFTPath, PathStrokeType(1.f));
        g.strokePath(leftChannelFFTPath, PathStrokeType(1.f));
        
//        PathStrokeType (float strokeThickness,
//                        JointStyle jointStyle,
//                        EndCapStyle endStyle = butt) noexcept;
        

//
        
        // FILL OUT
//        g.fillPath(rightChannelFFTPath,(AffineTransform().translation(responseArea.getX()+1, responseArea.getY()-0)));
        
        
        
        // Draw Multiple Lines
        
        //Draw Lines to Wish out
//        for ( double y2 = 0.5; y2 >= 0.0; y2 -= 0.1)
//        {
//
//        rightChannelFFTPath.applyTransform(AffineTransform().translation(responseArea.getX()+5, responseArea.getY()-1*10));
//        PathStrokeType pst(y2);
//        g.setColour(colours[0+y]);
//
//
//        //Response curve Definition
//        g.setColour(Colours::white);
//        g.strokePath(leftChannelFFTPath, PathStrokeType(1.f));
//
//        g.strokePath(rightChannelFFTPath, PathStrokeType(y2));
//
//
//        }
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
//============================================================================== EDITOR AREA //==============================================================================

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

//============================================================================== EDITOR //==============================================================================



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
    
    
    
    
    auto bounds = getLocalBounds();
    auto responseArea = bounds.removeFromTop(bounds.getHeight() * 0.33);
    
    responseCurveComponent.setBounds(responseArea);

    auto lowCutArea = bounds.removeFromLeft(bounds.getWidth() * 0.33);
    auto highCutArea = bounds.removeFromRight(bounds.getWidth() * 0.5);

    lowCutFreqSlider.setBounds(lowCutArea.removeFromTop(lowCutArea.getHeight() * 0.5));
//    lowCutSlopeSlider.setBounds(lowCutArea);

    highCutFreqSlider.setBounds(highCutArea.removeFromTop(highCutArea.getHeight() * 0.5));
    //highCutSlopeSlider.setBounds(highCutArea);

    peakFreqSlider.setBounds(bounds.removeFromTop(bounds.getHeight()* 0.33));
    peakGainSlider.setBounds(bounds.removeFromTop(bounds.getHeight()* 0.5));
    peakQualitySlider.setBounds(bounds);
    
    
    //dist
    disChoice.setBounds(lowCutArea.removeFromBottom(lowCutArea.getHeight() * 0.5));
    Threshold.setBounds(highCutArea.removeFromTop(highCutArea.getHeight() * 0.5));
    Mix.setBounds(highCutArea);
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
