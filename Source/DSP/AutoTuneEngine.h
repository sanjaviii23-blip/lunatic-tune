#pragma once
// LUNATIC TUNE - motor de afinación.
// Combina lo mejor de dos filosofías:
//  - Tipo Auto-Tune Pro: escala/tonalidad, velocidad de retune, Humanize, Flex-Tune, vibrato natural, MIDI.
//  - Tipo Tune Real-Time: respuesta inmediata, latencia baja y fija, control de formantes.
#include "PitchDetector.h"
#include "PsolaShifter.h"
#include "Scales.h"

#include <array>
#include <atomic>

namespace lunatic
{
struct TuneParams
{
    float retuneMs = 25.0f;      // 0 = efecto "hard tune"
    float humanize = 0.0f;       // 0..1
    float flexCents = 0.0f;      // zona de tolerancia sin corregir
    float vibrato = 1.0f;        // 0 = aplanado, 1 = natural, 2 = exagerado
    float formantSemis = 0.0f;   // -5..+5
    float transposeSemis = 0.0f; // -7..+7
    float detuneCents = 0.0f;
    float refA4 = 440.0f;
    int key = 0;
    int scale = 0;
    std::array<bool, 12> noteEnabled { true, true, true, true, true, true, true, true, true, true, true, true };
    bool midiMode = false;
    std::array<bool, 12> midiHeld {};
};

class AutoTuneEngine
{
public:
    static constexpr int kNumRanges = 4;
    static constexpr float kRangeMinHz[kNumRanges] = { 65.0f, 90.0f, 130.0f, 220.0f };
    static constexpr float kMaxFormantSemis = 5.0f;
    static constexpr float kMaxShiftSemis = 7.0f;

    void prepare (double sampleRate, int rangeIndex)
    {
        fs = sampleRate;
        rangeIndex = std::clamp (rangeIndex, 0, kNumRanges - 1);
        const float fmin = kRangeMinHz[rangeIndex];
        detector.prepare (fs, fmin);
        shifter.prepare (fs, fmin, std::pow (2.0f, kMaxFormantSemis / 12.0f));
        hop = std::max (64, (int) (fs * 0.006));
        ratioCoef = 1.0f - std::exp (-1.0f / (float) (0.002 * fs));
        formantCoef = 1.0f - std::exp (-1.0f / (float) (0.02 * fs));
        reset();
    }

    int latencySamples() const { return shifter.latency(); }

    void reset()
    {
        detector.reset();
        shifter.reset();
        hopCount = 0;
        ratioCur = ratioTarget = 1.0f;
        formantCur = formantTarget = 1.0f;
        periodIn = 0.0f;
        wasVoiced = false;
        voicedHold = 0;
        target = -1;
        c = 0.0f;
        mSlow = 0.0f;
        holdTime = 0.0f;
        shiftSemis = 0.0f;
        lastDry = 0.0f;
        meterDetected.store (-1.0f);
        meterOutput.store (-1.0f);
        meterTarget.store (-1.0f);
    }

    void setParams (const TuneParams& np)
    {
        p = np;
        noTargets = false;
        if (p.midiMode)
        {
            bool any = false;
            for (int i = 0; i < 12; ++i)
            {
                allowed[(size_t) i] = p.midiHeld[(size_t) i];
                any = any || allowed[(size_t) i];
            }
            noTargets = ! any;
        }
        else
        {
            bool any = false;
            for (int i = 0; i < 12; ++i)
            {
                allowed[(size_t) i] = p.noteEnabled[(size_t) i] && scaleAllows (p.scale, p.key, i);
                any = any || allowed[(size_t) i];
            }
            noTargets = ! any;
        }
        formantTarget = std::pow (2.0f, std::clamp (p.formantSemis, -kMaxFormantSemis, kMaxFormantSemis) / 12.0f);
    }

    // Devuelve la señal procesada (100 % wet). La señal seca alineada queda en dry().
    float process (float in)
    {
        detector.push (in);
        if (++hopCount >= hop)
        {
            hopCount = 0;
            onHop();
        }
        ratioCur += ratioCoef * (ratioTarget - ratioCur);
        formantCur += formantCoef * (formantTarget - formantCur);
        const float wet = shifter.process (in, periodIn, ratioCur, formantCur);
        lastDry = shifter.delayedDry();
        return wet;
    }

    float dry() const { return lastDry; }

