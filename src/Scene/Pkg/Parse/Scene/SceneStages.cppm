export module wescene.pkg.parse:scene_stages;
import rstd;
import wescene.json;
import wescene.fs;
import wescene.pkg.scene_obj;
import wescene.scene;

using rstd::collections::HashMap;

using namespace rstd::prelude;

export namespace owe
{

using SceneObjectVar = wpscene::SceneObject;

enum class SceneAnimationBindingIssue
{
    MissingRelation,
    NonReciprocalRelation,
    AmbiguousTarget,
    MetadataMismatch,
    Cycle,
    DuplicateName,
    UnsupportedTarget,
};

struct SceneAnimationBindingDiagnostic {
    SceneAnimationBindingIssue issue { SceneAnimationBindingIssue::MissingRelation };
    u64                        binding {};
    String                     field;
    String                     relation;
};

class SceneAnimationBindingScope {
public:
    void Clear() {
        m_tracks.clear();
        m_diagnostics.clear();
    }
    void Insert(u64 identity, SceneAnimationTrack track) {
        (void)m_tracks.insert(identity, rstd::move(track));
    }
    auto Resolve(const wpscene::FieldBindingSpec&) const -> Option<SceneAnimationTrack>;
    void Report(SceneAnimationBindingDiagnostic diagnostic) {
        m_diagnostics.push(rstd::move(diagnostic));
    }
    auto Diagnostics() const -> slice<SceneAnimationBindingDiagnostic> {
        return m_diagnostics.as_slice();
    }

private:
    HashMap<u64, SceneAnimationTrack>    m_tracks;
    Vec<SceneAnimationBindingDiagnostic> m_diagnostics;
};

auto BuildAnimationBindingScope(const wpscene::FieldBindings&) -> SceneAnimationBindingScope;
auto BuildAnimationBindingScope(const wpscene::ImageObject&) -> SceneAnimationBindingScope;

enum class TextRenderMode
{
    Direct,
    Offscreen,
};

struct TextSurfaceRequirements {
    bool color_blend { false };
    bool has_effect { false };
    bool copy_background { false };
    bool opaque_background { false };
    bool linked_source { false };
};

constexpr auto ResolveTextRenderMode(TextSurfaceRequirements requirements) -> TextRenderMode {
    return requirements.color_blend || requirements.has_effect || requirements.copy_background ||
                   requirements.opaque_background || requirements.linked_source
               ? TextRenderMode::Offscreen
               : TextRenderMode::Direct;
}

// How a layer with effects reaches the scene. Wallpaper Engine draws the last pass of the last
// effect straight onto the scene, at the screen's resolution, whatever its shader (probes
// effect-repeat-scaled and effect-pass-resolution: a 32 x 32 layer drawn 288 wide shows every texel
// of a one-pass effect, while an effect with another after it runs at the layer's 32 x 32). A
// passthrough draws the layer's effect target instead only when none of its effects loaded and
// nothing else draws the layer.
struct EffectFinalDraw {
    bool linked_source { false };
    bool fullscreen { false };
    bool copybackground { false };
    // the layer's source draws itself on the scene (a passthrough layer, or one without a source
    // draw)
    bool source_composites_final { false };
    bool last_effect_loaded { false };
};

constexpr auto NeedsEffectFinalPassthrough(EffectFinalDraw draw) -> bool {
    return ! draw.linked_source && ! draw.fullscreen && ! draw.copybackground &&
           ! draw.source_composites_final && ! draw.last_effect_loaded;
}

// Whether an effect's pass draws anything (effect-pass-resolution): Wallpaper Engine draws the
// passes of an effect into one buffer, so a pass before the last with no target of its own is
// overwritten by the last, which reads the effect's input.
struct EffectPassDraw {
    rstd::size_t index { 0 }, count { 1 };
    bool         has_target { false };
};

constexpr auto EffectPassIsDrawn(EffectPassDraw pass) -> bool {
    return pass.has_target || pass.index + 1 >= pass.count;
}

// Compatibility entry for callers with an already-parsed raw value.
Vec<SceneObjectVar> ExpandObjects(const Json&, fs::VFS&, wpscene::SceneVersion,
                                  Option<ref<rstd::json::Map>> user_props = None());

// Canonical cheap expansion path. SceneDocument owns authored object order
// and schema classification; no Scene / glslang state is constructed.
Vec<SceneObjectVar> ExpandObjects(ref<wpscene::SceneDocument>, mut_ref<fs::VFS>,
                                  Option<ref<rstd::json::Map>> user_props = None());

// Resolves the effective width/height without mutating the parsed metadata.
array<i32, 2> ResolveOrthoProjectionExtent(const wpscene::SceneMetadata&, slice<SceneObjectVar>);

} // namespace owe
