module;
#include <time.h>

module wescene.script;
import eigen;
import rstd;
import wescene.scene;
import wescene.types;

using namespace rstd::prelude;
using rstd::path::PathBuf;
using rstd::sync::Arc;

namespace owe::script
{

namespace
{
constexpr double kRadToDeg = 180.0 / f64::consts::PI.to_primitive();
constexpr double kDegToRad = f64::consts::PI.to_primitive() / 180.0;
} // namespace

struct ScriptScene::Impl {
    JsRuntime     rt;
    Vec<Actuator> actuators;
    bool          async_pending { false };
    Vec<owe::SceneAnimationEventDispatch> async_events;
};

ScriptScene::ScriptScene(Option<Arc<AudioResponseDemand>> demand): m_impl(Box<Impl>::make()) {
    m_impl->rt.SetAudioResponseDemand(rstd::move(demand));
}
ScriptScene::~ScriptScene() = default;

JsRuntime& ScriptScene::runtime() noexcept { return m_impl->rt; }
void       ScriptScene::AddActuator(Actuator a) { m_impl->actuators.push(rstd::move(a)); }
bool       ScriptScene::DiscardDeferredFieldScript(FieldScript& script) {
    if (! m_impl->rt.OwnsDeferredFieldScript(script)) return false;
    for (auto& actuator : m_impl->actuators)
        if (actuator.script == &script) actuator.script = nullptr;
    return m_impl->rt.DiscardDeferredFieldScript(script);
}
// Empty = no scripts AND no actuators. Visibility-bound side-effect-only
// scripts (audio bar fanout) don't register an actuator but still need
// their TickAll to run, so emptiness must also consult the runtime.
bool ScriptScene::empty() const noexcept {
    if (! m_impl->actuators.is_empty()) return false;
    return m_impl->rt.Empty();
}

ScriptApply MakeNodeTransformApply(Arc<owe::SceneNode> node, NodeTransformTarget target) {
    return ScriptApply::make([node = rstd::move(node), target](const ScriptValue& v) {
        if (v.is_Empty()) return;

        // Script angle values are degrees; node rotation is radians. Read the
        // current rotation back as degrees so partial (Vec2 / scalar) updates
        // compose in the same unit the script works in.
        Eigen::Vector3f current = [&] {
            switch (target) {
            case NodeTransformTarget::Translate: return node->Translate();
            case NodeTransformTarget::Scale: return node->Scale();
            case NodeTransformTarget::Rotation:
                return Eigen::Vector3f { node->Rotation() * float(kRadToDeg) };
            }
            return Eigen::Vector3f { 0.0f, 0.0f, 0.0f };
        }();

        Eigen::Vector3f next = current;
        if (auto* p = (v.is_Vec3() ? &v.as_Vec3().value : nullptr)) {
            next = Eigen::Vector3f { static_cast<float>(p->x),
                                     static_cast<float>(p->y),
                                     static_cast<float>(p->z) };
        } else if (auto* p = (v.is_Vec2() ? &v.as_Vec2().value : nullptr)) {
            next =
                Eigen::Vector3f { static_cast<float>(p->x), static_cast<float>(p->y), current.z() };
        } else if (auto* p = (v.is_Scalar() ? &v.as_Scalar().value : nullptr)) {
            // Scalar splats across all three axes for scale; falls back to
            // current.x for translate/rotation (rare but seen in the corpus
            // when scripts mistakenly bind to the wrong field kind).
            if (target == NodeTransformTarget::Scale) {
                float s = static_cast<float>(p->v);
                next    = Eigen::Vector3f { s, s, s };
            } else {
                next.x() = static_cast<float>(p->v);
            }
        } else {
            return;
        }

        switch (target) {
        case NodeTransformTarget::Translate: node->SetTranslate(next); break;
        case NodeTransformTarget::Scale: node->SetScale(next); break;
        case NodeTransformTarget::Rotation: node->SetRotation(next * float(kDegToRad)); break;
        }
    });
}

ScriptApply MakeNodeAlphaApply(Arc<owe::SceneNode> node) {
    return ScriptApply::make([node = rstd::move(node)](const ScriptValue& v) {
        if (v.is_Empty()) return;

        if (auto* p = (v.is_Scalar() ? &v.as_Scalar().value : nullptr)) {
            node->SetUserAlpha(static_cast<float>(p->v));
        } else if (auto* p = (v.is_Bool() ? &v.as_Bool().value : nullptr)) {
            node->SetUserAlpha(p->v ? 1.0f : 0.0f);
        } else if (auto* p = (v.is_Vec2() ? &v.as_Vec2().value : nullptr)) {
            node->SetUserAlpha(static_cast<float>(p->x));
        } else if (auto* p = (v.is_Vec3() ? &v.as_Vec3().value : nullptr)) {
            node->SetUserAlpha(static_cast<float>(p->x));
        }
    });
}

ScriptApply MakeNodeVolumeApply(Arc<owe::SceneNode> node) {
    return ScriptApply::make([node = rstd::move(node)](const ScriptValue& value) {
        if (auto* volume = (value.is_Scalar() ? &value.as_Scalar().value : nullptr)) {
            node->SetVolume(static_cast<float>(volume->v));
        } else if (auto* volume = (value.is_Bool() ? &value.as_Bool().value : nullptr)) {
            node->SetVolume(volume->v ? 1.0f : 0.0f);
        } else if (auto* volume = (value.is_Vec2() ? &value.as_Vec2().value : nullptr)) {
            node->SetVolume(static_cast<float>(volume->x));
        } else if (auto* volume = (value.is_Vec3() ? &value.as_Vec3().value : nullptr)) {
            node->SetVolume(static_cast<float>(volume->x));
        }
    });
}

ScriptApply MakeNodeColorApply(Arc<owe::SceneNode> node) {
    return ScriptApply::make([node = rstd::move(node)](const ScriptValue& value) {
        auto& current = node->Color();
        if (auto* color = (value.is_Vec3() ? &value.as_Vec3().value : nullptr)) {
            node->SetColor({ static_cast<float>(color->x),
                             static_cast<float>(color->y),
                             static_cast<float>(color->z) });
        } else if (auto* color = (value.is_Vec2() ? &value.as_Vec2().value : nullptr)) {
            node->SetColor(
                { static_cast<float>(color->x), static_cast<float>(color->y), current.z() });
        } else if (auto* color = (value.is_Scalar() ? &value.as_Scalar().value : nullptr)) {
            const auto component = static_cast<float>(color->v);
            node->SetColor({ component, component, component });
        }
    });
}

void ScriptScene::Tick(const FrameInputs&                      fi,
                       slice<owe::SceneAnimationEventDispatch> animation_events) {
    m_impl->rt.SetFrameInputs(fi);
    m_impl->rt.TickAll(animation_events);
    ApplyActuators();
}

void ScriptScene::BeginTick(const FrameInputs& fi, Vec<owe::SceneAnimationEventDispatch> animation_events) {
    if (m_impl->async_pending) return;
    m_impl->async_events = rstd::move(animation_events);
    m_impl->rt.SetFrameInputs(fi);
    // The caller (the render thread) keeps the gate until it has finished with the scene for this frame.
    SceneGate::AcquireRender();
    m_impl->rt.TickAllAsync(m_impl->async_events.as_slice());
    m_impl->async_pending = true;
}

void ScriptScene::FinishTick() {
    if (! m_impl->async_pending) return;
    SceneGate::ReleaseAll();
    m_impl->rt.WaitTick();
    m_impl->async_pending = false;
    ApplyActuators();
}

bool ScriptScene::TickPending() const noexcept { return m_impl->async_pending; }

void ScriptScene::ApplyActuators() {
    const auto actuator_count = m_impl->actuators.len();
    for (usize index {}; index < actuator_count; ++index) {
        auto* script = m_impl->actuators[index].script;
        if (! script || ! script->alive() || script->initialization_deferred()) continue;
        auto apply = m_impl->actuators[index].apply.deref_mut();
        apply->operator()(script->last_value());
    }
    m_impl->actuators.retain([](const Actuator& actuator) {
        return actuator.script && actuator.script->alive();
    });
    m_impl->rt.PruneInactiveFieldScripts();
}

void InstallScriptScene(owe::Scene& scene, Box<ScriptScene> script_scene) {
    scene.InstallExtension(rstd::move(script_scene));
}

// the local time of day (0..1) at `epoch_ms`
float LocalTimeOfDay(double epoch_ms) {
    const auto seconds = static_cast<time_t>(epoch_ms / 1000.0);
    tm         local {};
    localtime_r(&seconds, &local);
    const double into_day = local.tm_hour * 3600.0 + local.tm_min * 60.0 + local.tm_sec +
                            (epoch_ms - static_cast<double>(seconds) * 1000.0) / 1000.0;
    return static_cast<float>(into_day / 86400.0);
}

bool BeginSceneScripts(owe::Scene& scene, const FrameInputs& fi) {
    auto script_scene = scene.ExtensionMut<ScriptScene>();
    if (script_scene.is_none() || (**script_scene).TickPending()) return false;
    if (static_cast<double>(fi.runtime) < scene.ScriptStart()) return false;
    if (! (**script_scene).runtime().AsyncTickAllowed()) return false;
    {
        auto camera = scene.ScriptCameraTransforms();
        (**script_scene).runtime().SetCameraSnapshot(camera.is_some(), camera.is_some() ? *camera : owe::SceneCameraTransforms {}, scene.SceneFov());
    }
    auto events = scene.ConsumeAnimationEvents();
    auto inputs = fi;
    if (auto start = scene.ScriptClock(); start.is_some()) {
        inputs.clock_ms    = Some(*start + static_cast<double>(fi.runtime) * 1000.0);
        inputs.time_of_day = LocalTimeOfDay(*inputs.clock_ms);
    }
    (**script_scene).BeginTick(inputs, rstd::move(events));
    return true;
}

void TickSceneScripts(owe::Scene& scene, const FrameInputs& fi) {
    auto script_scene = scene.ExtensionMut<ScriptScene>();
    if (script_scene.is_none()) return;
    if ((**script_scene).TickPending()) {
        // started early by BeginSceneScripts: collect it where the scripts would have run
        (**script_scene).FinishTick();
        (void)scene.ConsumeAnimationEvents();
        scene.ProcessLayerDeletions();
        return;
    }
    auto events = scene.ConsumeAnimationEvents();
    // Scripts start at the scene's script start, on the scene's clock.
    if (static_cast<double>(fi.runtime) < scene.ScriptStart()) return;
    if (auto start = scene.ScriptClock(); start.is_some()) {
        // the scripts' clock started with the scene: Date and engine.timeOfDay read it
        auto inputs        = fi;
        inputs.clock_ms    = Some(*start + static_cast<double>(fi.runtime) * 1000.0);
        inputs.time_of_day = LocalTimeOfDay(*inputs.clock_ms);
        (**script_scene).Tick(inputs, events.as_slice());
    } else {
        (**script_scene).Tick(fi, events.as_slice());
    }
    scene.ProcessLayerDeletions();
}

void SetSceneUserProperty(owe::Scene& scene, ref<str> key, const Json& property) {
    auto script_scene = scene.ExtensionMut<ScriptScene>();
    if (script_scene.is_some()) (**script_scene).runtime().SetUserProperty(key, property);
    scene.ApplyUserLightVisibilityBindings(key, property);
}

void SetSceneUserProperties(owe::Scene& scene, const rstd::json::Map& properties) {
    auto script_scene = scene.ExtensionMut<ScriptScene>();
    if (script_scene.is_some()) (**script_scene).runtime().SetUserProperties(properties);
    properties.iter().for_each([&](auto entry) {
        auto [key, property] = entry;
        scene.ApplyUserLightVisibilityBindings(key->as_str(), *property);
    });
}

void SetSceneMediaStatus(owe::Scene& scene, const MediaStatus& status) {
    auto script_scene = scene.ExtensionMut<ScriptScene>();
    if (script_scene.is_some()) (**script_scene).runtime().SetMediaStatus(status);
}

void SetScenePersistence(owe::Scene& scene, PathBuf path) {
    auto script_scene = scene.ExtensionMut<ScriptScene>();
    if (script_scene.is_none()) return;
    (**script_scene).runtime().SetPersistence(rstd::move(path));
}

} // namespace owe::script
