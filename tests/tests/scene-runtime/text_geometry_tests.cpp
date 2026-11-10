#include <rstd/test/gtest.hpp>

#include <array>
#include <memory>
#include <vector>

import rstd;
import rstd.cppstd;
import wescene.scene;
import wescene.text;
import wescene.script;
import wescene.json;
import wescene.testing.json_builder;
import wescene.types;

using namespace rstd::literals;
using namespace rstd::prelude;
using rstd::sync::Arc;

namespace
{
auto TextResources() -> Arc<owe::text::TextResources> {
    return Arc<owe::text::TextResources>::make();
}
auto TextFont() -> owe::text::TextFont { return { .family = "monospace"_Str, .size = 64 }; }
void Rasterize(owe::text::TextResources& resources, ref<str> text) {
    auto layout =
        resources.Layout({ .text = text, .family = "monospace"_str, .size = 64 }).unwrap();
    for (auto& g : layout.glyphs())
        resources.Rasterize(layout.fonts()[usize(g.font)], g.glyph).unwrap();
}
auto AtlasFont() -> vectext::FontInstance {
    struct Raster final : vectext::FontRasterizer {
        auto rasterize(vectext::Index glyph, double scale) const
            -> Result<vectext::GlyphBitmap, vectext::TextError> override {
            Vec<unsigned char> pixels;
            pixels.resize(usize(6), static_cast<unsigned char>(glyph));
            return Ok(vectext::GlyphBitmap { 3, 2, {}, scale, rstd::move(pixels) });
        }
    };
    auto raster = Box<Raster>::make();
    return vectext::FontInstance(
        "atlas-test"_Str,
        Box<vectext::FontRasterizer>::from_raw(
            rstd::move(raster).into_raw().cast<vectext::FontRasterizer>()));
}
// Glyphs drawn from shapes at any scale (1 unit = 1 pixel at scale 1), their coverage sampled 8 x 8
// a pixel: 1 a 10 x 10 square, 2 an equilateral triangle of side 20 pointing up, 3 a 20 x 20 square
// with a 10 x 10 hole. The origin is the shape's top-left in raster pixels, its bottom on the
// baseline.
auto ShapeFont() -> vectext::FontInstance {
    struct Raster final : vectext::FontRasterizer {
        auto rasterize(vectext::Index glyph, double scale) const
            -> Result<vectext::GlyphBitmap, vectext::TextError> override {
            const double size   = glyph == 1 ? 10 : 20;
            const double height = glyph == 2 ? size * 0.8660254037844386 : size;
            const int    w = int(size * scale + 0.999), h = int(height * scale + 0.999);
            auto         inside = [&](double x, double y) {
                if (glyph == 2)
                    return y >= height - (size / 2 - (x < size / 2 ? size / 2 - x : x - size / 2)) *
                                             1.7320508075688772 &&
                           y <= height;
                if (glyph == 3 && x > 5 && x < 15 && y > 5 && y < 15) return false;
                return x >= 0 && x <= size && y >= 0 && y <= height;
            };
            Vec<unsigned char> pixels;
            pixels.resize(usize(w * h), static_cast<unsigned char>(0));
            for (int y = 0; y < h; ++y)
                for (int x = 0; x < w; ++x) {
                    int hits {};
                    for (int sy = 0; sy < 8; ++sy)
                        for (int sx = 0; sx < 8; ++sx)
                            hits +=
                                inside((x + (sx + 0.5) / 8) / scale, (y + (sy + 0.5) / 8) / scale);
                    pixels[usize(y * w + x)] = static_cast<unsigned char>((hits * 255 + 32) / 64);
                }
            return Ok(vectext::GlyphBitmap { rstd::uint32_t(w),
                                             rstd::uint32_t(h),
                                             { 0, -double(h) },
                                             scale,
                                             rstd::move(pixels) });
        }
    };
    auto raster = Box<Raster>::make();
    return vectext::FontInstance(
        "shape-test"_Str,
        Box<vectext::FontRasterizer>::from_raw(
            rstd::move(raster).into_raw().cast<vectext::FontRasterizer>()));
}
// A pixel of an atlas glyph
auto AtlasPixel(owe::text::TextResources& resources, const owe::text::AtlasGlyph& glyph, int x,
                int y) -> int {
    auto& page = *resources.Pages()[usize(glyph.page)];
    return int(page.AtlasPixels()[usize(glyph.y + rstd::uint32_t(y)) * usize(page.Extent()) +
                                  usize(glyph.x + rstd::uint32_t(x))]);
}
} // namespace
TEST(TextAtlas, ImageRetainsLivePixelsAfterCacheDestruction) {
    Option<Arc<owe::Image>> image;
    Vec<rstd::uint8_t>      expected;
    {
        auto resources = TextResources();
        Rasterize(*resources, "A"_str);
        auto& page = *resources->Pages()[usize()];
        image      = owe::text::BuildAtlasImage(page, "atlas"_str);
        ASSERT_TRUE(image.is_some());
        Rasterize(*resources, "B"_str);
        expected = Vec<rstd::uint8_t>::from(page.AtlasPixels());
        EXPECT_EQ((*image)->content->slots[usize()].mipmaps[usize()].data.get(),
                  page.AtlasPixels().as_raw_ptr());
    }
    auto& mip = (*image)->content->slots[usize()].mipmaps[usize()];
    EXPECT_EQ(Vec<rstd::uint8_t>::from(
                  slice<rstd::uint8_t>::from_raw_parts(mip.data.get(), expected.len())),
              expected);
}

