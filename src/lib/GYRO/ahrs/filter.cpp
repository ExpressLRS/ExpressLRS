/*
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

#include <math.h>

#include "filter.h"

namespace {

constexpr float M_PIf    = 3.14159265358979323846f;
constexpr float M_2PIf   = 6.28318530717958647693f;
constexpr float M_1_2PIf = 0.15915494309189533577f;

inline float tan_approx(float x) { return tanf(x); }
inline float sin_approx(float x) { return sinf(x); }
inline float cos_approx(float x) { return cosf(x); }

inline float limitCutoff(float cutoff, float sampleRate)
{
    // 95% of Nyquist
    return fminf(cutoff, 0.475f * sampleRate);
}

} // namespace


// NIL filter

void NilFilter::init(float cutoff, float sampleRate)
{
    (void)cutoff;
    (void)sampleRate;
}

void NilFilter::update(float cutoff, float sampleRate)
{
    (void)cutoff;
    (void)sampleRate;
}


/*
 * PT1 Low Pass filter
 *
 * It is calculated like this:
 *
 *   yₙ = yₙ₋₁ + α(xₙ - yₙ₋₁)
 *
 * where
 *
 *   Fs = Sampling frequency
 *   Fc = Cutoff frequency
 *
 *            Fc
 *    ω = 2π⋅――――
 *            Fs
 *
 *            1          ω           Fc
 *    α = ――――――――― = ―――――――― = ――――――――――――
 *         1/ω + 1     1 + ω      Fc + Fs/2π
 *
 *
 * The transfer function is:
 *
 *                  α
 *   H(z) = ―――――――――――――――――
 *           1 - (1 - α)⋅z⁻¹
 *
 *
 * This is a first order low-pass filter, which is transformed
 * from s-domain to z-domain with the backwards difference method:
 *
 *         1     z - 1
 *    s ← ――― ⋅ ―――――――
 *         T       z
 *
 * This is also known as rectangular integration.
 *
 * Like the bilinear transform, it is warping frequencies, and
 * the actual -3dB cutoff frequency can be calculated with
 *
 *          Fs         ⎡         α²    ⎤
 *    Fg = ―――― ⋅ acos ⎢1 - ―――――――――――⎥
 *          2π         ⎣     2⋅(1 - α) ⎦
 *
 *
 * Or, α can be calculated from the required cutoff frequency:
 *
 *     α = cosω - 1 + √(cos²ω - 4⋅cosω + 3)
 *
 *
 * This could be used for correcting the frequency warping, like it is
 * done with the bilinear transform.
 *
 * HOWEVER!
 *
 * It is NOT done here, because:
 *
 *   - PTx filters have poor performance when cutoff frequency Fc is higher than Fs/10
 *   - Pre-warping would limit the maximum cutoff to around 83% of Nyquist
 *   - Nobody is using pre-warping with PT filters
 *   - It would be surprising for developers
 *
 */

float PT1Filter::gain(float cutoff, float sampleRate)
{
    cutoff = limitCutoff(cutoff, sampleRate);

    const float gamma = M_1_2PIf * sampleRate;
    const float alpha = cutoff / (cutoff + gamma);

    return fminf(alpha, 1.0f);
}


// PT2 Low Pass filter

float PT2Filter::gain(float cutoff, float sampleRate)
{
    // order=2: 1 / sqrt( (2^(1 / order) - 1)) = 1.553773974
    return PT1Filter::gain(cutoff * 1.553773974f, sampleRate);
}

float PT2Filter::apply(float input)
{
    y2 += (input - y2) * filterGain;
    y1 += (y2   - y1) * filterGain;
    return y1;
}


// PT3 Low Pass filter

float PT3Filter::gain(float cutoff, float sampleRate)
{
    // order=3: 1 / sqrt( (2^(1 / order) - 1)) = 1.961459177
    return PT1Filter::gain(cutoff * 1.961459177f, sampleRate);
}

float PT3Filter::apply(float input)
{
    y3 += (input - y3) * filterGain;
    y2 += (y3   - y2) * filterGain;
    y1 += (y2   - y1) * filterGain;
    return y1;
}


// EWMA1 Low Pass filter

