module;

#include <rstd/enum.hpp>

#if defined(__SSE__)
#    include <xmmintrin.h>
#endif

module wescene.pkg.parse;
import eigen;
import wescene.core;
import rstd.log;
import wescene.utils;
import wescene.scene;
import wescene.particle;
import wescene.particle.program;

using namespace rstd::prelude;
using namespace owe;
using namespace Eigen;
using namespace rstd::literals;
using rstd::mtp::same_as;

namespace
{

constexpr float  kTau   = f32::consts::TAU.to_primitive();
constexpr double kTau64 = f64::consts::TAU.to_primitive();

inline Vector3d GenRandomVec3(const array<float, 3>& min, const array<float, 3>& max) {
    Vector3d result(3);
    for (int32_t i = 0; i < 3; i++) {
        result[i] = Random::get(min[usize(i)], max[usize(i)]);
    }
    return result;
}

inline float GenRandom(float min, float max, float exponent) {
    auto random = Random::get(0.0f, 1.0f);
    if (exponent != 1.0f) random = f32(random).powf(f32(exponent)).to_primitive();
    return static_cast<float>(algorism::lerp(random, min, max));
}

enum class SequenceLimitBehavior
{
    Repeat,
    Mirror,
    Clamp,
};

auto ParseSequenceLimitBehavior(const Json& json) -> SequenceLimitBehavior {
    String value { "repeat"_Str };
    owe::GetJsonValue(json, "limitbehavior"_str, value, false);
    if (value == "mirror"_str) return SequenceLimitBehavior::Mirror;
    if (value == "clamp"_str) return SequenceLimitBehavior::Clamp;
    return SequenceLimitBehavior::Repeat;
}

auto SequenceIndex(u64 sequence, u64 count, SequenceLimitBehavior behavior) -> u64 {
    if (count <= u64(1)) return u64();
    if (behavior == SequenceLimitBehavior::Clamp) return rstd::cmp::min(sequence, count - u64(1));
    if (behavior == SequenceLimitBehavior::Mirror) {
        auto period = (count - u64(1)) * u64(2);
        auto value  = sequence % period;
        return value < count ? value : period - value;
    }
    return sequence % count;
}

template<typename States>
auto SpawnSequence(const States& states, particle::ParticleSlot slot) -> u64 {
    return states[slot.index].spawn_sequence;
}

auto SequenceBasis(const Eigen::Vector3d& axis) -> Eigen::Vector3d {
    if (f64(axis.z()).abs().to_primitive() > 0.5) return Eigen::Vector3d { 0.0, 1.0, 0.0 };
    return Eigen::Vector3d { 1.0, 0.0, 0.0 };
}

struct MapSequenceAroundControlPoint {
    i32                   controlpoint {};
    float                 count { 1.0f };
    array<float, 2>       bounds { 0.0f, 1.0f };
    array<float, 3>       axis { 0.0f, 0.0f, 1.0f };
    array<float, 3>       speed_min { 0.0f, 0.0f, 0.0f };
    array<float, 3>       speed_max { 0.0f, 0.0f, 0.0f };
    SequenceLimitBehavior limit_behavior { SequenceLimitBehavior::Repeat };

    static auto ReadFromJson(const Json& json) -> MapSequenceAroundControlPoint {
        MapSequenceAroundControlPoint value;
        owe::GetJsonValue(json, "controlpoint"_str, value.controlpoint, false);
        owe::GetJsonValue(json, "count"_str, value.count, false);
        owe::GetJsonValue(json, "bounds"_str, value.bounds, false);
        owe::GetJsonValue(json, "axis"_str, value.axis, false);
        owe::GetJsonValue(json, "speedmin"_str, value.speed_min, false);
        owe::GetJsonValue(json, "speedmax"_str, value.speed_max, false);
        value.limit_behavior = ParseSequenceLimitBehavior(json);
        return value;
    }
};

struct MapSequenceAroundControlPointProgram {
    MapSequenceAroundControlPoint config;

    void Initialize(ParticleSpawnColumns& columns, particle::ParticleSpawnRequest request,
                    ref<dyn<rstd::any::Any>> frame_context) {
        auto frame         = ParticleFrameFrom(frame_context);
        auto controlpoints = frame->subsystem->Controlpoints();
        auto controlpoint  = rstd::as_cast<usize>(config.controlpoint % i32(8));
        auto center        = controlpoints[controlpoint].offset;

        Eigen::Vector3d axis { Eigen::Vector3f { config.axis.data() }.cast<double>() };
        if (axis.squaredNorm() <= 1e-12) axis = Eigen::Vector3d { 0.0, 0.0, 1.0 };
        axis.normalize();
        auto basis    = SequenceBasis(axis);
        basis         = (basis - axis * basis.dot(axis)).normalized();
        auto tangent  = basis.cross(axis).normalized();
        auto slot     = request.slot;
        auto sequence = SpawnSequence(columns.states, slot);
        auto relative = columns.positions[slot.index].cast<double>() - center;
        auto parallel = axis * relative.dot(axis);
        auto radius   = (relative - parallel).norm();
        auto angle    = kTau64 * static_cast<double>(sequence.to_primitive()) /
                        rstd::cmp::max(static_cast<double>(config.count), 1e-6);
        angle *= static_cast<double>(config.bounds[usize(1)] - config.bounds[usize(0)]);
        angle += kTau64 * static_cast<double>(config.bounds[usize(0)]);
        columns.positions[slot.index] = (center + parallel +
                                         radius * (f64(angle).cos().to_primitive() * basis +
                                                   f64(angle).sin().to_primitive() * tangent))
                                            .cast<float>();

        auto velocity = GenRandomVec3(config.speed_min, config.speed_max);
        if (velocity.squaredNorm() > 1e-12) {
            columns.velocities[slot.index] = (columns.velocities[slot.index].cast<double>() +
                                              Eigen::AngleAxisd(-angle, axis) * velocity)
                                                 .cast<float>();
        }
    }
};

struct MapSequenceBetweenControlPoints {
    i32                   controlpoint_start {};
    i32                   controlpoint_end { 1 };
    u32                   count { 2 };
    SequenceLimitBehavior limit_behavior { SequenceLimitBehavior::Repeat };
    // flag 4: the sequence tapers, the particle at index i of n sin(pi (i + 0.5) / n) of its size
    // (particles-sequence-arc)
    bool taper { false };

    static auto ReadFromJson(const Json& json, u32 implicit_count)
        -> MapSequenceBetweenControlPoints {
        MapSequenceBetweenControlPoints value;
        u32 flags {};
        owe::GetJsonValue(json, "flags"_str, flags, false);
        value.taper = (flags.to_primitive() & 4u) != 0u;
        value.count = rstd::cmp::max(implicit_count, u32(2));
        owe::GetJsonValue(json, "controlpointstart"_str, value.controlpoint_start, false);
        owe::GetJsonValue(json, "controlpointend"_str, value.controlpoint_end, false);
        if (json.get("count"_str).is_some()) {
            owe::GetJsonValue(json, "count"_str, value.count, false);
            value.count = rstd::cmp::max(value.count, u32(2));
        }
        value.limit_behavior = ParseSequenceLimitBehavior(json);
        return value;
    }
};

struct MapSequenceBetweenControlPointsProgram {
    MapSequenceBetweenControlPoints config;

    void Initialize(ParticleSpawnColumns& columns, particle::ParticleSpawnRequest request,
                    ref<dyn<rstd::any::Any>> frame_context) {
        auto frame         = ParticleFrameFrom(frame_context);
        auto controlpoints = frame->subsystem->Controlpoints();
        auto start_index   = rstd::as_cast<usize>(config.controlpoint_start % i32(8));
        auto end_index     = rstd::as_cast<usize>(config.controlpoint_end % i32(8));
        auto start         = controlpoints[start_index].offset;
        auto end           = controlpoints[end_index].offset;
        auto path          = end - start;
        auto count         = rstd::as_cast<u64>(config.count);
        auto sequence      = SpawnSequence(columns.states, request.slot);
        auto index         = SequenceIndex(sequence, count, config.limit_behavior);
        auto amount        = static_cast<double>(index.to_primitive()) /
                             static_cast<double>((count - u64(1)).to_primitive());

        Eigen::Vector3d relative = columns.positions[request.slot.index].cast<double>() - start;
        if (path.squaredNorm() > 1e-12) {
            auto direction = path.normalized();
            relative -= direction * relative.dot(direction);
        }
        columns.positions[request.slot.index] = (start + amount * path + relative).cast<float>();
        if (config.taper) {
            const double scale = f64(3.14159265358979323846 *
                                     (static_cast<double>(index.to_primitive()) + 0.5) /
                                     static_cast<double>(count.to_primitive()))
                                     .sin()
                                     .to_primitive();
            columns.sizes[request.slot.index] *= static_cast<float>(scale);
            columns.initial_sizes[request.slot.index] *= static_cast<float>(scale);
        }
    }
};

// colorlist: a colour picked from a list, which multiplies the particle's colour
// (particles-colorlist: Wallpaper Engine paints a yellow colorrandom and the list's cyan as dark
// green, its yellow as yellow, its magenta as red, in either order, so the list is applied after
// the other initializers).
struct ColorListProgram {
    Vec<Eigen::Vector3f> colors;

    void Initialize(ParticleSpawnColumns& columns, particle::ParticleSpawnRequest request,
                    ref<dyn<rstd::any::Any>>) {
        if (colors.is_empty()) return;
        auto pick = static_cast<usize>(Random::get(0.0, 1.0) *
                                       static_cast<double>(colors.len().to_primitive()));
        if (pick >= colors.len()) pick = colors.len() - usize(1);
        auto& color                                = columns.colors[request.slot.index];
        color                                      = color.cwiseProduct(colors[pick]);
        columns.initial_colors[request.slot.index] = color;
    }
};

// hsvcolorrandom: a random colour from hue, saturation and value ranges (particles-hsvrange: the
// hues Wallpaper Engine picks stay within huemin..huemax; equal ends give any hue at all).
struct HsvColorRandomProgram {
    double hue_min { 0.0 }, hue_max { 1.0 };
    double saturation_min { 1.0 }, saturation_max { 1.0 };
    double value_min { 1.0 }, value_max { 1.0 };

    void Initialize(ParticleSpawnColumns& columns, particle::ParticleSpawnRequest request,
                    ref<dyn<rstd::any::Any>>) {
        const double hue = hue_min == hue_max
                               ? Random::get(0.0, 1.0)
                               : algorism::lerp(Random::get(0.0, 1.0), hue_min, hue_max);
        const double saturation =
            algorism::lerp(Random::get(0.0, 1.0), saturation_min, saturation_max);
        const double    value = algorism::lerp(Random::get(0.0, 1.0), value_min, value_max);
        Eigen::Vector3f color = HsvToRgb(hue, saturation, value).cast<float>();
        columns.colors[request.slot.index]         = color;
        columns.initial_colors[request.slot.index] = color;
    }
};

// the control point inputs, from a particle's position and the control points' (in the particles'
// space)
inline auto RemapControlpointInput(const ParticleRemap& remap, const Eigen::Vector3d& position,
                                   const Eigen::Vector3d& first, const Eigen::Vector3d& second)
    -> Eigen::Vector3d {
    switch (remap.input) {
    case ParticleRemap::Input::DistanceToControlPoint:
        return Eigen::Vector3d::Constant((position - first).norm());
    case ParticleRemap::Input::DeltaToControlPoint: return first - position;
    case ParticleRemap::Input::DirectionToControlPoint: {
        const Eigen::Vector3d delta = first - position;
        return delta.norm() > 1e-12 ? Eigen::Vector3d(delta.normalized())
                                    : Eigen::Vector3d(Eigen::Vector3d::Zero());
    }
    case ParticleRemap::Input::PositionBetweenControlPoints: {
        const Eigen::Vector3d line = second - first;
        const double          span = line.squaredNorm();
        return Eigen::Vector3d::Constant(span > 1e-12 ? (position - first).dot(line) / span : 0.0);
    }
    default: return Eigen::Vector3d::Zero();
    }
}

// the values of remapvalue's inputs that don't come from the particle
inline auto RemapFrameInput(const ParticleRemap& remap, const ParticleFrame& frame)
    -> Eigen::Vector3d {
    switch (remap.input) {
    case ParticleRemap::Input::SystemTime:
    case ParticleRemap::Input::LayerTime:
        return Eigen::Vector3d::Constant(frame.time.to_primitive());
    case ParticleRemap::Input::Runtime:
        return Eigen::Vector3d::Constant(frame.subsystem->SceneRuntimeSeconds());
    case ParticleRemap::Input::TimeOfDay:
        return Eigen::Vector3d::Constant(frame.subsystem->LocalTimeOfDay());
    case ParticleRemap::Input::LayerOrigin: return frame.world_from_spawn_space.block<3, 1>(0, 3);
    default: return Eigen::Vector3d::Zero();
    }
}

// remapinitialvalue: remapvalue's mapping once, when a particle is born (a particle that moves on
// keeps the value it was born with; particles-remapinitialvalue).
struct RemapInitialValueProgram {
    ParticleRemap remap;

    void Initialize(ParticleSpawnColumns& columns, particle::ParticleSpawnRequest request,
                    ref<dyn<rstd::any::Any>> frame_context) {
        auto            frame = ParticleFrameFrom(frame_context);
        auto            slot  = request.slot.index;
        Eigen::Vector3d input = RemapFrameInput(remap, *frame);
        switch (remap.input) {
        case ParticleRemap::Input::DistanceToControlPoint:
        case ParticleRemap::Input::DeltaToControlPoint:
        case ParticleRemap::Input::DirectionToControlPoint:
        case ParticleRemap::Input::PositionBetweenControlPoints: {
            auto controlpoints = frame->subsystem->Controlpoints();
            input              = RemapControlpointInput(
                remap,
                columns.positions[slot].cast<double>(),
                controlpoints[rstd::as_cast<usize>(remap.controlpoint)].offset,
                controlpoints[rstd::as_cast<usize>(remap.controlpoint1)].offset);
            break;
        }
        case ParticleRemap::Input::Velocity: input = columns.velocities[slot].cast<double>(); break;
        case ParticleRemap::Input::Opacity:
            input = Eigen::Vector3d::Constant(columns.alphas[slot]);
            break;
        case ParticleRemap::Input::Rotation:
            input = Eigen::Vector3d::Constant(columns.rotations[slot].z());
            break;
        case ParticleRemap::Input::AngularSpeed:
            input = Eigen::Vector3d::Constant(columns.angular_velocities[slot].z());
            break;
        default: break;
        }
        switch (remap.output) {
        case ParticleRemap::Output::Color: {
            auto value = remap.Apply(columns.colors[slot].cast<double>(), input).cast<float>();
            columns.colors[slot]         = value;
            columns.initial_colors[slot] = value;
            break;
        }
        case ParticleRemap::Output::Opacity: {
            auto value =
                float(remap.Apply(Eigen::Vector3d::Constant(columns.alphas[slot]), input).x());
            columns.alphas[slot]         = value;
            columns.initial_alphas[slot] = value;
            break;
        }
        case ParticleRemap::Output::Size: {
            auto value =
                float(remap.Apply(Eigen::Vector3d::Constant(columns.sizes[slot]), input).x());
            columns.sizes[slot]         = value;
            columns.initial_sizes[slot] = value;
            break;
        }
        case ParticleRemap::Output::Velocity:
            columns.velocities[slot] =
                remap.Apply(columns.velocities[slot].cast<double>(), input).cast<float>();
            break;
        }
    }
};

inline float UiColorToLinear(float value) { return value * value; }

inline float UiScalarToLinear(float value) { return value; }

} // namespace

