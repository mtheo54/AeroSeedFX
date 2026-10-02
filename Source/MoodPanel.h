#pragma once
#include <JuceHeader.h>
#include "AeroUI.h"

namespace aero
{
//==============================================================================
// Petit lien souligne ("Un exemple")
class LinkLabel : public juce::Component
{
public:
    explicit LinkLabel (juce::String t) : text (std::move (t)) { setMouseCursor (juce::MouseCursor::PointingHandCursor); }
    std::function<void()> onClick;

    void paint (juce::Graphics& g) override
    {
        g.setFont (uiFont (12.0f));
        g.setColour (juce::Colours::white.withAlpha (hover ? 1.0f : 0.72f));
        g.drawText (text, getLocalBounds(), juce::Justification::centredLeft);
        const int w = juce::jmin (getWidth(), g.getCurrentFont().getStringWidth (text));
        g.fillRect (0, getHeight() - 3, w, 1);
    }
    void mouseEnter (const juce::MouseEvent&) override { hover = true;  repaint(); }
    void mouseExit (const juce::MouseEvent&) override  { hover = false; repaint(); }
    void mouseUp (const juce::MouseEvent& e) override  { if (e.mouseWasClicked() && onClick) onClick(); }

private:
    juce::String text;
    bool hover = false;
};

//==============================================================================
// Panneau "Mood" : mesures de l'image (6 axes) + mood tape par l'utilisateur + AERO.
// Il s'ouvre dans le grand ecran, a gauche, et repousse le spectre (l'editeur gere le glissement).
class MoodPanel : public juce::Component, private juce::Timer
{
public:
    explicit MoodPanel (AeroSeedAudioProcessor& p) : proc (p)
    {
        input.setFont (uiFont (14.0f));
        input.setIndents (14, 6);
        input.setJustification (juce::Justification::centredLeft);
        input.setColour (juce::TextEditor::textColourId, juce::Colours::white);
        input.setColour (juce::TextEditor::highlightColourId, juce::Colour (0x665cc8ff));
        input.setColour (juce::CaretComponent::caretColourId, juce::Colours::white);
        input.setTextToShowWhenEmpty (fr (L"sombre, planant, m\u00e9tallique..."), juce::Colours::white.withAlpha (0.6f));
        input.setText (proc.getMoodText(), juce::dontSendNotification);
        input.onTextChange = [this] { pendingSince = juce::Time::getMillisecondCounterHiRes(); };
        input.onReturnKey  = [this] { commitText(); };
        addAndMakeVisible (input);

        aero.setButtonText ("AERO");
        aero.onClick = [this] { commitText(); proc.exploreAero(); flash = 1.0f; };
        addAndMakeVisible (aero);

        example.onClick = [this]
        {
            input.setText (aeromood::example (juce::Random::getSystemRandom().nextInt (aeromood::kNumExamples)), juce::dontSendNotification);
            commitText();
        };
        addAndMakeVisible (example);

        startTimerHz (15);
    }

    void resized() override
    {
        const int w = getWidth();
        input.setBounds (18, 176, w - 18 - 18 - 80, 30);
        aero.setBounds (w - 18 - 76, 174, 76, 34);
        example.setBounds (18, 232, 120, 18);
    }