float Ewma1Filter::weight(float cutoff, float sampleRate)
{
    cutoff = limitCutoff(cutoff, sampleRate);

    const float gamma = M_1_2PIf * sampleRate;
    const float w = (cutoff + gamma) / cutoff;

    return fmaxf(w, 1.0f);
}

void Ewma1Filter::update(float cutoff, float sampleRate)
{
    W = weight(cutoff, sampleRate);
    if (N > W)
        N = W;
}

void Ewma1Filter::updateWeight(float w)
{
    W = w;
    if (N > W)
        N = W;
}

float Ewma1Filter::apply(float input)
{
    uint32_t count = N + 1;
    float weightNow = W;

    if (count < weightNow)
        weightNow = N = count;

    y1 += (input - y1) / weightNow;

    return y1;
}


// EWMA2 Low Pass filter

float Ewma2Filter::weight(float cutoff, float sampleRate)
{
    // order=2: 1 / sqrt( (2^(1 / order) - 1)) = 1.553773974
    return Ewma1Filter::weight(cutoff * 1.553773974f, sampleRate);
}

void Ewma2Filter::update(float cutoff, float sampleRate)
{
    W = weight(cutoff, sampleRate);
    if (N > W)
        N = W;
}

void Ewma2Filter::updateWeight(float w)
{
    W = w;
    if (N > W)
        N = W;
}

float Ewma2Filter::apply(float input)
{
    uint32_t count = N + 1;
    float weightNow = W;

    if (count < weightNow)
        weightNow = N = count;

    y2 += (input - y2) / weightNow;
    y1 += (y2   - y1) / weightNow;

    return y1;
}


// EWMA3 Low Pass filter

float Ewma3Filter::weight(float cutoff, float sampleRate)
{
    // order=3: 1 / sqrt( (2^(1 / order) - 1)) = 1.961459177
    return Ewma1Filter::weight(cutoff * 1.961459177f, sampleRate);
}

void Ewma3Filter::update(float cutoff, float sampleRate)
{
    W = weight(cutoff, sampleRate);
    if (N > W)
        N = W;
}

void Ewma3Filter::updateWeight(float w)
{
    W = w;
    if (N > W)
        N = W;
}

float Ewma3Filter::apply(float input)
{
    uint32_t count = N + 1;
    float weightNow = W;

    if (count < weightNow)
        weightNow = N = count;

    y3 += (input - y3) / weightNow;
    y2 += (y3   - y2) / weightNow;
    y1 += (y2   - y1) / weightNow;

    return y1;
}


/*
 * Differentiator with bandwidth limit
 *
 *   Fc = Cutoff frequency
 *   Fs = Sampling frequency
 *
 *   Wc = 2⋅π⋅Fc
 *
 *                Wc          Wc
 *  H(s) = s ⋅ ―――――――― = ――――――――――――
 *              s + Wc     1 + Wc⋅s⁻¹
 *
 *
 * Apply bilinear transform:
 *
 *          b₀ + b₁⋅z⁻¹
 *  H(z) = ―――――――――――――
 *          a₀ + a₁⋅z⁻¹
 *
 * Where
 *      b₀ = 2*Fs * K / (1 + K)
 *      b₁ = -b₀
 *      a₀ = 1
 *      a₁ = (1 - K) / (1 + K)
 *
 * And
 *       K = tan(π⋅Fc/Fs)
 *
 */

void DiffFilter::update(float cutoff, float sampleRate)
{
    cutoff = limitCutoff(cutoff, sampleRate / 2);

    const float W = tan_approx(M_PIf * cutoff / sampleRate);

    a = (W - 1) / (W + 1);
    b = 2 * sampleRate * W / (W + 1);
}

float DiffFilter::apply(float input)
{
    const float out = b * (input - x1) - a * y1;

    x1 = input;
    y1 = out;

    return out;
}


/*
 * Bilinear (trapezoidal) Integrator
 *
 *  Fs = Sampling frequency
 *
 *          1
 *  H(s) = ―――
 *          s
 *
 * Apply bilinear transform:
 *
 *          b₀ + b₁⋅z⁻¹        1       1 + z⁻¹
 *  H(z) = ―――――――――――――  =  ―――――― ⋅ ―――――――――
 *          a₀ + a₁⋅z⁻¹       2⋅Fs     1 - z⁻¹
 *
 * Where
 *      b₀ = 1/(2⋅Fs)
 *      b₁ = b₀
 *      a₀ = 1
 *      a₁ = 1
 */

