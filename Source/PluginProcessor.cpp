#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    inline float shapeSample (int type, float x)
    {
        switch (type)
        {
            case 1:  return juce::jlimit (-1.0f, 1.0f, x);                     // hard clip
            case 2:  { float t = std::fmod (x + 1.0f, 4.0f); if (t < 0.0f) t += 4.0f;   // wavefolder
                       return (t < 2.0f ? t : 4.0f - t) - 1.0f; }
            default: return std::tanh (x);
        }
    }

    constexpr double barsForSyncMode[] = { 0.0, 0.5, 1.0, 2.0, 4.0, 8.0 };

    juce::NormalisableRange<float> logRange (float lo, float hi)
    {
        return { lo, hi,
                 [] (float s, float e, float v) { return s * std::pow (e / s, v); },
                 [] (float s, float e, float v) { return std::log (v / s) / std::log (e / s); },
                 [] (float s, float e, float v) { return juce::jlimit (s, e, v); } };
    }
}

//==============================================================================
AeroSeedAudioProcessor::AeroSeedAudioProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Parameters", createParameters())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout AeroSeedAudioProcessor::createParameters()
{
    using PID = juce::ParameterID;
    using F = juce::AudioParameterFloat;  using C = juce::AudioParameterChoice;  using B = juce::AudioParameterBool;
    juce::AudioProcessorValueTreeState::ParameterLayout l;

    // Synchronisation
    l.add (std::make_unique<C> (PID { "SYNC_MODE", 1 }, "Sync Mode", juce::StringArray { "Continuous", "1/2 Bar", "1 Bar", "2 Bars", "4 Bars", "8 Bars" }, 0));
    l.add (std::make_unique<F> (PID { "TRIG_START", 1 }, "Trigger Start", 0.0f, 1.0f, 0.0f));
    l.add (std::make_unique<F> (PID { "TRIG_END", 1 },   "Trigger End",   0.0f, 1.0f, 1.0f));
    l.add (std::make_unique<F> (PID { "MIX", 1 },        "Dry/Wet",       0.0f, 1.0f, 1.0f));

    // Harmonique (facon Chroma)
    juce::StringArray scales { juce::String (L"Chromatique"), "Majeur", "Mineur", "Dorien", "Phrygien", "Lydien", "Mixolydien",
                               "Pentatonique maj.", "Pentatonique min.", "Mineur harmonique", "Blues", "Gamme par tons",
                               juce::String (L"Personnalis\u00e9e") };
    l.add (std::make_unique<B> (PID { "H_ON", 1 }, "Harmonic On", true));
    l.add (std::make_unique<C> (PID { "H_KEY", 1 }, "Key", juce::StringArray { "C","C#","D","D#","E","F","F#","G","G#","A","A#","B" }, 0));
    l.add (std::make_unique<C> (PID { "H_SCALE", 1 }, "Scale", scales, 1));
    l.add (std::make_unique<juce::AudioParameterInt> (PID { "H_MASK", 1 }, "Custom Notes", 0, 4095, (int) aerodsp::maskFor (0, 1)));
    l.add (std::make_unique<F> (PID { "H_MORPH", 1 }, "Spectral Morph", juce::NormalisableRange<float> (0.0f, 100.0f, 1.0f), 0.0f));
    l.add (std::make_unique<F> (PID { "H_GATE", 1 },  "Spectral Gate",  juce::NormalisableRange<float> (-80.0f, 0.0f, 1.0f), -80.0f));

    // Filtre
    l.add (std::make_unique<B> (PID { "F_ON", 1 }, "Filter On", true));
    l.add (std::make_unique<C> (PID { "FILTER_TYPE", 1 }, "Filter Type", juce::StringArray { "Low-pass", "High-pass", "Band-pass" }, 0));
    l.add (std::make_unique<C> (PID { "FILTER_ORDER", 1 }, "Filter Order",
                                juce::StringArray { "Avant la distortion", juce::String (L"Apr\u00e8s la distortion") }, 0));
    l.add (std::make_unique<F> (PID { "CUTOFF", 1 }, "Cutoff", logRange (20.0f, 20000.0f), 20000.0f));
    l.add (std::make_unique<F> (PID { "RESO", 1 }, "Resonance", juce::NormalisableRange<float> (0.5f, 10.0f, 0.01f), 0.71f));

    // Distortion
    l.add (std::make_unique<B> (PID { "D_ON", 1 }, "Distortion On", true));
    l.add (std::make_unique<C> (PID { "DIST_TYPE", 1 }, "Distortion Type", juce::StringArray { "tanh", "hard clip", "wavefolder" }, 0));
    l.add (std::make_unique<F> (PID { "DRIVE", 1 }, "Drive", juce::NormalisableRange<float> (1.0f, 10.0f, 0.01f), 1.0f));

    // Bitcrusher (+ multibande)
    l.add (std::make_unique<B> (PID { "CRUSH_ON", 1 }, "Bitcrusher On", false));
    l.add (std::make_unique<F> (PID { "BITS", 1 }, "Bits", juce::NormalisableRange<float> (2.0f, 16.0f, 1.0f), 8.0f));
    l.add (std::make_unique<B> (PID { "MB_ON", 1 }, "Multiband", false));
    l.add (std::make_unique<F> (PID { "B1", 1 }, "Low Bits",  juce::NormalisableRange<float> (2.0f, 16.0f, 1.0f), 8.0f));
    l.add (std::make_unique<F> (PID { "B2", 1 }, "Mid Bits",  juce::NormalisableRange<float> (2.0f, 16.0f, 1.0f), 8.0f));
    l.add (std::make_unique<F> (PID { "B3", 1 }, "High Bits", juce::NormalisableRange<float> (2.0f, 16.0f, 1.0f), 8.0f));
    l.add (std::make_unique<F> (PID { "X1", 1 }, "Crossover 1", logRange (40.0f, 2000.0f), 200.0f));
    l.add (std::make_unique<F> (PID { "X2", 1 }, "Crossover 2", logRange (1000.0f, 18000.0f), 2500.0f));

    // Chorus
    l.add (std::make_unique<B> (PID { "C_ON", 1 }, "Chorus On", true));
    l.add (std::make_unique<F> (PID { "CH_RATE", 1 },  "Chorus Rate",  juce::NormalisableRange<float> (0.1f, 10.0f, 0.01f), 1.2f));
    l.add (std::make_unique<F> (PID { "CH_DEPTH", 1 }, "Chorus Depth", juce::NormalisableRange<float> (0.0f, 100.0f, 1.0f), 40.0f));
    l.add (std::make_unique<F> (PID { "CH_MIX", 1 },   "Chorus Mix",   juce::NormalisableRange<float> (0.0f, 100.0f, 1.0f), 0.0f));
    return l;
}

