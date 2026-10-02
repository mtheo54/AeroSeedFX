#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "LogoData.h"
#include "BgData.h"

using namespace aero;

namespace
{
    constexpr int maxLongSide = 3840, maxShortSide = 2160;
    constexpr juce::int64 maxFileBytes = 64LL * 1024 * 1024;

    // positions (interface 1280 x 720)
    const juce::Rectangle<int> rLogoFinal (510, 18, 260, 95), rLogoBig (260, 200, 760, 276);
    const juce::Rectangle<int> rSpectrum (40, 128, 1200, 258), rPanelA (40, 538, 780, 152), rPanelB (840, 538, 400, 152);
    const char* moduleIds[5] = { "H_ON", "F_ON", "D_ON", "CRUSH_ON", "C_ON" };

    float easeIO (float t)       { return t < 0.5f ? 4 * t * t * t : 1.0f - std::pow (-2.0f * t + 2.0f, 3.0f) / 2.0f; }
    float easeOutBack (float t)  { const float c1 = 1.70158f, c3 = c1 + 1.0f; return 1.0f + c3 * std::pow (t - 1.0f, 3.0f) + c1 * std::pow (t - 1.0f, 2.0f); }

    bool hasImageExtension (const juce::String& p) { return p.endsWithIgnoreCase (".png") || p.endsWithIgnoreCase (".jpg") || p.endsWithIgnoreCase (".jpeg"); }

    // dimensions lues dans l'en-tete PNG / JPEG, sans decoder l'image
    bool peekImageSize (const juce::File& file, int& w, int& h)
    {
        juce::FileInputStream in (file);
        if (! in.openedOk()) return false;
        juce::uint8 sig[8] = {};
        if (in.read (sig, 8) != 8) return false;
        static const juce::uint8 pngSig[8] = { 0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A };
        if (std::memcmp (sig, pngSig, 8) == 0) { in.setPosition (16); w = in.readIntBigEndian(); h = in.readIntBigEndian(); return w > 0 && h > 0; }
        if (sig[0] == 0xFF && sig[1] == 0xD8)
        {
            in.setPosition (2);
            while (! in.isExhausted())
            {
                if ((juce::uint8) in.readByte() != 0xFF) continue;
                auto marker = (juce::uint8) in.readByte();
                while (marker == 0xFF && ! in.isExhausted()) marker = (juce::uint8) in.readByte();
                if (marker == 0xD8 || marker == 0x01 || (marker >= 0xD0 && marker <= 0xD7)) continue;
                const int len = (int) (juce::uint16) in.readShortBigEndian();
                if (marker >= 0xC0 && marker <= 0xCF && marker != 0xC4 && marker != 0xC8 && marker != 0xCC)
                {
                    in.skipNextBytes (1);
                    h = (int) (juce::uint16) in.readShortBigEndian();
                    w = (int) (juce::uint16) in.readShortBigEndian();
                    return w > 0 && h > 0;
                }
                in.skipNextBytes (juce::jmax (0, len - 2));
            }
        }
        return false;
    }

    bool tooBig (int w, int h) { return juce::jmax (w, h) > maxLongSide || juce::jmin (w, h) > maxShortSide; }

    void alert (const juce::String& title, const juce::String& msg)
    {
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, title, msg);
    }
}

