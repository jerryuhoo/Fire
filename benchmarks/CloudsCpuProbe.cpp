#include "DSP/Clouds/CloudsEngine.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <vector>

int main()
{
    using Clock = std::chrono::steady_clock;
    for (double rate : {48000.0, 96000.0, 192000.0})
        for (int mode = 0; mode < 3; ++mode)
        {
            fire::effects::CloudsEngine engine;
            const auto prepareStart = Clock::now();
            engine.prepare(rate);
            const auto prepareMs = std::chrono::duration<double, std::milli>(Clock::now() - prepareStart).count();
            fire::effects::CloudsParameters p;
            if (mode == 1) p.density = 1.0f;
            if (mode == 2)
            {
                p.density = 1.0f; p.size = 1.0f; p.texture = 1.0f;
                p.feedback = 1.0f; p.reverb = 1.0f; p.pitch = 24.0f;
            }
            const auto count = static_cast<int>(rate * 2.0);
            std::vector<float> inL(static_cast<size_t>(count)), inR(static_cast<size_t>(count));
            for (int i = 0; i < count; ++i)
            {
                inL[static_cast<size_t>(i)] = static_cast<float>(.12 * std::sin(i * 6.283185307179586 * 110.0 / rate));
                inR[static_cast<size_t>(i)] = static_cast<float>(.1 * std::sin(i * 6.283185307179586 * 173.0 / rate));
            }
            for (int i = 0; i < count / 2; ++i)
            {
                auto l = inL[static_cast<size_t>(i)], r = inR[static_cast<size_t>(i)];
                engine.process(l, r, p);
            }
            std::vector<double> times;
            double sum = 0;
            for (int run = 0; run < 5; ++run)
            {
                const auto start = Clock::now();
                for (int i = 0; i < count; ++i)
                {
                    auto l = inL[static_cast<size_t>(i)], r = inR[static_cast<size_t>(i)];
                    engine.process(l, r, p);
                    sum += l + r;
                }
                times.push_back(std::chrono::duration<double>(Clock::now() - start).count());
            }
            std::sort(times.begin(), times.end());
            std::printf("rate=%.0f mode=%d prepare_ms=%.2f block128_us=%.2f core_percent=%.3f checksum=%.5f\n",
                rate, mode, prepareMs, times[2] * 1e6 * 128.0 / count, times[2] / 2.0 * 100.0, sum);
        }
}