bool AeroSeedAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo()) return false;
    return out == layouts.getMainInputChannelSet();
}

//==============================================================================
void AeroSeedAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    preparedChannels  = juce::jlimit (1, 2, getTotalNumOutputChannels());

    const int fftOrder = sampleRate > 60000.0 ? 12 : 11;   // ~ meme resolution en Hz a 88.2/96 kHz
    for (auto& h : harmonizer) h.prepare (sampleRate, fftOrder);
    latency = harmonizer[0].latency();
    setLatencySamples (latency);                           // FL Studio compense automatiquement

    for (auto& d : dryDelay) d.assign ((size_t) latency, 0.0f);
    dryDelayPos = 0;
    for (auto& x : crossover) x.prepare (sampleRate);

    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) samplesPerBlock, (juce::uint32) preparedChannels };
    filter.prepare (spec);  filter.reset();
    chorus.prepare (spec);  chorus.reset();  chorus.setCentreDelay (7.0f);  chorus.setFeedback (0.0f);

    wetBuffer.setSize (preparedChannels, samplesPerBlock);
    mono.assign ((size_t) samplesPerBlock, 0.0f);

    cutoffSmooth.reset (sampleRate, 0.05);  driveSmooth.reset (sampleRate, 0.05);  effectRamp.reset (sampleRate, 0.01);
    cutoffSmooth.setCurrentAndTargetValue (juce::jlimit (20.0f, (float) (0.45 * sampleRate), value ("CUTOFF")));
    driveSmooth.setCurrentAndTargetValue (value ("DRIVE"));
    effectRamp.setCurrentAndTargetValue (1.0f);
}

