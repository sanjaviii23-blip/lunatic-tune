#pragma once
// LUNATIC TUNE - cambio de tono en tiempo real tipo PSOLA (síntesis síncrona al tono).
// Conserva los formantes de forma natural y permite desplazarlos de forma independiente.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace lunatic
{
class PsolaShifter
{
public:
    void prepare (double sampleRate, float minHz, float maxFormantRatio)
    {
        fs = sampleRate;
        tMax = (int) std::ceil (fs / minHz);
        defaultT = std::min ((int) std::lround (fs * 0.010), tMax);
        defaultT = std::max (defaultT, 32);
        const float maxQ = std::max (1.0f, maxFormantRatio);
        D = (int) std::ceil ((1.0f + maxQ) * (float) tMax + (float) tMax * 0.125f) + 8;

        int sz = 1;
        while (sz < 4 * (D + 2 * tMax) + 4096) sz <<= 1;
        in.assign ((size_t) sz, 0.0f);
        out.assign ((size_t) sz, 0.0f);
        wt.assign ((size_t) sz, 0.0f);
        mask = sz - 1;
        reset();
    }

    int latency() const { return D; }

    void reset()
    {
        std::fill (in.begin(), in.end(), 0.0f);
        std::fill (out.begin(), out.end(), 0.0f);
        std::fill (wt.begin(), wt.end(), 0.0f);
        inIdx = 0;
        outIdx = 0;
        nextSynth = (double) defaultT;
        epochValid = false;
        pol = 1;
    }

    // periodIn: periodo de entrada en muestras (0 = sin voz). ratio: factor de tono. q: factor de formante.
    float process (float x, float periodIn, float ratio, float q)
    {
        in[(size_t) (inIdx & mask)] = x;
        ++inIdx;

        const bool voiced = periodIn > 0.0f;
        const float T = voiced ? std::clamp (periodIn, 24.0f, (float) tMax) : (float) defaultT;
        const float r = voiced ? std::clamp (ratio, 0.6f, 1.7f) : 1.0f;
        const float qq = voiced ? std::clamp (q, 0.7f, 1.0f * maxQClamp()) : 1.0f;
        const int half = voiced ? (int) std::lround (T) : defaultT;

        while (nextSynth - (double) half <= (double) outIdx)
        {
            placeGrain (nextSynth, half, T, qq, voiced);
            nextSynth += voiced ? (double) T / (double) r : (double) defaultT;
        }

        const size_t i = (size_t) (outIdx & mask);
        const float w = wt[i];
        const float y = w > 1e-3f ? out[i] / w : 0.0f;
        out[i] = 0.0f;
        wt[i] = 0.0f;
        ++outIdx;
        return y;
    }

    // Muestra de entrada alineada (retardada D) con la salida más reciente
    float delayedDry() const { return in[(size_t) ((inIdx - 1 - D) & mask)]; }

private:
    static constexpr float maxQClamp() { return 1.34f; }

    static float hermite (const float* buf, int64_t m, double p)
    {
        const int64_t i = (int64_t) std::floor (p);
        const float f = (float) (p - (double) i);
        const float y0 = buf[(i - 1) & m], y1 = buf[i & m], y2 = buf[(i + 1) & m], y3 = buf[(i + 2) & m];
        const float c1 = 0.5f * (y2 - y0);
        const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
        const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
        return ((c3 * f + c2) * f + c1) * f + y1;
    }

    // Busca el pulso glotal (epoch) más cercano manteniendo una cadena coherente
    double findEpoch (double aNom, float T, bool& ok)
    {
        ok = true;
        const int r = std::max (2, (int) (T * 0.125f));
        auto refine = [&] (int64_t centre, int radius) -> double
        {
            int64_t pk = centre;
            float best = -1e30f;
            for (int64_t i = centre - radius; i <= centre + radius; ++i)
            {
                const float v = (float) pol * in[(size_t) (i & mask)];
                if (v > best) { best = v; pk = i; }
            }
            const float y0 = (float) pol * in[(size_t) ((pk - 1) & mask)];
            const float y1 = (float) pol * in[(size_t) (pk & mask)];
            const float y2 = (float) pol * in[(size_t) ((pk + 1) & mask)];
            const float den = y0 - 2.0f * y1 + y2;
            float off = std::fabs (den) > 1e-12f ? 0.5f * (y0 - y2) / den : 0.0f;
            off = std::clamp (off, -0.5f, 0.5f);
            return (double) pk + (double) off;
        };

        if (epochValid && aNom - lastEpoch >= 0.0 && aNom - lastEpoch < 4.0 * (double) T)
        {
            const double m = std::floor ((aNom - lastEpoch) / (double) T);
            const double pred = lastEpoch + m * (double) T;
            const double e = refine ((int64_t) std::llround (pred), r);
            if (std::fabs (e - pred) <= (double) r + 1.0)
            {
                lastEpoch = e;
                return e;
            }
        }

        // (re)inicio de la cadena: pico dominante en el último periodo
        const int64_t c = (int64_t) std::llround (aNom - 0.5 * (double) T);
        float mx = -1e30f, mn = 1e30f;
        for (int64_t i = c - (int64_t) (T * 0.5f); i <= c + (int64_t) (T * 0.5f); ++i)
        {
            const float v = in[(size_t) (i & mask)];
            mx = std::max (mx, v);
            mn = std::min (mn, v);
        }
        pol = (mx >= -mn) ? 1 : -1;
        const double e = refine (c, (int) (T * 0.5f));
        lastEpoch = e;
        epochValid = true;
        return e;
    }

    void placeGrain (double Cf, int half, float T, float q, bool voiced)
    {
        double a = Cf - (double) D;
        if (voiced)
        {
            bool ok = true;
            a = findEpoch (a, T, ok);
        }
        else
        {
            epochValid = false;
        }

        const double pMax = (double) (inIdx - 3);
        const double pMin = (double) (inIdx - (int64_t) in.size() + 16);
        const int64_t k0 = std::max<int64_t> ((int64_t) std::ceil (Cf - half), outIdx);
        const int64_t k1 = (int64_t) std::floor (Cf + half);
        const float invHalf = 3.14159265f / (float) half;
        for (int64_t k = k0; k <= k1; ++k)
        {
            const double u = (double) k - Cf;
            const float w = 0.5f * (1.0f + std::cos (invHalf * (float) u));
            const double p = std::clamp (a + u * (double) q, pMin, pMax);
            const float s = hermite (in.data(), mask, p);
            out[(size_t) (k & mask)] += w * s;
            wt[(size_t) (k & mask)] += w;
        }
    }

    double fs = 44100.0;
    int tMax = 700, defaultT = 441, D = 1500;
    int64_t mask = 0, inIdx = 0, outIdx = 0;
    double nextSynth = 0.0, lastEpoch = 0.0;
    bool epochValid = false;
    int pol = 1;
    std::vector<float> in, out, wt;
};
} // namespace lunatic
