module;

#include <rstd/macro.hpp>
#include <lz4.h>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

module wescene.pkg.parse;
import wescene.core;
import wescene.types;
import rstd;
import rstd.log;
import wescene.utils;
import wescene.scene;
import wescene.pkg_asset_version;

using rstd::mem::memcmp;

using namespace owe;
using namespace rstd::prelude;
using namespace rstd::literals;
using rstd::ffi::CString;
using rstd::os::unix::ffi::OsStrExt;
using rstd::ptr_::copy_nonoverlapping;
using rstd::sync::Arc;

enum class TexFlagEnum : rstd::uint32_t
{
    // true for no bilinear
    noInterpolation = 0,
    // true for no repeat
    clampUVs = 1,
    sprite   = 2,

    compo1 = 20,
    compo2 = 21,
    compo3 = 22,
    compo4 = 23
};
using TexFlags = BitFlags<TexFlagEnum>;

namespace
{
// Magic-bytes sniffer used as a fallback when the .tex header's
// `image_type` slot says UNKNOWN but the body is actually an embedded
// image container. Some PKGV0022+ assets ship this way (the texture's
// declared image_type is -1 even though the LZ4-decompressed payload
// is a self-contained PNG/JPEG). Without this fallback the body bytes
// are memcpy'd into a "raw RGBA8" slot, which decodes to garbage and
// the wallpaper renders as a flat clear-color screen.
ImageType DetectEmbeddedImageType(const unsigned char* data, rstd::size_t size) {
    if (size >= 8 && memcmp(data, "\x89PNG\r\n\x1a\n", rstd::usize(8)) == 0) return ImageType::PNG;
    if (size >= 3 && data[0] == 0xff && data[1] == 0xd8 && data[2] == 0xff) return ImageType::JPEG;
    if (size >= 6 && (memcmp(data, "GIF87a", rstd::usize(6)) == 0 ||
                      memcmp(data, "GIF89a", rstd::usize(6)) == 0))
        return ImageType::GIF;
    if (size >= 2 && data[0] == 'B' && data[1] == 'M') return ImageType::BMP;
    if (size >= 4 && ((data[0] == 'I' && data[1] == 'I' && data[2] == 0x2a && data[3] == 0x00) ||
                      (data[0] == 'M' && data[1] == 'M' && data[2] == 0x00 && data[3] == 0x2a)))
        return ImageType::TIFF;
    // ISO BMFF / MP4 / MOV / 3GP — "....ftyp...." at offset 4. WE's scene
    // wallpapers can inline an H.264/AAC mp4 here; the renderer side
    // hands the bytes to wavsen::video::VideoDecoder.
    if (size >= 12 && memcmp(data + 4, "ftyp", rstd::usize(4)) == 0) return ImageType::VIDEO;
    // Matroska / WebM EBML header.
    if (size >= 4 && data[0] == 0x1A && data[1] == 0x45 && data[2] == 0xDF && data[3] == 0xA3)
        return ImageType::VIDEO;
    return ImageType::UNKNOWN;
}

TextureFormat ToTexFormate(int type) {
    /*
        type
        RGBA8888 = 0,
        DXT5 = 4,
        DXT3 = 6,
        DXT1 = 7,
        RG88 = 8,
        R8 = 9,
    */
    switch (type) {
    case 0: return TextureFormat::RGBA8;
    case 4: return TextureFormat::BC3;
    case 6: return TextureFormat::BC2;
    case 7: return TextureFormat::BC1;
    case 8: return TextureFormat::RG8;
    case 9: return TextureFormat::R8;
    default:
        rstd_error("ERROR::ToTexFormate Unkown image type: {}", type);
        return TextureFormat::RGBA8;
    }
}
struct TexBodyHeader {
    TexFormatVersion version;
    rstd::uint32_t   condition_count {};
};

auto InvalidTextureData(ref<str> name, ref<str> section) -> ImageParseError {
    return { .kind    = ImageParseErrorKind::InvalidData,
             .message = rstd::format("texture {} has invalid {}", name, section) };
}

auto ReadMipBytes(rstd::io::ReadRange source, bool compressed, int decoded_size, ref<str> name)
    -> Result<Vec<u8>, ImageParseError> {
    fs::BinaryReader reader(rstd::move(source));
    auto             input = reader.read_all_bytes();
    if (input.is_err()) return Err(InvalidTextureData(name, "mip payload"_str));
    if (! compressed) return Ok(rstd::move(input).unwrap());
    auto output = Vec<u8>::with_capacity(usize(decoded_size));
    output.resize(usize(decoded_size), u8());
    auto size = LZ4_decompress_safe(reinterpret_cast<const char*>(input->data()),
                                    reinterpret_cast<char*>(output.data()),
                                    static_cast<int>(input->len().to_primitive()),
                                    decoded_size);
    if (size != decoded_size) return Err(InvalidTextureData(name, "LZ4 payload"_str));
    return Ok(rstd::move(output));
}

constexpr stbi_io_callbacks ImageCallbacks {
    [](void* user, char* data, int size) -> int {
        return static_cast<int>(static_cast<fs::BinaryReader*>(user)->Read(data, size));
    },
    [](void* user, int count) {
        (void)static_cast<fs::BinaryReader*>(user)->SeekCur(count);
    },
    [](void* user) -> int {
        return static_cast<fs::BinaryReader*>(user)->remaining() == u64();
    },
};

auto DescribeMip(rstd::io::ReadRange source, bool compressed, int decoded_size, ImageType declared,
                 int width, int height, ref<str> name) -> Result<ImageType, ImageParseError> {
    fs::BinaryReader reader(source.clone());
    Vec<u8>          stored, prefix;
    if (compressed) {
        auto bytes = reader.read_all_bytes();
        if (bytes.is_err()) return Err(InvalidTextureData(name, "mip payload"_str));
        stored = rstd::move(bytes).unwrap();
    }
    int limit = compressed ? rstd::min(decoded_size, 128) : 16;
    for (;;) {
        prefix.resize(usize(limit), u8());
        int size;
        if (compressed) {
            size = LZ4_decompress_safe_partial(reinterpret_cast<const char*>(stored.data()),
                                               reinterpret_cast<char*>(prefix.data()),
                                               static_cast<int>(stored.len().to_primitive()),
                                               limit,
                                               limit);
            if (size < 0) return Err(InvalidTextureData(name, "LZ4 header"_str));
        } else {
            size = static_cast<int>(reader.Read(prefix.data(), limit));
            reader.Rewind();
        }
        auto type = declared == ImageType::UNKNOWN
                        ? DetectEmbeddedImageType(
                              reinterpret_cast<const unsigned char*>(prefix.data()), size)
                        : declared;
        if (type == ImageType::UNKNOWN) return Ok(type);
        if (type == ImageType::VIDEO) {
            if (compressed) return Err(InvalidTextureData(name, "compressed video"_str));
            return Ok(type);
        }
        int  w {}, h {}, channels {};
        bool valid =
            compressed
                ? stbi_info_from_memory(reinterpret_cast<const unsigned char*>(prefix.data()),
                                        size,
                                        &w,
                                        &h,
                                        &channels)
                : stbi_info_from_callbacks(&ImageCallbacks, &reader, &w, &h, &channels);
        if (valid) {
            if (w != width || h != height || i64(w) * i64(h) > i64(i32::MAX.to_primitive() / 4))
                return Err(InvalidTextureData(name, "container extent"_str));
            return Ok(type);
        }
        if (! compressed || limit == decoded_size || size < limit)
            return Err(InvalidTextureData(name, "container header"_str));
        limit = limit > decoded_size / 2 ? decoded_size : limit * 2;
    }
}

bool SkipTextureBytes(fs::BinaryReader& file, rstd::uint32_t size) {
    return u64(size) <= file.remaining() && file.SeekCur(static_cast<rstd::ptrdiff_t>(size));
}

auto SkipConditionalPatches(fs::BinaryReader& file, const TexBodyHeader& body, ref<str> name)
    -> Result<empty, ImageParseError> {
    if (body.condition_count == 0) return Ok(empty {});
    if (file.remaining() < u64(4)) return Err(InvalidTextureData(name, "patch groups"_str));
    const auto groups = file.ReadUint32();
    if (u64(groups) > file.remaining() / u64(4))
        return Err(InvalidTextureData(name, "patch group count"_str));
    for (rstd::uint32_t group = 0; group < groups; ++group) {
        if (file.remaining() < u64(4)) return Err(InvalidTextureData(name, "patch count"_str));
        const auto patches = file.ReadUint32();
        if (u64(patches) > file.remaining() / u64(32))
            return Err(InvalidTextureData(name, "patch count"_str));
        for (rstd::uint32_t patch = 0; patch < patches; ++patch) {
            // Conditional overrides follow each base mip, including when not applied.
            if (file.remaining() < u64(32)) return Err(InvalidTextureData(name, "patch"_str));
            file.ReadUint32(); // unknown
            file.ReadUint32(); // condition id
            file.ReadUint32(); // x
            file.ReadUint32(); // y
            file.ReadUint32(); // width
            file.ReadUint32(); // height
            file.ReadInt32();  // image type
            if (! SkipTextureBytes(file, file.ReadUint32()))
                return Err(InvalidTextureData(name, "patch payload"_str));
        }
    }
    return Ok(empty {});
}

auto LoadHeader(fs::BinaryReader& file, ImageHeader& header, ref<str> name)
    -> Result<TexBodyHeader, ImageParseError> {
    TexBodyHeader    body;
    TexFormatVersion v;
    v.texv = ReadTexVersion(file);
    v.texi = ReadTexVersion(file);
    (void)header.extraHeader.insert("texv"_Str, ImageExtra { .val = v.texv });
    (void)header.extraHeader.insert("texi"_Str, ImageExtra { .val = v.texi });

    header.format = ToTexFormate(file.ReadInt32());
    TexFlags flags(file.ReadUint32());
    {
        header.isSprite     = flags[TexFlagEnum::sprite];
        header.sample.wrapS = header.sample.wrapT =
            flags[TexFlagEnum::clampUVs] ? TextureWrap::CLAMP_TO_EDGE : TextureWrap::REPEAT;
        header.sample.minFilter = header.sample.magFilter =
            flags[TexFlagEnum::noInterpolation] ? TextureFilter::NEAREST : TextureFilter::LINEAR;
        (void)header.extraHeader.insert("compo1"_Str,
                                        ImageExtra { .val = flags[TexFlagEnum::compo1] });
        (void)header.extraHeader.insert("compo2"_Str,
                                        ImageExtra { .val = flags[TexFlagEnum::compo2] });
        (void)header.extraHeader.insert("compo3"_Str,
                                        ImageExtra { .val = flags[TexFlagEnum::compo3] });
        (void)header.extraHeader.insert("compo4"_Str,
                                        ImageExtra { .val = flags[TexFlagEnum::compo4] });
    }

    /*
        picture:
        width, height --> pow of 2 (tex size)
        mapw, maph    --> pic size
        mips
        mipw,miph     --> pow of 2

        sprites:
        width, height --> piece of sprite sheet
        mapw, maph    --> same
        1 mip
        mipw,mimp     --> tex size
    */

    header.width  = file.ReadInt32();
    header.height = file.ReadInt32();
    // in sprite this mean one pic
    header.mapWidth  = file.ReadInt32();
    header.mapHeight = file.ReadInt32();

    file.ReadInt32(); // unknown

    v.texb = ReadTexVersion(file);
    (void)header.extraHeader.insert("texb"_Str, ImageExtra { .val = v.texb });

    header.count = file.ReadInt32();

    if (v.body_has_image_type()) header.type = static_cast<ImageType>(file.ReadInt32());
    if (v.body_has_conditions()) {
        if (file.remaining() < u64(4)) return Err(InvalidTextureData(name, "condition count"_str));
        body.condition_count = file.ReadUint32();
        if (u64(body.condition_count) > file.remaining() / u64(13))
            return Err(InvalidTextureData(name, "condition count"_str));
        for (rstd::uint32_t index = 0; index < body.condition_count; ++index) {
            if (! SkipTextureBytes(file, 12))
                return Err(InvalidTextureData(name, "condition record"_str));
            char value;
            do {
                if (file.Read(&value, 1) != 1)
                    return Err(InvalidTextureData(name, "condition string"_str));
            } while (value != '\0');
        }
    }

    if (v.texv != 5 || v.texi != 1 || v.texb < 1 || v.texb > 4) {
        rstd_error(
            "TexImageParser: unsupported version texv={} texi={} texb={}", v.texv, v.texi, v.texb);
    }
    body.version = v;
    return Ok(body);
}

void SetHeaderPow2(ImageHeader& header, rstd::int32_t mip_0_w, rstd::int32_t mip_0_h) {
    header.mipmap_pow2   = algorism::IsPowOfTwo(u32(static_cast<rstd::uint32_t>(mip_0_w))) ||
                           algorism::IsPowOfTwo(u32(static_cast<rstd::uint32_t>(mip_0_h)));
    header.mipmap_larger = mip_0_w * mip_0_h > header.mapWidth * header.mapHeight;
}

Option<rstd::uint8_t> HexValue(char c) {
    if (c >= '0' && c <= '9') return Some(static_cast<rstd::uint8_t>(c - '0'));
    if (c >= 'a' && c <= 'f') return Some(static_cast<rstd::uint8_t>(c - 'a' + 10));
    if (c >= 'A' && c <= 'F') return Some(static_cast<rstd::uint8_t>(c - 'A' + 10));
    return None();
}

Option<Vec<u8>> PercentDecode(ref<str> raw) {
    auto       out   = Vec<u8>::with_capacity(raw.len());
    const auto bytes = raw.as_bytes();
    for (usize i {}; i < bytes.len();) {
        if (bytes[i] != u8('%')) {
            out.push(u8(bytes[i++]));
            continue;
        }
        if (i + usize(2) >= bytes.len()) return None();
        auto hi = HexValue(static_cast<char>(bytes[i + usize(1)].to_primitive()));
        auto lo = HexValue(static_cast<char>(bytes[i + usize(2)].to_primitive()));
        if (hi.is_none() || lo.is_none()) return None();
        out.push(u8(static_cast<rstd::uint8_t>((*hi << 4) | *lo)));
        i += usize(3);
    }
    return Some(rstd::move(out));
}

Option<CString> ResolveExternalImagePath(ref<str> name) {
    ref<str> path;
    if (name.starts_with("file://localhost/"_str)) {
        path = *name.get(usize(16), name.len());
    } else if (name.starts_with("file:///"_str)) {
        path = *name.get(usize(7), name.len());
    } else if (name.starts_with("/"_str)) {
        path = name;
    } else {
        return None();
    }
    auto decoded = PercentDecode(path);
    auto bytes   = decoded.is_some() ? rstd::move(*decoded) : Vec<u8>::from(path.as_bytes());
    // Filesystem and stb previously consumed the path through a terminating NUL.
    for (usize i {}; i < bytes.len(); ++i) {
        if (bytes[i] == u8()) {
            bytes.truncate(i);
            break;
        }
    }
    auto metadata = rstd::fs::metadata(fs::Path(OsStrExt::from_bytes(bytes.as_slice())));
    if (metadata.is_err() || ! metadata->is_file()) return None();
    return Some(CString::make(rstd::move(bytes)).unwrap());
}

ImageHeader MakeExternalImageHeader(int width, int height) {
    ImageHeader header;
    header.width     = width;
    header.height    = height;
    header.mapWidth  = width;
    header.mapHeight = height;
    header.format    = TextureFormat::RGBA8;
    header.type      = ImageType::PNG;
    header.count     = 1;
    header.sample    = TextureSample { TextureWrap::CLAMP_TO_EDGE,
                                       TextureWrap::CLAMP_TO_EDGE,
                                       TextureFilter::LINEAR,
                                       TextureFilter::LINEAR };
    SetHeaderPow2(header, width, height);
    return header;
}

auto ParseExternalImage(ref<str> key, const CString& path) -> Result<Arc<Image>, ImageParseError> {
    int   width = 0, height = 0, channels = 0;
    auto* pixels = stbi_load(path.as_ref().as_ptr(), &width, &height, &channels, 4);
    if (! pixels || width <= 0 || height <= 0) {
        if (pixels) stbi_image_free(pixels);
        return Err(ImageParseError {
            .kind    = ImageParseErrorKind::DecodeFailed,
            .message = rstd::format("decode external image {} failed", key),
        });
    }

    auto img_ptr          = Arc<Image>::make();
    img_ptr->content->key = String::make(key);
    img_ptr->header       = MakeExternalImageHeader(width, height);
    img_ptr->content->slots.push(vrento::Image::Slot {});
    auto& slot  = img_ptr->content->slots[usize(0)];
    slot.width  = width;
    slot.height = height;
    slot.mipmaps.push(ImageData {});
    auto& mipmap  = slot.mipmaps[usize(0)];
    mipmap.width  = width;
    mipmap.height = height;
    mipmap.size   = isize(static_cast<rstd::ptrdiff_t>(width * height * 4));
    mipmap.data   = ImageDataPtr(reinterpret_cast<rstd::uint8_t*>(pixels), [](rstd::uint8_t* data) {
        stbi_image_free(data);
    });
    img_ptr->FinalizeContent();
    return Ok(rstd::move(img_ptr));
}

} // namespace

