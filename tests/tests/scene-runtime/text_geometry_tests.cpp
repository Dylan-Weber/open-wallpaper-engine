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
auto WidestLine(const vectext::TextLayout& layout) -> float {
    float widest {};
    for (auto& line : layout.lines()) widest = rstd::cmp::max(widest, float(line.logical.width));
    return widest;
}
// Wallpaper Engine's width of laid-out text: from the pen, or from ink that hangs left of it, to
// the right edge of the ink (not of the advances).
auto InkWidth(const vectext::TextLayout& layout) -> float {
    float left {}, right {};
    for (auto& line : layout.lines()) {
        if (line.ink.width == 0) continue;
        left  = rstd::cmp::min(left, float(line.ink.x - line.logical.x));
        right = rstd::cmp::max(right, float(line.ink.x + line.ink.width - line.logical.x));
    }
    return right - left;
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
        EXPECT_FLOAT_EQ(layouter.TextWidth(), InkWidth(layout));
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
    EXPECT_FLOAT_EQ(layouter.TextWidth(), InkWidth(*layouter.Layout()));
}

// A layer that limits its width is only as wide as its widest wrapped line, and so is its
// background: Wallpaper Engine draws text-layout's "wrapped" box 444 units wide, not maxwidth 450 +
// padding = 490. Its lines keep their alignment inside that box.
TEST(TextLayouter, WrappedTextIsAsWideAsItsWidestLine) {
    auto                  resources  = TextResources();
    constexpr std::size_t peak_quads = 16;
    const rstd::initializer_list<owe::SceneVertexArray::SceneVertexAttribute> attributes {
        { .name = "a_Position"_Str, .type = owe::VertexType::FLOAT3 },
        { .name = "a_TexCoord"_Str, .type = owe::VertexType::FLOAT2 },
        { .name = "a_Color"_Str, .type = owe::VertexType::FLOAT4 },
    };
    for (auto align : array<ref<str>, 3> { "left"_str, "center"_str, "right"_str }) {
        auto mesh = Arc<owe::SceneMesh>::make();
        mesh->AddVertexArray(owe::SceneVertexArray(attributes, rstd::usize(peak_quads * 4)));
        mesh->AddIndexArray(owe::SceneIndexArray(rstd::usize(peak_quads * 6)));
        owe::text::TextLayoutStyle style;
        style.halign           = rstd::into(align);
        style.wrap_width       = 400;
        style.padding          = 10;
        style.opaquebackground = true;
        owe::text::TextLayouter layouter(
            resources.clone(), TextFont(), mesh.clone(), rstd::move(style), peak_quads);
        ASSERT_TRUE(layouter.SetText("AAA AAAAA A"_str).is_ok());
        const auto& layout = *layouter.Layout();
        ASSERT_EQ(layout.lines().len(), usize(2));
        // (its ink: the letters' bare sides are not counted, see LinesAreMeasuredByTheirInk)
        const float widest = layouter.TextWidth();
        ASSERT_LT(widest, 400.0f);
        EXPECT_LE(widest, WidestLine(layout));
        // (a little less than the layout's ink: the pen moves by whole pixels, see
        // LettersAdvanceByWholePixels)
        EXPECT_LE(widest, InkWidth(layout));
        EXPECT_GT(widest, InkWidth(layout) - 9.0f);

        // the background quad comes first: the box and its padding
        const auto& vertices = mesh->GetVertexArray(rstd::usize());
        const auto  stride   = vertices.OneSize().to_primitive();
        const auto  x        = [&](std::size_t quad, std::size_t corner) {
            return vertices.Data()[(quad * 4 + corner) * stride];
        };
        EXPECT_FLOAT_EQ(x(0, 0), -widest / 2 - 10);
        EXPECT_FLOAT_EQ(x(0, 1), widest / 2 + 10);
        const auto quads = mesh->GetIndexArray(rstd::usize()).RenderDataCount().to_primitive() / 6;
        float      left = x(1, 0), right = x(1, 1);
        for (std::size_t q = 1; q < quads; ++q) {
            left  = rstd::cmp::min(left, x(q, 0));
            right = rstd::cmp::max(right, x(q, 1));
        }
        // letters' ink sits a few pixels inside their advances
        if (align == "left"_str) EXPECT_NEAR(left, -widest / 2, 10.0f);
        if (align == "right"_str) EXPECT_NEAR(right, widest / 2, 10.0f);
        if (align == "center"_str) EXPECT_NEAR(left + right, 0.0f, 2.0f);
        EXPECT_GE(left, -widest / 2 - 1);
        EXPECT_LE(right, widest / 2 + 1);
    }
}