TEST(TextAtlas, DirtySnapshotsAreNondestructiveAndRetryable) {
    owe::text::AtlasPage page(16, "atlas"_Str, true);
    auto                 first = page.SnapshotDirtyUpdates();
    ASSERT_EQ(first.serial, 1u);
    ASSERT_EQ(first.rects.len(), usize(1));
    EXPECT_EQ(first.rects[usize()].rect.x, 0u);
    EXPECT_EQ(first.rects[usize()].rect.y, 0u);
    EXPECT_EQ(first.rects[usize()].rect.w, 4u);
    EXPECT_EQ(first.rects[usize()].rect.h, 4u);
    auto bytes = rstd::move(first.rects[usize()].bytes);
    EXPECT_FALSE(page.DirtyRects().is_empty());
    {
        auto cancelled = page.SnapshotDirtyUpdates();
        EXPECT_EQ(cancelled.serial, first.serial);
        EXPECT_EQ(cancelled.rects[usize()].bytes.as_slice(), bytes.as_slice());
    }
    auto retry = page.SnapshotDirtyUpdates();
    EXPECT_EQ(retry.serial, first.serial);
    ASSERT_EQ(retry.rects.len(), usize(1));
    EXPECT_EQ(retry.rects[usize()].bytes.as_slice(), bytes.as_slice());
    auto shared = retry.rects[usize()].bytes.clone();
    EXPECT_EQ(shared.as_slice().as_raw_ptr(), retry.rects[usize()].bytes.as_slice().as_raw_ptr());
    for (auto pixel : bytes.as_slice()) EXPECT_EQ(pixel, u8(255));
    page.AcknowledgeUpdates(retry.serial);
    EXPECT_TRUE(page.DirtyRects().is_empty());
    EXPECT_TRUE(page.SnapshotDirtyUpdates().rects.is_empty());
}

TEST(TextAtlas, DirtySnapshotOwnsPackedPixelsAfterPageDestruction) {
    Option<owe::text::AtlasUpdateBatch> snapshot;
    Vec<u8>                             expected;
    {
        owe::text::TextResources resources({ .extent = 16 });
        auto                     font = AtlasFont();
        resources.Rasterize(font, 1).unwrap();
        resources.Rasterize(font, 2).unwrap();
        resources.Rasterize(font, 3).unwrap();
        auto& page = *resources.Pages()[usize()];
        snapshot   = Some(page.SnapshotDirtyUpdates());
        ASSERT_EQ(snapshot->rects.len(), usize(1));
        auto& update = snapshot->rects[usize()];
        auto& rect   = update.rect;
        EXPECT_EQ(update.bytes.len(), usize(rect.w) * usize(rect.h));
        for (rstd::uint32_t y = 0; y < rect.h; ++y)
            for (rstd::uint32_t x = 0; x < rect.w; ++x)
                EXPECT_EQ(update.bytes[usize(y) * usize(rect.w) + usize(x)],
                          u8(page.AtlasPixels()[usize(rect.y + y) * usize(page.Extent()) +
                                                usize(rect.x + x)]));
        expected  = Vec<u8>::from(update.bytes.as_slice());
        auto next = resources.Rasterize(font, 4).unwrap();
        ASSERT_GE(next.x, rect.x);
        ASSERT_GE(next.y, rect.y);
        ASSERT_LE(next.x + next.width, rect.x + rect.w);
        ASSERT_LE(next.y + next.height, rect.y + rect.h);
        EXPECT_EQ(update.bytes[usize(next.y - rect.y) * usize(rect.w) + usize(next.x - rect.x)],
                  u8());
        EXPECT_EQ(page.AtlasPixels()[usize(next.y) * usize(page.Extent()) + usize(next.x)], 4u);
        EXPECT_EQ(update.bytes.as_slice(), expected.as_slice());
        page.AcknowledgeUpdates(snapshot->serial);
    }
    EXPECT_EQ(snapshot->rects[usize()].bytes.as_slice(), expected.as_slice());
}

TEST(TextAtlas, AcknowledgeSnapshotPreservesNewDirty) {
    owe::text::TextResources resources({ .extent = 32 });
    auto                     font = AtlasFont();
    resources.Rasterize(font, 1).unwrap();
    auto& page  = *resources.Pages()[usize()];
    auto  first = page.SnapshotDirtyUpdates();
    auto  next  = resources.Rasterize(font, 2).unwrap();
    EXPECT_EQ(page.SnapshotDirtyUpdates().serial, first.serial + 1);
    page.AcknowledgeUpdates(first.serial);
    ASSERT_EQ(page.DirtyRects().len(), usize(1));
    auto pending = page.SnapshotDirtyUpdates();
    ASSERT_EQ(pending.rects.len(), usize(1));
    EXPECT_EQ(pending.rects[usize()].rect.x, next.x);
    EXPECT_EQ(pending.rects[usize()].rect.y, next.y);
    EXPECT_EQ(pending.rects[usize()].rect.w, next.width);
    EXPECT_EQ(pending.rects[usize()].rect.h, next.height);
    for (auto pixel : pending.rects[usize()].bytes.as_slice()) EXPECT_EQ(pixel, u8(2));
    page.AcknowledgeUpdates(first.serial);
    page.AcknowledgeUpdates(0);
    page.AcknowledgeUpdates(pending.serial + 1);
    EXPECT_EQ(page.DirtyRects().len(), usize(1));
    page.AcknowledgeUpdates(pending.serial);
    EXPECT_TRUE(page.DirtyRects().is_empty());
    resources.Rasterize(font, 3).unwrap();
    auto latest = page.SnapshotDirtyUpdates();
    EXPECT_EQ(latest.serial, pending.serial + 1);
    page.AcknowledgeUpdates(pending.serial);
    EXPECT_EQ(page.DirtyRects().len(), usize(1));
}

TEST(TextAtlas, LaterSnapshotConfirmationMakesEarlierConfirmationHarmless) {
    owe::text::TextResources resources({ .extent = 32 });
    auto                     font  = AtlasFont();
    auto&                    page  = *resources.Pages()[usize()];
    auto                     first = page.SnapshotDirtyUpdates();
    resources.Rasterize(font, 1).unwrap();
    auto later = page.SnapshotDirtyUpdates();
    page.AcknowledgeUpdates(later.serial);
    EXPECT_TRUE(page.DirtyRects().is_empty());
    resources.Rasterize(font, 2).unwrap();
    page.AcknowledgeUpdates(first.serial);
    page.AcknowledgeUpdates(later.serial);
    EXPECT_EQ(page.DirtyRects().len(), usize(1));
    page.AcknowledgeUpdates(page.SnapshotDirtyUpdates().serial);
    EXPECT_TRUE(page.DirtyRects().is_empty());
}

