#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

// Boite a outils graphique d'AeroSeed FX (style Frutiger Aero / liquid glass)
namespace aero
{
inline juce::String fr (const wchar_t* s) { return juce::String (s); }   // texte accentue

//==============================================================================
// POLICES : Segoe UI (police d'origine de Windows Vista, presente sur tous les PC Windows),
// sinon une police proche ; chiffres de l'analyseur en police a chasse fixe.
inline juce::String pickFont (std::initializer_list<const char*> wanted, const juce::String& fallback)
{
    static const auto names = juce::Font::findAllTypefaceNames();
    for (auto* w : wanted) if (names.contains (w)) return w;
    return fallback;
}
inline juce::Font uiFont (float h, int style = juce::Font::plain)
{
    static const juce::String name = pickFont ({ "Segoe UI", "Hind", "Frutiger", "Myriad Pro", "Lucida Grande", "Helvetica Neue" },
                                               juce::Font::getDefaultSansSerifFontName());
    return juce::Font (name, h, style);
}
inline juce::Font monoFont (float h, int style = juce::Font::plain)
{
    static const juce::String name = pickFont ({ "JetBrains Mono", "Consolas", "Menlo", "DejaVu Sans Mono" },
                                               juce::Font::getDefaultMonospacedFontName());
    return juce::Font (name, h, style);
}

//==============================================================================
// VERRE LIQUIDE : remplissage translucide, reflet speculaire, bord lumineux, ombre douce
inline void paintGlass (juce::Graphics& g, juce::Rectangle<float> r, float radius,
                        bool active = false, bool hover = false, bool down = false,
                        juce::Colour accent = juce::Colour (0xff3aa0e8))
{
    if (down) r = r.reduced (1.5f);
    else if (hover) r = r.expanded (1.0f);

    // ombre
    for (int i = 3; i >= 1; --i)
    {
        g.setColour (juce::Colour (0x0d00284f).withMultipliedAlpha ((float) i));
        g.fillRoundedRectangle (r.translated (0.0f, 2.0f + i * 1.5f).expanded ((float) i), radius + i);
    }
    // corps
    juce::ColourGradient body (juce::Colours::white.withAlpha (0.40f), 0.0f, r.getY(), juce::Colours::white.withAlpha (0.10f), 0.0f, r.getBottom(), false);
    body.addColour (0.55, juce::Colours::white.withAlpha (0.07f));
    g.setGradientFill (body);
    g.fillRoundedRectangle (r, radius);
    if (active)
    {
        juce::ColourGradient tint (accent.brighter (0.5f).withAlpha (0.78f), 0.0f, r.getY(), accent.withAlpha (0.45f), 0.0f, r.getBottom(), false);
        g.setGradientFill (tint);
        g.fillRoundedRectangle (r, radius);
    }
    if (hover) { g.setColour (juce::Colours::white.withAlpha (0.10f)); g.fillRoundedRectangle (r, radius); }

    // reflet speculaire (moitie haute)
    auto top = r.reduced (r.getWidth() * 0.06f, 2.0f).withHeight (r.getHeight() * 0.45f);
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.62f), 0.0f, top.getY(), juce::Colours::white.withAlpha (0.0f), 0.0f, top.getBottom(), false));
    g.fillRoundedRectangle (top, juce::jmin (radius, top.getHeight() * 0.5f));

    // bord lumineux en degrade (fort en haut a gauche, plus doux en bas a droite)
    juce::ColourGradient rim (juce::Colours::white.withAlpha (0.98f), r.getX(), r.getY(), juce::Colours::white.withAlpha (0.55f), r.getRight(), r.getBottom(), false);
    rim.addColour (0.4, juce::Colours::white.withAlpha (0.15f));
    g.setGradientFill (rim);
    g.drawRoundedRectangle (r.reduced (0.75f), radius, 1.5f);
}

// Texte avec ombre (lisible sur tous les fonds)
inline void shadowText (juce::Graphics& g, const juce::String& t, juce::Rectangle<int> r, juce::Justification j, juce::Colour c, bool shadow)
{
    if (shadow) { g.setColour (juce::Colours::black.withAlpha (0.5f)); g.drawText (t, r.translated (1, 1), j); }
    g.setColour (c);
    g.drawText (t, r, j);
}

