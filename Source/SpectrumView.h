#pragma once
#include <JuceHeader.h>
#include "AeroUI.h"

namespace aero
{
//==============================================================================
// Moteur d'analyse (thread interface) : lit les FIFO du processeur, calcule spectres, tendance et tonalite.
// Une seule instance, partagee par les deux vues (ecran principal + editeur).
class AnalyzerEngine
{
public:
    using Bins = std::array<float, aerodsp::kDisplayBins>;

    void update (AeroSeedAudioProcessor& p, float dt)
    {
        const double sr = p.getSampleRate() > 0 ? p.getSampleRate() : 44100.0;
        if (sr != lastSr) { inA.prepare (sr); outA.prepare (sr); lastSr = sr; }

        // echantillons recus depuis la derniere image ; sans signal (transport arrete), on injecte du silence
        const int expected = juce::jlimit (64, (int) scratch.size(), (int) (sr * dt));
        feed (p.inputFifo, inA, expected);
        feed (p.outputFifo, outA, expected);

        inA.compute (rawIn);   outA.compute (rawOut);
        for (int i = 0; i < aerodsp::kDisplayBins; ++i)
        {
            const float a = rawOut[(size_t) i], b = rawIn[(size_t) i];
            wet[(size_t) i] += (a - wet[(size_t) i]) * (a > wet[(size_t) i] ? 0.7f : 0.22f);
            dry[(size_t) i] += (b - dry[(size_t) i]) * (b > dry[(size_t) i] ? 0.7f : 0.22f);
            trendRaw[(size_t) i] += (wet[(size_t) i] - trendRaw[(size_t) i]) * 0.012f;
        }
        aerodsp::smoothTrend (trendRaw, trend);
        slope = aerodsp::trendSlopeDbPerOctave (trend);

        keyIn.addSpectrum (inA.getMagnitudes(), inA.getBinHz(), 0.993f);
        keyOut.addSpectrum (outA.getMagnitudes(), outA.getBinHz(), 0.993f);
        resIn = keyIn.estimate();  resOut = keyOut.estimate();
    }

    Bins wet, dry, trend;
    float slope = 0.0f;
    aerodsp::KeyDetector keyIn, keyOut;
    aerodsp::KeyDetector::Result resIn, resOut;

    AnalyzerEngine() { wet.fill (-120.0f); dry.fill (-120.0f); trendRaw.fill (-90.0f); trend.fill (-90.0f); rawIn.fill (-120.0f); rawOut.fill (-120.0f); }

private:
    void feed (AnalyserFifo& fifo, aerodsp::SpectrumAnalyser& a, int expected)
    {
        int total = 0, got;
        while ((got = fifo.pull (scratch.data(), (int) scratch.size())) > 0) { a.push (scratch.data(), got); total += got; }
        if (total == 0) { std::fill (scratch.begin(), scratch.begin() + expected, 0.0f); a.push (scratch.data(), expected); }
    }

    aerodsp::SpectrumAnalyser inA, outA;
    Bins rawIn, rawOut, trendRaw;
    std::array<float, 8192> scratch {};
    double lastSr = 0.0;
};

//==============================================================================
// Spectre en LECTURE SEULE (aucun reglage) : style magma, image de la seed en fond, tendance, tonalite, infobulle.
class SpectrumView : public juce::Component
{
public:
    SpectrumView (AeroSeedAudioProcessor& p, AnalyzerEngine& e) : proc (p), engine (e) {}

    bool imageOnly = false;          // bascule Image / Spectre
    juce::Image background;          // image de la seed

    void mouseMove (const juce::MouseEvent& e) override { hover = e.position; repaint(); }
    void mouseExit (const juce::MouseEvent&) override   { hover = { -1.0f, -1.0f }; repaint(); }

