#pragma once
#include <JuceHeader.h>
#include "AeroDSP.h"

//==============================================================================
// FIFO sans verrou audio -> interface (un producteur : thread audio, un consommateur : interface)
class AnalyserFifo
{
public:
    void push (const float* src, int num) noexcept
    {
        const auto scope = fifo.write (juce::jmin (num, fifo.getFreeSpace()));   // si l'interface est en retard, on perd des echantillons (sans gravite)
        if (scope.blockSize1 > 0) std::copy (src, src + scope.blockSize1, data.begin() + scope.startIndex1);
        if (scope.blockSize2 > 0) std::copy (src + scope.blockSize1, src + scope.blockSize1 + scope.blockSize2, data.begin() + scope.startIndex2);
    }

    int pull (float* dest, int maxNum) noexcept
    {
        const auto scope = fifo.read (juce::jmin (maxNum, fifo.getNumReady()));
        if (scope.blockSize1 > 0) std::copy (data.begin() + scope.startIndex1, data.begin() + scope.startIndex1 + scope.blockSize1, dest);
        if (scope.blockSize2 > 0) std::copy (data.begin() + scope.startIndex2, data.begin() + scope.startIndex2 + scope.blockSize2, dest + scope.blockSize1);
        return scope.blockSize1 + scope.blockSize2;
    }

private:
    static constexpr int capacity = 32768;
    juce::AbstractFifo fifo { capacity };
    std::array<float, capacity> data {};
};

//==============================================================================
// AeroSeed FX : plugin d'EFFET uniquement (il traite le son d'un instrument existant, il n'en genere pas).
class AeroSeedAudioProcessor : public juce::AudioProcessor
{
public:
    AeroSeedAudioProcessor();
    ~AeroSeedAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "AeroSeed FX"; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==============================================================================
    // Thread interface uniquement
    void generatePatchFromImage (const juce::Image& img);
    void resetToSeed();
    void toggleNote (int pitchClass);                 // clavier de la section Harmonique
    void setParameterReal (const char* id, float realValue);

    static uint64_t computeSeedFromImage (const juce::Image& img);

    bool        hasSeed() const               { return seedValid.load(); }
    uint64_t    getSeed() const               { return currentSeed.load(); }
    aerodsp::Patch getSeedPatch() const       { return seedValid.load() ? aerodsp::makePatch (currentSeed.load()) : aerodsp::Patch{}; }
    bool        isEditedFromSeed() const;
    juce::Image getThumbnail() const;
    int         getThumbnailVersion() const   { return thumbVersion.load(); }
    float       getCyclePhase() const         { return cyclePhase.load(); }
    float       value (const char* id) const  { return apvts.getRawParameterValue (id)->load(); }
    int         intValue (const char* id) const { return juce::roundToInt (value (id)); }
    uint16_t    getEffectiveMask() const;

    bool introAlreadyShown = false;   // l'ecran de choix n'apparait qu'une fois par instance

    AnalyserFifo inputFifo, outputFifo;
    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameters();
    void applyPatchToParameters (const aerodsp::Patch& p);

    std::atomic<bool>     seedValid { false };
    std::atomic<uint64_t> currentSeed { 0 };

    // ---- audio
    double currentSampleRate = 44100.0;
    int    preparedChannels = 2;
    int    latency = 2048;

    aerodsp::SpectralHarmonizer harmonizer[2];
    aerodsp::Crossover3         crossover[2];
    std::vector<float>          dryDelay[2];
    int                         dryDelayPos = 0;

    juce::dsp::StateVariableTPTFilter<float> filter;
    juce::dsp::Chorus<float>                 chorus;
    juce::AudioBuffer<float>                 wetBuffer;
    std::vector<float>                       mono;

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> cutoffSmooth;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear>         driveSmooth, effectRamp;

    // ---- miniature de l'image (affichage + sauvegarde du projet)
    mutable juce::CriticalSection thumbLock;
    juce::Image                   thumbnail;
    std::atomic<int>              thumbVersion { 0 };
    std::atomic<float>            cyclePhase { -1.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AeroSeedAudioProcessor)
};
