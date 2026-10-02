#pragma once
//==============================================================================
// AeroMood.h - "reconnaissance" de mood en C++17 pur (aucune dependance JUCE, teste dans Tests/DSPTests.cpp).
// Ce n'est PAS une IA : comme PhenoType, un dictionnaire local de mots + des mesures simples sur l'image.
//==============================================================================
#include "AeroDSP.h"
#include "MoodWords.h"
#include <cstring>
#include <string>
#include <vector>

namespace aeromood
{
using aerodsp::MoodAxes;

//==============================================================================
// Texte UTF-8 -> minuscules ASCII sans accents. Tout ce qui n'est pas une lettre devient un espace.
inline std::string normalizeFrench (const std::string& utf8)
{
    std::string out;
    out.reserve (utf8.size());
    size_t i = 0;
    while (i < utf8.size())
    {
        const auto c = (unsigned char) utf8[i];
        uint32_t cp = 0;
        int len = 1;
        if      (c < 0x80)           cp = c;
        else if ((c >> 5) == 0x6)    { cp = c & 0x1Fu; len = 2; }
        else if ((c >> 4) == 0xE)    { cp = c & 0x0Fu; len = 3; }
        else if ((c >> 3) == 0x1E)   { cp = c & 0x07u; len = 4; }
        else                         { out += ' '; ++i; continue; }   // octet invalide

        if (i + (size_t) len > utf8.size()) break;
        bool valid = true;
        for (int k = 1; k < len; ++k)
        {
            const auto cc = (unsigned char) utf8[i + (size_t) k];
            if ((cc >> 6) != 0x2) { valid = false; break; }
            cp = (cp << 6) | (cc & 0x3Fu);
        }
        i += valid ? (size_t) len : 1;
        if (! valid) { out += ' '; continue; }

        if (cp >= 'a' && cp <= 'z')                     out += (char) cp;
        else if (cp >= 'A' && cp <= 'Z')                out += (char) (cp - 'A' + 'a');
        else if ((cp >= 0xC0 && cp <= 0xC5) || (cp >= 0xE0 && cp <= 0xE5)) out += 'a';
        else if (cp == 0xC6 || cp == 0xE6)              out += "ae";
        else if (cp == 0xC7 || cp == 0xE7)              out += 'c';
        else if ((cp >= 0xC8 && cp <= 0xCB) || (cp >= 0xE8 && cp <= 0xEB)) out += 'e';
        else if ((cp >= 0xCC && cp <= 0xCF) || (cp >= 0xEC && cp <= 0xEF)) out += 'i';
        else if (cp == 0xD1 || cp == 0xF1)              out += 'n';
        else if ((cp >= 0xD2 && cp <= 0xD6) || cp == 0xD8 || (cp >= 0xF2 && cp <= 0xF6) || cp == 0xF8) out += 'o';
        else if ((cp >= 0xD9 && cp <= 0xDC) || (cp >= 0xF9 && cp <= 0xFC)) out += 'u';
        else if (cp == 0xDD || cp == 0xFD || cp == 0xFF) out += 'y';
        else if (cp == 0x152 || cp == 0x153)            out += "oe";
        else                                            out += ' ';
    }
    return out;
}

// Recherche dichotomique dans le dictionnaire (trie par ordre des octets)
inline const MoodWord* findWord (const std::string& w)
{
    int lo = 0, hi = kNumMoodWords - 1;
    while (lo <= hi)
    {
        const int mid = (lo + hi) / 2;
        const int cmp = std::strcmp (w.c_str(), kMoodWords[mid].word);
        if (cmp == 0) return &kMoodWords[mid];
        if (cmp < 0) hi = mid - 1; else lo = mid + 1;
    }
    return nullptr;
}

//==============================================================================
struct TextMood
{
    MoodAxes axes;
    bool has[aerodsp::numMoodAxes] {};   // l'axe a-t-il ete mentionne par au moins un mot ?
    std::vector<std::string> matched;    // mots reconnus (dans l'ordre de la phrase)
};

inline TextMood parse (const std::string& utf8)
{
    TextMood t;
    const std::string norm = normalizeFrench (utf8);
    size_t i = 0;
    while (i < norm.size())
    {
        while (i < norm.size() && norm[i] == ' ') ++i;
        size_t j = i;
        while (j < norm.size() && norm[j] != ' ') ++j;
        if (j > i)
        {
            const std::string tok = norm.substr (i, j - i);
            if (const auto* w = findWord (tok))
            {
                t.matched.push_back (tok);
                for (int a = 0; a < aerodsp::numMoodAxes; ++a)
                    if (w->axes[a] != 0.0f) { t.axes.v[a] += w->axes[a]; t.has[a] = true; }
            }
        }
        i = j;
    }
    for (auto& v : t.axes.v) v = std::clamp (v, -1.0, 1.0);
    return t;
}

// Le texte l'emporte sur les axes qu'il mentionne ; les autres axes viennent de l'image (ou 0 sans image)
inline MoodAxes merge (const TextMood& text, const MoodAxes* image)
{
    MoodAxes out;
    for (int a = 0; a < aerodsp::numMoodAxes; ++a)
        out.v[a] = text.has[a] ? text.axes.v[a] : (image != nullptr ? image->v[a] : 0.0);
    return out;
}

//==============================================================================
// Analyse de l'image a partir de la grille 16x16 deja utilisee pour la seed
struct CellStats { double meanR = 0, meanB = 0, meanL = 0, varL = 0, sat = 0; };
constexpr int kGrid = 16, kCells = kGrid * kGrid;

inline MoodAxes fromCells (const CellStats (&c)[kCells])
{
    double sumL = 0, sumR = 0, sumB = 0, sumSat = 0, sumVar = 0;
    for (const auto& cell : c) { sumL += cell.meanL; sumR += cell.meanR; sumB += cell.meanB; sumSat += cell.sat; sumVar += cell.varL; }
    const double avgL = sumL / kCells, avgR = sumR / kCells, avgB = sumB / kCells, avgSat = sumSat / kCells;

    double macroSq = 0;
    for (const auto& cell : c) macroSq += (cell.meanL - avgL) * (cell.meanL - avgL);
    const double macroStd = std::sqrt (macroSq / kCells), microStd = std::sqrt (sumVar / kCells);

    double edges = 0; int edgeCount = 0;
    for (int gy = 0; gy < kGrid; ++gy)
        for (int gx = 0; gx < kGrid; ++gx)
        {
            const int idx = gy * kGrid + gx;
            if (gx < kGrid - 1) { edges += std::abs (c[idx].meanL - c[idx + 1].meanL);     ++edgeCount; }
            if (gy < kGrid - 1) { edges += std::abs (c[idx].meanL - c[idx + kGrid].meanL); ++edgeCount; }
        }
    const double avgEdge = edges / edgeCount;

    auto cl = [] (double v) { return std::clamp (v, -1.0, 1.0); };
    MoodAxes m;
    m.v[aerodsp::Bright] = cl ((avgL - 128.0) / 110.0);
    m.v[aerodsp::Warm]   = cl ((avgR - avgB) / 70.0);
    m.v[aerodsp::Vivid]  = cl ((avgSat - 40.0) / 70.0);
    m.v[aerodsp::Calm]   = cl (1.0 - macroStd / 55.0);
    m.v[aerodsp::Dense]  = cl ((avgEdge - 8.0) / 22.0);
    m.v[aerodsp::Grain]  = cl ((microStd - 10.0) / 18.0);
    return m;
}

//==============================================================================
// Phrases d'exemple (bouton "Un exemple")
constexpr int kNumExamples = 15;
inline const char* example (int i)
{
    static const char* ex[kNumExamples] =
    {
        "sombre chaotique metallique", "lumineux doux planant", "glacial granuleux nerveux",
        "chaleureux dense organique", "vintage melancolique", "cyberpunk urbain agressif",
        "feerique cristallin serein", "orageux brutal sature", "minimaliste epure zen",
        "volcanique intense brulant", "onirique ethere flottant", "gothique abyssal tenebreux",
        "printanier vif colore", "industriel rugueux tendu", "aquatique limpide paisible"
    };
    return ex[((i % kNumExamples) + kNumExamples) % kNumExamples];
}

inline const char* axisName (int axis, bool positive)
{
    static const char* n[aerodsp::numMoodAxes][2] =
        { { "Sombre", "Lumineux" }, { "Froid", "Chaleureux" }, { "Terne", "Vif" },
          { "Chaotique", "Serein" }, { "Minimal", "Dense" }, { "Lisse", "Granuleux" } };
    return n[axis][positive ? 1 : 0];
}

} // namespace aeromood