void IntegratorFilter::update(float sampleRate, float min, float max)
{
    minVal = min;
    maxVal = max;
    filterGain = 1.0f / (2 * sampleRate);
}

float IntegratorFilter::apply(float input)
{
    float out = y1 + (input + x1) * filterGain;

    out = constrain(out, minVal, maxVal);

    x1 = input;
    y1 = out;

    return out;
}


// First order filters

void FirstOrderFilter::init(Mode filterMode, float cutoff, float sampleRate, bool dynamic)
{
    x1 = 0;
    y1 = 0;
    mode = filterMode;
    dynamicUpdate = dynamic;
    recompute(cutoff, sampleRate);
}

void FirstOrderFilter::update(float cutoff, float sampleRate)
{
    if (dynamicUpdate)
        recompute(cutoff, sampleRate);
}

void FirstOrderFilter::recompute(float cutoff, float sampleRate)
{
    if (mode == Mode::Lowpass) {
        cutoff = limitCutoff(cutoff, sampleRate);

        const float W = tan_approx(M_PIf * cutoff / sampleRate);

        a1 = (W - 1) / (W + 1);
        b0 = W / (W + 1);
        b1 = b0;
    } else {
        cutoff = limitCutoff(cutoff, sampleRate / 2);

        const float W = tan_approx(M_PIf * cutoff / sampleRate);

        a1 = (W - 1) / (W + 1);
        b0 = 1 / (W + 1);
        b1 = -b0;
    }
}

float FirstOrderFilter::apply(float input)
{
    const float out = b0 * input + b1 * x1 - a1 * y1;

    x1 = input;
    y1 = out;

    return out;
}


// BiQuad filter a.k.a. Second-Order-Section

void BiquadFilter::init(float cutoff, float sampleRate, float Q, uint8_t filterType)
{
    x1 = x2 = 0;
    y1 = y2 = 0;

    update(cutoff, sampleRate, Q, filterType);
}

void BiquadFilter::update(float cutoff, float sampleRate, float Q, uint8_t filterType)
{
    cutoff = limitCutoff(cutoff, sampleRate);

    const float omega = M_2PIf * cutoff / sampleRate;
    const float sinom = sin_approx(omega);
    const float cosom = cos_approx(omega);
    const float alpha = sinom / (2 * Q);

    switch (filterType) {
        case BIQUAD_LPF:
            b1 = 1 - cosom;
            b0 = b1 / 2;
            b2 = b0;
            a1 = -2 * cosom;
            a2 = 1 - alpha;
            break;

        case BIQUAD_HPF:
            b0 = (1 + cosom) / 2;
            b1 = -1 - cosom;
            b2 = b0;
            a1 = -2 * cosom;
            a2 = 1 - alpha;
            break;

        case BIQUAD_BPF:
            b0 = alpha;
            b1 = 0;
            b2 = -alpha;
            a1 = -2 * cosom;
            a2 = 1 - alpha;
            break;

        case BIQUAD_NOTCH:
            b0 = 1;
            b1 = -2 * cosom;
            b2 = 1;
            a1 = b1;
            a2 = 1 - alpha;
            break;
    }

    const float a0 = 1 + alpha;

    b0 /= a0;
    b1 /= a0;
    b2 /= a0;
    a1 /= a0;
    a2 /= a0;
}

float BiquadFilter::applyDF1(float input)
{
    const float out =
        b0 * input +
        b1 * x1 +
        b2 * x2 -
        a1 * y1 -
        a2 * y2;

    x2 = x1;
    x1 = input;
    y2 = y1;
    y1 = out;

    return out;
}

float BiquadFilter::applyTF2(float input)
{
    const float out = b0 * input + x1;

    x1 = b1 * input - a1 * out + x2;
    x2 = b2 * input - a2 * out;

    y1 = out;

    return out;
}


// Generic Low-Pass Filter (LPF) adapters

void ButterworthLPF::init(float cutoff, float sampleRate, bool dynamic)
{
    dynamicUpdate = dynamic;
    biquad.init(BUTTER_C * cutoff, sampleRate, BUTTER_Q, BIQUAD_LPF);
}

