// Copyright 2014 Emilie Gillet.
//
// Author: Emilie Gillet (emilie.o.gillet@gmail.com)
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//
// See http://creativecommons.org/licenses/MIT/ for more information.
//
// -----------------------------------------------------------------------------
//
// Circular buffer storing audio samples.

#ifndef FIRE_CLOUDS_DSP_AUDIO_BUFFER_H_
#define FIRE_CLOUDS_DSP_AUDIO_BUFFER_H_

#include "../../stmlib/stmlib.h"
#include "../../stmlib/dsp/dsp.h"
#include <algorithm>
#include <cmath>

namespace fire_clouds_vendor {
const int32_t kCrossFadeSize = 256;
const int32_t kInterpolationTail = 8;
enum Resolution { RESOLUTION_16_BIT };
enum InterpolationMethod { INTERPOLATION_ZOH, INTERPOLATION_LINEAR, INTERPOLATION_HERMITE };

// Fire adaptation: the normal 16-bit mode only. Valid-prefix tracking replaces
// clearing the audio allocation on every reset. Unwritten/expired taps are zero.
template<Resolution resolution>
class AudioBuffer {
 public:
  void Init(void* buffer, int32_t size, int16_t* tail) {
    samples_ = static_cast<int16_t*>(buffer);
    size_ = size - kInterpolationTail;
    tail_ = tail;
    Reset();
  }
  void Reset() {
    write_head_ = valid_samples_ = 0;
    tail_size_ = fade_position_ = 0;
    frozen_ = false;
  }
  void Write(float input) {
    // A nonzero first recorded sample otherwise appears halfway through a
    // later grain's window. Only the recording-start prefix receives this fade.
    if (valid_samples_ < 160) {
      const float fade = static_cast<float>(0.5 - 0.5 * std::cos(M_PI * valid_samples_ / 160.0));
      input *= fade;
    }
    samples_[write_head_] = Quantize(input);
    if (++write_head_ == size_) write_head_ = 0;
    valid_samples_ = std::min(valid_samples_ + 1, size_);
  }
  void WriteFade(const float* input, int32_t size, int32_t stride, bool write) {
    if (!write) {
      if (!frozen_) tail_size_ = fade_position_ = 0;
      frozen_ = true;
      while (size--) {
        if (tail_size_ < kCrossFadeSize) tail_[tail_size_++] = Quantize(*input);
        input += stride;
      }
      return;
    }
    if (frozen_) {
      frozen_ = false;
      fade_position_ = 0;
    }
    while (size--) {
      float sample = *input;
      if (tail_size_ != 0) {
        // Upstream indexed tail[256] at the last fade sample, and a short
        // freeze read unrecorded tail entries. Both bounds are explicit here.
        const int index = std::min(fade_position_, tail_size_ - 1);
        const float previous = static_cast<float>(tail_[index]) / 32768.0f;
        const float blend = static_cast<float>(fade_position_) / kCrossFadeSize;
        sample = previous + blend * (sample - previous);
        if (++fade_position_ == kCrossFadeSize) tail_size_ = 0;
      }
      Write(sample);
      input += stride;
    }
  }
  template<InterpolationMethod method>
  float Read(int32_t integral, uint16_t fractional) const {
    const float t = static_cast<float>(fractional) / 65536.0f;
    if (method == INTERPOLATION_ZOH) return ReadStored(integral);
    if (method == INTERPOLATION_LINEAR) {
      const float x0 = ReadStored(integral), x1 = ReadStored(integral + 1);
      return x0 + (x1 - x0) * t;
    }
    // Preserve the upstream Hermite tap convention and polynomial.
    const float xm1 = ReadStored(integral), x0 = ReadStored(integral + 1);
    const float x1 = ReadStored(integral + 2), x2 = ReadStored(integral + 3);
    const float c = (x1 - xm1) * 0.5f;
    const float v = x0 - x1;
    const float w = c + v;
    const float a = w + v + (x2 - x0) * 0.5f;
    const float b = w + a;
    return (((a * t - b) * t + c) * t + x0);
  }
  int32_t size() const { return size_; }
  int32_t head() const { return write_head_; }
  int32_t valid_samples() const { return valid_samples_; }
  // Fire project/preset state owns the samples. Restore the logical ring;
  // scheduled grains and feedback state restart independently of the material.
  void RestoreRecordingState(int32_t head, int32_t valid) {
    write_head_ = std::max(0, std::min(size_ - 1, head));
    valid_samples_ = std::max(0, std::min(size_, valid));
    tail_size_ = fade_position_ = 0;
    frozen_ = true;
  }
 private:
  static int16_t Quantize(float value) {
    if (!std::isfinite(value)) value = 0.0f;
    value = std::max(-1.0f, std::min(32767.0f / 32768.0f, value));
    return static_cast<int16_t>(value * 32768.0f);
  }
  float ReadStored(int32_t index) const {
    if (size_ <= 0) return 0.0f;
    // Normal scheduled grains stay within one wrap of the recording ring.
    if (index >= size_) index -= size_;
    else if (index < 0) index += size_;
    if (index < 0 || index >= size_) {
      index %= size_;
      if (index < 0) index += size_;
    }
    if (valid_samples_ < size_ && index >= valid_samples_) return 0.0f;
    return static_cast<float>(samples_[index]) / 32768.0f;
  }
  int16_t* samples_ = nullptr;
  int16_t* tail_ = nullptr;
  int32_t size_ = 0, write_head_ = 0, valid_samples_ = 0;
  int32_t tail_size_ = 0, fade_position_ = 0;
  bool frozen_ = false;
  DISALLOW_COPY_AND_ASSIGN(AudioBuffer);
 public:
  AudioBuffer() = default;
};
}
#endif
