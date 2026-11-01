#pragma once
#include <JuceHeader.h>
#include <vector>
#include <cmath>

// Détection automatique du BPM et de la tonalité.
namespace Analyzer
{
    inline void toMono (const juce::AudioBuffer<float>& b, std::vector<float>& mono)
    {
        const int n = b.getNumSamples(), nch = b.getNumChannels();
        mono.assign ((size_t) n, 0.0f);
        for (int ch = 0; ch < nch; ++ch)
        {
            auto* d = b.getReadPointer (ch);
            for (int i = 0; i < n; ++i) mono[(size_t) i] += d[i] / (float) nch;
        }
    }

    // BPM : flux spectral -> autocorrélation -> pondération autour de 120 BPM
    // -> ajustement sur un nombre entier de temps (si le sample est une boucle découpée proprement).
    inline double detectBpm (const std::vector<float>& mono, double sr)
    {
        constexpr int order = 10, N = 1 << order, hop = 256;
        if (mono.size() < (size_t) (N * 8)) return 0.0;

        juce::dsp::FFT fft (order);
        juce::dsp::WindowingFunction<float> win ((size_t) N, juce::dsp::WindowingFunction<float>::hann);
        std::vector<float> data ((size_t) N * 2), prev ((size_t) N / 2 + 1, 0.0f), flux;

        for (size_t p = 0; p + N <= mono.size(); p += (size_t) hop)
        {
            std::fill (data.begin(), data.end(), 0.0f);
            std::copy (mono.begin() + (long) p, mono.begin() + (long) p + N, data.begin());
            win.multiplyWithWindowingTable (data.data(), (size_t) N);
            fft.performFrequencyOnlyForwardTransform (data.data());
            float f = 0.0f;
            for (int k = 0; k < N / 2 + 1; ++k)
            {
                const float m = std::log1p (data[(size_t) k]);
                const float d = m - prev[(size_t) k];
                if (d > 0.0f) f += d;
                prev[(size_t) k] = m;
            }
            flux.push_back (f);
        }

        const int F = (int) flux.size();
        if (F < 64) return 0.0;

        double mean = 0.0;
        for (auto v : flux) mean += v;
        mean /= F;
        for (auto& v : flux) v = juce::jmax (0.0f, v - (float) mean);

        const double fps = sr / hop;
        const int minLag = juce::jmax (2, (int) std::floor (fps * 60.0 / 200.0));
        int maxLag = (int) std::ceil (fps * 60.0 / 60.0);
        maxLag = juce::jmin (maxLag, F / 2 - 1);
        if (maxLag <= minLag + 2) return 0.0;

        std::vector<double> ac ((size_t) maxLag * 2 + 2, 0.0);
        for (int lag = minLag - 1; lag < (int) ac.size() && lag < F - 1; ++lag)
        {
            double s = 0.0;
            for (int i = 0; i + lag < F; ++i) s += (double) flux[(size_t) i] * flux[(size_t) (i + lag)];
            ac[(size_t) lag] = s / (F - lag);
        }

        int best = minLag;
        double bestScore = -1.0;
        for (int lag = minLag; lag <= maxLag; ++lag)
        {
            const double bpm = 60.0 * fps / lag;
            const double w = std::exp (-0.5 * std::pow (std::log2 (bpm / 120.0) / 0.9, 2.0));
            double sc = ac[(size_t) lag];
            if (lag * 2 < (int) ac.size()) sc += 0.5 * ac[(size_t) lag * 2];
            sc *= w;
            if (sc > bestScore) { bestScore = sc; best = lag; }
        }

        double lagRef = best;
        const double y0 = ac[(size_t) best - 1], y1 = ac[(size_t) best], y2 = ac[(size_t) best + 1];
        const double den = y0 - 2.0 * y1 + y2;
        if (std::abs (den) > 1e-12) lagRef += juce::jlimit (-1.0, 1.0, 0.5 * (y0 - y2) / den);

        double bpm = 60.0 * fps / lagRef;

        const double dur = (double) mono.size() / sr;
        const double n = std::round (bpm * dur / 60.0);
        if (n >= 1.0)
        {
            const double cand = 60.0 * n / dur;
            if (std::abs (cand - bpm) / bpm < 0.03) bpm = cand;
        }
        return bpm;
    }

