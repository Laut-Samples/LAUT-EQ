/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include <array>
//==============================================================================//==============================================================================
//// FFT Data Generator


enum FFTOrder
{
    order1024 = 10,
    order2048 = 11,
    order4096 = 12,
    order8192 = 13
};

template<typename BlockType>
struct FFTDataGenerator
{
    /**
     produces the FFT data from an audio buffer.
     */
    // Feed Audio into FFT
    void produceFFTDataForRendering(const juce::AudioBuffer<float>& audioData, const float negativeInfinity)
    {
        const auto fftSize = getFFTSize(); // Order
        
        fftData.assign(fftData.size(), 0);
        auto* readIndex = audioData.getReadPointer(0);
        std::copy(readIndex, readIndex + fftSize, fftData.begin());
        
        // first apply a windowing function to our data
        window->multiplyWithWindowingTable (fftData.data(), fftSize);       // [1]
        
        // then render our FFT data..
        forwardFFT->performFrequencyOnlyForwardTransform (fftData.data());  // [2]
        
        int numBins = (int)fftSize / 2;
        
        //normalize the fft values.
        for( int i = 0; i < numBins; ++i )
        {
            auto v = fftData[i];
            //            fftData[i] /= (float) numBins;
            if( !std::isinf(v) && !std::isnan(v) )
            {
                v /= float(numBins);
            }
            else
            {
                v = 0.f;
            }
            fftData[i] = v;
        }
        
        //convert them to decibels
        for( int i = 0; i < numBins; ++i )
        {
            fftData[i] = juce::Decibels::gainToDecibels(fftData[i], negativeInfinity);
        }
        
        
        
        
        fftDataFifo.push(fftData);
    }
    
    void changeOrder(FFTOrder newOrder)
    {
        //when you change order, recreate the window, forwardFFT, fifo, fftData
        //also reset the fifoIndex
        //things that need recreating should be created on the heap via std::make_unique<>
        
        order = newOrder;
        auto fftSize = getFFTSize();
        
        forwardFFT = std::make_unique<juce::dsp::FFT>(order);
        window = std::make_unique<juce::dsp::WindowingFunction<float>>(fftSize, juce::dsp::WindowingFunction<float>::hamming);
        
//        enum WindowingMethod
//        {
//            rectangular = 0,
//            triangular,
//            hann,
//            hamming,
//            blackman,
//            blackmanHarris,
//            flatTop,
//            kaiser,
//            numWindowingMethods
//        };
        
        fftData.clear();
        fftData.resize(fftSize * 2, 0);
        
        fftDataFifo.prepare(fftData.size());
    }
    //==============================================================================

    int getFFTSize() const { return 1 << order; }
    int getNumAvailableFFTDataBlocks() const { return fftDataFifo.getNumAvailableForReading(); }        // so how much fft data we have available
    //==============================================================================
    bool getFFTData(BlockType& fftData) { return fftDataFifo.pull(fftData); }                           // get fft data
    
    
    // GET
    
private:
    FFTOrder order;
    BlockType fftData;
    std::unique_ptr<juce::dsp::FFT> forwardFFT;
    std::unique_ptr<juce::dsp::WindowingFunction<float>> window;
    
    Fifo<BlockType> fftDataFifo;
};

//==============================================================================//==============================================================================
/// PATH Generator class
// Take the fft data info about size and spit out a path

template<typename PathType>
struct AnalyzerPathGenerator
{
    /*
     converts 'renderData[]' into a juce::Path
     */
    
    // feed Data
    void generatePath(const std::vector<float>& renderData,
                      juce::Rectangle<float> fftBounds,
                      int fftSize,
                      float binWidth,
                      float negativeInfinity)
    {
        auto top = fftBounds.getY();
        auto bottom = fftBounds.getHeight();
        auto width = fftBounds.getWidth();
        
        int numBins = (int)fftSize / 2;
        
        PathType p;
        p.preallocateSpace(3 * (int)fftBounds.getWidth());
        
        auto map = [bottom, top, negativeInfinity](float v)
        {
            return juce::jmap(v,
                              negativeInfinity, 0.f,
                              float(bottom+10),   top);
        };
        
        auto y = map(renderData[0]);
        
        //        jassert( !std::isnan(y) && !std::isinf(y) );
        if( std::isnan(y) || std::isinf(y) )
            y = bottom;
        
        p.startNewSubPath(0, y);
        
        const int pathResolution = 1; //you can draw line-to's every 'pathResolution' pixels.
        
        for( int binNum = 1; binNum < numBins; binNum += pathResolution )
        {
            y = map(renderData[binNum]);
            
            //            jassert( !std::isnan(y) && !std::isinf(y) );
            
            if( !std::isnan(y) && !std::isinf(y) )
            {
                auto binFreq = binNum * binWidth;
                auto normalizedBinX = juce::mapFromLog10(binFreq, 20.f, 20000.f);
                int binX = std::floor(normalizedBinX * width);
                p.lineTo(binX, y);
            }
        }
        
        pathFifo.push(p);
    }
    
    int getNumPathsAvailable() const
    {
        return pathFifo.getNumAvailableForReading();
    }
    
    // Pull data // path output 
    bool getPath(PathType& path)
    {
        return pathFifo.pull(path);
    }
    
private:
    Fifo<PathType> pathFifo;
};

//==============================================================================SLIDER //==============================================================================
//// Slider Component


struct CustomRotarySlider : juce::Slider
{
    CustomRotarySlider() : juce::Slider(juce::Slider::SliderStyle::RotaryHorizontalDrag,
                                        juce::Slider::TextEntryBoxPosition::NoTextBox)
    {
        
    }
};

 //==============================================================================/// PATH PRODUCER //==============================================================================


struct PathProducer
{
    // Constructor for..
    
    PathProducer(SingleChannelSampleFifo<LAUTEQAudioProcessor::BlockType>& scsf) :
    leftChannelFifo(&scsf)
    {
        leftChannelFFTDataGenerator.changeOrder(FFTOrder::order8192);
        monoBuffer.setSize(1, leftChannelFFTDataGenerator.getFFTSize());
    }
    
    
    // give rectangle , sample rate
    void process(juce::Rectangle<float> fftBounds, double sampleRate);
    
    
    juce::Path getPath() {return leftChannelFFTPath; }
    
    private:
    
    // Pointer to Audio Process Channels 
    SingleChannelSampleFifo<LAUTEQAudioProcessor::BlockType>* leftChannelFifo;
    
    
    // Monobuffer because single channel is mono for FFT
    juce::AudioBuffer<float> monoBuffer;
    
    // Instance of the class
    FFTDataGenerator<std::vector<float>> leftChannelFFTDataGenerator;
    
    // pathProducer in Path Generator
    AnalyzerPathGenerator<juce::Path> pathProducer;
    
    // path to draw and pull into 
    juce::Path leftChannelFFTPath;
};


//============================================================================== RESPONSE CURVE //==============================================================================

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
    
    
//    
//    juce::Array<float> getHistory()
//    {
//        return history;
//    }

private:
        LAUTEQAudioProcessor& audioProcessor;
        juce::Atomic<bool> parametersChanged { false };
        void updateChain();
    
        MonoChain monoChain;
    
    juce::Rectangle<int> getRenderArea();
    
    juce::Rectangle<int> getAnalysisArea();
    
    
    
    // FFT DATA To Path Producer 
    PathProducer leftPathProducer, rightPathProducer;
    
    juce::ColourGradient grand;
    

    
};

//============================================================================== EDITOR //==============================================================================
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
