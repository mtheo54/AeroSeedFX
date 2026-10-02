#pragma once
#include <JuceHeader.h>
#include "AeroUI.h"
#include "SpectrumView.h"

namespace aero
{
//==============================================================================
// Editeur d'effets : tout l'ecran se floute, les reglages s'affichent directement dessus (pas de fenetre).
class EditPanel : public juce::Component, private juce::Timer
{
public:
    enum Tab { Harm = 0, Filt, Dist, Crush, Chor, numTabs };

    EditPanel (AeroSeedAudioProcessor& p, AnalyzerEngine& engine) : proc (p), spectrum (p, engine)
    {
        addAndMakeVisible (spectrum);

        const char* tabNames[numTabs] = { "Harmonique", "Filtre", "Distortion", "Bitcrusher", "Chorus" };
        for (int i = 0; i < numTabs; ++i)
        {
            auto& b = tabs[(size_t) i];
            b.setButtonText (tabNames[i]);
            b.setClickingTogglesState (true);
            b.setRadioGroupId (4242);
            b.onClick = [this, i] { if (tabs[(size_t) i].getToggleState()) showTab (i); };
            addAndMakeVisible (b);
        }
        for (auto& pg : pages) addChildComponent (pg);

        // --- Harmonique
        toggle (Harm, "H_ON");
        combo (Harm, "H_KEY", fr (L"Tonalit\u00e9"));
        combo (Harm, "H_SCALE", "Gamme");
        keyboard.onToggle = [this] (int pc) { proc.toggleNote (pc); };
        pages[Harm].addAndMakeVisible (keyboard);
        slider (Harm, "H_MORPH", "Morph", LivingSlider::Water, 0, "%", [] (const aerodsp::Patch& x) { return (float) x.morph; });
        slider (Harm, "H_GATE", "Gate", LivingSlider::Ice, 0, "dB", [] (const aerodsp::Patch& x) { return (float) x.gateDb; });
        hint.setText (fr (L"Spectral Morph : m\u00e9lange le son d'origine et le son accord\u00e9 dans le domaine fr\u00e9quentiel, l'attaque reste nette.\n"
                          L"Spectral Gate : supprime les tra\u00een\u00e9es m\u00e9talliques (ringing) cr\u00e9\u00e9es par l'accordage."), juce::dontSendNotification);
        hint.setFont (uiFont (14.0f));
        hint.setJustificationType (juce::Justification::topLeft);
        hint.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.85f));
        pages[Harm].addAndMakeVisible (hint);

        // --- Filtre
        toggle (Filt, "F_ON");
        combo (Filt, "FILTER_TYPE", "Type");
        combo (Filt, "FILTER_ORDER", "Ordre");
        slider (Filt, "CUTOFF", "Cutoff", LivingSlider::Plant, 0, "Hz", [] (const aerodsp::Patch& x) { return (float) x.cutoff; });
        slider (Filt, "RESO", fr (L"R\u00e9sonance"), LivingSlider::Spark, 2, "Q", [] (const aerodsp::Patch& x) { return (float) x.resonance; });

        // --- Distortion
        toggle (Dist, "D_ON");
        combo (Dist, "DIST_TYPE", "Type");
        slider (Dist, "DRIVE", "Drive", LivingSlider::Fire, 2, "x", [] (const aerodsp::Patch& x) { return (float) x.drive; });

        // --- Bitcrusher
        toggle (Crush, "CRUSH_ON");
        toggle (Crush, "MB_ON", "Multibande");
        slider (Crush, "BITS", "Bits", LivingSlider::Ice, 0, "bit", [] (const aerodsp::Patch& x) { return (float) x.bits; });
        slider (Crush, "B1", "Bas", LivingSlider::Ice, 0, "bit", [] (const aerodsp::Patch& x) { return (float) x.bandBits[0]; });
        slider (Crush, "B2", fr (L"M\u00e9dium"), LivingSlider::Ice, 0, "bit", [] (const aerodsp::Patch& x) { return (float) x.bandBits[1]; });
        slider (Crush, "B3", "Haut", LivingSlider::Ice, 0, "bit", [] (const aerodsp::Patch& x) { return (float) x.bandBits[2]; });
        slider (Crush, "X1", "Coupure 1", LivingSlider::Spark, 0, "Hz", [] (const aerodsp::Patch& x) { return (float) x.xover1; });
        slider (Crush, "X2", "Coupure 2", LivingSlider::Spark, 0, "Hz", [] (const aerodsp::Patch& x) { return (float) x.xover2; });

        // --- Chorus
        toggle (Chor, "C_ON");
        slider (Chor, "CH_RATE", "Rate", LivingSlider::Water, 2, "Hz", [] (const aerodsp::Patch& x) { return (float) x.chorusRate; });
        slider (Chor, "CH_DEPTH", "Depth", LivingSlider::Water, 0, "%", [] (const aerodsp::Patch& x) { return (float) x.chorusDepth; });
        slider (Chor, "CH_MIX", "Mix", LivingSlider::Water, 0, "%", [] (const aerodsp::Patch& x) { return (float) x.chorusMix; });