    void paint (juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat();
        juce::Path clip; clip.addRoundedRectangle (bounds, 22.0f);
        g.saveState();
        g.reduceClipRegion (clip);

        // fond : image de la seed (assombrie) ou verre fume
        if (background.isValid())
            g.drawImage (background, bounds, imageOnly ? juce::RectanglePlacement::centred : juce::RectanglePlacement::fillDestination);
        g.setGradientFill (juce::ColourGradient (juce::Colour::fromFloatRGBA (0.02f, 0.06f, 0.16f, imageOnly ? 0.0f : 0.30f), 0.0f, 0.0f,
                                                 juce::Colour::fromFloatRGBA (0.02f, 0.06f, 0.16f, imageOnly ? 0.0f : 0.55f), 0.0f, bounds.getBottom(), false));
        g.fillRect (bounds);

        if (! imageOnly) paintSpectrum (g, bounds);

        if (! proc.hasSeed())
        {
            auto pill = juce::Rectangle<float> (380.0f, 34.0f).withCentre ({ bounds.getCentreX(), bounds.getBottom() - 30.0f });
            paintGlass (g, pill, 17.0f);
            g.setFont (uiFont (15.0f, juce::Font::bold));
            shadowText (g, fr (L"Glisse une image ici pour g\u00e9n\u00e9rer l'effet"), pill.toNearestInt(), juce::Justification::centred, juce::Colours::white, true);
        }
        g.restoreState();
        g.setColour (juce::Colours::white.withAlpha (0.85f));
        g.drawRoundedRectangle (bounds.reduced (1.0f), 22.0f, 2.0f);
    }

private:
    void paintSpectrum (juce::Graphics& g, juce::Rectangle<float> b)
    {
        const float W = b.getWidth(), H = b.getHeight(), top = 34.0f, ph = H - top;
        auto X = [&] (double f) { return (float) (std::log (f / aerodsp::kMinHz) / std::log (aerodsp::kMaxHz / aerodsp::kMinHz)) * W; };
        auto Y = [&] (float db) { return top + ph * juce::jlimit (0.0f, 1.0f, -db / 72.0f); };
        auto at = [] (const AnalyzerEngine::Bins& a, float x)
        {
            const int i = juce::jlimit (0, aerodsp::kDisplayBins - 1, (int) x); const int j = juce::jmin (aerodsp::kDisplayBins - 1, i + 1);
            const float u = x - i; return a[(size_t) i] * (1 - u) + a[(size_t) j] * u;
        };
        auto binX = [&] (float x) { return x / W * (aerodsp::kDisplayBins - 1); };

        // grille fine (decades)
        g.setColour (juce::Colour::fromFloatRGBA (0.75f, 0.8f, 0.94f, 0.2f));
        for (double dec : { 10.0, 100.0, 1000.0, 10000.0 })
            for (int k = 1; k < 10; ++k)
            {
                const double f = dec * k;
                if (f >= aerodsp::kMinHz && f <= aerodsp::kMaxHz) g.fillRect (std::round (X (f)), 0.0f, 1.0f, H);
            }

        // notes de la gamme (si la section Harmonique agit)
        if (proc.value ("H_ON") > 0.5f && proc.value ("H_MORPH") > 0.0f)
        {
            const uint16_t mask = proc.getEffectiveMask();
            const int key = proc.intValue ("H_KEY");
            for (int m = 15; m < 140; ++m)
            {
                const double f = aerodsp::noteToFreq (m);
                if (f < aerodsp::kMinHz || f > aerodsp::kMaxHz || ! ((mask >> (m % 12)) & 1)) continue;
                g.setColour (juce::Colour::fromFloatRGBA (1.0f, 0.84f, 0.47f, m % 12 == key ? 0.38f : 0.12f));
                g.fillRect (X (f), top, 1.0f, ph);
            }
        }

        // remplissage magma + bandes de couleur selon le niveau
        juce::Path area; area.startNewSubPath (0.0f, H);
        for (float x = 0; x <= W; x += 1.0f) area.lineTo (x, Y (at (engine.wet, binX (x))));
        area.lineTo (W, H); area.closeSubPath();
        juce::ColourGradient fg (juce::Colour (0xd90d0326), 0.0f, H, juce::Colour (0xd9f0583c), 0.0f, top, false);
        fg.addColour (0.45, juce::Colour (0xd94a0f6e));  fg.addColour (0.75, juce::Colour (0xd9b02f8e));
        g.setGradientFill (fg);  g.fillPath (area);
        for (float x = 0; x < W; x += 2.0f)
        {
            const float v = at (engine.wet, binX (x)), y = Y (v);
            g.setColour (magma ((v + 72.0f) / 72.0f).withAlpha (0.42f));
            g.fillRect (x, y, 2.0f, H - y);
        }

        // courbes : sortie (claire), entree (fine), tendance (pointilles cyan)
        auto curve = [&] (const AnalyzerEngine::Bins& a)
        {
            juce::Path p;
            for (float x = 0; x <= W; x += 1.0f) { const float y = Y (at (a, binX (x))); if (x == 0) p.startNewSubPath (x, y); else p.lineTo (x, y); }
            return p;
        };
        g.setColour (juce::Colour::fromFloatRGBA (0.8f, 0.73f, 0.96f, 0.5f));  g.strokePath (curve (engine.dry), juce::PathStrokeType (1.5f));
        g.setColour (juce::Colour (0xffbccaf5));                               g.strokePath (curve (engine.wet), juce::PathStrokeType (2.0f));
        {
            juce::Path dashed;
            const float dashes[] = { 9.0f, 6.0f };
            juce::PathStrokeType (2.6f).createDashedStroke (dashed, curve (engine.trend), dashes, 2);
            g.setColour (juce::Colour (0x557ff3ff));  g.strokePath (curve (engine.trend), juce::PathStrokeType (6.0f));   // halo
            g.setColour (juce::Colour (0xff7ff3ff));  g.fillPath (dashed);
        }

        // reperes de frequence
        g.setFont (monoFont (15.0f));
        g.setColour (juce::Colour::fromFloatRGBA (0.7f, 0.75f, 0.9f, 0.65f));
        static const std::pair<const char*, double> labels[] = { { "100Hz", 100.0 }, { "1kHz", 1000.0 }, { "10kHz", 10000.0 } };
        for (auto& l : labels)
            g.drawText (l.first, (int) X (l.second) + 7, 6, 80, 20, juce::Justification::centredLeft);

        g.setColour (juce::Colour (0xff7ff3ff));  g.setFont (monoFont (14.0f));
        g.drawText ("TENDANCE  " + juce::String (engine.slope >= 0 ? "+" : "") + juce::String (engine.slope, 1) + " dB/oct",
                    (int) W - 330, (int) H - 30, 316, 20, juce::Justification::centredRight);

        paintKeyBox (g, W);
        if (hover.x >= 0.0f) paintTooltip (g, W, H, X, Y, at, binX);
    }