struct SingleRandom {
    float       min { 0.0f };
    float       max { 0.0f };
    float       exponent { 1.0f };
    static void ReadFromJson(const Json& j, SingleRandom& r) {
        owe::GetJsonValue(j, "min"_str, r.min, false);
        owe::GetJsonValue(j, "max"_str, r.max, false);
        owe::GetJsonValue(j, "exponent"_str, r.exponent, false);
    };
};
struct VecRandom {
    array<float, 3> min { 0.0f, 0.0f, 0.0f };
    array<float, 3> max { 0.0f, 0.0f, 0.0f };
    float           exponent { 1.0f };

    static void ReadFromJson(const Json& j, VecRandom& r) {
        owe::GetJsonValue(j, "min"_str, r.min, false);
        owe::GetJsonValue(j, "max"_str, r.max, false);
        owe::GetJsonValue(j, "exponent"_str, r.exponent, false);
    };
};
struct TurbulentRandom {
    float  scale { 1.0f };
    double timescale { 1.0f };
    float  offset { 0.0f };
    float  speedmin { 100.0f };
    float  speedmax { 250.0f };
    float  phasemin { 0.0f };
    float  phasemax { 0.1f };

    array<float, 3> forward { 0.0f, 1.0f, 0.0f };
    array<float, 3> normal { 0.0f, 0.0f, 1.0f };

    static void ReadFromJson(const Json& j, TurbulentRandom& r) {
        owe::GetJsonValue(j, "scale"_str, r.scale, false);
        owe::GetJsonValue(j, "timescale"_str, r.timescale, false);
        owe::GetJsonValue(j, "offset"_str, r.offset, false);
        owe::GetJsonValue(j, "speedmin"_str, r.speedmin, false);
        owe::GetJsonValue(j, "speedmax"_str, r.speedmax, false);
        owe::GetJsonValue(j, "phasemin"_str, r.phasemin, false);
        owe::GetJsonValue(j, "phasemax"_str, r.phasemax, false);
        owe::GetJsonValue(j, "forward"_str, r.forward, false);
        owe::GetJsonValue(j, "right"_str, r.normal, false);
        owe::GetJsonValue(j, "normal"_str, r.normal, false);
    };
};
struct NoopSpawnProgram {
    void Initialize(ParticleSpawnColumns&, particle::ParticleSpawnRequest,
                    ref<dyn<rstd::any::Any>>) {}
};

// inheritcontrolpointvelocity: the particle starts with its control point's velocity times a factor
// from min to max (particles-controlpoint-velocity: a world-space particle gets the object's speed,
// half of it at 0.5; one in its object's space moves with the object, so its control point is at
// rest and it gets none).
struct InheritControlpointVelocityProgram {
    i32    controlpoint { 0 };
    double min { 0.0 }, max { 1.0 };

    void Initialize(ParticleSpawnColumns& columns, particle::ParticleSpawnRequest request,
                    ref<dyn<rstd::any::Any>> frame_context) {
        auto                  frame = ParticleFrameFrom(frame_context);
        const Eigen::Vector3d local = frame->controlpoint_velocity.col(
            rstd::as_cast<usize>(controlpoint % i32(8)).to_primitive());
        const double factor = algorism::lerp(Random::get(0.0, 1.0), min, max);
        columns.velocities[request.slot.index] =
            (columns.velocities[request.slot.index].cast<double>() + local * factor).cast<float>();
    }
};

// The colour and opacity of the parent particle a child system's instance was made for
// (inheritinitialvaluefromevent takes only the colour, inheritvaluefromevent both;
// particles-inherit-event: not the size or velocity).
struct ParentLook {
    Eigen::Vector3f color;
    float           alpha {};
};

inline auto ParentParticleLook(const ParticleFrame& frame) -> Option<ParentLook> {
    const auto& bounded = frame.subsystem->InstanceState(frame.instance_index).bounded;
    if (bounded.parent == nullptr || bounded.parent_subsystem == nullptr ||
        bounded.particle_index < isize())
        return None();
    auto& storage = bounded.parent->Storage();
    auto  index   = rstd::as_cast<usize>(bounded.particle_index);
    if (index >= storage.Len()) return None();
    particle::ParticleSlotReader reader(storage, particle::ParticleSlot { .index = index });
    const auto&                  attributes = bounded.parent_subsystem->Attributes();
    return Some(ParentLook { .color = Eigen::Vector3f(reader.Read(attributes.color)),
                             .alpha = float(reader.Read(attributes.alpha)) });
}

// inheritinitialvaluefromevent: the colour of the particle whose event made this system
struct InheritInitialValueFromEventProgram {
    void Initialize(ParticleSpawnColumns& columns, particle::ParticleSpawnRequest request,
                    ref<dyn<rstd::any::Any>> frame_context) {
        auto look = ParentParticleLook(*ParticleFrameFrom(frame_context));
        if (look.is_none()) return;
        columns.colors[request.slot.index]         = look->color;
        columns.initial_colors[request.slot.index] = look->color;
    }
};

struct ColorRandomProgram {
    array<float, 3> min;
    array<float, 3> max;

    void Initialize(ParticleSpawnColumns& columns, particle::ParticleSpawnRequest request,
                    ref<dyn<rstd::any::Any>>) {
        auto            random = Random::get(0.0, 1.0);
        Eigen::Vector3f value;
        for (usize component {}; component < usize(3); ++component) {
            auto raw   = component.to_primitive();
            value[raw] = static_cast<float>(algorism::lerp(random, min[component], max[component]));
        }
        // times the colour the emitter gave (a layer image's pixels; white for the others), not
        // instead of it
        const Eigen::Vector3f base                 = columns.colors[request.slot.index];
        const Eigen::Vector3f tinted               = base.cwiseProduct(value);
        columns.colors[request.slot.index]         = tinted;
        columns.initial_colors[request.slot.index] = tinted;
    }
};

struct LifetimeRandomProgram {
    SingleRandom config;

    void Initialize(ParticleSpawnColumns& columns, particle::ParticleSpawnRequest request,
                    ref<dyn<rstd::any::Any>>) {
        auto value                            = GenRandom(config.min, config.max, config.exponent);
        columns.lifetimes[request.slot.index] = value;
        columns.initial_lifetimes[request.slot.index] = value;
    }
};

struct SizeRandomProgram {
    SingleRandom config;

    void Initialize(ParticleSpawnColumns& columns, particle::ParticleSpawnRequest request,
                    ref<dyn<rstd::any::Any>>) {
        auto value                        = GenRandom(config.min, config.max, config.exponent);
        columns.sizes[request.slot.index] = value;
        columns.initial_sizes[request.slot.index] = value;
    }
};

struct AlphaRandomProgram {
    SingleRandom config;

    void Initialize(ParticleSpawnColumns& columns, particle::ParticleSpawnRequest request,
                    ref<dyn<rstd::any::Any>>) {
        auto value                         = GenRandom(config.min, config.max, config.exponent);
        columns.alphas[request.slot.index] = value;
        columns.initial_alphas[request.slot.index] = value;
    }
};

struct VectorRandomProgram {
    enum class Target
    {
        Velocity,
        Rotation,
        AngularVelocity,
    };

    VecRandom config;
    Target    target { Target::Velocity };

    void Initialize(ParticleSpawnColumns& columns, particle::ParticleSpawnRequest request,
                    ref<dyn<rstd::any::Any>>) {
        Eigen::Vector3f value;
        for (usize component {}; component < usize(3); ++component) {
            auto raw   = component.to_primitive();
            value[raw] = GenRandom(config.min[component], config.max[component], config.exponent);
        }
        if (target == Target::Velocity) {
            columns.velocities[request.slot.index] += value;
        } else if (target == Target::Rotation) {
            columns.rotations[request.slot.index] += value;
        } else {
            columns.angular_velocities[request.slot.index] += value;
        }
    }
};

struct TurbulentVelocityRandomProgram {
    TurbulentRandom config;
    Eigen::Vector3f normal;
    Eigen::Vector3f forward;
    Eigen::Vector3f position;

    void Initialize(ParticleSpawnColumns& columns, particle::ParticleSpawnRequest request,
                    ref<dyn<rstd::any::Any>>) {
        auto  duration = request.emitter_duration;
        float speed    = Random::get(config.speedmin, config.speedmax);
        float phase    = Random::get(config.phasemin, config.phasemax);
        if (duration > f64(10.0)) {
            position[0] += speed;
            duration = f64();
        }
        Eigen::Vector3f result;
        do {
            result = algorism::CurlNoise((position + normal * phase).cast<double>()).cast<float>();
            result -= normal * result.dot(normal);
            if (result.squaredNorm() <= 1e-8f) result = forward;
            result.normalize();
            position += result * 0.005f * static_cast<float>(config.timescale);
            duration -= f64(0.01);
        } while (duration > f64(0.01));

        auto cosine = rstd::cmp::min(1.0f, rstd::cmp::max(-1.0f, result.dot(forward)));
        auto angle  = static_cast<float>(
            f32(normal.dot(forward.cross(result))).atan2(f32(cosine)).to_primitive());
        auto scale = rstd::cmp::max(config.scale * 0.5f, 0.0f);
        result     = Eigen::AngleAxisf(angle * scale + config.offset, normal) * forward;
        columns.velocities[request.slot.index] += result * speed;
    }
};

struct OverrideSpawnProgram {
    ParticleInstanceModifiers modifiers;

    void Initialize(ParticleSpawnColumns& columns, particle::ParticleSpawnRequest request,
                    ref<dyn<rstd::any::Any>>) {
        auto index = request.slot.index;
        columns.lifetimes[index] *= modifiers.Lifetime();
        columns.initial_lifetimes[index] = columns.lifetimes[index];
        columns.alphas[index] *= UiScalarToLinear(modifiers.Alpha());
        columns.initial_alphas[index] = columns.alphas[index];
        columns.sizes[index] *= modifiers.Size();
        columns.initial_sizes[index] = columns.sizes[index];
        columns.velocities[index] *= modifiers.Speed();
        columns.angular_velocities[index] *= modifiers.Speed();
        if (modifiers.HasColorOverride()) {
            auto            color = modifiers.Color();
            Eigen::Vector3f value { color[usize(0)], color[usize(1)], color[usize(2)] };
            if (modifiers.UsesLegacyColor()) value /= 255.0f;
            value                         = { UiColorToLinear(value[0]),
                                              UiColorToLinear(value[1]),
                                              UiColorToLinear(value[2]) };
            columns.colors[index]         = value;
            columns.initial_colors[index] = value;
        }
    }
};

class ParticleSpawnInstructionValue {
    RSTD_ENUM(ParticleSpawnInstructionValue, (Noop, (NoopSpawnProgram value;)),
              (ColorRandom, (ColorRandomProgram value;)),
              (LifetimeRandom, (LifetimeRandomProgram value;)),
              (SizeRandom, (SizeRandomProgram value;)), (AlphaRandom, (AlphaRandomProgram value;)),
              (VectorRandom, (VectorRandomProgram value;)),
              (TurbulentVelocityRandom, (TurbulentVelocityRandomProgram value;)),
              (MapSequenceAroundControlPoint, (MapSequenceAroundControlPointProgram value;)),
              (MapSequenceBetweenControlPoints, (MapSequenceBetweenControlPointsProgram value;)),
              (Override, (OverrideSpawnProgram value;)), (ColorList, (ColorListProgram value;)),
              (InheritControlpointVelocity, (InheritControlpointVelocityProgram value;)),
              (InheritInitialValueFromEvent, (InheritInitialValueFromEventProgram value;)),
              (HsvColorRandom, (HsvColorRandomProgram value;)),
              (RemapInitialValue, (RemapInitialValueProgram value;)))
};

template<typename T>
auto MakeParticleSpawnInstructionValue(T value) -> ParticleSpawnInstructionValue {
    if constexpr (same_as<T, NoopSpawnProgram>)
        return ParticleSpawnInstructionValue::Noop(rstd::move(value));
    else if constexpr (same_as<T, ColorRandomProgram>)
        return ParticleSpawnInstructionValue::ColorRandom(rstd::move(value));
    else if constexpr (same_as<T, LifetimeRandomProgram>)
        return ParticleSpawnInstructionValue::LifetimeRandom(rstd::move(value));
    else if constexpr (same_as<T, SizeRandomProgram>)
        return ParticleSpawnInstructionValue::SizeRandom(rstd::move(value));
    else if constexpr (same_as<T, AlphaRandomProgram>)
        return ParticleSpawnInstructionValue::AlphaRandom(rstd::move(value));
    else if constexpr (same_as<T, VectorRandomProgram>)
        return ParticleSpawnInstructionValue::VectorRandom(rstd::move(value));
    else if constexpr (same_as<T, TurbulentVelocityRandomProgram>)
        return ParticleSpawnInstructionValue::TurbulentVelocityRandom(rstd::move(value));
    else if constexpr (same_as<T, MapSequenceAroundControlPointProgram>)
        return ParticleSpawnInstructionValue::MapSequenceAroundControlPoint(rstd::move(value));
    else if constexpr (same_as<T, MapSequenceBetweenControlPointsProgram>)
        return ParticleSpawnInstructionValue::MapSequenceBetweenControlPoints(rstd::move(value));
    else if constexpr (same_as<T, ColorListProgram>)
        return ParticleSpawnInstructionValue::ColorList(rstd::move(value));
    else if constexpr (same_as<T, InheritControlpointVelocityProgram>)
        return ParticleSpawnInstructionValue::InheritControlpointVelocity(rstd::move(value));
    else if constexpr (same_as<T, InheritInitialValueFromEventProgram>)
        return ParticleSpawnInstructionValue::InheritInitialValueFromEvent(rstd::move(value));
    else if constexpr (same_as<T, HsvColorRandomProgram>)
        return ParticleSpawnInstructionValue::HsvColorRandom(rstd::move(value));
    else if constexpr (same_as<T, RemapInitialValueProgram>)
        return ParticleSpawnInstructionValue::RemapInitialValue(rstd::move(value));
    else {
        static_assert(same_as<T, OverrideSpawnProgram>);
        return ParticleSpawnInstructionValue::Override(rstd::move(value));
    }
}

struct ParticleSpawnInstruction::Impl {
    template<typename T>
    explicit Impl(T value): value(MakeParticleSpawnInstructionValue(rstd::move(value))) {}

    ParticleSpawnInstructionValue value;
};

ParticleSpawnInstruction::ParticleSpawnInstruction(Box<Impl> impl): m_impl(rstd::move(impl)) {}
ParticleSpawnInstruction::ParticleSpawnInstruction(ParticleSpawnInstruction&&) noexcept = default;
auto ParticleSpawnInstruction::operator=(ParticleSpawnInstruction&&) noexcept
    -> ParticleSpawnInstruction&                      = default;