uint16_t AeroSeedAudioProcessor::getEffectiveMask() const
{
    const int scale = intValue ("H_SCALE");
    return scale == aerodsp::kCustomScale ? (uint16_t) intValue ("H_MASK") : aerodsp::maskFor (intValue ("H_KEY"), scale);
}

//==============================================================================
void AeroSeedAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int n = buffer.getNumSamples();
    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch) buffer.clear (ch, 0, n);
    const int numCh = juce::jmin (buffer.getNumChannels(), preparedChannels, 2);
    if (numCh <= 0 || n <= 0) return;

    if (wetBuffer.getNumSamples() < n) wetBuffer.setSize (preparedChannels, n, false, false, true);   // rare : bloc plus grand que prevu
    if ((int) mono.size() < n) mono.resize ((size_t) n);

    auto pushMono = [&] (const juce::AudioBuffer<float>& b, AnalyserFifo& fifo)
    {
        for (int i = 0; i < n; ++i)
        {
            float s = 0.0f;
            for (int ch = 0; ch < numCh; ++ch) s += b.getSample (ch, i);
            mono[(size_t) i] = s / (float) numCh;
        }
        fifo.push (mono.data(), n);
    };
    pushMono (buffer, inputFifo);   // spectre "entree" (l'instrument)

    //------------------------------------------------------------------ parametres (lus une fois par bloc)
    const int   syncMode  = juce::jlimit (0, 5, intValue ("SYNC_MODE"));
    const float trigStart = value ("TRIG_START"), trigEnd = value ("TRIG_END"), globalMix = value ("MIX");

    const bool hOn = value ("H_ON") > 0.5f;
    const uint16_t mask = getEffectiveMask();
    for (auto& h : harmonizer) h.setParameters (mask, hOn ? value ("H_MORPH") * 0.01f : 0.0f, value ("H_GATE"));

    const bool fOn = value ("F_ON") > 0.5f, filterFirst = intValue ("FILTER_ORDER") == 0;
    using FT = juce::dsp::StateVariableTPTFilterType;
    switch (intValue ("FILTER_TYPE")) { case 1: filter.setType (FT::highpass); break; case 2: filter.setType (FT::bandpass); break; default: filter.setType (FT::lowpass); }
    filter.setResonance (value ("RESO"));
    cutoffSmooth.setTargetValue (juce::jlimit (20.0f, (float) (0.45 * currentSampleRate), value ("CUTOFF")));
    filter.setCutoffFrequency (cutoffSmooth.skip (n));

    const bool dOn = value ("D_ON") > 0.5f;
    const int  distType = intValue ("DIST_TYPE");
    driveSmooth.setTargetValue (value ("DRIVE"));

    const bool crushOn = value ("CRUSH_ON") > 0.5f, mbOn = value ("MB_ON") > 0.5f;
    const float levels  = aerodsp::crushLevels (value ("BITS"));
    const float bandLv[3] = { aerodsp::crushLevels (value ("B1")), aerodsp::crushLevels (value ("B2")), aerodsp::crushLevels (value ("B3")) };
    if (crushOn && mbOn) for (auto& x : crossover) x.setFrequencies (value ("X1"), value ("X2"));

    const bool  cOn = value ("C_ON") > 0.5f;
    const float chMix = value ("CH_MIX") * 0.01f;
    chorus.setRate (value ("CH_RATE"));  chorus.setDepth (value ("CH_DEPTH") * 0.01f);  chorus.setMix (chMix);

    //------------------------------------------------------------------ chaine "wet" (echantillon par echantillon)
    float* wet[2] = { nullptr, nullptr };
    float* io[2]  = { nullptr, nullptr };
    for (int ch = 0; ch < numCh; ++ch) { wet[ch] = wetBuffer.getWritePointer (ch); io[ch] = buffer.getWritePointer (ch); }

    for (int i = 0; i < n; ++i)
    {
        const float drive = driveSmooth.getNextValue(), comp = 1.0f / std::sqrt (drive);

        for (int ch = 0; ch < numCh; ++ch)
        {
            const float x = io[ch][i];

            // le signal sec est retarde de la meme latence que le traitement spectral (dry/wet aligne)
            float& slot = dryDelay[ch][(size_t) dryDelayPos];
            io[ch][i] = slot;
            slot = x;

            float y = harmonizer[ch].process (x);
            if (fOn && filterFirst) y = filter.processSample (ch, y);
            if (dOn)                y = shapeSample (distType, y * drive) * comp;
            if (crushOn)
            {
                if (mbOn)
                {
                    float lo, mid, hi;
                    crossover[ch].process (y, lo, mid, hi);
                    y = aerodsp::crushSample (lo, bandLv[0]) + aerodsp::crushSample (mid, bandLv[1]) + aerodsp::crushSample (hi, bandLv[2]);
                }
                else y = aerodsp::crushSample (y, levels);
            }
            if (fOn && ! filterFirst) y = filter.processSample (ch, y);
            wet[ch][i] = y;
        }
        dryDelayPos = (dryDelayPos + 1) % latency;
    }

    if (cOn && chMix > 0.001f)
    {
        juce::dsp::AudioBlock<float> block (wetBuffer);
        auto sub = block.getSubBlock (0, (size_t) n).getSubsetChannelBlock (0, (size_t) numCh);
        chorus.process (juce::dsp::ProcessContextReplacing<float> (sub));
    }

    //------------------------------------------------------------------ synchro DAW + fenetre d'activation
    bool syncActive = false;
    double ppqStart = 0.0, ppqPerSample = 0.0, cycleBeats = 4.0;
    if (syncMode > 0)
        if (auto* ph = getPlayHead())
            if (auto pos = ph->getPosition())
                if (pos->getIsPlaying())
                    if (auto ppq = pos->getPpqPosition())
                    {
                        double bpm = 120.0;
                        if (auto b = pos->getBpm()) bpm = *b;
                        int num = 4, den = 4;
                        if (auto ts = pos->getTimeSignature()) { num = ts->numerator; den = ts->denominator; }
                        cycleBeats   = barsForSyncMode[syncMode] * juce::jmax (1, num) * 4.0 / juce::jmax (1, den);
                        ppqPerSample = bpm / 60.0 / currentSampleRate;
                        ppqStart     = *ppq - latency * ppqPerSample;   // la sortie est en retard de 'latency' echantillons
                        syncActive   = cycleBeats > 0.0;
                    }

    auto phaseAt = [cycleBeats] (double ppq) { double p = std::fmod (ppq, cycleBeats) / cycleBeats; return p < 0.0 ? p + 1.0 : p; };
    auto inside  = [trigStart, trigEnd] (double p)
    {
        return trigStart <= trigEnd ? (p >= trigStart && p < trigEnd) : (p >= trigStart || p < trigEnd);
    };

    if (syncActive) cyclePhase.store ((float) phaseAt (ppqStart));
    else            { cyclePhase.store (-1.0f); effectRamp.setTargetValue (1.0f); }

    for (int i = 0; i < n; ++i)
    {
        if (syncActive) effectRamp.setTargetValue (inside (phaseAt (ppqStart + i * ppqPerSample)) ? 1.0f : 0.0f);
        const float m = globalMix * effectRamp.getNextValue();
        for (int ch = 0; ch < numCh; ++ch) io[ch][i] = io[ch][i] * (1.0f - m) + wet[ch][i] * m;
    }

    pushMono (buffer, outputFifo);   // spectre "sortie"
}

