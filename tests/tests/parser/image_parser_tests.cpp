#include <rstd/test/gtest.hpp>

#include <filesystem>
#include <fstream>
#include <lz4.h>

import rstd;
import vrento;
import rstd.cppstd;
import wescene.fs;
import wescene.pkg.parse;
import wescene.pkg_asset_version;
import wescene.scene;
import wescene.types;

using namespace rstd::prelude;
using namespace rstd::literals;
using rstd::sync::Arc;

TEST(BitFlags, PreservesWidthAndHighToLowText) {
    enum class Flag : rstd::uint8_t
    {
        First = 0,
        Last  = 7
    };
    owe::BitFlags<Flag> flags(0x181);
    EXPECT_EQ(flags.size(), 8u);
    EXPECT_EQ(flags.count(), 2u);
    EXPECT_EQ(flags.to_string(), "10000001"_str);
    EXPECT_TRUE(flags[Flag::First]);
    EXPECT_TRUE(flags[Flag::Last]);
    EXPECT_FALSE(flags.all());
    flags.reset(Flag::Last).set(Flag::First, false);
    EXPECT_TRUE(flags.none());
    EXPECT_FALSE(flags.any());
    flags = owe::BitFlags<Flag>(0x1ff);
    EXPECT_TRUE(flags.all());
    EXPECT_EQ(flags.count(), 8u);
    flags.reset();
    EXPECT_EQ(flags.to_string(), "00000000"_str);
}

TEST(BitFlags, SupportsSignedEnumsAndFull64BitWidth) {
    enum class Signed : rstd::int32_t
    {
        Last = 31
    };
    owe::BitFlags<Signed> signed_flags;
    signed_flags.set(Signed::Last);
    EXPECT_TRUE(signed_flags[31]);
    EXPECT_EQ(signed_flags.count(), 1u);
    EXPECT_EQ(signed_flags.to_string(), "10000000000000000000000000000000"_str);
    enum class Wide : rstd::uint64_t
    {
        First = 0,
        Last  = 63
    };
    owe::BitFlags<Wide> wide(u64::MAX.to_primitive());
    EXPECT_TRUE(wide.all());
    EXPECT_EQ(wide.count(), 64u);
    wide.reset().set(Wide::Last).set(Wide::First);
    EXPECT_TRUE(wide[63]);
    EXPECT_TRUE(wide[0]);
    EXPECT_EQ(wide.count(), 2u);
    EXPECT_EQ(wide.to_string().len(), usize(64));
}

TEST(ImageHeader, CloneOwnsMetadataIndependently) {
    owe::ImageHeader source;
    source.width     = 37;
    source.mapHeight = 19;
    source.isSprite  = true;
    source.extraHeader.insert("texv"_Str, owe::ImageExtra { .val = 3 });
    auto copy = source.clone();
    copy.extraHeader.insert("texv"_Str, owe::ImageExtra { .val = 4 });
    copy.extraHeader.insert("compo1"_Str, owe::ImageExtra { .val = 1 });
    EXPECT_EQ(source.extraHeader.get("texv"_str).unwrap()->val, 3);
    EXPECT_FALSE(source.extraHeader.contains_key("compo1"_str));
    EXPECT_EQ(copy.width, 37);
    EXPECT_EQ(copy.mapHeight, 19);
    EXPECT_TRUE(copy.isSprite);
    EXPECT_EQ(owe::ToString(owe::ImageType::PNG), "PNG"_str);
    EXPECT_EQ(owe::ToString(owe::TextureFormat::BC3), "BC3"_str);
    EXPECT_TRUE(owe::ToString(static_cast<owe::ImageType>(999)).is_empty());
    EXPECT_TRUE(owe::ToString(static_cast<owe::TextureFormat>(999)).is_empty());
}

