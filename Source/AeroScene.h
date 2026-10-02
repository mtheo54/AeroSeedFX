#pragma once
#include <JuceHeader.h>
#include "AeroUI.h"

// Decor : fonds d'ecran, bulles, meteo (Dry/Wet) et animation d'ouverture
namespace aero
{
constexpr int kW = 1280, kH = 720;

//==============================================================================
// FONDS : 5 dessines par le code + 3 photos
struct ThemeDef
{
    const wchar_t* name;
    juce::uint32 g[4];
    bool diagonal, clouds;  juce::uint32 cloudCol;
    bool hills;             juce::uint32 h1a, h1b, h2a, h2b;
    float sunX, sunY, sunR; juce::uint32 sunCol;
    bool rays, stars, lightText;
};

constexpr int kNumThemes = 8, kFirstPhoto = 5;

inline const ThemeDef& theme (int i)
{
    static const ThemeDef t[kNumThemes] =
    {
        { L"Ciel",   { 0xff2f9be6, 0xff7fcdf3, 0xffbfe6f7, 0xffd9f2fb }, false, true, 0xffffffff, true, 0xff7ed957, 0xff2e9e2f, 0xff9be564, 0xff3aa83a, 130, 60, 420, 0xffffffff, false, false, false },
        { L"Oc\u00e9an", { 0xff08457f, 0xff1599c9, 0xff3fc0dc, 0xff6fe0e8 }, false, false, 0, true, 0xfff4e6b0, 0xffd4b86c, 0xfff4e6b0, 0xffd4b86c, 0, 0, 0, 0, true, false, true },
        { L"Aurora", { 0xff087a55, 0xff2fc09a, 0xff8fe6c4, 0xffc4f7e0 }, true, false, 0, false, 0, 0, 0, 0, 820, -40, 420, 0xffffffff, false, false, false },
        { L"Coucher de soleil", { 0xff4b3a9c, 0xffd9507c, 0xffff9f5e, 0xffffe2a0 }, false, true, 0xffffc0cc, true, 0xff6d8f3f, 0xff2b5a30, 0xff6d8f3f, 0xff2b5a30, 250, 520, 330, 0xffffc878, false, false, true },
        { L"Nuit",   { 0xff050d2e, 0xff10306b, 0xff144a8c, 0xff1b62a8 }, false, false, 0, false, 0, 0, 0, 0, 1020, 130, 130, 0xfffffbe0, false, true, true },
        { L"Colline",     { 0xff3b7fd4, 0xff3b7fd4, 0xff7cc04a, 0xff4e9a2a }, false, false, 0, false, 0, 0, 0, 0, 0, 0, 0, 0, false, false, true },
        { L"Terre & eau", { 0xff2f8fe0, 0xff2f8fe0, 0xff29b6f6, 0xff0277bd }, false, false, 0, false, 0, 0, 0, 0, 0, 0, 0, 0, false, false, true },
        { L"Dauphins",    { 0xff1565c0, 0xff1565c0, 0xff0d47a1, 0xff0a2a66 }, false, false, 0, false, 0, 0, 0, 0, 0, 0, 0, 0, false, false, true },
    };
    return t[juce::jlimit (0, kNumThemes - 1, i)];
}

inline void paintProcedural (juce::Graphics& g, const ThemeDef& d)
{
    const float W = (float) kW, H = (float) kH;
    juce::ColourGradient sky (juce::Colour (d.g[0]), 0.0f, 0.0f, juce::Colour (d.g[3]), d.diagonal ? W : 0.0f, H, false);
    sky.addColour (0.4, juce::Colour (d.g[1]));  sky.addColour (0.7, juce::Colour (d.g[2]));
    g.setGradientFill (sky);  g.fillAll();

    if (d.sunR > 0.0f)
    {
        const juce::Colour c (d.sunCol);
        juce::ColourGradient sun (c.withAlpha (0.95f), d.sunX, d.sunY, c.withAlpha (0.0f), d.sunX + d.sunR, d.sunY, true);
        sun.addColour (0.38, c.withAlpha (0.35f));
        g.setGradientFill (sun);  g.fillAll();
    }
    if (d.rays)
    {
        g.setColour (juce::Colours::white.withAlpha (0.14f));
        for (int i = 0; i < 10; ++i)
        {
            const float a = -0.5f + i * 0.14f;
            juce::Path p;
            p.addTriangle (400.0f, -80.0f, 400.0f + std::sin (a) * 1500.0f, -80.0f + std::cos (a) * 1500.0f,
                           400.0f + std::sin (a + 0.06f) * 1500.0f, -80.0f + std::cos (a + 0.06f) * 1500.0f);
            g.fillPath (p);
        }
    }
    if (d.stars)
    {
        juce::Random r (42);
        for (int i = 0; i < 110; ++i)
        {
            const float s = 1.0f + r.nextFloat() * 2.0f;
            g.setColour (juce::Colours::white.withAlpha (0.5f + r.nextFloat() * 0.5f));
            g.fillEllipse (r.nextFloat() * W, r.nextFloat() * H * 0.75f, s, s);
        }
    }
    {   // grand reflet glossy
        const auto tf = juce::AffineTransform::rotation (-0.14f, 468.0f, 504.0f);
        juce::Path e; e.addEllipse (-216.0f, 270.0f, 1368.0f, 468.0f);
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (d.diagonal ? 0.4f : 0.28f), 0.0f, 270.0f, juce::Colours::white.withAlpha (0.0f), 0.0f, 550.0f, false));
        g.fillPath (e, tf);
        juce::Path arc; arc.addCentredArc (468.0f, 504.0f, 684.0f, 234.0f, 0.0f, -juce::MathConstants<float>::halfPi, juce::MathConstants<float>::halfPi, true);
        g.setColour (juce::Colours::white.withAlpha (0.55f));
        g.strokePath (arc, juce::PathStrokeType (3.0f), tf);
    }
    if (d.clouds)
    {
        const float cl[][4] = { { 540, 54, 200, 0.92f }, { 36, 270, 160, 0.7f }, { 684, 340, 150, 0.6f }, { 936, 90, 180, 0.85f }, { 1100, 360, 140, 0.6f } };
        for (auto& c : cl)
        {
            g.setColour (juce::Colour (d.cloudCol).withMultipliedAlpha (c[3]));
            g.fillRoundedRectangle (c[0], c[1], c[2], 47.0f, 23.5f);
            g.fillEllipse (c[0] + 32.0f, c[1] - 40.0f, 80.0f, 80.0f);
            g.fillEllipse (c[0] + 100.0f, c[1] - 25.0f, 58.0f, 58.0f);
        }
    }
    if (d.hills)
    {
        auto hill = [&] (float x, float y, float w, float h, juce::uint32 a, juce::uint32 b)
        { g.setGradientFill (juce::ColourGradient (juce::Colour (a), 0.0f, y, juce::Colour (b), 0.0f, y + h * 0.5f, false)); g.fillEllipse (x, y, w, h); };
        hill (-144.0f, 558.0f, 756.0f, 540.0f, d.h1a, d.h1b);
        hill (632.0f, 594.0f, 828.0f, 500.0f, d.h2a, d.h2b);
    }
}