//==============================================================================
class AeroLookAndFeel : public juce::LookAndFeel_V4
{
public:
    AeroLookAndFeel()
    {
        setColour (juce::ComboBox::textColourId, juce::Colour (0xff0b2a44));
        setColour (juce::ComboBox::arrowColourId, juce::Colour (0xff1565c0));
        setColour (juce::Label::textColourId, juce::Colours::white);
        setColour (juce::TextButton::textColourOffId, juce::Colours::white);
        setColour (juce::TextButton::textColourOnId, juce::Colours::white);
        setColour (juce::ToggleButton::textColourId, juce::Colours::white);
        setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xf2eaf6ff));
        setColour (juce::PopupMenu::textColourId, juce::Colour (0xff0b2a44));
        setColour (juce::PopupMenu::highlightedBackgroundColourId, juce::Colour (0xff5cc8ff));
        setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
        setColour (juce::TextEditor::textColourId, juce::Colour (0xff0b2a44));
        setColour (juce::TextEditor::backgroundColourId, juce::Colours::white);
        setColour (juce::TextEditor::highlightColourId, juce::Colour (0x665cc8ff));
        setColour (juce::CaretComponent::caretColourId, juce::Colour (0xff1565c0));
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down) override
    {
        const auto r = b.getLocalBounds().toFloat().reduced (3.0f);
        paintGlass (g, r, r.getHeight() * 0.5f, b.getToggleState(), over, down);
    }

    juce::Font getTextButtonFont (juce::TextButton&, int h) override { return uiFont (juce::jmin (17.0f, h * 0.48f), juce::Font::bold); }

    void drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool) override
    {
        g.setFont (getTextButtonFont (b, b.getHeight()));
        shadowText (g, b.getButtonText(), b.getLocalBounds(), juce::Justification::centred, juce::Colours::white, true);
    }

    // interrupteur "iOS" en verre
    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool over, bool) override
    {
        const bool on = b.getToggleState();
        auto pill = juce::Rectangle<float> (2.0f, (b.getHeight() - 28.0f) * 0.5f, 50.0f, 28.0f);
        g.setColour (on ? juce::Colour (0xff27c160) : juce::Colours::white.withAlpha (0.22f));
        g.fillRoundedRectangle (pill, 14.0f);
        if (on) { g.setGradientFill (juce::ColourGradient (juce::Colour (0xff6fe99a), 0.0f, pill.getY(), juce::Colour (0xff1fb85a), 0.0f, pill.getBottom(), false)); g.fillRoundedRectangle (pill, 14.0f); }
        g.setColour (juce::Colours::black.withAlpha (0.25f));
        g.drawRoundedRectangle (pill.reduced (0.5f), 14.0f, 1.0f);
        auto knob = juce::Rectangle<float> (22.0f, 22.0f).withCentre ({ on ? pill.getRight() - 14.0f : pill.getX() + 14.0f, pill.getCentreY() });
        g.setColour (juce::Colours::black.withAlpha (0.3f));
        g.fillEllipse (knob.translated (0.0f, 1.5f));
        g.setGradientFill (juce::ColourGradient (juce::Colours::white, knob.getX(), knob.getY(), juce::Colour (0xffd7efff), knob.getRight(), knob.getBottom(), false));
        g.fillEllipse (over ? knob.expanded (1.0f) : knob);
        if (b.getButtonText().isNotEmpty())
        {
            g.setFont (uiFont (16.0f));
            shadowText (g, b.getButtonText(), b.getLocalBounds().withTrimmedLeft (62), juce::Justification::centredLeft, juce::Colours::white, true);
        }
    }

    void drawComboBox (juce::Graphics& g, int w, int h, bool down, int, int, int, int, juce::ComboBox& box) override
    {
        const auto r = juce::Rectangle<float> (0.0f, 0.0f, (float) w, (float) h).reduced (2.0f);
        paintGlass (g, r, juce::jmin (12.0f, r.getHeight() * 0.5f), false, box.isMouseOver (true), down);
        juce::Path chevron;
        const float cx = (float) w - 20.0f, cy = h * 0.5f;
        chevron.startNewSubPath (cx - 5.0f, cy - 2.5f); chevron.lineTo (cx, cy + 2.5f); chevron.lineTo (cx + 5.0f, cy - 2.5f);
        g.setColour (box.findColour (juce::ComboBox::arrowColourId));
        g.strokePath (chevron, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    juce::Font getComboBoxFont (juce::ComboBox&) override       { return uiFont (16.0f, juce::Font::bold); }
    juce::Font getPopupMenuFont() override                       { return uiFont (16.0f); }
    void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override
    {
        label.setBounds (10, 1, box.getWidth() - 34, box.getHeight() - 2);
        label.setFont (getComboBoxFont (box));
    }

    // bouton rotatif : bille de verre
    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider&) override
    {
        const auto b = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (6.0f);
        const float d = juce::jmin (b.getWidth(), b.getHeight());
        const auto c = b.getCentre();
        const auto r = juce::Rectangle<float> (d, d).withCentre (c);
        g.setColour (juce::Colour (0x5500326e));
        g.fillEllipse (r.translated (0.0f, 6.0f).expanded (2.0f));
        juce::ColourGradient body (juce::Colours::white, r.getX() + d * 0.3f, r.getY() + d * 0.22f, juce::Colour (0xff1e6ec8), r.getRight(), r.getBottom(), true);
        body.addColour (0.38, juce::Colour (0xffbee6ff));
        body.addColour (0.70, juce::Colour (0xff46a0e6));
        g.setGradientFill (body);
        g.fillEllipse (r);
        g.setColour (juce::Colours::white.withAlpha (0.75f));
        g.drawEllipse (r.reduced (0.75f), 1.5f);
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.7f), 0.0f, r.getY(), juce::Colours::white.withAlpha (0.0f), 0.0f, r.getCentreY(), false));
        g.fillEllipse (r.reduced (d * 0.14f, d * 0.06f).withHeight (d * 0.42f));
        juce::Path p;
        p.addRoundedRectangle (-3.0f, -d * 0.5f + 9.0f, 6.0f, d * 0.32f, 3.0f);
        g.setColour (juce::Colour (0xff0d47a1));
        g.fillPath (p, juce::AffineTransform::rotation (a0 + pos * (a1 - a0)).translated (c));
    }
};