TEST(VideoPlayback, AdapterSharesAndRetainsControlState) {
    using namespace rstd;
    auto                                  state = sync::Arc<owe::VideoPlaybackState>::make();
    sync::Arc<dyn<vrento::VideoPlayback>> playback =
        sync::Arc<dyn<vrento::VideoPlayback>>::make(owe::SharedVideoPlayback { state.clone() });
    state->Pause();
    state->Seek(f64(3));
    state->SetRate(f64(2));
    EXPECT_FALSE(playback->Snapshot().playing);
    EXPECT_EQ(playback->Snapshot().seek_seconds, f64(3));
    EXPECT_EQ(playback->Snapshot().rate, f64(2));
    playback->PublishTime(f64(4), Some(f64(20)));
    EXPECT_EQ(state->CurrentTime(), f64(4));
    EXPECT_EQ(state->Duration().unwrap(), f64(20));
    state = sync::Arc<owe::VideoPlaybackState>::make();
    playback->PublishTime(f64(5), None<f64>());
    EXPECT_FALSE(playback->Snapshot().playing);
    EXPECT_EQ(playback->Snapshot().seek_seconds, f64(3));
}

TEST(TextLayouter, TabIsIgnoredWithoutAControlQuad) {
    auto                  resources  = TextResources();
    constexpr std::size_t peak_quads = 16;
    auto                  mesh       = Arc<owe::SceneMesh>::make();
    rstd::initializer_list<owe::SceneVertexArray::SceneVertexAttribute> attributes {
        { .name = "a_Position"_Str, .type = owe::VertexType::FLOAT3 },
        { .name = "a_TexCoord"_Str, .type = owe::VertexType::FLOAT2 },
        { .name = "a_Color"_Str, .type = owe::VertexType::FLOAT4 },
    };
    mesh->AddVertexArray(owe::SceneVertexArray(attributes, rstd::usize(peak_quads * 4)));
    mesh->AddIndexArray(owe::SceneIndexArray(rstd::usize(peak_quads * 6)));

    owe::text::TextLayouter layouter(resources.clone(), TextFont(), mesh.clone(), {}, peak_quads);
    layouter.SetText("hour:\n\t\t\t\tminute:"_str);
    const float tab_width = layouter.TextWidth();
    EXPECT_EQ(mesh->GetIndexArray(rstd::usize()).RenderDataCount(), rstd::usize(72));

    layouter.SetText("hour:\nminute:"_str);
    EXPECT_FLOAT_EQ(layouter.TextWidth(), tab_width);
    EXPECT_EQ(mesh->GetIndexArray(rstd::usize()).RenderDataCount(), rstd::usize(72));
}

TEST(TextLayouter, LayoutOriginPreservesFontBaselineAcrossTextChanges) {
    auto resources = TextResources();
    const rstd::initializer_list<owe::SceneVertexArray::SceneVertexAttribute> attributes {
        { .name = "a_Position"_Str, .type = owe::VertexType::FLOAT3 },
        { .name = "a_TexCoord"_Str, .type = owe::VertexType::FLOAT2 },
        { .name = "a_Color"_Str, .type = owe::VertexType::FLOAT4 },
    };
    for (auto origin :
         { owe::text::TextMeshOrigin::Layout, owe::text::TextMeshOrigin::InkBounds }) {
        auto mesh = Arc<owe::SceneMesh>::make();
        mesh->AddVertexArray(owe::SceneVertexArray(attributes, rstd::usize(4)));
        mesh->AddIndexArray(owe::SceneIndexArray(rstd::usize(6)));
        owe::text::TextLayoutStyle style;
        style.mesh_origin = origin;
        owe::text::TextLayouter layouter(
            resources.clone(), TextFont(), mesh.clone(), rstd::move(style), 1);
        for (const auto* text : { "0", "g" }) {
            layouter.SetText(rstd::cppstd::as_str(text).unwrap());
            const auto& layout = *layouter.Layout();
            const auto& glyph  = layout.glyphs()[usize()];
            const auto  bitmap =
                layout.fonts()[usize(glyph.font)].rasterize(glyph.glyph, 1).unwrap();
            const auto  metrics  = layouter.Metrics();
            const float baseline = metrics.text_height * .5f - float(layout.baseline());
            const float top      = baseline - float(bitmap.origin.y);
            const float center   = top - float(bitmap.height) * .5f;
            EXPECT_FLOAT_EQ(metrics.source_center_y, center);
            const auto& vertices = mesh->GetVertexArray(rstd::usize());
            const auto  stride   = vertices.OneSize().to_primitive();
            const float offset   = origin == owe::text::TextMeshOrigin::InkBounds ? center : 0.0f;
            EXPECT_FLOAT_EQ(vertices.Data()[1], top - offset);
            EXPECT_FLOAT_EQ(vertices.Data()[2 * stride + 1],
                            top - static_cast<float>(bitmap.height) - offset);
        }
    }
}

TEST(TextLayouter, FontBoundsExcludeTrailingLeadingWithoutMovingBaselines) {
    auto source      = rstd::path::PathBuf::from(__FILE__ ""_str);
    auto root        = source.as_path().parent().unwrap().parent().unwrap().parent().unwrap();
    auto path        = rstd::path::PathBuf::from(root).join("fixtures/fonts/LineGap.ttf"_str);
    auto bytes       = rstd::fs::read(path.as_path()).unwrap();
    auto resources   = TextResources();
    auto font_source = resources->RegisterFont(bytes.as_slice()).unwrap();
    auto mesh        = Arc<owe::SceneMesh>::make();
    owe::text::TextLayoutStyle style;
    style.mesh_origin = owe::text::TextMeshOrigin::Layout;
    style.halign      = "left"_Str;
    style.wrap_width  = 300;
    owe::text::TextLayouter layouter(resources.clone(),
                                     { .source = Some(font_source), .size = 100 },
                                     mesh.clone(),
                                     rstd::move(style),
                                     8);
    for (auto text : array<ref<str>, 3> { "A"_str, "g"_str, "A\n\ng"_str }) {
        ASSERT_TRUE(layouter.SetText(text).is_ok());
        const auto& layout = *layouter.Layout();
        const auto  bounds = layout.typographic_bounds();
        EXPECT_FLOAT_EQ(layouter.TextWidth(), 300);
        EXPECT_FLOAT_EQ(layouter.TextHeight(), float(bounds.height));
        EXPECT_FLOAT_EQ(float(layout.logical_bounds().height - bounds.height), 100);
        const auto& glyph    = layout.glyphs()[usize()];
        auto        bitmap   = layout.fonts()[usize(glyph.font)].rasterize(glyph.glyph, 1).unwrap();
        const auto& vertices = mesh->GetVertexArray(usize());
        EXPECT_FLOAT_EQ(vertices.Data()[1],
                        float(bounds.height / 2 + bounds.y - glyph.position.y - bitmap.origin.y));
    }
    EXPECT_FLOAT_EQ(layouter.TextHeight(), 500);
    ASSERT_TRUE(layouter.SetText("A"_str).is_ok());
    EXPECT_FLOAT_EQ(layouter.TextHeight(), 100);
    ASSERT_TRUE(layouter.SetPixelSize(50).is_ok());
    EXPECT_FLOAT_EQ(layouter.TextHeight(), 50);
    ASSERT_TRUE(layouter.SetHorizontalAlign("right"_str).is_ok());
    EXPECT_FLOAT_EQ(layouter.TextHeight(), 50);
    EXPECT_FLOAT_EQ(layouter.TextWidth(), 300);
}

