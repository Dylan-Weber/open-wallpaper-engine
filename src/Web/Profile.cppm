module;

#include <rstd/macro.hpp>

export module weweb:profile;
import rstd;

using namespace rstd::prelude;
using namespace rstd::literals;

export namespace weweb
{

class CefProfileLease {
    rstd::path::PathBuf m_path;
    rstd::fs::FileLock  m_lock;

    CefProfileLease(rstd::path::PathBuf path, rstd::fs::FileLock lock)
        : m_path(rstd::move(path)), m_lock(rstd::move(lock)) {}

public:
    CefProfileLease(const CefProfileLease&)            = delete;
    CefProfileLease& operator=(const CefProfileLease&) = delete;
    CefProfileLease(CefProfileLease&&)                 = default;
    CefProfileLease& operator=(CefProfileLease&&)      = default;

    static auto acquire(ref<rstd::path::Path> root) -> rstd::io::Result<CefProfileLease> {
        if (root.is_empty()) {
            return Err(rstd::io::Error::from_kind({ rstd::io::ErrorKind::InvalidInput }));
        }
        const auto profiles = rstd::path::PathBuf::from(root).join("profiles"_str);
        for (u64 slot {};; ++slot) {
            auto path = profiles.join(rstd::format("{}", slot).as_str());
            rstd_try(rstd::fs::create_dir_all(path.as_path()));
            // Keep the lease inode stable across profile reuse.
            auto file = rstd_try(rstd::fs::OpenOptions::make()
                                     .read(true)
                                     .write(true)
                                     .create(true)
                                     .mode(u32(0600))
                                     .open(path.join(".lease"_str).as_path()));
            auto lock = rstd_try(rstd::fs::FileLock::try_acquire(
                rstd::move(file), rstd::fs::FileLockMode::Exclusive));
            if (lock.is_some()) return Ok(CefProfileLease(rstd::move(path), rstd::move(*lock)));
        }
    }

    auto path() const noexcept -> ref<rstd::path::Path> { return m_path.as_path(); }
};

} // namespace weweb
