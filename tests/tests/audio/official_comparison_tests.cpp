#include <rstd/test/gtest.hpp>
import owe.audio_response;
import rstd;
import rstd.json;
using namespace rstd::prelude;
using namespace rstd::literals;

TEST(AudioComparison, OfficialReferenceReplay) {
    auto file = rstd::env::var("OWE_AUDIO_REPLAY"_str);
    if (file.is_err())
        GTEST_SKIP() << "set OWE_AUDIO_REPLAY to the prepared official comparison JSON";
    auto path     = rstd::path::PathBuf::from(file->as_str());
    auto text     = rstd::fs::read_to_string(path.as_path()).unwrap();
    auto document = rstd::json::from_str(text.as_str()).unwrap();
    auto frames   = document.as_array().unwrap();
    using owe::audio::AnalysisBackend;
    owe::audio::ResponseEngine simd, scalar(AnalysisBackend::LifftoScalar);
    double dot[2] {}, energy[2] {}, error[2] {}, official_energy = 0, maximum = 0;
    for (const auto& row : *frames) {
        owe::audio::PcmWindow window {};
        window.generation = 1;
        window.sequence   = (**row.get("sequence"_str)).as_u64().unwrap().to_primitive();
        window.end_sample_frame =
            (**row.get("end_sample_frame"_str)).as_u64().unwrap().to_primitive();
        window.captured_at_ns = window.end_sample_frame * 1000000000ULL / 48000;
        window.sample_rate_hz = 48000;
        window.channels       = 2;
        window.frames         = 4096;
        auto samples          = (**row.get("samples"_str)).as_array().unwrap();
        auto official         = (**row.get("official"_str)).as_array().unwrap();
        ASSERT_EQ(samples->len(), usize(8192));
        ASSERT_EQ(official->len(), usize(128));
        for (usize i; i < usize(8192); ++i)
            window.samples[i] = float((*samples)[i].as_f64().unwrap().to_primitive());
        owe::audio::ResponseFrame result[2] {};
        ASSERT_TRUE(simd.analyze(window, result[0]));
        ASSERT_TRUE(scalar.analyze(window, result[1]));
        for (int j = 0; j < 128; ++j) {
            double target = (*official)[usize(j)].as_f64().unwrap().to_primitive();
            official_energy += target * target;
            double baseline = j < 64 ? result[0].left[usize(j)] : result[0].right[usize(j - 64)];
            for (int b = 0; b < 2; ++b) {
                double value = j < 64 ? result[b].left[usize(j)] : result[b].right[usize(j - 64)];
                EXPECT_NEAR(value, baseline, 2e-5 + 2e-4 * f64(baseline).abs().to_primitive());
                maximum = rstd::cmp::max(maximum, f64(value - baseline).abs().to_primitive());
                dot[b] += value * target;
                energy[b] += value * value;
                error[b] += (value - target) * (value - target);
            }
        }
    }
    for (int b = 0; b < 2; ++b)
        rstd::io::println("official_replay backend={} frames={} cosine={} raw_relative_rmse={} "
                          "scalar_simd_max={}",
                          b,
                          frames->len(),
                          f64(dot[b]) / f64(energy[b] * official_energy).sqrt(),
                          f64(error[b] / official_energy).sqrt(),
                          f64(maximum));
}
