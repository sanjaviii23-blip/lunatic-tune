// Pruebas offline del motor DSP (sin JUCE).  g++ -O2 -std=c++17 -I../Source/DSP dsp_test.cpp -o dsp_test
#include "AutoTuneEngine.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <random>

using namespace lunatic;

static const double kFs = 48000.0;
static const double kPi = 3.14159265358979323846;

struct Resonator
{
    double b0, a1, a2, z1 = 0, z2 = 0;
    Resonator (double f, double bw)
    {
        const double r = std::exp (-kPi * bw / kFs);
        a1 = -2 * r * std::cos (2 * kPi * f / kFs);
        a2 = r * r;
        b0 = 1 - r;
    }
    double process (double x) { const double y = b0 * x - a1 * z1 - a2 * z2; z2 = z1; z1 = y; return y; }
};

// Voz sintética: diente de sierra limitado en banda + dos formantes
struct Voice
{
    double phase = 0;
    Resonator f1 { 700, 130 }, f2 { 1200, 160 }, f3 { 2600, 250 };
    float next (double f0)
    {
        phase += f0 / kFs;
        phase -= std::floor (phase);
        double s = 0;
        const int H = (int) std::min (40.0, 5000.0 / f0);
        for (int h = 1; h <= H; ++h)
            s += std::sin (2 * kPi * h * phase) / h;
        s *= 0.6;
        return (float) (4.0 * (f1.process (s) + 0.6 * f2.process (s) + 0.3 * f3.process (s)));
    }
};

struct Measured { double meanCents = 0, stdCents = 0; int n = 0; };

static Measured measurePitch (const std::vector<float>& sig, double targetHz, int from)
{
    PitchDetector det;
    det.prepare (kFs, 65.0f);
    std::vector<double> cents;
    int c = 0;
    for (size_t i = 0; i < sig.size(); ++i)
    {
        det.push (sig[i]);
        if (++c >= 256)
        {
            c = 0;
            auto r = det.analyze (0.003f);
            if (r.voiced && (int) i > from)
                cents.push_back (1200.0 * std::log2 (r.f0 / targetHz));
        }
    }
    Measured m;
    m.n = (int) cents.size();
    if (m.n == 0) return m;
    for (double v : cents) m.meanCents += v;
    m.meanCents /= m.n;
    for (double v : cents) m.stdCents += (v - m.meanCents) * (v - m.meanCents);
    m.stdCents = std::sqrt (m.stdCents / m.n);
    return m;
}

static int failures = 0;
static void check (bool ok, const char* what)
{
    std::printf ("  [%s] %s\n", ok ? "OK " : "FALLO", what);
    if (! ok) ++failures;
}