auto TexImageParser::Parse(ref<str> name) const -> Result<Arc<Image>, ImageParseError> {
    if (ResolveExternalImagePath(name)) return Decode(name);
    // Package content is immutable for the lifetime of this parser snapshot.
    auto cell = [&] {
        auto cache = m_images->lock().unwrap_unchecked();
        if (auto image = cache->get(name)) return (*image)->clone();
        auto created = Arc<ImageCell>::make(None<Arc<Image>>());
        (void)cache->insert(String::make(name), created.clone());
        return created;
    }();
    bool publish {};
    auto result = [&]() -> Result<Arc<Image>, ImageParseError> {
        auto content = cell->lock().unwrap_unchecked();
        if (*content) return Ok((**content).clone());
        auto decoded = Decode(name);
        if (decoded.is_ok()) {
            content->insert(decoded->clone());
            publish = true;
        }
        return decoded;
    }();
    if (publish && m_ready) (*m_ready)->operator()((*result)->content.clone());
    return result;
}

auto TexImageParser::Decode(ref<str> name) const -> Result<Arc<Image>, ImageParseError> {
    if (auto path = ResolveExternalImagePath(name)) return ParseExternalImage(name, *path);
    auto prepared       = rstd_try(Prepare(name));
    auto image          = Arc<Image>::make();
    image->content->key = String::make(name);
    image->header       = prepared->description.header.clone();
    if (prepared->condition_count > 0)
        rstd_warn("texture {}: conditional patches are not applied; using base mipmaps", name);
    image->content->slots.reserve(prepared->slots.len());
    for (const auto& source_slot : prepared->slots) {
        vrento::Image::Slot slot;
        slot.mipmaps.reserve(source_slot.len());
        for (const auto& source : source_slot) {
            ImageData mip { .width = source.width, .height = source.height };
            if (source.type == ImageType::VIDEO) {
                mip.video_source = Some(source.source.clone());
            } else {
                auto bytes = rstd_try(ReadMipBytes(
                    source.source.clone(), source.compressed, source.decoded_size, name));
                if (source.type != ImageType::UNKNOWN) {
                    int   width {}, height {}, channels {};
                    auto* pixels =
                        stbi_load_from_memory(reinterpret_cast<const unsigned char*>(bytes.data()),
                                              static_cast<int>(bytes.len().to_primitive()),
                                              &width,
                                              &height,
                                              &channels,
                                              4);
                    if (! pixels)
                        return Err(ImageParseError {
                            .kind    = ImageParseErrorKind::DecodeFailed,
                            .message = rstd::format("decode embedded image {} failed", name),
                        });
                    mip.data = ImageDataPtr(pixels, [](rstd::uint8_t* data) {
                        stbi_image_free(data);
                    });
                    if (width != source.width || height != source.height)
                        return Err(InvalidTextureData(name, "decoded extent"_str));
                    mip.size = isize(rstd::ptrdiff_t(width) * height * 4);
                } else {
                    auto size = bytes.len().to_primitive();
                    mip.data  = ImageDataPtr(new rstd::uint8_t[size], [](rstd::uint8_t* data) {
                        delete[] data;
                    });
                    copy_nonoverlapping(ptr<rstd::uint8_t>::from_raw_parts(
                                            reinterpret_cast<const rstd::uint8_t*>(bytes.data())),
                                        mut_ptr<rstd::uint8_t>::from_raw_parts(mip.data.get()),
                                        bytes.len());
                    mip.size = isize(size);
                }
            }
            slot.mipmaps.push(rstd::move(mip));
        }
        if (! source_slot.is_empty()) {
            slot.width  = source_slot[usize()].width;
            slot.height = source_slot[usize()].height;
        }
        image->content->slots.push(rstd::move(slot));
    }
    image->FinalizeContent();
    return Ok(rstd::move(image));
}