//==============================================================================
// SEED
uint64_t AeroSeedAudioProcessor::computeSeedFromImage (const juce::Image& img)
{
    constexpr int grid = 16;
    uint64_t hash = 14695981039346656037ULL;
    const int w = img.getWidth(), h = img.getHeight();
    if (w <= 0 || h <= 0) return hash;

    juce::Image::BitmapData bmp (img, juce::Image::BitmapData::readOnly);
    for (int gy = 0; gy < grid; ++gy)
        for (int gx = 0; gx < grid; ++gx)
        {
            int x1 = (int) (((juce::int64) (gx + 1) * w) / grid), y1 = (int) (((juce::int64) (gy + 1) * h) / grid);
            int x0 = (int) (((juce::int64) gx * w) / grid),       y0 = (int) (((juce::int64) gy * h) / grid);
            x1 = juce::jmin (juce::jmax (x1, x0 + 1), w);  y1 = juce::jmin (juce::jmax (y1, y0 + 1), h);
            x0 = juce::jmin (x0, x1 - 1);                  y0 = juce::jmin (y0, y1 - 1);

            uint64_t r = 0, g = 0, b = 0, c = 0;
            for (int y = y0; y < y1; ++y)
                for (int x = x0; x < x1; ++x) { const auto px = bmp.getPixelColour (x, y); r += px.getRed(); g += px.getGreen(); b += px.getBlue(); ++c; }

            for (uint64_t v : { (r / c) >> 4, (g / c) >> 4, (b / c) >> 4 }) { hash ^= v; hash *= 1099511628211ULL; }
        }
    return hash;
}

