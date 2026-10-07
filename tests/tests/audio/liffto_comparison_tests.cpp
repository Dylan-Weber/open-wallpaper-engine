#include <rstd/test/gtest.hpp>
import owe.audio_response;
import owe.scene_audio_response;
import owe.audio_test_input;
import liffto;
import rstd;
using namespace rstd::prelude;
using owe::audio::AnalysisBackend;
using rstd::size_t;

void CompareFrame(const owe::audio::ResponseFrame& a, const owe::audio::ResponseFrame& b) {
    EXPECT_EQ(a.generation, b.generation);
    EXPECT_EQ(a.sequence, b.sequence);
    EXPECT_EQ(a.captured_at_ns, b.captured_at_ns);
    EXPECT_EQ(a.end_sample_frame, b.end_sample_frame);
    for (usize i; i < usize(64); ++i) {
        EXPECT_NEAR(a.left[i], b.left[i], 2e-5f + 2e-4f * f32(a.left[i]).abs().to_primitive()) << i;
        EXPECT_NEAR(a.right[i], b.right[i], 2e-5f + 2e-4f * f32(a.right[i]).abs().to_primitive())
            << i;
    }
}
template<size_t N>
void CompareBands(const owe::scene_audio::ResolutionBuffers<N>& a,
                  const owe::scene_audio::ResolutionBuffers<N>& b) {
    for (usize i; i < usize(N); ++i) {
        EXPECT_NEAR(a.left[i], b.left[i], 2e-5f + 2e-4f * f32(a.left[i]).abs().to_primitive());
        EXPECT_NEAR(a.right[i], b.right[i], 2e-5f + 2e-4f * f32(a.right[i]).abs().to_primitive());
        EXPECT_NEAR(
            a.average[i], b.average[i], 2e-5f + 2e-4f * f32(a.average[i]).abs().to_primitive());
    }
}
TEST(AudioComparison, WindowsStateAndSceneHistory) {
    for (auto backend :
         array<AnalysisBackend, 2> { AnalysisBackend::LifftoScalar, AnalysisBackend::LifftoSimd }) {
        owe::audio::ResponseEngine reference, candidate(backend);
        EXPECT_EQ(reference.backend(), AnalysisBackend::LifftoSimd);
        EXPECT_EQ(candidate.workspace_bytes(), size_t(0));
        owe::scene_audio::ResponseProcessor reference_scene, new_scene;
        owe::audio::ResponseFrame           a {}, b {};
        for (int frame = 0; frame < 90; ++frame) {
            auto w = owe::audio_test::window(frame / 10, frame + 1);
            if (frame == 45) {
                reference.end();
                candidate.end();
                reference_scene.end();
                new_scene.end();
            }
            w.generation         = frame < 60 ? 1 : 2;
            const auto unchanged = w;
            ASSERT_TRUE(reference.analyze(w, a));
            ASSERT_TRUE(candidate.analyze(w, b));
            for (usize i; i < usize(owe::audio::kSampleCount); ++i)
                EXPECT_EQ(f32(w.samples[i]).to_bits(), f32(unchanged.samples[i]).to_bits());
            CompareFrame(a, b);
            if (frame < 10)
                EXPECT_EQ(candidate.workspace_bytes(), size_t(0));
            else
                EXPECT_EQ(candidate.workspace_bytes(), size_t(65536));
            EXPECT_FALSE(reference.analyze(w, a));
            EXPECT_FALSE(candidate.analyze(w, b));
            reference_scene.submit(a);
            new_scene.submit(b);
            for (float dt : array<float, 3> { 1.0f / 30, 1.0f / 120, 0.1f }) {
                owe::scene_audio::Buffers x {}, y {};
                ASSERT_TRUE(reference_scene.advance(dt, x));
                ASSERT_TRUE(new_scene.advance(dt, y));
                EXPECT_EQ(x.generation, y.generation);
                EXPECT_EQ(x.sequence, y.sequence);
                EXPECT_EQ(x.captured_at_ns, y.captured_at_ns);
                EXPECT_EQ(x.end_sample_frame, y.end_sample_frame);
                CompareBands(x.bands64, y.bands64);
                CompareBands(x.bands32, y.bands32);
                CompareBands(x.bands16, y.bands16);
            }
        }
        const auto saved   = b;
        auto       invalid = owe::audio_test::window(1, 100);
        invalid.generation = 0;
        EXPECT_FALSE(candidate.analyze(invalid, b));
        CompareFrame(saved, b);
        invalid.generation     = 3;
        invalid.sample_rate_hz = 44100;
        EXPECT_FALSE(candidate.analyze(invalid, b));
        CompareFrame(saved, b);
        invalid.sample_rate_hz = owe::audio::kSampleRate;
        invalid.channels       = 1;
        EXPECT_FALSE(candidate.analyze(invalid, b));
        invalid.channels = 2;
        invalid.frames   = 1392;
        EXPECT_FALSE(candidate.analyze(invalid, b));
        invalid.frames   = owe::audio::kWindowFrames;
        invalid.sequence = 0;
        EXPECT_FALSE(candidate.analyze(invalid, b));
        CompareFrame(saved, b);
        candidate.end();
        EXPECT_EQ(candidate.workspace_bytes(), size_t(65536));
        auto silent = owe::audio_test::window(0);
        ASSERT_TRUE(candidate.analyze(silent, b));
        for (float value : b.left) EXPECT_EQ(value, 0.0f);
        for (float value : b.right) EXPECT_EQ(value, 0.0f);
        EXPECT_EQ(candidate.workspace_bytes(), size_t(65536));
    }
}