auto owe::ParseImages(ref<dyn<IImageParser>> parser, slice<String> names, usize max_workers)
    -> Vec<Result<Arc<Image>, ImageParseError>> {
    auto parse_sequential = [parser, names]() mutable {
        auto images = Vec<Result<Arc<Image>, ImageParseError>>::with_capacity(names.len());
        for (usize index {}; index < names.len(); ++index)
            images.push(parser->Parse(names[index].as_str()));
        return images;
    };

    if (names.len() < usize(2) || max_workers < usize(2)) return parse_sequential();

    const auto worker_count = rstd::min(names.len(), max_workers);
    auto       group = rstd::thread::BlockingTaskGroup<Result<Arc<Image>, ImageParseError>>::make(
        worker_count, worker_count);
    if (group.is_err()) return parse_sequential();

    usize submitted_count {};
    for (; submitted_count < names.len(); ++submitted_count) {
        auto name      = names[submitted_count].clone();
        auto submitted = group->submit([parser, name = rstd::move(name)]() mutable {
            return parser->Parse(name.as_str());
        });
        if (submitted.is_err()) break;
    }

    auto outcomes = rstd::move(*group).join();
    auto images   = Vec<Result<Arc<Image>, ImageParseError>>::with_capacity(names.len());
    for (auto& outcome : outcomes) {
        auto value = rstd::move(outcome).into_value();
        if (value.is_some()) {
            images.push(rstd::move(value).unwrap_unchecked());
        } else {
            images.push(Err(ImageParseError {
                .kind    = ImageParseErrorKind::DecodeFailed,
                .message = "texture parse task failed"_Str,
            }));
        }
    }
    for (usize index = submitted_count; index < names.len(); ++index) {
        images.push(parser->Parse(names[index].as_str()));
    }
    return images;
}