ParticleSpawnInstruction::~ParticleSpawnInstruction() = default;

auto ParticleSpawnInstruction::SequenceCount() const -> Option<u32> {
    RSTD_MATCH(m_impl->value) {
        RSTD_CASE(Noop) { return None(); }
        RSTD_CASE(ColorRandom) { return None(); }
        RSTD_CASE(LifetimeRandom) { return None(); }
        RSTD_CASE(SizeRandom) { return None(); }
        RSTD_CASE(AlphaRandom) { return None(); }
        RSTD_CASE(VectorRandom) { return None(); }
        RSTD_CASE(TurbulentVelocityRandom) { return None(); }
        RSTD_CASE(MapSequenceAroundControlPoint) { return None(); }
        RSTD_CASE(MapSequenceBetweenControlPoints, instruction) {
            return Some(rstd::as_cast<u32>(instruction.config.count));
        }
        RSTD_CASE(HsvColorRandom) { return None(); }
        RSTD_CASE(RemapInitialValue) { return None(); }
        RSTD_CASE(Override) { return None(); }
        RSTD_CASE(ColorList) { return None(); }
        RSTD_CASE(InheritControlpointVelocity) { return None(); }
        RSTD_CASE(InheritInitialValueFromEvent) { return None(); }
    }
    rstd::unreachable();
}

template<typename T>
auto ParticleSpawnInstruction::Make(T value) -> ParticleSpawnInstruction {
    return ParticleSpawnInstruction(Box<Impl>::make(rstd::move(value)));
}

void ParticleSpawnInstruction::Initialize(ParticleSpawnColumns&          columns,
                                          particle::ParticleSpawnRequest request,
                                          ref<dyn<rstd::any::Any>>       frame) {
    RSTD_MATCH(m_impl->value) {
        RSTD_CASE(Noop, instruction) { instruction.Initialize(columns, request, frame); }
        RSTD_CASE(ColorRandom, instruction) { instruction.Initialize(columns, request, frame); }
        RSTD_CASE(LifetimeRandom, instruction) { instruction.Initialize(columns, request, frame); }
        RSTD_CASE(SizeRandom, instruction) { instruction.Initialize(columns, request, frame); }
        RSTD_CASE(AlphaRandom, instruction) { instruction.Initialize(columns, request, frame); }
        RSTD_CASE(VectorRandom, instruction) { instruction.Initialize(columns, request, frame); }
        RSTD_CASE(TurbulentVelocityRandom, instruction) {
            instruction.Initialize(columns, request, frame);
        }
        RSTD_CASE(MapSequenceAroundControlPoint, instruction) {
            instruction.Initialize(columns, request, frame);
        }
        RSTD_CASE(MapSequenceBetweenControlPoints, instruction) {
            instruction.Initialize(columns, request, frame);
        }
        RSTD_CASE(HsvColorRandom, instruction) { instruction.Initialize(columns, request, frame); }
        RSTD_CASE(RemapInitialValue, instruction) {
            instruction.Initialize(columns, request, frame);
        }
        RSTD_CASE(Override, instruction) { instruction.Initialize(columns, request, frame); }
        RSTD_CASE(ColorList, instruction) { instruction.Initialize(columns, request, frame); }
        RSTD_CASE(InheritControlpointVelocity, instruction) {
            instruction.Initialize(columns, request, frame);
        }

        RSTD_CASE(InheritInitialValueFromEvent, instruction) {
            instruction.Initialize(columns, request, frame);
        }
    }
}

// the list's colour multiplies what the other initializers made, so it runs after them
auto ParticleSpawnInstruction::RunsLast() const -> bool {
    RSTD_MATCH(m_impl->value) {
        RSTD_CASE(ColorList) { return true; }
        RSTD_CASE(Noop) { return false; }
        RSTD_CASE(ColorRandom) { return false; }
        RSTD_CASE(LifetimeRandom) { return false; }
        RSTD_CASE(SizeRandom) { return false; }
        RSTD_CASE(AlphaRandom) { return false; }
        RSTD_CASE(VectorRandom) { return false; }
        RSTD_CASE(TurbulentVelocityRandom) { return false; }
        RSTD_CASE(MapSequenceAroundControlPoint) { return false; }
        RSTD_CASE(MapSequenceBetweenControlPoints) { return false; }
        RSTD_CASE(Override) { return false; }
        RSTD_CASE(InheritControlpointVelocity) { return false; }
        RSTD_CASE(InheritInitialValueFromEvent) { return false; }
        RSTD_CASE(HsvColorRandom) { return false; }
        RSTD_CASE(RemapInitialValue) { return false; }
    }
    rstd::unreachable();
}

ParticleRotationRange owe::ReadRotationRandom(const Json& initializer) {
    ParticleRotationRange range;
    range.max[usize(2)] = kTau;
    auto read           = [&](ref<str> key, array<float, 3>& out) {
        auto value = initializer.get(key);
        if (value.is_none()) return;
        if (auto number = (*value)->as_f64(); number.is_some()) {
            out = { 0.0f, 0.0f, static_cast<float>(number->to_primitive()) };
            return;
        }
        owe::GetJsonValue(initializer, key, out, false);
    };
    read("min"_str, range.min);
    read("max"_str, range.max);
    owe::GetJsonValue(initializer, "exponent"_str, range.exponent, false);
    return range;
}

ParticleSpawnInstruction ParticleParser::GenInitializer(const Json& wpj,
                                                        u32         implicit_sequence_count) {
    do {
        if (wpj.get("name"_str).is_none()) break;
        String name;
        owe::GetJsonValue(wpj, "name"_str, name);

        if (name == "colorrandom"_str) {
            VecRandom r;
            r.min = { 0.0f, 0.0f, 0.0f };
            r.max = { 255.0f, 255.0f, 255.0f };
            VecRandom::ReadFromJson(wpj, r);
            return ParticleSpawnInstruction::Make(ColorRandomProgram {
                .min = rstd::move(r.min).map([](float value) {
                    return value / 255.0f;
                }),
                .max = rstd::move(r.max).map([](float value) {
                    return value / 255.0f;
                }),
            });
        } else if (name == "colorlist"_str) {
            Vec<Eigen::Vector3f> colors;
            if (auto listed = wpj.get("colors"_str); listed.is_some()) {
                if (auto items = (*listed)->as_array(); items.is_some()) {
                    for (const auto& item : **items) {
                        array<float, 3> color { 1.0f, 1.0f, 1.0f };
                        owe::GetJsonValue(item, color);
                        colors.push(
                            Eigen::Vector3f(color[usize(0)], color[usize(1)], color[usize(2)]));
                    }
                }
            }
            return ParticleSpawnInstruction::Make(
                ColorListProgram { .colors = rstd::move(colors) });
        } else if (name == "inheritcontrolpointvelocity"_str) {
            i32   controlpoint { 0 };
            float min { 0.0f }, max { 1.0f };
            owe::GetJsonValue(wpj, "controlpoint"_str, controlpoint, false);
            owe::GetJsonValue(wpj, "min"_str, min, false);
            owe::GetJsonValue(wpj, "max"_str, max, false);
            return ParticleSpawnInstruction::Make(InheritControlpointVelocityProgram {
                .controlpoint = controlpoint, .min = double(min), .max = double(max) });
        } else if (name == "inheritinitialvaluefromevent"_str) {
            return ParticleSpawnInstruction::Make(InheritInitialValueFromEventProgram {});
        } else if (name == "hsvcolorrandom"_str) {
            float hue_min { 0.0f }, hue_max { 1.0f }, saturation_min { 1.0f },
                saturation_max { 1.0f }, value_min { 1.0f }, value_max { 1.0f };
            owe::GetJsonValue(wpj, "huemin"_str, hue_min, false);
            owe::GetJsonValue(wpj, "huemax"_str, hue_max, false);
            owe::GetJsonValue(wpj, "saturationmin"_str, saturation_min, false);
            owe::GetJsonValue(wpj, "saturationmax"_str, saturation_max, false);
            owe::GetJsonValue(wpj, "valuemin"_str, value_min, false);
            owe::GetJsonValue(wpj, "valuemax"_str, value_max, false);
            return ParticleSpawnInstruction::Make(HsvColorRandomProgram {
                .hue_min        = hue_min,
                .hue_max        = hue_max,
                .saturation_min = saturation_min,
                .saturation_max = rstd::cmp::max(saturation_max, saturation_min),
                .value_min      = value_min,
                .value_max      = rstd::cmp::max(value_max, value_min),
            });
        } else if (name == "remapinitialvalue"_str) {
            auto remap = ParticleRemap::FromJson(wpj);
            if (remap.is_none()) break;
            return ParticleSpawnInstruction::Make(
                RemapInitialValueProgram { .remap = rstd::move(*remap) });
        } else if (name == "lifetimerandom"_str) {
            SingleRandom r = { 0.0f, 1.0f };
            SingleRandom::ReadFromJson(wpj, r);
            return ParticleSpawnInstruction::Make(LifetimeRandomProgram { .config = r });
        } else if (name == "sizerandom"_str) {
            SingleRandom r = { 0.0f, 20.0f };
            SingleRandom::ReadFromJson(wpj, r);
            return ParticleSpawnInstruction::Make(SizeRandomProgram { .config = r });
        } else if (name == "alpharandom"_str) {
            SingleRandom r = { 0.05f, 1.0f };
            SingleRandom::ReadFromJson(wpj, r);
            return ParticleSpawnInstruction::Make(AlphaRandomProgram { .config = r });
        } else if (name == "velocityrandom"_str) {
            VecRandom r;
            r.min[usize(0)] = r.min[usize(1)] = -32.0f;
            r.max[usize(0)] = r.max[usize(1)] = 32.0f;
            VecRandom::ReadFromJson(wpj, r);
            return ParticleSpawnInstruction::Make(VectorRandomProgram {
                .config = r,
                .target = VectorRandomProgram::Target::Velocity,
            });
        } else if (name == "rotationrandom"_str) {
            auto range = ReadRotationRandom(wpj);
            return ParticleSpawnInstruction::Make(VectorRandomProgram {
                .config =
                    VecRandom { .min = range.min, .max = range.max, .exponent = range.exponent },
                .target = VectorRandomProgram::Target::Rotation,
            });
        } else if (name == "angularvelocityrandom"_str) {
            VecRandom r;
            r.min[usize(2)] = -5.0f;
            r.max[usize(2)] = 5.0f;
            VecRandom::ReadFromJson(wpj, r);
            return ParticleSpawnInstruction::Make(VectorRandomProgram {
                .config = r,
                .target = VectorRandomProgram::Target::AngularVelocity,
            });
        } else if (name == "turbulentvelocityrandom"_str) {
            TurbulentRandom r;
            TurbulentRandom::ReadFromJson(wpj, r);
            Vector3f normal(r.normal.data());
            if (normal.squaredNorm() <= 1e-8f) normal = Vector3f::UnitZ();
            normal.normalize();

            Vector3f forward(r.forward.data());
            forward -= normal * forward.dot(normal);
            if (forward.squaredNorm() <= 1e-8f) forward = normal.unitOrthogonal();
            forward.normalize();

            return ParticleSpawnInstruction::Make(TurbulentVelocityRandomProgram {
                .config  = r,
                .normal  = normal,
                .forward = forward,
                .position =
                    GenRandomVec3({ 0.0f, 0.0f, 0.0f }, { 10.0f, 10.0f, 10.0f }).cast<float>(),
            });
        } else if (name == "mapsequencearoundcontrolpoint"_str) {
            return ParticleSpawnInstruction::Make(MapSequenceAroundControlPointProgram {
                .config = MapSequenceAroundControlPoint::ReadFromJson(wpj),
            });
        } else if (name == "mapsequencebetweencontrolpoints"_str) {
            return ParticleSpawnInstruction::Make(MapSequenceBetweenControlPointsProgram {
                .config =
                    MapSequenceBetweenControlPoints::ReadFromJson(wpj, implicit_sequence_count),
            });
        }
    } while (false);
    return ParticleSpawnInstruction::Make(NoopSpawnProgram {});
}

ParticleSpawnInstruction ParticleParser::GenOverride(ParticleInstanceModifiers modifiers) {
    return ParticleSpawnInstruction::Make(
        OverrideSpawnProgram { .modifiers = rstd::move(modifiers) });
}
double FadeValueChange(float life, float start, float end, float startValue,
                       float endValue) noexcept {
    if (life <= start)
        return startValue;
    else if (life > end)
        return endValue;
    else {
        double pass = (life - start) / (end - start);
        return algorism::lerp(pass, startValue, endValue);
    }
}

struct ValueChange {
    float starttime { 0 };
    float endtime { 1.0f };
    float startvalue { 1.0f };
    float endvalue { 0.0f };

    static auto ReadFromJson(const Json& j) {
        ValueChange v;
        owe::GetJsonValue(j, "starttime"_str, v.starttime, false);
        owe::GetJsonValue(j, "endtime"_str, v.endtime, false);
        owe::GetJsonValue(j, "startvalue"_str, v.startvalue, false);
        owe::GetJsonValue(j, "endvalue"_str, v.endvalue, false);
        return v;
    }
};
double FadeValueChange(float life, const ValueChange& v) noexcept {
    return FadeValueChange(life, v.starttime, v.endtime, v.startvalue, v.endvalue);
}

struct VecChange {
    float           starttime { 0 };
    float           endtime { 1.0f };
    array<float, 3> startvalue { 1.0f, 1.0f, 1.0f };
    array<float, 3> endvalue { 0.0f, 0.0f, 0.0f };

    static auto ReadFromJson(const Json& j) {
        VecChange v;
        owe::GetJsonValue(j, "starttime"_str, v.starttime, false);
        owe::GetJsonValue(j, "endtime"_str, v.endtime, false);
        owe::GetJsonValue(j, "startvalue"_str, v.startvalue, false);
        owe::GetJsonValue(j, "endvalue"_str, v.endvalue, false);
        return v;
    }
};

struct FrequencyValue {
    array<float, 3> mask { 1.0f, 1.0f, 0.0f };

    float frequencymin { 0.0f };
    float frequencymax { 10.0f };
    float scalemin { 0.0f };
    float scalemax { 1.0f };
    float phasemin { 0.0f };
    float phasemax { kTau };

    static auto ReadFromJson(const Json& j, ref<str> name) {
        FrequencyValue v;
        if (name == "oscillatesize"_str) {
            v.scalemin = 0.8f;
            v.scalemax = 1.2f;
        } else if (name == "oscillateposition"_str) {
            v.frequencymax = 5.0f;
        }
        owe::GetJsonValue(j, "frequencymin"_str, v.frequencymin, false);
        owe::GetJsonValue(j, "frequencymax"_str, v.frequencymax, false);
        if (v.frequencymax == 0.0f) v.frequencymax = v.frequencymin;
        owe::GetJsonValue(j, "scalemin"_str, v.scalemin, false);
        owe::GetJsonValue(j, "scalemax"_str, v.scalemax, false);
        owe::GetJsonValue(j, "phasemin"_str, v.phasemin, false);
        owe::GetJsonValue(j, "phasemax"_str, v.phasemax, false);
        owe::GetJsonValue(j, "mask"_str, v.mask, false);
        return v;
    };
    inline void GenFrequency(bool lifetime_ok, OscillationStateRef st) {
        if (! lifetime_ok) st.reset = true;
        if (st.reset) {
            st.frequency = Random::get(frequencymin, frequencymax);
            st.scale     = Random::get(scalemin, scalemax);
            st.phase =
                static_cast<float>(Random::get(static_cast<double>(phasemin), phasemax + kTau64));
            st.reset = false;
        }
    }
    inline double GetScale(OscillationStateRef st, double time) {
        double f = st.frequency / kTau;
        double w = kTau * f;
        return algorism::lerp(
            (f64(w * time + st.phase).cos().to_primitive() + 1.0f) * 0.5f, scalemin, scalemax);
    }
    inline double GetMove(OscillationStateRef st, double time, f64 time_pass) {
        double f = st.frequency / kTau;
        double w = kTau * f;
        return -1.0f * st.scale * w * f64(w * time + st.phase).sin().to_primitive() *
               time_pass.to_primitive();
    }
};