namespace
{

class TrackingImageParser {
public:
    auto Describe(ref<str>) const -> Result<owe::ImageDescription, owe::ImageParseError> {
        return Ok(owe::ImageDescription {});
    }
    auto Parse(ref<str> name) const -> Result<Arc<owe::Image>, owe::ImageParseError> {
        auto current  = m_active.fetch_add(1) + 1;
        auto observed = m_peak.load();
        while (current > observed && ! m_peak.compare_exchange_weak(observed, current)) {
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        auto image          = Arc<owe::Image>::make();
        image->content->key = rstd::into(name);
        m_active.fetch_sub(1);
        return Ok(rstd::move(image));
    }

    auto ParseHeader(ref<str>) const -> Result<owe::ImageHeader, owe::ImageParseError> {
        return Ok(owe::ImageHeader {});
    }

    auto ParseMany(slice<String> names) const
        -> Vec<Result<Arc<owe::Image>, owe::ImageParseError>> {
        auto parser = dyn<owe::IImageParser>::from_ref(*this);
        return owe::ParseImages(parser.as_ref(), names);
    }

    int peak() const { return m_peak.load(); }

private:
    mutable std::atomic<int> m_active { 0 };
    mutable std::atomic<int> m_peak { 0 };
};

class MixedImageParser {
public:
    auto Describe(ref<str>) const -> Result<owe::ImageDescription, owe::ImageParseError> {
        return Ok(owe::ImageDescription {});
    }
    auto Parse(ref<str> name) const -> Result<Arc<owe::Image>, owe::ImageParseError> {
        if (rstd::cppstd::as_string_view(name) == "bad") {
            return Err(owe::ImageParseError {
                .kind    = owe::ImageParseErrorKind::DecodeFailed,
                .message = String::make("bad image"_str),
            });
        }
        auto image          = Arc<owe::Image>::make();
        image->content->key = rstd::into(name);
        return Ok(rstd::move(image));
    }

    auto ParseHeader(ref<str>) const -> Result<owe::ImageHeader, owe::ImageParseError> {
        return Ok(owe::ImageHeader {});
    }

    auto ParseMany(slice<String> names) const
        -> Vec<Result<Arc<owe::Image>, owe::ImageParseError>> {
        auto parser = dyn<owe::IImageParser>::from_ref(*this);
        return owe::ParseImages(parser.as_ref(), names);
    }
};

void WriteU32(std::ofstream& output, std::uint32_t value) {
    output.write(reinterpret_cast<const char*>(&value), sizeof(value));
}

class ConditionalTextureTest {
protected:
    std::filesystem::path root =
        std::filesystem::temp_directory_path() /
        ("owe-conditional-texture-" + std::to_string(rstd::process::id().to_primitive()));
    owe::fs::VFS vfs;

    void SetUp() noexcept {
        std::filesystem::create_directories(root / "materials");
        auto physical =
            owe::fs::make_physical_fs(owe::fs::Path(rstd::cppstd::as_str(root.string()).unwrap()));
        ASSERT_TRUE(physical.is_ok());
        ASSERT_TRUE(vfs.mount("/assets"_str, rstd::move(physical).unwrap_unchecked()).is_ok());
    }
    void TearDown() noexcept { std::filesystem::remove_all(root); }

    void WriteTexture(bool sprite, std::uint32_t conditions) {
        std::ofstream output(root / "materials" / "conditional.tex", std::ios::binary);
        output.write("TEXV0005", 9);
        output.write("TEXI0001", 9);
        for (auto value : { 0u, sprite ? 4u : 0u, 2u, 2u, 2u, 2u, 0u }) WriteU32(output, value);
        output.write("TEXB0004", 9);
        WriteU32(output, 1);
        WriteU32(output, 0xffffffffu);
        WriteU32(output, conditions);
        for (std::uint32_t index = 0; index < conditions; ++index) {
            for (auto value : { index + 1, 1u, 0u }) WriteU32(output, value);
            const char json[] = R"({"condition":"newproperty"})";
            output.write(json, sizeof(json));
        }
        WriteU32(output, 2);
        for (auto extent : { 2u, 1u }) {
            for (auto value : { extent, extent, 0u, 0u, extent * extent * 4 })
                WriteU32(output, value);
            for (std::uint32_t pixel = 0; pixel < extent * extent; ++pixel)
                WriteU32(output, 0xff332211u);
            if (conditions == 0) continue;
            WriteU32(output, 2);
            WriteU32(output, 0);
            WriteU32(output, conditions);
            for (std::uint32_t index = 0; index < conditions; ++index) {
                for (auto value : { 0u, index + 1, 0u, 0u, 1u, 1u, 4u, 4u, 0xffffffffu })
                    WriteU32(output, value);
            }
        }
        if (! sprite) return;
        output.write("TEXS0003", 9);
        for (auto value : { 1u, 2u, 2u, 0u }) WriteU32(output, value);
        const float frame[] { 0.1f, 0.0f, 0.0f, 2.0f, 0.0f, 0.0f, 2.0f };
        output.write(reinterpret_cast<const char*>(frame), sizeof(frame));
    }