auto TexImageParser::ParseMany(slice<String> names) const
    -> Vec<Result<Arc<Image>, ImageParseError>> {
    auto parser = dyn<IImageParser>::from_ref(*this);
    return ParseImages(parser, names);
}

auto TexImageParser::ParseHeader(ref<str> name) const -> Result<ImageHeader, ImageParseError> {
    if (auto path = ResolveExternalImagePath(name)) {
        int width {}, height {}, channels {};
        if (stbi_info(path->as_ref().as_ptr(), &width, &height, &channels) && width > 0 &&
            height > 0)
            return Ok(MakeExternalImageHeader(width, height));
        return Err(ImageParseError {
            .kind    = ImageParseErrorKind::DecodeFailed,
            .message = rstd::format("read external image header {} failed", name),
        });
    }
    if (name.contains("_alias_"_str)) return Ok(ImageHeader {});
    auto prepared = rstd_try(Prepare(name));
    return Ok(prepared->description.header.clone());
}

auto TexImageParser::Describe(ref<str> name) const -> Result<ImageDescription, ImageParseError> {
    if (ResolveExternalImagePath(name)) {
        auto             header = rstd_try(ParseHeader(name));
        ImageDescription result { .header = rstd::move(header) };
        result.slots.push({ result.header.width, result.header.height, usize(1) });
        return Ok(rstd::move(result));
    }
    auto prepared = rstd_try(Prepare(name));
    return Ok(prepared->description.clone());
}

