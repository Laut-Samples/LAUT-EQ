/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
LAUTEQAudioProcessor::LAUTEQAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       )
#endif
{
}

LAUTEQAudioProcessor::~LAUTEQAudioProcessor()
{
}

//==============================================================================
const juce::String LAUTEQAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool LAUTEQAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool LAUTEQAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool LAUTEQAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double LAUTEQAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int LAUTEQAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int LAUTEQAudioProcessor::getCurrentProgram()
{
    return 0;
}

void LAUTEQAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String LAUTEQAudioProcessor::getProgramName (int index)
{
    return {};
}

void LAUTEQAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void LAUTEQAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    
    
    // This structure is passed into a DSP algorithm's prepare() method, and contains
    // information about various aspects of the context in which it can expect to be called.
    juce::dsp::ProcessSpec spec;
    spec.maximumBlockSize = samplesPerBlock;
    spec.numChannels = 1;
    spec.sampleRate = sampleRate;
    
    
    // prepare process spec
    leftChain.prepare(spec);
    rightChain.prepare(spec);
    
    
    updateFilters();
    
}

void LAUTEQAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool LAUTEQAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
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

void LAUTEQAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    
    
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // In case we have more outputs than inputs, this code clears any output
    // channels that didn't contain input data, (because these aren't
    // guaranteed to be empty - they may contain garbage).
    // This is here to avoid people getting screaming feedback
    // when they first compile a plugin, but obviously you don't need to keep
    // this code if your algorithm always overwrites all the output channels.
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    // Dist
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
    {
        auto* channelData = buffer.getWritePointer(channel);
        
        for (int i = 0; i < buffer.getNumSamples(); ++i) {
            
            auto input = channelData[i];
            auto cleanOut = channelData[i];
            
            if (menuChoice == 1)
                //Hard Clipping
            {
                if (input > thresh)
                {
                    input = thresh;
                }
                else if (input < -thresh)
                {
                    input = -thresh;
                }
                else
                {
                    input = input;
                }
            }
            if (menuChoice == 2)
                //Soft Clipping Exp
            {
                if (input > thresh)
                {
                    input = 1.0f - expf(-input);
                }
                else
                {
                    input = -1.0f + expf(input);
                }
            }
            if (menuChoice == 3)
                //Half-Wave Rectifier
            {
                if (input > thresh)
                {
                    input = input;
                }
                else
                {
                    input = 0;
                }
            }
            channelData[i] = ((1 - mix) * cleanOut) + (mix * input);
        }
    }
    
    // Jump to update Filter function
    updateFilters();
    
    // create dsp sample block initialized with buffer
    juce::dsp::AudioBlock<float> block(buffer);
    
    /** Returns an AudioBlock that represents one of the channels in this block. */
    auto leftBlock = block.getSingleChannelBlock(0);                                // get block channel 0 and store in leftBlock
    auto rightBlock = block.getSingleChannelBlock(1);                               // get block channel 1 and store in rightBlock
    
    
    //This context is intended for use in situations where a single block is being used
    //    for both the input and output, so it will return the same object for both its
    //        getInputBlock() and getOutputBlock() methods.
            
    juce::dsp::ProcessContextReplacing<float> leftContext(leftBlock);               // replace leftcontext with leftBlock
    juce::dsp::ProcessContextReplacing<float> rightContext(rightBlock);             // replace rightcontext with rightBlock
    
    /** Process `context` through all inner processors in sequence. */
    leftChain.process(leftContext);                                                 // process replacing
    rightChain.process(rightContext);                                               // process replacing
    
}

//==============================================================================
bool LAUTEQAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* LAUTEQAudioProcessor::createEditor()

{
        return new LAUTEQAudioProcessorEditor (*this);
    //return new juce::GenericAudioProcessorEditor(*this);
}

//==============================================================================
void LAUTEQAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
    juce::MemoryOutputStream mos(destData, true);
    apvts.state.writeToStream(mos);
}

void LAUTEQAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
    
    auto tree = juce::ValueTree::readFromData(data, sizeInBytes);
    if( tree.isValid() )
    {
        apvts.replaceState(tree);
        updateFilters();
    }
}

ChainSettings getChainSettings(juce::AudioProcessorValueTreeState& apvts)
{
    ChainSettings settings;
    
    // points to the parameter id and change values in processor
    settings.lowCutFreq = apvts.getRawParameterValue("LowCut Freq")->load();
    settings.highCutFreq = apvts.getRawParameterValue("HighCut Freq")->load();
    settings.peakFreq = apvts.getRawParameterValue("Peak Freq")->load();
    settings.peakGainInDecibels = apvts.getRawParameterValue("Peak Gain")->load();
    settings.peakQuality = apvts.getRawParameterValue("Peak Quality")->load();
    settings.lowCutSlope = static_cast<Slope>(apvts.getRawParameterValue("LowCut Slope")->load());
    settings.highCutSlope = static_cast<Slope>(apvts.getRawParameterValue("HighCut Slope")->load());
    
    return settings;
}