    void WritePayload(owe::ImageType type, bool compressed, slice<u8> payload, int width = 1,
                      int height = 1, unsigned format = 4) {
        Vec<u8> stored;
        if (compressed) {
            stored.resize(usize(LZ4_compressBound(int(payload.len().to_primitive()))), u8());
            auto size = LZ4_compress_default(reinterpret_cast<const char*>(payload.as_raw_ptr()),
                                             reinterpret_cast<char*>(stored.data()),
                                             int(payload.len().to_primitive()),
                                             int(stored.len().to_primitive()));
            ASSERT_GT(size, 0);
            stored.truncate(usize(size));
        } else {
            stored = Vec<u8>::from(payload);
        }
        std::ofstream output(root / "materials" / "payload.tex", std::ios::binary);
        output.write("TEXV0005", 9);
        output.write("TEXI0001", 9);
        for (auto value : array<unsigned, 7> { format,
                                               0u,
                                               unsigned(width),
                                               unsigned(height),
                                               unsigned(width),
                                               unsigned(height),
                                               0u })
            WriteU32(output, value);
        output.write("TEXB0003", 9);
        WriteU32(output, 1);
        WriteU32(output, unsigned(type));
        WriteU32(output, 1);
        WriteU32(output, width);
        WriteU32(output, height);
        WriteU32(output, compressed ? 1 : 0);
        WriteU32(output, payload.len().to_primitive());
        WriteU32(output, stored.len().to_primitive());
        output.write(reinterpret_cast<const char*>(stored.data()), stored.len().to_primitive());
    }
};

constexpr unsigned char RedPng[] {
    0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 0x00, 0x0d, 0x49, 0x48,
    0x44, 0x52, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x08, 0x06, 0x00, 0x00,
    0x00, 0x1f, 0x15, 0xc4, 0x89, 0x00, 0x00, 0x00, 0x0d, 0x49, 0x44, 0x41, 0x54, 0x08,
    0xd7, 0x63, 0xf8, 0xcf, 0xc0, 0xf0, 0x1f, 0x00, 0x05, 0x00, 0x01, 0xff, 0x89, 0x99,
    0x3d, 0x1d, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4e, 0x44, 0xae, 0x42, 0x60, 0x82,
};

TEST_F(ConditionalTextureTest, EmbeddedDescriptionMatchesDecodeWithoutPublishingPixels) {
    for (auto compressed : array<bool, 2> { false, true }) {
        for (auto type :
             array<owe::ImageType, 2> { owe::ImageType::UNKNOWN, owe::ImageType::PNG }) {
            WritePayload(type,
                         compressed,
                         slice<u8>::from_raw_parts(reinterpret_cast<const byte*>(RedPng),
                                                   usize(sizeof(RedPng))));
            auto [tx, rx] = rstd::sync::mpmc::channel<Arc<vrento::Image>>();
            owe::TexImageParser parser(
                &vfs, Some(owe::ImageReadyCallback::make([tx](Arc<vrento::Image> image) {
                    tx.send(rstd::move(image)).unwrap();
                })));
            auto description = parser.Describe("payload"_str);
            ASSERT_TRUE(description.is_ok());
            EXPECT_EQ(description->header.type, owe::ImageType::PNG);
            EXPECT_EQ(description->header.format, owe::TextureFormat::RGBA8);
            ASSERT_EQ(description->slots.len(), usize(1));
            EXPECT_EQ(description->slots[usize()].mip_count, usize(1));
            EXPECT_TRUE(parser.ParseHeader("payload"_str).is_ok());
            EXPECT_TRUE(rx.try_recv().is_err());
            auto image = parser.Parse("payload"_str);
            ASSERT_TRUE(image.is_ok());
            auto& mip = (*image)->content->slots[usize()].mipmaps[usize()];
            EXPECT_EQ(mip.width, description->slots[usize()].width);
            EXPECT_EQ(mip.height, description->slots[usize()].height);
            EXPECT_EQ(mip.size, isize(4));
            EXPECT_EQ(mip.data.get()[0], 255);
            EXPECT_EQ(mip.data.get()[1], 0);
            EXPECT_EQ(mip.data.get()[2], 0);
            EXPECT_EQ(mip.data.get()[3], 255);
            EXPECT_TRUE(rx.try_recv().is_ok());
        }
    }
}

TEST_F(ConditionalTextureTest, ContainerDescriptionDoesNotDecodeImageData) {
    auto bytes = Vec<u8>::from(
        slice<u8>::from_raw_parts(reinterpret_cast<const byte*>(RedPng), usize(sizeof(RedPng))));
    for (usize i(41); i < usize(54); ++i) bytes[i] = u8();
    WritePayload(owe::ImageType::UNKNOWN, false, bytes.as_slice());
    owe::TexImageParser parser(&vfs);
    EXPECT_TRUE(parser.Describe("payload"_str).is_ok());
    auto decoded = parser.Parse("payload"_str);
    ASSERT_TRUE(decoded.is_err());
    EXPECT_EQ(decoded.unwrap_err().kind, owe::ImageParseErrorKind::DecodeFailed);
}

TEST_F(ConditionalTextureTest, RawBcDescriptionAndVideoRangeMatchContent) {
    for (auto compressed : array<bool, 2> { false, true }) {
        Vec<u8> bytes;
        bytes.resize(usize(16), u8(17));
        WritePayload(owe::ImageType::UNKNOWN, compressed, bytes.as_slice(), 4, 4);
        owe::TexImageParser parser(&vfs);
        auto                description = parser.Describe("payload"_str).unwrap();
        auto                image       = parser.Parse("payload"_str).unwrap();
        EXPECT_EQ(description.header.format, owe::TextureFormat::BC3);
        EXPECT_EQ(image->header.format, description.header.format);
        EXPECT_EQ(image->content->slots[usize()].mipmaps[usize()].size, isize(16));
    }
    auto bytes = Vec<u8>::from("\0\0\0\x10"
                               "ftypisom00000000"_str.as_bytes());
    WritePayload(owe::ImageType::UNKNOWN, false, bytes.as_slice(), 16, 8);
    owe::TexImageParser parser(&vfs);
    auto                description = parser.Describe("payload"_str).unwrap();
    EXPECT_EQ(description.header.type, owe::ImageType::VIDEO);
    EXPECT_EQ(description.header.format, owe::TextureFormat::RGBA8);
    auto  image = parser.Parse("payload"_str).unwrap();
    auto& mip   = image->content->slots[usize()].mipmaps[usize()];
    EXPECT_FALSE(bool(mip.data));
    ASSERT_TRUE(mip.video_source.is_some());
    EXPECT_EQ(mip.video_source->len(), rstd::as_cast<u64>(bytes.len()));
    owe::fs::BinaryReader reader(mip.video_source->clone());
    EXPECT_EQ(reader.read_all_bytes().unwrap().as_slice(), bytes.as_slice());
}

TEST_F(ConditionalTextureTest, DescriptionPreservesEverySlotAndMip) {
    {
        std::ofstream output(root / "materials" / "slots.tex", std::ios::binary);
        output.write("TEXV0005", 9);
        output.write("TEXI0001", 9);
        for (auto value : array<unsigned, 7> { 4u, 0u, 8u, 8u, 8u, 8u, 0u })
            WriteU32(output, value);
        output.write("TEXB0002", 9);
        WriteU32(output, 2);
        for (unsigned slot {}; slot < 2; ++slot) {
            WriteU32(output, 2);
            for (unsigned mip {}; mip < 2; ++mip) {
                auto extent = 8u >> (slot + mip);
                auto size   = ((extent + 3) / 4) * ((extent + 3) / 4) * 16;
                for (auto value : array<unsigned, 5> { extent, extent, 0u, 0u, size })
                    WriteU32(output, value);
                for (unsigned index {}; index < size / 4; ++index) WriteU32(output, index);
            }
        }
    }
    owe::TexImageParser parser(&vfs);
    auto                described = parser.Describe("slots"_str).unwrap();
    auto                image     = parser.Parse("slots"_str).unwrap();
    ASSERT_EQ(described.slots.len(), usize(2));
    for (usize index {}; index < described.slots.len(); ++index) {
        const auto& slot = image->content->slots[index];
        EXPECT_EQ(described.slots[index].width, slot.width);
        EXPECT_EQ(described.slots[index].height, slot.height);
        EXPECT_EQ(described.slots[index].mip_count, slot.mipmaps.len());
        EXPECT_EQ(slot.mipmaps.len(), usize(2));
    }
}

TEST_F(ConditionalTextureTest, BaseMipmapsRemainAlignedWithMultipleConditions) {
    for (auto conditions : { 0u, 1u, 2u }) {
        WriteTexture(false, conditions);
        owe::TexImageParser parser(&vfs);
        auto                parsed = parser.Parse("conditional"_str);
        ASSERT_TRUE(parsed.is_ok());
        auto image = rstd::move(parsed).unwrap_unchecked();
        ASSERT_EQ(image->content->slots.len(), usize(1));
        ASSERT_EQ(image->content->slots[usize(0)].mipmaps.len(), usize(2));
        for (std::size_t index = 0; index < 2; ++index) {
            const auto& mip = image->content->slots[usize(0)].mipmaps[usize(index)];
            EXPECT_EQ(mip.width, index == 0 ? 2 : 1);
            EXPECT_EQ(mip.height, mip.width);
            EXPECT_EQ(mip.size, isize(mip.width * mip.height * 4));
            ASSERT_TRUE(static_cast<bool>(mip.data));
            EXPECT_EQ(mip.data.get()[0], 0x11);
        }
        auto header = parser.ParseHeader("conditional"_str);
        ASSERT_TRUE(header.is_ok());
        EXPECT_EQ(header->width, image->header.width);
        EXPECT_EQ(header->mipmap_larger, image->header.mipmap_larger);
    }
}

TEST_F(ConditionalTextureTest, AssetVersionWriterPreservesFixedRecord) {
    struct Case {
        ref<str> prefix;
        int      version;
        ref<str> expected;
    };
    for (const auto& item : array<Case, 8> {
             Case { "SPVS"_str, 5, "SPVS0005\0"_str },
             Case { "SPVS-extra"_str, -1, "SPVS-000\0"_str },
             Case { "X"_str, -1, "X-0001\0\0\0"_str },
             Case { ""_str, 0, "0000\0\0\0\0\0"_str },
             Case { "SPVS"_str, 12345, "SPVS1234\0"_str },
             Case { "X\0YZ"_str, 7, "X0007\0\0\0\0"_str },
             Case { "SPVS"_str, i32::MIN.to_primitive(), "SPVS-214\0"_str },
             Case { "SPVS"_str, i32::MAX.to_primitive(), "SPVS2147\0"_str },
         }) {
        auto output = vfs.open_write("/assets/version"_str,
                                     owe::fs::WriteOptions { .create = true, .truncate = true });
        ASSERT_TRUE(output.is_ok());
        {
            owe::fs::BinaryWriter writer(rstd::move(output).unwrap());
            owe::WriteAssetVersion(item.prefix, writer, item.version);
        }
        auto path =
            rstd::path::PathBuf::from(rstd::cppstd::as_str((root / "version").string()).unwrap());
        auto bytes = rstd::fs::read(path.as_path());
        ASSERT_TRUE(bytes.is_ok());
        EXPECT_EQ(bytes->as_slice(), item.expected.as_bytes());
    }
}

TEST_F(ConditionalTextureTest, MetadataDoesNotDecodeAndParsePublishesOwnedContentOnce) {
    WriteTexture(false, 0);
    auto [tx, rx] = rstd::sync::mpmc::channel<Arc<vrento::Image>>();
    auto ready    = owe::ImageReadyCallback::make([tx](Arc<vrento::Image> image) {
        tx.send(rstd::move(image)).unwrap();
    });
    Option<Arc<vrento::Image>> retained;
    {
        owe::TexImageParser parser(&vfs, Some(rstd::move(ready)));
        ASSERT_TRUE(parser.ParseHeader("conditional"_str).is_ok());
        auto description = parser.Describe("conditional"_str);
        ASSERT_TRUE(description.is_ok());
        ASSERT_EQ(description->slots.len(), usize(1));
        EXPECT_EQ(description->slots[usize()].mip_count, usize(2));
        EXPECT_TRUE(rx.try_recv().is_err());
        ASSERT_TRUE(parser.Parse("conditional"_str).is_ok());
        auto queued = rx.try_recv();
        ASSERT_TRUE(queued.is_ok());
        retained = Some(queued.unwrap());
        Vec<String> names;
        names.push("conditional"_Str);
        names.push("conditional"_Str);
        auto images = parser.ParseMany(names.as_slice());
        for (const auto& image : images) {
            ASSERT_TRUE(image.is_ok());
            EXPECT_TRUE(Arc<vrento::Image>::ptr_eq((*image)->content, *retained));
        }
        EXPECT_TRUE(rx.try_recv().is_err());
    }
    EXPECT_EQ((*retained)->slots[usize()].mipmaps[usize()].data.get()[0], 0x11);
}

TEST_F(ConditionalTextureTest, SpriteHeaderSkipsConditionalPatches) {
    WriteTexture(true, 1);
    owe::TexImageParser parser(&vfs);
    auto                header = parser.ParseHeader("conditional"_str);
    ASSERT_TRUE(header.is_ok());
    EXPECT_EQ(header->spriteAnim.numFrames(), usize(1));
    EXPECT_EQ(header->extraHeader.get("texs"_str).unwrap()->val, 3);
    EXPECT_TRUE(parser.Parse("conditional"_str).is_ok());
}

TEST_F(ConditionalTextureTest, FullLoadReusesPreparedHeaderAndOpenSource) {
    WriteTexture(true, 1);
    owe::TexImageParser parser(&vfs);
    auto                header = parser.ParseHeader("conditional"_str);
    ASSERT_TRUE(header.is_ok());
    std::filesystem::rename(root / "materials" / "conditional.tex", root / "original.tex");
    {
        std::ofstream replacement(root / "materials" / "conditional.tex");
    }
    auto parsed = parser.Parse("conditional"_str);
    ASSERT_TRUE(parsed.is_ok());
    auto image = rstd::move(parsed).unwrap_unchecked();
    EXPECT_EQ(image->header.spriteAnim.numFrames(), usize(1));
    EXPECT_EQ(image->content->slots[usize(0)].mipmaps.len(), usize(2));
}

TEST_F(ConditionalTextureTest, ParserRetainsMountedSourcesAfterVfsDestruction) {
    WriteTexture(false, 0);
    Option<owe::TexImageParser> parser;
    {
        owe::fs::VFS source;
        auto         mount =
            owe::fs::make_physical_fs(owe::fs::Path(rstd::cppstd::as_str(root.string()).unwrap()));
        ASSERT_TRUE(mount.is_ok());
        ASSERT_TRUE(source.mount("/assets"_str, rstd::move(mount).unwrap_unchecked()).is_ok());
        parser = Some(owe::TexImageParser(&source));
    }
    EXPECT_TRUE(parser->Parse("conditional"_str).is_ok());
}

TEST_F(ConditionalTextureTest, RejectsTruncatedConditionsAndPatches) {
    for (auto length : { 63u, 70u, 83u, 260u }) {
        WriteTexture(false, 1);
        std::filesystem::resize_file(root / "materials" / "conditional.tex", length);
        owe::TexImageParser parser(&vfs);
        auto                parsed = parser.Parse("conditional"_str);
        ASSERT_TRUE(parsed.is_err()) << length;
        EXPECT_EQ(parsed.unwrap_err_unchecked().kind, owe::ImageParseErrorKind::InvalidData);
        if (length < 100) EXPECT_TRUE(parser.ParseHeader("conditional"_str).is_err());
    }
}

TEST(ImageParser, BatchPreservesOrderAndBoundsConcurrency) {
    TrackingImageParser parser;
    Vec<String>         names;
    for (const char* name : { "0", "1", "2", "3", "4", "5", "6", "7" })
        names.push(String::make(rstd::cppstd::as_str(name).unwrap()));

    auto image_parser = dyn<owe::IImageParser>::from_ref(parser);
    auto images       = owe::ParseImages(image_parser.as_ref(), names.as_slice());

    ASSERT_EQ(images.len(), names.len());
    for (usize index {}; index < names.len(); ++index) {
        ASSERT_TRUE(images[index].is_ok());
        auto image = rstd::move(images[index]).unwrap_unchecked();
        EXPECT_EQ(image->content->key, names[index].as_str());
    }
    EXPECT_GT(parser.peak(), 1);
    EXPECT_LE(parser.peak(), 4);
}

TEST(ImageParser, TextureHeaderExposesFourthPackedComponent) {
    auto root = std::filesystem::temp_directory_path() /
                ("owe-image-parser-" + std::to_string(rstd::process::id().to_primitive()));
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root / "materials");
    {
        std::ofstream output(root / "materials" / "mask.tex", std::ios::binary);
        output.write("TEXV0005", 9);
        output.write("TEXI0001", 9);
        WriteU32(output, 4);
        WriteU32(output, 1u << 23);
        for (int i {}; i < 4; ++i) WriteU32(output, 1);
        WriteU32(output, 0);
        output.write("TEXB0001", 9);
        for (int i {}; i < 4; ++i) WriteU32(output, 1);
        WriteU32(output, 16);
        for (int i {}; i < 4; ++i) WriteU32(output, 0);
    }