//==============================================================================
// Barre de la fenetre d'activation (2 poignees liees aux parametres, automatisables)
class TriggerBar : public juce::Component
{
public:
    TriggerBar (juce::RangedAudioParameter& s, juce::RangedAudioParameter& e)
        : startAtt (s, [this] (float v) { startValue = v; repaint(); }), endAtt (e, [this] (float v) { endValue = v; repaint(); })
    {
        startAtt.sendInitialUpdate();
        endAtt.sendInitialUpdate();
    }

    void setPlayhead (float p) { if (std::abs (p - playhead) > 1.0e-4f) { playhead = p; repaint(); } }
    void enablementChanged() override { repaint(); }

    void paint (juce::Graphics& g) override
    {
        const float a = isEnabled() ? 1.0f : 0.45f;
        const auto b = getLocalBounds().toFloat().reduced (1.0f);
        const auto t = track();
        g.setColour (juce::Colours::white.withAlpha (0.3f * a)); g.fillRoundedRectangle (b, 14.0f);
        g.setColour (juce::Colours::white.withAlpha (0.9f * a)); g.drawRoundedRectangle (b, 14.0f, 1.5f);
        g.setColour (juce::Colours::black.withAlpha (0.12f * a));
        for (int i = 1; i < 4; ++i) g.fillRect (t.getX() + t.getWidth() * i * 0.25f - 0.5f, t.getY(), 1.0f, t.getHeight());

        const float xs = t.getX() + startValue * t.getWidth(), xe = t.getX() + endValue * t.getWidth();
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff5cc8ff).withAlpha (0.9f * a), 0.0f, t.getY(), juce::Colour (0xff1e88e5).withAlpha (0.9f * a), 0.0f, t.getBottom(), false));
        if (startValue <= endValue) g.fillRoundedRectangle (xs, t.getY(), juce::jmax (0.0f, xe - xs), t.getHeight(), 8.0f);
        else { g.fillRoundedRectangle (xs, t.getY(), t.getRight() - xs, t.getHeight(), 8.0f); g.fillRoundedRectangle (t.getX(), t.getY(), xe - t.getX(), t.getHeight(), 8.0f); }
        g.setColour (juce::Colours::white.withAlpha (0.25f * a));
        g.fillRoundedRectangle (t.getX(), t.getY(), t.getWidth(), t.getHeight() * 0.45f, 8.0f);

        for (float x : { xs, xe })
        {
            auto h = juce::Rectangle<float> (x - 6.0f, b.getY() + 2.0f, 12.0f, b.getHeight() - 4.0f);
            g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (a), 0.0f, h.getY(), juce::Colour (0xffcfe9ff).withAlpha (a), 0.0f, h.getBottom(), false));
            g.fillRoundedRectangle (h, 6.0f);
            g.setColour (juce::Colour (0xff1565c0).withAlpha (0.8f * a)); g.drawRoundedRectangle (h, 6.0f, 1.2f);
        }
        if (playhead >= 0.0f && isEnabled())
        {
            g.setColour (juce::Colour (0xffff8f00));
            g.fillRect (t.getX() + playhead * t.getWidth() - 1.5f, b.getY() + 1.0f, 3.0f, b.getHeight() - 2.0f);
        }
        g.setFont (uiFont (15.0f, juce::Font::bold));
        g.setColour (juce::Colours::black.withAlpha (0.55f * a));
        g.drawText (juce::String (juce::roundToInt (startValue * 100)) + "%  -  " + juce::String (juce::roundToInt (endValue * 100)) + "%",
                    getLocalBounds(), juce::Justification::centred);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        const float v = toValue (e.position.x);
        dragged = startValue == endValue ? (v < startValue ? &startAtt : &endAtt)
                                         : (std::abs (v - startValue) <= std::abs (v - endValue) ? &startAtt : &endAtt);
        dragged->beginGesture();
        dragged->setValueAsPartOfGesture (v);
    }
    void mouseDrag (const juce::MouseEvent& e) override { if (dragged) dragged->setValueAsPartOfGesture (toValue (e.position.x)); }
    void mouseUp (const juce::MouseEvent&) override     { if (dragged) { dragged->endGesture(); dragged = nullptr; } }