void AeroSeedAudioProcessor::setParameterReal (const char* id, float realValue)
{
    if (auto* p = apvts.getParameter (id))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (realValue));
        p->endChangeGesture();
    }
}

void AeroSeedAudioProcessor::applyPatchToParameters (const aerodsp::Patch& p)
{
    for (auto* id : { "H_ON", "F_ON", "D_ON", "C_ON" }) setParameterReal (id, 1.0f);
    setParameterReal ("H_KEY",   (float) p.key);
    setParameterReal ("H_SCALE", (float) p.scale);
    setParameterReal ("H_MASK",  (float) aerodsp::maskFor (p.key, p.scale));
    setParameterReal ("H_MORPH", (float) p.morph);
    setParameterReal ("H_GATE",  (float) p.gateDb);
    setParameterReal ("FILTER_TYPE",  (float) p.filterType);
    setParameterReal ("FILTER_ORDER", (float) p.filterOrder);
    setParameterReal ("CUTOFF", (float) p.cutoff);
    setParameterReal ("RESO",   (float) p.resonance);
    setParameterReal ("DIST_TYPE", (float) p.distType);
    setParameterReal ("DRIVE",     (float) p.drive);
    setParameterReal ("CRUSH_ON", p.crushOn ? 1.0f : 0.0f);
    setParameterReal ("BITS",     (float) p.bits);
    setParameterReal ("MB_ON",    p.multiband ? 1.0f : 0.0f);
    setParameterReal ("B1", (float) p.bandBits[0]);  setParameterReal ("B2", (float) p.bandBits[1]);  setParameterReal ("B3", (float) p.bandBits[2]);
    setParameterReal ("X1", (float) p.xover1);       setParameterReal ("X2", (float) p.xover2);
    setParameterReal ("CH_RATE",  (float) p.chorusRate);
    setParameterReal ("CH_DEPTH", (float) p.chorusDepth);
    setParameterReal ("CH_MIX",   (float) p.chorusMix);
}

bool AeroSeedAudioProcessor::isEditedFromSeed() const
{
    if (! seedValid.load()) return false;
    const auto p = aerodsp::makePatch (currentSeed.load());
    auto diff = [] (float a, double b) { return std::abs (a - (float) b) > 1.0e-3f * juce::jmax (1.0f, (float) std::abs (b)); };

    return value ("H_ON") < 0.5f || value ("F_ON") < 0.5f || value ("D_ON") < 0.5f || value ("C_ON") < 0.5f
        || intValue ("H_KEY") != p.key || getEffectiveMask() != aerodsp::maskFor (p.key, p.scale)
        || diff (value ("H_MORPH"), p.morph) || diff (value ("H_GATE"), p.gateDb)
        || intValue ("FILTER_TYPE") != p.filterType || intValue ("FILTER_ORDER") != p.filterOrder
        || diff (value ("CUTOFF"), p.cutoff) || diff (value ("RESO"), p.resonance)
        || intValue ("DIST_TYPE") != p.distType || diff (value ("DRIVE"), p.drive)
        || (value ("CRUSH_ON") > 0.5f) != p.crushOn || diff (value ("BITS"), p.bits)
        || (value ("MB_ON") > 0.5f) != p.multiband || diff (value ("B1"), p.bandBits[0]) || diff (value ("B2"), p.bandBits[1])
        || diff (value ("B3"), p.bandBits[2]) || diff (value ("X1"), p.xover1) || diff (value ("X2"), p.xover2)
        || diff (value ("CH_RATE"), p.chorusRate) || diff (value ("CH_DEPTH"), p.chorusDepth) || diff (value ("CH_MIX"), p.chorusMix);
}