struct Turbulence {
    // the minimum time offset of the noise field for a particle.
    float phasemin { 0 };
    // the maximum time offset of the noise field for a particle.
    float phasemax { 0 };
    // the minimum velocity applied to particles.
    float speedmin { 500.0f };
    // the maximum velocity applied to particles.
    float speedmax { 1000.0f };
    // how fast the noise field changes shape.
    float timescale { 20.0f };

    float scale { 0.01f };

    array<float, 3> mask { 1.0f, 1.0f, 0.0f };

    static auto ReadFromJson(const Json& j) {
        Turbulence v;
        owe::GetJsonValue(j, "phasemin"_str, v.phasemin, false);
        owe::GetJsonValue(j, "phasemax"_str, v.phasemax, false);
        owe::GetJsonValue(j, "speedmin"_str, v.speedmin, false);
        owe::GetJsonValue(j, "speedmax"_str, v.speedmax, false);
        owe::GetJsonValue(j, "timescale"_str, v.timescale, false);
        owe::GetJsonValue(j, "mask"_str, v.mask, false);
        owe::GetJsonValue(j, "scale"_str, v.scale, false);
        return v;
    };
};

struct Vortex {
    enum class FlagEnum
    {
        infinite_axis               = 0, // 1
        maintain_distance_to_center = 1, // 2
    };
    using EFlags = BitFlags<FlagEnum>;

    i32 controlpoint { 0 };

    // anything below this distance receives force multiplied with speed inner.
    float distanceinner { 500.0f };
    // anything above this distance receives force multiplied with speed outer.
    float distanceouter { 650.0f };
    // amount of force applied to inner ring.
    float speedinner { 2500.0f };
    // amount of force applied to outer ring.
    float speedouter { 0 };

    EFlags flags { 0 };

    // positional offset from the center of the control point.
    array<float, 3> offset { 0.0f, 0.0f, 0.0f };

    // the axis to rotate around.
    array<float, 3> axis { 0.0f, 0.0f, 1.0f };

    float ringradius {};
    float ringwidth {};
    float ringpulldistance {};

    static auto ReadFromJson(const Json& j) {
        Vortex v;
        owe::GetJsonValue(j, "controlpoint"_str, v.controlpoint, false);
        if (v.controlpoint >= i32(8)) rstd_error("wrong contropoint index {}", v.controlpoint);
        v.controlpoint %= i32(8);

        owe::GetJsonValue(j, "distanceinner"_str, v.distanceinner, false);
        owe::GetJsonValue(j, "distanceouter"_str, v.distanceouter, false);
        owe::GetJsonValue(j, "speedinner"_str, v.speedinner, false);
        owe::GetJsonValue(j, "speedouter"_str, v.speedouter, false);

        i32 _flags { 0 };
        owe::GetJsonValue(j, "flags"_str, _flags, false);
        v.flags = EFlags(static_cast<rstd::uint32_t>(_flags.to_primitive()));

        owe::GetJsonValue(j, "offset"_str, v.offset, false);
        owe::GetJsonValue(j, "axis"_str, v.axis, false);
        owe::GetJsonValue(j, "ringradius"_str, v.ringradius, false);
        owe::GetJsonValue(j, "ringwidth"_str, v.ringwidth, false);
        owe::GetJsonValue(j, "ringpulldistance"_str, v.ringpulldistance, false);

        return v;
    };
};

struct VortexFrame {
    Vector3d center { Vector3d::Zero() };
    Vector3d axis { Vector3d::UnitZ() };
};

auto ResolveVortexFrame(const Vortex& vortex, const ParticleSubSystem& subsystem) -> VortexFrame {
    auto controlpoint = subsystem.SimulationControlpoint(rstd::as_cast<usize>(vortex.controlpoint));
    Vector3d local_offset = Vector3f { vortex.offset.data() }.cast<double>();
    Vector3d axis         = Vector3f { vortex.axis.data() }.cast<double>();
    local_offset          = controlpoint.basis * local_offset;
    axis                  = controlpoint.basis * axis;
    if (axis.squaredNorm() <= 1e-12) axis = Vector3d::UnitZ();
    return {
        .center = controlpoint.center + local_offset,
        .axis   = axis.normalized(),
    };
}

auto VortexSpeedAtDistance(const Vortex& vortex, double distance) -> double {
    auto distance_range = static_cast<double>(vortex.distanceouter - vortex.distanceinner);
    if (distance_range < 0.0 || distance < vortex.distanceinner) return vortex.speedinner;
    if (distance > vortex.distanceouter) return vortex.speedouter;
    auto amount = (distance - vortex.distanceinner) / (distance_range + 0.1);
    return algorism::lerp(amount, vortex.speedinner, vortex.speedouter);
}

struct ControlPointForce {
    i32 controlpoint { 0 };
    u32 flags { 2 };

    // how strongly the control point attracts or repels.
    float scale { 512.0f };
    // the maximum distance between particle and control point where the force takes effect.
    float threshold { 512.0f };

    // positional offset from the center of the control point.
    array<float, 3> origin { 0.0f, 0.0f, 0.0f };

    static auto ReadFromJson(const Json& j) {
        ControlPointForce v;
        owe::GetJsonValue(j, "controlpoint"_str, v.controlpoint, false);
        if (v.controlpoint >= i32(8)) rstd_error("wrong contropoint index {}", v.controlpoint);
        v.controlpoint %= i32(8);

        owe::GetJsonValue(j, "scale"_str, v.scale, false);
        owe::GetJsonValue(j, "threshold"_str, v.threshold, false);
        owe::GetJsonValue(j, "flags"_str, v.flags, false);

        owe::GetJsonValue(j, "origin"_str, v.origin, false);
        return v;
    };
};

struct MaintainDistanceState {
    float distance {};
    bool  initialized { false };
};

class MaintainDistanceAttribute {
public:
    using Value = MaintainDistanceState;

    MaintainDistanceAttribute(particle::ParticleAttributeDescriptor descriptor, Value default_value)
        : m_storage(rstd::move(descriptor), rstd::move(default_value)) {}

    auto Descriptor() const -> ref<particle::ParticleAttributeDescriptor> {
        return m_storage.Descriptor();
    }
    auto ConcreteType() const noexcept -> rstd::any::TypeId { return m_storage.ConcreteType(); }
    auto ValueType() const noexcept -> rstd::any::TypeId { return m_storage.ValueTypeId(); }
    auto Len() const noexcept -> usize { return m_storage.Len(); }
    auto Capacity() const noexcept -> usize { return m_storage.Capacity(); }
    void Reserve(usize total_slots) { m_storage.Reserve(total_slots); }
    void AppendDefaults(usize count) { m_storage.AppendDefaults(count); }
    void ResetSlots(slice<particle::ParticleSlot> slots) { m_storage.ResetSlots(slots); }
    void Clear() { m_storage.Clear(); }
    auto Values() const noexcept -> slice<Value> { return m_storage.Values(); }
    auto ValuesMut() noexcept -> mut_ref<Value[]> { return m_storage.ValuesMut(); }
    auto CloneEmpty() const -> MaintainDistanceAttribute {
        return MaintainDistanceAttribute(m_storage.CloneDescriptor(), m_storage.DefaultValue());
    }

private:
    particle::ParticleValueAttributeStorage<Value> m_storage;
};

struct MaintainDistance {
    i32   controlpoint {};
    float variable_strength { 5.0f };

    static auto ReadFromJson(const Json& json) -> MaintainDistance {
        MaintainDistance value;
        owe::GetJsonValue(json, "controlpoint"_str, value.controlpoint, false);
        owe::GetJsonValue(json, "variablestrength"_str, value.variable_strength, false);
        value.controlpoint %= i32(8);
        return value;
    }
};

auto RegisterMaintainDistanceAttribute(ParticleSubSystem& subsystem, usize operator_index)
    -> particle::ParticleAttributeKey<MaintainDistanceAttribute> {
    auto name   = rstd::format("maintain_distance_{}", operator_index);
    auto result = subsystem.SchemaBuilder().Register<MaintainDistanceAttribute>(
        name.as_str(), "we.operator.maintain_distance"_str, MaintainDistanceState {});
    if (result.is_err()) rstd::panic { "failed to register maintain distance attribute" };
    return result.unwrap();
}

template<typename Attribute, typename Value>
auto RegisterOscillationAttribute(particle::ParticleSchemaBuilder& builder, ref<str> name,
                                  Value default_value)
    -> particle::ParticleAttributeKey<Attribute> {
    auto result = builder.Register<Attribute>(name,
                                              "we.operator.oscillation"_str,
                                              particle::ParticleAttributeResetPolicy::Custom,
                                              default_value);
    if (result.is_err()) rstd::panic { "failed to register oscillation attribute" };
    return result.unwrap();
}

auto RegisterOscillationAttributes(ParticleSubSystem& subsystem, usize operator_index,
                                   ref<str> suffix) -> OscillationAttributes {
    auto                  prefix  = rstd::format("oscillation_{}_{}", operator_index, suffix);
    auto&                 builder = subsystem.SchemaBuilder();
    OscillationAttributes attributes {
        .reset = RegisterOscillationAttribute<OscillationResetAttribute>(
            builder, rstd::format("{}_reset", prefix).as_str(), true),
        .frequency = RegisterOscillationAttribute<OscillationFrequencyAttribute>(
            builder, rstd::format("{}_frequency", prefix).as_str(), 0.0f),
        .scale = RegisterOscillationAttribute<OscillationScaleAttribute>(
            builder, rstd::format("{}_scale", prefix).as_str(), 1.0f),
        .phase = RegisterOscillationAttribute<OscillationPhaseAttribute>(
            builder, rstd::format("{}_phase", prefix).as_str(), 0.0f),
    };
    return attributes;
}

auto LifetimePosition(float lifetime, float initial_lifetime) noexcept -> double {
    if (lifetime < 0.0f) return 1.0;
    return 1.0 - static_cast<double>(lifetime / initial_lifetime);
}

auto LifetimePassed(float lifetime, float initial_lifetime) noexcept -> double {
    return static_cast<double>(initial_lifetime - lifetime);
}

struct MovementOperator {
    ParticleAttributes                                        attributes;
    float                                                     drag {};
    Eigen::Vector3d                                           gravity;
    ParticleInstanceModifiers                                 modifiers;
    particle::ParticleWriteIndex<particle::VelocityAttribute> velocity;

    void Compile(particle::ParticleViewCompiler& compiler) {
        compiler.WriteBase(attributes.position);
        velocity = compiler.Write(attributes.velocity);
    }

    void Update(particle::ParticleUpdateContext& context) {
        auto frame      = ParticleFrameFrom(context.frame);
        auto positions  = context.view.PositionsMut();
        auto velocities = context.view.Write(velocity);
        auto delta      = context.delta.to_primitive();
        auto speed      = modifiers.Speed();
        // Wallpaper Engine (FUN_14023fbc0 case 1): gravity is added, the particle moves, then drag takes
        // min(drag dt2, 1) of the speed, dt2 being the frame-rate compensated step (particles-drag-step: a thrown
        // particle stops at speed / (drag x 0.503, 0.818, 1) at 15, 30, 60 fps).
        const double keep = 1.0 - rstd::cmp::min(double(drag) * context.delta2.to_primitive(), 1.0);
        for (auto slot : context.slots) {
            const Eigen::Vector3d pulled =
                velocities[slot.index].cast<double>() +
                speed * (frame->world_space ? frame->local_from_world_dir * gravity : gravity) * delta;
            positions[slot.index] = (positions[slot.index].cast<double>() + pulled * delta).cast<float>();
            velocities[slot.index] = (pulled * keep).cast<float>();
        }
    }
};

struct AngularMovementOperator {
    ParticleAttributes                                               attributes;
    float                                                            drag {};
    Eigen::Vector3d                                                  force;
    particle::ParticleWriteIndex<particle::RotationAttribute>        rotation;
    particle::ParticleWriteIndex<particle::AngularVelocityAttribute> angular_velocity;

    void Compile(particle::ParticleViewCompiler& compiler) {
        rotation         = compiler.Write(attributes.rotation);
        angular_velocity = compiler.Write(attributes.angular_velocity);
    }

    void Update(particle::ParticleUpdateContext& context) {
        auto rotations  = context.view.Write(rotation);
        auto velocities = context.view.Write(angular_velocity);
        auto delta      = context.delta.to_primitive();
        // the same as movement (FUN_14023fbc0 case 2): force added, turned, then min(drag dt2, 1) of the angular speed
        // taken
        const double keep = 1.0 - rstd::cmp::min(double(drag) * context.delta2.to_primitive(), 1.0);
        for (auto slot : context.slots) {
            const Eigen::Vector3d pushed = velocities[slot.index].cast<double>() + force * delta;
            rotations[slot.index] = (rotations[slot.index].cast<double>() + pushed * delta).cast<float>();
            velocities[slot.index] = (pushed * keep).cast<float>();
        }
    }
};

struct ScalarChangeOperator {
    enum class Target
    {
        Size,
        Alpha,
    };

    ParticleAttributes                                              attributes;
    ValueChange                                                     change;
    Target                                                          target { Target::Alpha };
    ParticleInstanceModifiers                                       modifiers;
    particle::ParticleWriteIndex<particle::SizeAttribute>           size;
    particle::ParticleWriteIndex<particle::AlphaAttribute>          alpha;
    particle::ParticleReadIndex<particle::LifetimeAttribute>        lifetime;
    particle::ParticleReadIndex<particle::InitialLifetimeAttribute> initial_lifetime;

    void Compile(particle::ParticleViewCompiler& compiler) {
        if (target == Target::Size)
            size = compiler.Write(attributes.size);
        else
            alpha = compiler.Write(attributes.alpha);
        lifetime         = compiler.Read(attributes.lifetime);
        initial_lifetime = compiler.Read(attributes.initial_lifetime);
    }

    void Update(particle::ParticleUpdateContext& context) {
        auto lifetimes = context.view.Read(lifetime);
        auto initial   = context.view.Read(initial_lifetime);
        if (target == Target::Size) {
            auto values = context.view.Write(size);
            auto scale  = modifiers.Size();
            for (auto slot : context.slots) {
                values[slot.index] *=
                    scale *
                    FadeValueChange(LifetimePosition(lifetimes[slot.index], initial[slot.index]),
                                    change);
            }
        } else {
            auto values = context.view.Write(alpha);
            for (auto slot : context.slots) {
                values[slot.index] *= FadeValueChange(
                    LifetimePosition(lifetimes[slot.index], initial[slot.index]), change);
            }
        }
    }
};