private:
    juce::Rectangle<float> track() const { return getLocalBounds().toFloat().reduced (18.0f, 12.0f); }
    float toValue (float x) const { const auto t = track(); return juce::jlimit (0.0f, 1.0f, (x - t.getX()) / t.getWidth()); }

    float startValue = 0.0f, endValue = 1.0f, playhead = -1.0f;
    juce::ParameterAttachment startAtt, endAtt;
    juce::ParameterAttachment* dragged = nullptr;
};

//==============================================================================
// Curseur "vivant" : feu, plante qui pousse, eclairs, glace, eau qui jaillit
class LivingSlider : public juce::Component, private juce::Timer
{
public:
    enum Theme { Fire, Plant, Spark, Ice, Water };

    LivingSlider (juce::RangedAudioParameter& p, Theme t, std::function<void (float)> onChange)
        : param (p), theme (t), onValueChanged (std::move (onChange)),
          attachment (p, [this] (float v) { frac = param.convertTo0to1 (v); if (onValueChanged) onValueChanged (v); repaint(); })
    {
        attachment.sendInitialUpdate();
        startTimerHz (30);
    }

    std::function<float()> seedValue;

    void setRealValue (float v)
    {
        const auto& r = param.getNormalisableRange();
        attachment.setValueAsCompleteGesture (juce::jlimit (r.start, r.end, v));
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.getNumberOfClicks() > 1) { if (seedValue) setRealValue (seedValue()); return; }   // double-clic : valeur de la seed
        dragging = true;
        attachment.beginGesture();
        setFromX (e.position.x);
    }
    void mouseDrag (const juce::MouseEvent& e) override { if (dragging) setFromX (e.position.x); }
    void mouseUp (const juce::MouseEvent&) override     { if (dragging) { attachment.endGesture(); dragging = false; } }

    static float hf (int i, int s) { const float x = std::sin (i * 12.9898f + s * 78.233f) * 43758.5453f; return x - std::floor (x); }

    static void flame (juce::Graphics& g, float x, float y, float h, float w, juce::Colour c)
    {
        juce::Path p;
        p.startNewSubPath (x - w, y);
        p.quadraticTo (x - w * 0.4f, y - h * 0.5f, x, y - h);
        p.quadraticTo (x + w * 0.4f, y - h * 0.5f, x + w, y);
        p.closeSubPath();
        g.setColour (c);
        g.fillPath (p);
    }

    void paint (juce::Graphics& g) override
    {
        const float t = (float) (juce::Time::getMillisecondCounterHiRes() * 0.001);
        const float W = (float) getWidth(), H = (float) getHeight();
        const float x0 = 12.0f, x1 = x0 + frac * (W - 24.0f), cy = H - 12.0f;
        juce::Colour c0, c1;
        switch (theme)
        {
            case Fire:  c0 = juce::Colour (0xffff3d00); c1 = juce::Colour (0xffffb300); break;
            case Plant: c0 = juce::Colour (0xff2e9e2f); c1 = juce::Colour (0xff9be564); break;
            case Spark: c0 = juce::Colour (0xff7c4dff); c1 = juce::Colour (0xff00e5ff); break;
            case Ice:   c0 = juce::Colour (0xff4fc3f7); c1 = juce::Colours::white;      break;
            default:    c0 = juce::Colour (0xff1e88e5); c1 = juce::Colour (0xff7fe3ff); break;
        }
        g.setColour (juce::Colour (0x8c1e323c));
        g.fillRoundedRectangle (x0 - 8.0f, cy - 6.0f, W - 2.0f * x0 + 16.0f, 12.0f, 6.0f);
        g.setGradientFill (juce::ColourGradient (c0, x0, 0.0f, c1, x1 + 1.0f, 0.0f, false));
        g.fillRoundedRectangle (x0 - 8.0f, cy - 6.0f, x1 - x0 + 16.0f, 12.0f, 6.0f);

        if (theme == Fire)
        {
            for (float x = x0; x <= x1; x += 4.0f)
            {
                const float n = (std::sin (x * 0.7f + t * 9.0f) + std::sin (x * 1.9f - t * 13.0f) + 2.0f) / 4.0f;
                const float h = (8.0f + 24.0f * n) * (0.45f + 0.55f * juce::jmin (1.0f, frac * 1.5f));
                flame (g, x, cy - 4.0f, h, 5.0f, juce::Colour::fromFloatRGBA (1.0f, 0.34f, 0.13f, 0.9f));
                flame (g, x, cy - 4.0f, h * 0.6f, 3.0f, juce::Colour::fromFloatRGBA (1.0f, 0.92f, 0.23f, 0.95f));
            }
        }
        else if (theme == Plant)
        {
            auto sy = [&] (float x) { return cy - 8.0f - std::sin (x * 0.08f + t * 1.5f) * 2.0f; };
            juce::Path stem; stem.startNewSubPath (x0, sy (x0));
            for (float x = x0 + 3.0f; x <= x1; x += 3.0f) stem.lineTo (x, sy (x));
            g.setColour (juce::Colour (0xff2e7d32)); g.strokePath (stem, juce::PathStrokeType (3.0f));
            int i = 0;
            for (float x = x0 + 8.0f; x < x1; x += 18.0f, ++i)
            {
                const float z = 13.0f * juce::jmin (1.0f, (x1 - x) / 50.0f + 0.3f), s = (i % 2) ? -1.0f : 1.0f;
                juce::Path leaf; leaf.addEllipse (-z * 0.45f, -z * 1.7f, z * 0.9f, z * 2.0f);
                g.setColour ((i % 2) ? juce::Colour (0xff66bb6a) : juce::Colour (0xff9be564));
                g.fillPath (leaf, juce::AffineTransform::rotation (s * (0.9f + std::sin (t * 1.5f + i) * 0.12f)).translated (x, sy (x)));
            }
            if (frac > 0.02f)
            {
                const float y = sy (x1) - 6.0f;
                if (frac > 0.9f)
                    for (int k = 0; k < 5; ++k)
                    { g.setColour (juce::Colour (0xffff80ab)); g.fillEllipse (x1 + std::cos (k * 1.26f + t) * 7.0f - 4.0f, y + std::sin (k * 1.26f + t) * 7.0f - 4.0f, 8.0f, 8.0f); }
                const float r = frac > 0.9f ? 4.0f : 5.0f;
                g.setColour (frac > 0.9f ? juce::Colour (0xffffeb3b) : juce::Colour (0xfff06292));
                g.fillEllipse (x1 - r, y - r, 2 * r, 2 * r);
            }
        }
        else if (theme == Spark)
        {
            const int sd = (int) std::floor (t * 16.0f);
            juce::Path bolt; bolt.startNewSubPath (x0, cy - 4.0f);
            int i = 0;
            for (float x = x0 + 7.0f; x < x1; x += 7.0f, ++i) bolt.lineTo (x, cy - 4.0f - hf (i, sd) * 28.0f * (0.4f + 0.6f * frac) + 8.0f);
            bolt.lineTo (x1, cy - 4.0f);
            g.setColour (juce::Colour (0x8000e5ff)); g.strokePath (bolt, juce::PathStrokeType (6.0f));
            g.setColour (juce::Colours::white);      g.strokePath (bolt, juce::PathStrokeType (2.0f));
        }
        else if (theme == Ice)
        {
            int i = 0;
            for (float x = x0; x <= x1; x += 9.0f, ++i)
            {
                const float h = 8.0f + hf (i, 3) * 22.0f * (0.5f + 0.5f * frac);
                juce::Path tri; tri.addTriangle (x - 5.0f, cy - 4.0f, x, cy - 4.0f - h, x + 5.0f, cy - 4.0f);
                g.setColour (juce::Colour::fromFloatRGBA (0.78f, 0.94f, 1.0f, 0.92f)); g.fillPath (tri);
                g.setColour (juce::Colours::white); g.strokePath (tri, juce::PathStrokeType (1.0f));
                if (hf (i, (int) std::floor (t * 3.0f)) > 0.88f) g.fillRect (x - 1.0f, cy - 8.0f - h, 3.0f, 3.0f);
            }
        }
        else
        {
            auto sw = [&] (float x) { return cy - 8.0f - std::sin (x * 0.15f + t * 4.0f) * (2.0f + 3.0f * frac); };
            juce::Path w; w.startNewSubPath (x0, cy);
            for (float x = x0; x <= x1; x += 3.0f) w.lineTo (x, sw (x));
            w.lineTo (x1, cy); w.closeSubPath();
            g.setColour (juce::Colour::fromFloatRGBA (0.24f, 0.67f, 1.0f, 0.85f)); g.fillPath (w);
            for (int k = 0; k < 7; ++k)
            {
                const float q = std::fmod (t * 1.2f + k / 7.0f, 1.0f), r = 3.0f * (1.0f - q * 0.5f);
                const float px = x1 + (k - 3.0f) * 5.0f * q, py = cy - 8.0f - std::sin (q * juce::MathConstants<float>::pi) * (14.0f + 30.0f * frac);
                g.setColour (juce::Colour::fromFloatRGBA (0.75f, 0.92f, 1.0f, 1.0f - q * 0.6f));
                g.fillEllipse (px - r, py - r, 2 * r, 2 * r);
            }
        }

        juce::ColourGradient bead (juce::Colours::white, x1 - 3.0f, cy - 3.0f, c0, x1 + 7.0f, cy + 7.0f, true);
        bead.addColour (0.6, c1);
        g.setGradientFill (bead);
        g.fillEllipse (x1 - 9.0f, cy - 9.0f, 18.0f, 18.0f);
        g.setColour (juce::Colours::white);
        g.drawEllipse (x1 - 9.0f, cy - 9.0f, 18.0f, 18.0f, 2.0f);
    }

