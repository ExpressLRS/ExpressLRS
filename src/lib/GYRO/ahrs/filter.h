/*
 * CODE ADAPTED from RotorFlight
 *
 * This file is part of Rotorflight.
 *
 * Rotorflight is free software. You can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Rotorflight is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this software. If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

//#include <cstdint>
//#include <new>
//#include <utility>

#include "targets.h"

#define BUTTER_Q        0.707106781f     /* 2nd order Butterworth: 1/sqrt(2) */
#define BESSEL_Q        0.577350269f     /* 2nd order Bessel: 1/sqrt(3) */
#define DAMPED_Q        0.5f             /* 2nd order Critically damped: 1/sqrt(4) */

#define BUTTER_C        1.0f
#define BESSEL_C        1.272019649f
#define DAMPED_C        1.553773974f

#define BUTTER_4A_Q     0.541196100f     /* 4nd order Butterworth 1st section */
#define BUTTER_4B_Q     1.306562965f     /* 4nd order Butterworth 2nd section */

#define BUTTER_4A_C     1.0f
#define BUTTER_4B_C     1.0f

#define BESSEL_4A_Q     0.805538282f
#define BESSEL_4B_Q     0.521934582f

#define BESSEL_4A_C     1.603357516f
#define BESSEL_4B_C     1.430171560f

enum {
    LPF_NONE = 0,
    LPF_1ST_ORDER,      /* Default first order filter type */
    LPF_2ND_ORDER,      /* Default second order filter type */
    LPF_PT1,
    LPF_PT2,
    LPF_PT3,
    LPF_ORDER1,
    LPF_BUTTER,
    LPF_BESSEL,
    LPF_DAMPED,
};

enum {
    BIQUAD_NULL = 0,
    BIQUAD_LPF,
    BIQUAD_HPF,
    BIQUAD_BPF,
    BIQUAD_NOTCH,
};

enum {
    LPF_UPDATE  = BIT(0),
    LPF_EWMA    = BIT(1),
};

// Common interface implemented by every "simple" filter that only needs
// (cutoff, sampleRate) to reconfigure itself. This is what LowpassFilter
// dispatches through, replacing the old init/apply/update function pointer
// trio.
class FilterBase
{
public:
    virtual ~FilterBase() = default;

    virtual float apply(float input) = 0;
    virtual void update(float cutoff, float sampleRate) = 0;
    virtual float output() const = 0;
};


// NIL filter - passes the input straight through
class NilFilter final : public FilterBase
{
public:
    NilFilter() = default;
    NilFilter(float cutoff, float sampleRate) { init(cutoff, sampleRate); }

    void init(float cutoff, float sampleRate);

    float apply(float input) override { return y1 = input; }
    void update(float cutoff, float sampleRate) override;
    float output() const override { return y1; }

private:
    float y1 = 0;
};


// PT1 Low Pass filter
class PT1Filter final : public FilterBase
{
public:
    PT1Filter() = default;
    PT1Filter(float cutoff, float sampleRate) { init(cutoff, sampleRate); }

    static float gain(float cutoff, float sampleRate);

    void init(float cutoff, float sampleRate) { y1 = 0; filterGain = gain(cutoff, sampleRate); }
    void initGain(float g) { y1 = 0; filterGain = g; }
    void updateGain(float g) { filterGain = g; }

    void update(float cutoff, float sampleRate) override { filterGain = gain(cutoff, sampleRate); }
    float apply(float input) override { y1 += (input - y1) * filterGain; return y1; }
    float output() const override { return y1; }

private:
    float y1 = 0;
    float filterGain = 0;
};


// PT2 Low Pass filter (cascaded PT1 stages)
class PT2Filter final : public FilterBase
{
public:
    PT2Filter() = default;
    PT2Filter(float cutoff, float sampleRate) { init(cutoff, sampleRate); }

    static float gain(float cutoff, float sampleRate);

    void init(float cutoff, float sampleRate) { y1 = y2 = 0; filterGain = gain(cutoff, sampleRate); }
    void initGain(float g) { y1 = y2 = 0; filterGain = g; }
    void updateGain(float g) { filterGain = g; }