TEST(TextGeometry, DynamicEffectFollowsCurrentTextBounds) {
    const owe::text::TextGeometryPolicy policy {
        .frame_width  = 419.0f,
        .frame_height = 221.0f,
        .dynamic      = true,
        .has_effect   = true,
    };
    const owe::text::TextLayoutMetrics metrics {
        .text_width    = 607.0f,
        .text_height   = 157.0f,
        .source_width  = 563.0f,
        .source_height = 143.0f,
        .padding       = 32.0f,
    };

    const auto geometry = owe::text::ResolveTextGeometry(policy, metrics);

    EXPECT_FLOAT_EQ(geometry.rt_width, 671.0f);
    EXPECT_FLOAT_EQ(geometry.draw_width, 671.0f);
    EXPECT_FLOAT_EQ(geometry.uv_source_width, 671.0f);
    EXPECT_FLOAT_EQ(geometry.effect_frame_width, 671.0f);
    EXPECT_FLOAT_EQ(geometry.draw_height, 221.0f);
    EXPECT_FLOAT_EQ(geometry.uv_source_height, 221.0f);
    EXPECT_FLOAT_EQ(geometry.effect_frame_height, 221.0f);
}

TEST(TextLayouter, FailedUpdatesKeepAcceptedStateAndRetry) {
    auto                    resources = TextResources();
    auto                    mesh      = Arc<owe::SceneMesh>::make();
    owe::text::TextLayouter layouter(resources.clone(), TextFont(), mesh.clone(), {}, 1);
    ASSERT_TRUE(layouter.SetText("A"_str).is_ok());
    auto id    = layouter.Layout()->id();
    auto count = resources->LayoutCount();
    EXPECT_TRUE(layouter.SetText("A"_str).is_ok());
    EXPECT_EQ(resources->LayoutCount(), count);
    EXPECT_EQ(layouter.SetText("AB"_str).unwrap_err(), vectext::TextError::Limit);
    EXPECT_EQ(layouter.Layout()->id(), id);
    EXPECT_EQ(layouter.SetText("AB"_str).unwrap_err(), vectext::TextError::Limit);
    EXPECT_EQ(resources->LayoutCount(), count + 2);
    EXPECT_EQ(layouter.SetPixelSize(-1).unwrap_err(), vectext::TextError::InvalidValue);
    EXPECT_EQ(layouter.Layout()->id(), id);
    EXPECT_TRUE(layouter.SetText("B"_str).is_ok());
    EXPECT_TRUE(layouter.Layout()->id() != id);
}
TEST(TextAtlas, InstanceScaleAndPageIdentity) {
    struct Raster final : vectext::FontRasterizer {
        auto rasterize(vectext::Index, double scale) const
            -> Result<vectext::GlyphBitmap, vectext::TextError> override {
            Vec<unsigned char> pixels;
            pixels.resize(usize(36), 255);
            return Ok(vectext::GlyphBitmap { 6, 6, {}, scale, rstd::move(pixels) });
        }
    };
    auto make_font = [] {
        auto raster = Box<Raster>::make();
        return vectext::FontInstance(
            "test"_Str,
            Box<vectext::FontRasterizer>::from_raw(
                rstd::move(raster).into_raw().cast<vectext::FontRasterizer>()));
    };
    owe::text::TextResources resources({ .extent = 8, .max_pages = 4 });
    auto                     a = make_font(), b = make_font();
    auto                     first    = resources.Rasterize(a, 1).unwrap();
    auto                     repeated = resources.Rasterize(a, 1).unwrap();
    EXPECT_EQ(first.page, repeated.page);
    EXPECT_EQ(resources.RasterCount(), 1u);
    auto second = resources.Rasterize(b, 1).unwrap();
    EXPECT_NE(first.page, second.page);
    auto scaled = resources.Rasterize(a, 1, 2).unwrap();
    EXPECT_NE(scaled.page, first.page);
    EXPECT_EQ(resources.Rasterize(a, 2).unwrap_err(), vectext::TextError::Limit);
    EXPECT_TRUE(resources.Rasterize(a, 1).is_ok());
    EXPECT_FALSE(resources.Pages()[usize(first.page)]->DirtyRects().is_empty());
}
TEST(TextLayouter, ContinuousBatchesPreservePageOrder) {
    auto resources = Arc<owe::text::TextResources>::make(
        owe::text::AtlasOptions { .extent = 64, .max_pages = 16 });
    auto                    mesh = Arc<owe::SceneMesh>::make();
    owe::text::TextLayouter layouter(resources.clone(), TextFont(), mesh.clone(), {}, 32);
    ASSERT_TRUE(layouter.SetText("WMW"_str).is_ok());
    ASSERT_EQ(mesh->Submeshes().len(), usize(3));
    EXPECT_EQ(mesh->Submeshes()[usize()].material_slot, mesh->Submeshes()[usize(2)].material_slot);
    EXPECT_NE(mesh->Submeshes()[usize()].material_slot, mesh->Submeshes()[usize(1)].material_slot);
    EXPECT_FALSE(mesh->Submeshes()[usize()].preserve_output);
    EXPECT_TRUE(mesh->Submeshes()[usize(1)].preserve_output);
}
TEST(TextLayouter, ResourcesSurviveSceneExtensionDestruction) {
    Option<Box<owe::text::TextLayouter>> layouter;
    {
        owe::Scene scene;
        auto       resources = owe::text::EnsureSceneTextResources(scene);
        auto       mesh      = Arc<owe::SceneMesh>::make();
        layouter             = Some(Box<owe::text::TextLayouter>::make(
            resources.clone(), TextFont(), mesh.clone(), owe::text::TextLayoutStyle {}, 16));
        ASSERT_TRUE((*layouter)->SetText("A"_str).is_ok());
    }
    EXPECT_TRUE((*layouter)->SetText("B"_str).is_ok());
}
TEST(TextLayouter, WarmUpdatesKeepBufferStorageAndRasterCache) {
    auto                    resources = TextResources();
    auto                    mesh      = Arc<owe::SceneMesh>::make(true);
    owe::text::TextLayouter layouter(resources.clone(), TextFont(), mesh.clone(), {}, 8);
    ASSERT_TRUE(layouter.SetText("AB"_str).is_ok());
    auto generation = mesh->BufferView(u32())->vertices[usize()].storage_generation;
    auto rasters    = resources->RasterCount();
    mesh->ConsumeDirtyFlags();
    ASSERT_TRUE(layouter.SetText("BA"_str).is_ok());
    EXPECT_EQ(mesh->BufferView(u32())->vertices[usize()].storage_generation, generation);
    EXPECT_EQ(resources->RasterCount(), rasters);
    EXPECT_EQ(mesh->DirtyFlags() & owe::SceneMeshDirtyLayout, owe::SceneMeshDirtyNone);
    EXPECT_NE(mesh->DirtyFlags() & owe::SceneMeshDirtyData, owe::SceneMeshDirtyNone);
}
TEST(TextAtlas, RegistrationRetainsDirtyPixelsUntilUpload) {
    owe::Scene scene;
    auto       resources = owe::text::EnsureSceneTextResources(scene);
    Rasterize(*resources, "A"_str);
    ASSERT_TRUE(resources->RegisterPages(scene));
    auto& page = *resources->Pages()[usize()];
    EXPECT_FALSE(page.DirtyRects().is_empty());
    EXPECT_TRUE(scene.Texture(page.AtlasUrl()).is_some());
    page.AcknowledgeUpdates(page.SnapshotDirtyUpdates().serial);
    EXPECT_TRUE(page.DirtyRects().is_empty());
    Rasterize(*resources, "B"_str);
    EXPECT_FALSE(page.DirtyRects().is_empty());
}

