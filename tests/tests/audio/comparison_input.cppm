export module owe.audio_test_input;
import owe.audio_response;
import rstd;

export namespace owe::audio_test
{
using namespace rstd::prelude;
inline auto window(int pattern, rstd::uint64_t sequence = 1) -> audio::PcmWindow {
    audio::PcmWindow w {};
    w.generation       = 1;
    w.sequence         = sequence;
    w.captured_at_ns   = sequence * 10000000;
    w.end_sample_frame = sequence * 480;
    w.sample_rate_hz   = audio::kSampleRate;
    w.channels         = audio::kChannels;
    w.frames           = audio::kWindowFrames;
    for (rstd::size_t j = 0; j < audio::kWindowFrames; ++j) {
        float left = 0, right = 0;
        auto  time = f32(j) / f32(audio::kSampleRate);
        if (pattern == 1 || pattern == 2) {
            left  = (f32(0.2) * (f32::consts::TAU * f32(110) * time).sin()).to_primitive();
            right = (f32(0.1) * (f32::consts::TAU * f32(1760) * time).sin()).to_primitive();
            if (pattern == 2)
                left += (f32(0.05) * (f32::consts::TAU * f32(3200) * time).sin()).to_primitive();
        }
        if (pattern == 3 && j == audio::kWindowFrames - 1) left = 1;
        if (pattern == 4 && j == audio::kWindowFrames - 1392) left = 1;
        if (pattern == 5 && j == audio::kWindowFrames - 1393) left = 1;
        if (pattern == 6) {
            left  = __builtin_nanf("");
            right = __builtin_inff();
        }
        if (pattern == 7 && j == audio::kWindowFrames - 1) left = -1;
        if (pattern == 8 && j == audio::kWindowFrames - 1) left = -0.9999f;
        w.samples[usize(2 * j)]     = left;
        w.samples[usize(2 * j + 1)] = right;
    }
    return w;
}
} // namespace owe::audio_test