    auto physical =
        owe::fs::make_physical_fs(owe::fs::Path(rstd::cppstd::as_str(root.string()).unwrap()));
    ASSERT_TRUE(physical.is_ok());
    owe::fs::VFS vfs;
    ASSERT_TRUE(vfs.mount("/assets"_str, rstd::move(physical).unwrap_unchecked()).is_ok());

    owe::TexImageParser parser(&vfs);
    auto                parsed = parser.ParseHeader("mask"_str);
    ASSERT_TRUE(parsed.is_ok());
    auto header = rstd::move(parsed).unwrap_unchecked();
    EXPECT_EQ(header.extraHeader.get("compo1"_str).unwrap()->val, 0);
    EXPECT_EQ(header.extraHeader.get("compo2"_str).unwrap()->val, 0);
    EXPECT_EQ(header.extraHeader.get("compo3"_str).unwrap()->val, 0);
    EXPECT_EQ(header.extraHeader.get("compo4"_str).unwrap()->val, 1);

    std::filesystem::remove_all(root);
}

TEST(ImageParser, SceneBatchPreservesRuntimeParserAndErrorPositions) {
    owe::Scene scene;
    scene.SetImageParser(rstd::sync::Arc<dyn<owe::IImageParser>>::make(MixedImageParser {}));
    auto runtime          = Arc<owe::Image>::make();
    runtime->content->key = "runtime-value"_Str;
    scene.RegisterRuntimeImage(String::make("runtime"_str), runtime.clone());

    Vec<String> names;
    for (const char* name : { "first", "runtime", "bad", "last" }) {
        names.push(String::make(rstd::cppstd::as_str(name).unwrap()));
    }
    auto images = scene.ParseImages(names.as_slice());

    ASSERT_EQ(images.len(), names.len());
    ASSERT_TRUE(images[usize(0)].is_ok());
    EXPECT_EQ((*images[usize(0)])->content->key, "first"_str);
    ASSERT_TRUE(images[usize(1)].is_ok());
    EXPECT_TRUE(Arc<owe::Image>::ptr_eq(*images[usize(1)], runtime));
    ASSERT_TRUE(images[usize(2)].is_err());
    EXPECT_EQ(images[usize(2)].unwrap_err_unchecked().kind, owe::ImageParseErrorKind::DecodeFailed);
    ASSERT_TRUE(images[usize(3)].is_ok());
    EXPECT_EQ((*images[usize(3)])->content->key, "last"_str);
}

TEST(ImageParser, CapturedSourceRetainsOldRuntimeImageAndParser) {
    Option<Arc<owe::SceneImageSource>> source;
    auto                               original = Arc<owe::Image>::make();
    original->content->key                      = "original"_Str;
    {
        owe::Scene scene;
        scene.SetImageParser(Arc<dyn<owe::IImageParser>>::make(MixedImageParser {}));
        scene.RegisterRuntimeImage(String::make("runtime"_str), original.clone());
        source = Some(scene.CaptureImageSource());
        scene.RegisterRuntimeImage(String::make("runtime"_str), Arc<owe::Image>::make());
    }
    auto retained = (*source)->Parse("runtime"_str);
    ASSERT_TRUE(retained.is_ok());
    EXPECT_TRUE(Arc<owe::Image>::ptr_eq(retained.unwrap_unchecked(), original));
    EXPECT_TRUE((*source)->Parse("bad"_str).is_err());
    auto first = (*source)->Parse("first"_str);
    ASSERT_TRUE(first.is_ok());
    auto reused = (*source)->Parse("first"_str);
    ASSERT_TRUE(reused.is_ok());
    EXPECT_TRUE(Arc<owe::Image>::ptr_eq(*first, *reused));
}

} // namespace