TEST(TextRuntime, RejectedScriptPropertiesRemainReadableAndRetry) {
    owe::SceneNode         node;
    owe::script::JsRuntime runtime;
    bool                   accept {};
    unsigned               attempts {};
    runtime.RegisterTextAlignSetters(
        &node,
        "center"_Str,
        "center"_Str,
        12,
        owe::script::JsRuntime::TextAlignSetter::make([&](ref<str>) {
            ++attempts;
            return accept;
        }),
        owe::script::JsRuntime::TextAlignSetter::make([](ref<str>) {
            return true;
        }),
        None(),
        Some(owe::script::JsRuntime::PointSizeSetter::make([&](double) {
            ++attempts;
            return accept;
        })));
    auto* script = runtime.MakeFieldScript(
        "export function update() { thisLayer.horizontalalign='left'; thisLayer.pointsize=24; return (thisLayer.horizontalalign==='left' ? 100 : 0) + thisLayer.pointsize; }"_str,
        "text-rejected-setters"_str,
        owe::script::FieldKind::Scalar,
        owe::MakeObject(),
        owe::IntoJson(0),
        owe::script::ScriptBindingContext::ForLayer(&node, "visible"_str));
    ASSERT_NE(script, nullptr);
    runtime.SetSceneRoot(&node);
    runtime.TickAll();
    EXPECT_EQ(script->last_value().as_Scalar().value.v, 12.0);
    EXPECT_EQ(attempts, 2u);
    accept = true;
    runtime.TickAll();
    EXPECT_EQ(script->last_value().as_Scalar().value.v, 124.0);
    EXPECT_EQ(attempts, 4u);
}

TEST(TextAtlas, InvalidPageExtentHasNoPixels) {
    owe::text::AtlasPage page(0, "invalid"_Str, true);
    EXPECT_TRUE(page.AtlasPixels().is_empty());
    EXPECT_TRUE(owe::text::BuildAtlasImage(page, "invalid"_str).is_none());
    auto snapshot = page.SnapshotDirtyUpdates();
    EXPECT_EQ(snapshot.serial, 0u);
    EXPECT_TRUE(snapshot.rects.is_empty());
    page.AcknowledgeUpdates(1);
    EXPECT_TRUE(page.DirtyRects().is_empty());
}

TEST(TextAtlas, AnOutlinedGlyphIsTheGlyphGrownByTheThickness) {
    // Wallpaper Engine's outline reaches outlinethickness layer units from the glyph (text-outline)
    owe::text::TextResources resources;
    auto                     font  = ShapeFont();
    auto                     plain = resources.Rasterize(font, 1).unwrap();
    auto                     grown = resources.Rasterize(font, 1, 1, 3.0).unwrap();
    ASSERT_EQ(plain.width, 10u);
    ASSERT_EQ(plain.height, 10u);
    // grown by 3 on every side, its origin moved to match
    EXPECT_EQ(grown.width, 10u + 6u);
    EXPECT_EQ(grown.height, 10u + 6u);
    EXPECT_DOUBLE_EQ(grown.origin.x, plain.origin.x - 3);
    EXPECT_DOUBLE_EQ(grown.origin.y, plain.origin.y - 3);
    EXPECT_EQ(AtlasPixel(resources, grown, 8, 8), 255);                  // the glyph itself
    EXPECT_EQ(AtlasPixel(resources, grown, 0, 8), 255);                  // 3 left of it
    EXPECT_EQ(AtlasPixel(resources, grown, 8, 0), 255);                  // 3 above it
    EXPECT_EQ(AtlasPixel(resources, grown, 15, 8), 255);                 // 3 right of it
    EXPECT_EQ(resources.Rasterize(font, 1, 1, 3.0).unwrap().x, grown.x); // cached
}

