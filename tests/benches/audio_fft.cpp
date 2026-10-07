import owe.audio_response;
import owe.scene_audio_response;
import owe.audio_test_input;
import liffto;
import rstd;
import rstd.bench;
using namespace rstd::prelude;
using namespace rstd::literals;
using namespace rstd::bench;
using owe::audio::AnalysisBackend;
using rstd::size_t;

enum class Operation
{
    Plan,
    Workspace,
    Fft,
    Audio,
    Scene,
    Silence,
    Rejected
};
auto backend_name(AnalysisBackend b) -> rstd::ref<rstd::str> {
    return b == AnalysisBackend::LifftoScalar ? "scalar"_str : "simd"_str;
}
auto operation_name(Operation o) -> rstd::ref<rstd::str> {
    switch (o) {
    case Operation::Plan: return "plan-create-drop"_str;
    case Operation::Workspace: return "first-workspace-forward"_str;
    case Operation::Fft: return "fft2089"_str;
    case Operation::Audio: return "stereo128"_str;
    case Operation::Scene: return "stereo-scene32frames"_str;
    case Operation::Silence: return "silence"_str;
    case Operation::Rejected: return "rejected"_str;
    }
    return "invalid"_str;
}
struct AudioBatch {
    owe::audio::ResponseEngine          engine;
    owe::scene_audio::ResponseProcessor processor;
    owe::audio::ResponseFrame           response {};
    owe::scene_audio::Buffers           bands {};
    owe::audio::PcmWindow               pcm;
    explicit AudioBatch(AnalysisBackend b, bool silent = false)
        : engine(b), pcm(owe::audio_test::window(silent ? 0 : 2)) {
        engine.analyze(pcm, response);
        engine.end();
    }
    void run(bool scene) {
        int count = scene ? 32 : 1;
        for (int i = 0; i < count; ++i) {
            ++pcm.sequence;
            pcm.captured_at_ns += 10000000;
            pcm.end_sample_frame += 480;
            if (! engine.analyze(pcm, response)) rstd::panic { "benchmark input rejected" };
            if (scene) {
                processor.submit(response);
                processor.advance(1.0f / 30, bands);
            }
        }
        rstd::hint::black_box(response);
        rstd::hint::black_box(bands);
    }
};
struct AudioSession {
    AnalysisBackend                   backend;
    Operation                         operation;
    liffto::FftPlan<f32>              plan;
    liffto::Workspace<f32>            scratch;
    array<liffto::Complex<f32>, 2089> input {}, output {};
    AudioBatch                        audio;
    AudioSession(AnalysisBackend b, Operation o)
        : backend(b),
          operation(o),
          plan(liffto::FftPlan<f32>::make(usize(2089), liffto::Normalization::None,
                                          b == AnalysisBackend::LifftoScalar
                                              ? liffto::Backend::Scalar
                                              : liffto::Backend::Simd)
                   .unwrap()),
          scratch(liffto::Workspace<f32>::make(plan.workspace_len()).unwrap()),
          audio(b, o == Operation::Silence) {
        // Fixed complex stimulus; audio encoding is measured by stereo128.
        for (size_t j = 0; j < 2089; ++j) {
            auto re =
                f32(127) + f32(25.4) * (f32::consts::TAU * f32(110) * f32(j) / f32(48000)).sin();
            input[usize(j)] = { re, f32(1) / re };
        }
        plan.forward(input.as_slice(), output.as_mut_slice(), scratch).unwrap();
    }
    auto check() -> Result<empty, String> {
        f64 expected {};
        for (auto value : input) expected += f64(value.re.to_primitive());
        if ((f64(output[usize()].re.to_primitive()) - expected).abs() > f64(0.05))
            return Err("FFT DC check failed"_Str);
        return Ok(empty {});
    }
    auto finish() -> Result<empty, String> { return check(); }
    auto run(Bench& bench, rstd::ref<rstd::str> name) -> Result<BenchmarkResult, BenchError> {
        if (operation == Operation::Plan)
            return bench.run(name, [&] {
                auto p = liffto::FftPlan<f32>::make(usize(2089),
                                                    liffto::Normalization::None,
                                                    backend == AnalysisBackend::LifftoScalar
                                                        ? liffto::Backend::Scalar
                                                        : liffto::Backend::Simd)
                             .unwrap();
                rstd::hint::black_box(p);
            });
        if (operation == Operation::Workspace)
            return bench.run(name, [&] {
                auto w = liffto::Workspace<f32>::make(plan.workspace_len()).unwrap();
                plan.forward(input.as_slice(), output.as_mut_slice(), w).unwrap();
                rstd::hint::black_box(output);
            });
        if (operation == Operation::Fft)
            return bench.run(name, [&] {
                plan.forward(input.as_slice(), output.as_mut_slice(), scratch).unwrap();
                rstd::hint::black_box(output);
            });
        if (operation == Operation::Rejected) {
            audio.run(false);
            return bench.run(name, [&] {
                return audio.engine.analyze(audio.pcm, audio.response);
            });
        }
        RunConfig work;
        work.batch = f64(operation == Operation::Scene ? 32 : 1);
        work.unit  = "frame"_Str;
        return bench.run_batched_ref(
            name,
            [&] {
                return AudioBatch(backend, operation == Operation::Silence);
            },
            [&](AudioBatch& batch) {
                batch.run(operation == Operation::Scene);
            },
            BatchConfig { usize(1) },
            rstd::move(work));
    }
};
struct AudioFactory {
    AnalysisBackend backend;
    Operation       operation;
    auto prepare() -> Result<AudioSession, String> { return Ok(AudioSession(backend, operation)); }
};
struct AudioReporter {
    auto report(const RunResult& result) -> Result<empty, String> {
        if (! result.outcome.is_Passed()) {
            TextReporter text;
            return text.report(result);
        }
        const auto& m = result.outcome.as_Passed().measurement;
        auto        s = m.summary();
        rstd::io::println("summary,{},{},{},{},{}",
                          result.descriptor.id,
                          result.repetition,
                          s.median_ns_per_unit,
                          s.minimum_ns_per_unit,
                          s.maximum_ns_per_unit);
        for (const auto& e : m.measurements())
            rstd::io::println("epoch,{},{},{},{}",
                              result.descriptor.id,
                              result.repetition,
                              e.iterations,
                              e.elapsed.as_nanos());
        return Ok(empty {});
    }
};
int main(int argc, const char* const* argv) {
    rstd::env::args_init(argc, argv);
    RunnerOptions options;
    options.repeats                    = u64(3);
    options.sampling.epochs            = usize(9);
    options.sampling.warmup_iterations = u64(3);
    auto args                          = rstd::env::args();
    args.next();
    while (auto arg = args.next()) {
        if (*arg == "--quick"_str) {
            options.quick   = true;
            options.repeats = u64(1);
        } else
            return 2;
    }
    auto plan = liffto::FftPlan<f32>::make(usize(2089)).unwrap();
    rstd::io::println(
        "memory,liffto,table={},scratch={},io={}", plan.table_bytes(), usize(65536), usize(33424));
    Suite                     suite;
    constexpr AnalysisBackend backends[]   = { AnalysisBackend::LifftoScalar,
                                               AnalysisBackend::LifftoSimd };
    constexpr Operation       operations[] = { Operation::Plan,    Operation::Workspace,
                                               Operation::Fft,     Operation::Audio,
                                               Operation::Scene,   Operation::Silence,
                                               Operation::Rejected };
    for (auto b : backends)
        for (auto o : operations) {
            CaseDescriptor d { .id    = rstd::format("{}-{}", backend_name(b), operation_name(o)),
                               .suite = "owe-audio"_Str,
                               .revision       = "2"_Str,
                               .implementation = String::make(backend_name(b)) };
            suite.add(rstd::move(d), AudioFactory { b, o }).unwrap();
        }
    AudioReporter reporter;
    auto          result = Runner(rstd::move(options)).run(suite, reporter);
    return result.is_ok() && result->passed() ? 0 : 1;
}