private:
    void timerCallback() override { if (isShowing()) repaint(); }
    void setFromX (float x)
    {
        const float f = juce::jlimit (0.0f, 1.0f, (x - 12.0f) / (float) juce::jmax (1, getWidth() - 24));
        attachment.setValueAsPartOfGesture (param.convertFrom0to1 (f));
    }

    juce::RangedAudioParameter& param;
    Theme theme;
    std::function<void (float)> onValueChanged;
    float frac = 0.0f;
    bool dragging = false;
    juce::ParameterAttachment attachment;
};

//==============================================================================
// Gouttes d'eau PIXELISEES (grille 320x180 agrandie sans lissage)
class RippleOverlay : public juce::Component
{
public:
    RippleOverlay() : img (juce::Image::ARGB, gw, gh, true) { setInterceptsMouseClicks (false, false); }

    void addRipple (juce::Point<float> p, bool small)
    {
        ripples.push_back ({ (int) (p.x * gw / juce::jmax (1, getWidth())), (int) (p.y * gh / juce::jmax (1, getHeight())),
                             juce::Time::getMillisecondCounterHiRes(), small ? 500.0 : 850.0, small ? 4.0f : 11.0f, small });
        if (ripples.size() > 14) ripples.erase (ripples.begin());
    }

    void tick()
    {
        if (ripples.empty() && ! dirty) return;
        img.clear (img.getBounds());
        const double now = juce::Time::getMillisecondCounterHiRes();
        for (size_t i = ripples.size(); i-- > 0;)
        {
            const auto o = ripples[i];
            const float a = (float) ((now - o.t0) / o.life);
            if (a >= 1.0f) { ripples.erase (ripples.begin() + (long) i); continue; }
            const float ease = 1.0f - std::pow (1.0f - a, 2.2f), fade = 1.0f - a;
            if (! o.small && a < 0.3f) disc (o.x, o.y, juce::roundToInt (1.0f + a * 4.0f), quant (1.0f - a / 0.3f));
            for (int k = 0; k < (o.small ? 1 : 3); ++k)
            {
                const float rr = o.maxR * ease - (o.small ? 0.0f : k * 3.0f);
                if (rr >= 1.5f) ring (o.x, o.y, juce::roundToInt (rr), quant (fade * (1.0f - k * 0.22f)));
            }
        }
        dirty = ! ripples.empty();
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        g.setImageResamplingQuality (juce::Graphics::lowResamplingQuality);
        g.drawImage (img, getLocalBounds().toFloat());
    }

private:
    struct Ripple { int x, y; double t0, life; float maxR; bool small; };
    static constexpr int gw = 320, gh = 180;
    static int quant (float v) { return (int) (std::ceil (juce::jlimit (0.0f, 1.0f, v) * 4.0f) / 4.0f * 255.0f); }