//==============================================================================
// BULLES qui montent
struct Bubbles
{
    struct B { float x, y, r, speed, phase; };
    std::vector<B> list;
    juce::Random rng;

    Bubbles() { for (int i = 0; i < 14; ++i) list.push_back ({ rng.nextFloat() * kW, rng.nextFloat() * kH, 12.0f + rng.nextFloat() * 46.0f, 0.8f + rng.nextFloat() * 0.9f, rng.nextFloat() * 6.28f }); }

    void update()
    {
        for (auto& b : list)
        {
            b.y -= b.speed;  b.x += std::sin (b.phase + b.y * 0.01f) * 0.3f;
            if (b.y < -b.r * 2.0f) { b.y = kH + b.r; b.x = rng.nextFloat() * kW; }
        }
    }

    void paint (juce::Graphics& g) const
    {
        for (auto& b : list)
        {
            const auto r = juce::Rectangle<float> (b.r * 2.0f, b.r * 2.0f).withCentre ({ b.x, b.y });
            juce::ColourGradient grad (juce::Colours::white.withAlpha (0.55f), r.getX() + b.r * 0.6f, r.getY() + b.r * 0.5f, juce::Colour (0x666ed7ff), r.getRight(), r.getBottom(), true);
            grad.addColour (0.5, juce::Colours::white.withAlpha (0.06f));
            g.setGradientFill (grad);  g.fillEllipse (r);
            g.setColour (juce::Colours::white.withAlpha (0.6f));  g.drawEllipse (r, 1.0f);
        }
    }
};