// PEAK processor change

void LAUTEQAudioProcessor::updatePeakFilter(const ChainSettings &chainSettings)
{
    // define peak filter
    auto peakCoefficients = juce::dsp::IIR::Coefficients<float>::makePeakFilter(getSampleRate(),                        // sample rate
                                                                                chainSettings.peakFreq,                 // get settings from apvts string fader
                                                                                chainSettings.peakQuality,              // get settings from apvts string peak q
                                                                                juce::Decibels::decibelsToGain(chainSettings.peakGainInDecibels)); // range
    
    updateCoefficients(leftChain.template get<ChainPositions::Peak>().coefficients, peakCoefficients);
    updateCoefficients(rightChain.template get<ChainPositions::Peak>().coefficients, peakCoefficients);
}

void LAUTEQAudioProcessor::updateCoefficients(Coefficients& old, const Coefficients& replacements)
    {
        *old = *replacements;
    }

//LOWCUT processor change

void LAUTEQAudioProcessor::updateLowCutFilters(const ChainSettings &chainSettings)
{
    
    auto cutCoefficients = juce::dsp::FilterDesign<float>::designIIRHighpassHighOrderButterworthMethod(chainSettings.lowCutFreq, getSampleRate(), 2 * (chainSettings.lowCutSlope +1));
    
    auto& leftLowCut = leftChain.get<ChainPositions::LowCut>();
    auto& rightLowCut = rightChain.get<ChainPositions::LowCut>();
    
    updateCutFilter(leftLowCut, cutCoefficients, chainSettings. lowCutSlope);
    updateCutFilter(rightLowCut, cutCoefficients, chainSettings.lowCutSlope);

    
}


// HIGHCUT processor change

    void LAUTEQAudioProcessor::updateHighCutFilters(const ChainSettings &chainSettings)
{
    
    auto highCutCoefficients = juce::dsp::FilterDesign<float>::designIIRLowpassHighOrderButterworthMethod(chainSettings.highCutFreq,
                                                                                                          getSampleRate(),
                                                                                                          2 * (chainSettings.highCutSlope +1));
    
    auto& leftHighCut = leftChain.get<ChainPositions::HighCut>();
    auto& rightHighCut = rightChain.get<ChainPositions::HighCut>();
    
    updateCutFilter(leftHighCut, highCutCoefficients, chainSettings.highCutSlope);
    updateCutFilter(rightHighCut, highCutCoefficients, chainSettings.highCutSlope);
    
}


// Update Filters if used

void LAUTEQAudioProcessor::updateFilters()
{
    auto chainSettings = getChainSettings(apvts);
    
    updateLowCutFilters(chainSettings);
    updatePeakFilter(chainSettings);
    updateHighCutFilters(chainSettings);
}


// Define Parameter and layout

    juce::AudioProcessorValueTreeState::ParameterLayout LAUTEQAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    
    layout.add(std::make_unique<juce::AudioParameterFloat>("LowCut Freq",
                                                           "LowCut Freq",
                                                           juce::NormalisableRange<float>(20.f,20000.f, 1.f, 0.25f),
                                                           20.f));
    
    layout.add(std::make_unique<juce::AudioParameterFloat>("HighCut Freq",
                                                           "HighCut Freq",
                                                           juce::NormalisableRange<float>(20.f,20000.f, 1.f, 0.25f),
                                                           20000.f));  
    
    layout.add(std::make_unique<juce::AudioParameterFloat>("Peak Freq",
                                                           "Peak Freq",
                                                           juce::NormalisableRange<float>(20.f,20000.f, 1.f, 0.25f),
                                                           750.f));
    
    layout.add(std::make_unique<juce::AudioParameterFloat>("Peak Gain",
                                                           "Peak Gain",
                                                           juce::NormalisableRange<float>(-24.f, 24.f, 0.5f, 1.f),
                                                           0.0f));
    
    layout.add(std::make_unique<juce::AudioParameterFloat>("Peak Quality",
                                                           "Peak Quality",
                                                           juce::NormalisableRange<float>(0.1f, 10.f, 0.05f, 1.f),
                                                           1.f));
    
    
    
    juce::StringArray stringArray;
    for( int i = 0; i < 4; ++i )
    {
        juce::String str;
        str << (12 + i*12);
        str << " db/Oct";
        stringArray.add(str);
    
    }

    layout.add(std::make_unique<juce::AudioParameterChoice>("LowCut Slope", "LowCut Slope", stringArray, 0 ));
    layout.add(std::make_unique<juce::AudioParameterChoice>("HighCut Slope", "HighCut Slope", stringArray, 0 ));
    
    
    
    //juce::StringArray distArray;
    
    
    //distArray.addItem("Hard Clip", 1);
    //distArray.addItem("Soft Clip", 2);
    //distArray.addItem("Half-Wave Rect", 3);
    
    //layout.add(std::make_unique<juce::AudioParameterChoice>("Distortion", "Distortion", distArray, 0));
    
    
    
    
    return layout;
}


//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new LAUTEQAudioProcessor();
}