TEST(TextAtlas, AnOutlineKeepsASquareGlyphsCornersSquare) {
    // Wallpaper Engine strokes the glyph's outline with mitred joins (text-outline-stroke): a
    // square letter's outline is a square, not one with rounded corners
    owe::text::TextResources resources;
    auto                     font  = ShapeFont();
    auto                     grown = resources.Rasterize(font, 1, 1, 6.0).unwrap();
    ASSERT_EQ(grown.width, 22u);
    ASSERT_EQ(grown.height, 22u);
    EXPECT_EQ(AtlasPixel(resources, grown, 0, 0), 255);   // 6 left of and 6 above the corner
    EXPECT_EQ(AtlasPixel(resources, grown, 21, 21), 255); // the opposite corner
    EXPECT_EQ(AtlasPixel(resources, grown, 21, 0), 255);
    EXPECT_EQ(AtlasPixel(resources, grown, 1, 1), 255);
}

TEST(TextAtlas, AnOutlineMitresAPointedGlyph) {
    // an equilateral triangle's corners are 60 degrees: the mitre reaches twice the thickness past
    // each
    owe::text::TextResources resources;
    auto                     font  = ShapeFont();
    auto                     plain = resources.Rasterize(font, 2).unwrap();
    auto                     grown = resources.Rasterize(font, 2, 1, 4.0).unwrap();
    // the apex (10, 0 in the glyph) reaches 8 up; a round outline would reach 4
    const double apex_x = plain.origin.x + 10 - grown.origin.x,
                 apex_y = plain.origin.y - grown.origin.y;
    EXPECT_NEAR(apex_y, 8.0, 1.0);
    EXPECT_GE(AtlasPixel(resources, grown, int(apex_x), int(apex_y) - 6), 128); // 6 above the apex
    EXPECT_LT(AtlasPixel(resources, grown, int(apex_x), 0), 128); // 8 above: the mitre's tip
    // the base's corners mitre out 2 x 4 along the base too: 4 * sqrt(3) to the side, 4 below
    EXPECT_NEAR(double(grown.width), 20 + 2 * 4 * 1.7320508075688772, 1.0);
}

TEST(TextAtlas, AnOutlineGrowsIntoALettersHole) {
    owe::text::TextResources resources;
    auto                     font  = ShapeFont();
    auto                     grown = resources.Rasterize(font, 3, 1, 2.0).unwrap();
    ASSERT_EQ(grown.width, 24u);
    // the hole spans 7..17 in the outline's bitmap: its outer 2 pixels are outline, its middle is
    // empty
    EXPECT_EQ(AtlasPixel(resources, grown, 7, 12), 255);
    EXPECT_EQ(AtlasPixel(resources, grown, 8, 12), 255);
    EXPECT_EQ(AtlasPixel(resources, grown, 12, 12), 0);
    EXPECT_EQ(AtlasPixel(resources, grown, 15, 12), 255);
    // and the hole's corners stay square
    EXPECT_EQ(AtlasPixel(resources, grown, 9, 9), 0);
    EXPECT_EQ(AtlasPixel(resources, grown, 8, 8), 255);
}

TEST(TextLayouter, AnOutlineDrawsTheGrownGlyphsBehindTheText) {
    auto                       resources = TextResources();
    auto                       mesh      = Arc<owe::SceneMesh>::make();
    owe::text::TextLayoutStyle style;
    style.color             = { 1.0f, 0.85f, 0.3f };
    style.alpha             = 0.5f;
    style.outline           = true;
    style.outline_color     = { 0.6f, 0.05f, 0.05f };
    style.outline_thickness = 4.0f;
    owe::text::TextLayouter layouter(
        resources.clone(), TextFont(), mesh.clone(), rstd::move(style), 8);
    ASSERT_TRUE(layouter.SetText("H"_str).is_ok());
    const auto& vertices = mesh->GetVertexArray(usize());
    // two quads: the outline, then the letter
    ASSERT_GE(vertices.VertexCount(), usize(8));
    const float* data = vertices.Data();
    // position, texcoord and colour, each padded to the same width
    const auto stride = vertices.OneSize().to_primitive();
    const auto colour = stride / 3 * 2;
    auto       at     = [&](rstd::size_t vertex, rstd::size_t offset) {
        return data[vertex * stride + offset];
    };
    EXPECT_FLOAT_EQ(at(0, colour), 0.6f);
    EXPECT_FLOAT_EQ(at(0, colour + 3), 0.5f);
    EXPECT_FLOAT_EQ(at(4, colour), 1.0f);
    // the outline quad is 2 * 4 wider than the letter (H's corners are square, so nothing mitres
    // past that)
    const float outline_width = at(1, 0) - at(0, 0);
    const float letter_width  = at(5, 0) - at(4, 0);
    EXPECT_NEAR(outline_width - letter_width, 8.0f, 2.0f);
}

TEST(TextOutlinePlace, AGlyphGoesWhereWallpaperEnginesSignedDistanceAtlasPutsIt) {
    // Wallpaper Engine draws outlined text from a signed distance atlas (text-outline-offset): the
    // glyph's whole-pixel box (231 x 304 here, H at 100 points: 417 pixels an em) is stored at 32
    // pixels an em, its sizes truncated (17 x 23) and scaled by the larger, with 12 pixels round
    // it; drawn back on the box grown by 12 / 32 em, the letter lands a little right of and inside
    // its place
    const auto place = owe::text::OutlinedGlyphPlace(41, 272, -304, 0, 417);
    EXPECT_NEAR(place.scale_x * 41 + place.offset_x, 43.7713, 1e-3);
    EXPECT_NEAR(place.scale_x * 272 + place.offset_x, 275.5544, 1e-3);
    EXPECT_NEAR(place.scale_y * -304 + place.offset_y, -302.9069, 1e-3);
    EXPECT_NEAR(place.scale_y * 0 + place.offset_y, -1.0931, 1e-3);
}