// Wallpaper Engine places text by its ink, not its advances (gnome-extension test/wallpapers
// text-bearings*, measured against Arial's metrics):
// - a line spans from its pen, or from ink that hangs left of it, to its ink's right edge: a right
//   aligned L ends at its ink, an f that reaches past its advance is not cut;
// - lines align against the widest of those spans;
// - ink that hangs left of the pen (j) moves every line right by that much;
// - the text, and its background box, is as wide as the widest line's ink plus that hang.
TEST(TextLayouter, LinesAreMeasuredByTheirInk) {
    auto source      = rstd::path::PathBuf::from(__FILE__ ""_str);
    auto root        = source.as_path().parent().unwrap().parent().unwrap().parent().unwrap();
    auto path        = rstd::path::PathBuf::from(root).join("fixtures/fonts/Bearings.ttf"_str);
    auto bytes       = rstd::fs::read(path.as_path()).unwrap();
    auto resources   = TextResources();
    auto font_source = resources->RegisterFont(bytes.as_slice()).unwrap();
    constexpr std::size_t                                                     peak_quads = 8;
    const rstd::initializer_list<owe::SceneVertexArray::SceneVertexAttribute> attributes {
        { .name = "a_Position"_Str, .type = owe::VertexType::FLOAT3 },
        { .name = "a_TexCoord"_Str, .type = owe::VertexType::FLOAT2 },
        { .name = "a_Color"_Str, .type = owe::VertexType::FLOAT4 },
    };
    struct Case {
        const char*        text;
        const char*        align;
        float              width;
        std::vector<float> ink; // left and right of each glyph's quad
    };
    // Bearings.ttf at 100 pixels: L ink 10..50 of 60, o 5..45 of 50, j -10..20 of 30,
    // f 5..45 of 40
    const std::vector<Case> cases {
        { "L", "left", 50, { -15, 25 } },
        { "L", "right", 50, { -15, 25 } },
        { "j", "left", 30, { -15, 15 } },
        { "j", "center", 30, { -15, 15 } },
        { "f", "right", 45, { -17.5f, 22.5f } },
        // lines Lo (10..105), j (-10..20) and f (5..45): 105 + the j's 10, and every pen at 10
        { "Lo\nj\nf", "left", 115, { -37.5f, 2.5f, 17.5f, 57.5f, -57.5f, -27.5f, -42.5f, -2.5f } },
        { "Lo\nj\nf", "center", 115, { -37.5f, 2.5f, 17.5f, 57.5f, -20, 10, -12.5f, 27.5f } },
        { "Lo\nj\nf", "right", 115, { -37.5f, 2.5f, 17.5f, 57.5f, 17.5f, 47.5f, 17.5f, 57.5f } },
    };
    for (const auto& c : cases) {
        auto mesh = Arc<owe::SceneMesh>::make();
        mesh->AddVertexArray(owe::SceneVertexArray(attributes, rstd::usize(peak_quads * 4)));
        mesh->AddIndexArray(owe::SceneIndexArray(rstd::usize(peak_quads * 6)));
        owe::text::TextLayoutStyle style;
        style.mesh_origin      = owe::text::TextMeshOrigin::Layout;
        style.halign           = rstd::into(rstd::cppstd::as_str(c.align).unwrap());
        style.padding          = 10;
        style.opaquebackground = true;
        owe::text::TextLayouter layouter(resources.clone(),
                                         { .source = Some(font_source), .size = 100 },
                                         mesh.clone(),
                                         rstd::move(style),
                                         peak_quads);
        ASSERT_TRUE(layouter.SetText(rstd::cppstd::as_str(c.text).unwrap()).is_ok());
        EXPECT_NEAR(layouter.TextWidth(), c.width, 0.01f) << c.text << " " << c.align;
        const auto& vertices = mesh->GetVertexArray(rstd::usize());
        const auto  stride   = vertices.OneSize().to_primitive();
        const auto  x        = [&](std::size_t quad, std::size_t corner) {
            return vertices.Data()[(quad * 4 + corner) * stride];
        };
        EXPECT_NEAR(x(0, 0), -c.width / 2 - 10, 0.01f) << c.text << " " << c.align;
        EXPECT_NEAR(x(0, 1), c.width / 2 + 10, 0.01f) << c.text << " " << c.align;
        ASSERT_EQ(mesh->GetIndexArray(rstd::usize()).RenderDataCount().to_primitive(),
                  (1 + c.ink.size() / 2) * 6)
            << c.text;
        for (std::size_t g = 0; g < c.ink.size() / 2; ++g) {
            // the rasterizer may pad a quad by a pixel
            EXPECT_NEAR(x(g + 1, 0), c.ink[g * 2], 1.01f) << c.text << " " << c.align << " " << g;
            EXPECT_NEAR(x(g + 1, 1), c.ink[g * 2 + 1], 1.01f)
                << c.text << " " << c.align << " " << g;
        }
    }
}