struct AlphaFadeOperator {
    ParticleAttributes                                              attributes;
    float                                                           fade_in { 0.5f };
    float                                                           fade_out { 0.5f };
    particle::ParticleWriteIndex<particle::AlphaAttribute>          alpha;
    particle::ParticleReadIndex<particle::LifetimeAttribute>        lifetime;
    particle::ParticleReadIndex<particle::InitialLifetimeAttribute> initial_lifetime;

    void Compile(particle::ParticleViewCompiler& compiler) {
        alpha            = compiler.Write(attributes.alpha);
        lifetime         = compiler.Read(attributes.lifetime);
        initial_lifetime = compiler.Read(attributes.initial_lifetime);
    }

    void Update(particle::ParticleUpdateContext& context) {
        auto values    = context.view.Write(alpha);
        auto lifetimes = context.view.Read(lifetime);
        auto initial   = context.view.Read(initial_lifetime);
        for (auto slot : context.slots) {
            auto life = LifetimePosition(lifetimes[slot.index], initial[slot.index]);
            if (life <= fade_in) {
                values[slot.index] *= FadeValueChange(life, 0, fade_in, 0, 1.0f);
            } else if (life > fade_out) {
                values[slot.index] *= 1.0f - FadeValueChange(life, fade_out, 1.0f, 0, 1.0f);
            }
        }
    }
};

struct ColorChangeOperator {
    ParticleAttributes                                              attributes;
    VecChange                                                       change;
    particle::ParticleWriteIndex<particle::ColorAttribute>          color;
    particle::ParticleReadIndex<particle::LifetimeAttribute>        lifetime;
    particle::ParticleReadIndex<particle::InitialLifetimeAttribute> initial_lifetime;

    void Compile(particle::ParticleViewCompiler& compiler) {
        color            = compiler.Write(attributes.color);
        lifetime         = compiler.Read(attributes.lifetime);
        initial_lifetime = compiler.Read(attributes.initial_lifetime);
    }

    void Update(particle::ParticleUpdateContext& context) {
        auto colors    = context.view.Write(color);
        auto lifetimes = context.view.Read(lifetime);
        auto initial   = context.view.Read(initial_lifetime);
        for (auto slot : context.slots) {
            auto            life = LifetimePosition(lifetimes[slot.index], initial[slot.index]);
            Eigen::Vector3f factor;
            for (usize component {}; component < usize(3); ++component) {
                auto raw    = component.to_primitive();
                factor[raw] = static_cast<float>(FadeValueChange(life,
                                                                 change.starttime,
                                                                 change.endtime,
                                                                 change.startvalue[component],
                                                                 change.endvalue[component]));
            }
            colors[slot.index] = colors[slot.index].cwiseProduct(factor);
        }
    }
};

struct OscillationIndices {
    particle::ParticleWriteIndex<OscillationResetAttribute>     reset;
    particle::ParticleWriteIndex<OscillationFrequencyAttribute> frequency;
    particle::ParticleWriteIndex<OscillationScaleAttribute>     scale;
    particle::ParticleWriteIndex<OscillationPhaseAttribute>     phase;

    void Compile(particle::ParticleViewCompiler& compiler,
                 const OscillationAttributes&    attributes) {
        reset     = compiler.Write(attributes.reset);
        frequency = compiler.Write(attributes.frequency);
        scale     = compiler.Write(attributes.scale);
        phase     = compiler.Write(attributes.phase);
    }

    auto Bind(particle::ParticleWriteView view) const -> OscillationValues {
        return {
            .reset     = view.Write(reset),
            .frequency = view.Write(frequency),
            .scale     = view.Write(scale),
            .phase     = view.Write(phase),
        };
    }
};

struct OscillateScalarOperator {
    enum class Target
    {
        Alpha,
        Size,
    };

    ParticleAttributes                                              attributes;
    OscillationAttributes                                           state_attributes;
    FrequencyValue                                                  frequency;
    Target                                                          target { Target::Alpha };
    OscillationIndices                                              state;
    particle::ParticleWriteIndex<particle::AlphaAttribute>          alpha;
    particle::ParticleWriteIndex<particle::SizeAttribute>           size;
    particle::ParticleReadIndex<particle::LifetimeAttribute>        lifetime;
    particle::ParticleReadIndex<particle::InitialLifetimeAttribute> initial_lifetime;

    void Compile(particle::ParticleViewCompiler& compiler) {
        if (target == Target::Alpha)
            alpha = compiler.Write(attributes.alpha);
        else
            size = compiler.Write(attributes.size);
        lifetime         = compiler.Read(attributes.lifetime);
        initial_lifetime = compiler.Read(attributes.initial_lifetime);
        state.Compile(compiler, state_attributes);
    }

    template<typename Values>
    void Apply(Values values, particle::ParticleUpdateContext& context) {
        auto lifetimes = context.view.Read(lifetime);
        auto initial   = context.view.Read(initial_lifetime);
        auto states    = state.Bind(context.view);
        for (auto slot : context.slots) {
            auto oscillator = states.At(slot.index);
            frequency.GenFrequency(lifetimes[slot.index] > 0.0f, oscillator);
            values[slot.index] *= frequency.GetScale(
                oscillator, LifetimePassed(lifetimes[slot.index], initial[slot.index]));
        }
    }

    void Update(particle::ParticleUpdateContext& context) {
        if (target == Target::Alpha)
            Apply(context.view.Write(alpha), context);
        else
            Apply(context.view.Write(size), context);
    }
};

struct OscillatePositionOperator {
    ParticleAttributes                                              attributes;
    array<FrequencyValue, 3>                                        frequencies;
    array<OscillationAttributes, 3>                                 state_attributes;
    array<OscillationIndices, 3>                                    states;
    ParticleOperatorWindow                                          window;
    particle::ParticleReadIndex<particle::LifetimeAttribute>        lifetime;
    particle::ParticleReadIndex<particle::InitialLifetimeAttribute> initial_lifetime;

    void Compile(particle::ParticleViewCompiler& compiler) {
        compiler.WriteBase(attributes.position);
        lifetime         = compiler.Read(attributes.lifetime);
        initial_lifetime = compiler.Read(attributes.initial_lifetime);
        for (usize index {}; index < usize(3); ++index) {
            states[index].Compile(compiler, state_attributes[index]);
        }
    }

    void Update(particle::ParticleUpdateContext& context) {
        auto                        positions = context.view.PositionsMut();
        auto                        lifetimes = context.view.Read(lifetime);
        auto                        initial   = context.view.Read(initial_lifetime);
        array<OscillationValues, 3> values {
            states[usize()].Bind(context.view),
            states[usize(1)].Bind(context.view),
            states[usize(2)].Bind(context.view),
        };
        for (auto slot : context.slots) {
            Eigen::Vector3d offset { Eigen::Vector3d::Zero() };
            auto            time = LifetimePassed(lifetimes[slot.index], initial[slot.index]);
            for (usize component {}; component < usize(3); ++component) {
                auto raw = component.to_primitive();
                if (frequencies[usize()].mask[component] < 0.01f) continue;
                auto oscillator = values[component].At(slot.index);
                frequencies[component].GenFrequency(lifetimes[slot.index] > 0.0f, oscillator);
                offset[raw] = frequencies[component].GetMove(oscillator, time, context.delta);
            }
            if (! window.Always())
                offset *= window.Weight(LifetimePosition(lifetimes[slot.index], initial[slot.index]));
            positions[slot.index] = (positions[slot.index].cast<double>() + offset).cast<float>();
        }
    }
};

// turbulence (operator case 14 of wallpaper64.exe's FUN_14023fbc0): each particle draws a random r
// once; the velocity gains n * mask * speed * dt2, where speed = speedmin + r (speedmax - speedmin)
// and n is simplex noise at p = (position + r (phasemax - phasemin) + time * timescale) * scale:
// n = (noise(p.x, p.y, p.z), noise(p.z, p.x, p.y), noise(p.y, p.z, p.x)), not normalized.
struct TurbulenceOperator {
    ParticleAttributes                                              attributes;
    Turbulence                                                      config;
    ParticleInstanceModifiers                                       modifiers;
    OscillationAttributes                                           state_attributes;
    OscillationIndices                                              state;
    particle::ParticleWriteIndex<particle::VelocityAttribute>       velocity;
    ParticleOperatorWindow                                          window;
    particle::ParticleReadIndex<particle::LifetimeAttribute>        lifetime;
    particle::ParticleReadIndex<particle::InitialLifetimeAttribute> initial_lifetime;

    void Compile(particle::ParticleViewCompiler& compiler) {
        compiler.ReadBase(attributes.position);
        velocity = compiler.Write(attributes.velocity);
        lifetime         = compiler.Read(attributes.lifetime);
        initial_lifetime = compiler.Read(attributes.initial_lifetime);
        state.Compile(compiler, state_attributes);
    }

    void Update(particle::ParticleUpdateContext& context) {
        auto         frame      = ParticleFrameFrom(context.frame);
        auto         positions  = context.view.Positions();
        auto         velocities = context.view.Write(velocity);
        auto         lifetimes  = context.view.Read(lifetime);
        auto         initial    = context.view.Read(initial_lifetime);
        auto         random     = state.Bind(context.view);
        const double delta      = context.delta2.to_primitive();
        const double time       = frame->time.to_primitive() * config.timescale;
        for (auto slot : context.slots) {
            // the particle's random number, drawn the first time it is seen (kept in the phase
            // slot of an oscillation state)
            auto own = random.At(slot.index);
            if (! (lifetimes[slot.index] > 0.0f)) own.reset = true;
            if (own.reset) {
                own.phase = static_cast<float>(Random::get(0.0, 1.0));
                own.reset = false;
            }
            const double weight =
                window.Always()
                    ? 1.0
                    : window.Weight(LifetimePosition(lifetimes[slot.index], initial[slot.index]));
            if (weight <= 0.0) continue;
            const double    r     = own.phase;
            const double    phase = r * (config.phasemax - config.phasemin) + time;
            Eigen::Vector3d p =
                (positions[slot.index].cast<double>() + Eigen::Vector3d::Constant(phase)) *
                static_cast<double>(config.scale);
            Eigen::Vector3d n(algorism::SimplexNoise3(p.x(), p.y(), p.z()),
                              algorism::SimplexNoise3(p.z(), p.x(), p.y()),
                              algorism::SimplexNoise3(p.y(), p.z(), p.x()));
            const double speed =
                (config.speedmin + r * (config.speedmax - config.speedmin)) * modifiers.Speed();
            for (usize component {}; component < usize(3); ++component) {
                n[component.to_primitive()] *= config.mask[component];
            }
            velocities[slot.index] =
                (velocities[slot.index].cast<double>() + n * (speed * delta * weight)).cast<float>();
        }
    }
};

struct VortexOperator {
    ParticleAttributes                                        attributes;
    Vortex                                                    config;
    bool                                                      extended { false };
    particle::ParticleWriteIndex<particle::VelocityAttribute> velocity;

    void Compile(particle::ParticleViewCompiler& compiler) {
        compiler.ReadBase(attributes.position);
        velocity = compiler.Write(attributes.velocity);
    }

    void Update(particle::ParticleUpdateContext& context) {
        auto particle_frame = ParticleFrameFrom(context.frame);
        auto frame          = ResolveVortexFrame(config, *particle_frame->subsystem);
        auto positions      = context.view.Positions();
        auto velocities     = context.view.Write(velocity);
        auto delta          = context.delta.to_primitive();
        for (auto slot : context.slots) {
            auto relative        = positions[slot.index].cast<double>() - frame.center;
            auto radial          = relative - frame.axis * relative.dot(frame.axis);
            auto radial_distance = radial.norm();
            if (radial_distance <= 1e-9) continue;

            if (! extended) {
                Eigen::Vector3d direction = -frame.axis.cross(radial).normalized();
                velocities[slot.index] =
                    (velocities[slot.index].cast<double>() +
                     direction * VortexSpeedAtDistance(config, radial_distance) * 0.5 * delta)
                        .cast<float>();
                continue;
            }

            auto distance =
                config.flags[Vortex::FlagEnum::infinite_axis] ? radial_distance : relative.norm();
            bool ring_shape = config.ringradius > 0.0f && config.ringwidth > 0.0f &&
                              config.ringpulldistance > 0.0f;
            if (ring_shape) {
                Eigen::Vector3d ring_position =
                    radial.normalized() * static_cast<double>(config.ringradius);
                auto ring_delta    = ring_position - relative;
                auto ring_distance = ring_delta.norm();
                if (ring_distance >= static_cast<double>(config.ringpulldistance)) continue;

                auto ring_width = static_cast<double>(config.ringwidth);
                auto pull_range =
                    rstd::cmp::max(1e-9, static_cast<double>(config.ringpulldistance) - ring_width);
                double ring_influence {};
                double pull_influence {};
                double ring_speed {};
                if (ring_distance <= ring_width) {
                    auto amount    = ring_distance / ring_width;
                    ring_speed     = algorism::lerp(amount, config.speedinner, config.speedouter);
                    ring_influence = 1.0;
                    pull_influence = amount;
                } else {
                    pull_influence = (ring_distance - ring_width) / pull_range;
                    ring_influence = f64(1.0 - pull_influence).sqrt().to_primitive();
                    ring_speed     = config.speedouter;
                }

                auto            ring_strength = ring_speed * ring_influence * 0.5;
                Eigen::Vector3d tangent       = -frame.axis.cross(radial).normalized();
                Eigen::Vector3d current       = velocities[slot.index].cast<double>();
                if (! config.flags[Vortex::FlagEnum::maintain_distance_to_center]) {
                    current += tangent * ring_strength * delta;
                } else {
                    auto axis_velocity = frame.axis * current.dot(frame.axis);
                    auto tangent_speed = current.dot(tangent) + ring_strength * delta;
                    current            = axis_velocity + tangent * tangent_speed;
                }
                auto pull_strength =
                    f64(VortexSpeedAtDistance(config, distance)).abs().to_primitive() * 0.5;
                current += ring_delta.normalized() * pull_strength * pull_influence * delta;
                velocities[slot.index] = current.cast<float>();
                continue;
            }

            auto strength = VortexSpeedAtDistance(config, distance) * 0.5;
            if (f64(strength).abs().to_primitive() <= 1e-9) continue;
            Eigen::Vector3d tangent = -frame.axis.cross(radial).normalized();
            Eigen::Vector3d current = velocities[slot.index].cast<double>();
            if (! config.flags[Vortex::FlagEnum::maintain_distance_to_center]) {
                current += tangent * strength * delta;
            } else {
                auto axis_velocity = frame.axis * current.dot(frame.axis);
                auto tangent_speed = current.dot(tangent) + strength * delta;
                current            = axis_velocity + tangent * tangent_speed;
            }
            velocities[slot.index] = current.cast<float>();
        }
    }
};

struct MaintainDistanceOperator {
    ParticleAttributes                                        attributes;
    MaintainDistance                                          config;
    particle::ParticleAttributeKey<MaintainDistanceAttribute> state_key;
    particle::ParticleWriteIndex<particle::VelocityAttribute> velocity;
    particle::ParticleWriteIndex<MaintainDistanceAttribute>   state;

