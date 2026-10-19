export module wescene.texture_memory;
import rstd;

using namespace rstd::prelude;

export namespace owe
{
struct TextureMemoryOptions {
    rstd::alloc::SharedAllocator bytes;

    TextureMemoryOptions()
        : bytes(rstd::alloc::SharedAllocator::make(
              rstd::alloc::SharedTrackingAllocator<rstd::alloc::VirtualMemoryAllocator>::make())) {}
    explicit TextureMemoryOptions(rstd::alloc::SharedAllocator allocator)
        : bytes(rstd::move(allocator)) {}
    auto clone() const -> TextureMemoryOptions { return TextureMemoryOptions(bytes.clone()); }
};

struct TextureMemoryStatistics {
    rstd::alloc::AllocationStats                 logical;
    Option<rstd::alloc::VirtualMemoryStatistics> virtual_memory;
};

class TextureMemorySession {
    TextureMemoryOptions                          options_;
    Box<dyn<rstd::Fn<TextureMemoryStatistics()>>> snapshot_;

    template<typename A>
    explicit TextureMemorySession(rstd::alloc::SharedTrackingAllocator<A> tracker)
        : options_(rstd::alloc::SharedAllocator::make(tracker.clone())),
          snapshot_(
              Box<dyn<rstd::Fn<TextureMemoryStatistics()>>>::make([tracker = rstd::move(tracker)] {
                  TextureMemoryStatistics result { .logical = tracker.snapshot() };
                  if constexpr (requires { tracker.upstream_statistics(); })
                      result.virtual_memory = Some(tracker.upstream_statistics());
                  return result;
              })) {}

public:
    static auto System() -> TextureMemorySession {
        return TextureMemorySession(rstd::alloc::SharedTrackingAllocator<>::make());
    }
    static auto VirtualMemory() -> TextureMemorySession {
        return TextureMemorySession(
            rstd::alloc::SharedTrackingAllocator<rstd::alloc::VirtualMemoryAllocator>::make());
    }
    auto options() const -> TextureMemoryOptions { return options_.clone(); }
    auto statistics() const -> TextureMemoryStatistics { return snapshot_->operator()(); }
};
} // namespace owe