void ButterworthLPF::update(float cutoff, float sampleRate)
{
    if (dynamicUpdate)
        biquad.update(BUTTER_C * cutoff, sampleRate, BUTTER_Q, BIQUAD_LPF);
}

float ButterworthLPF::apply(float input)
{
    return dynamicUpdate ? biquad.applyDF1(input) : biquad.applyTF2(input);
}

void BesselLPF::init(float cutoff, float sampleRate, bool dynamic)
{
    dynamicUpdate = dynamic;
    biquad.init(BESSEL_C * cutoff, sampleRate, BESSEL_Q, BIQUAD_LPF);
}

void BesselLPF::update(float cutoff, float sampleRate)
{
    if (dynamicUpdate)
        biquad.update(BESSEL_C * cutoff, sampleRate, BESSEL_Q, BIQUAD_LPF);
}

float BesselLPF::apply(float input)
{
    return dynamicUpdate ? biquad.applyDF1(input) : biquad.applyTF2(input);
}

void DampedLPF::init(float cutoff, float sampleRate, bool dynamic)
{
    dynamicUpdate = dynamic;
    biquad.init(DAMPED_C * cutoff, sampleRate, DAMPED_Q, BIQUAD_LPF);
}

void DampedLPF::update(float cutoff, float sampleRate)
{
    if (dynamicUpdate)
        biquad.update(DAMPED_C * cutoff, sampleRate, DAMPED_Q, BIQUAD_LPF);
}

float DampedLPF::apply(float input)
{
    return dynamicUpdate ? biquad.applyDF1(input) : biquad.applyTF2(input);
}


// Polymorphic low-pass filter container

void LowpassFilter::init(uint8_t type, float cutoff, float sampleRate, uint32_t flags)
{
    destroy();

    if (cutoff == 0 || sampleRate == 0)
        type = LPF_NONE;

    const bool dynamic = (flags & LPF_UPDATE) != 0;

    switch (type) {
        case LPF_PT1:
            if (flags & LPF_EWMA)
                impl = construct<Ewma1Filter>(cutoff, sampleRate);
            else
                impl = construct<PT1Filter>(cutoff, sampleRate);
            break;

        case LPF_PT2:
            if (flags & LPF_EWMA)
                impl = construct<Ewma2Filter>(cutoff, sampleRate);
            else
                impl = construct<PT2Filter>(cutoff, sampleRate);
            break;

        case LPF_PT3:
            if (flags & LPF_EWMA)
                impl = construct<Ewma3Filter>(cutoff, sampleRate);
            else
                impl = construct<PT3Filter>(cutoff, sampleRate);
            break;

        case LPF_1ST_ORDER:
        case LPF_ORDER1:
            impl = construct<FirstOrderFilter>(FirstOrderFilter::Mode::Lowpass, cutoff, sampleRate, dynamic);
            break;

        case LPF_BUTTER:
            impl = construct<ButterworthLPF>(cutoff, sampleRate, dynamic);
            break;

        case LPF_2ND_ORDER:
        case LPF_BESSEL:
            impl = construct<BesselLPF>(cutoff, sampleRate, dynamic);
            break;

        case LPF_DAMPED:
            impl = construct<DampedLPF>(cutoff, sampleRate, dynamic);
            break;

        default:
            impl = construct<NilFilter>();
            break;
    }
}


// Notch filter

void NotchFilter::init(float cutoff, float Q, float sampleRate, uint32_t flags)
{
    dynamicUpdate = (flags & LPF_UPDATE) != 0;
    active = (cutoff > 0 && Q > 0);

    if (active)
        biquad.init(cutoff, sampleRate, Q, BIQUAD_NOTCH);
}

void NotchFilter::update(float cutoff, float Q, float sampleRate)
{
    if (active)
        biquad.update(cutoff, sampleRate, Q, BIQUAD_NOTCH);
}

float NotchFilter::apply(float input)
{
    if (!active)
        return input;

    return dynamicUpdate ? biquad.applyDF1(input) : biquad.applyTF2(input);
}


// Simple fixed-point lowpass filter based on integer math

int32_t SimpleLowpassFilter::update(int32_t newVal)
{
    fp = (fp << beta) - fp;
    fp += newVal << fpShift;
    fp >>= beta;
    return fp >> fpShift;
}