    void paint (juce::Graphics& g) override
    {
        const auto r = getLocalBounds().toFloat().reduced (1.0f);
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.3f), 0.0f, r.getY(), juce::Colours::white.withAlpha (0.1f), 0.0f, r.getBottom(), false));
        g.fillRoundedRectangle (r, 24.0f);
        g.setColour (juce::Colours::white.withAlpha (0.7f));
        g.drawRoundedRectangle (r.reduced (0.75f), 24.0f, 1.5f);

        const int w = getWidth();
        g.setFont (uiFont (12.0f, juce::Font::bold));
        shadowText (g, "MOOD DE L'IMAGE", { 18, 12, w - 36, 18 }, juce::Justification::centredLeft, juce::Colours::white, true);

        aerodsp::MoodAxes img;
        const bool hasImg = proc.getImageMood (img);
        if (! hasImg)
        {
            g.setFont (uiFont (12.0f, juce::Font::italic));
            shadowText (g, fr (L"D\u00e9pose une image pour l'analyser"), { 18, 31, w - 36, 16 }, juce::Justification::centredLeft, juce::Colours::white.withAlpha (0.75f), true);
        }

        // 6 axes bipolaires : le point glisse vers le pole dominant
        g.setFont (uiFont (11.0f));
        for (int a = 0; a < aerodsp::numMoodAxes; ++a)
        {
            const int y = 52 + a * 15;
            shadowText (g, aeromood::axisName (a, false), { 18, y, 60, 13 }, juce::Justification::centredRight, juce::Colours::white.withAlpha (0.92f), true);
            shadowText (g, aeromood::axisName (a, true),  { w - 78, y, 60, 13 }, juce::Justification::centredLeft, juce::Colours::white.withAlpha (0.92f), true);
            const auto track = juce::Rectangle<float> (84.0f, y + 4.5f, (float) w - 168.0f, 4.0f);
            g.setColour (juce::Colours::white.withAlpha (0.25f));  g.fillRoundedRectangle (track, 2.0f);
            const float x = track.getX() + track.getWidth() * (float) ((img.v[a] + 1.0) * 0.5);
            const auto dot = juce::Rectangle<float> (10.0f, 10.0f).withCentre ({ x, track.getCentreY() });
            g.setColour (juce::Colour (0xa6ffc43c));  g.fillEllipse (dot.expanded (2.0f));
            g.setColour (juce::Colours::white);       g.fillEllipse (dot);
        }

        g.setColour (juce::Colours::white.withAlpha (0.3f));
        g.fillRect (18, 148, w - 36, 1);
        g.setFont (uiFont (12.0f, juce::Font::bold));
        shadowText (g, fr (L"MOOD PERSONNALIS\u00c9"), { 18, 154, w - 36, 18 }, juce::Justification::centredLeft, juce::Colours::white, true);

        // mots reconnus (bulles) ; le fond clignote un instant apres AERO
        const auto tagsArea = juce::Rectangle<int> (18, 212, w - 36, 18);
        if (flash > 0.01f) { g.setColour (juce::Colour (0x597c4dff).withMultipliedAlpha (flash)); g.fillRoundedRectangle (tagsArea.toFloat().expanded (4.0f, 2.0f), 8.0f); }
        const auto words = proc.getMatchedMoodWords();
        g.setFont (uiFont (12.0f));
        if (words.isEmpty())
        {
            const auto msg = input.isEmpty() ? fr (L"Les mots reconnus apparaissent ici") : fr (L"aucun mot reconnu");
            shadowText (g, msg, tagsArea, juce::Justification::centredLeft, juce::Colours::white.withAlpha (0.68f), true);
        }
        else
        {
            float x = (float) tagsArea.getX();
            for (const auto& word : words)
            {
                const float tw = (float) g.getCurrentFont().getStringWidth (word) + 16.0f;
                if (x + tw > (float) tagsArea.getRight()) break;
                const auto pill = juce::Rectangle<float> (x, (float) tagsArea.getY(), tw, (float) tagsArea.getHeight());
                g.setColour (juce::Colours::white.withAlpha (0.22f));  g.fillRoundedRectangle (pill, 9.0f);
                g.setColour (juce::Colours::white.withAlpha (0.45f));  g.drawRoundedRectangle (pill, 9.0f, 1.0f);
                g.setColour (juce::Colours::white);                    g.drawText (word, pill.toNearestInt(), juce::Justification::centred);
                x += tw + 5.0f;
            }
        }
    }

private:
    void commitText()
    {
        pendingSince = 0.0;
        proc.setMoodText (input.getText());
        repaint();
    }

    void timerCallback() override
    {
        // saisie : on attend 180 ms sans frappe avant d'appliquer (comme la maquette)
        if (pendingSince > 0.0 && juce::Time::getMillisecondCounterHiRes() - pendingSince > 180.0) commitText();
        if (! isShowing()) return;   // panneau ferme : rien a redessiner

        aerodsp::MoodAxes m;
        const bool has = proc.getImageMood (m);
        const auto words = proc.getMatchedMoodWords().joinIntoString (" ");
        const juce::String sig = juce::String ((int) has) + words + juce::String (m.v[0], 3) + juce::String (m.v[1], 3) + juce::String (m.v[5], 3);
        if (flash > 0.01f) { flash *= 0.85f; repaint(); }
        if (sig != lastSig) { lastSig = sig; repaint(); }
    }

    AeroSeedAudioProcessor& proc;
    juce::TextEditor input;
    juce::TextButton aero;
    LinkLabel example { "Un exemple" };
    double pendingSince = 0.0;
    float flash = 0.0f;
    juce::String lastSig;
};

} // namespace aero
