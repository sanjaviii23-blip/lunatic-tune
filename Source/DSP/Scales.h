#pragma once
#include <array>
#include <vector>

namespace lunatic
{
// Máscara relativa a la tónica: índice 0 = tónica, 1 = +1 semitono, etc.
struct ScaleDef
{
    const char* name;
    std::array<bool, 12> mask;
};

inline const std::vector<ScaleDef>& scaleTable()
{
    static const std::vector<ScaleDef> table = {
        { "Cromática",          { 1,1,1,1,1,1,1,1,1,1,1,1 } },
        { "Mayor",              { 1,0,1,0,1,1,0,1,0,1,0,1 } },
        { "Menor natural",      { 1,0,1,1,0,1,0,1,1,0,1,0 } },
        { "Menor armónica",     { 1,0,1,1,0,1,0,1,1,0,0,1 } },
        { "Menor melódica",     { 1,0,1,1,0,1,0,1,0,1,0,1 } },
        { "Pentatónica mayor",  { 1,0,1,0,1,0,0,1,0,1,0,0 } },
        { "Pentatónica menor",  { 1,0,0,1,0,1,0,1,0,0,1,0 } },
        { "Blues",              { 1,0,0,1,0,1,1,1,0,0,1,0 } },
        { "Dórica",             { 1,0,1,1,0,1,0,1,0,1,1,0 } },
        { "Frigia",             { 1,1,0,1,0,1,0,1,1,0,1,0 } },
        { "Lidia",              { 1,0,1,0,1,0,1,1,0,1,0,1 } },
        { "Mixolidia",          { 1,0,1,0,1,1,0,1,0,1,1,0 } },
    };
    return table;
}

inline const char* noteName(int pitchClass)
{
    static const char* names[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    return names[((pitchClass % 12) + 12) % 12];
}

inline bool scaleAllows(int scaleIndex, int key, int pitchClass)
{
    const auto& t = scaleTable();
    if (scaleIndex < 0 || scaleIndex >= (int) t.size())
        scaleIndex = 0;
    return t[(size_t) scaleIndex].mask[(size_t) (((pitchClass - key) % 12 + 12) % 12)];
}
} // namespace lunatic
