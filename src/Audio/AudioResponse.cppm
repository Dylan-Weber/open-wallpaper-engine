export module owe.audio_response;

import liffto;
import rstd;

export namespace owe::audio
{

using namespace rstd::prelude;

inline constexpr rstd::uint32_t kSampleRate   = 48000;
inline constexpr rstd::uint32_t kChannels     = 2;
inline constexpr rstd::size_t   kWindowFrames = 4096;
inline constexpr rstd::size_t   kSampleCount  = kWindowFrames * kChannels;
inline constexpr rstd::size_t   kResponseBins = 64;

struct PcmWindow {
    rstd::uint64_t                   generation;
    rstd::uint64_t                   sequence;
    rstd::uint64_t                   captured_at_ns;
    rstd::uint64_t                   end_sample_frame;
    rstd::uint32_t                   sample_rate_hz;
    rstd::uint32_t                   channels;
    rstd::uint32_t                   frames;
    rstd::array<float, kSampleCount> samples;
};

struct ResponseFrame {
    rstd::uint64_t                    generation;
    rstd::uint64_t                    sequence;
    rstd::uint64_t                    captured_at_ns;
    rstd::uint64_t                    end_sample_frame;
    rstd::array<float, kResponseBins> left;
    rstd::array<float, kResponseBins> right;
};

enum class AnalysisBackend
{
    LifftoScalar,
    LifftoSimd
};

class ResponseEngine {
public:
    ResponseEngine() = default;
    explicit ResponseEngine(AnalysisBackend backend): backend_(backend) {}
    auto backend() const noexcept -> AnalysisBackend { return backend_; }
    auto workspace_bytes() const noexcept -> rstd::size_t;
    bool analyze(const PcmWindow& window, ResponseFrame& output);
    void end();

private:
    AnalysisBackend                backend_ = AnalysisBackend::LifftoSimd;
    Option<liffto::Workspace<f32>> liffto_workspace_;
    rstd::uint64_t                 generation_ = 0;
    rstd::uint64_t                 sequence_   = 0;
    bool                           primed_     = false;
};

} // namespace owe::audio