    void paintKeyBox (juce::Graphics& g, float W)
    {
        static const char* names[] = { "C","C#","D","D#","E","F","F#","G","G#","A","A#","B" };
        auto keyName = [] (const aerodsp::KeyDetector::Result& r)
        {
            if (r.confidence < 0.15f) return juce::String ("--");   // pas assez de notes (silence, bruit)
            return juce::String (names[r.key]) + (r.minor ? " mineur" : " majeur");
        };
        const auto box = juce::Rectangle<float> (W - 276.0f, 10.0f, 264.0f, 116.0f);
        g.setColour (juce::Colour::fromFloatRGBA (0.03f, 0.03f, 0.07f, 0.74f));  g.fillRoundedRectangle (box, 12.0f);
        g.setColour (juce::Colours::white.withAlpha (0.2f));                     g.drawRoundedRectangle (box, 12.0f, 1.0f);
        const int x = (int) box.getX(), y = (int) box.getY(), w = (int) box.getWidth();
        g.setFont (monoFont (12.0f));  g.setColour (juce::Colour::fromFloatRGBA (0.7f, 0.75f, 0.9f, 0.8f));
        g.drawText (fr (L"ENTR\u00c9E"), x + 14, y + 12, 70, 18, juce::Justification::centredLeft);
        g.drawText ("SORTIE", x + 14, y + 36, 70, 18, juce::Justification::centredLeft);
        g.drawText (juce::String (juce::roundToInt (engine.resIn.confidence * 100)) + "%",  x + w - 64, y + 12, 50, 18, juce::Justification::centredRight);
        g.drawText (juce::String (juce::roundToInt (engine.resOut.confidence * 100)) + "%", x + w - 64, y + 36, 50, 18, juce::Justification::centredRight);
        g.setFont (monoFont (17.0f, juce::Font::bold));
        g.setColour (juce::Colours::white);       g.drawText (keyName (engine.resIn),  x + 82, y + 11, 130, 20, juce::Justification::centredLeft);
        g.setColour (juce::Colour (0xffffb36b));  g.drawText (keyName (engine.resOut), x + 82, y + 35, 130, 20, juce::Justification::centredLeft);

        const auto& c = engine.keyOut.getChroma();
        float mx = 1.0e-9f; for (float v : c) mx = juce::jmax (mx, v);
        g.setFont (monoFont (10.0f));
        for (int i = 0; i < 12; ++i)
        {
            const float h = juce::jmax (2.0f, c[(size_t) i] / mx * 28.0f), bx = box.getX() + 16.0f + i * 20.0f;
            g.setColour (i == engine.resOut.key ? juce::Colour (0xffffb36b) : magma (0.3f + 0.7f * c[(size_t) i] / mx).withAlpha (0.9f));
            g.fillRect (bx, box.getY() + 94.0f - h, 14.0f, h);
            g.setColour (juce::Colour::fromFloatRGBA (0.7f, 0.75f, 0.9f, 0.7f));
            g.drawText (names[i], (int) bx - 3, y + 98, 20, 12, juce::Justification::centred);
        }
    }