    // Medidores para la interfaz (notas MIDI fraccionarias, -1 = sin voz)
    std::atomic<float> meterDetected { -1.0f }, meterOutput { -1.0f }, meterTarget { -1.0f };

private:
    int chooseTarget (float m) const
    {
        if (noTargets)
            return -1;
        int best = -1;
        float bestDist = 1e9f;
        const int centre = (int) std::floor (m);
        for (int n = centre - 13; n <= centre + 14; ++n)
        {
            if (! allowed[(size_t) (((n % 12) + 12) % 12)])
                continue;
            const float dist = std::fabs ((float) n - m);
            if (dist < bestDist) { bestDist = dist; best = n; }
        }
        // histéresis: evita saltar entre dos notas cuando la voz está en el límite
        if (target >= 0 && allowed[(size_t) (((target % 12) + 12) % 12)]
            && std::fabs ((float) target - m) <= bestDist + 0.15f)
            return target;
        return best;
    }

    void onHop()
    {
        const auto r = detector.analyze (0.0032f);
        const float hopSec = (float) hop / (float) fs;

        if (r.voiced)
        {
            const float m = 69.0f + 12.0f * std::log2 (r.f0 / p.refA4);
            if (! wasVoiced)
            {
                mh[0] = mh[1] = mh[2] = m;
                mSlow = m;
                c = 0.0f;
                holdTime = 0.0f;
                target = -1;
                periodIn = (float) (fs / r.f0);
            }
            mh[2] = mh[1]; mh[1] = mh[0]; mh[0] = m;
            const float mm = std::max (std::min (mh[0], mh[1]), std::min (std::max (mh[0], mh[1]), mh[2]));
            wasVoiced = true;
            voicedHold = 4;

            // Componente lenta ("nota cantada") vs vibrato
            if (std::fabs (mm - mSlow) > 0.9f)
                mSlow = mm;
            else
                mSlow += (1.0f - std::exp (-hopSec / 0.12f)) * (mm - mSlow);
            const float vib = mm - mSlow;

            const int tgt = chooseTarget (mSlow);
            if (tgt != target) { target = tgt; holdTime = 0.0f; }
            else holdTime += hopSec;

            float e = 0.0f;
            if (target >= 0)
            {
                e = (float) target - mSlow;
                const float tol = p.flexCents * 0.01f;
                const float ae = std::max (0.0f, std::fabs (e) - tol);
                e = std::copysign (ae, e);
            }
            const float hf = std::min (1.0f, holdTime / 0.4f);
            const float tauMs = p.retuneMs * (1.0f + 3.0f * p.humanize * hf);
            if (tauMs < 1.0f)
                c = e;
            else
                c += (1.0f - std::exp (-hopSec * 1000.0f / tauMs)) * (e - c);

            shiftSemis = c - (1.0f - p.vibrato) * vib + p.transposeSemis + p.detuneCents * 0.01f;

            const float tn = (float) (fs / r.f0);
            periodIn += 0.5f * (tn - periodIn);

            meterDetected.store (mm);
            meterOutput.store (mm + shiftSemis);
            meterTarget.store (target >= 0 ? (float) target : -1.0f);
        }
        else if (voicedHold > 0)
        {
            --voicedHold;
        }
        else
        {
            wasVoiced = false;
            periodIn = 0.0f;
            shiftSemis = 0.0f;
            target = -1;
            meterDetected.store (-1.0f);
            meterOutput.store (-1.0f);
            meterTarget.store (-1.0f);
        }
        ratioTarget = std::pow (2.0f, std::clamp (shiftSemis, -kMaxShiftSemis, kMaxShiftSemis) / 12.0f);
    }

    TuneParams p;
    PitchDetector detector;
    PsolaShifter shifter;
    std::array<bool, 12> allowed {};
    bool noTargets = false;

    double fs = 44100.0;
    int hop = 256, hopCount = 0, voicedHold = 0, target = -1;
    bool wasVoiced = false;
    float ratioCur = 1.0f, ratioTarget = 1.0f, ratioCoef = 0.01f;
    float formantCur = 1.0f, formantTarget = 1.0f, formantCoef = 0.001f;
    float periodIn = 0.0f, c = 0.0f, mSlow = 0.0f, holdTime = 0.0f, shiftSemis = 0.0f, lastDry = 0.0f;
    float mh[3] = { 0, 0, 0 };
};
} // namespace lunatic