//==============================================================================
AeroSeedAudioProcessorEditor::AeroSeedAudioProcessorEditor (AeroSeedAudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
      spectrum (p, engine),
      triggerBar (*p.apvts.getParameter ("TRIG_START"), *p.apvts.getParameter ("TRIG_END")),
      editPanel (p, engine),
      intro (anim)
{
    setLookAndFeel (&laf);

    logoImage = juce::ImageFileFormat::loadFrom (LogoData::png, (size_t) LogoData::pngSize);
    photos[0] = juce::ImageFileFormat::loadFrom (BgData::bliss,   (size_t) BgData::blissSize);
    photos[1] = juce::ImageFileFormat::loadFrom (BgData::cyber,   (size_t) BgData::cyberSize);
    photos[2] = juce::ImageFileFormat::loadFrom (BgData::dolphin, (size_t) BgData::dolphinSize);

    // --- spectre + bascule Image / Spectre
    addAndMakeVisible (spectrum);
    for (auto* b : { &viewImage, &viewSpectrum })
    {
        b->setClickingTogglesState (true);
        b->setRadioGroupId (7);
        addAndMakeVisible (*b);
    }
    viewImage.setButtonText ("Image");
    viewSpectrum.setButtonText ("Spectre");
    viewSpectrum.setToggleState (true, juce::dontSendNotification);
    viewImage.onClick    = [this] { spectrum.imageOnly = viewImage.getToggleState(); spectrum.repaint(); };
    viewSpectrum.onClick = [this] { spectrum.imageOnly = ! viewSpectrum.getToggleState(); spectrum.repaint(); };

    // --- pastille de graine, fonds d'ecran
    seedChip.onClick = [this] { chooseImageFile(); };
    addAndMakeVisible (seedChip);
    wallpaperButton.setButtonText (fr (L"Fond d'\u00e9cran"));
    wallpaperButton.onClick = [this] { wallpaperMenu.setVisible (! wallpaperMenu.isVisible()); wallpaperMenu.toFront (false); };
    addAndMakeVisible (wallpaperButton);

    // --- modules d'effets
    const juce::String titles[5] = { "Harmonique", "Filtre", "Distortion", "Bitcrusher", "Chorus" };
    const juce::Colour accents[5] = { juce::Colour (0xffffb300), juce::Colour (0xff66bb6a), juce::Colour (0xffff5a2a), juce::Colour (0xff4fc3f7), juce::Colour (0xff5c9dff) };
    for (int i = 0; i < 5; ++i)
    {
        cards[(size_t) i] = std::make_unique<ModuleCard> (titles[i], accents[i]);
        cards[(size_t) i]->onOpen   = [this, i] { openEditor (i); };
        cards[(size_t) i]->onToggle = [this, i] { proc.setParameterReal (moduleIds[i], proc.value (moduleIds[i]) > 0.5f ? 0.0f : 1.0f); refreshModules(); };
        addAndMakeVisible (*cards[(size_t) i]);
    }

    // --- synchronisation + mix
    if (auto* c = dynamic_cast<juce::AudioParameterChoice*> (proc.apvts.getParameter ("SYNC_MODE"))) syncCombo.addItemList (c->choices, 1);
    addAndMakeVisible (syncCombo);
    syncAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, "SYNC_MODE", syncCombo);
    addAndMakeVisible (triggerBar);
    mixKnob.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    mixKnob.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible (mixKnob);
    mixAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, "MIX", mixKnob);

    // --- calques superieurs
    editPanel.onClose = [this] { editPanel.setVisible (false); };
    addChildComponent (editPanel);

    for (int i = 0; i < kNumThemes; ++i)   // vignettes du menu des fonds
    {
        if (i >= kFirstPhoto) { wallpaperMenu.thumbs[(size_t) i] = photos[(size_t) (i - kFirstPhoto)].rescaled (160, 90, juce::Graphics::mediumResamplingQuality); continue; }
        juce::Image full (juce::Image::RGB, kW, kH, false);
        { juce::Graphics g (full); paintProcedural (g, theme (i)); }
        wallpaperMenu.thumbs[(size_t) i] = full.rescaled (160, 90, juce::Graphics::mediumResamplingQuality);
    }
    wallpaperMenu.onSelect = [this] (int i) { setTheme (i); wallpaperMenu.setVisible (false); };
    addChildComponent (wallpaperMenu);

    intro.imageCard.onClick  = [this] { chooseImageFile(); };
    intro.manualCard.onClick = [this] { startOpening(); };
    addAndMakeVisible (intro);

    logo.image = logoImage;  credits.logo = logoImage;
    logo.onClick = [this] { credits.setVisible (true); credits.toFront (false); ripples.toFront (false); };
    addAndMakeVisible (logo);
    addChildComponent (credits);
    addAndMakeVisible (ripples);

    themeIndex = juce::jlimit (0, kNumThemes - 1, (int) proc.apvts.state.getProperty ("bg", 0));
    wallpaperMenu.selected = themeIndex;
    rebuildBackground();

    // l'ecran de choix n'apparait qu'au premier lancement (pas si un projet avec image est recharge)
    needChoice = ! proc.introAlreadyShown && ! proc.hasSeed();
    intro.setStage (IntroLayer::Intro);

    addMouseListener (this, true);   // gouttes d'eau pour tous les clics
    setSize (kW, kH);
    lastTime = juce::Time::getMillisecondCounterHiRes() * 0.001;
    refreshModules();
    refreshSeedInfo();
    startTimerHz (30);
}

AeroSeedAudioProcessorEditor::~AeroSeedAudioProcessorEditor()
{
    stopTimer();
    removeMouseListener (this);
    setLookAndFeel (nullptr);
}