    void put (int x, int y, int r, int g, int b, int a)
    {
        if (x < 0 || y < 0 || x >= gw || y >= gh || a <= (int) img.getPixelAt (x, y).getAlpha()) return;
        img.setPixelAt (x, y, juce::Colour ((juce::uint8) r, (juce::uint8) g, (juce::uint8) b, (juce::uint8) juce::jlimit (0, 255, a)));
    }
    void disc (int x, int y, int r, int a)
    {
        for (int dy = -r; dy <= r; ++dy) for (int dx = -r; dx <= r; ++dx) if (dx * dx + dy * dy <= r * r) put (x + dx, y + dy, 255, 255, 255, a);
    }
    void ring (int x, int y, int rr, int a)
    {
        const int n = rr + 4;
        for (int dy = -n; dy <= n; ++dy)
            for (int dx = -n; dx <= n; ++dx)
            {
                const float e = std::hypot ((float) dx, (float) dy) - (float) rr;
                if (e > -1.0f && e <= 0.4f)       put (x + dx, y + dy, 255, 255, 255, a);
                else if (e > 0.4f && e < 1.5f)    { if (dy >= 0) put (x + dx, y + dy, 20, 80, 150, (int) (a * 0.55f)); else put (x + dx, y + dy, 170, 225, 255, (int) (a * 0.55f)); }
                else if (e <= -1.0f && e > -3.0f && ((x + dx + y + dy) & 1) == 0) put (x + dx, y + dy, 255, 255, 255, (int) (a * 0.35f));
            }
    }

