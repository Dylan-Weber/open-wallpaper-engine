module;

export module wescene.text;
import rstd;
import eigen;
export import vectext;
import wescene.types;
import wescene.scene;

using namespace rstd::prelude;
using rstd::sync::Arc;

export namespace owe::text
{

struct AtlasDirtyRect {
    rstd::uint32_t x {}, y {}, w {}, h {};
};
struct AtlasUpdateRect {
    AtlasDirtyRect rect;
    // Owned R8 rows, tightly packed with row pitch rect.w.
    vrento::texture::TextureUploadPayload bytes;
};
struct AtlasUpdateBatch {
    rstd::uint64_t       serial {};
    Vec<AtlasUpdateRect> rects;
};
struct AtlasOptions {
    rstd::uint32_t extent { 2048 }, max_pages { 16 };
};
struct AtlasGlyph {
    rstd::uint32_t page {}, x {}, y {}, width {}, height {};
    vectext::Point origin;
    double         scale { 1 };
};
class AtlasPage {
    struct Impl;
    Box<Impl> m_impl;
    friend class TextResources;

public:
    AtlasPage(rstd::uint32_t extent, String url, bool white_cell = false);
    ~AtlasPage();
    auto Extent() const -> rstd::uint32_t;
    auto AtlasUrl() const -> ref<str>;
    auto AtlasPixels() const -> slice<rstd::uint8_t>;
    auto RetainAtlasPixels() const -> owe::ImageDataPtr;
    auto SnapshotDirtyUpdates() const -> AtlasUpdateBatch;
    // Confirm this page's batch only on submission success, never on NotResident or failure.
    void AcknowledgeUpdates(rstd::uint64_t serial);
    auto DirtyRects() const -> slice<AtlasDirtyRect>;
    // Only for synchronous callers without an outstanding snapshot.
    void ClearDirtyRects();
};
// Letter-pair kerning read from a font file: the pair adjustments of its GPOS 'kern' feature, or
// its legacy 'kern' table when it has no such feature, in font units.
class FontKerning {
    Vec<unsigned char>    m_bytes;
    Vec<rstd::uint32_t>   m_subtables;   // GPOS pair adjustment subtables, from the file's start
    Vec<rstd::uint32_t>   m_lookup_ends; // where each kern lookup's subtables end in m_subtables
    Vec<rstd::uint32_t>   m_kern_tables; // legacy format 0 subtables, from the file's start
    rstd::uint32_t        m_units_per_em { 1000 };

public:
    static auto Read(slice<u8> font) -> FontKerning;
    auto        Empty() const -> bool;
    auto        UnitsPerEm() const -> rstd::uint32_t { return m_units_per_em; }
    auto        Pair(rstd::uint32_t left, rstd::uint32_t right) const -> rstd::int32_t;
};
class TextResources {
    struct Impl;
    Box<Impl> m_impl;

public:
    explicit TextResources(AtlasOptions options = {});
    TextResources(vectext::FontCollection collection, AtlasOptions options = {});
    ~TextResources();
    auto RegisterFont(slice<u8> bytes) -> Result<vectext::FontSource, vectext::TextError>;
    // The kerning of a font RegisterFont took, if it has any.
    auto Kerning(const vectext::FontSource& source) const -> const FontKerning*;
    auto Layout(vectext::TextRequest request) -> Result<vectext::TextLayout, vectext::TextError>;
    // `outline` > 0: the glyph's outline stroked that many pixels to either side with mitred corners, filled, for a
    // text outline drawn behind it
    auto Rasterize(const vectext::FontInstance& font, vectext::Index glyph, double scale = 1,
                   double outline = 0)
        -> Result<AtlasGlyph, vectext::TextError>;
    auto Pages() const -> slice<Arc<AtlasPage>>;
    auto RasterCount() const -> rstd::uint64_t;
    auto LayoutCount() const -> rstd::uint64_t;
    auto RegisterPages(Scene& scene) -> bool;
};
struct TextFont {
    Option<vectext::FontSource> source;
    String                      family;
    double                      size { 16 };
};
auto EnsureSceneTextResources(Scene& scene) -> Arc<TextResources>;
auto SceneTextResources(Scene& scene) -> TextResources*;
auto BuildAtlasImage(const AtlasPage& page, ref<str> key) -> Option<Arc<owe::Image>>;

// Lazily compiles the embedded text HLSL shader (one-time, process-wide
// cached) and returns a ready-to-bind SceneShader. The shader expects:
//   - vertex inputs: a_Position (float3), a_TexCoord (float2),
//                    a_Color (float4)
//   - uniform block ww_Uniforms with member g_ModelViewProjectionMatrix
//   - combined image sampler g_Texture0 (R8 atlas; .r = coverage)
// Returns nullptr if the SPIR-V compile fails.
Option<Arc<owe::SceneShader>> GetTextSceneShader();

// Mirrors WE's text-effect background seed draw: sample the current scene into
// the text RT and keep alpha at zero.
Option<Arc<owe::SceneShader>> GetTextCopyBackgroundSceneShader();

// From clip space to the scene texture's coordinates (before the divide by w): the top of the
// screen (NDC y = 1) is the texture's top row (v = 0). The background seed samples the scene
// through it.
auto NdcToTexture() -> Eigen::Matrix4d;

enum class TextUniformOutput : rstd::uint32_t
{
    ModelViewProjection,
    EffectModelViewProjection,
};

struct TextEffectProjectionState {
    Arc<SceneNode>  node;
    array<float, 2> size { 0.0f, 0.0f };
};

struct TextUniformState {
    Arc<SceneNode>                         node;
    Option<Arc<SceneCamera>>               camera;
    Option<Arc<SceneCamera>>               active_camera;
    Option<Arc<TextEffectProjectionState>> effect_projection;