    void Compile(particle::ParticleViewCompiler& compiler) {
        compiler.ReadBase(attributes.position);
        velocity = compiler.Write(attributes.velocity);
        state    = compiler.Write(state_key);
    }

    void Update(particle::ParticleUpdateContext& context) {
        auto frame      = ParticleFrameFrom(context.frame);
        auto positions  = context.view.Positions();
        auto velocities = context.view.Write(velocity);
        auto states     = context.view.Write(state);
        auto center =
            frame->subsystem->SimulationControlpoint(rstd::as_cast<usize>(config.controlpoint))
                .center;
        for (auto slot : context.slots) {
            auto relative = positions[slot.index].cast<double>() - center;
            auto distance = relative.norm();
            if (distance <= 1e-9) continue;
            if (! states[slot.index].initialized) {
                states[slot.index].distance    = static_cast<float>(distance);
                states[slot.index].initialized = true;
            }
            auto direction       = relative / distance;
            auto current         = velocities[slot.index].cast<double>();
            auto radial_velocity = current.dot(direction);
            auto target_velocity = (static_cast<double>(states[slot.index].distance) - distance) *
                                   static_cast<double>(config.variable_strength);
            velocities[slot.index] =
                (current + direction * (target_velocity - radial_velocity)).cast<float>();
        }
    }
};

// What the collision operators do to a particle on a surface with this normal
// (particles-collisionplane): bounce makes its speed along the normal -bouncefactor * v_n whichever
// way it moves (Wallpaper Engine's kernel adds n (v . n) x factor without looking at the sign, so a
// particle already moving away is turned back: ft-collisionplane), slide 0, delete removes it; the
// sideways speed stays. stop takes all its speed (particles-collisionbounds-single:
// Wallpaper Engine's fourth collision kernel).
enum class CollisionBehavior
{
    Bounce,
    Slide,
    Delete,
    Stop,
};

inline double Abs(double value) { return value < 0.0 ? -value : value; }

inline CollisionBehavior ReadCollisionBehavior(const Json& wpj) {
    String behavior;
    owe::GetJsonValue(wpj, "collisionbehavior"_str, behavior, false);
    return behavior == "slide"_str    ? CollisionBehavior::Slide
           : behavior == "delete"_str ? CollisionBehavior::Delete
           : behavior == "stop"_str   ? CollisionBehavior::Stop
                                      : CollisionBehavior::Bounce;
}

inline Eigen::Vector3d AfterContact(CollisionBehavior behavior, double bounce,
                                    const Eigen::Vector3d& normal, const Eigen::Vector3d& v) {
    if (behavior == CollisionBehavior::Stop) return Eigen::Vector3d::Zero();
    const double along   = normal.dot(v);
    const double rebound = behavior == CollisionBehavior::Slide ? 0.0 : -along * bounce;
    return v + normal * (rebound - along);
}

// the shape a collision operator keeps particles out of, as far as the shape is concerned
struct CollisionState {
    ParticleAttributes attributes;
    CollisionBehavior  behavior { CollisionBehavior::Bounce };
    double             bounce { 0.5 };
    particle::ParticleWriteIndex<particle::VelocityAttribute> velocity;
    particle::ParticleWriteIndex<particle::LifetimeAttribute> lifetime;
    particle::ParticleWriteIndex<particle::AlphaAttribute>    alpha;

    void Compile(particle::ParticleViewCompiler& compiler) {
        compiler.WriteBase(attributes.position);
        velocity = compiler.Write(attributes.velocity);
        lifetime = compiler.Write(attributes.lifetime);
        alpha    = compiler.Write(attributes.alpha);
    }

    // runs `hit(position, velocity, delta) -> Option<normal and the point the particle is put on>`
    // per particle
    template<typename Hit>
    void Collide(particle::ParticleUpdateContext& context, Hit&& hit) {
        auto positions  = context.view.PositionsMut();
        auto velocities = context.view.Write(velocity);
        auto lifetimes  = context.view.Write(lifetime);
        auto alphas     = context.view.Write(alpha);
        auto delta      = context.delta.to_primitive();
        for (auto slot : context.slots) {
            const Eigen::Vector3d position = positions[slot.index].cast<double>();
            const Eigen::Vector3d v        = velocities[slot.index].cast<double>();
            Eigen::Vector3d       normal, on;
            if (! hit(position, v, delta, normal, on)) continue;
            if (behavior == CollisionBehavior::Delete) {
                lifetimes[slot.index] =
                    0.0f; // the lifecycle removes it next frame; unseen till then
                alphas[slot.index] = 0.0f;
                continue;
            }
            positions[slot.index]  = on.cast<float>();
            velocities[slot.index] = AfterContact(behavior, bounce, normal, v).cast<float>();
        }
    }
};

// collisionplane: a particle may not go below the plane normal . x = distance
// (particles-collisionplane).
struct CollisionPlaneOperator {
    CollisionState  state;
    Eigen::Vector3d normal { 0.0, 1.0, 0.0 };
    double          distance { 0.0 };

    void Compile(particle::ParticleViewCompiler& compiler) { state.Compile(compiler); }

    void Update(particle::ParticleUpdateContext& context) {
        state.Collide(context,
                      [&](const Eigen::Vector3d& position,
                          const Eigen::Vector3d&,
                          double,
                          Eigen::Vector3d& n,
                          Eigen::Vector3d& on) {
                          const double depth = normal.dot(position) - distance;
                          if (depth >= 0.0) return false;
                          n  = normal;
                          on = position - normal * depth;
                          return true;
                      });
    }
};

// collisionsphere: particles stay out of the sphere (particles-collisionshapes).
struct CollisionSphereOperator {
    CollisionState  state;
    Eigen::Vector3d origin { 0.0, -150.0, 0.0 };
    double          radius { 100.0 };

    void Compile(particle::ParticleViewCompiler& compiler) { state.Compile(compiler); }

    void Update(particle::ParticleUpdateContext& context) {
        state.Collide(context,
                      [&](const Eigen::Vector3d& position,
                          const Eigen::Vector3d& v,
                          double,
                          Eigen::Vector3d& n,
                          Eigen::Vector3d& on) {
                          Eigen::Vector3d offset   = position - origin;
                          const double    distance = offset.norm();
                          if (distance >= radius) return false;
                          if (distance > 1e-9)
                              n = offset / distance;
                          else
                              n = v.norm() > 1e-9 ? Eigen::Vector3d(v.normalized())
                                                  : Eigen::Vector3d(1.0, 0.0, 0.0);
                          on = origin + n * radius;
                          return true;
                      });
    }
};

// collisionquad: a flat rectangle, size.x across and size.y deep, facing up (its angles do
// nothing), that a particle falling through from above lands on (particles-collisionshapes,
// -quad-sides: one rising through it from below passes).
struct CollisionQuadOperator {
    CollisionState  state;
    Eigen::Vector3d origin { 0.0, -200.0, 0.0 };
    double          width { 200.0 }, depth { 200.0 };

    void Compile(particle::ParticleViewCompiler& compiler) { state.Compile(compiler); }

    void Update(particle::ParticleUpdateContext& context) {
        state.Collide(context,
                      [&](const Eigen::Vector3d& position,
                          const Eigen::Vector3d& v,
                          double                 delta,
                          Eigen::Vector3d&       n,
                          Eigen::Vector3d&       on) {
                          const double below = position.y() - origin.y();
                          if (below >= 0.0 || below - v.y() * delta < 0.0) return false;
                          if (Abs(position.x() - origin.x()) > width * 0.5 ||
                              Abs(position.z() - origin.z()) > depth * 0.5)
                              return false;
                          n  = Eigen::Vector3d(0.0, 1.0, 0.0);
                          on = Eigen::Vector3d(position.x(), origin.y(), position.z());
                          return true;
                      });
    }
};

// collisionbounds: particles stay inside the scene, 0..width across and 0..height up
// (particles-collisionbounds-rest: single particles bouncing between the walls and the ceiling and
// floor at 0.5).
struct CollisionBoundsOperator {
    CollisionState state;

    void Compile(particle::ParticleViewCompiler& compiler) { state.Compile(compiler); }

    void Update(particle::ParticleUpdateContext& context) {
        auto frame = ParticleFrameFrom(context.frame);
        if (frame->scene_extent.x() <= 0.0 || frame->scene_extent.y() <= 0.0) return;
        const Eigen::Matrix4d world_from_local =
            frame->world_space ? Eigen::Matrix4d::Identity()
                               : Eigen::Matrix4d(frame->local_from_world.inverse());
        const Eigen::Matrix4d local_from_world =
            frame->world_space ? Eigen::Matrix4d::Identity() : frame->local_from_world;
        const Eigen::Matrix3d world_dir  = world_from_local.block<3, 3>(0, 0);
        const Eigen::Matrix3d local_dir  = world_dir.inverse();
        const Eigen::Vector2d extent     = frame->scene_extent;
        auto                  positions  = context.view.PositionsMut();
        auto                  velocities = context.view.Write(state.velocity);
        auto                  lifetimes  = context.view.Write(state.lifetime);
        auto                  alphas     = context.view.Write(state.alpha);
        for (auto slot : context.slots) {
            Eigen::Vector3d position =
                (world_from_local * positions[slot.index].cast<double>().homogeneous()).head<3>();
            Eigen::Vector3d velocity = world_dir * velocities[slot.index].cast<double>();
            bool            hit      = false;
            auto            wall     = [&](int axis, double edge, double sign) {
                if ((position[axis] - edge) * sign >= 0.0) return;
                hit                    = true;
                position[axis]         = edge;
                Eigen::Vector3d normal = Eigen::Vector3d::Zero();
                normal[axis]           = sign;
                velocity = AfterContact(state.behavior, state.bounce, normal, velocity);
            };
            wall(0, 0.0, 1.0);
            wall(0, extent.x(), -1.0);
            wall(1, 0.0, 1.0);
            wall(1, extent.y(), -1.0);
            if (! hit) continue;
            if (state.behavior == CollisionBehavior::Delete) {
                lifetimes[slot.index] = 0.0f;
                alphas[slot.index]    = 0.0f;
                continue;
            }
            positions[slot.index] =
                (local_from_world * position.homogeneous()).head<3>().cast<float>();
            velocities[slot.index] = (local_dir * velocity).cast<float>();
        }
    }
};

struct ControlPointAttractOperator {
    ParticleAttributes                                        attributes;
    ControlPointForce                                         config;
    particle::ParticleWriteIndex<particle::VelocityAttribute> velocity;

    void Compile(particle::ParticleViewCompiler& compiler) {
        compiler.ReadBase(attributes.position);
        velocity = compiler.Write(attributes.velocity);
    }

    void Update(particle::ParticleUpdateContext& context) {
        auto frame      = ParticleFrameFrom(context.frame);
        auto positions  = context.view.Positions();
        auto velocities = context.view.Write(velocity);
        auto controlpoint =
            frame->subsystem->SimulationControlpoint(rstd::as_cast<usize>(config.controlpoint));
        Eigen::Vector3d offset =
            controlpoint.center +
            controlpoint.basis * Eigen::Vector3f { config.origin.data() }.cast<double>();
        auto delta = context.delta.to_primitive();
        for (auto slot : context.slots) {
            Eigen::Vector3d difference = offset - positions[slot.index].cast<double>();
            auto            distance   = difference.norm();
            if (distance <= 0.0 || distance >= config.threshold) continue;
            auto impulse = config.scale * delta * (1.0 - distance / config.threshold);
            // WE limits the velocity increment near the control point by default.
            if ((config.flags & u32(2)) != u32()) impulse = rstd::cmp::min(distance, impulse);
            velocities[slot.index] =
                (velocities[slot.index].cast<double>() + difference * (impulse / distance))
                    .cast<float>();
        }
    }
};

// capvelocity: no particle goes faster than maxspeed (particles-capvelocity).
struct CapVelocityOperator {
    ParticleAttributes                                              attributes;
    double                                                          max_speed {};
    ParticleOperatorWindow                                          window;
    particle::ParticleWriteIndex<particle::VelocityAttribute>       velocity;
    particle::ParticleReadIndex<particle::LifetimeAttribute>        lifetime;
    particle::ParticleReadIndex<particle::InitialLifetimeAttribute> initial_lifetime;

    void Compile(particle::ParticleViewCompiler& compiler) {
        velocity         = compiler.Write(attributes.velocity);
        lifetime         = compiler.Read(attributes.lifetime);
        initial_lifetime = compiler.Read(attributes.initial_lifetime);
    }

    void Update(particle::ParticleUpdateContext& context) {
        auto velocities = context.view.Write(velocity);
        auto lifetimes  = context.view.Read(lifetime);
        auto initial    = context.view.Read(initial_lifetime);
        for (auto slot : context.slots) {
            const Eigen::Vector3d v     = velocities[slot.index].cast<double>();
            const double          speed = v.norm();
            if (speed <= max_speed) continue;
            // the window moves the velocity that much of the way to the capped one
            const double weight =
                window.Always()
                    ? 1.0
                    : window.Weight(LifetimePosition(lifetimes[slot.index], initial[slot.index]));
            if (weight <= 0.0) continue;
            const Eigen::Vector3d capped = v * (max_speed / speed);
            velocities[slot.index]       = (v + (capped - v) * weight).cast<float>();
        }
    }
};

// inheritvaluefromevent: the colour and opacity of the particle the system follows, every frame
// (particles-inherit-event: a child of a half-transparent parent is half as bright)
struct InheritValueFromEventOperator {
    ParticleAttributes                                     attributes;
    particle::ParticleWriteIndex<particle::ColorAttribute> color;
    particle::ParticleWriteIndex<particle::AlphaAttribute> alpha;

    void Compile(particle::ParticleViewCompiler& compiler) {
        color = compiler.Write(attributes.color);
        alpha = compiler.Write(attributes.alpha);
    }

    void Update(particle::ParticleUpdateContext& context) {
        auto inherited = ParentParticleLook(*ParticleFrameFrom(context.frame));
        if (inherited.is_none()) return;
        auto colors = context.view.Write(color);
        auto alphas = context.view.Write(alpha);
        for (auto slot : context.slots) {
            colors[slot.index] = inherited->color;
            alphas[slot.index] = inherited->alpha;
        }
    }
};

// The CPU's approximate reciprocal square root (rsqrtss: about 12 bits), which Wallpaper Engine
// normalises some vectors with; without SSE, the exact one.
static float ApproximateReciprocalSqrt(float value) {
#if defined(__SSE__)
    return _mm_cvtss_f32(_mm_rsqrt_ss(_mm_set_ss(value)));
#else
    return 1.0f / Eigen::numext::sqrt(value);
#endif
}

// maintaindistancebetweencontrolpoints (particles-maintain-between, particles-maintain-drift):
// along the axis from control point 0 to control point 1 a particle is held between them (clamped
// to the nearer end); its place across the axis does not change. Wallpaper Engine normalises the
// axis with the approximate reciprocal square root and projects with it, so every frame (not every
// second) the place along the axis is scaled by |axis|^2: 1 - 3.36e-4 for control points 100, 200
// or 400 apart, 1 - 1.45e-4 for 150 or 300, 1 + 4.2e-4 for a 45 degree axis 200 long (moving away
// from control point 0).
struct MaintainDistanceBetweenOperator {
    ParticleAttributes attributes;