//==============================================================================
// METEO : plus le Dry/Wet est bas, plus il pleut (seulement sur les fonds dessines par le code)
struct Weather
{
    juce::Image clouds;
    struct Drop { float x, y, v, l; };
    std::vector<Drop> drops;
    juce::Random rng;
    float storm = 0.0f, offset = 0.0f, flash = 0.0f;

    Weather() : clouds (juce::Image::ARGB, kW, 300, true)
    {
        juce::Graphics g (clouds);
        struct P { float x, y, r; };
        std::vector<P> puffs;
        for (int i = 0; i < 9; ++i)
        {
            const float x = i * 150.0f + 40.0f, y = 50.0f + LivingSlider::hf (i, 7) * 60.0f, r = 62.0f + LivingSlider::hf (i, 8) * 44.0f;
            for (int k = 0; k < 7; ++k) puffs.push_back ({ x + (k - 3) * r * 0.55f, y + std::sin (k * 1.7f + i) * r * 0.25f, r * (0.55f + 0.3f * LivingSlider::hf (k, i + 11)) });
        }
        for (float dx : { -(float) kW, 0.0f, (float) kW })   // tuile qui boucle horizontalement
            for (auto& q : puffs)
            {
                const float X = q.x + dx;
                if (X < -q.r * 2 || X > kW + q.r * 2) continue;
                g.setGradientFill (juce::ColourGradient (juce::Colour (0xfa5c667c), X, q.y - q.r * 0.25f, juce::Colour (0xf5262c3c), X + q.r, q.y, true));
                g.fillEllipse (X - q.r, q.y - q.r, q.r * 2, q.r * 2);
            }
        for (float dx : { -(float) kW, 0.0f, (float) kW })
            for (auto& q : puffs)
            {
                const float X = q.x + dx;
                g.setColour (juce::Colour (0x33a5afc8));
                g.fillEllipse (X - q.r * 0.52f, q.y - q.r * 0.38f - q.r * 0.52f, q.r * 1.04f, q.r * 1.04f);
            }
        for (int i = 0; i < 380; ++i) drops.push_back ({ rng.nextFloat() * 1400.0f, rng.nextFloat() * kH, 850.0f + rng.nextFloat() * 520.0f, 14.0f + rng.nextFloat() * 16.0f });
    }

    bool isActive() const { return storm > 0.004f; }

    void update (float target, float dt)
    {
        storm += (target - storm) * 0.12f;
        offset = std::fmod (offset + 10.0f * dt, (float) kW);
        const int n = (int) std::round (storm * drops.size());
        for (int i = 0; i < n; ++i)
        {
            auto& d = drops[(size_t) i];
            d.y += d.v * dt;  d.x -= d.v * dt * 0.12f;
            if (d.y > kH + 20) { d.y = -20.0f; d.x = rng.nextFloat() * 1400.0f; }
        }
        if (storm > 0.7f && flash <= 0.02f && rng.nextFloat() < 0.008f) flash = 1.0f;   // eclair
        flash *= 0.8f;
    }

    void paint (juce::Graphics& g) const
    {
        if (! isActive()) return;
        g.setColour (juce::Colour::fromFloatRGBA (0.055f, 0.086f, 0.157f, storm * 0.42f));
        g.fillAll();
        const float ca = juce::jmin (1.0f, storm * 1.4f);
        g.setOpacity (ca * 0.55f);
        g.drawImageAt (clouds, (int) (-offset * 0.55f), -100);  g.drawImageAt (clouds, (int) (kW - offset * 0.55f), -100);
        g.setOpacity (ca);
        g.drawImageAt (clouds, (int) -offset, -40);              g.drawImageAt (clouds, (int) (kW - offset), -40);
        g.setOpacity (1.0f);

        juce::Path rain;
        const int n = (int) std::round (storm * drops.size());
        for (int i = 0; i < n; ++i) { const auto& d = drops[(size_t) i]; rain.startNewSubPath (d.x, d.y); rain.lineTo (d.x + d.l * 0.12f, d.y - d.l); }
        g.setColour (juce::Colour (0x99cde1ff));
        g.strokePath (rain, juce::PathStrokeType (1.6f));

        if (flash > 0.02f) { g.setColour (juce::Colour::fromFloatRGBA (0.92f, 0.95f, 1.0f, flash * 0.45f)); g.fillAll(); }
    }
};