    explicit TextUniformState(Arc<SceneNode> value): node(rstd::move(value)) {}
};

class TextUniformSource {
public:
    explicit TextUniformSource(Arc<TextUniformState> state): m_state(rstd::move(state)) {}

    auto Describe(mut_ref<dyn<UniformBindingSink>>) const -> Result<empty, UniformError>;
    auto Version(ref<dyn<UniformUpdateContext>>) const -> u64;
    auto Evaluate(ref<dyn<UniformUpdateContext>>, mut_ref<dyn<UniformValueSink>>) const
        -> Result<empty, UniformError>;
    auto AcquireBindingLease() const -> Option<Box<dyn<UniformBindingLease>>> { return None(); }

private:
    Arc<TextUniformState> m_state;
};

enum class TextOpacityOutput : rstd::uint32_t
{
    Opacity,
};

class TextOpacityUniformSource {
public:
    explicit TextOpacityUniformSource(Option<Arc<SceneNode>> node): m_node(rstd::move(node)) {}

    auto Describe(mut_ref<dyn<UniformBindingSink>>) const -> Result<empty, UniformError>;
    auto Version(ref<dyn<UniformUpdateContext>>) const -> u64;
    auto Evaluate(ref<dyn<UniformUpdateContext>>, mut_ref<dyn<UniformValueSink>>) const
        -> Result<empty, UniformError>;
    auto AcquireBindingLease() const -> Option<Box<dyn<UniformBindingLease>>> { return None(); }

private:
    Option<Arc<SceneNode>> m_node;
};

enum class TextMeshOrigin
{
    Layout,
    InkBounds,
};

struct TextLayoutStyle {
    TextMeshOrigin  mesh_origin { TextMeshOrigin::InkBounds };
    array<float, 3> color { 1.0f, 1.0f, 1.0f };
    float           alpha { 1.0f };
    float           brightness { 1.0f };

    bool            opaquebackground { false };
    array<float, 3> background_color { 0.0f, 0.0f, 0.0f };
    float           background_brightness { 1.0f };