TEST_F(ConditionalTextureTest, ExternalPathsPreserveDecodedBytesAndMalformedEscapes) {
    struct Case {
        std::string filename;
        std::string encoded;
    };
    for (const auto& item : {
             Case { "space \xc3\xa9.ppm", "space%20%C3%A9.ppm" },
             Case { "raw-\xff.ppm", "raw-%FF.ppm" },
             Case { "literal%GG.ppm", "literal%GG.ppm" },
             Case { "nul.ppm", "nul.ppm%00ignored" },
         }) {
        {
            std::ofstream output(root / item.filename, std::ios::binary);
            output << "P6\n1 1\n255\n";
            const unsigned char pixel[] { 17, 34, 51 };
            output.write(reinterpret_cast<const char*>(pixel), sizeof(pixel));
        }
        for (const auto& prefix :
             { std::string {}, std::string("file://"), std::string("file://localhost") }) {
            const auto          key = prefix + root.string() + "/" + item.encoded;
            owe::TexImageParser parser(&vfs);
            auto                header = parser.ParseHeader(rstd::cppstd::as_str(key).unwrap());
            ASSERT_TRUE(header.is_ok()) << key;
            EXPECT_EQ(header->width, 1);
            EXPECT_EQ(header->height, 1);
            auto description = parser.Describe(rstd::cppstd::as_str(key).unwrap());
            ASSERT_TRUE(description.is_ok());
            ASSERT_EQ(description->slots.len(), usize(1));
            EXPECT_EQ(description->slots[usize()].width, 1);
            EXPECT_EQ(description->slots[usize()].height, 1);
            auto image = parser.Parse(rstd::cppstd::as_str(key).unwrap());
            ASSERT_TRUE(image.is_ok()) << key;
            auto& mip = (*image)->content->slots[usize()].mipmaps[usize()];
            EXPECT_EQ(mip.data.get()[0], 17);
            EXPECT_EQ(mip.data.get()[1], 34);
            EXPECT_EQ(mip.data.get()[2], 51);
            EXPECT_EQ(mip.data.get()[3], 255);
        }
    }
    owe::TexImageParser parser(&vfs);
    EXPECT_TRUE(parser.ParseHeader(rstd::cppstd::as_str(root.string()).unwrap()).is_err());
}

TEST(ImageParser, AssetVersionPreservesPrefixNumericAndCursorRules) {
    struct Case {
        ref<str> tag;
        int      expected;
    };
    for (const auto& item : array<Case, 7> {
             Case { "TEXV0005"_str, 5 },
             Case { "TEXZ12x9"_str, 12 },
             Case { "TEXV-123"_str, -123 },
             Case { "TEXV+123"_str, 0 },
             Case { "TEXV 123"_str, 0 },
             Case { "MDLV0005"_str, 0 },
             Case { "TEXV000"_str, 0 },
         }) {
        auto bytes = Vec<u8>::from(item.tag.as_bytes());
        bytes.push(u8());
        bytes.push(u8(127));
        owe::fs::BinaryReader reader(rstd::move(bytes));
        EXPECT_EQ(owe::ReadTexVersion(reader), item.expected);
        EXPECT_EQ(reader.Tell(), 9);
    }
}