    void update(float cutoff, float sampleRate) override { filterGain = gain(cutoff, sampleRate); }
    float apply(float input) override;
    float output() const override { return y1; }

private:
    float y1 = 0, y2 = 0;
    float filterGain = 0;
};


// PT3 Low Pass filter (cascaded PT1 stages)
class PT3Filter final : public FilterBase
{
public:
    PT3Filter() = default;
    PT3Filter(float cutoff, float sampleRate) { init(cutoff, sampleRate); }

    static float gain(float cutoff, float sampleRate);

    void init(float cutoff, float sampleRate) { y1 = y2 = y3 = 0; filterGain = gain(cutoff, sampleRate); }
    void initGain(float g) { y1 = y2 = y3 = 0; filterGain = g; }
    void updateGain(float g) { filterGain = g; }

    void update(float cutoff, float sampleRate) override { filterGain = gain(cutoff, sampleRate); }
    float apply(float input) override;
    float output() const override { return y1; }

private:
    float y1 = 0, y2 = 0, y3 = 0;
    float filterGain = 0;
};


// EWMA1 Low Pass filter
class Ewma1Filter final : public FilterBase
{
public:
    Ewma1Filter() = default;
    Ewma1Filter(float cutoff, float sampleRate) { init(cutoff, sampleRate); }

    static float weight(float cutoff, float sampleRate);

    void init(float cutoff, float sampleRate) { y1 = 0; N = 0; W = weight(cutoff, sampleRate); }
    void initWeight(float w) { y1 = 0; N = 0; W = w; }

    void update(float cutoff, float sampleRate) override;
    void updateWeight(float w);

    float apply(float input) override;
    float output() const override { return y1; }

private:
    float y1 = 0;
    float W = 0;
    uint32_t N = 0;
};


// EWMA2 Low Pass filter (cascaded EWMA1 stages)
class Ewma2Filter final : public FilterBase
{
public:
    Ewma2Filter() = default;
    Ewma2Filter(float cutoff, float sampleRate) { init(cutoff, sampleRate); }

    static float weight(float cutoff, float sampleRate);

    void init(float cutoff, float sampleRate) { y1 = y2 = 0; N = 0; W = weight(cutoff, sampleRate); }
    void initWeight(float w) { y1 = y2 = 0; N = 0; W = w; }

    void update(float cutoff, float sampleRate) override;
    void updateWeight(float w);

    float apply(float input) override;
    float output() const override { return y1; }

private:
    float y1 = 0, y2 = 0;
    float W = 0;
    uint32_t N = 0;
};


// EWMA3 Low Pass filter (cascaded EWMA1 stages)
class Ewma3Filter final : public FilterBase
{
public:
    Ewma3Filter() = default;
    Ewma3Filter(float cutoff, float sampleRate) { init(cutoff, sampleRate); }

    static float weight(float cutoff, float sampleRate);

    void init(float cutoff, float sampleRate) { y1 = y2 = y3 = 0; N = 0; W = weight(cutoff, sampleRate); }
    void initWeight(float w) { y1 = y2 = y3 = 0; N = 0; W = w; }

    void update(float cutoff, float sampleRate) override;
    void updateWeight(float w);

    float apply(float input) override;
    float output() const override { return y1; }

private:
    float y1 = 0, y2 = 0, y3 = 0;
    float W = 0;
    uint32_t N = 0;
};


// Differentiator with bandwidth limit
class DiffFilter final : public FilterBase
{
public:
    DiffFilter() = default;
    DiffFilter(float cutoff, float sampleRate) { init(cutoff, sampleRate); }

    void init(float cutoff, float sampleRate) { x1 = 0; y1 = 0; update(cutoff, sampleRate); }
    void update(float cutoff, float sampleRate) override;
    float apply(float input) override;
    float output() const override { return y1; }

private:
    float x1 = 0, y1 = 0;
    float a = 0, b = 0;
};


