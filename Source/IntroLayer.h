#pragma once
#include <JuceHeader.h>
#include "AeroScene.h"

namespace aero
{
//==============================================================================
// Carte de choix (ecran noir de demarrage)
class ChoiceCard : public juce::Component
{
public:
    ChoiceCard (bool imageCard, juce::String t, juce::String d) : isImage (imageCard), title (std::move (t)), desc (std::move (d))
    {
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }

    std::function<void()> onClick;

    void paint (juce::Graphics& g) override
    {
        const auto r = getLocalBounds().toFloat().reduced (2.0f);
        if (hover)
        {
            g.setColour (juce::Colour (0x335cc8ff));
            g.fillRoundedRectangle (r.expanded (1.0f), 28.0f);
        }
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.09f), 0.0f, r.getY(), juce::Colours::white.withAlpha (0.02f), 0.0f, r.getBottom(), false));
        g.fillRoundedRectangle (r, 28.0f);
        g.setColour (hover ? juce::Colour (0xff5cc8ff) : juce::Colours::white.withAlpha (0.2f));
        g.drawRoundedRectangle (r, 28.0f, 1.5f);

        // icone (degrade aqua -> vert)
        const auto ic = juce::Rectangle<float> (96.0f, 96.0f).withCentre ({ r.getCentreX(), r.getY() + 92.0f });
        juce::Path p;
        if (isImage)
        {
            p.addRoundedRectangle (ic.getX() + 12, ic.getY() + 16, 72, 60, 10);
            p.addEllipse (ic.getX() + 27, ic.getY() + 31, 14, 14);
            p.startNewSubPath (ic.getX() + 14, ic.getY() + 68); p.lineTo (ic.getX() + 36, ic.getY() + 46);
            p.lineTo (ic.getX() + 52, ic.getY() + 62); p.lineTo (ic.getX() + 64, ic.getY() + 50); p.lineTo (ic.getX() + 82, ic.getY() + 68);
        }
        else
            for (int i = 0; i < 3; ++i) { const float y = ic.getY() + 28 + i * 20.0f; p.startNewSubPath (ic.getX() + 16, y); p.lineTo (ic.getX() + 80, y); }
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff5cc8ff), ic.getX(), ic.getY(), juce::Colour (0xff7ed957), ic.getRight(), ic.getBottom(), false));
        g.strokePath (p, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        if (! isImage)
            static const std::pair<float, float> knobs[] = { { 34.0f, 28.0f }, { 62.0f, 48.0f }, { 44.0f, 68.0f } };
            for (auto k : knobs)
            {
                const auto knob = juce::Rectangle<float> (18.0f, 18.0f).withCentre ({ ic.getX() + k.first, ic.getY() + k.second });
                g.setColour (juce::Colours::black);  g.fillEllipse (knob);
                g.setGradientFill (juce::ColourGradient (juce::Colour (0xff5cc8ff), knob.getX(), knob.getY(), juce::Colour (0xff7ed957), knob.getRight(), knob.getBottom(), false));
                g.drawEllipse (knob.reduced (2.0f), 4.0f);
            }

        g.setColour (juce::Colours::white);
        g.setFont (uiFont (30.0f, juce::Font::bold));
        g.drawText (title, r.toNearestInt().withY ((int) r.getY() + 160).withHeight (38), juce::Justification::centred);
        g.setColour (juce::Colours::white.withAlpha (0.65f));
        g.setFont (uiFont (17.0f));
        g.drawFittedText (desc, r.toNearestInt().reduced (40, 0).withY ((int) r.getY() + 208).withHeight (60), juce::Justification::centredTop, 3);
    }

    void mouseEnter (const juce::MouseEvent&) override { hover = true;  repaint(); }
    void mouseExit (const juce::MouseEvent&) override  { hover = false; repaint(); }
    void mouseUp (const juce::MouseEvent& e) override  { if (e.mouseWasClicked() && onClick) onClick(); }

private:
    bool isImage, hover = false;
    juce::String title, desc;
};

//==============================================================================
// Couche d'introduction : noir + halo (intro), cartes de choix, puis vague d'ouverture qui revele l'interface
class IntroLayer : public juce::Component
{
public:
    enum Stage { Intro, Choose, Opening, Done };

    explicit IntroLayer (OpeningAnimation& a)
        : imageCard (true, "Glisse une image", fr (L"La seed de l'image g\u00e9n\u00e8re ton effet : toujours le m\u00eame son pour la m\u00eame image.")),
          manualCard (false, "Effets manuels", fr (L"R\u00e8gle toi-m\u00eame l'harmonique, le filtre, la distortion, le bitcrusher et le chorus.")),
          anim (a)
    {
        addChildComponent (imageCard);
        addChildComponent (manualCard);
    }

    ChoiceCard imageCard, manualCard;   // publics : l'editeur branche leurs actions

    void setStage (Stage s)
    {
        stage = s;
        imageCard.setVisible (s == Choose);
        manualCard.setVisible (s == Choose);
        setInterceptsMouseClicks (s == Intro || s == Choose, true);
        setVisible (s != Done);
        repaint();
    }
    Stage getStage() const { return stage; }

    void setTimes (float introT, float openT, float cardsT)
    {
        introTime = introT;  openTime = openT;
        const float a = juce::jlimit (0.0f, 1.0f, cardsT / 0.7f);
        for (auto* c : { &imageCard, &manualCard })
        {
            c->setAlpha (a);
            c->setTransform (juce::AffineTransform::translation (0.0f, (1.0f - a) * 26.0f));
        }
        repaint();
    }

    void resized() override
    {
        imageCard.setBounds (190, 215, 420, 330);
        manualCard.setBounds (670, 215, 420, 330);
    }

    void paint (juce::Graphics& g) override
    {
        if (stage == Intro || stage == Choose)
        {
            g.fillAll (juce::Colours::black);
            const float glow = stage == Intro ? juce::jlimit (0.0f, 1.0f, 2.2f - introTime) : 0.0f;   // halo dore pendant l'intro
            if (glow > 0.0f)
            {
                g.setGradientFill (juce::ColourGradient (juce::Colour::fromFloatRGBA (1.0f, 0.77f, 0.24f, 0.32f * glow), 640.0f, 340.0f,
                                                         juce::Colour::fromFloatRGBA (1.0f, 0.77f, 0.24f, 0.0f), 640.0f + 400.0f, 340.0f, true));
                g.fillAll();
            }
        }
        else if (stage == Opening)
        {
            g.setColour (juce::Colours::black);
            g.fillPath (OpeningAnimation::veil (openTime));   // zone pas encore atteinte par la vague
            anim.paint (g, openTime);
        }
    }

private:
    OpeningAnimation& anim;
    Stage stage = Intro;
    float introTime = 0.0f, openTime = 0.0f;
};

} // namespace aero