    void Compile(particle::ParticleViewCompiler& compiler) {
        compiler.WriteBase(attributes.position);
    }

    void Update(particle::ParticleUpdateContext& context) {
        auto                  frame     = ParticleFrameFrom(context.frame);
        auto                  positions = context.view.PositionsMut();
        const auto            first     = frame->subsystem->SimulationControlpoint(usize()).center;
        const auto            second    = frame->subsystem->SimulationControlpoint(usize(1)).center;
        const Eigen::Vector3d span      = second - first;
        const double          length    = span.norm();
        if (length <= 1e-9) return;
        const Eigen::Vector3d axis = span / length;
        const Eigen::Vector3f approx =
            span.cast<float>() * ApproximateReciprocalSqrt(span.cast<float>().squaredNorm());
        const double scale = approx.squaredNorm();
        for (auto slot : context.slots) {
            const Eigen::Vector3d relative = positions[slot.index].cast<double>() - first;
            const double          along    = relative.dot(axis);
            const double          held = rstd::cmp::min(rstd::cmp::max(along, 0.0), length) * scale;
            positions[slot.index]      = (first + relative + axis * (held - along)).cast<float>();
        }
    }
};

// reducemovementnearcontrolpoint: a drag near a control point (particles-reduce-near, particles-reduce-near-rest).
struct ReduceMovementNearControlPointOperator {
    ParticleAttributes attributes;
    i32                controlpoint { 0 };
    double             inner { 0 }, outer { 0 };
    double             reduction_inner { 0 }, reduction_outer { 0 };
    particle::ParticleWriteIndex<particle::VelocityAttribute> velocity;

    void Compile(particle::ParticleViewCompiler& compiler) {
        compiler.ReadBase(attributes.position);
        velocity = compiler.Write(attributes.velocity);
    }

    void Update(particle::ParticleUpdateContext& context) {
        auto frame      = ParticleFrameFrom(context.frame);
        auto positions  = context.view.Positions();
        auto velocities = context.view.Write(velocity);
        auto center =
            frame->subsystem->SimulationControlpoint(rstd::as_cast<usize>(controlpoint)).center;
        // Wallpaper Engine (FUN_14023fbc0 case 13): the reduction goes linearly with the distance from reductioninner
        // at distanceinner to reductionouter at distanceouter, held at the end values beyond them, and a frame takes
        // min(reduction dt, 1) of the speed
        auto         delta = context.delta.to_primitive();
        const double span  = outer - inner;
        for (auto slot : context.slots) {
            const double distance = (positions[slot.index].cast<double>() - center).norm();
            const double t =
                span > 0.0 ? rstd::cmp::min(rstd::cmp::max((distance - inner) / span, 0.0), 1.0)
                           : (distance < inner ? 0.0 : 1.0);
            const double reduction = reduction_inner + (reduction_outer - reduction_inner) * t;
            const double taken     = rstd::cmp::min(rstd::cmp::max(reduction * delta, 0.0), 1.0);
            velocities[slot.index] = (velocities[slot.index].cast<double>() * (1.0 - taken)).cast<float>();
        }
    }
};

struct RemapValueOperator {
    ParticleAttributes                                              attributes;
    ParticleRemap                                                   remap;
    particle::ParticleWriteIndex<particle::ColorAttribute>          color;
    particle::ParticleWriteIndex<particle::AlphaAttribute>          alpha;
    particle::ParticleWriteIndex<particle::SizeAttribute>           size;
    particle::ParticleReadIndex<particle::LifetimeAttribute>        lifetime;
    particle::ParticleReadIndex<particle::InitialLifetimeAttribute> initial_lifetime;
    particle::ParticleReadIndex<particle::VelocityAttribute>        velocity;
    particle::ParticleReadIndex<particle::AlphaAttribute>           input_alpha;
    particle::ParticleWriteIndex<particle::VelocityAttribute>       output_velocity;
    particle::ParticleReadIndex<particle::RotationAttribute>        rotation;
    particle::ParticleReadIndex<particle::AngularVelocityAttribute> angular_velocity;

    void Compile(particle::ParticleViewCompiler& compiler) {
        compiler.ReadBase(attributes.position);
        lifetime         = compiler.Read(attributes.lifetime);
        initial_lifetime = compiler.Read(attributes.initial_lifetime);
        rotation         = compiler.Read(attributes.rotation);
        angular_velocity = compiler.Read(attributes.angular_velocity);
        // the output attribute is written, so an input from the same attribute reads it there
        if (remap.output != ParticleRemap::Output::Velocity)
            velocity = compiler.Read(attributes.velocity);
        if (remap.output != ParticleRemap::Output::Opacity)
            input_alpha = compiler.Read(attributes.alpha);
        switch (remap.output) {
        case ParticleRemap::Output::Color: color = compiler.Write(attributes.color); break;
        case ParticleRemap::Output::Opacity: alpha = compiler.Write(attributes.alpha); break;
        case ParticleRemap::Output::Size: size = compiler.Write(attributes.size); break;
        case ParticleRemap::Output::Velocity:
            output_velocity = compiler.Write(attributes.velocity);
            break;
        }
    }

    void Update(particle::ParticleUpdateContext& context) {
        auto                  frame     = ParticleFrameFrom(context.frame);
        auto                  positions = context.view.Positions();
        auto                  lifetimes = context.view.Read(lifetime);
        auto                  initial   = context.view.Read(initial_lifetime);
        auto                  rotations = context.view.Read(rotation);
        auto                  spins     = context.view.Read(angular_velocity);
        const Eigen::Vector3d center =
            frame->subsystem->SimulationControlpoint(rstd::as_cast<usize>(remap.controlpoint))
                .center;
        const Eigen::Vector3d center1 =
            frame->subsystem->SimulationControlpoint(rstd::as_cast<usize>(remap.controlpoint1))
                .center;
        const Eigen::Vector3d frame_input = RemapFrameInput(remap, *frame);
        auto                  input       = [&](particle::ParticleSlot slot) -> Eigen::Vector3d {
            switch (remap.input) {
            case ParticleRemap::Input::Lifetime:
                return Eigen::Vector3d::Constant(
                    LifetimePosition(lifetimes[slot.index], initial[slot.index]));
            case ParticleRemap::Input::DistanceToControlPoint:
            case ParticleRemap::Input::DeltaToControlPoint:
            case ParticleRemap::Input::DirectionToControlPoint:
            case ParticleRemap::Input::PositionBetweenControlPoints:
                return RemapControlpointInput(
                    remap, positions[slot.index].cast<double>(), center, center1);
            case ParticleRemap::Input::Velocity:
                return remap.output == ParticleRemap::Output::Velocity
                           ? Eigen::Vector3d(
                                 context.view.Write(output_velocity)[slot.index].cast<double>())
                           : Eigen::Vector3d(
                                 context.view.Read(velocity)[slot.index].cast<double>());
            case ParticleRemap::Input::Opacity:
                return Eigen::Vector3d::Constant(remap.output == ParticleRemap::Output::Opacity
                                                     ? context.view.Write(alpha)[slot.index]
                                                     : context.view.Read(input_alpha)[slot.index]);
            case ParticleRemap::Input::Rotation:
                return Eigen::Vector3d::Constant(rotations[slot.index].z());
            case ParticleRemap::Input::AngularSpeed:
                return Eigen::Vector3d::Constant(spins[slot.index].z());
            default: return frame_input;
            }
        };
        auto life = [&](particle::ParticleSlot slot) {
            return LifetimePosition(lifetimes[slot.index], initial[slot.index]);
        };
        switch (remap.output) {
        case ParticleRemap::Output::Color: {
            auto values = context.view.Write(color);
            for (auto slot : context.slots)
                values[slot.index] =
                    remap.Blend(values[slot.index].cast<double>(), input(slot), life(slot))
                        .cast<float>();
            break;
        }
        case ParticleRemap::Output::Opacity: {
            auto values = context.view.Write(alpha);
            for (auto slot : context.slots)
                values[slot.index] = float(remap
                                               .Blend(Eigen::Vector3d::Constant(values[slot.index]),
                                                      input(slot),
                                                      life(slot))
                                               .x());
            break;
        }
        case ParticleRemap::Output::Size: {
            auto values = context.view.Write(size);
            for (auto slot : context.slots)
                values[slot.index] = float(remap
                                               .Blend(Eigen::Vector3d::Constant(values[slot.index]),
                                                      input(slot),
                                                      life(slot))
                                               .x());
            break;
        }
        case ParticleRemap::Output::Velocity: {
            // added or taken each frame, whatever its length (particles-remap-velocity: add 0.01
            // gave 0.01 a frame)
            auto values = context.view.Write(output_velocity);
            for (auto slot : context.slots)
                values[slot.index] =
                    remap.Blend(values[slot.index].cast<double>(), input(slot), life(slot))
                        .cast<float>();
            break;
        }
        }
    }
};

// boids (particles-boids), as Wallpaper Engine runs it: particles are taken four at a time and each
// one looks at every particle of every group, including the unused places of the last group of
// four, which count as particles at rest at the system's origin. A neighbour closer than
// separationthreshold pushes away by (threshold - distance) and one closer than neighborthreshold
// adds its velocity and position; the separation push and the differences from the mean velocity
// and position are averaged (with the CPU's approximate reciprocals) and scaled by their factors
// and dt2, the time step shortened below 40 frames a second by (0.025 / frame time)^0.7. Above 200
// particles only every n-th group is updated a frame (n = count / 200 + 1), looking at every n-th
// group, with the factors multiplied by n. With flags bit 1 a particle that speeds up past maxspeed
// is slowed to it (unless it was already faster).
struct Boids {
    float separation_threshold { 20.0f };
    float neighbor_threshold { 50.0f };
    float max_speed { 500.0f };
    float separation_factor { 15.0f };
    float alignment_factor { 1.0f };
    float cohesion_factor { 2.0f };
    u32   flags { 0 };

    static auto ReadFromJson(const Json& json) -> Boids {
        Boids value;
        owe::GetJsonValue(json, "separationthreshold"_str, value.separation_threshold, false);
        owe::GetJsonValue(json, "neighborthreshold"_str, value.neighbor_threshold, false);
        owe::GetJsonValue(json, "maxspeed"_str, value.max_speed, false);
        owe::GetJsonValue(json, "separationfactor"_str, value.separation_factor, false);
        owe::GetJsonValue(json, "alignmentfactor"_str, value.alignment_factor, false);
        owe::GetJsonValue(json, "cohesionfactor"_str, value.cohesion_factor, false);
        owe::GetJsonValue(json, "flags"_str, value.flags, false);
        return value;
    }
};

struct BoidsOperator {
    ParticleAttributes                                        attributes;
    Boids                                                     config;
    particle::ParticleWriteIndex<particle::VelocityAttribute> velocity;
    rstd::size_t                                              frame {};

    static float Reciprocal(float value) {
#if defined(__SSE__)
        return _mm_cvtss_f32(_mm_rcp_ss(_mm_set_ss(value)));
#else
        return 1.0f / value;
#endif
    }

    static float ReciprocalSqrt(float value) {
#if defined(__SSE__)
        return _mm_cvtss_f32(_mm_rsqrt_ss(_mm_set_ss(value)));
#else
        return 1.0f / Eigen::numext::sqrt(value);
#endif
    }

    void Compile(particle::ParticleViewCompiler& compiler) {
        compiler.ReadBase(attributes.position);
        velocity = compiler.Write(attributes.velocity);
    }

    void Update(particle::ParticleUpdateContext& context) {
        using index_t         = rstd::size_t;
        auto       positions  = context.view.Positions();
        auto       velocities = context.view.Write(velocity);
        Vec<usize> slots;
        for (auto slot : context.slots) slots.push(usize(slot.index.to_primitive()));
        const index_t count = slots.len().to_primitive();
        if (count == 0) return;
        const index_t lanes = (count + 3) / 4 * 4;
        // the particles and the empty places of the last group, as Wallpaper Engine lays them out
        Vec<Eigen::Vector3f> place;
        Vec<Eigen::Vector3f> speed;
        for (index_t lane = 0; lane < lanes; ++lane) {
            if (lane < count) {
                place.push(Eigen::Vector3f(positions[slots[usize(lane)]]));
                speed.push(Eigen::Vector3f(velocities[slots[usize(lane)]]));
            } else {
                place.push(Eigen::Vector3f::Zero());
                speed.push(Eigen::Vector3f::Zero());
            }
        }
        const float   scaled = static_cast<float>(context.delta2.to_primitive());
        const index_t groups = lanes / 4;
        const index_t stride = count / 200 + 1;
        const index_t phase  = static_cast<index_t>(frame % stride);
        const float   step   = scaled * static_cast<float>(stride);
        const float   push   = config.separation_factor * step;
        const float   align  = config.alignment_factor * step;
        const float   pull   = config.cohesion_factor * step;
        const float   limit2 = config.max_speed * config.max_speed;
        for (index_t group = phase; group < groups; group += stride) {
            // the four of a group are worked out together from the velocities before the group
            Eigen::Vector3f result[4];
            for (index_t lane = group * 4; lane < group * 4 + 4; ++lane) {
                const Eigen::Vector3f here       = place[usize(lane)];
                Eigen::Vector3f       apart      = Eigen::Vector3f::Zero(),
                                      mean_speed = Eigen::Vector3f::Zero(),
                                      mean_place = Eigen::Vector3f::Zero();
                float                 near {}, neighbours {};
                const index_t         first = static_cast<index_t>((frame + group * 4) % stride);
                for (index_t other_group = first; other_group < groups; other_group += stride) {
                    for (index_t other = other_group * 4; other < other_group * 4 + 4; ++other) {
                        const Eigen::Vector3f d  = here - place[usize(other)];
                        const float           d2 = d.squaredNorm();
                        if (d2 == 0.0f) continue;
                        const float inverse  = ReciprocalSqrt(d2);
                        const float distance = inverse * d2;
                        if (distance < config.separation_threshold) {
                            apart += d * (inverse * config.separation_threshold - 1.0f);
                            near += 1.0f;
                        }
                        if (distance < config.neighbor_threshold) {
                            mean_speed += speed[usize(other)];
                            mean_place += place[usize(other)];
                            neighbours += 1.0f;
                        }
                    }
                }
                const Eigen::Vector3f before = speed[usize(lane)];
                Eigen::Vector3f       change = Eigen::Vector3f::Zero();
                if (near != 0.0f) change += apart * (Reciprocal(near) * push);
                if (neighbours != 0.0f) {
                    const float share = Reciprocal(neighbours);
                    change +=
                        (mean_speed * share - before) * align + (mean_place * share - here) * pull;
                }
                Eigen::Vector3f next = before + change;
                if ((config.flags & u32(1)) != u32()) {
                    const float next2 = next.squaredNorm();
                    if (rstd::cmp::max(before.squaredNorm(), limit2) < next2)
                        next *= ReciprocalSqrt(next2) * config.max_speed;
                }
                result[lane - group * 4] = next;
            }
            for (index_t lane = group * 4; lane < group * 4 + 4; ++lane) {
                speed[usize(lane)] = result[lane - group * 4];
                if (lane < count) velocities[slots[usize(lane)]] = speed[usize(lane)];
            }
        }
        if (context.last_pass) ++frame;
    }
};