TEST(TextOutlinePlace, AGlyphDrawnAtALaterSizeKeepsTheFirstSizesCell) {
    // Wallpaper Engine keeps one distance-atlas cell for a glyph of its primary font, made at the
    // first size the glyph is drawn at (text-outline-stroke: the 36-point rows, then 48 and 60).
    // DejaVu Sans' H at 150 pixels an em (box 14..99 by -109..0) is a 18 x 23 cell at scale 23 / 109;
    // at 250 pixels (box 24..164 by -182..0) it is drawn from that cell, on its own box
    const auto cell = owe::text::OutlinedGlyphCell(14, 99, -109, 0, 150);
    EXPECT_DOUBLE_EQ(cell.atlas_w, 18);
    EXPECT_DOUBLE_EQ(cell.atlas_h, 23);
    EXPECT_NEAR(cell.scale, 23.0 / 109, 1e-9);
    const auto place = owe::text::OutlinedGlyphPlace(24, 164, -182, 0, 250, cell);
    EXPECT_NEAR(place.scale_x * 24 + place.offset_x, 24.4796, 1e-3);
    EXPECT_NEAR(place.scale_x * 164 + place.offset_x, 162.6906, 1e-3);
    EXPECT_NEAR(place.scale_y * -182 + place.offset_y, -181.7414, 1e-3);
    EXPECT_NEAR(place.scale_y * 0 + place.offset_y, -0.5904, 1e-3);
    // a glyph's own cell is what the five-argument form uses
    const auto own   = owe::text::OutlinedGlyphCell(24, 164, -182, 0, 250);
    const auto same  = owe::text::OutlinedGlyphPlace(24, 164, -182, 0, 250, own);
    const auto plain = owe::text::OutlinedGlyphPlace(24, 164, -182, 0, 250);
    EXPECT_DOUBLE_EQ(same.scale_x, plain.scale_x);
    EXPECT_DOUBLE_EQ(same.offset_x, plain.offset_x);
    EXPECT_DOUBLE_EQ(same.scale_y, plain.scale_y);
    EXPECT_DOUBLE_EQ(same.offset_y, plain.offset_y);
}

TEST(TextAtlas, AnOutlinedGlyphsCellIsTheFirstOneStored) {
    auto source    = rstd::path::PathBuf::from(__FILE__ ""_str);
    auto root      = source.as_path().parent().unwrap().parent().unwrap().parent().unwrap();
    auto path      = rstd::path::PathBuf::from(root).join("fixtures/fonts/DejaVuSans.ttf"_str);
    auto bytes     = rstd::fs::read(path.as_path()).unwrap();
    auto resources = TextResources();
    auto font      = resources->RegisterFont(bytes.as_slice()).unwrap();
    const auto small = owe::text::OutlinedGlyphCell(14, 99, -109, 0, 150);
    const auto large = owe::text::OutlinedGlyphCell(24, 164, -182, 0, 250);
    EXPECT_NE(small.atlas_w, large.atlas_w);
    auto first = resources->SharedOutlinedCell(font, 7, small);
    EXPECT_DOUBLE_EQ(first.atlas_w, small.atlas_w);
    // the same glyph at another size gets the first cell; another glyph its own
    auto again = resources->SharedOutlinedCell(font, 7, large);
    EXPECT_DOUBLE_EQ(again.atlas_w, small.atlas_w);
    EXPECT_DOUBLE_EQ(again.ppem, 150);
    auto other = resources->SharedOutlinedCell(font, 8, large);
    EXPECT_DOUBLE_EQ(other.atlas_w, large.atlas_w);
}

TEST(TextLayouter, AnOutlinedLetterAtALaterSizeIsDrawnFromTheFirstSizesCell) {
    // text-outline-ctx: with a 16-point row before it, the 60-point H is 3 pixels narrower
    auto source = rstd::path::PathBuf::from(__FILE__ ""_str);
    auto root   = source.as_path().parent().unwrap().parent().unwrap().parent().unwrap();
    auto path   = rstd::path::PathBuf::from(root).join("fixtures/fonts/DejaVuSans.ttf"_str);
    auto bytes  = rstd::fs::read(path.as_path()).unwrap();
    auto width  = [&](Arc<owe::text::TextResources> resources, vectext::FontSource font,
                     double size) {
        auto mesh = Arc<owe::SceneMesh>::make();
        owe::text::TextLayoutStyle style;
        style.outline           = true;
        style.outline_thickness = 4.0f;
        owe::text::TextLayouter layouter(resources.clone(),
                                         { .source = Some(font), .size = size },
                                         mesh.clone(),
                                         rstd::move(style),
                                         8);
        EXPECT_TRUE(layouter.SetText("H"_str).is_ok());
        const auto&  vertices = mesh->GetVertexArray(usize());
        const auto   stride   = vertices.OneSize().to_primitive();
        const auto   first    = vertices.VertexCount().to_primitive() - 4; // the letter
        const float* data     = vertices.Data();
        return data[(first + 1) * stride] - data[first * stride];
    };
    auto       lone   = TextResources();
    const auto alone  = width(lone.clone(), lone->RegisterFont(bytes.as_slice()).unwrap(), 250);
    auto       shared = TextResources();
    auto       font   = shared->RegisterFont(bytes.as_slice()).unwrap();
    (void)width(shared.clone(), font, 150);
    const auto later = width(shared.clone(), font, 250);
    EXPECT_GT(alone - later, 1.5f);
    EXPECT_LT(alone - later, 5.0f);
}

TEST(TextAtlas, AnOutlinedGlyphIsRasterisedFromItsOutlineHintedAtTheCellsSize) {
    // text-outline-scaled: Wallpaper Engine's 28-point letters are drawn from the cell of the
    // 7-point ones (about 29 pixels an em). DejaVu Sans' "2" hinted at 29.17 pixels is 21 pixels
    // tall (21.66 unhinted) with a 3-pixel base bar (2.4 unhinted), and drawn at 116.67 pixels
    // from that cell it is 4 times that
    auto source    = rstd::path::PathBuf::from(__FILE__ ""_str);
    auto root      = source.as_path().parent().unwrap().parent().unwrap().parent().unwrap();
    auto path      = rstd::path::PathBuf::from(root).join("fixtures/fonts/DejaVuSans.ttf"_str);
    auto bytes     = rstd::fs::read(path.as_path()).unwrap();
    auto resources = TextResources();
    auto font      = resources->RegisterFont(bytes.as_slice()).unwrap();
    constexpr double kHint = 29.17, kSize = 116.67;
    auto             hinted = resources->RasterizeHinted(font, 21, kHint, kHint, 1, 0).unwrap();
    EXPECT_NEAR(hinted.ink_bottom - hinted.ink_top, 21.0, 0.1);
    EXPECT_NEAR(hinted.ink_right - hinted.ink_left, 13.42, 0.1);
    // y is down from the baseline: the glyph stands on it
    EXPECT_NEAR(hinted.origin.y + hinted.ink_top, -21.0, 0.1);
    EXPECT_NEAR(hinted.origin.x + hinted.ink_left, 2.125, 0.1);
    auto later = resources->RasterizeHinted(font, 21, kHint, kSize, 1, 0).unwrap();
    EXPECT_NEAR(later.ink_bottom - later.ink_top, 21.0 * kSize / kHint, 0.2);
    EXPECT_NEAR(later.ink_right - later.ink_left, 13.42 * kSize / kHint, 0.3);
    // a finer raster (a scaled layer) is the same shape in more pixels
    auto fine = resources->RasterizeHinted(font, 21, kHint, kSize, 2, 0).unwrap();
    EXPECT_NEAR((fine.ink_bottom - fine.ink_top) / 2, later.ink_bottom - later.ink_top, 0.3);
    // outlined: grown by the thickness to either side
    auto grown = resources->RasterizeHinted(font, 21, kHint, kHint, 1, 3).unwrap();
    EXPECT_NEAR(grown.height - hinted.height, 6, 1.5);
}