void AeroSeedAudioProcessor::generatePatchFromImage (const juce::Image& img)
{
    if (! img.isValid()) return;

    const float scale = juce::jmin (1.0f, 640.0f / (float) juce::jmax (img.getWidth(), img.getHeight()));
    auto thumb = img.rescaled (juce::jmax (1, juce::roundToInt (img.getWidth() * scale)),
                               juce::jmax (1, juce::roundToInt (img.getHeight() * scale)), juce::Graphics::highResamplingQuality);
    { const juce::ScopedLock sl (thumbLock); thumbnail = thumb; }
    ++thumbVersion;

    const auto seed = computeSeedFromImage (img);
    currentSeed.store (seed);
    seedValid.store (true);
    applyPatchToParameters (aerodsp::makePatch (seed));
}

void AeroSeedAudioProcessor::resetToSeed()
{
    if (seedValid.load()) applyPatchToParameters (aerodsp::makePatch (currentSeed.load()));
}

void AeroSeedAudioProcessor::toggleNote (int pc)
{
    const int key = intValue ("H_KEY");
    const uint16_t m = (uint16_t) (getEffectiveMask() ^ (1u << (pc % 12)));
    setParameterReal ("H_MASK", (float) m);
    setParameterReal ("H_SCALE", (float) aerodsp::scaleForMask (key, m));
}

juce::Image AeroSeedAudioProcessor::getThumbnail() const
{
    const juce::ScopedLock sl (thumbLock);
    return thumbnail;
}

//==============================================================================
// Sauvegarde du projet : parametres + seed + miniature (JPEG) + fond d'ecran choisi
void AeroSeedAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    if (seedValid.load()) state.setProperty ("seed", juce::String::toHexString ((juce::int64) currentSeed.load()), nullptr);

    {
        const juce::ScopedLock sl (thumbLock);
        if (thumbnail.isValid())
        {
            juce::MemoryOutputStream mos;
            juce::JPEGImageFormat jpg;
            jpg.setQuality (0.85f);
            if (jpg.writeImageToStream (thumbnail, mos))
                state.setProperty ("thumb", juce::Base64::toBase64 (mos.getData(), mos.getDataSize()), nullptr);
        }
    }

    if (auto xml = state.createXml()) copyXmlToBinary (*xml, destData);
}

void AeroSeedAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType())) return;

    auto vt = juce::ValueTree::fromXml (*xml);
    const auto seedText = vt.getProperty ("seed").toString(), thumbText = vt.getProperty ("thumb").toString();
    apvts.replaceState (vt);

    if (thumbText.isNotEmpty())
    {
        juce::MemoryOutputStream decoded;
        if (juce::Base64::convertFromBase64 (decoded, thumbText))
        {
            auto img = juce::ImageFileFormat::loadFrom (decoded.getData(), decoded.getDataSize());
            if (img.isValid()) { { const juce::ScopedLock sl (thumbLock); thumbnail = img; } ++thumbVersion; }
        }
    }

    if (seedText.isNotEmpty()) { currentSeed.store ((uint64_t) seedText.getHexValue64()); seedValid.store (true); }
}

juce::AudioProcessorEditor* AeroSeedAudioProcessor::createEditor() { return new AeroSeedAudioProcessorEditor (*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()          { return new AeroSeedAudioProcessor(); }
