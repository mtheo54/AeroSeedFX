// Tests du coeur DSP (hors JUCE) :  g++ -O2 -std=c++17 DSPTests.cpp -o tests && ./tests
#include "../Source/AeroDSP.h"
#include "../Source/AeroMood.h"
#include <cstdio>
#include <random>

using namespace aerodsp;
static int failures = 0;
#define CHECK(cond, ...) do { if (cond) std::printf ("  OK   " __VA_ARGS__); else { std::printf ("  FAIL " __VA_ARGS__); ++failures; } std::printf ("\n"); } while (0)

// frequence dominante (recherche fine par Goertzel) dans [lo, hi]
static double dominantFreq (const std::vector<float>& x, double sr, double lo, double hi)
{
    double bestF = 0, bestP = -1;
    for (double f = lo; f <= hi; f += 0.25)
    {
        const double w = 2 * kPi * f / sr, c = 2 * std::cos (w);
        double s1 = 0, s2 = 0;
        for (float v : x) { const double s0 = v + c * s1 - s2; s2 = s1; s1 = s0; }
        const double p = s1 * s1 + s2 * s2 - c * s1 * s2;
        if (p > bestP) { bestP = p; bestF = f; }
    }
    return bestF;
}

int main()
{
    const double sr = 48000.0;

    std::printf ("[1] FFT aller-retour\n");
    {
        FFT fft (10);
        std::vector<std::complex<float>> a (1024), b;
        std::mt19937 rng (1); std::uniform_real_distribution<float> d (-1, 1);
        for (auto& v : a) v = { d (rng), 0 };
        b = a; fft.forward (b.data()); fft.inverse (b.data());
        double err = 0; for (size_t i = 0; i < a.size(); ++i) err = std::max (err, (double) std::abs (b[i] / 1024.0f - a[i]));
        CHECK (err < 1e-5, "erreur max %.2e", err);
    }

    std::printf ("[2] Harmoniseur, Morph = 0 : sortie = entree retardee (reconstruction parfaite)\n");
    {
        SpectralHarmonizer h; h.prepare (sr); h.setParameters (maskFor (0, 1), 0.0f, -80.0f);
        const int L = h.latency(), n = 48000;
        std::mt19937 rng (2); std::uniform_real_distribution<float> d (-0.5f, 0.5f);
        std::vector<float> in (n), out (n);
        for (int i = 0; i < n; ++i) { in[i] = d (rng); out[i] = h.process (in[i]); }
        double err = 0, ref = 0;
        for (int i = 4096; i < n; ++i) { err = std::max (err, (double) std::abs (out[i] - in[i - L])); ref = std::max (ref, (double) std::abs (in[i])); }
        CHECK (err / ref < 1e-3, "latence %d echantillons, erreur relative max %.2e", L, err / ref);
    }

    std::printf ("[3] Harmoniseur, Morph = 100 %% : 460 Hz en Do majeur -> La 440 Hz\n");
    {
        SpectralHarmonizer h; h.prepare (sr); h.setParameters (maskFor (0, 1), 1.0f, -80.0f);
        const int n = 96000; std::vector<float> out;
        for (int i = 0; i < n; ++i) { const float y = h.process (0.5f * (float) std::sin (2 * kPi * 460.0 * i / sr)); if (i > n - 24000) out.push_back (y); }
        const double f = dominantFreq (out, sr, 400, 520);
        float pk = 0; for (float v : out) pk = std::max (pk, std::abs (v));
        CHECK (std::abs (f - 440.0) < 1.5, "frequence de sortie %.2f Hz (amplitude crete %.2f)", f, pk);
        CHECK (pk > 0.45f && pk < 0.56f, "niveau conserve (entree 0.50)");
    }

    std::printf ("[4] Harmoniseur, Morph = 100 %% : 440 Hz deja dans la gamme -> inchange\n");
    {
        SpectralHarmonizer h; h.prepare (sr); h.setParameters (maskFor (0, 1), 1.0f, -80.0f);
        const int n = 96000; std::vector<float> out;
        for (int i = 0; i < n; ++i) { const float y = h.process (0.5f * (float) std::sin (2 * kPi * 440.0 * i / sr)); if (i > n - 24000) out.push_back (y); }
        const double f = dominantFreq (out, sr, 400, 520);
        CHECK (std::abs (f - 440.0) < 1.0, "frequence de sortie %.2f Hz", f);
    }

    std::printf ("[5] Spectral Gate : un signal sous le seuil est supprime (seulement avec Morph)\n");
    {
        auto run = [&] (float morph, float gate)
        {
            SpectralHarmonizer h; h.prepare (sr); h.setParameters (maskFor (0, 1), morph, gate);
            double e = 0; const int n = 48000;
            for (int i = 0; i < n; ++i) { const float y = h.process (0.003f * (float) std::sin (2 * kPi * 440.0 * i / sr)); if (i > n / 2) e += y * y; }
            return std::sqrt (e / (n / 2));
        };
        const double open = run (1.0f, -80.0f), closed = run (1.0f, -30.0f), dry = run (0.0f, -30.0f);
        CHECK (closed < open * 0.05, "gate ferme : %.2e contre %.2e", closed, open);
        CHECK (std::abs (dry - open) / open < 0.05, "Morph 0 : le gate n'agit pas (%.2e)", dry);
    }

    std::printf ("[6] Transitoire : une attaque brutale garde le son d'origine\n");
    {
        SpectralHarmonizer h; h.prepare (sr); h.setParameters (maskFor (0, 1), 1.0f, -80.0f);
        float maxT = 0, pk = 0;
        for (int i = 0; i < 48000; ++i)
        {
            const float x = (i >= 24000 && i < 24100) ? 0.8f : 0.0f;   // clic
            pk = std::max (pk, std::abs (h.process (x)));
            maxT = std::max (maxT, h.lastTransient());
        }
        CHECK (maxT > 0.99f, "transitoire detecte (%.2f)", maxT);
        CHECK (pk < 1.5f, "pas d'emballement (crete %.2f)", pk);
    }

    std::printf ("[7] Crossover 3 bandes LR4 : somme plate\n");
    {
        Crossover3 x; x.prepare (sr); x.setFrequencies (200, 2500);
        const int n = 1 << 14; std::vector<std::complex<float>> s (n);
        for (int i = 0; i < n; ++i) { float l, m, h; x.process (i == 0 ? 1.0f : 0.0f, l, m, h); s[i] = { l + m + h, 0 }; }
        FFT f (14); f.forward (s.data());
        double lo = 1e9, hi = -1e9;
        for (int k = 1; k < n / 2; ++k) { const double db = 20 * std::log10 (std::abs (s[k])); lo = std::min (lo, db); hi = std::max (hi, db); }
        CHECK (hi - lo < 0.01, "ecart d'amplitude %.4f dB", hi - lo);
    }

    std::printf ("[8] Detection de tonalite\n");
    {
        auto detect = [&] (std::vector<double> notes)
        {
            SpectrumAnalyser a; a.prepare (sr); KeyDetector kd;
            std::vector<float> block (1024); long t = 0;
            for (int b = 0; b < 400; ++b)
            {
                for (auto& v : block)
                {
                    double y = 0;
                    for (double m : notes) for (int hN = 1; hN <= 6; ++hN) y += 0.08 / hN * std::sin (2 * kPi * noteToFreq (m) * hN * t / sr);
                    v = (float) y; ++t;
                }
                a.push (block.data(), (int) block.size());
                std::array<float, kDisplayBins> db; a.compute (db);
                kd.addSpectrum (a.getMagnitudes(), a.getBinHz());
            }
            return kd.estimate();
        };
        const char* names[] = { "C","C#","D","D#","E","F","F#","G","G#","A","A#","B" };
        auto r1 = detect ({ 60, 64, 67, 72 });          // Do majeur
        CHECK (r1.key == 0 && ! r1.minor, "Do-Mi-Sol -> %s %s (%.0f %%)", names[r1.key], r1.minor ? "mineur" : "majeur", r1.confidence * 100);
        auto r2 = detect ({ 57, 60, 64, 69 });          // La mineur
        CHECK (r2.key == 9 && r2.minor, "La-Do-Mi -> %s %s (%.0f %%)", names[r2.key], r2.minor ? "mineur" : "majeur", r2.confidence * 100);
    }

    std::printf ("[9] Analyseur : sinus 1 kHz a -6 dBFS\n");
    {
        SpectrumAnalyser a; a.prepare (sr);
        std::vector<float> s (8192);
        for (int i = 0; i < 8192; ++i) s[i] = 0.5f * (float) std::sin (2 * kPi * 1000.0 * i / sr);
        a.push (s.data(), 8192);
        std::array<float, kDisplayBins> db; a.compute (db);
        float mx = -200; for (float v : db) mx = std::max (mx, v);
        CHECK (std::abs (mx + 6.02f) < 1.5f, "crete affichee %.2f dBFS (perte de fenetre Hann <= 1.42 dB)", mx);
    }

    std::printf ("[10] Seed -> reglages (a comparer avec la maquette)\n");
    {
        const auto p = makePatch (0x0123456789ABCDEFULL);
        std::printf ("  cutoff=%g reso=%g drive=%g fType=%d dist=%d order=%d crush=%d bits=%d rate=%g depth=%g mix=%g key=%d scale=%d morph=%g gate=%g mb=%d b=%d/%d/%d x1=%g x2=%g\n",
            p.cutoff, p.resonance, p.drive, p.filterType, p.distType, p.filterOrder, (int) p.crushOn, p.bits, p.chorusRate, p.chorusDepth,
            p.chorusMix, p.key, p.scale, p.morph, p.gateDb, (int) p.multiband, p.bandBits[0], p.bandBits[1], p.bandBits[2], p.xover1, p.xover2);
    }

    std::printf ("[11] Mood : accents, majuscules et ponctuation\n");
    {
        const auto n = aeromood::normalizeFrench ("\xC3\x89th\xC3\xA9r\xC3\xA9" "e, M\xC3\x89TALLIQUE & sombre!");   // "Etheree, METALLIQUE & sombre!" avec accents
        const auto t = aeromood::parse ("\xC3\x89th\xC3\xA9r\xC3\xA9" "e, M\xC3\x89TALLIQUE & sombre!");
        CHECK (n.find ("etheree") != std::string::npos && n.find ("metallique") != std::string::npos, "normalisation : \"%s\"", n.c_str());
        CHECK (t.matched.size() == 3 && t.matched[0] == "etheree" && t.matched[1] == "metallique" && t.matched[2] == "sombre",
               "%d mots reconnus", (int) t.matched.size());
    }

    std::printf ("[12] Mood : addition des mots et bornes\n");
    {
        const auto t = aeromood::parse ("sombre chaotique metallique");
        CHECK (std::abs (t.axes[Bright] + 0.8) < 1e-6, "lumineux = %.2f (sombre -1 + metallique +0.2)", t.axes[Bright]);
        CHECK (std::abs (t.axes[Calm] + 1.0) < 1e-6, "serein = %.2f (borne a -1)", t.axes[Calm]);
        CHECK (t.has[Grain] && ! t.has[Warm], "axes mentionnes detectes");
        const auto u = aeromood::parse ("chaud froid");   // s'annulent : l'axe est quand meme 'mentionne'
        CHECK (u.has[Warm] && std::abs (u.axes[Warm]) < 1e-6, "mots opposes : chaud = %.2f, axe mentionne", u.axes[Warm]);
        CHECK (aeromood::parse ("bonjour piano 123").matched.empty(), "mots inconnus ignores");
    }

    std::printf ("[13] Mood : le texte l'emporte sur l'image, axe par axe\n");
    {
        MoodAxes img; for (int a = 0; a < numMoodAxes; ++a) img.v[a] = 0.5;
        const auto m = aeromood::merge (aeromood::parse ("sombre"), &img);
        CHECK (m[Bright] == -1.0 && m[Warm] == 0.5 && m[Grain] == 0.5, "lumineux %.1f (texte), chaud %.1f (image)", m[Bright], m[Warm]);
        const auto z = aeromood::merge (aeromood::parse (""), nullptr);
        CHECK (z[Bright] == 0.0 && z[Dense] == 0.0, "sans texte ni image : neutre");
    }

    std::printf ("[14] Mood : analyse d'image\n");
    {
        aeromood::CellStats black[aeromood::kCells];
        for (auto& c : black) c = { 0, 0, 0, 0, 0 };
        const auto mb = aeromood::fromCells (black);
        CHECK (mb[Bright] == -1.0 && mb[Calm] == 1.0, "image noire unie : sombre %.2f, serein %.2f", mb[Bright], mb[Calm]);

        aeromood::CellStats warm[aeromood::kCells];
        const double L = 0.299 * 230 + 0.587 * 150 + 0.114 * 60;
        for (auto& c : warm) c = { 230, 60, L, 0, 170 };
        const auto mw = aeromood::fromCells (warm);
        CHECK (mw[Warm] == 1.0 && mw[Vivid] == 1.0 && mw[Bright] > 0.3, "orange vif : chaud %.2f, vif %.2f, lumineux %.2f", mw[Warm], mw[Vivid], mw[Bright]);

        aeromood::CellStats checker[aeromood::kCells];   // damier contraste : chaotique et dense
        for (int i = 0; i < aeromood::kCells; ++i)
        {
            const double v = ((i % 16 + i / 16) % 2) ? 230.0 : 25.0;
            checker[i] = { v, v, v, 900.0, 0 };
        }
        const auto mc = aeromood::fromCells (checker);
        CHECK (mc[Calm] < -0.5 && mc[Dense] == 1.0 && mc[Grain] > 0.9, "damier : serein %.2f, dense %.2f, grain %.2f", mc[Calm], mc[Dense], mc[Grain]);
    }

    std::printf ("[15] Mood : chaque mot des phrases d'exemple est reconnu\n");
    {
        int missing = 0;
        for (int i = 0; i < aeromood::kNumExamples; ++i)
        {
            const std::string ex = aeromood::example (i);
            const auto t = aeromood::parse (ex);
            int words = 1; for (char c : ex) if (c == ' ') ++words;
            if ((int) t.matched.size() != words) { std::printf ("     manque dans \"%s\"\n", ex.c_str()); ++missing; }
        }
        CHECK (missing == 0, "%d exemple(s) incomplet(s)", missing);
    }

    std::printf ("[16] Seed + mood -> reglages (a comparer avec la maquette)\n");
    {
        MoodAxes a; a.v[Bright] = -0.8; a.v[Warm] = 0.3; a.v[Vivid] = 0.5; a.v[Calm] = -1.0; a.v[Dense] = 0.2; a.v[Grain] = 1.0;
        const auto p = makePatch (0x0123456789ABCDEFULL, a);
        std::printf ("  cutoff=%g reso=%g drive=%g fType=%d dist=%d order=%d crush=%d bits=%d rate=%g depth=%g mix=%g key=%d scale=%d morph=%g gate=%g mb=%d b=%d/%d/%d x1=%g x2=%g\n",
            p.cutoff, p.resonance, p.drive, p.filterType, p.distType, p.filterOrder, (int) p.crushOn, p.bits, p.chorusRate, p.chorusDepth,
            p.chorusMix, p.key, p.scale, p.morph, p.gateDb, (int) p.multiband, p.bandBits[0], p.bandBits[1], p.bandBits[2], p.xover1, p.xover2);
        const auto q = makePatch (0x0123456789ABCDEFULL);
        CHECK (q.cutoff == 287 && q.drive == 2.67, "sans mood : reglages identiques a l'ancienne version");
    }

    std::printf ("\n%s (%d echec(s))\n", failures ? "ECHEC" : "TOUT EST OK", failures);
    return failures ? 1 : 0;
}