// Bilinear (trapezoidal) integrator with output clamping.
// Not a FilterBase: reconfiguration needs (sampleRate, min, max), not (cutoff, sampleRate).
class IntegratorFilter
{
public:
    IntegratorFilter() = default;
    IntegratorFilter(float sampleRate, float min, float max) { init(sampleRate, min, max); }

    void init(float sampleRate, float min, float max) { reset(); update(sampleRate, min, max); }
    void reset() { x1 = 0; y1 = 0; }
    void update(float sampleRate, float min, float max);
    float apply(float input);
    float output() const { return y1; }

private:
    float x1 = 0, y1 = 0;
    float minVal = 0, maxVal = 0;
    float filterGain = 0;
};


// First order LPF/HPF. Same difference-equation shape as the C order1Filter_t;
// which one it behaves as is fixed at init() time via Mode.
class FirstOrderFilter final : public FilterBase
{
public:
    enum class Mode { Lowpass, Highpass };

    FirstOrderFilter() = default;
    FirstOrderFilter(Mode mode, float cutoff, float sampleRate, bool dynamic = true)
    {
        init(mode, cutoff, sampleRate, dynamic);
    }

    void init(Mode mode, float cutoff, float sampleRate, bool dynamic = true);

    // update() is a no-op when constructed with dynamic=false, matching the
    // original LPF_UPDATE flag behaviour (fixed coefficients, cheaper apply).
    void update(float cutoff, float sampleRate) override;
    float apply(float input) override;
    float output() const override { return y1; }

private:
    void recompute(float cutoff, float sampleRate);

    Mode mode = Mode::Lowpass;
    bool dynamicUpdate = true;
    float x1 = 0, y1 = 0;
    float b0 = 0, b1 = 0, a1 = 0;
};


// BiQuad filter a.k.a. Second-Order-Section.
// General purpose building block used directly by NotchFilter/FilterStack,
// and wrapped by ButterworthLPF/BesselLPF/DampedLPF below. Not a FilterBase:
// reconfiguration needs (cutoff, sampleRate, Q, type), not just (cutoff, sampleRate).
class BiquadFilter
{
public:
    BiquadFilter() = default;
    BiquadFilter(float cutoff, float sampleRate, float Q, uint8_t filterType)
    {
        init(cutoff, sampleRate, Q, filterType);
    }

    void init(float cutoff, float sampleRate, float Q, uint8_t filterType);
    void update(float cutoff, float sampleRate, float Q, uint8_t filterType);

    float applyDF1(float input);
    float applyTF2(float input);

    float output() const { return y1; }

    // Applies a cascade of biquad sections in Transposed Direct Form 2.
    static float applyStack(BiquadFilter *sections, int count, float input)
    {
        for (int i = 0; i < count; i++)
            input = sections[i].applyTF2(input);
        return input;
    }

private:
    float x1 = 0, x2 = 0, y1 = 0, y2 = 0;
    float b0 = 0, b1 = 0, b2 = 0;
    float a1 = 0, a2 = 0;
};


// Generic Low-Pass Filter (LPF) adapters: fixed Q/gain-correction biquad
// sections exposed through the plain (cutoff, sampleRate) FilterBase interface.
// `dynamic` mirrors the original LPF_UPDATE flag: when false, coefficients are
// computed once and apply() uses the cheaper Transposed Direct Form 2; when
// true, update() recomputes coefficients each call and apply() uses Direct
// Form 1, which stays stable under changing coefficients.
class ButterworthLPF final : public FilterBase
{
public:
    ButterworthLPF() = default;
    ButterworthLPF(float cutoff, float sampleRate, bool dynamic = true) { init(cutoff, sampleRate, dynamic); }

    void init(float cutoff, float sampleRate, bool dynamic = true);
    void update(float cutoff, float sampleRate) override;
    float apply(float input) override;
    float output() const override { return biquad.output(); }

private:
    BiquadFilter biquad;
    bool dynamicUpdate = true;
};

class BesselLPF final : public FilterBase
{
public:
    BesselLPF() = default;
    BesselLPF(float cutoff, float sampleRate, bool dynamic = true) { init(cutoff, sampleRate, dynamic); }