    // `outline`: the letters grown by `outline_thickness` (layer units), drawn behind them in `outline_color`
    bool            outline { false };
    array<float, 3> outline_color { 0.0f, 0.0f, 0.0f };
    float           outline_thickness { 0.0f };

    String halign; // "left" / "right" / contains-substring; default = center
    float  padding { 0.0f };
    float  padding_y { -1.0f }; // above and below; below 0: as padding

    // Text-flow limits from the layer (`maxwidth` / `maxrows`, gated by
    // `limitwidth` / `limitrows`). `wrap_width` is in the same pixel space
    // as the glyph advances; 0 means no wrapping, 0 rows means no limit.
    float          wrap_width { 0.0f };
    rstd::uint32_t max_rows { 0 };
    bool           row_limit_ellipsis { false };
};

struct TextLayoutMetrics {
    float text_width { 0.0f };
    float text_height { 0.0f };
    float source_width { 0.0f };
    float source_height { 0.0f };
    float source_center_x { 0.0f };
    float source_center_y { 0.0f };
    float padding { 0.0f };
    float padding_y { -1.0f }; // above and below; below 0: as padding
    // How much further below the text than above it a background box reaches: the last line's
    // line gap (0 without a background).
    float background_gap { 0.0f };
    // How far a background box reaches above the line to take in the ink that rises past it (0
    // without a background).
    float background_overflow { 0.0f };
    // the last line's descent below its baseline
    float descent { 0.0f };
};

// How far text's box moves from the layer's origin for its alignment: left, right, top and bottom
// put that edge of the box on the origin; centred text sits with its baseline half the font's
// ascent below the origin, its box half the descent below it.
auto TextAnchorOffset(ref<str> horizontal, ref<str> vertical, float width, float height,
                      float descent) -> array<float, 2>;
// The pixel size text is laid out at: Wallpaper Engine's em is 4 scene units a point and its
// letters are drawn 25/24 of that (measured: an H of 20 to 200 points is 4.2% taller than at 4
// units a point), not rounded to a whole pixel; 1 to 1024.
auto PointsToPixels(double points) -> double;

struct TextGeometryPolicy {
    float frame_width { 1.0f };
    float frame_height { 1.0f };
    bool  dynamic { false };
    bool  has_effect { false };
    bool  frame_bound { false };
    bool  preserve_text_bbox { false };
};

struct TextGeometry {
    float rt_width { 1.0f };
    float rt_height { 1.0f };
    float draw_width { 1.0f };
    float draw_height { 1.0f };
    float draw_offset_x { 0.0f };
    float draw_offset_y { 0.0f };
    float uv_source_width { 1.0f };
    float uv_source_height { 1.0f };
    float effect_frame_width { 1.0f };
    float effect_frame_height { 1.0f };
};

TextGeometry ResolveTextGeometry(const TextGeometryPolicy& policy,
                                 const TextLayoutMetrics&  metrics);

class TextLayouter {
public:
    TextLayouter(Arc<TextResources> resources, TextFont font, Arc<SceneMesh> mesh,
                 TextLayoutStyle style, rstd::size_t peak_quads, Scene* scene = nullptr);
    ~TextLayouter();
    TextLayouter(const TextLayouter&)            = delete;
    TextLayouter& operator=(const TextLayouter&) = delete;

    auto SetText(ref<str> utf8) -> Result<empty, vectext::TextError>;
    auto SetPixelSize(double size) -> Result<empty, vectext::TextError>;
    auto SetHorizontalAlign(ref<str> align) -> Result<empty, vectext::TextError>;
    auto Layout() const -> const Option<vectext::TextLayout>&;

    // For ParseTextObj's initial-bbox log; reflects the most recent layout.
    float             TextWidth() const noexcept;
    float             TextHeight() const noexcept;
    float             SourceWidth() const noexcept;
    float             SourceHeight() const noexcept;
    owe::SceneMesh&   Mesh() const noexcept;
    TextLayoutMetrics Metrics() const noexcept;

private:
    struct Impl;
    Box<Impl> m_impl;
};

} // namespace owe::text