template<size_t N>
void CheckTransform() {
    for (auto backend :
         array<liffto::Backend, 2> { liffto::Backend::Scalar, liffto::Backend::Simd }) {
        auto plan =
            liffto::FftPlan<f32>::make(usize(N), liffto::Normalization::None, backend).unwrap();
        auto scratch = liffto::Workspace<f32>::make(plan.workspace_len()).unwrap();
        array<liffto::Complex<f32>, N> input {}, output {}, inplace {};
        for (int pattern = 0; pattern < 4; ++pattern) {
            for (size_t j = 0; j < N; ++j) {
                f32 re(127), im = f32(1) / f32(127);
                if (pattern == 1 && j == N - 1) re += f32(127);
                if (pattern == 2 && j < 1392)
                    re += f32(25.4) * (f32::consts::TAU * f32(110) * f32(j) / f32(48000)).sin();
                if (pattern == 3) {
                    re = (f32(j) * f32(0.17)).sin();
                    im = (f32(j) * f32(0.13)).cos();
                } else
                    im = f32(1) / re;
                input[usize(j)] = { re, im };
            }
            plan.forward(input.as_slice(), output.as_mut_slice(), scratch).unwrap();
            for (size_t k = 0; k < N; ++k) {
                if (N <= 8 || k == 0 || k == 1 || k == 17 || k == 639 || k == N - 1) {
                    f64 re {}, im {};
                    for (size_t j = 0; j < N; ++j) {
                        auto angle = -f64::consts::TAU * f64(k) * f64(j) / f64(N);
                        auto c = angle.cos(), s = angle.sin();
                        auto value = input[usize(j)];
                        re += f64(value.re.to_primitive()) * c - f64(value.im.to_primitive()) * s;
                        im += f64(value.re.to_primitive()) * s + f64(value.im.to_primitive()) * c;
                    }
                    // Strong DC cancellation requires an absolute bound for near-zero bins.
                    double tolerance = N <= 8 ? (pattern == 3 ? 2e-5 : 3e-4) : 0.05;
                    EXPECT_NEAR(output[usize(k)].re.to_primitive(), re.to_primitive(), tolerance);
                    EXPECT_NEAR(output[usize(k)].im.to_primitive(), im.to_primitive(), tolerance);
                }
                if (pattern == 0 && k > 0)
                    EXPECT_LT((output[usize(k)].re * output[usize(k)].re +
                               output[usize(k)].im * output[usize(k)].im)
                                  .sqrt(),
                              f32(0.5));
            }
            if (pattern == 0) {
                EXPECT_NEAR(output[usize()].re.to_primitive(), 127.0 * N, 0.5);
                EXPECT_NEAR(output[usize()].im.to_primitive(), double(N) / 127, 0.005);
            }
            inplace = input;
            plan.forward_in_place(inplace.as_mut_slice(), scratch).unwrap();
            for (usize j; j < usize(N); ++j) EXPECT_EQ(inplace[j], output[j]);
        }
    }
}
TEST(FftComparison, DirectReferenceAndLayouts) {
    CheckTransform<7>();
    CheckTransform<8>();
    CheckTransform<2089>();
}