auto TexImageParser::Prepare(ref<str> name) const -> Result<Arc<PreparedImage>, ImageParseError> {
    auto cache = m_headers->lock().unwrap_unchecked();
    if (auto cached = cache->get(name)) return Ok((*cached)->clone());
    auto path   = rstd::format("/assets/materials/{}.tex", name);
    auto source = m_vfs->open_read(fs::Path(path.as_str()));
    if (source.is_err())
        return Err(ImageParseError {
            .kind    = ImageParseErrorKind::MissingContent,
            .message = rstd::format("open texture {} failed", name),
        });
    auto             input = rstd::move(source).unwrap();
    fs::BinaryReader file(input.clone());
    auto             prepared = Arc<PreparedImage>::make();
    auto&            header   = prepared->description.header;
    auto             body     = rstd_try(LoadHeader(file, header, name));
    prepared->condition_count = body.condition_count;
    auto ver                  = body.version;
    if (header.count < 0 || u64(header.count) > file.remaining() / u64(4))
        return Err(InvalidTextureData(name, "image count"_str));
    auto                         declared = header.type;
    Vec<Option<array<float, 2>>> imageDatas;
    imageDatas.resize(usize(header.count), None());
    prepared->slots.reserve(usize(header.count));
    for (int index {}; index < header.count; ++index) {
        if (file.remaining() < u64(4)) return Err(InvalidTextureData(name, "mip count"_str));
        auto count = file.ReadInt32();
        if (count < 0 || count > 32) return Err(InvalidTextureData(name, "mip count"_str));
        Vec<PreparedMip> mips;
        mips.reserve(usize(count));
        for (int mip_index {}; mip_index < count; ++mip_index) {
            auto fields = ver.body_has_lz4_prelude() ? u64(20) : u64(12);
            if (file.remaining() < fields) return Err(InvalidTextureData(name, "mip header"_str));
            auto width  = file.ReadInt32();
            auto height = file.ReadInt32();
            bool compressed {};
            int  decoded_size {};
            if (ver.body_has_lz4_prelude()) {
                compressed   = file.ReadInt32() == 1;
                decoded_size = file.ReadInt32();
            }
            auto stored_size = file.ReadInt32();
            if (width <= 0 || height <= 0 || stored_size <= 0 || decoded_size < 0 ||
                (compressed && decoded_size == 0) || u64(stored_size) > file.remaining())
                return Err(InvalidTextureData(name, "mip payload"_str));
            auto range = input.subrange(file.position(), u64(stored_size));
            if (range.is_err()) return Err(InvalidTextureData(name, "mip range"_str));
            PreparedMip mip { .width        = width,
                              .height       = height,
                              .source       = rstd::move(range).unwrap(),
                              .compressed   = compressed,
                              .decoded_size = decoded_size };
            if (ver.body_has_image_type()) {
                mip.type = rstd_try(DescribeMip(mip.source.clone(),
                                                mip.compressed,
                                                mip.decoded_size,
                                                declared,
                                                mip.width,
                                                mip.height,
                                                name));
                if (mip.type != ImageType::UNKNOWN) {
                    header.type   = mip.type;
                    header.format = TextureFormat::RGBA8;
                }
            }
            if (mip_index == 0) {
                imageDatas[usize(index)] =
                    Some(array<float, 2> { float(mip.width), float(mip.height) });
                SetHeaderPow2(header, mip.width, mip.height);
            }
            if (! SkipTextureBytes(file, stored_size))
                return Err(InvalidTextureData(name, "mip payload"_str));
            rstd_try(SkipConditionalPatches(file, body, name));
            mips.push(rstd::move(mip));
        }
        ImageSlotDescription slot { .mip_count = mips.len() };
        if (! mips.is_empty()) {
            slot.width  = mips[usize()].width;
            slot.height = mips[usize()].height;
        }
        prepared->description.slots.push(rstd::move(slot));
        prepared->slots.push(rstd::move(mips));
    }
    if (header.isSprite) {
        // sprite pos
        ver.texs = ReadTexVersion(file);
        (void)header.extraHeader.insert("texs"_Str, ImageExtra { .val = ver.texs });
        if (ver.texs < 1 || ver.texs > 3) {
            return Err(ImageParseError {
                .kind = ImageParseErrorKind::InvalidData,
                .message =
                    rstd::format("texture {} has unsupported texs version {}", name, ver.texs),
            });
        }
        rstd::int32_t framecount = file.ReadInt32();
        if (framecount <= 0) {
            return Err(ImageParseError {
                .kind    = ImageParseErrorKind::InvalidData,
                .message = rstd::format("texture {} has no sprite frames", name),
            });
        }
        if (ver.sprite_has_atlas_size()) {
            rstd::int32_t width  = file.ReadInt32();
            rstd::int32_t height = file.ReadInt32();
            (void)width;
            (void)height;
        }

        for (rstd::int32_t i = 0; i < framecount; i++) {
            SpriteFrame sf;
            sf.imageId = file.ReadInt32();
            // Two ways an imageId can be poison: outright negative (old
            // sentinel) or pointing past image_count, or pointing to an
            // image whose mip section was empty. All three previously
            // tripped vector::operator[]'s assertion. Skip the frame's
            // remaining bytes so subsequent frames stay aligned.
            const auto bad_id =
                sf.imageId < 0 ||
                static_cast<rstd::size_t>(sf.imageId) >= imageDatas.len().to_primitive() ||
                imageDatas[usize(static_cast<rstd::size_t>(sf.imageId))].is_none();
            if (bad_id) {
                rstd_error("TexImageParser: invalid sprite frame imageId={} (image_count={}) in {}",
                           sf.imageId,
                           imageDatas.len().to_primitive(),
                           name);
                file.ReadFloat();             // frametime
                for (int j = 0; j < 6; ++j) { // x, y, xAxis[0..1], yAxis[0..1]
                    if (ver.sprite_frame_coords_int())
                        file.ReadInt32();
                    else
                        file.ReadFloat();
                }
                continue;
            }
            float spriteWidth =
                (*imageDatas[usize(static_cast<rstd::size_t>(sf.imageId))])[usize()];
            float spriteHeight =
                (*imageDatas[usize(static_cast<rstd::size_t>(sf.imageId))])[usize(1)];

            sf.frametime = file.ReadFloat();
            if (ver.sprite_frame_coords_int()) {
                sf.x               = (float)file.ReadInt32() / spriteWidth;
                sf.y               = (float)file.ReadInt32() / spriteHeight;
                sf.xAxis[usize()]  = (float)file.ReadInt32();
                sf.xAxis[usize(1)] = (float)file.ReadInt32();
                sf.yAxis[usize()]  = (float)file.ReadInt32();
                sf.yAxis[usize(1)] = (float)file.ReadInt32();
            } else {
                sf.x               = file.ReadFloat() / spriteWidth;
                sf.y               = file.ReadFloat() / spriteHeight;
                sf.xAxis[usize()]  = file.ReadFloat();
                sf.xAxis[usize(1)] = file.ReadFloat();
                sf.yAxis[usize()]  = file.ReadFloat();
                sf.yAxis[usize(1)] = file.ReadFloat();
            }
            sf.width  = (float)f64(f64(sf.xAxis[usize()]).powf(f64(2)).to_primitive() +
                                   f64(sf.xAxis[usize(1)]).powf(f64(2)).to_primitive())
                            .sqrt()
                            .to_primitive();
            sf.height = (float)f64(f64(sf.yAxis[usize()]).powf(f64(2)).to_primitive() +
                                   f64(sf.yAxis[usize(1)]).powf(f64(2)).to_primitive())
                            .sqrt()
                            .to_primitive();
            sf.xAxis[usize()] /= spriteWidth;
            sf.xAxis[usize(1)] /= spriteWidth;
            sf.yAxis[usize()] /= spriteHeight;
            sf.yAxis[usize(1)] /= spriteHeight;
            sf.rate = sf.height / sf.width;
            header.spriteAnim.AppendFrame(sf);
        }
        if (header.spriteAnim.numFrames() == usize()) {
            return Err(ImageParseError {
                .kind    = ImageParseErrorKind::InvalidData,
                .message = rstd::format("texture {} has no valid sprite frames", name),
            });
        }
    }
    (void)cache->insert(String::make(name), prepared.clone());
    return Ok(rstd::move(prepared));
}
