#include <rstd/test/gtest.hpp>

import rstd;
import weweb;

using namespace rstd::prelude;
using namespace rstd::literals;
using weweb::CefProfileLease;

TEST(CefProfile, DistinctLeasesReuseReleasedDirectories) {
    auto directory  = rstd::fs::TempDir::make("owe-cef-profile"_str).unwrap();
    auto root       = rstd::path::PathBuf::from(directory.path());
    auto first      = Some(CefProfileLease::acquire(root).unwrap());
    auto second     = CefProfileLease::acquire(root).unwrap();
    auto first_path = root.join("profiles/0"_str);
    EXPECT_TRUE(first->path() == first_path.as_path());
    EXPECT_TRUE(second.path() == root.join("profiles/1"_str).as_path());
    ASSERT_TRUE(rstd::fs::write(first_path.join("retained"_str), "cache"_bytes).is_ok());
    auto lease_inode = rstd::fs::metadata(first_path.join(".lease"_str)).unwrap().ino();

    first       = None();
    auto reused = CefProfileLease::acquire(root).unwrap();
    EXPECT_TRUE(reused.path() == first_path.as_path());
    EXPECT_EQ(rstd::fs::read_to_string(first_path.join("retained"_str)).unwrap(), "cache"_str);
    EXPECT_EQ(rstd::fs::metadata(first_path.join(".lease"_str)).unwrap().ino(), lease_inode);
    EXPECT_FALSE(rstd::fs::exists(root.join("profiles/2"_str)).unwrap());
}

TEST(CefProfile, MovingLeaseKeepsDirectoryLocked) {
    auto directory = rstd::fs::TempDir::make("owe-cef-profile-move"_str).unwrap();
    auto root      = rstd::path::PathBuf::from(directory.path());
    Option<CefProfileLease> moved;
    {
        auto original = CefProfileLease::acquire(root).unwrap();
        moved         = Some(rstd::move(original));
    }
    {
        auto second = CefProfileLease::acquire(root).unwrap();
        EXPECT_TRUE(second.path() == root.join("profiles/1"_str).as_path());
    }
    moved       = None();
    auto reused = CefProfileLease::acquire(root).unwrap();
    EXPECT_TRUE(reused.path() == root.join("profiles/0"_str).as_path());
}

TEST(CefProfile, RejectsEmptyRoot) {
    auto result = CefProfileLease::acquire(rstd::path::PathBuf {});
    ASSERT_TRUE(result.is_err());
    EXPECT_EQ(result.unwrap_err().kind().code, rstd::io::ErrorKind::InvalidInput);
}

TEST(CefProfile, ReportsFileErrorsWithoutTryingAnotherSlot) {
    auto directory = rstd::fs::TempDir::make("owe-cef-profile-error"_str).unwrap();
    auto root      = rstd::path::PathBuf::from(directory.path());
    ASSERT_TRUE(rstd::fs::create_dir_all(root.join("profiles/0/.lease"_str)).is_ok());
    EXPECT_TRUE(CefProfileLease::acquire(root).is_err());
    EXPECT_FALSE(rstd::fs::exists(root.join("profiles/1"_str)).unwrap());
}