TEST(FftComparison, WorkspaceAllocationAndConcurrentPlans) {
    using Budget   = rstd::alloc::BudgetAllocator<>;
    auto budget    = Budget::try_make(usize(1024 * 1024)).unwrap();
    auto allocator = ::alloc::allocator_ref(budget);
    auto plan      = liffto::FftPlan<f32, decltype(allocator)>::make_in(
                         usize(2089), allocator, liffto::Normalization::None, liffto::Backend::Simd)
                         .unwrap();
    auto before    = budget.statistics();
    {
        auto workspace =
            liffto::Workspace<f32, decltype(allocator)>::make_in(plan.workspace_len(), allocator)
                .unwrap();
        EXPECT_EQ(budget.statistics().allocations, before.allocations + usize(1));
        EXPECT_EQ(budget.statistics().live_bytes, before.live_bytes + usize(65536));
        array<liffto::Complex<f32>, 2089> data {};
        data[usize()] = { f32(1), f32() };
        auto calls    = budget.statistics().allocations;
        for (int i = 0; i < 8; ++i) plan.forward_in_place(data.as_mut_slice(), workspace).unwrap();
        EXPECT_EQ(budget.statistics().allocations, calls);
    }
    EXPECT_EQ(budget.statistics().live_bytes, before.live_bytes);
    auto workers = Vec<rstd::thread::JoinHandle<float>>::make();
    for (int i = 0; i < 4; ++i)
        workers.push(
            rstd::thread::builder::Builder::make()
                .spawn([&plan] {
                    auto scratch = liffto::Workspace<f32>::make(plan.workspace_len()).unwrap();
                    array<liffto::Complex<f32>, 2089> input {}, output {};
                    input[usize()] = { f32(1), f32() };
                    for (int j = 0; j < 16; ++j)
                        plan.forward(input.as_slice(), output.as_mut_slice(), scratch).unwrap();
                    return output[usize(639)].re.to_primitive();
                })
                .unwrap());
    for (auto& worker : workers) EXPECT_NEAR(rstd::move(worker).join().unwrap(), 1.0f, 1e-5f);
}

TEST(AudioComparison, IndependentConcurrentEngines) {
    for (auto backend :
         array<AnalysisBackend, 2> { AnalysisBackend::LifftoScalar, AnalysisBackend::LifftoSimd }) {
        owe::audio::ResponseEngine reference(backend);
        owe::audio::ResponseFrame  expected {};
        auto                       input = owe::audio_test::window(2, 32);
        ASSERT_TRUE(reference.analyze(input, expected));
        auto workers = Vec<rstd::thread::JoinHandle<owe::audio::ResponseFrame>>::make();
        for (int i = 0; i < 4; ++i)
            workers.push(rstd::thread::builder::Builder::make()
                             .spawn([backend] {
                                 owe::audio::ResponseEngine engine(backend);
                                 owe::audio::ResponseFrame  result {};
                                 for (int j = 1; j <= 32; ++j) {
                                     auto pcm = owe::audio_test::window(2, j);
                                     if (! engine.analyze(pcm, result))
                                         rstd::panic { "concurrent audio input rejected" };
                                 }
                                 return result;
                             })
                             .unwrap());
        for (auto& worker : workers) CompareFrame(expected, rstd::move(worker).join().unwrap());
    }
}
