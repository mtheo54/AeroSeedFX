#pragma once
//==============================================================================
// AeroDSP.h  -  Coeur de traitement d'AeroSeed FX en C++17 pur (aucune dependance JUCE).
// Tout ce fichier est teste en dehors du plugin (voir Tests/DSPTests.cpp).
//
//  - Seed : image -> uint64 -> reglages (ordre de tirage FIGE, identique a la maquette)
//  - Gammes / tonalites
//  - FFT radix-2
//  - SpectralHarmonizer : recale les frequences hors gamme sur la note la plus proche (facon Chroma),
//    avec Spectral Morph (melange dry/accorde dans le domaine frequentiel, transitoires preserves)
//    et Spectral Gate (supprime le "ringing" genere par l'accordage)
//  - Crossover3 : separation 3 bandes Linkwitz-Riley 24 dB/oct (somme a plat) pour le bitcrusher multibande
//  - SpectrumAnalyser / KeyDetector / outils de tendance pour l'affichage
//==============================================================================
#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstdint>
#include <vector>

namespace aerodsp
{
constexpr double kPi = 3.14159265358979323846;

//==============================================================================
// SEED
//==============================================================================
inline uint64_t splitmix64 (uint64_t& state)
{
    uint64_t z = (state += 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

inline double nextUnit (uint64_t& state)   // [0, 1[ sur 24 bits : exact et identique partout
{
    return (double) (splitmix64 (state) >> 40) / 16777216.0;
}

inline double round2 (double v) { return std::round (v * 100.0) / 100.0; }

struct Patch
{
    // Filtre
    int   filterType = 0;  int filterOrder = 0;  double cutoff = 20000.0;  double resonance = 0.71;
    // Distortion
    int   distType = 0;    double drive = 1.0;
    // Bitcrusher
    bool  crushOn = false; int bits = 8;  bool multiband = false;  int bandBits[3] { 8, 8, 8 };
    double xover1 = 200.0; double xover2 = 2500.0;
    // Chorus
    double chorusRate = 1.2; double chorusDepth = 40.0; double chorusMix = 0.0;
    // Harmonique
    int   key = 0;  int scale = 1;  double morph = 0.0;  double gateDb = -80.0;
};

// ORDRE DE TIRAGE FIGE (22 tirages). Ne jamais le modifier : ajouter les nouveaux tirages A LA FIN.
inline Patch makePatch (uint64_t seed)
{
    uint64_t s = seed;
    double u[22];
    for (auto& v : u) v = nextUnit (s);

    Patch p;
    p.cutoff      = std::round (200.0 * std::pow (75.0, u[0]));
    p.resonance   = round2 (0.6 + u[1] * 3.4);
    p.drive       = round2 (1.0 + u[2] * 9.0);
    p.filterType  = std::min (2, (int) std::floor (u[3] * 3.0));
    p.distType    = std::min (2, (int) std::floor (u[4] * 3.0));
    p.filterOrder = u[5] < 0.5 ? 0 : 1;
    p.crushOn     = u[6] < 0.3;
    p.bits        = (int) std::floor (4.0 + u[7] * 8.0);
    p.chorusRate  = round2 (0.2 + u[8] * 4.8);
    p.chorusDepth = std::round (10.0 + u[9] * 70.0);
    p.chorusMix   = std::round (u[10] * 60.0);
    p.key         = (int) std::floor (u[11] * 12.0);
    p.scale       = 1 + (int) std::floor (u[12] * 11.0);
    p.morph       = std::round (40.0 + u[13] * 60.0);
    /* u[14] : reserve */
    p.gateDb      = std::round (-80.0 + u[15] * 40.0);
    p.multiband   = u[16] < 0.4;
    p.bandBits[0] = 3 + (int) std::floor (u[17] * 9.0);
    p.bandBits[1] = 3 + (int) std::floor (u[18] * 9.0);
    p.bandBits[2] = 3 + (int) std::floor (u[19] * 9.0);
    p.xover1      = std::round (120.0 * std::pow (8.0, u[20]));
    p.xover2      = std::round (1500.0 * std::pow (6.0, u[21]));
    return p;
}

//==============================================================================
// GAMMES
//==============================================================================
constexpr int kNumScales = 13;          // 0 = chromatique ... 11, 12 = personnalisee
constexpr int kCustomScale = 12;

inline uint16_t relativeMask (int scale)
{
    static const uint16_t masks[kNumScales] =
    {
        0xFFF,                                                   // Chromatique
        (1<<0)|(1<<2)|(1<<4)|(1<<5)|(1<<7)|(1<<9)|(1<<11),       // Majeur
        (1<<0)|(1<<2)|(1<<3)|(1<<5)|(1<<7)|(1<<8)|(1<<10),       // Mineur
        (1<<0)|(1<<2)|(1<<3)|(1<<5)|(1<<7)|(1<<9)|(1<<10),       // Dorien
        (1<<0)|(1<<1)|(1<<3)|(1<<5)|(1<<7)|(1<<8)|(1<<10),       // Phrygien
        (1<<0)|(1<<2)|(1<<4)|(1<<6)|(1<<7)|(1<<9)|(1<<11),       // Lydien
        (1<<0)|(1<<2)|(1<<4)|(1<<5)|(1<<7)|(1<<9)|(1<<10),       // Mixolydien
        (1<<0)|(1<<2)|(1<<4)|(1<<7)|(1<<9),                      // Pentatonique majeure
        (1<<0)|(1<<3)|(1<<5)|(1<<7)|(1<<10),                     // Pentatonique mineure
        (1<<0)|(1<<2)|(1<<3)|(1<<5)|(1<<7)|(1<<8)|(1<<11),       // Mineur harmonique
        (1<<0)|(1<<3)|(1<<5)|(1<<6)|(1<<7)|(1<<10),              // Blues
        (1<<0)|(1<<2)|(1<<4)|(1<<6)|(1<<8)|(1<<10),              // Gamme par tons
        0                                                        // Personnalisee
    };
    return masks[std::clamp (scale, 0, kNumScales - 1)];
}

// Masque absolu (bit 0 = C ... bit 11 = B)
inline uint16_t maskFor (int key, int scale)
{
    const uint16_t rel = relativeMask (scale);
    uint16_t m = 0;
    for (int i = 0; i < 12; ++i)
        if (rel >> i & 1) m |= (uint16_t) (1u << ((i + key) % 12));
    return m;
}

// Gamme correspondant exactement a un masque pour une tonalite (ou la gamme personnalisee)
inline int scaleForMask (int key, uint16_t mask)
{
    for (int s = 0; s < kCustomScale; ++s)
        if (maskFor (key, s) == mask) return s;
    return kCustomScale;
}

inline double noteToFreq (double midi) { return 440.0 * std::pow (2.0, (midi - 69.0) / 12.0); }
inline double freqToNote (double f)    { return 69.0 + 12.0 * std::log2 (f / 440.0); }

// Frequence de la note autorisee la plus proche (f inchangee si le masque est vide)
inline double snapFrequency (double f, uint16_t mask)
{
    if (mask == 0 || f <= 0.0) return f;
    const double m = freqToNote (f);
    const int r = (int) std::lround (m);
    int best = -1000;  double bestDist = 1e9;
    for (int k = r - 6; k <= r + 6; ++k)
    {
        const int pc = ((k % 12) + 12) % 12;
        if ((mask >> pc) & 1)
        {
            const double d = std::abs ((double) k - m);
            if (d < bestDist) { bestDist = d; best = k; }
        }
    }
    return best == -1000 ? f : noteToFreq (best);
}

//==============================================================================
// FFT radix-2 (complexe, en place). forward : sans normalisation ; inverse : sans 1/N.
//==============================================================================
class FFT
{
public:
    explicit FFT (int order = 11) { setOrder (order); }

    void setOrder (int order)
    {
        n = 1 << order;
        rev.assign ((size_t) n, 0);
        for (int i = 0; i < n; ++i)
        {
            int r = 0;
            for (int b = 0; b < order; ++b) if (i >> b & 1) r |= 1 << (order - 1 - b);
            rev[(size_t) i] = r;
        }
        tw.resize ((size_t) n / 2);
        for (int k = 0; k < n / 2; ++k)
            tw[(size_t) k] = std::polar (1.0f, (float) (-2.0 * kPi * k / n));
    }

    int size() const { return n; }
    void forward (std::complex<float>* a) const { run (a, false); }
    void inverse (std::complex<float>* a) const { run (a, true); }

private:
    void run (std::complex<float>* a, bool inv) const
    {
        for (int i = 0; i < n; ++i)
            if (i < rev[(size_t) i]) std::swap (a[i], a[rev[(size_t) i]]);

        for (int len = 2; len <= n; len <<= 1)
        {
            const int half = len / 2, step = n / len;
            for (int i = 0; i < n; i += len)
                for (int j = 0; j < half; ++j)
                {
                    auto w = tw[(size_t) (j * step)];
                    if (inv) w = std::conj (w);
                    const auto u = a[i + j];
                    const auto v = a[i + j + half] * w;
                    a[i + j] = u + v;
                    a[i + j + half] = u - v;
                }
        }
    }

    int n = 0;
    std::vector<int> rev;
    std::vector<std::complex<float>> tw;
};

//==============================================================================
// SPECTRAL HARMONIZER (un canal). Vocodeur de phase en flux continu (structure de Bernsee).
// Latence fixe = N echantillons (2048), mesuree par les tests.
//==============================================================================
class SpectralHarmonizer
{
public:
    void prepare (double sampleRate, int fftOrder = 11, int oversampling = 4)
    {
        sr = sampleRate;
        fft.setOrder (fftOrder);
        N = fft.size();
        osamp = oversampling;
        hop = N / osamp;
        half = N / 2;

        win.resize ((size_t) N);
        for (int i = 0; i < N; ++i) win[(size_t) i] = (float) (0.5 - 0.5 * std::cos (2.0 * kPi * i / N));   // Hann periodique

        auto z = [] (auto& v, size_t s) { v.assign (s, {}); };
        z (inFifo, (size_t) N);  z (outFifo, (size_t) N);  z (outAccum, (size_t) (2 * N));
        z (buf, (size_t) N);
        z (lastPhase, (size_t) half + 1);  z (sumPhase, (size_t) half + 1);
        z (anaMag, (size_t) half + 1);     z (anaPhase, (size_t) half + 1);  z (anaFreq, (size_t) half + 1);
        z (prevMag, (size_t) half + 1);
        z (synMag, (size_t) half + 1);     z (synFreqW, (size_t) half + 1);
        z (synSrcPh, (size_t) half + 1);   z (synSrcMag, (size_t) half + 1);  z (lockedPh, (size_t) half + 1);
        peakOf.assign ((size_t) half + 1, 0);
        gateGain.assign ((size_t) half + 1, 1.0f);
        reset();
    }

    void reset()
    {
        std::fill (inFifo.begin(), inFifo.end(), 0.0f);
        std::fill (outFifo.begin(), outFifo.end(), 0.0f);
        std::fill (outAccum.begin(), outAccum.end(), 0.0f);
        std::fill (lastPhase.begin(), lastPhase.end(), 0.0f);
        std::fill (sumPhase.begin(), sumPhase.end(), 0.0f);
        std::fill (prevMag.begin(), prevMag.end(), 0.0f);
        std::fill (gateGain.begin(), gateGain.end(), 1.0f);
        rover = fifoLatency();
    }

    int latency() const { return N; }   // latence reelle entree -> sortie (verifiee par Tests/DSPTests.cpp)

    // mask : notes autorisees ; morph : 0..1 ; gateDb : seuil en dBFS (-80 = inactif en pratique)
    void setParameters (uint16_t newMask, float morph01, float newGateDb)
    {
        mask = newMask;
        morph = std::clamp (morph01, 0.0f, 1.0f);
        gateDb = newGateDb;
    }

    float process (float x)
    {
        inFifo[(size_t) rover] = x;
        const float y = outFifo[(size_t) (rover - fifoLatency())];
        if (++rover >= N)
        {
            rover = fifoLatency();
            processFrame();
        }
        return y;
    }

    float lastTransient() const { return transient; }

private:
    int fifoLatency() const { return N - hop; }

    void processFrame()
    {
        const double fpb = sr / N;                       // Hz par bin
        const double expct = 2.0 * kPi * hop / N;        // avance de phase attendue par bin

        for (int k = 0; k < N; ++k) buf[(size_t) k] = { inFifo[(size_t) k] * win[(size_t) k], 0.0f };
        fft.forward (buf.data());

        // ---- analyse : amplitude, phase, frequence vraie de chaque bin
        double flux = 0.0, total = 0.0;
        for (int k = 0; k <= half; ++k)
        {
            const float mag = std::abs (buf[(size_t) k]);
            const float ph  = std::arg (buf[(size_t) k]);
            double d = ph - lastPhase[(size_t) k];
            lastPhase[(size_t) k] = ph;
            d -= k * expct;
            d = std::remainder (d, 2.0 * kPi);           // ramene dans [-pi, pi]
            anaFreq[(size_t) k] = (float) ((k + osamp * d / (2.0 * kPi)) * fpb);
            anaMag[(size_t) k] = mag;
            anaPhase[(size_t) k] = ph;
            flux  += std::max (0.0f, mag - prevMag[(size_t) k]);
            total += mag;
            prevMag[(size_t) k] = mag;
        }

        // ---- transitoire : le Morph s'efface pendant les attaques (elles restent nettes)
        const double fluxRatio = total > 1e-9 ? flux / total : 0.0;
        transient = (float) std::clamp ((fluxRatio - 0.3) / 0.3, 0.0, 1.0);
        const float m = morph * (1.0f - transient);

        // ---- spectre accorde : chaque bin est decale de l'ecart (note autorisee - frequence vraie).
        //      Tout le pic d'une note se deplace d'un bloc : sa forme et son niveau sont conserves.
        std::fill (synMag.begin(), synMag.end(), 0.0f);
        std::fill (synFreqW.begin(), synFreqW.end(), 0.0f);
        std::fill (synSrcMag.begin(), synSrcMag.end(), 0.0f);
        if (m > 0.0f)
        {
            for (int k = 1; k <= half; ++k)
            {
                const double f = anaFreq[(size_t) k];
                const double fs = (f < 30.0 || f > sr * 0.45) ? f : snapFrequency (f, mask);
                const int kk = k + (int) std::lround ((fs - f) / fpb);
                if (kk < 1 || kk > half) continue;
                synMag[(size_t) kk]   += anaMag[(size_t) k];
                synFreqW[(size_t) kk] += (float) (fs * anaMag[(size_t) k]);
                if (anaMag[(size_t) k] > synSrcMag[(size_t) kk])   // phase du contributeur principal
                {
                    synSrcMag[(size_t) kk] = anaMag[(size_t) k];
                    synSrcPh[(size_t) kk]  = anaPhase[(size_t) k];
                }
            }
        }

        // phase de synthese accumulee (coherente d'une trame a l'autre)
        for (int k = 0; k <= half; ++k)
        {
            const double sf = synMag[(size_t) k] > 1e-12f ? synFreqW[(size_t) k] / synMag[(size_t) k] : k * fpb;
            double d = sf / fpb - k;
            d = 2.0 * kPi * d / osamp + k * expct;
            sumPhase[(size_t) k] = (float) std::remainder (sumPhase[(size_t) k] + d, 2.0 * kPi);
        }

        // verrouillage de phase autour des pics (Laroche & Dolson) : chaque pic garde sa forme et son niveau,
        // et le son "phase" typique des vocodeurs disparait.
        {
            int lastPeak = -1;
            for (int k = 1; k < half; ++k)
            {
                const bool isPeak = synMag[(size_t) k] > 1e-9f && synMag[(size_t) k] > synMag[(size_t) k - 1] && synMag[(size_t) k] >= synMag[(size_t) k + 1];
                if (! isPeak) continue;
                const int from = lastPeak < 0 ? 0 : (lastPeak + k) / 2 + 1;
                for (int j = from; j <= k; ++j) peakOf[(size_t) j] = k;
                if (lastPeak >= 0) for (int j = lastPeak + 1; j < from; ++j) peakOf[(size_t) j] = lastPeak;
                lastPeak = k;
            }
            for (int j = std::max (0, lastPeak); j <= half; ++j) peakOf[(size_t) j] = std::max (lastPeak, 0);
            for (int k = 0; k <= half; ++k)
            {
                const int pk = peakOf[(size_t) k];
                lockedPh[(size_t) k] = lastPeak < 0 ? sumPhase[(size_t) k]
                                                    : sumPhase[(size_t) pk] + (synSrcPh[(size_t) k] - synSrcPh[(size_t) pk]);
            }
        }

        const float gateNorm = 4.0f / (float) N;   // amplitude d'un sinus (fenetre de Hann)
        for (int k = 0; k <= half; ++k)
        {
            const float dryMag = anaMag[(size_t) k], wetMag = synMag[(size_t) k];
            float mag = (1.0f - m) * dryMag + m * wetMag;                        // Spectral Morph
            const float ph = (m * wetMag >= (1.0f - m) * dryMag) ? lockedPh[(size_t) k] : anaPhase[(size_t) k];

            // Spectral Gate : coupe les composantes faibles (ringing), seulement sur la part accordee
            const float ampDb = 20.0f * std::log10 (mag * gateNorm + 1e-12f);
            const float target = ampDb < gateDb ? 0.0f : 1.0f;
            gateGain[(size_t) k] += (target - gateGain[(size_t) k]) * 0.6f;
            mag *= 1.0f - m * (1.0f - gateGain[(size_t) k]);

            buf[(size_t) k] = std::polar (mag, ph);
        }
        buf[0] = { buf[0].real(), 0.0f };
        buf[(size_t) half] = { buf[(size_t) half].real(), 0.0f };
        for (int k = half + 1; k < N; ++k) buf[(size_t) k] = std::conj (buf[(size_t) (N - k)]);

        fft.inverse (buf.data());

        // ---- overlap-add (Hann analyse + synthese, recouvrement 4 : somme des w^2 = 1.5)
        const float scale = 1.0f / ((float) N * 1.5f);
        for (int k = 0; k < N; ++k) outAccum[(size_t) k] += win[(size_t) k] * buf[(size_t) k].real() * scale;

        for (int k = 0; k < hop; ++k) outFifo[(size_t) k] = outAccum[(size_t) k];
        std::copy (outAccum.begin() + hop, outAccum.end(), outAccum.begin());
        std::fill (outAccum.end() - hop, outAccum.end(), 0.0f);
        std::copy (inFifo.begin() + hop, inFifo.end(), inFifo.begin());
    }

    double sr = 44100.0;
    int N = 2048, hop = 512, half = 1024, osamp = 4, rover = 0;
    FFT fft { 11 };
    uint16_t mask = 0xFFF;
    float morph = 0.0f, gateDb = -80.0f, transient = 0.0f;

    std::vector<float> win, inFifo, outFifo, outAccum, lastPhase, sumPhase, anaMag, anaPhase, anaFreq,
                       prevMag, synMag, synFreqW, synSrcPh, synSrcMag, lockedPh, gateGain;
    std::vector<int> peakOf;
    std::vector<std::complex<float>> buf;
};

//==============================================================================
// FILTRES BIQUAD (RBJ) + CROSSOVER 3 BANDES LINKWITZ-RILEY (LR4)
//==============================================================================
struct Biquad
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;

    enum Type { LowPass, HighPass, AllPass };

    void set (Type t, double fc, double q, double sr)
    {
        const double w = 2.0 * kPi * std::clamp (fc, 10.0, sr * 0.49) / sr;
        const double c = std::cos (w), al = std::sin (w) / (2.0 * q), a0 = 1.0 + al;
        switch (t)
        {
            case LowPass:  b0 = (1 - c) / 2; b1 = 1 - c;    b2 = (1 - c) / 2; break;
            case HighPass: b0 = (1 + c) / 2; b1 = -(1 + c); b2 = (1 + c) / 2; break;
            case AllPass:  b0 = 1 - al;      b1 = -2 * c;   b2 = 1 + al;      break;
        }
        a1 = -2 * c; a2 = 1 - al;
        b0 /= a0; b1 /= a0; b2 /= a0; a1 /= a0; a2 /= a0;
    }

    void reset() { z1 = z2 = 0; }

    float process (float x)
    {
        const double y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return (float) y;
    }
};

// low + mid + high = passe-tout (amplitude parfaitement plate)
class Crossover3
{
public:
    void prepare (double sampleRate) { sr = sampleRate; f1 = f2 = -1.0; setFrequencies (200.0, 2500.0); reset(); }

    void reset() { for (auto* b : all()) b->reset(); }

    // Change uniquement les coefficients : l'etat des filtres est conserve (pas de clic pendant une automation)
    void setFrequencies (double newF1, double newF2)
    {
        newF2 = std::max (newF2, newF1 * 1.1);
        if (newF1 == f1 && newF2 == f2) return;
        f1 = newF1; f2 = newF2;
        const double q = 1.0 / std::sqrt (2.0);
        Biquad c;
        c.set (Biquad::LowPass,  f1, q, sr); copyCoeffs (c, lp1a); copyCoeffs (c, lp1b);
        c.set (Biquad::HighPass, f1, q, sr); copyCoeffs (c, hp1a); copyCoeffs (c, hp1b);
        c.set (Biquad::LowPass,  f2, q, sr); copyCoeffs (c, lp2a); copyCoeffs (c, lp2b);
        c.set (Biquad::HighPass, f2, q, sr); copyCoeffs (c, hp2a); copyCoeffs (c, hp2b);
        c.set (Biquad::AllPass,  f2, q, sr); copyCoeffs (c, ap2);   // LR4 LP + HP = passe-tout du 2e ordre (Q = 0.707)
    }

    void process (float x, float& low, float& mid, float& high)
    {
        low = ap2.process (lp1b.process (lp1a.process (x)));
        const float rest = hp1b.process (hp1a.process (x));
        mid  = lp2b.process (lp2a.process (rest));
        high = hp2b.process (hp2a.process (rest));
    }

private:
    static void copyCoeffs (const Biquad& src, Biquad& dst) { dst.b0 = src.b0; dst.b1 = src.b1; dst.b2 = src.b2; dst.a1 = src.a1; dst.a2 = src.a2; }

    std::array<Biquad*, 9> all() { return { &lp1a, &lp1b, &hp1a, &hp1b, &lp2a, &lp2b, &hp2a, &hp2b, &ap2 }; }

    double sr = 44100.0, f1 = -1.0, f2 = -1.0;
    Biquad lp1a, lp1b, hp1a, hp1b, lp2a, lp2b, hp2a, hp2b, ap2;
};

inline float crushSample (float x, float levels) { return std::round (x * levels) / levels; }
inline float crushLevels (float bits) { return std::pow (2.0f, bits - 1.0f); }

//==============================================================================
// ANALYSE POUR L'AFFICHAGE (cote interface)
//==============================================================================
constexpr int    kDisplayBins = 320;
constexpr double kMinHz = 20.0, kMaxHz = 20000.0;

inline double displayBinFreq (double i) { return kMinHz * std::pow (kMaxHz / kMinHz, i / (kDisplayBins - 1)); }

class SpectrumAnalyser
{
public:
    static constexpr int order = 12, size = 1 << order;   // FFT 4096

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        ring.assign ((size_t) size, 0.0f);
        win.resize ((size_t) size);
        winSum = 0.0;
        for (int i = 0; i < size; ++i) { win[(size_t) i] = (float) (0.5 - 0.5 * std::cos (2.0 * kPi * i / size)); winSum += win[(size_t) i]; }
        buf.assign ((size_t) size, {});
        magnitudes.assign ((size_t) size / 2 + 1, 0.0f);
        writePos = 0;
    }

    void push (const float* data, int num)
    {
        for (int i = 0; i < num; ++i) { ring[(size_t) writePos] = data[i]; writePos = (writePos + 1) & (size - 1); }
    }

    // Spectre en dBFS sur kDisplayBins bandes logarithmiques (20 Hz - 20 kHz)
    void compute (std::array<float, kDisplayBins>& outDb)
    {
        for (int i = 0; i < size; ++i)
            buf[(size_t) i] = { ring[(size_t) ((writePos + i) & (size - 1))] * win[(size_t) i], 0.0f };
        fft.forward (buf.data());

        const float norm = (float) (2.0 / winSum);
        for (int k = 0; k <= size / 2; ++k) magnitudes[(size_t) k] = std::abs (buf[(size_t) k]) * norm;

        const double binHz = sr / size;
        for (int i = 0; i < kDisplayBins; ++i)
        {
            const double fa = displayBinFreq (i - 0.5), fb = displayBinFreq (i + 0.5);
            int ka = (int) std::floor (fa / binHz), kb = (int) std::ceil (fb / binHz);
            ka = std::clamp (ka, 1, size / 2);  kb = std::clamp (kb, ka, size / 2);
            float m = 0.0f;
            if (kb - ka <= 1)   // bande plus etroite qu'un bin : interpolation
            {
                const double x = displayBinFreq (i) / binHz;
                const int k0 = std::clamp ((int) std::floor (x), 0, size / 2 - 1);
                const double t = x - k0;
                m = (float) (magnitudes[(size_t) k0] * (1.0 - t) + magnitudes[(size_t) k0 + 1] * t);
            }
            else
                for (int k = ka; k <= kb; ++k) m = std::max (m, magnitudes[(size_t) k]);

            outDb[(size_t) i] = 20.0f * std::log10 (m + 1e-9f);
        }
    }

    const std::vector<float>& getMagnitudes() const { return magnitudes; }
    double getBinHz() const { return sr / size; }

private:
    FFT fft { order };
    double sr = 44100.0, winSum = 1.0;
    int writePos = 0;
    std::vector<float> ring, win, magnitudes;
    std::vector<std::complex<float>> buf;
};

// Detection de tonalite : chromagramme (pics 40 Hz - 2.4 kHz) + correlation de Krumhansl-Schmuckler
class KeyDetector
{
public:
    struct Result { int key = 0; bool minor = false; float confidence = 0.0f; };