    template <typename XF, typename YF, typename AT, typename BX>
    void paintTooltip (juce::Graphics& g, float W, float H, XF, YF Y, AT at, BX binX)
    {
        static const char* names[] = { "C","C#","D","D#","E","F","F#","G","G#","A","A#","B" };
        const double f = aerodsp::kMinHz * std::pow (aerodsp::kMaxHz / aerodsp::kMinHz, hover.x / W);
        const float db = at (engine.wet, binX (hover.x));
        const double m = aerodsp::freqToNote (f);
        const int nn = (int) std::lround (m), ct = (int) std::lround ((m - nn) * 100.0);
        const auto txt = juce::String (db, 2) + "dB | " + juce::String (f, 2) + "Hz | " + names[((nn % 12) + 12) % 12]
                         + juce::String (nn / 12 - 1) + (ct >= 0 ? " + " : " - ") + juce::String (std::abs (ct)) + " Cents";
        g.setColour (juce::Colours::white.withAlpha (0.35f));  g.fillRect (hover.x, 0.0f, 1.0f, H);
        g.setColour (juce::Colours::white);                    g.fillEllipse (hover.x - 4.0f, Y (db) - 4.0f, 8.0f, 8.0f);
        g.setFont (monoFont (16.0f, juce::Font::bold));
        const float tw = (float) g.getCurrentFont().getStringWidth (txt) + 20.0f;
        const float tx = juce::jlimit (6.0f, W - tw - 6.0f, hover.x + 14.0f), ty = juce::jlimit (34.0f, H - 34.0f, hover.y - 36.0f);
        g.setColour (juce::Colour (0xe61e1e24));  g.fillRect (tx, ty, tw, 30.0f);
        g.setColour (juce::Colour (0xfff3f4ff));  g.drawText (txt, (int) tx + 10, (int) ty, (int) tw - 10, 30, juce::Justification::centredLeft);
    }

    static juce::Colour magma (float t)
    {
        static const float s[5][4] = { { 0.0f, 14, 5, 38 }, { 0.3f, 78, 14, 110 }, { 0.55f, 184, 48, 140 }, { 0.8f, 236, 72, 60 }, { 1.0f, 255, 150, 62 } };
        t = juce::jlimit (0.0f, 1.0f, t);
        for (int i = 1; i < 5; ++i)
            if (t <= s[i][0])
            {
                const float u = (t - s[i - 1][0]) / (s[i][0] - s[i - 1][0]);
                return juce::Colour ((juce::uint8) (s[i - 1][1] + (s[i][1] - s[i - 1][1]) * u), (juce::uint8) (s[i - 1][2] + (s[i][2] - s[i - 1][2]) * u),
                                     (juce::uint8) (s[i - 1][3] + (s[i][3] - s[i - 1][3]) * u));
            }
        return juce::Colour (255, 150, 62);
    }

    AeroSeedAudioProcessor& proc;
    AnalyzerEngine& engine;
    juce::Point<float> hover { -1.0f, -1.0f };
};

} // namespace aero