//==============================================================================
void AeroSeedAudioProcessorEditor::resized()
{
    spectrum.setBounds (rSpectrum);
    viewImage.setBounds (rSpectrum.getX() + 14, rSpectrum.getBottom() - 50, 96, 38);
    viewSpectrum.setBounds (rSpectrum.getX() + 114, rSpectrum.getBottom() - 50, 110, 38);
    seedChip.setBounds (40, 34, 320, 70);
    wallpaperButton.setBounds (1052, 18, 190, 44);
    wallpaperMenu.setBounds (716, 66, 524, 200);
    for (int i = 0; i < 5; ++i) cards[(size_t) i]->setBounds (40 + i * 242, 414, 228, 110);
    syncCombo.setBounds (60, 576, 200, 42);
    triggerBar.setBounds (60, 630, 740, 48);
    mixKnob.setBounds (862, 562, 120, 120);
    editPanel.setBounds (getLocalBounds());
    intro.setBounds (getLocalBounds());
    credits.setBounds (getLocalBounds());
    ripples.setBounds (getLocalBounds());
    if (intro.getStage() == IntroLayer::Done) logo.setBounds (rLogoFinal);

    for (int i = 0; i < 5; ++i) anim.modules[(size_t) i] = cards[(size_t) i]->getBounds().toFloat();
    anim.bar = triggerBar.getBounds().toFloat();
}

void AeroSeedAudioProcessorEditor::setTheme (int index)
{
    themeIndex = juce::jlimit (0, kNumThemes - 1, index);
    proc.apvts.state.setProperty ("bg", themeIndex, nullptr);   // sauvegarde avec le projet
    wallpaperMenu.selected = themeIndex;
    rebuildBackground();
    repaint();
}

void AeroSeedAudioProcessorEditor::rebuildBackground()
{
    bgCache = juce::Image (juce::Image::RGB, kW, kH, false);
    juce::Graphics g (bgCache);
    if (themeIndex >= kFirstPhoto && photos[(size_t) (themeIndex - kFirstPhoto)].isValid())
        g.drawImage (photos[(size_t) (themeIndex - kFirstPhoto)], bgCache.getBounds().toFloat(), juce::RectanglePlacement::fillDestination);
    else
        paintProcedural (g, theme (juce::jmin (themeIndex, kFirstPhoto - 1)));
}

//==============================================================================
void AeroSeedAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.drawImageAt (bgCache, 0, 0);
    bubbles.paint (g);
    weather.paint (g);

    const bool light = theme (themeIndex).lightText || weather.storm > 0.5f;
    const auto txt = light ? juce::Colours::white : juce::Colour (0xd9000000);

    // panneaux du bas (apparition pendant l'ouverture)
    float pa = 1.0f, py = 0.0f;
    if (intro.getStage() == IntroLayer::Opening)
    {
        const float p = juce::jlimit (0.0f, 1.0f, (openTime - 1.75f) / 0.85f);
        pa = juce::jmin (1.0f, p * 1.6f);  py = (1.0f - easeOutBack (p)) * 54.0f;
    }
    if (pa > 0.0f)
    {
        g.saveState();
        g.addTransform (juce::AffineTransform::translation (0.0f, py));
        g.beginTransparencyLayer (pa);
        for (auto r : { rPanelA, rPanelB })
        {
            const auto rf = r.toFloat();
            g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.3f), 0.0f, rf.getY(), juce::Colours::white.withAlpha (0.1f), 0.0f, rf.getBottom(), false));
            g.fillRoundedRectangle (rf, 24.0f);
            g.setColour (juce::Colours::white.withAlpha (0.7f));
            g.drawRoundedRectangle (rf.reduced (0.75f), 24.0f, 1.5f);
        }
        g.setFont (uiFont (13.0f, juce::Font::bold));
        shadowText (g, "SYNCHRONISATION", { 62, 546, 300, 20 }, juce::Justification::centredLeft, txt, light);
        shadowText (g, "SORTIE", { 862, 546, 200, 20 }, juce::Justification::centredLeft, txt, light);
        g.setFont (uiFont (15.0f));
        shadowText (g, fr (L"Fen\u00eatre d'activation"), { 276, 586, 300, 22 }, juce::Justification::centredLeft, txt, light);

        if (proc.intValue ("SYNC_MODE") > 0)
        {
            const float ph = proc.getCyclePhase(), s = proc.value ("TRIG_START"), e = proc.value ("TRIG_END");
            const bool inside = ph < 0.0f || (s <= e ? (ph >= s && ph < e) : (ph >= s || ph < e));
            shadowText (g, inside ? "FX ON" : "FX OFF", { 600, 586, 176, 22 }, juce::Justification::centredRight, txt, light);
            g.setColour (inside ? juce::Colour (0xff27c160) : juce::Colours::white.withAlpha (0.4f));
            g.fillEllipse (784.0f, 591.0f, 12.0f, 12.0f);
        }
        g.setFont (uiFont (42.0f, juce::Font::bold));
        shadowText (g, juce::String (juce::roundToInt (proc.value ("MIX") * 100.0f)) + " %", { 1000, 576, 220, 50 }, juce::Justification::centredLeft, txt, light);
        g.setFont (uiFont (14.0f));
        shadowText (g, "Dry / Wet", { 1002, 628, 200, 20 }, juce::Justification::centredLeft, txt, light);
        g.endTransparencyLayer();
        g.restoreState();
    }

    if (intro.getStage() == IntroLayer::Done || intro.getStage() == IntroLayer::Opening)
    {
        g.setFont (uiFont (13.0f, juce::Font::bold));
        shadowText (g, fr (L"MODULES D'EFFETS  \u00b7  CLIQUE POUR \u00c9DITER"), { 46, 392, 500, 18 }, juce::Justification::centredLeft, txt, light);
    }

    if (isDragging)
    {
        g.setColour (juce::Colour (0xff1e88e5));
        g.drawRoundedRectangle (rSpectrum.toFloat().expanded (3.0f), 26.0f, 5.0f);
    }
}

