#pragma once
// LUNATIC TUNE - detector de tono monofónico (YIN sobre señal diezmada + refinado a tasa completa).
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace lunatic
{
class PitchDetector
{
public:
    struct Result
    {
        bool voiced = false;
        float f0 = 0.0f;
        float confidence = 0.0f;
    };

    void prepare (double sampleRate, float minHz, float maxHz = 1100.0f)
    {
        fs = sampleRate;
        dec = std::max (1, (int) std::lround (fs / 12000.0));
        fsd = fs / dec;
        tauMinD = std::max (2, (int) std::floor (fsd / maxHz));
        tauMaxD = (int) std::ceil (fsd / minHz) + 2;
        winD = 384;
        needD = winD + tauMaxD + 2;

        int ringD = 1;
        while (ringD < needD + 16) ringD <<= 1;
        histD.assign ((size_t) ringD, 0.0f);
        maskD = ringD - 1;

        winF = 256 * dec;
        const int tauMaxF = (tauMaxD + 2) * dec + dec + 8;
        needF = winF + tauMaxF + 8;
        int ringF = 1;
        while (ringF < needF + 16) ringF <<= 1;
        histF.assign ((size_t) ringF, 0.0f);
        maskF = ringF - 1;

        d.assign ((size_t) tauMaxD + 2, 0.0f);
        cm = d;
        scratchD.assign ((size_t) needD, 0.0f);
        scratchF.assign ((size_t) needF, 0.0f);

        if (dec > 1)
        {
            const double fc = 0.4 * fsd;
            lp[0].setLowpass (fc, fs, 0.7071);
            lp[1].setLowpass (fc, fs, 0.7071);
        }
        reset();
    }

    void reset()
    {
        std::fill (histD.begin(), histD.end(), 0.0f);
        std::fill (histF.begin(), histF.end(), 0.0f);
        wD = wF = 0;
        decCount = 0;
        lp[0].z1 = lp[0].z2 = lp[1].z1 = lp[1].z2 = 0.0;
    }

    void push (float x)
    {
        histF[(size_t) (wF & maskF)] = x;
        ++wF;
        float y = x;
        if (dec > 1)
            y = lp[1].process (lp[0].process (x));
        if (++decCount >= dec)
        {
            decCount = 0;
            histD[(size_t) (wD & maskD)] = y;
            ++wD;
        }
    }

    Result analyze (float gateLinear, float yinThreshold = 0.15f)
    {
        Result res;
        if (wD < needD || wF < needF)
            return res;

        const int N = needD;
        for (int i = 0; i < N; ++i)
            scratchD[(size_t) i] = histD[(size_t) ((wD - N + i) & maskD)];
        const float* x = scratchD.data();

        double e = 0.0;
        for (int j = N - winD; j < N; ++j)
            e += (double) x[j] * x[j];
        if (std::sqrt (e / winD) < gateLinear)
            return res;

        const int base = N - winD;
        float running = 0.0f;
        cm[0] = 1.0f;
        for (int tau = 1; tau <= tauMaxD; ++tau)
        {
            float s = 0.0f;
            const float* a = x + base;
            const float* b = x + base - tau;
            for (int j = 0; j < winD; ++j)
            {
                const float df = a[j] - b[j];
                s += df * df;
            }
            d[(size_t) tau] = s;
            running += s;
            cm[(size_t) tau] = running > 1e-12f ? s * (float) tau / running : 1.0f;
        }

        int tauEst = -1;
        for (int t = tauMinD; t < tauMaxD; ++t)
        {
            if (cm[(size_t) t] < yinThreshold)
            {
                while (t + 1 < tauMaxD && cm[(size_t) t + 1] < cm[(size_t) t])
                    ++t;
                tauEst = t;
                break;
            }
        }
        if (tauEst < 0)
        {
            int best = tauMinD;
            for (int t = tauMinD; t < tauMaxD; ++t)
                if (cm[(size_t) t] < cm[(size_t) best])
                    best = t;
            if (cm[(size_t) best] < 0.35f)
                tauEst = best;
            else
                return res;
        }

        float tauF = (float) tauEst;
        if (tauEst > 1 && tauEst < tauMaxD)
        {
            const float a = cm[(size_t) tauEst - 1], b = cm[(size_t) tauEst], c = cm[(size_t) tauEst + 1];
            const float den = a - 2.0f * b + c;
            if (std::fabs (den) > 1e-9f)
                tauF += std::clamp (0.5f * (a - c) / den, -0.5f, 0.5f);
        }
        res.confidence = 1.0f - cm[(size_t) tauEst];

        // Refinado a tasa completa alrededor de la estimación
        const int tau0 = (int) std::lround (tauF * dec);
        const int lo = std::max (2, tau0 - dec - 2);
        const int hi = tau0 + dec + 2;
        const int needNow = winF + hi + 2;
        if (needNow > needF)
        {
            res.voiced = true;
            res.f0 = (float) (fs / (tauF * dec));
            return res;
        }
        for (int i = 0; i < needNow; ++i)
            scratchF[(size_t) i] = histF[(size_t) ((wF - needNow + i) & maskF)];
        const float* xf = scratchF.data();
        const int baseF = needNow - winF;

        const int count = hi - lo + 1;
        float dfv[64];
        const int cnt = std::min (count, 64);
        int bestK = 0;
        for (int k = 0; k < cnt; ++k)
        {
            const int tau = lo + k;
            float s = 0.0f;
            const float* a = xf + baseF;
            const float* b = xf + baseF - tau;
            for (int j = 0; j < winF; ++j)
            {
                const float df = a[j] - b[j];
                s += df * df;
            }
            dfv[k] = s;
            if (s < dfv[bestK]) bestK = k;
        }
        float tauRef = (float) (lo + bestK);
        if (bestK > 0 && bestK < cnt - 1)
        {
            const float a = dfv[bestK - 1], b = dfv[bestK], c = dfv[bestK + 1];
            const float den = a - 2.0f * b + c;
            if (std::fabs (den) > 1e-12f)
                tauRef += std::clamp (0.5f * (a - c) / den, -0.5f, 0.5f);
        }
        else
        {
            tauRef = tauF * dec; // mínimo en el borde: nos quedamos con la estimación gruesa
        }

        res.voiced = true;
        res.f0 = (float) (fs / tauRef);
        return res;
    }

private:
    struct Biquad
    {
        double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
        void setLowpass (double fc, double fsr, double q)
        {
            const double w0 = 2.0 * 3.14159265358979323846 * fc / fsr;
            const double cw = std::cos (w0), alpha = std::sin (w0) / (2.0 * q);
            const double a0 = 1.0 + alpha;
            b0 = (1.0 - cw) * 0.5 / a0;
            b1 = (1.0 - cw) / a0;
            b2 = b0;
            a1 = -2.0 * cw / a0;
            a2 = (1.0 - alpha) / a0;
        }
        float process (float in)
        {
            const double y = b0 * in + z1;
            z1 = b1 * in - a1 * y + z2;
            z2 = b2 * in - a2 * y;
            return (float) y;
        }
    };

    double fs = 44100.0, fsd = 11025.0;
    int dec = 4, tauMinD = 10, tauMaxD = 100, winD = 384, needD = 0, winF = 1024, needF = 0;
    int decCount = 0;
    int64_t wD = 0, wF = 0, maskD = 0, maskF = 0;
    std::vector<float> histD, histF, d, cm, scratchD, scratchF;
    Biquad lp[2];
};
} // namespace lunatic
