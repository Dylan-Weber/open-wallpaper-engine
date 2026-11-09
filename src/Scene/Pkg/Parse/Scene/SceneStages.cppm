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

// Whether a pass of an effect leaves anything another pass or the layer sees. In Wallpaper Engine a
// pass's default input ("previous") is the effect's input, not the pass before it
// (effect-pass-chain: a repeat pass then a copy pass, with no targets or binds, shows the plain
// texture; with the repeat "compose": true, which the effect parser turns into a target and a bind,
// the copy shows the repeat). So a pass that is not the effect's last and writes no target of its
// own is dropped. Effects with copy commands keep every pass: their commands are placed by pass
// position.
struct EffectPassUse {
    bool last { false };
    bool has_target { false };
    bool effect_has_commands { false };
};

constexpr auto EffectPassIsSeen(EffectPassUse use) -> bool {
    return use.last || use.has_target || use.effect_has_commands;
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