//==============================================================================
void AeroSeedAudioProcessorEditor::timerCallback()
{
    const double now = juce::Time::getMillisecondCounterHiRes() * 0.001;
    const float dt = (float) juce::jlimit (0.0, 0.1, now - lastTime);
    lastTime = now;
    sinceOpen += dt;

    // --- logo : apparition (zoom avec rebond) puis deplacement a sa place
    if (intro.getStage() != IntroLayer::Done)
    {
        const float move = easeIO (juce::jlimit (0.0f, 1.0f, (sinceOpen - 1.6f) / 1.3f));
        const auto big = rLogoBig.toFloat(), fin = rLogoFinal.toFloat();
        const auto r = juce::Rectangle<float> (big.getX() + (fin.getX() - big.getX()) * move, big.getY() + (fin.getY() - big.getY()) * move,
                                               big.getWidth() + (fin.getWidth() - big.getWidth()) * move, big.getHeight() + (fin.getHeight() - big.getHeight()) * move);
        logo.setBounds (r.toNearestInt());
        const float pop = juce::jlimit (0.0f, 1.0f, sinceOpen / 1.1f);
        const float s = 0.2f + 0.8f * easeOutBack (pop);
        logo.setTransform (pop < 1.0f ? juce::AffineTransform::scale (s, s, r.getCentreX(), r.getCentreY()) : juce::AffineTransform());
        logo.setAlpha (juce::jlimit (0.0f, 1.0f, sinceOpen / 0.4f));
    }

    switch (intro.getStage())
    {
        case IntroLayer::Intro:
            intro.setTimes (sinceOpen, 0.0f, 0.0f);
            if (sinceOpen > 2.2f) { if (needChoice) { intro.setStage (IntroLayer::Choose); chooseTime = 0.0f; } else startOpening(); }
            break;
        case IntroLayer::Choose:
            chooseTime += dt;
            intro.setTimes (sinceOpen, 0.0f, chooseTime);
            break;
        case IntroLayer::Opening:
            openTime += dt;
            anim.update (openTime, dt);
            intro.setTimes (sinceOpen, openTime, 1.0f);
            applyEntrances (openTime);
            if (openTime > OpeningAnimation::total && sinceOpen > 2.9f) finishOpening();
            break;
        case IntroLayer::Done: break;
    }

    bubbles.update();
    weather.update (themeIndex < kFirstPhoto ? 1.0f - proc.value ("MIX") : 0.0f, dt);

    if (intro.getStage() == IntroLayer::Opening || intro.getStage() == IntroLayer::Done)
    {
        engine.update (proc, dt);
        spectrum.repaint();
    }

    const int v = proc.getThumbnailVersion();
    if (v != lastThumbVersion) { lastThumbVersion = v; refreshSeedInfo(); }

    triggerBar.setPlayhead (proc.getCyclePhase());
    const bool seq = proc.intValue ("SYNC_MODE") > 0;
    if (triggerBar.isEnabled() != seq) triggerBar.setEnabled (seq);

    if (++frameCounter % 3 == 0) refreshModules();
    ripples.tick();
    repaint();
}