    juce::Image img;
    std::vector<Ripple> ripples;
    bool dirty = false;
};

//==============================================================================
// Clavier 12 notes (section Harmonique) : notes autorisees en ambre, tonique entouree
class NoteKeyboard : public juce::Component
{
public:
    std::function<void (int)> onToggle;

    void setState (uint16_t m, int k) { if (m != mask || k != key) { mask = m; key = k; repaint(); } }

    void paint (juce::Graphics& g) override
    {
        static const char* names[] = { "C","C#","D","D#","E","F","F#","G","G#","A","A#","B" };
        for (int i = 0; i < 12; ++i)
        {
            const auto r = keyRect (i);
            const bool on = (mask >> i) & 1;
            paintGlass (g, r, 11.0f, on, hovered == i, false, juce::Colour (0xffffa726));
            if (i == key) { g.setColour (juce::Colour (0xffffd54f)); g.drawRoundedRectangle (r.reduced (1.0f), 11.0f, 2.5f); }
            g.setFont (uiFont (14.0f, juce::Font::bold));
            shadowText (g, names[i], r.toNearestInt(), juce::Justification::centred, juce::Colours::white.withAlpha (on ? 1.0f : 0.55f), true);
        }
    }
    void mouseMove (const juce::MouseEvent& e) override { const int h = hit (e.position); if (h != hovered) { hovered = h; repaint(); } }
    void mouseExit (const juce::MouseEvent&) override   { hovered = -1; repaint(); }
    void mouseUp (const juce::MouseEvent& e) override   { const int h = hit (e.position); if (h >= 0 && e.mouseWasClicked() && onToggle) onToggle (h); }

private:
    juce::Rectangle<float> keyRect (int i) const { const float w = (getWidth() - 11 * 5.0f) / 12.0f; return { i * (w + 5.0f), 2.0f, w, (float) getHeight() - 4.0f }; }
    int hit (juce::Point<float> p) const { for (int i = 0; i < 12; ++i) if (keyRect (i).contains (p)) return i; return -1; }
    uint16_t mask = 0; int key = 0, hovered = -1;
};

//==============================================================================
// Carte de module (ecran principal) : clic = ouvrir l'editeur sur l'onglet, interrupteur = on/off
class ModuleCard : public juce::Component
{
public:
    ModuleCard (juce::String t, juce::Colour a) : title (std::move (t)), accent (a) {}

    std::function<void()> onOpen, onToggle;

    void setContent (bool on, const juce::String& l1, const juce::String& l2)
    {
        if (on != enabled || l1 != line1 || l2 != line2) { enabled = on; line1 = l1; line2 = l2; repaint(); }
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (3.0f);
        g.beginTransparencyLayer (enabled ? 1.0f : 0.6f);
        paintGlass (g, r, 22.0f, false, hover, false);
        g.setFont (uiFont (17.0f, juce::Font::bold));
        const auto dotR = juce::Rectangle<float> (12.0f, 12.0f).withCentre ({ r.getX() + 22.0f, r.getY() + 24.0f });
        g.setColour (accent.withAlpha (0.45f));  g.fillEllipse (dotR.expanded (3.0f));
        g.setColour (accent);                    g.fillEllipse (dotR);
        shadowText (g, title, r.toNearestInt().reduced (36, 12).withHeight (24), juce::Justification::centredLeft, juce::Colours::white, true);
        g.setFont (uiFont (21.0f, juce::Font::bold));
        shadowText (g, line1, r.toNearestInt().reduced (16, 0).withY ((int) r.getY() + 42).withHeight (26), juce::Justification::centredLeft, juce::Colours::white, true);
        g.setFont (uiFont (14.0f));
        shadowText (g, line2, r.toNearestInt().reduced (16, 0).withY ((int) r.getY() + 68).withHeight (20), juce::Justification::centredLeft, juce::Colours::white.withAlpha (0.92f), true);
        auto acc = juce::Rectangle<float> (r.getX() + 16.0f, r.getBottom() - 12.0f, r.getWidth() - 32.0f, 3.0f);
        g.setColour (accent.withAlpha (0.35f)); g.fillRoundedRectangle (acc.expanded (0.0f, 2.0f), 3.0f);
        g.setColour (accent);                   g.fillRoundedRectangle (acc, 1.5f);

        const auto sw = switchRect();
        g.setColour (enabled ? juce::Colour (0xff27c160) : juce::Colours::white.withAlpha (0.3f));
        g.fillRoundedRectangle (sw, sw.getHeight() * 0.5f);
        g.setColour (juce::Colours::white);
        g.fillEllipse (juce::Rectangle<float> (16.0f, 16.0f).withCentre ({ enabled ? sw.getRight() - 11.0f : sw.getX() + 11.0f, sw.getCentreY() }));
        g.endTransparencyLayer();
    }