    // Tonalité : chromagramme + profils de Krumhansl-Schmuckler.
    inline int detectKey (const std::vector<float>& mono, double sr, bool& isMinor)
    {
        constexpr int order = 13, N = 1 << order;
        isMinor = false;
        if (mono.size() < (size_t) N) return 0;

        juce::dsp::FFT fft (order);
        juce::dsp::WindowingFunction<float> win ((size_t) N, juce::dsp::WindowingFunction<float>::hann);
        std::vector<float> data ((size_t) N * 2);
        double chroma[12] = {};
        const size_t hop = juce::jmax ((size_t) N / 2, mono.size() / 400);

        for (size_t p = 0; p + N <= mono.size(); p += hop)
        {
            std::fill (data.begin(), data.end(), 0.0f);
            std::copy (mono.begin() + (long) p, mono.begin() + (long) p + N, data.begin());
            win.multiplyWithWindowingTable (data.data(), (size_t) N);
            fft.performFrequencyOnlyForwardTransform (data.data());
            for (int k = 1; k < N / 2; ++k)
            {
                const double f = k * sr / N;
                if (f < 55.0 || f > 2000.0) continue;
                const int midi = (int) std::lround (69.0 + 12.0 * std::log2 (f / 440.0));
                chroma[((midi % 12) + 12) % 12] += std::sqrt ((double) data[(size_t) k]);
            }
        }

        static const double maj[12] = { 6.35, 2.23, 3.48, 2.33, 4.38, 4.09, 2.52, 5.19, 2.39, 3.66, 2.29, 2.88 };
        static const double mnr[12] = { 6.33, 2.68, 3.52, 5.38, 2.60, 3.53, 2.54, 4.75, 3.98, 2.69, 3.34, 3.17 };

        auto corr = [&] (const double* prof, int tonic)
        {
            double mc = 0, mp = 0;
            for (int i = 0; i < 12; ++i) { mc += chroma[i]; mp += prof[i]; }
            mc /= 12; mp /= 12;
            double num = 0, dc = 0, dp = 0;
            for (int i = 0; i < 12; ++i)
            {
                const double a = chroma[(tonic + i) % 12] - mc, b = prof[i] - mp;
                num += a * b; dc += a * a; dp += b * b;
            }
            return num / (std::sqrt (dc * dp) + 1e-12);
        };

        double best = -2.0; int bestT = 0;
        for (int t = 0; t < 12; ++t)
        {
            const double cM = corr (maj, t), cm = corr (mnr, t);
            if (cM > best) { best = cM; bestT = t; isMinor = false; }
            if (cm > best) { best = cm; bestT = t; isMinor = true; }
        }
        return bestT;
    }

    // Position (en échantillons) de la première attaque : sert à aligner la grille sur le 1er temps.
    inline int detectFirstOnset (const std::vector<float>& mono, double sr)
    {
        constexpr int win = 128, hop = 64;
        if (mono.size() < (size_t) win * 4) return 0;

        std::vector<float> env;
        for (size_t p = 0; p + win <= mono.size(); p += hop)
        {
            double s = 0.0;
            for (int i = 0; i < win; ++i) s += (double) mono[p + (size_t) i] * mono[p + (size_t) i];
            env.push_back ((float) std::sqrt (s / win));
        }
        float mx = 0.0f;
        for (auto v : env) mx = juce::jmax (mx, v);
        if (mx <= 1e-6f) return 0;

        const float thr = 0.15f * mx;
        for (size_t i = 0; i < env.size(); ++i)
            if (env[i] > thr)
                return juce::jmax (0, (int) (i * hop) - (int) (0.005 * sr)); // 5 ms avant, pour ne pas couper l'attaque
        return 0;
    }
}