void AeroSeedAudioProcessorEditor::startOpening()
{
    if (intro.getStage() == IntroLayer::Opening || intro.getStage() == IntroLayer::Done) return;
    proc.introAlreadyShown = true;
    openTime = 0.0f;
    anim.reset();
    intro.setStage (IntroLayer::Opening);
    applyEntrances (0.0f);
}

void AeroSeedAudioProcessorEditor::finishOpening()
{
    intro.setStage (IntroLayer::Done);
    logo.setBounds (rLogoFinal);
    logo.setTransform ({});
    logo.setAlpha (1.0f);
    applyEntrances (100.0f);   // tout a sa place, transformations retirees
}

// apparition de l'interface (meme minutage que la maquette)
void AeroSeedAudioProcessorEditor::applyEntrances (float t)
{
    enum Kind { Rise, SlideL, SlideR, Zoom };
    auto enter = [t] (juce::Component& c, float delay, float dur, Kind k)
    {
        const float p = juce::jlimit (0.0f, 1.0f, (t - delay) / dur);
        if (p >= 1.0f) { c.setTransform ({}); c.setAlpha (1.0f); return; }
        const float e = easeOutBack (p), inv = 1.0f - e;
        const auto b = c.getBounds().toFloat();
        juce::AffineTransform tf;
        switch (k)
        {
            case Rise:   tf = juce::AffineTransform::translation (0.0f, inv * 54.0f); break;
            case SlideL: tf = juce::AffineTransform::translation (-inv * 70.0f, 0.0f); break;
            case SlideR: tf = juce::AffineTransform::translation (inv * 70.0f, 0.0f); break;
            case Zoom:   tf = juce::AffineTransform::scale (0.93f + 0.07f * e, 0.93f + 0.07f * e, b.getCentreX(), b.getCentreY()).translated (0.0f, inv * 24.0f); break;
        }
        c.setTransform (tf);
        c.setAlpha (juce::jlimit (0.0f, 1.0f, p * 1.6f));
    };
    enter (spectrum, 0.85f, 0.95f, Zoom);
    enter (viewImage, 0.85f, 0.95f, Zoom);
    enter (viewSpectrum, 0.85f, 0.95f, Zoom);
    enter (seedChip, 1.0f, 0.85f, SlideL);
    enter (wallpaperButton, 1.1f, 0.85f, SlideR);
    for (int i = 0; i < 5; ++i) enter (*cards[(size_t) i], 1.05f + 0.13f * i, 0.8f, Rise);
    for (auto* c : { (juce::Component*) &syncCombo, (juce::Component*) &triggerBar, (juce::Component*) &mixKnob }) enter (*c, 1.75f, 0.85f, Rise);
}

//==============================================================================
void AeroSeedAudioProcessorEditor::refreshModules()
{
    auto txt = [this] (const char* id) { return proc.apvts.getParameter (id)->getCurrentValueAsText(); };
    const float cutoff = proc.value ("CUTOFF");
    const juce::String dot = fr (L" \u00b7 ");
    const bool mb = proc.value ("MB_ON") > 0.5f;

    cards[0]->setContent (proc.value ("H_ON") > 0.5f, txt ("H_KEY") + " " + txt ("H_SCALE"), "Morph " + juce::String (proc.intValue ("H_MORPH")) + " %");
    cards[1]->setContent (proc.value ("F_ON") > 0.5f, txt ("FILTER_TYPE"),
                          (cutoff >= 1000.0f ? juce::String (cutoff / 1000.0f, 1) + " kHz" : juce::String (juce::roundToInt (cutoff)) + " Hz") + dot + "Q " + juce::String (proc.value ("RESO"), 2));
    cards[2]->setContent (proc.value ("D_ON") > 0.5f, txt ("DIST_TYPE"), "Drive x" + juce::String (proc.value ("DRIVE"), 2));
    cards[3]->setContent (proc.value ("CRUSH_ON") > 0.5f, mb ? "Multibande" : juce::String (proc.intValue ("BITS")) + " bits",
                          mb ? juce::String (proc.intValue ("B1")) + " / " + juce::String (proc.intValue ("B2")) + " / " + juce::String (proc.intValue ("B3")) + " bits" : "Crush global");
    cards[4]->setContent (proc.value ("C_ON") > 0.5f, juce::String (proc.value ("CH_RATE"), 2) + " Hz", "Mix " + juce::String (proc.intValue ("CH_MIX")) + " %");
}