    void reset() { chroma.fill (0.0f); }

    void addSpectrum (const std::vector<float>& mag, double binHz, float decay = 0.9975f)
    {
        for (auto& c : chroma) c *= decay;
        float mx = 0.0f;
        for (float v : mag) mx = std::max (mx, v);
        if (mx < 1e-5f) return;

        for (size_t k = 1; k + 1 < mag.size(); ++k)
        {
            const double f = k * binHz;
            if (f < 40.0 || f > 2400.0) continue;
            const float v = mag[k];
            if (v < 0.05f * mx || v < mag[k - 1] || v < mag[k + 1]) continue;   // pics seulement
            const int pc = ((int) std::lround (freqToNote (f)) % 12 + 12) % 12;
            chroma[(size_t) pc] += v / mx;
        }
    }

    Result estimate() const
    {
        static const float maj[12] = { 6.35f, 2.23f, 3.48f, 2.33f, 4.38f, 4.09f, 2.52f, 5.19f, 2.39f, 3.66f, 2.29f, 2.88f };
        static const float mnr[12] = { 6.33f, 2.68f, 3.52f, 5.38f, 2.60f, 3.53f, 2.54f, 4.75f, 3.98f, 2.69f, 3.34f, 3.17f };
        Result best; float bestR = -2.0f;
        for (int t = 0; t < 12; ++t)
            for (int md = 0; md < 2; ++md)
            {
                const float* pr = md ? mnr : maj;
                double sx = 0, sy = 0, sxy = 0, sxx = 0, syy = 0;
                for (int i = 0; i < 12; ++i)
                {
                    const double x = chroma[(size_t) ((i + t) % 12)], y = pr[i];
                    sx += x; sy += y; sxy += x * y; sxx += x * x; syy += y * y;
                }
                const double den = std::sqrt ((12 * sxx - sx * sx) * (12 * syy - sy * sy));
                const float r = den > 1e-12 ? (float) ((12 * sxy - sx * sy) / den) : 0.0f;
                if (r > bestR) { bestR = r; best.key = t; best.minor = md == 1; }
            }
        best.confidence = std::clamp (bestR, 0.0f, 1.0f);
        return best;
    }