    void mouseEnter (const juce::MouseEvent&) override { hover = true;  repaint(); }
    void mouseExit (const juce::MouseEvent&) override  { hover = false; repaint(); }
    void mouseUp (const juce::MouseEvent& e) override
    {
        if (! e.mouseWasClicked()) return;
        if (switchRect().expanded (6.0f).contains (e.position)) { if (onToggle) onToggle(); }
        else if (onOpen) onOpen();
    }

private:
    juce::Rectangle<float> switchRect() const { return { (float) getWidth() - 62.0f, 16.0f, 42.0f, 22.0f }; }
    juce::String title, line1, line2;
    juce::Colour accent;
    bool enabled = true, hover = false;
};

//==============================================================================
// Logo cliquable (ouvre les credits caches)
class LogoComponent : public juce::Component
{
public:
    LogoComponent() { setMouseCursor (juce::MouseCursor::PointingHandCursor); }
    void paint (juce::Graphics& g) override { if (image.isValid()) g.drawImage (image, getLocalBounds().toFloat(), juce::RectanglePlacement::centred); }
    void mouseUp (const juce::MouseEvent& e) override { if (e.mouseWasClicked() && onClick) onClick(); }
    juce::Image image;
    std::function<void()> onClick;
};

class CreditsOverlay : public juce::Component
{
public:
    CreditsOverlay() { setVisible (false); }
    void mouseDown (const juce::MouseEvent&) override { setVisible (false); }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colours::black.withAlpha (0.55f));
        const auto panel = juce::Rectangle<float> (340.0f, 100.0f, (float) getWidth() - 680.0f, (float) getHeight() - 200.0f);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xfff4fbff), 0.0f, panel.getY(), juce::Colour (0xffbfe3f7), 0.0f, panel.getBottom(), false));
        g.fillRoundedRectangle (panel, 22.0f);
        g.setColour (juce::Colours::white); g.drawRoundedRectangle (panel, 22.0f, 3.0f);
        const auto a = panel.toNearestInt();
        if (logo.isValid()) g.drawImage (logo, juce::Rectangle<float> ((float) a.getCentreX() - 160.0f, (float) a.getY() + 18.0f, 320.0f, 116.0f), juce::RectanglePlacement::centred);
        g.setColour (juce::Colour (0xff37474f)); g.setFont (uiFont (18.0f));
        g.drawText (fr (L"Id\u00e9e originale du concept"), a.getX(), a.getY() + 156, a.getWidth(), 26, juce::Justification::centred);
        g.setColour (juce::Colour (0xff1565c0)); g.setFont (uiFont (54.0f, juce::Font::bold));
        g.drawText ("Omyll", a.getX(), a.getY() + 184, a.getWidth(), 66, juce::Justification::centred);
        g.setColour (juce::Colour (0xff37474f)); g.setFont (uiFont (19.0f));
        g.drawFittedText (fr (L"AeroSeed FX est n\u00e9 d'une id\u00e9e propos\u00e9e par Omyll : g\u00e9n\u00e9rer un effet audio unique et reproductible "
                              L"\u00e0 partir d'une simple image, comme une \u00ab seed \u00bb. Merci \u00e0 lui pour ce concept !"),
                          a.getX() + 40, a.getY() + 262, a.getWidth() - 80, 120, juce::Justification::centredTop, 6);
        g.setColour (juce::Colour (0x9937474f)); g.setFont (uiFont (15.0f));
        g.drawText (fr (L"Clique n'importe o\u00f9 pour fermer"), a.getX(), a.getBottom() - 36, a.getWidth(), 22, juce::Justification::centred);
    }
    juce::Image logo;
};

} // namespace aero