TEST(TextOutlinePlace, AGlyphTheAtlasHoldsExactlyStaysPut) {
    // 10 pixels at 32 an em is 10 atlas pixels: nothing is truncated
    const auto place = owe::text::OutlinedGlyphPlace(3, 13, -10, 0, 32);
    EXPECT_DOUBLE_EQ(place.scale_x, 1.0);
    EXPECT_NEAR(place.offset_x, 0.0, 1e-9);
    EXPECT_DOUBLE_EQ(place.scale_y, 1.0);
    EXPECT_NEAR(place.offset_y, 0.0, 1e-9);
}

TEST(TextLayouter, AnOutlinedLetterIsPlacedAsTheAtlasHasIt) {
    // the outlined letter (and its outline) move by the atlas's placement; the same letter without
    // an outline doesn't
    auto layout = [](bool outline) {
        auto                       mesh = Arc<owe::SceneMesh>::make();
        owe::text::TextLayoutStyle style;
        style.outline           = outline;
        style.outline_thickness = 4.0f;
        owe::text::TextLayouter layouter(
            TextResources(), TextFont(), mesh.clone(), rstd::move(style), 8);
        EXPECT_TRUE(layouter.SetText("H"_str).is_ok());
        const auto& vertices = mesh->GetVertexArray(usize());
        const auto  stride   = vertices.OneSize().to_primitive();
        // the letter is the last quad
        const auto   first = vertices.VertexCount().to_primitive() - 4;
        const float* data  = vertices.Data();
        return array<float, 2> { data[first * stride], data[(first + 1) * stride] };
    };
    const auto plain = layout(false), outlined = layout(true);
    // H at 64 pixels: its box's width truncates in the atlas, so the letter doesn't stay put
    auto moved = [](float a, float b) {
        return a > b ? a - b : b - a;
    };
    EXPECT_GT(rstd::cmp::max(moved(outlined[usize()], plain[usize()]),
                             moved(outlined[usize(1)], plain[usize(1)])),
              0.05f);
    // and is still about the letter's size
    EXPECT_NEAR(outlined[usize(1)] - outlined[usize()], plain[usize(1)] - plain[usize()], 2.0f);
}

TEST(TextAtlas, AHintedGlyphsBoxIsInWholePixels) {
    // Wallpaper Engine's distance atlas boxes a glyph by its hinted outline (text-outline-offset):
    // DejaVu Sans' H is 58 pixels wide at 25 points (104.17 pixels an em) and 116 at 50 points,
    // where its unhinted outline spans 10.22..68.11 and 20.45..136.21
    auto source = rstd::path::PathBuf::from(__FILE__ ""_str);
    auto root   = source.as_path().parent().unwrap().parent().unwrap().parent().unwrap();
    auto path   = rstd::path::PathBuf::from(root).join("fixtures/fonts/DejaVuSans.ttf"_str);
    auto bytes  = rstd::fs::read(path.as_path()).unwrap();
    auto resources = TextResources();
    auto font      = resources->RegisterFont(bytes.as_slice()).unwrap();
    auto layout    = resources->Layout({ .text = "H"_str, .size = 100, .source = Some(font) }).unwrap();
    ASSERT_EQ(layout.glyphs().len(), usize(1));
    const auto glyph = layout.glyphs()[usize()].glyph;
    const double sizes[2] { 25 * 300.0 / 72, 50 * 300.0 / 72 };
    const int    widths[2] { 58, 116 };
    for (int i = 0; i < 2; ++i) {
        auto box = resources->HintedGlyphBox(font, glyph, sizes[i]);
        ASSERT_TRUE(box.is_some());
        EXPECT_EQ((*box)[usize(2)] - (*box)[usize()], widths[i]);
        EXPECT_LT((*box)[usize(1)], (*box)[usize(3)]); // y down: top above bottom
    }
    // no box for a font it wasn't given
    EXPECT_TRUE(resources->HintedGlyphBox(vectext::FontSource { 99, 99 }, glyph, 100).is_none());
}

TEST(TextLayouter, AnOutlinedLetterOnAScaledLayerIsRasterisedAtItsScale) {
    // Wallpaper Engine's outlined text stays sharp on a scaled layer (text-oot-clock: 7 points on
    // a layer scaled 4): owe rasterises it `raster_scale` times finer, the quads keeping their size
    // in layer units
    auto layout = [](float raster_scale) {
        auto                       mesh = Arc<owe::SceneMesh>::make();
        owe::text::TextLayoutStyle style;
        style.outline           = true;
        style.outline_thickness = 2.0f;
        style.raster_scale      = raster_scale;
        owe::text::TextLayouter layouter(
            TextResources(), TextFont(), mesh.clone(), rstd::move(style), 8);
        EXPECT_TRUE(layouter.SetText("H"_str).is_ok());
        const auto&  vertices = mesh->GetVertexArray(usize());
        const auto   stride   = vertices.OneSize().to_primitive();
        const auto   uv       = stride / 3;
        const auto   first    = vertices.VertexCount().to_primitive() - 4; // the letter
        const float* data     = vertices.Data();
        return array<float, 2> { data[(first + 1) * stride] - data[first * stride],
                                 data[(first + 1) * stride + uv] - data[first * stride + uv] };
    };
    const auto fine = layout(4.0f), plain = layout(1.0f);
    EXPECT_NEAR(fine[usize()], plain[usize()], 1.0f);                   // the same width
    EXPECT_NEAR(fine[usize(1)] / plain[usize(1)], 4.0f, 0.5f);          // four times the pixels
}