//==============================================================================
// ANIMATION D'OUVERTURE : vague d'eau qui revele l'interface, puis un element par module, puis un eclair
struct OpeningAnimation
{
    static constexpr float waveDuration = 1.25f, total = 3.4f;
    struct Drop { float x, y, vx, vy, r, age, life; };
    std::vector<Drop> drops;
    juce::Random rng;
    std::array<juce::Rectangle<float>, 5> modules;   // positions des cartes (fournies par l'editeur)
    juce::Rectangle<float> bar;                      // barre de trigger

    static float easeIO (float t) { return t < 0.5f ? 4 * t * t * t : 1.0f - std::pow (-2.0f * t + 2.0f, 3.0f) / 2.0f; }
    static float crest (float x, float t)
    {
        return 790.0f - 880.0f * easeIO (juce::jmin (1.0f, t / waveDuration)) + std::sin (x * 0.012f + t * 7.0f) * 24.0f + std::sin (x * 0.031f - t * 5.0f) * 10.0f;
    }

    // zone encore noire (au-dessus de la crete)
    static juce::Path veil (float t)
    {
        juce::Path p;
        p.startNewSubPath (0.0f, 0.0f);  p.lineTo ((float) kW, 0.0f);
        for (int x = kW; x >= 0; x -= 16) p.lineTo ((float) x, crest ((float) x, t));
        p.closeSubPath();
        return p;
    }

    void reset() { drops.clear(); }

    void update (float t, float dt)
    {
        if (t < waveDuration + 0.1f)
            for (int k = 0; k < 6; ++k)
            {
                const float x = rng.nextFloat() * kW;
                drops.push_back ({ x, crest (x, t), (rng.nextFloat() - 0.5f) * 170.0f, -(220.0f + rng.nextFloat() * 430.0f), 2.0f + rng.nextFloat() * 4.0f, 0.0f, 0.9f + rng.nextFloat() * 0.8f });
            }
        for (size_t i = drops.size(); i-- > 0;)
        {
            auto& d = drops[i];
            d.age += dt;  d.vy += 900.0f * dt;  d.x += d.vx * dt;  d.y += d.vy * dt;
            if (d.age > d.life) drops.erase (drops.begin() + (long) i);
        }
    }