// Wallpaper Engine moves the pen by whole pixels: each letter's advance rounded down at the text's
// size (user-properties' "from a property" at 64 points: its letters' places fit floored Arial
// advances to 0.3 pixels, and owe's whole advances drifted 5 pixels wider along the line).
TEST(TextLayouter, LettersAdvanceByWholePixels) {
    auto source      = rstd::path::PathBuf::from(__FILE__ ""_str);
    auto root        = source.as_path().parent().unwrap().parent().unwrap().parent().unwrap();
    auto path        = rstd::path::PathBuf::from(root).join("fixtures/fonts/Bearings.ttf"_str);
    auto bytes       = rstd::fs::read(path.as_path()).unwrap();
    auto resources   = TextResources();
    auto font_source = resources->RegisterFont(bytes.as_slice()).unwrap();
    constexpr std::size_t                                                     peak_quads = 8;
    const rstd::initializer_list<owe::SceneVertexArray::SceneVertexAttribute> attributes {
        { .name = "a_Position"_Str, .type = owe::VertexType::FLOAT3 },
        { .name = "a_TexCoord"_Str, .type = owe::VertexType::FLOAT2 },
        { .name = "a_Color"_Str, .type = owe::VertexType::FLOAT4 },
    };
    auto mesh = Arc<owe::SceneMesh>::make();
    mesh->AddVertexArray(owe::SceneVertexArray(attributes, rstd::usize(peak_quads * 4)));
    mesh->AddIndexArray(owe::SceneIndexArray(rstd::usize(peak_quads * 6)));
    owe::text::TextLayoutStyle style;
    style.mesh_origin = owe::text::TextMeshOrigin::Layout;
    style.halign      = "left"_Str;
    // at 101 pixels L advances 60.6 and o 50.5: the pens are 0, 60, 110, 170, 220, 280, 330
    owe::text::TextLayouter layouter(resources.clone(),
                                     { .source = Some(font_source), .size = 101 },
                                     mesh.clone(),
                                     rstd::move(style),
                                     peak_quads);
    ASSERT_TRUE(layouter.SetText("LoLoLoL"_str).is_ok());
    // from the first L's pen to the last one's ink: 330 + 50.5 (not 333.3 + 50.5)
    EXPECT_NEAR(layouter.TextWidth(), 380.5f, 0.01f);
    const auto& vertices = mesh->GetVertexArray(rstd::usize());
    const auto  stride   = vertices.OneSize().to_primitive();
    const float pens[7] { 0, 60, 110, 170, 220, 280, 330 };
    for (std::size_t glyph = 0; glyph < 7; ++glyph) {
        const float ink = glyph % 2 == 0 ? 10.1f : 5.05f;
        EXPECT_NEAR(vertices.Data()[glyph * 4 * stride], pens[glyph] + ink - 380.5f / 2, 1.01f)
            << glyph;
    }
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
