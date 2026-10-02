#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "AeroUI.h"
#include "AeroScene.h"
#include "SpectrumView.h"
#include "EditPanel.h"
#include "IntroLayer.h"

namespace aero
{
// Pastille de graine (miniature de l'image + code de la seed)
class SeedChip : public juce::Component
{
public:
    std::function<void()> onClick;
    void set (const juce::Image& img, const juce::String& t1, const juce::String& t2) { image = img; line1 = t1; line2 = t2; repaint(); }

    void paint (juce::Graphics& g) override
    {
        const auto r = getLocalBounds().toFloat().reduced (3.0f);
        paintGlass (g, r, r.getHeight() * 0.5f, false, hover);
        const auto c = juce::Rectangle<float> (46.0f, 46.0f).withCentre ({ r.getX() + 34.0f, r.getCentreY() });
        g.setColour (juce::Colours::white.withAlpha (0.35f));  g.fillEllipse (c);
        if (image.isValid())
        {
            g.saveState();
            juce::Path p; p.addEllipse (c); g.reduceClipRegion (p);
            g.drawImage (image, c, juce::RectanglePlacement::fillDestination);
            g.restoreState();
        }
        g.setColour (juce::Colours::white);  g.drawEllipse (c, 2.0f);
        g.setFont (uiFont (17.0f, juce::Font::bold));
        shadowText (g, line1, { (int) r.getX() + 66, (int) r.getY() + 10, (int) r.getWidth() - 80, 22 }, juce::Justification::centredLeft, juce::Colours::white, true);
        g.setFont (uiFont (13.0f));
        shadowText (g, line2, { (int) r.getX() + 66, (int) r.getY() + 32, (int) r.getWidth() - 80, 18 }, juce::Justification::centredLeft, juce::Colours::white.withAlpha (0.92f), true);
    }
    void mouseEnter (const juce::MouseEvent&) override { hover = true;  repaint(); }
    void mouseExit (const juce::MouseEvent&) override  { hover = false; repaint(); }
    void mouseUp (const juce::MouseEvent& e) override  { if (e.mouseWasClicked() && onClick) onClick(); }

private:
    juce::Image image;
    juce::String line1, line2;
    bool hover = false;
};

// Menu des fonds d'ecran (vignettes)
class WallpaperMenu : public juce::Component
{
public:
    std::function<void (int)> onSelect;
    std::array<juce::Image, kNumThemes> thumbs;
    int selected = 0;

    void paint (juce::Graphics& g) override
    {
        const auto r = getLocalBounds().toFloat().reduced (2.0f);
        g.setColour (juce::Colour::fromFloatRGBA (0.04f, 0.11f, 0.17f, 0.82f));  g.fillRoundedRectangle (r, 20.0f);
        g.setColour (juce::Colours::white.withAlpha (0.55f));                    g.drawRoundedRectangle (r, 20.0f, 1.5f);
        for (int i = 0; i < kNumThemes; ++i)
        {
            const auto t = tile (i);
            g.saveState();
            juce::Path p; p.addRoundedRectangle (t, 12.0f); g.reduceClipRegion (p);
            if (thumbs[(size_t) i].isValid()) g.drawImage (thumbs[(size_t) i], t, juce::RectanglePlacement::fillDestination);
            g.restoreState();
            g.setColour (i == selected ? juce::Colour (0xff5cc8ff) : juce::Colours::white.withAlpha (i == hovered ? 0.95f : 0.5f));
            g.drawRoundedRectangle (t, 12.0f, i == selected ? 3.0f : 2.0f);
            g.setColour (juce::Colours::white);  g.setFont (uiFont (13.0f, juce::Font::bold));
            g.drawText (fr (theme (i).name), t.toNearestInt().withY ((int) t.getBottom() + 2).withHeight (18), juce::Justification::centred);
        }
    }
    void mouseMove (const juce::MouseEvent& e) override { const int h = hit (e.position); if (h != hovered) { hovered = h; repaint(); } }
    void mouseUp (const juce::MouseEvent& e) override   { const int h = hit (e.position); if (h >= 0 && onSelect) { selected = h; onSelect (h); } }

private:
    juce::Rectangle<float> tile (int i) const { const float w = (getWidth() - 28.0f - 36.0f) / 4.0f; return { 14.0f + (i % 4) * (w + 12.0f), 14.0f + (i / 4) * 90.0f, w, 62.0f }; }
    int hit (juce::Point<float> p) const { for (int i = 0; i < kNumThemes; ++i) if (tile (i).contains (p)) return i; return -1; }
    int hovered = -1;
};
} // namespace aero

//==============================================================================
class AeroSeedAudioProcessorEditor : public juce::AudioProcessorEditor,
                                     public juce::FileDragAndDropTarget,
                                     private juce::Timer
{
public:
    explicit AeroSeedAudioProcessorEditor (AeroSeedAudioProcessor&);
    ~AeroSeedAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;

    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void fileDragEnter (const juce::StringArray&, int, int) override;
    void fileDragExit (const juce::StringArray&) override;
    void filesDropped (const juce::StringArray& files, int x, int y) override;

private:
    void timerCallback() override;
    void startOpening();
    void finishOpening();
    void applyEntrances (float t);
    void openEditor (int tab);
    void chooseImageFile();
    void loadImageFile (const juce::File& f);
    void setTheme (int index);
    void rebuildBackground();
    void refreshModules();
    void refreshSeedInfo();

    AeroSeedAudioProcessor& proc;
    aero::AeroLookAndFeel laf;

    juce::Image logoImage, bgCache;
    std::array<juce::Image, 3> photos;
    int themeIndex = 0, lastThumbVersion = -1, frameCounter = 0;
    bool isDragging = false, needChoice = true;
    double lastTime = 0.0;
    float sinceOpen = 0.0f, chooseTime = 0.0f, openTime = 0.0f;
    juce::uint32 lastTrailMs = 0;

    aero::Bubbles bubbles;
    aero::Weather weather;
    aero::OpeningAnimation anim;
    aero::AnalyzerEngine engine;

    // composants (ordre d'ajout = ordre d'affichage)
    aero::SpectrumView spectrum;
    juce::TextButton viewImage, viewSpectrum;
    aero::SeedChip seedChip;
    juce::TextButton wallpaperButton;
    std::array<std::unique_ptr<aero::ModuleCard>, 5> cards;
    juce::ComboBox syncCombo;
    aero::TriggerBar triggerBar;
    juce::Slider mixKnob;
    aero::EditPanel editPanel;
    aero::WallpaperMenu wallpaperMenu;
    aero::IntroLayer intro;
    aero::LogoComponent logo;
    aero::CreditsOverlay credits;
    aero::RippleOverlay ripples;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> syncAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>   mixAttachment;
    std::unique_ptr<juce::FileChooser> chooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AeroSeedAudioProcessorEditor)
};