        resetButton.setButtonText (fr (L"Revenir \u00e0 la seed"));
        resetButton.onClick = [this] { proc.resetToSeed(); };
        addAndMakeVisible (resetButton);
        doneButton.setButtonText (fr (L"Termin\u00e9"));
        doneButton.onClick = [this] { if (onClose) onClose(); };
        addAndMakeVisible (doneButton);

        setVisible (false);
        showTab (Harm);
        startTimerHz (10);
    }

    std::function<void()> onClose;

    void setSpectrumImage (const juce::Image& img) { spectrum.background = img; spectrum.repaint(); }

    void showTab (int i)
    {
        current = juce::jlimit (0, numTabs - 1, i);
        tabs[(size_t) current].setToggleState (true, juce::dontSendNotification);
        for (int k = 0; k < numTabs; ++k) pages[(size_t) k].setVisible (k == current);
    }

    // fond flou : capture de l'interface reduite, floutee puis agrandie (rapide et doux)
    void setBlurredBackground (const juce::Image& snapshot)
    {
        if (! snapshot.isValid()) { blurred = {}; return; }
        auto small = snapshot.rescaled (snapshot.getWidth() / 6, snapshot.getHeight() / 6, juce::Graphics::highResamplingQuality);
        juce::Image tmp (small.getFormat(), small.getWidth(), small.getHeight(), true);
        juce::ImageConvolutionKernel k (7);
        k.createGaussianBlur (2.5f);
        k.applyToImage (tmp, small, small.getBounds());
        k.applyToImage (small, tmp, small.getBounds());
        blurred = small.rescaled (snapshot.getWidth(), snapshot.getHeight(), juce::Graphics::highResamplingQuality);
    }

    void paint (juce::Graphics& g) override
    {
        if (blurred.isValid()) g.drawImageAt (blurred, 0, 0);
        g.fillAll (juce::Colour::fromFloatRGBA (0.02f, 0.09f, 0.15f, 0.42f));
        g.setFont (uiFont (27.0f, juce::Font::bold));
        shadowText (g, fr (L"\u00c9dition de l'effet"), { 90, 26, 600, 34 }, juce::Justification::centredLeft, juce::Colours::white, true);
        g.setFont (uiFont (16.0f));
        shadowText (g, status, { 90, 60, 600, 22 }, juce::Justification::centredLeft, juce::Colour (0xffffd180), true);
        g.setFont (uiFont (14.0f));
        shadowText (g, fr (L"Double-clic sur un curseur = valeur de la seed  \u00b7  clic sur une valeur pour la taper"),
                    { 330, getHeight() - 70, 800, 42 }, juce::Justification::centredLeft, juce::Colours::white.withAlpha (0.8f), true);
    }

    void resized() override
    {
        doneButton.setBounds (getWidth() - 90 - 150, 30, 150, 44);
        spectrum.setBounds (90, 122, getWidth() - 180, 230);
        for (int i = 0; i < numTabs; ++i) tabs[(size_t) i].setBounds (90 + i * 170, 366, 160, 44);
        for (auto& pg : pages) pg.setBounds (90, 420, getWidth() - 180, 226);
        resetButton.setBounds (90, getHeight() - 72, 230, 46);
        layoutPages();
    }

