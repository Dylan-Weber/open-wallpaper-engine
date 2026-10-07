#include <rstd/test/gtest.hpp>
#if __has_feature(address_sanitizer)
#    include <sanitizer/allocator_interface.h>
#endif
import owe.audio_response;
import owe.audio_test_input;
import rstd;
using namespace rstd::prelude;

#if __has_feature(address_sanitizer)
static thread_local bool         count_allocations;
static thread_local rstd::size_t allocation_count;
static void                      AllocationHook(const volatile void*, rstd::size_t) {
    if (count_allocations) ++allocation_count;
}
static void FreeHook(const volatile void*) {}

TEST(AudioAllocation, ColdSilenceAndSteadyState) {
    ASSERT_GT(__sanitizer_install_malloc_and_free_hooks(AllocationHook, FreeHook), 0);
    for (auto backend : array<owe::audio::AnalysisBackend, 2> {
             owe::audio::AnalysisBackend::LifftoScalar, owe::audio::AnalysisBackend::LifftoSimd }) {
        allocation_count  = 0;
        count_allocations = true;
        owe::audio::ResponseEngine engine(backend);
        auto                       silent = owe::audio_test::window(0);
        owe::audio::ResponseFrame  output {};
        auto                       accepted = engine.analyze(silent, output);
        count_allocations                   = false;
        ASSERT_TRUE(accepted);
        EXPECT_EQ(allocation_count, rstd::size_t(0));
        auto input        = owe::audio_test::window(2, 2);
        allocation_count  = 0;
        count_allocations = true;
        accepted          = engine.analyze(input, output);
        count_allocations = false;
        ASSERT_TRUE(accepted);
        EXPECT_GT(allocation_count, rstd::size_t(0));
        auto cold         = allocation_count;
        allocation_count  = 0;
        count_allocations = true;
        for (int j = 3; j < 20; ++j) {
            input.sequence = j;
            accepted       = engine.analyze(input, output) && accepted;
        }
        engine.end();
        input.sequence    = 1;
        accepted          = engine.analyze(input, output) && accepted;
        count_allocations = false;
        ASSERT_TRUE(accepted);
        EXPECT_EQ(allocation_count, rstd::size_t(0));
        rstd::io::println("audio_allocations backend={} cold={} steady={}",
                          int(backend),
                          usize(cold),
                          usize(allocation_count));
    }
}
#endif