int main()
{
    // ---- Test 1: nota constante 35 cents desafinada, hard tune -> debe caer en A3 = 220 Hz
    {
        std::printf ("Test 1: 35 cents de error, retune instantáneo (Do cromático)\n");
        AutoTuneEngine eng;
        eng.prepare (kFs, 1);
        TuneParams p; p.retuneMs = 0; p.vibrato = 0; eng.setParams (p);
        Voice v;
        const double f0 = 220.0 * std::pow (2.0, 35.0 / 1200.0);
        std::vector<float> out;
        for (int i = 0; i < (int) (kFs * 3); ++i) out.push_back (eng.process (v.next (f0)));
        auto m = measurePitch (out, 220.0, (int) (kFs * 1.0));
        std::printf ("  latencia = %d muestras (%.1f ms); error medio = %.2f cents, desviación = %.2f cents (n=%d)\n",
                     eng.latencySamples(), 1000.0 * eng.latencySamples() / kFs, m.meanCents, m.stdCents, m.n);
        check (std::fabs (m.meanCents) < 4.0, "error medio < 4 cents");
        check (m.stdCents < 6.0, "estabilidad < 6 cents");
    }

    // ---- Test 2: vibrato. Con vibrato=0 se aplana; con vibrato=1 se conserva
    for (int mode = 0; mode < 2; ++mode)
    {
        std::printf ("Test 2%c: vibrato 5.5 Hz ±40 cents sobre nota -25 cents, vibrato=%d\n", 'a' + mode, mode);
        AutoTuneEngine eng;
        eng.prepare (kFs, 1);
        TuneParams p; p.retuneMs = 5; p.vibrato = (float) mode; eng.setParams (p);
        Voice v;
        std::vector<float> out;
        for (int i = 0; i < (int) (kFs * 4); ++i)
        {
            const double t = i / kFs;
            const double cents = -25.0 + 40.0 * std::sin (2 * kPi * 5.5 * t);
            out.push_back (eng.process (v.next (261.6256 * std::pow (2.0, cents / 1200.0))));
        }
        auto m = measurePitch (out, 261.6256, (int) (kFs * 1.5));
        std::printf ("  error medio = %.2f cents, desviación (profundidad de vibrato) = %.2f cents\n", m.meanCents, m.stdCents);
        check (std::fabs (m.meanCents) < 8.0, "centrado en Do4 (|error medio| < 8 cents)");
        if (mode == 0) check (m.stdCents < 12.0, "vibrato aplanado (desviación < 12 cents)");
        else check (m.stdCents > 18.0, "vibrato conservado (desviación > 18 cents)");
    }

    // ---- Test 3: ruido sin tono -> pasa sin alterar el nivel
    {
        std::printf ("Test 3: ruido blanco (sin voz)\n");
        AutoTuneEngine eng;
        eng.prepare (kFs, 1);
        TuneParams p; p.retuneMs = 0; eng.setParams (p);
        std::mt19937 rng (7);
        std::normal_distribution<float> nd (0.0f, 0.1f);
        double ei = 0, eo = 0;
        const int N = (int) (kFs * 2);
        std::vector<float> in (N);
        for (int i = 0; i < N; ++i) in[(size_t) i] = nd (rng);
        std::vector<float> out (N);
        for (int i = 0; i < N; ++i) out[(size_t) i] = eng.process (in[(size_t) i]);
        const int lat = eng.latencySamples();
        double err = 0;
        for (int i = (int) kFs; i < N; ++i)
        {
            ei += in[(size_t) i] * in[(size_t) i];
            eo += out[(size_t) i] * out[(size_t) i];
            const double d = out[(size_t) i] - in[(size_t) (i - lat)];
            err += d * d;
        }
        std::printf ("  RMS entrada %.4f, salida %.4f, error vs. entrada retardada = %.1f dB\n",
                     std::sqrt (ei / (N - kFs)), std::sqrt (eo / (N - kFs)), 10 * std::log10 (err / ei + 1e-30));
        check (std::fabs (10 * std::log10 (eo / ei)) < 1.0, "nivel conservado (±1 dB)");
        check (10 * std::log10 (err / ei + 1e-30) < -30.0, "reconstrucción transparente (< -30 dB)");
    }

    // ---- Test 4: cambio de nota, velocidad de retune 120 ms
    {
        std::printf ("Test 4: salto de A3 a C4 con escala de Do mayor, retune 120 ms\n");
        AutoTuneEngine eng;
        eng.prepare (kFs, 1);
        TuneParams p; p.retuneMs = 120; p.vibrato = 1; p.scale = 1; p.key = 0; eng.setParams (p);
        Voice v;
        std::vector<float> out;
        const double a = 220.0 * std::pow (2.0, 30 / 1200.0), cNote = 261.6256 * std::pow (2.0, -30 / 1200.0);
        for (int i = 0; i < (int) (kFs * 4); ++i)
            out.push_back (eng.process (v.next (i < kFs * 2 ? a : cNote)));
        std::vector<float> tail (out.begin() + (int) (kFs * 3.2), out.end());
        auto m = measurePitch (tail, 261.6256, 0);
        std::printf ("  tras el salto: error medio = %.2f cents (desv. %.2f)\n", m.meanCents, m.stdCents);
        check (std::fabs (m.meanCents) < 6.0, "converge a Do4 (|error| < 6 cents)");
    }

    // ---- Rendimiento
    {
        AutoTuneEngine eng;
        eng.prepare (kFs, 1);
        TuneParams p; p.retuneMs = 20; eng.setParams (p);
        Voice v;
        const int N = (int) (kFs * 10);
        std::vector<float> in (N);
        for (int i = 0; i < N; ++i) in[(size_t) i] = v.next (200.0 * std::pow (2.0, 0.2 * std::sin (i / kFs)));
        const auto t0 = std::chrono::steady_clock::now();
        float acc = 0;
        for (int i = 0; i < N; ++i) acc += eng.process (in[(size_t) i]);
        const double sec = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
        std::printf ("Rendimiento: 10 s de audio en %.3f s => %.1f %% de un núcleo (acc=%g)\n", sec, 100.0 * sec / 10.0, acc);
        check (sec < 2.0, "tiempo real con margen amplio");
    }

    std::printf (failures == 0 ? "\nTODAS LAS PRUEBAS OK\n" : "\n%d PRUEBA(S) FALLARON\n", failures);
    return failures == 0 ? 0 : 1;
}