void AeroSeedAudioProcessorEditor::refreshSeedInfo()
{
    const auto thumb = proc.getThumbnail();
    spectrum.background = thumb;
    spectrum.repaint();
    editPanel.setSpectrumImage (thumb);
    if (proc.hasSeed())
        seedChip.set (thumb, "Seed " + juce::String::toHexString ((juce::int64) proc.getSeed()).paddedLeft ('0', 16).substring (0, 8).toUpperCase(),
                      "Clique pour changer d'image");
    else
        seedChip.set ({}, "Aucune image", "Glisse une image sur le spectre");
}

void AeroSeedAudioProcessorEditor::openEditor (int tab)
{
    wallpaperMenu.setVisible (false);
    editPanel.setBlurredBackground (createComponentSnapshot (getLocalBounds(), true, 1.0f));
    editPanel.showTab (tab);
    editPanel.setVisible (true);
    editPanel.toFront (false);
    logo.toFront (false);
    credits.toFront (false);
    ripples.toFront (false);
}

//==============================================================================
void AeroSeedAudioProcessorEditor::chooseImageFile()
{
    chooser = std::make_unique<juce::FileChooser> ("Choisis une image", juce::File(), "*.png;*.jpg;*.jpeg");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [this] (const juce::FileChooser& fc) { const auto f = fc.getResult(); if (f.existsAsFile()) loadImageFile (f); });
}

void AeroSeedAudioProcessorEditor::loadImageFile (const juce::File& file)
{
    const auto tooBigMsg = fr (L"L'image s\u00e9lectionn\u00e9e est trop grande.\n\nLa taille maximale autoris\u00e9e est de 3840 sur 2160 pixels (qualit\u00e9 4K).");
    if (file.getSize() > maxFileBytes) { alert ("Image trop volumineuse", tooBigMsg); return; }
    int w = 0, h = 0;
    if (peekImageSize (file, w, h) && tooBig (w, h)) { alert ("Image trop volumineuse", tooBigMsg); return; }   // avant tout decodage

    const auto img = juce::ImageFileFormat::loadFrom (file);
    if (! img.isValid()) { alert ("Image illisible", fr (L"Ce fichier n'a pas pu \u00eatre lu comme une image.")); return; }
    if (tooBig (img.getWidth(), img.getHeight())) { alert ("Image trop volumineuse", tooBigMsg); return; }

    proc.generatePatchFromImage (img);
    refreshSeedInfo();
    refreshModules();
    if (intro.getStage() == IntroLayer::Choose) startOpening();
}

bool AeroSeedAudioProcessorEditor::isInterestedInFileDrag (const juce::StringArray& files)
{
    if (intro.getStage() == IntroLayer::Intro || intro.getStage() == IntroLayer::Opening) return false;
    for (auto& f : files) if (hasImageExtension (f)) return true;
    return false;
}
void AeroSeedAudioProcessorEditor::fileDragEnter (const juce::StringArray&, int, int) { isDragging = true;  repaint(); }
void AeroSeedAudioProcessorEditor::fileDragExit (const juce::StringArray&)            { isDragging = false; repaint(); }

void AeroSeedAudioProcessorEditor::filesDropped (const juce::StringArray& files, int x, int y)
{
    isDragging = false;
    ripples.addRipple ({ (float) x, (float) y }, false);
    for (auto& path : files)
        if (hasImageExtension (path)) { loadImageFile (juce::File (path)); break; }
}

//==============================================================================
void AeroSeedAudioProcessorEditor::mouseDown (const juce::MouseEvent& e)
{
    ripples.addRipple (e.getEventRelativeTo (this).position, false);

    if (wallpaperMenu.isVisible())   // clic en dehors du menu des fonds : on le ferme
    {
        auto* c = e.originalComponent;
        const bool inMenu = c == &wallpaperMenu || wallpaperMenu.isParentOf (c) || c == &wallpaperButton;
        if (! inMenu) wallpaperMenu.setVisible (false);
    }
}

void AeroSeedAudioProcessorEditor::mouseDrag (const juce::MouseEvent& e)
{
    const auto nowMs = juce::Time::getMillisecondCounter();
    if (nowMs - lastTrailMs < 130) return;
    if (e.originalComponent != nullptr && (e.originalComponent == &editPanel || editPanel.isParentOf (e.originalComponent))) return;
    lastTrailMs = nowMs;
    ripples.addRipple (e.getEventRelativeTo (this).position, true);
}