    const std::array<float, 12>& getChroma() const { return chroma; }

private:
    std::array<float, 12> chroma {};
};

// Courbe de tendance : lissage spatial + pente en dB/octave (100 Hz - 10 kHz)
inline void smoothTrend (const std::array<float, kDisplayBins>& in, std::array<float, kDisplayBins>& out, int radius = 14)
{
    for (int i = 0; i < kDisplayBins; ++i)
    {
        double s = 0; int n = 0;
        for (int j = std::max (0, i - radius); j <= std::min (kDisplayBins - 1, i + radius); ++j) { s += in[(size_t) j]; ++n; }
        out[(size_t) i] = (float) (s / n);
    }
}

inline float trendSlopeDbPerOctave (const std::array<float, kDisplayBins>& t)
{
    double sx = 0, sy = 0, sxy = 0, sxx = 0; int n = 0;
    for (int i = 0; i < kDisplayBins; ++i)
    {
        const double f = displayBinFreq (i);
        if (f < 100.0 || f > 10000.0) continue;
        const double l = std::log2 (f);
        sx += l; sy += t[(size_t) i]; sxy += l * t[(size_t) i]; sxx += l * l; ++n;
    }
    const double den = n * sxx - sx * sx;
    return den > 1e-12 ? (float) ((n * sxy - sx * sy) / den) : 0.0f;
}

} // namespace aerodsp