    void init(float cutoff, float sampleRate, bool dynamic = true);
    void update(float cutoff, float sampleRate) override;
    float apply(float input) override;
    float output() const override { return biquad.output(); }

private:
    BiquadFilter biquad;
    bool dynamicUpdate = true;
};

class DampedLPF final : public FilterBase
{
public:
    DampedLPF() = default;
    DampedLPF(float cutoff, float sampleRate, bool dynamic = true) { init(cutoff, sampleRate, dynamic); }

    void init(float cutoff, float sampleRate, bool dynamic = true);
    void update(float cutoff, float sampleRate) override;
    float apply(float input) override;
    float output() const override { return biquad.output(); }

private:
    BiquadFilter biquad;
    bool dynamicUpdate = true;
};


// Polymorphic low-pass filter container, replacing the old filter_t union +
// function-pointer trio. Holds whichever concrete FilterBase the requested
// type needs, placement-new'd into inline storage so there is no heap use.
class LowpassFilter
{
public:
    LowpassFilter() = default;
    LowpassFilter(uint8_t type, float cutoff, float sampleRate, uint32_t flags = 0)
    {
        init(type, cutoff, sampleRate, flags);
    }

    ~LowpassFilter() { destroy(); }

    LowpassFilter(const LowpassFilter &) = delete;
    LowpassFilter &operator=(const LowpassFilter &) = delete;

    void init(uint8_t type, float cutoff, float sampleRate, uint32_t flags = 0);

    void update(float cutoff, float sampleRate) { if (impl) impl->update(cutoff, sampleRate); }
    float apply(float input) { return impl ? impl->apply(input) : input; }
    float output() const { return impl ? impl->output() : 0.0f; }

private:
    void destroy() { if (impl) { impl->~FilterBase(); impl = nullptr; } }

    template <typename T, typename... Args>
    T *construct(Args &&...args)
    {
        static_assert(sizeof(T) <= sizeof(Storage), "filter implementation too large for inline storage");
        return new (&storage) T(std::forward<Args>(args)...);
    }

    // Only ever used for its size/alignment; never actually constructed as a union.
    union Storage {
        NilFilter nil;
        PT1Filter pt1;
        PT2Filter pt2;
        PT3Filter pt3;
        Ewma1Filter ew1;
        Ewma2Filter ew2;
        Ewma3Filter ew3;
        FirstOrderFilter fos;
        ButterworthLPF butter;
        BesselLPF bessel;
        DampedLPF damped;
    };

    alignas(Storage) unsigned char storage[sizeof(Storage)];
    FilterBase *impl = nullptr;
};


// Notch filter: a BiquadFilter configured as BIQUAD_NOTCH, inert (pass-through)
// when constructed with cutoff/Q <= 0.
class NotchFilter
{
public:
    NotchFilter() = default;
    NotchFilter(float cutoff, float Q, float sampleRate, uint32_t flags = 0) { init(cutoff, Q, sampleRate, flags); }

    void init(float cutoff, float Q, float sampleRate, uint32_t flags = 0);
    void update(float cutoff, float Q, float sampleRate);
    float apply(float input);

    // Get notch filter Q given center frequency (f0) and lower cutoff frequency (f1)
    // Q = f0 / (f2 - f1) ; f2 = f0^2 / f1
    static float getQ(float centerFreq, float cutoffFreq)
    {
        return centerFreq * cutoffFreq / (centerFreq * centerFreq - cutoffFreq * cutoffFreq);
    }

private:
    BiquadFilter biquad;
    bool active = false;
    bool dynamicUpdate = false;
};


// Simple fixed-point lowpass filter based on integer math
class SimpleLowpassFilter
{
public:
    SimpleLowpassFilter() = default;
    SimpleLowpassFilter(int32_t beta, int32_t fpShift) { init(beta, fpShift); }

    void init(int32_t beta, int32_t fpShift) { fp = 0; this->beta = beta; this->fpShift = fpShift; }
    int32_t update(int32_t newVal);

private:
    int32_t fp = 0;
    int32_t beta = 0;
    int32_t fpShift = 0;
};