    void paint (juce::Graphics& g, float t) const
    {
        if (t < waveDuration + 0.35f)   // corps d'eau + ecume
        {
            const float base = 790.0f - 880.0f * easeIO (juce::jmin (1.0f, t / waveDuration));
            juce::Path water;
            water.startNewSubPath (0.0f, crest (0.0f, t));
            for (int x = 0; x <= kW; x += 8) water.lineTo ((float) x, crest ((float) x, t));
            water.lineTo ((float) kW, base + 190.0f);  water.lineTo (0.0f, base + 190.0f);  water.closeSubPath();
            juce::ColourGradient wg (juce::Colour::fromFloatRGBA (0.78f, 0.97f, 1.0f, 0.96f), 0.0f, base - 30.0f, juce::Colour::fromFloatRGBA (0.08f, 0.47f, 0.86f, 0.0f), 0.0f, base + 180.0f, false);
            wg.addColour (0.25, juce::Colour::fromFloatRGBA (0.27f, 0.75f, 1.0f, 0.72f));
            g.setGradientFill (wg);  g.fillPath (water);

            juce::Path foam;
            foam.startNewSubPath (0.0f, crest (0.0f, t));
            for (int x = 8; x <= kW; x += 8) foam.lineTo ((float) x, crest ((float) x, t));
            g.setColour (juce::Colours::white.withAlpha (0.28f));  g.strokePath (foam, juce::PathStrokeType (20.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.setColour (juce::Colours::white.withAlpha (0.95f));  g.strokePath (foam, juce::PathStrokeType (6.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
        for (auto& d : drops)
        {
            g.setColour (juce::Colour::fromFloatRGBA (0.92f, 0.98f, 1.0f, (1.0f - d.age / d.life) * 0.95f));
            g.fillEllipse (d.x - d.r, d.y - d.r, d.r * 2, d.r * 2);
        }
        static const float starts[5] = { 1.05f, 1.18f, 1.31f, 1.44f, 1.57f };
        for (int i = 0; i < 5; ++i) burst (g, i, t, starts[i]);

        if (t > 2.0f && t < 2.8f && ! bar.isEmpty())   // eclair sur la barre de trigger
        {
            const float p = juce::jmin (1.0f, (t - 2.0f) / 0.5f), xe = bar.getX() + bar.getWidth() * p, y = bar.getCentreY();
            const int sd = (int) std::floor (t * 22.0f);
            juce::Path bolt; bolt.startNewSubPath (bar.getX(), y);
            int k = 0;
            for (float x = bar.getX() + 12.0f; x < xe; x += 12.0f, ++k) bolt.lineTo (x, y + (LivingSlider::hf (k, sd) - 0.5f) * 30.0f);
            bolt.lineTo (xe, y);
            const float a = t > 2.65f ? (2.8f - t) / 0.15f : 1.0f;
            g.setColour (juce::Colour (0x8c00e5ff).withMultipliedAlpha (a));  g.strokePath (bolt, juce::PathStrokeType (9.0f));
            g.setColour (juce::Colours::white.withAlpha (a));                   g.strokePath (bolt, juce::PathStrokeType (2.4f));
        }
    }

private:
    static void star4 (juce::Graphics& g, float x, float y, float r, juce::Colour c)
    {
        juce::Path p;
        for (int a = 0; a < 8; ++a)
        {
            const float rr = (a % 2) ? r * 0.35f : r, an = a * juce::MathConstants<float>::pi / 4.0f;
            if (a == 0) p.startNewSubPath (x + std::cos (an) * rr, y + std::sin (an) * rr); else p.lineTo (x + std::cos (an) * rr, y + std::sin (an) * rr);
        }
        p.closeSubPath();
        g.setColour (c);  g.fillPath (p);
    }

    void burst (juce::Graphics& g, int i, float t, float start) const
    {
        const float u = (t - start) / 1.8f;
        if (u < 0.0f || u > 1.0f) return;
        const auto& m = modules[(size_t) i];
        if (m.isEmpty()) return;
        const float cx = m.getCentreX(), y0 = m.getY(), half = m.getWidth() * 0.5f - 4.0f;
        const float f = u < 0.15f ? u / 0.15f : (u > 0.65f ? (1.0f - u) / 0.35f : 1.0f);
        auto hf = LivingSlider::hf;
        g.beginTransparencyLayer (f);   // (setOpacity serait annule par chaque setColour)
        switch (i)
        {
            case 0:   // Harmonique : etincelles dorees + notes
                for (int k = 0; k < 14; ++k)
                {
                    const float tw = 0.5f + 0.5f * std::sin (t * 12.0f + k * 3.0f);
                    star4 (g, cx + (hf (k, 1) - 0.5f) * half * 1.8f, y0 - 8.0f - u * (60.0f + hf (k, 2) * 110.0f), (4.0f + hf (k, 3) * 7.0f) * tw + 1.0f, juce::Colour (0xffffe082));
                }
                for (int k = 0; k < 4; ++k)
                    note (g, cx - 92.0f + k * 52.0f + std::sin (t * 3.0f + k) * 6.0f, y0 - 40.0f - u * (70.0f + k * 14.0f), k % 2 == 1, juce::Colour (0xffffd54f));
                break;

            case 1:   // Filtre : une liane qui pousse
            {
                const float gr = juce::jmin (1.0f, u * 2.4f), L = cx - half, xe = L + gr * half * 2.0f;
                auto sy = [&] (float x) { return y0 - 4.0f - std::sin (x * 0.08f + t * 2.0f) * 2.0f; };
                juce::Path stem; stem.startNewSubPath (L, sy (L));
                for (float x = L + 4.0f; x <= xe; x += 4.0f) stem.lineTo (x, sy (x));
                g.setColour (juce::Colour (0xff2e7d32));
                g.strokePath (stem, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
                int k = 0;
                for (float x = L + 10.0f; x < xe; x += 20.0f, ++k)
                {
                    const float z = 15.0f * juce::jmin (1.0f, (xe - x) / 50.0f + 0.25f), sg = k % 2 ? -1.0f : 1.0f;
                    juce::Path leaf; leaf.addEllipse (-z * 0.45f, -z * 1.7f, z * 0.9f, z * 2.0f);
                    g.setColour (k % 2 ? juce::Colour (0xff66bb6a) : juce::Colour (0xff9be564));
                    g.fillPath (leaf, juce::AffineTransform::rotation (sg * (0.9f + std::sin (t * 2.0f + k) * 0.12f)).translated (x, sy (x)));
                }
                if (gr > 0.95f)
                    for (int p = 0; p < 5; ++p) { g.setColour (juce::Colour (0xffff80ab)); g.fillEllipse (xe + std::cos (p * 1.26f + t) * 8.0f - 5.0f, sy (xe) - 8.0f + std::sin (p * 1.26f + t) * 8.0f - 5.0f, 10.0f, 10.0f); }
                break;
            }

            case 2:   // Distortion : flamme + braises
            {
                const float env = std::sin (juce::jmin (1.0f, u * 1.15f) * juce::MathConstants<float>::pi);
                for (float x = cx - half; x <= cx + half; x += 5.0f)
                {
                    const float n = (std::sin (x * 0.7f + t * 11.0f) + std::sin (x * 1.9f - t * 14.0f) + 2.0f) / 4.0f, h = (10.0f + 44.0f * n) * env;
                    LivingSlider::flame (g, x, y0 + 2.0f, h, 6.0f, juce::Colour::fromFloatRGBA (1.0f, 0.34f, 0.13f, 0.92f));
                    LivingSlider::flame (g, x, y0 + 2.0f, h * 0.6f, 3.5f, juce::Colour::fromFloatRGBA (1.0f, 0.92f, 0.23f, 0.95f));
                }
                g.setColour (juce::Colour (0xf2ffbe46));
                for (int k = 0; k < 10; ++k) g.fillEllipse (cx + (hf (k, 5) - 0.5f) * half * 1.8f - 2.2f, y0 - u * (80.0f + hf (k, 6) * 100.0f) - 2.2f, 4.4f, 4.4f);
                break;
            }

            case 3:   // Bitcrusher : cristaux de glace
            {
                const float gw = juce::jmin (1.0f, u * 3.0f);
                int k = 0;
                for (float x = cx - half; x <= cx + half; x += 14.0f, ++k)
                {
                    const float h = (12.0f + hf (k, 3) * 32.0f) * gw;
                    juce::Path tri; tri.addTriangle (x - 7.0f, y0 + 2.0f, x, y0 + 2.0f - h, x + 7.0f, y0 + 2.0f);
                    g.setColour (juce::Colour (0xf2c8f0ff));  g.fillPath (tri);
                    g.setColour (juce::Colours::white);       g.strokePath (tri, juce::PathStrokeType (1.2f));
                    if (hf (k, (int) std::floor (t * 4.0f)) > 0.82f) star4 (g, x, y0 - h - 6.0f, 6.0f, juce::Colours::white);
                }
                break;
            }

            default:  // Chorus : fontaine
                for (int k = 0; k < 20; ++k)
                {
                    const float q = std::fmod (t * 1.4f + k / 20.0f, 1.0f), sg = k % 2 ? 1.0f : -1.0f, r = 3.4f * (1.0f - q * 0.4f);
                    g.setColour (juce::Colour::fromFloatRGBA (0.75f, 0.92f, 1.0f, 1.0f - q * 0.55f));
                    g.fillEllipse (cx + sg * hf (k, 3) * 78.0f * q - r, y0 - std::sin (q * juce::MathConstants<float>::pi) * (70.0f + hf (k, 4) * 60.0f) - r, r * 2, r * 2);
                }
                g.setColour (juce::Colour (0x8c78c8ff));
                g.fillEllipse (cx - 70.0f * std::sin (u * juce::MathConstants<float>::pi), y0 - 4.0f, 140.0f * std::sin (u * juce::MathConstants<float>::pi), 12.0f);
                break;
        }
        g.endTransparencyLayer();
    }

    static void note (juce::Graphics& g, float x, float y, bool doubleNote, juce::Colour c)
    {
        g.setColour (c);
        g.fillEllipse (x, y + 16.0f, 11.0f, 8.0f);
        g.fillRect (x + 9.0f, y, 2.2f, 20.0f);
        if (doubleNote) { g.fillEllipse (x + 16.0f, y + 12.0f, 11.0f, 8.0f); g.fillRect (x + 25.0f, y - 4.0f, 2.2f, 20.0f); g.fillRect (x + 9.0f, y, 18.2f, 4.0f); }
        else            { juce::Path f; f.startNewSubPath (x + 11.0f, y); f.quadraticTo (x + 20.0f, y + 6.0f, x + 15.0f, y + 13.0f); g.strokePath (f, juce::PathStrokeType (2.2f)); }
    }
};

} // namespace aero