private:
    struct Row
    {
        juce::Label name, value;
        std::unique_ptr<LivingSlider> slider;
        int decimals = 0;
        juce::String unit;
    };
    struct Item { int page; enum Kind { Toggle, Combo, Slider } kind; juce::Component* comp; juce::Label* label; Row* row; };

    juce::Label* makeLabel (int page, const juce::String& text)
    {
        auto l = std::make_unique<juce::Label> ("", text);
        l->setFont (uiFont (17.0f, juce::Font::bold));
        l->setColour (juce::Label::textColourId, juce::Colours::white);
        pages[(size_t) page].addAndMakeVisible (*l);
        labels.push_back (std::move (l));
        return labels.back().get();
    }

    void toggle (int page, const char* id, const juce::String& text = "Actif")
    {
        auto b = std::make_unique<juce::ToggleButton> ("");
        pages[(size_t) page].addAndMakeVisible (*b);
        buttonAtts.push_back (std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, id, *b));
        items.push_back ({ page, Item::Toggle, b.get(), makeLabel (page, text), nullptr });
        toggles.push_back (std::move (b));
    }

    void combo (int page, const char* id, const juce::String& text)
    {
        auto c = std::make_unique<juce::ComboBox>();
        if (auto* ch = dynamic_cast<juce::AudioParameterChoice*> (proc.apvts.getParameter (id))) c->addItemList (ch->choices, 1);
        pages[(size_t) page].addAndMakeVisible (*c);
        comboAtts.push_back (std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, id, *c));
        items.push_back ({ page, Item::Combo, c.get(), makeLabel (page, text), nullptr });
        combos.push_back (std::move (c));
    }

    void slider (int page, const char* id, const juce::String& text, LivingSlider::Theme th, int decimals, const juce::String& unit,
                 std::function<float (const aerodsp::Patch&)> seedGetter)
    {
        auto row = std::make_unique<Row>();
        auto* r = row.get();
        r->decimals = decimals;  r->unit = unit;
        r->name.setText (text, juce::dontSendNotification);
        r->name.setFont (uiFont (17.0f, juce::Font::bold));
        r->value.setFont (monoFont (15.0f, juce::Font::bold));
        r->value.setJustificationType (juce::Justification::centredRight);
        r->value.setEditable (true, false);
        r->value.setColour (juce::Label::backgroundColourId, juce::Colours::white.withAlpha (0.14f));
        r->value.setColour (juce::Label::outlineColourId, juce::Colours::white.withAlpha (0.45f));
        r->value.onTextChange = [r] { r->slider->setRealValue ((float) r->value.getText().getDoubleValue()); };
        r->slider = std::make_unique<LivingSlider> (*proc.apvts.getParameter (id), th,
            [r] (float v) { r->value.setText (juce::String (v, r->decimals) + " " + r->unit, juce::dontSendNotification); });
        r->slider->seedValue = [this, seedGetter] { return seedGetter (proc.getSeedPatch()); };
        auto& pg = pages[(size_t) page];
        pg.addAndMakeVisible (r->name);  pg.addAndMakeVisible (*r->slider);  pg.addAndMakeVisible (r->value);
        items.push_back ({ page, Item::Slider, r->slider.get(), &r->name, r });
        if (juce::String (id) == "BITS") bitsRow = r;
        if (id[0] == 'B' && id[1] >= '1' && id[1] <= '3') bandRows.push_back (r);
        if (id[0] == 'X') xoverRows.push_back (r);
        rows.push_back (std::move (row));
    }

    // colonnes : 2 par page (3 pour le Bitcrusher). Interrupteurs et menus a gauche, curseurs a droite.
    void layoutPages()
    {
        const int pageW = getWidth() - 180;
        for (int p = 0; p < numTabs; ++p)
        {
            const int cols = p == Crush ? 3 : 2, gap = p == Crush ? 40 : 70, colW = (pageW - gap * (cols - 1)) / cols;
            int y[3] = { 0, 0, 0 };
            int sliderCount = 0;
            for (auto& it : items)
            {
                if (it.page != p) continue;
                int col = it.kind == Item::Slider ? 1 : 0;
                if (p == Crush && it.kind == Item::Slider) col = (sliderCount == 0) ? 0 : (sliderCount <= 3 ? 1 : 2);
                if (it.kind == Item::Slider) ++sliderCount;
                const int x = col * (colW + gap);
                const int h = it.kind == Item::Slider ? 52 : 44;
                it.label->setBounds (x, y[col], 104, h);
                if (it.kind == Item::Slider)
                {
                    it.row->slider->setBounds (x + 104, y[col], colW - 104 - 104, 48);
                    it.row->value.setBounds (x + colW - 96, y[col] + 9, 96, 30);
                }
                else if (it.kind == Item::Combo) it.comp->setBounds (x + 104, y[col] + 4, juce::jmin (260, colW - 104), 36);
                else                             it.comp->setBounds (x + 104, y[col] + 4, 70, 36);
                y[col] += h;
            }
            if (p == Harm)
            {
                keyboard.setBounds (0, y[0] + 6, colW, 46);
                hint.setBounds (colW + gap, y[1] + 6, colW, 60);
            }
        }
    }

    void timerCallback() override
    {
        if (! isVisible()) return;
        const auto s = proc.isEditedFromSeed() ? fr (L"\u2022 modifi\u00e9") : proc.hasSeed() ? fr (L"valeurs de la seed")
                                                                                     : fr (L"d\u00e9pose une image pour g\u00e9n\u00e9rer la seed");
        if (s != status) { status = s; repaint(); }
        keyboard.setState (proc.getEffectiveMask(), proc.intValue ("H_KEY"));

        const bool crush = proc.value ("CRUSH_ON") > 0.5f, mb = proc.value ("MB_ON") > 0.5f;
        auto dim = [] (Row* r, bool on) { if (r) { r->slider->setEnabled (on); r->slider->setAlpha (on ? 1.0f : 0.38f); r->name.setAlpha (on ? 1.0f : 0.38f); } };
        dim (bitsRow, crush && ! mb);
        for (auto* r : bandRows)  dim (r, crush && mb);
        for (auto* r : xoverRows) dim (r, crush && mb);
    }

    AeroSeedAudioProcessor& proc;
    SpectrumView spectrum;
    std::array<juce::TextButton, numTabs> tabs;
    std::array<juce::Component, numTabs> pages;
    NoteKeyboard keyboard;
    juce::Label hint;
    juce::TextButton resetButton, doneButton;
    juce::Image blurred;
    juce::String status;
    int current = 0;

    std::vector<std::unique_ptr<juce::Label>> labels;
    std::vector<std::unique_ptr<juce::ToggleButton>> toggles;
    std::vector<std::unique_ptr<juce::ComboBox>> combos;
    std::vector<std::unique_ptr<Row>> rows;
    std::vector<Item> items;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>> buttonAtts;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>> comboAtts;
    Row* bitsRow = nullptr;
    std::vector<Row*> bandRows, xoverRows;
};

} // namespace aero