struct NoopUpdateOperator {
    void Compile(particle::ParticleViewCompiler&) {}
    void Update(particle::ParticleUpdateContext&) {}
};

Box<dyn<particle::ParticleUpdateProgram>>
ParticleParser::GenOperator(const Json& wpj, ParticleInstanceModifiers modifiers,
                            ParticleSubSystem& subsystem, usize operator_index) {
    auto attributes = subsystem.Attributes();
    do {
        if (wpj.get("name"_str).is_none()) break;
        String name;
        owe::GetJsonValue(wpj, "name"_str, name);
        if (name == "movement"_str) {
            float           drag { 0.0f };
            array<float, 3> gravity { 0.0f, 0.0f, 0.0f };
            owe::GetJsonValue(wpj, "drag"_str, drag, false);
            owe::GetJsonValue(wpj, "gravity"_str, gravity, false);
            return Box<dyn<particle::ParticleUpdateProgram>>::make(MovementOperator {
                .attributes = attributes,
                .drag       = drag,
                .gravity    = Vector3f(gravity.data()).cast<double>(),
                .modifiers  = modifiers.Clone(),
            });
        } else if (name == "angularmovement"_str) {
            float           drag { 0.0f };
            array<float, 3> force { 0.0f, 0.0f, 0.0f };
            owe::GetJsonValue(wpj, "drag"_str, drag, false);
            owe::GetJsonValue(wpj, "force"_str, force, false);
            return Box<dyn<particle::ParticleUpdateProgram>>::make(AngularMovementOperator {
                .attributes = attributes,
                .drag       = drag,
                .force      = Vector3f(force.data()).cast<double>(),
            });
        } else if (name == "sizechange"_str) {
            return Box<dyn<particle::ParticleUpdateProgram>>::make(ScalarChangeOperator {
                .attributes = attributes,
                .change     = ValueChange::ReadFromJson(wpj),
                .target     = ScalarChangeOperator::Target::Size,
                .modifiers  = modifiers.Clone(),
            });
        } else if (name == "alphafade"_str) {
            float fadeintime { 0.5f }, fadeouttime { 0.5f };
            owe::GetJsonValue(wpj, "fadeintime"_str, fadeintime, false);
            owe::GetJsonValue(wpj, "fadeouttime"_str, fadeouttime, false);
            return Box<dyn<particle::ParticleUpdateProgram>>::make(AlphaFadeOperator {
                .attributes = attributes,
                .fade_in    = fadeintime,
                .fade_out   = fadeouttime,
            });
        } else if (name == "alphachange"_str) {
            return Box<dyn<particle::ParticleUpdateProgram>>::make(ScalarChangeOperator {
                .attributes = attributes,
                .change     = ValueChange::ReadFromJson(wpj),
                .target     = ScalarChangeOperator::Target::Alpha,
                .modifiers  = modifiers.Clone(),
            });
        } else if (name == "colorchange"_str) {
            return Box<dyn<particle::ParticleUpdateProgram>>::make(ColorChangeOperator {
                .attributes = attributes,
                .change     = VecChange::ReadFromJson(wpj),
            });
        } else if (name == "oscillatealpha"_str) {
            return Box<dyn<particle::ParticleUpdateProgram>>::make(OscillateScalarOperator {
                .attributes = attributes,
                .state_attributes =
                    RegisterOscillationAttributes(subsystem, operator_index, "alpha"_str),
                .frequency = FrequencyValue::ReadFromJson(wpj, name.as_str()),
                .target    = OscillateScalarOperator::Target::Alpha,
            });
        } else if (name == "oscillatesize"_str) {
            return Box<dyn<particle::ParticleUpdateProgram>>::make(OscillateScalarOperator {
                .attributes = attributes,
                .state_attributes =
                    RegisterOscillationAttributes(subsystem, operator_index, "size"_str),
                .frequency = FrequencyValue::ReadFromJson(wpj, name.as_str()),
                .target    = OscillateScalarOperator::Target::Size,
            });
        } else if (name == "oscillateposition"_str) {
            auto frequency = FrequencyValue::ReadFromJson(wpj, name.as_str());
            return Box<dyn<particle::ParticleUpdateProgram>>::make(
                OscillatePositionOperator {
                    .attributes  = attributes,
                    .frequencies = { frequency, frequency, frequency },
                    .state_attributes = {
                        RegisterOscillationAttributes(subsystem, operator_index, "position_x"_str),
                        RegisterOscillationAttributes(subsystem, operator_index, "position_y"_str),
                        RegisterOscillationAttributes(subsystem, operator_index, "position_z"_str),
                    },
                    .window = ParticleOperatorWindow::FromJson(wpj),
                });
        } else if (name == "turbulence"_str) {
            auto config = Turbulence::ReadFromJson(wpj);
            return Box<dyn<particle::ParticleUpdateProgram>>::make(TurbulenceOperator {
                .attributes       = attributes,
                .config           = config,
                .modifiers        = modifiers.Clone(),
                .state_attributes = RegisterOscillationAttributes(subsystem, operator_index,
                                                                  "turbulence"_str),
                .window           = ParticleOperatorWindow::FromJson(wpj),
            });
        } else if (name == "vortex"_str) {
            return Box<dyn<particle::ParticleUpdateProgram>>::make(VortexOperator {
                .attributes = attributes,
                .config     = Vortex::ReadFromJson(wpj),
            });
        } else if (name == "maintaindistancebetweencontrolpoints"_str) {
            return Box<dyn<particle::ParticleUpdateProgram>>::make(
                MaintainDistanceBetweenOperator { .attributes = attributes });
        } else if (name == "vortex_v2"_str) {
            return Box<dyn<particle::ParticleUpdateProgram>>::make(VortexOperator {
                .attributes = attributes,
                .config     = Vortex::ReadFromJson(wpj),
                .extended   = true,
            });
        } else if (name == "maintaindistancetocontrolpoint"_str) {
            return Box<dyn<particle::ParticleUpdateProgram>>::make(MaintainDistanceOperator {
                .attributes = attributes,
                .config     = MaintainDistance::ReadFromJson(wpj),
                .state_key  = RegisterMaintainDistanceAttribute(subsystem, operator_index),
            });
        } else if (name == "collisionplane"_str || name == "collisionsphere"_str ||
                   name == "collisionquad"_str) {
            CollisionState state { .attributes = attributes,
                                   .behavior   = ReadCollisionBehavior(wpj) };
            // a missing bouncefactor is 0.5 (Wallpaper Engine fills it in: 0x1401c00a0)
            float          bounce { 0.5f };
            owe::GetJsonValue(wpj, "bouncefactor"_str, bounce, false);
            state.bounce = double(bounce);
            if (name == "collisionplane"_str) {
                array<float, 3> plane { 0.0f, 1.0f, 0.0f };
                float           distance { 0.0f };
                owe::GetJsonValue(wpj, "plane"_str, plane, false);
                owe::GetJsonValue(wpj, "distance"_str, distance, false);
                Eigen::Vector3d normal(plane[usize(0)], plane[usize(1)], plane[usize(2)]);
                if (normal.squaredNorm() < 1e-12) normal = Eigen::Vector3d(0.0, 1.0, 0.0);
                normal.normalize();
                return Box<dyn<particle::ParticleUpdateProgram>>::make(CollisionPlaneOperator {
                    .state = rstd::move(state), .normal = normal, .distance = double(distance) });
            }
            array<float, 3> origin { 0.0f, name == "collisionplane"_str ? 0.0f : -150.0f, 0.0f };
            if (name == "collisionquad"_str) origin = { 0.0f, -200.0f, 0.0f };
            owe::GetJsonValue(wpj, "origin"_str, origin, false);
            const Eigen::Vector3d center(origin[usize(0)], origin[usize(1)], origin[usize(2)]);
            if (name == "collisionsphere"_str) {
                float radius { 100.0f };
                owe::GetJsonValue(wpj, "radius"_str, radius, false);
                return Box<dyn<particle::ParticleUpdateProgram>>::make(CollisionSphereOperator {
                    .state = rstd::move(state), .origin = center, .radius = double(radius) });
            }
            array<float, 2> size { 200.0f, 200.0f };
            owe::GetJsonValue(wpj, "size"_str, size, false);
            return Box<dyn<particle::ParticleUpdateProgram>>::make(
                CollisionQuadOperator { .state  = rstd::move(state),
                                        .origin = center,
                                        .width  = double(size[usize(0)]),
                                        .depth  = double(size[usize(1)]) });
        } else if (name == "collisionbounds"_str) {
            CollisionState state { .attributes = attributes,
                                   .behavior   = ReadCollisionBehavior(wpj) };
            // a missing bouncefactor is 0.5 (Wallpaper Engine fills it in: 0x1401c00a0)
            float          bounce { 0.5f };
            owe::GetJsonValue(wpj, "bouncefactor"_str, bounce, false);
            state.bounce = double(bounce);
            return Box<dyn<particle::ParticleUpdateProgram>>::make(
                CollisionBoundsOperator { .state = rstd::move(state) });
        } else if (name == "inheritvaluefromevent"_str) {
            return Box<dyn<particle::ParticleUpdateProgram>>::make(
                InheritValueFromEventOperator { .attributes = attributes });
        } else if (name == "boids"_str) {
            return Box<dyn<particle::ParticleUpdateProgram>>::make(BoidsOperator {
                .attributes = attributes,
                .config     = Boids::ReadFromJson(wpj),
            });
        } else if (name == "controlpointattract"_str) {
            return Box<dyn<particle::ParticleUpdateProgram>>::make(ControlPointAttractOperator {
                .attributes = attributes,
                .config     = ControlPointForce::ReadFromJson(wpj),
            });
        } else if (name == "reducemovementnearcontrolpoint"_str) {
            i32   controlpoint { 0 };
            float inner { 0.0f }, outer { 0.0f }, reduction_inner { 0.0f },
                reduction_outer { 0.0f };
            owe::GetJsonValue(wpj, "controlpoint"_str, controlpoint, false);
            owe::GetJsonValue(wpj, "distanceinner"_str, inner, false);
            owe::GetJsonValue(wpj, "distanceouter"_str, outer, false);
            owe::GetJsonValue(wpj, "reductioninner"_str, reduction_inner, false);
            owe::GetJsonValue(wpj, "reductionouter"_str, reduction_outer, false);
            return Box<dyn<particle::ParticleUpdateProgram>>::make(
                ReduceMovementNearControlPointOperator {
                    .attributes      = attributes,
                    .controlpoint    = controlpoint % i32(8),
                    .inner           = double(inner),
                    .outer           = rstd::cmp::max(double(outer), double(inner)),
                    .reduction_inner = double(reduction_inner),
                    .reduction_outer = double(reduction_outer),
                });
        } else if (name == "remapvalue"_str) {
            auto remap = ParticleRemap::FromJson(wpj);
            if (remap.is_none()) break;
            return Box<dyn<particle::ParticleUpdateProgram>>::make(RemapValueOperator {
                .attributes = attributes,
                .remap      = rstd::move(*remap),
            });
        } else if (name == "capvelocity"_str) {
            float max_speed { 0.0f };
            owe::GetJsonValue(wpj, "maxspeed"_str, max_speed, false);
            return Box<dyn<particle::ParticleUpdateProgram>>::make(CapVelocityOperator {
                .attributes = attributes,
                .max_speed  = rstd::cmp::max(0.0, double(max_speed)),
                .window     = ParticleOperatorWindow::FromJson(wpj),
            });
        }
    } while (false);
    return Box<dyn<particle::ParticleUpdateProgram>>::make(NoopUpdateOperator {});
}

Box<dyn<particle::ParticleEmitterProgram>>
ParticleParser::GenEmitter(const wpscene::Emitter& wpe, ParticleSubSystem& subsystem,
                           usize emitter_index, Option<Arc<LayerImageSource>> layer_image) {
    ParticleAudioResponse audio_response {
        .enable    = wpe.audioprocessingmode != u32(),
        .amount    = wpe.audioamount,
        .exponent  = wpe.audioexponent,
        .frequency = wpe.audiofrequency,
        .bounds    = wpe.audiobounds,
    };
    if (wpe.name == "boxrandom"_str) {
        ParticleBoxEmitterArgs box;
        box.emit_speed     = wpe.rate;
        box.min_distance   = wpe.distancemin;
        box.max_distance   = wpe.distancemax;
        box.directions     = wpe.directions;
        box.origin         = wpe.origin;
        box.one_per_frame  = wpe.flags[wpscene::Emitter::FlagEnum::one_per_frame];
        box.instantaneous  = wpe.instantaneous;
        box.min_speed      = wpe.speedmin;
        box.max_speed      = wpe.speedmax;
        box.duration       = wpe.duration;
        box.timing         = ParticleEmitterTiming::From(wpe);
        box.controlpoint   = wpe.controlpoint;
        box.audio_response = audio_response;
        return Box<dyn<particle::ParticleEmitterProgram>>::make(
            BoxEmitterProgram(subsystem.SpawnPipeline(), rstd::move(box), emitter_index));
    } else if (wpe.name == "layerimage"_str && layer_image.is_some()) {
        ParticleLayerImageEmitterArgs image;
        image.emit_speed     = wpe.rate;
        image.one_per_frame  = wpe.flags[wpscene::Emitter::FlagEnum::one_per_frame];
        image.instantaneous  = wpe.instantaneous;
        image.duration       = wpe.duration;
        image.controlpoint   = wpe.controlpoint;
        image.audio_response = audio_response;
        image.source         = rstd::move(layer_image);
        return Box<dyn<particle::ParticleEmitterProgram>>::make(
            LayerImageEmitterProgram(subsystem.SpawnPipeline(), rstd::move(image), emitter_index));
    } else if (wpe.name == "sphererandom"_str) {
        ParticleSphereEmitterArgs sphere;
        sphere.emit_speed     = wpe.rate;
        sphere.min_distance   = wpe.distancemin[usize(0)];
        sphere.max_distance   = wpe.distancemax[usize(0)];
        sphere.directions     = wpe.directions;
        sphere.origin         = wpe.origin;
        sphere.sign           = wpe.sign;
        sphere.one_per_frame  = wpe.flags[wpscene::Emitter::FlagEnum::one_per_frame];
        sphere.instantaneous  = wpe.instantaneous;
        sphere.min_speed      = wpe.speedmin;
        sphere.max_speed      = wpe.speedmax;
        sphere.duration       = wpe.duration;
        sphere.timing         = ParticleEmitterTiming::From(wpe);
        sphere.controlpoint   = wpe.controlpoint;
        sphere.audio_response = audio_response;
        return Box<dyn<particle::ParticleEmitterProgram>>::make(
            SphereEmitterProgram(subsystem.SpawnPipeline(), rstd::move(sphere), emitter_index));
    }

    struct NoopEmitter {
        void Compile(particle::ParticleViewCompiler&) {}
        void Emit(particle::ParticleEmitterContext&) {}
    };
    return Box<dyn<particle::ParticleEmitterProgram>>::make(NoopEmitter {});
}
