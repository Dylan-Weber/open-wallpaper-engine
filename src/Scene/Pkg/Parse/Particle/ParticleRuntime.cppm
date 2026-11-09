module;

#include <cmath>

export module wescene.pkg.parse:particle_runtime;

import eigen;
import rstd;
import wescene.core;
import wescene.json;
import wescene.particle;
import wescene.particle.program;
import wescene.scene;

export import wescene.pkg.scene_obj;

using namespace rstd::prelude;
using namespace rstd::literals;
using rstd::sync::Arc;
using namespace rstd::literals;

export namespace owe
{

enum class ParticleAnimationMode
{
    SEQUENCE,
    RANDOMONE,
};

// A colour from hue (0 to 1 around the circle, wrapping), saturation and value.
inline auto HsvToRgb(double hue, double saturation, double value) -> Eigen::Vector3d {
    hue = hue - static_cast<double>(static_cast<long long>(hue));
    if (hue < 0.0) hue += 1.0;
    const double h = hue * 6.0;
    const int    i = static_cast<int>(h) % 6;
    const double f = h - static_cast<double>(static_cast<int>(h));
    const double p = value * (1.0 - saturation), q = value * (1.0 - saturation * f),
                 t = value * (1.0 - saturation * (1.0 - f));
    switch (i) {
    case 0: return { value, t, p };
    case 1: return { q, value, p };
    case 2: return { p, value, t };
    case 3: return { p, q, value };
    case 4: return { t, p, value };
    default: return { value, p, q };
    }
}

// remapvalue: maps an input (the distance to a control point, or how far through its life a
// particle is) from inputrangemin..max, clamped, linearly onto outputrangemin..max, and sets a
// particle's colour, opacity or size to it, or multiplies or adds it (particles-remapvalue). A size
// it sets is a radius: the particle is drawn twice as large. Other inputs, outputs and the noise
// transforms are not read: such an operator does nothing. A remapvalue can be limited to part of a
// particle's life (blend windows, BlendWeight).
// (Inputs: see Input; scalar ones give every axis the same value, so a colour of them is grey.)
struct ParticleRemap {
    enum class Input
    {
        DistanceToControlPoint,
        // from the particle to a control point (a vector), its direction (unit length), and how far
        // along the line from one control point to another the particle is, by its shadow on the
        // line (0 at the first, 1 at the second)
        DeltaToControlPoint,
        DirectionToControlPoint,
        PositionBetweenControlPoints,
        Lifetime,
        // the particle's speed along each axis, its opacity, rotation (about z) and angular speed
        // (about z)
        Velocity,
        Opacity,
        Rotation,
        AngularSpeed,
        // the object's position
        LayerOrigin,
        // seconds: the particle system's, the layer's, the engine's; and the local time of day as a
        // fraction
        SystemTime,
        LayerTime,
        Runtime,
        TimeOfDay,
    };
    enum class Output
    {
        Color,
        Opacity,
        Size,
        Velocity,
    };
    enum class Operation
    {
        Remap,
        Multiply,
        Add,
        Subtract,
    };
    // the shape the fraction of the input range goes through (particles-remap-transform): triangle
    // and square repeat twice over the range, sine once
    enum class Transform
    {
        Linear,
        Triangle,
        Sine,
        Square,
    };
    Input           input { Input::DistanceToControlPoint };
    i32             controlpoint { 0 };
    i32             controlpoint1 { 1 };
    double          input_min { 0.0 }, input_max { 1.0 };
    Output          output { Output::Color };
    Operation       operation { Operation::Remap };
    Transform       transform { Transform::Linear };
    Eigen::Vector3d output_min { Eigen::Vector3d::Zero() }, output_max { Eigen::Vector3d::Ones() };
    // transforminputscale: the sine goes scale / 2 times round over the input range
    // (particles-remap-timescale-range); the linear transform ignores it
    double transform_scale { 2.0 };
    // blendinstart..blendinend and blendoutstart..blendoutend, fractions of the particle's life
    // over which the remapped value is blended in from the particle's own and back out
    // (particles-remap-timescale); without them it is applied all the time
    Option<array<double, 2>> blend_in, blend_out;

    static auto FromJson(const Json& json) -> Option<ParticleRemap> {
        ParticleRemap remap;
        String        text;
        GetJsonValue(json, "transforminputscale"_str, remap.transform_scale, false);
        auto window = [&](ref<str> start, ref<str> end) -> Option<array<double, 2>> {
            if (json.get(start).is_none() && json.get(end).is_none()) return None();
            array<double, 2> range { 0.0, 0.0 };
            GetJsonValue(json, start, range[usize(0)], false);
            GetJsonValue(json, end, range[usize(1)], false);
            return Some(range);
        };
        remap.blend_in  = window("blendinstart"_str, "blendinend"_str);
        remap.blend_out = window("blendoutstart"_str, "blendoutend"_str);
        if (json.get("transformfunction"_str).is_some()) {
            GetJsonValue(json, "transformfunction"_str, text, false);
            if (text == "triangle"_str)
                remap.transform = Transform::Triangle;
            else if (text == "sine"_str)
                remap.transform = Transform::Sine;
            else if (text == "square"_str)
                remap.transform = Transform::Square;
            else if (text != "linear"_str)
                return None();
        }
        text = String {};
        GetJsonValue(json, "input"_str, text, false);
        if (text == "distancetocontrolpoint"_str)
            remap.input = Input::DistanceToControlPoint;
        else if (text == "deltatocontrolpoint"_str)
            remap.input = Input::DeltaToControlPoint;
        else if (text == "directiontocontrolpoint"_str)
            remap.input = Input::DirectionToControlPoint;
        else if (text == "positionbetweentwocontrolpoints"_str)
            remap.input = Input::PositionBetweenControlPoints;
        else if (text == "lifetimefraction"_str || text == "particlelifetime"_str)
            remap.input = Input::Lifetime;
        else if (text == "velocity"_str)
            remap.input = Input::Velocity;
        else if (text == "opacity"_str)
            remap.input = Input::Opacity;
        else if (text == "rotation"_str)
            remap.input = Input::Rotation;
        else if (text == "angularspeed"_str)
            remap.input = Input::AngularSpeed;
        else if (text == "layerorigin"_str)
            remap.input = Input::LayerOrigin;
        else if (text == "particlesystemtime"_str)
            remap.input = Input::SystemTime;
        else if (text == "layertime"_str)
            remap.input = Input::LayerTime;
        else if (text == "runtime"_str)
            remap.input = Input::Runtime;
        else if (text == "timeofday"_str)
            remap.input = Input::TimeOfDay;
        else
            return None();
        text = String {};
        GetJsonValue(json, "output"_str, text, false);
        const bool vector = text == "color"_str || text == "velocity"_str;
        if (text == "color"_str)
            remap.output = Output::Color;
        else if (text == "opacity"_str || text == "alpha"_str)
            remap.output = Output::Opacity;
        else if (text == "size"_str)
            remap.output = Output::Size;
        else if (text == "velocity"_str)
            remap.output = Output::Velocity;
        else
            return None();
        text = String {};
        GetJsonValue(json, "operation"_str, text, false);
        if (text == "multiply"_str)
            remap.operation = Operation::Multiply;
        else if (text == "add"_str)
            remap.operation = Operation::Add;
        else if (text == "subtract"_str)
            remap.operation = Operation::Subtract;
        else if (text.is_empty() || text == "remap"_str)
            remap.operation = Operation::Remap;
        else
            return None();
        GetJsonValue(json, "inputcontrolpoint0"_str, remap.controlpoint, false);
        remap.controlpoint = ((remap.controlpoint % i32(8)) + i32(8)) % i32(8);
        GetJsonValue(json, "inputcontrolpoint1"_str, remap.controlpoint1, false);
        remap.controlpoint1 = ((remap.controlpoint1 % i32(8)) + i32(8)) % i32(8);
        float min { 0.0f }, max { 1.0f };
        GetJsonValue(json, "inputrangemin"_str, min, false);
        GetJsonValue(json, "inputrangemax"_str, max, false);
        remap.input_min = min;
        remap.input_max = max;
        if (vector) {
            array<float, 3> low { 0.0f, 0.0f, 0.0f }, high { 1.0f, 1.0f, 1.0f };
            GetJsonValue(json, "outputrangemin"_str, low, false);
            GetJsonValue(json, "outputrangemax"_str, high, false);
            remap.output_min = Eigen::Vector3f { low.data() }.cast<double>();
            remap.output_max = Eigen::Vector3f { high.data() }.cast<double>();
        } else {
            float low { 0.0f }, high { 1.0f };
            GetJsonValue(json, "outputrangemin"_str, low, false);
            GetJsonValue(json, "outputrangemax"_str, high, false);
            remap.output_min = Eigen::Vector3d::Constant(low);
            remap.output_max = Eigen::Vector3d::Constant(high);
        }
        return Some(rstd::move(remap));
    }

    // how far along the input range a value is, 0 to 1, through the transform
    auto Fraction(double value) const -> double {
        const double span = input_max - input_min;
        double t = span != 0.0 ? (value - input_min) / span : (value >= input_max ? 1.0 : 0.0);
        t        = t < 0.0 ? 0.0 : (t > 1.0 ? 1.0 : t);
        switch (transform) {
        case Transform::Linear: return t;
        case Transform::Sine:
            return 0.5 - 0.5 * std::cos(3.14159265358979323846 * transform_scale * t);
        case Transform::Triangle: {
            const double p = 2.0 * t - std::floor(2.0 * t);
            return 1.0 - std::abs(2.0 * p - 1.0);
        }
        case Transform::Square: {
            const double p = 2.0 * t - std::floor(2.0 * t);
            return p >= 0.5 ? 1.0 : 0.0;
        }
        }
        return t;
    }

    // the output value for an input
    auto Map(double value) const -> Eigen::Vector3d {
        return output_min + (output_max - output_min) * Fraction(value);
    }

    // the output value for a vector input: each axis over the range
    auto Map(const Eigen::Vector3d& value) const -> Eigen::Vector3d {
        Eigen::Vector3d t(Fraction(value.x()), Fraction(value.y()), Fraction(value.z()));
        return output_min + (output_max - output_min).cwiseProduct(t);
    }

    // a particle value after the operation, given its value and the input
    auto Apply(const Eigen::Vector3d& current, double value) const -> Eigen::Vector3d {
        return Apply(current, Eigen::Vector3d::Constant(value));
    }

    auto Apply(const Eigen::Vector3d& current, const Eigen::Vector3d& value) const
        -> Eigen::Vector3d {
        Eigen::Vector3d mapped = Map(value);
        if (output == Output::Size && operation != Operation::Multiply) mapped *= 2.0;
        switch (operation) {
        case Operation::Multiply: return current.cwiseProduct(mapped);
        case Operation::Add: return current + mapped;
        case Operation::Subtract: return current - mapped;
        case Operation::Remap: break;
        }
        return mapped;
    }

    // how much of the remapped value a particle `life` (0 to 1) through its life takes
    auto BlendWeight(double life) const -> double {
        auto ramp = [life](const array<double, 2>& range) {
            const double span = range[usize(1)] - range[usize(0)];
            if (span <= 0.0) return life >= range[usize(0)] ? 1.0 : 0.0;
            const double t = (life - range[usize(0)]) / span;
            return t < 0.0 ? 0.0 : (t > 1.0 ? 1.0 : t);
        };
        double weight = 1.0;
        if (blend_in.is_some()) weight *= ramp(*blend_in);
        if (blend_out.is_some()) weight *= 1.0 - ramp(*blend_out);
        return weight;
    }

    // Apply, blended with the particle's value by BlendWeight
    auto Blend(const Eigen::Vector3d& current, const Eigen::Vector3d& value, double life) const
        -> Eigen::Vector3d {
        const double weight = BlendWeight(life);
        if (weight <= 0.0) return current;
        Eigen::Vector3d applied = Apply(current, value);
        return weight >= 1.0 ? applied : Eigen::Vector3d(current + (applied - current) * weight);
    }

    auto Blend(const Eigen::Vector3d& current, double value, double life) const -> Eigen::Vector3d {
        return Blend(current, Eigen::Vector3d::Constant(value), life);
    }
};

struct ParticleControlpoint {
    bool link_mouse { false };
    // Recomputed per instance after applying parent-particle overrides.
    bool                        mouse_driven { false };
    bool                        worldspace { false };
    Eigen::Vector3d             base_offset { 0.0, 0.0, 0.0 };
    Eigen::Vector3d             offset { 0.0, 0.0, 0.0 };
    Eigen::Matrix3d             rotation { Eigen::Matrix3d::Identity() };
    Option<SceneAnimationTrack> angle_track;
};

struct ParticleSimulationControlpoint {
    Eigen::Vector3d center { Eigen::Vector3d::Zero() };
    Eigen::Matrix3d basis { Eigen::Matrix3d::Identity() };
};

struct ParticleAudioResponse {
    bool            enable { false };
    float           amount { 1.0f };
    float           exponent { 1.0f };
    array<float, 2> frequency { 0.0f, 15.0f };
    array<float, 2> bounds { 0.0f, 1.0f };
};

struct ParticleFollowAnchor {
    bool  trail_renderer { false };
    float length { 0.0f };
    float max_length { 0.0f };
    float texture_ratio { 1.0f };
};

struct ParticleTrailUniformState {
    array<float, 4> render_var {};
};

class ParticleInstanceModifiers {
public:
    ParticleInstanceModifiers(Arc<wpscene::ParticleInstanceoverride> state,
                              wpscene::Particle::EFlags flags, bool controlpoints)
        : m_state(rstd::move(state)), m_flags(flags), m_controlpoints(controlpoints) {}

    auto Clone() const -> ParticleInstanceModifiers {
        return ParticleInstanceModifiers(m_state.clone(), m_flags, m_controlpoints);
    }

    bool  Enabled() const noexcept { return m_state->enabled; }
    float Alpha() const noexcept { return m_state->alpha; }
    float Count() const noexcept {
        return Disabled(wpscene::Particle::FlagEnum::disable_count_override) ? 1.0f
                                                                             : m_state->count;
    }
    float Lifetime() const noexcept {
        return Disabled(wpscene::Particle::FlagEnum::disable_lifetime_override) ? 1.0f
                                                                                : m_state->lifetime;
    }
    float Rate() const noexcept { return m_state->rate; }
    float Size() const noexcept {
        return Disabled(wpscene::Particle::FlagEnum::disable_size_override) ? 1.0f : m_state->size;
    }
    float Speed() const noexcept {
        return Disabled(wpscene::Particle::FlagEnum::disable_speed_override) ? 1.0f
                                                                             : m_state->speed;
    }
    bool HasColorOverride() const noexcept {
        return ! Disabled(wpscene::Particle::FlagEnum::disable_color_override) &&
               (m_state->overColor || m_state->overColorn);
    }
    bool UsesLegacyColor() const noexcept { return m_state->overColor; }
    auto Color() const noexcept -> const array<float, 3>& {
        return m_state->overColor ? m_state->color : m_state->colorn;
    }
    bool ControlpointsEnabled() const noexcept { return m_controlpoints; }
    auto Controlpoint(usize index) const noexcept -> const Option<array<float, 3>>& {
        return m_state->controlpoint[index];
    }
    auto ControlpointAngle(usize index) const noexcept -> const array<float, 3>& {
        return m_state->controlpointangle[index];
    }
    auto ControlpointFieldBindings() const noexcept -> const wpscene::FieldBindings* {
        return m_state->FieldBindingsView();
    }

private:
    bool Disabled(wpscene::Particle::FlagEnum flag) const noexcept { return m_flags[flag]; }

    Arc<wpscene::ParticleInstanceoverride> m_state;
    wpscene::Particle::EFlags              m_flags;
    bool                                   m_controlpoints { false };
};

struct ParticleAttributes {
    particle::ParticleAttributeKey<particle::PositionAttribute>            position;
    particle::ParticleAttributeKey<particle::VelocityAttribute>            velocity;
    particle::ParticleAttributeKey<particle::AccelerationAttribute>        acceleration;
    particle::ParticleAttributeKey<particle::RotationAttribute>            rotation;
    particle::ParticleAttributeKey<particle::AngularVelocityAttribute>     angular_velocity;
    particle::ParticleAttributeKey<particle::AngularAccelerationAttribute> angular_acceleration;
    particle::ParticleAttributeKey<particle::ColorAttribute>               color;
    particle::ParticleAttributeKey<particle::AlphaAttribute>               alpha;
    particle::ParticleAttributeKey<particle::SizeAttribute>                size;
    particle::ParticleAttributeKey<particle::LifetimeAttribute>            lifetime;
    particle::ParticleAttributeKey<particle::RandomAttribute>              random;
    particle::ParticleAttributeKey<particle::InitialColorAttribute>        initial_color;
    particle::ParticleAttributeKey<particle::InitialAlphaAttribute>        initial_alpha;
    particle::ParticleAttributeKey<particle::InitialSizeAttribute>         initial_size;
    particle::ParticleAttributeKey<particle::InitialLifetimeAttribute>     initial_lifetime;

    static auto Register(particle::ParticleSchemaBuilder&) -> ParticleAttributes;
};

#define OWE_WP_OSCILLATION_ATTRIBUTE(Name, Type)                                                   \
    struct Name {                                                                                  \
        using Value = Type;                                                                        \
                                                                                                   \
        Name(particle::ParticleAttributeDescriptor descriptor, Value default_value)                \
            : storage(rstd::move(descriptor), rstd::move(default_value)) {}                        \
                                                                                                   \
        auto Descriptor() const -> ref<particle::ParticleAttributeDescriptor> {                    \
            return storage.Descriptor();                                                           \
        }                                                                                          \
        auto ConcreteType() const noexcept -> rstd::any::TypeId { return storage.ConcreteType(); } \
        auto ValueType() const noexcept -> rstd::any::TypeId { return storage.ValueTypeId(); }     \
        auto Len() const noexcept -> usize { return storage.Len(); }                               \
        auto Capacity() const noexcept -> usize { return storage.Capacity(); }                     \
        void Reserve(usize total_slots) { storage.Reserve(total_slots); }                          \
        void AppendDefaults(usize count) { storage.AppendDefaults(count); }                        \
        void ResetSlots(slice<particle::ParticleSlot>) {}                                          \
        void Clear() { storage.Clear(); }                                                          \
        auto Values() const noexcept -> slice<Value> { return storage.Values(); }                  \
        auto ValuesMut() noexcept -> mut_ref<Value[]> { return storage.ValuesMut(); }              \
        auto CloneEmpty() const -> Name {                                                          \
            return Name(storage.CloneDescriptor(), storage.DefaultValue());                        \
        }                                                                                          \
                                                                                                   \
        particle::ParticleValueAttributeStorage<Value> storage;                                    \
    }

OWE_WP_OSCILLATION_ATTRIBUTE(OscillationResetAttribute, bool);
OWE_WP_OSCILLATION_ATTRIBUTE(OscillationFrequencyAttribute, float);
OWE_WP_OSCILLATION_ATTRIBUTE(OscillationScaleAttribute, float);
OWE_WP_OSCILLATION_ATTRIBUTE(OscillationPhaseAttribute, float);

#undef OWE_WP_OSCILLATION_ATTRIBUTE

struct OscillationStateRef {
    bool&  reset;
    float& frequency;
    float& scale;
    float& phase;
};

struct OscillationValues {
    mut_ref<bool[]>  reset;
    mut_ref<float[]> frequency;
    mut_ref<float[]> scale;
    mut_ref<float[]> phase;

    auto At(usize index) -> OscillationStateRef {
        return {
            .reset     = reset[index],
            .frequency = frequency[index],
            .scale     = scale[index],
            .phase     = phase[index],
        };
    }
};

struct OscillationAttributes {
    particle::ParticleAttributeKey<OscillationResetAttribute>     reset;
    particle::ParticleAttributeKey<OscillationFrequencyAttribute> frequency;
    particle::ParticleAttributeKey<OscillationScaleAttribute>     scale;
    particle::ParticleAttributeKey<OscillationPhaseAttribute>     phase;
};

struct TrailSlotState {
    usize           head {};
    usize           len {};
    usize           sample_count {};
    Eigen::Vector3f previous_position { Eigen::Vector3f::Zero() };
    bool            has_previous_position { false };
};

struct TrailHistoryAttribute {
    using Value = TrailSlotState;

    TrailHistoryAttribute(particle::ParticleAttributeDescriptor descriptor, usize sample_capacity);

    auto Descriptor() const -> ref<particle::ParticleAttributeDescriptor>;
    auto ConcreteType() const noexcept -> rstd::any::TypeId;
    auto ValueType() const noexcept -> rstd::any::TypeId;
    auto Len() const noexcept -> usize;
    auto Capacity() const noexcept -> usize;
    void Reserve(usize total_slots);
    void AppendDefaults(usize count);
    void ResetSlots(slice<particle::ParticleSlot> slots);
    void Clear();
    auto CloneEmpty() const -> TrailHistoryAttribute;

    void Push(particle::ParticleSlot slot, const Eigen::Vector3f& position);
    void Initialize(particle::ParticleSlot slot, const Eigen::Vector3f& position);
    auto At(particle::ParticleSlot slot, usize logical_index) const -> Eigen::Vector3f;
    auto State(particle::ParticleSlot slot) const -> TrailSlotState;
    void SetPreviousPosition(particle::ParticleSlot slot, const Eigen::Vector3f& position);
    auto SampleCapacity() const noexcept -> usize { return m_sample_capacity; }

private:
    particle::ParticleAttributeDescriptor m_descriptor;
    usize                                 m_sample_capacity {};
    Vec<TrailSlotState>                   m_states;
    Vec<Eigen::Vector3f>                  m_positions;
};

struct ParticleFrame {
    class ParticleSubSystem* subsystem { nullptr };
    usize                    instance_index {};
    array<float, 16>         audio_average {};
    Eigen::Vector3d          mouse_local { Eigen::Vector3d::Zero() };
    bool                     mouse_in_window { false };
    Eigen::Matrix3d          world_from_local_dir { Eigen::Matrix3d::Identity() };
    Eigen::Matrix3d          local_from_world_dir { Eigen::Matrix3d::Identity() };
    Eigen::Matrix4d          world_from_spawn_space { Eigen::Matrix4d::Identity() };
    Eigen::Matrix4d          local_from_world { Eigen::Matrix4d::Identity() };
    Eigen::Vector2d          scene_extent { Eigen::Vector2d::Zero() };
    // each control point's velocity in the particles' space (per second), measured between frames
    Eigen::Matrix<double, 3, 8> controlpoint_velocity { Eigen::Matrix<double, 3, 8>::Zero() };
    f64                         time {};
    f64                         delta {};
    f64                         emitter_delta {};
    usize                       trail_sample_steps {};
    f64                         trail_sample_remainder {};
    bool                        world_space { false };
};

auto ParticleFrameFrom(ref<dyn<rstd::any::Any>>) -> ref<ParticleFrame>;

// Wallpaper Engine's operator steps for a frame (FUN_140236cd0): dt2 = dt x min(0.025 / dt, 1)^0.7, dt being the
// frame's own time (particles-frametime-f15/-f30/-f60), and at 1..20 target frames a second two operator passes.
auto ParticleOperatorStepsFor(u32 target_fps, f64 delta) -> particle::ParticleOperatorSteps;

struct ParticleSpawnColumns {
    mut_ref<particle::ParticleSlotState[]> states;
    mut_ref<Eigen::Vector3f[]>             positions;
    mut_ref<Eigen::Vector3f[]>             velocities;
    mut_ref<Eigen::Vector3f[]>             rotations;
    mut_ref<Eigen::Vector3f[]>             angular_velocities;
    mut_ref<Eigen::Vector3f[]>             colors;
    mut_ref<float[]>                       alphas;
    mut_ref<float[]>                       sizes;
    mut_ref<float[]>                       lifetimes;
    mut_ref<float[]>                       randoms;
    mut_ref<Eigen::Vector3f[]>             initial_colors;
    mut_ref<float[]>                       initial_alphas;
    mut_ref<float[]>                       initial_sizes;
    mut_ref<float[]>                       initial_lifetimes;
};

class ParticleSpawnInstruction {
public:
    ParticleSpawnInstruction(const ParticleSpawnInstruction&)                    = delete;
    auto operator=(const ParticleSpawnInstruction&) -> ParticleSpawnInstruction& = delete;
    ParticleSpawnInstruction(ParticleSpawnInstruction&&) noexcept;
    auto operator=(ParticleSpawnInstruction&&) noexcept -> ParticleSpawnInstruction&;
    ~ParticleSpawnInstruction();

    void Initialize(ParticleSpawnColumns&, particle::ParticleSpawnRequest,
                    ref<dyn<rstd::any::Any>>);
    auto SequenceCount() const -> Option<u32>;
    auto RunsLast() const -> bool;

private:
    struct Impl;

    explicit ParticleSpawnInstruction(Box<Impl>);
    template<typename T>
    static auto Make(T value) -> ParticleSpawnInstruction;

    friend class ParticleParser;

    Box<Impl> m_impl;
};

class ParticleSpawnPipeline {
public:
    explicit ParticleSpawnPipeline(ParticleAttributes attributes): m_attributes(attributes) {}

    void Add(ParticleSpawnInstruction instruction) {
        if (instruction.RunsLast())
            m_last_instructions.push(rstd::move(instruction));
        else
            m_instructions.push(rstd::move(instruction));
    }
    void EnableWorldSpace() noexcept { m_world_space = true; }
    void Compile(particle::ParticleViewCompiler&);
    auto Bind(particle::ParticleWriteView) -> ParticleSpawnColumns;
    void Initialize(ParticleSpawnColumns&, particle::ParticleSpawnRequest,
                    ref<dyn<rstd::any::Any>>);

private:
    ParticleAttributes                                               m_attributes;
    Vec<ParticleSpawnInstruction>                                    m_instructions;
    Vec<ParticleSpawnInstruction>                                    m_last_instructions;
    particle::ParticleWriteIndex<particle::VelocityAttribute>        m_velocity;
    particle::ParticleWriteIndex<particle::RotationAttribute>        m_rotation;
    particle::ParticleWriteIndex<particle::AngularVelocityAttribute> m_angular_velocity;
    particle::ParticleWriteIndex<particle::ColorAttribute>           m_color;
    particle::ParticleWriteIndex<particle::AlphaAttribute>           m_alpha;
    particle::ParticleWriteIndex<particle::SizeAttribute>            m_size;
    particle::ParticleWriteIndex<particle::LifetimeAttribute>        m_lifetime;
    particle::ParticleWriteIndex<particle::RandomAttribute>          m_random;
    particle::ParticleWriteIndex<particle::InitialColorAttribute>    m_initial_color;
    particle::ParticleWriteIndex<particle::InitialAlphaAttribute>    m_initial_alpha;
    particle::ParticleWriteIndex<particle::InitialSizeAttribute>     m_initial_size;
    particle::ParticleWriteIndex<particle::InitialLifetimeAttribute> m_initial_lifetime;
    bool                                                             m_world_space { false };
    bool                                                             m_compiled { false };
};

// When an emitter emits (particles-emitter-periodic): with flag 4, in on phases of
// minperiodicduration..maxperiodicduration seconds and off phases of
// minperiodicdelay..maxperiodicdelay, each on phase emitting at most maxtoemitperperiod (0: no
// limit); "delay" starts it that many seconds late. A phase switch takes a frame of its own (1 s
// on and 1 s off repeat every 2.06 s at 30 fps).
struct ParticleEmitterTiming {
    bool  periodic { false };
    float min_duration {}, max_duration {};
    float min_delay {}, max_delay {};
    u32   max_per_period {};
    float delay {};

    static auto From(const wpscene::Emitter& emitter) -> ParticleEmitterTiming {
        return ParticleEmitterTiming {
            .periodic       = emitter.flags[wpscene::Emitter::FlagEnum::periodic],
            .min_duration   = emitter.min_periodic_duration,
            .max_duration   = emitter.max_periodic_duration,
            .min_delay      = emitter.min_periodic_delay,
            .max_delay      = emitter.max_periodic_delay,
            .max_per_period = emitter.max_emit_per_period,
            .delay          = emitter.delay,
        };
    }
};

// An operator's blend windows: blendinstart..blendinend and blendoutstart..blendoutend, fractions of
// a particle's life over which the operator's effect grows from nothing and fades back out,
// linearly (particles-operator-blend); without them it acts all the time.
struct ParticleOperatorWindow {
    Option<array<double, 2>> in, out;

    static auto FromJson(const Json& json) -> ParticleOperatorWindow {
        auto read = [&](ref<str> start, ref<str> end) -> Option<array<double, 2>> {
            if (json.get(start).is_none() && json.get(end).is_none()) return None();
            float low {}, high {};
            GetJsonValue(json, start, low, false);
            GetJsonValue(json, end, high, false);
            return Some(array<double, 2> { low, high });
        };
        return ParticleOperatorWindow {
            .in  = read("blendinstart"_str, "blendinend"_str),
            .out = read("blendoutstart"_str, "blendoutend"_str),
        };
    }

    auto Always() const -> bool { return in.is_none() && out.is_none(); }

    // how much of its effect an operator has on a particle `life` (0 to 1) through its life
    auto Weight(double life) const -> double {
        auto ramp = [life](const array<double, 2>& range) {
            const double span = range[usize(1)] - range[usize(0)];
            if (span <= 0.0) return life >= range[usize(0)] ? 1.0 : 0.0;
            const double t = (life - range[usize(0)]) / span;
            return t < 0.0 ? 0.0 : (t > 1.0 ? 1.0 : t);
        };
        double weight = 1.0;
        if (in.is_some()) weight *= ramp(*in);
        if (out.is_some()) weight *= 1.0 - ramp(*out);
        return weight;
    }
};

// Whether a particle renderer draws in the thick vertex format, where a segment carries both of
// its particles' colours (THICKFORMAT): sprites with a sprite sheet, sprite trails, and plain ropes,
// whose segments blend from one particle's colour to the next (particles-rope-color). A rope trail
// is thin.
inline bool ParticleRendererThickFormat(ref<str> renderer, bool has_sprite) {
    if (renderer == "ropetrail"_str) return false;
    if (renderer == "rope"_str) return true;
    return has_sprite || renderer->ends_with("trail"_str);
}

struct ParticleBoxEmitterArgs {
    array<float, 3>       directions;
    array<float, 3>       min_distance;
    array<float, 3>       max_distance;
    float                 emit_speed {};
    array<float, 3>       origin;
    bool                  one_per_frame { false };
    u32                   instantaneous {};
    float                 min_speed {};
    float                 max_speed {};
    float                 duration {};
    i32                   controlpoint {};
    ParticleEmitterTiming timing;
    ParticleAudioResponse audio_response;
};

struct ParticleSphereEmitterArgs {
    array<float, 3>       directions;
    float                 min_distance {};
    float                 max_distance {};
    float                 emit_speed {};
    array<float, 3>       origin;
    array<i32, 3>         sign;
    bool                  one_per_frame { false };
    u32                   instantaneous {};
    float                 min_speed {};
    float                 max_speed {};
    float                 duration {};
    i32                   controlpoint {};
    ParticleEmitterTiming timing;
    ParticleAudioResponse audio_response;
};

// An image layer a layerimage emitter emits from (particles-layerimage): the layer's rectangle, its
// size's half-extents before the layer's scale, turned and moved by the layer's node; the particles
// take the layer's colour; a layer less than half opaque emits nothing (a pixel counts with its
// opacity times the layer's). a pixel of a layer's image that emits: its place in layer units from
// the centre, and its colour
struct LayerImagePoint {
    float           x {}, y {};
    Eigen::Vector3f color { 1.0f, 1.0f, 1.0f };
};

struct LayerImageSource {
    Arc<SceneNode>  node;
    double          half_width {};
    double          half_height {};
    Eigen::Vector3f color { 1.0f, 1.0f, 1.0f };
    float           alpha { 1.0f };
    // the lattice of the layer's image, when it has one (LayerImageLattice): the emitter picks one
    // of its points at random
    Vec<LayerImagePoint> points;
};

// The points a layer's image emits from, as Wallpaper Engine reads it (particles-layerimage-pitch,
// -block): one at the centre of each 4x4 texel block (texel 4i+2 from the top left, no jitter; a
// part block at the edge is left out, not probed), in texels from the image's centre (y up), where
// the block's average opacity times the layer's is half or more, in the block's average colour.
// `data` is 8-bit RGB, RGBA or BGRA by `channels` and `bgra`.
Vec<LayerImagePoint> LayerImageLattice(const unsigned char* data, int width, int height,
                                       int channels, bool bgra, float layer_alpha);

// A solid layer's points: the lattice of a white image of its size in whole units
// (particles-layerimage-alpha: a 300x150 rectangle has 37 rows of blocks from the top and none in
// its last 2 units).
Vec<LayerImagePoint> SolidLayerLattice(int width, int height);

struct ParticleLayerImageEmitterArgs {
    float                         emit_speed {};
    bool                          one_per_frame { false };
    u32                           instantaneous {};
    float                         duration {};
    i32                           controlpoint {};
    ParticleAudioResponse         audio_response;
    Option<Arc<LayerImageSource>> source;
};

class LayerImageEmitterProgram {
public:
    LayerImageEmitterProgram(ParticleSpawnPipeline& pipeline, ParticleLayerImageEmitterArgs args,
                             usize index)
        : m_pipeline(rstd::addressof(pipeline)), m_args(rstd::move(args)), m_index(index) {}

    void Compile(particle::ParticleViewCompiler&);
    void Emit(particle::ParticleEmitterContext&);

private:
    ParticleSpawnPipeline*        m_pipeline;
    ParticleLayerImageEmitterArgs m_args;
    usize                         m_index {};
};

class BoxEmitterProgram {
public:
    BoxEmitterProgram(ParticleSpawnPipeline& pipeline, ParticleBoxEmitterArgs args, usize index)
        : m_pipeline(rstd::addressof(pipeline)), m_args(rstd::move(args)), m_index(index) {}

    void Compile(particle::ParticleViewCompiler&);
    void Emit(particle::ParticleEmitterContext&);

private:
    ParticleSpawnPipeline* m_pipeline;
    ParticleBoxEmitterArgs m_args;
    usize                  m_index {};
};

class SphereEmitterProgram {
public:
    SphereEmitterProgram(ParticleSpawnPipeline& pipeline, ParticleSphereEmitterArgs args,
                         usize index)
        : m_pipeline(rstd::addressof(pipeline)), m_args(rstd::move(args)), m_index(index) {}

    void Compile(particle::ParticleViewCompiler&);
    void Emit(particle::ParticleEmitterContext&);

private:
    ParticleSpawnPipeline*    m_pipeline;
    ParticleSphereEmitterArgs m_args;
    usize                     m_index {};
};

struct ParticleAnimationSpec {
    ParticleAnimationMode mode { ParticleAnimationMode::SEQUENCE };
    float                 sequence_multiplier { 1.0f };
};

struct ParticlePlaybackState {
    rstd::sync::atomic::Atomic<bool> playing { true };
    rstd::sync::atomic::Atomic<u32>  reset_sequence {};
    rstd::sync::atomic::Atomic<u32>  pending_emit_count {};
};

class ParticleSubSystem;

struct ParticleInstanceState {
    struct EmitterState {
        f64 timer {};
        f64 elapsed {};
        // periodic emission and the start delay (ParticleEmitterTiming): whether the emitter is
        // in an on phase, how long it has been in it, how long it lasts, what it has emitted
        bool timing_started { false };
        bool timing_on { true };
        f64  phase {};
        f64  phase_length {};
        u32  period_emitted {};
    };

    struct BoundedData {
        particle::ParticleInstance* parent { nullptr };
        const ParticleSubSystem*    parent_subsystem { nullptr };
        usize                       parent_instance_index {};
        isize                       particle_index { isize(-1) };
        bool                        previous_lifetime_ok { true };
        Eigen::Vector3f             position { 0.0f, 0.0f, 0.0f };
    } bounded;

    bool              death { false };
    bool              no_live_particle { false };
    bool              warmup_pending { true };
    // the parent's periodic emission began a new on phase: the instantaneous emitters fire again
    bool              restart_pending { false };
    Vec<EmitterState> emitters;

    auto Emitter(usize index) -> EmitterState& {
        while (emitters.len() <= index) emitters.emplace_back();
        return emitters[index];
    }

    void Reset() {
        bounded          = {};
        death            = false;
        no_live_particle = false;
        warmup_pending   = true;
        emitters.clear();
    }
};

// Advances an emitter's phase by a frame of `delta` seconds; true while it may emit.
inline bool AdvanceEmitterTiming(ParticleInstanceState::EmitterState& state,
                                 const ParticleEmitterTiming& timing, double delta) {
    if (! timing.periodic && timing.delay <= 0.0f) return true;
    auto between = [](float low, float high) {
        return f64(low == high ? static_cast<double>(low) : Random::get(low, high));
    };
    if (! state.timing_started) {
        state.timing_started = true;
        state.timing_on      = timing.delay <= 0.0f;
        state.phase          = f64();
        state.phase_length   = state.timing_on ? between(timing.min_duration, timing.max_duration)
                                               : f64(static_cast<double>(timing.delay));
        state.period_emitted = u32();
    }
    // after a delay alone the emitter stays on
    const bool endless = state.timing_on && ! timing.periodic;
    if (! endless && state.phase.to_primitive() >= state.phase_length.to_primitive() - 1e-9) {
        state.timing_on = ! state.timing_on;
        state.phase     = f64();
        if (state.timing_on) {
            state.phase_length   = between(timing.min_duration, timing.max_duration);
            state.period_emitted = u32();
        } else {
            state.phase_length = between(timing.min_delay, timing.max_delay);
        }
    } else {
        state.phase += f64(delta);
    }
    return state.timing_on;
}

// How many of `count` particles an emitter in an on phase may still emit.
inline auto EmitterTimingAllowance(const ParticleInstanceState::EmitterState& state,
                                   const ParticleEmitterTiming& timing, u32 count) -> u32 {
    if (! timing.periodic || timing.max_per_period == u32()) return count;
    return rstd::cmp::min(count, timing.max_per_period.saturating_sub(state.period_emitted));
}

struct ParticleInstanceRef {
    particle::ParticleInstance* instance { nullptr };
    ParticleInstanceState*      state { nullptr };
    usize                       index {};
};

class ParticleSubSystem : NoCopy, NoMove {
public:
    enum class SpawnType
    {
        STATIC,
        STATIC_CONTROLPOINT,
        EVENT_FOLLOW,
        EVENT_SPAWN,
        EVENT_DEATH,
    };

    static auto EffectiveInstanceCapacity(u32 max_instance_count, SpawnType spawn_type) noexcept
        -> u32 {
        // Static subsystems own one persistent instance; the limit only bounds event pools.
        return spawn_type == SpawnType::STATIC ? u32(1) : max_instance_count;
    }

    static auto MaxParticleCapacity(u32 max_count, u32 max_instance_count,
                                    SpawnType spawn_type) noexcept -> Option<u32> {
        return max_count.checked_mul(EffectiveInstanceCapacity(max_instance_count, spawn_type));
    }

    ParticleSubSystem(Scene&, Arc<SceneMesh>, u32 max_count, f64 rate, u32 max_instance_count,
                      f64 probability, SpawnType, ParticleAnimationSpec, ParticleFollowAnchor = {},
                      u32 trail_length = {}, f64 trail_duration = {}, f64 start_time = {},
                      bool                                   world_space         = false,
                      Option<Arc<ParticleTrailUniformState>> trail_uniform_state = None());
    ~ParticleSubSystem();

    void Finalize();
    void Tick(f64 frame_time, bool update_mesh = true);
    auto QueryNewInstance() -> Option<ParticleInstanceRef>;

    void AddEmitter(Box<dyn<particle::ParticleEmitterProgram>>);
    void AddInitializer(ParticleSpawnInstruction);
    void AddOperator(Box<dyn<particle::ParticleUpdateProgram>>);
    void AddChild(Box<ParticleSubSystem>);
    // a periodic emitter began a new on phase: static children's instantaneous emitters fire again
    // (particles-child-static)
    void RestartStaticChildren();

    auto SchemaBuilder() noexcept -> particle::ParticleSchemaBuilder& { return m_schema_builder; }
    auto Attributes() const noexcept -> const ParticleAttributes& { return m_attributes; }
    auto SpawnPipeline() noexcept -> ParticleSpawnPipeline& { return m_spawn_pipeline; }
    auto Controlpoints() const noexcept -> slice<ParticleControlpoint> {
        return m_controlpoints.as_slice();
    }
    auto ControlpointsMut() noexcept -> mut_ref<ParticleControlpoint[]> {
        return m_controlpoints.as_mut_slice();
    }
    auto SimulationControlpoint(usize index) const -> ParticleSimulationControlpoint;
    // the engine's run time in seconds, and the local time of day as a fraction (0.5 at noon)
    auto SceneRuntimeSeconds() const -> double;
    auto LocalTimeOfDay() const -> double;
    void SetInstanceModifiers(ParticleInstanceModifiers value) {
        m_instance_modifiers = Some(rstd::move(value));
    }
    void SetControlpointAngleTrack(usize index, SceneAnimationTrack track) {
        if (m_owner_node != nullptr) {
            auto property = rstd::format("controlpointangle{}", index);
            m_owner_node->BindFieldAnimation(rstd::move(property), track.playback.clone());
        }
        m_controlpoints[index].angle_track = Some(rstd::move(track));
    }
    void SetOwnerNode(SceneNode* node) noexcept { m_owner_node = node; }
    auto OwnerNode() const noexcept -> const SceneNode* { return m_owner_node; }
    void SetPlaybackState(Arc<ParticlePlaybackState> state) {
        m_playback_state = Some(rstd::move(state));
    }
    void SetParentControlpointStartIndex(i32 value) {
        m_parent_controlpoint_start_index = Some(value);
    }

    auto Type() const noexcept -> SpawnType { return m_spawn_type; }
    auto MaxInstanceCount() const noexcept -> u32 { return m_max_instance_count; }
    auto MaxParticleCapacity() const noexcept -> Option<u32> {
        return m_max_count.checked_mul(m_max_instance_count);
    }
    void CompileRuntimeView(particle::ParticleViewCompiler&);
    auto FollowWorldPosition(particle::ParticleInstance&, usize parent_instance_index,
                             particle::ParticleSlot) const -> Eigen::Vector3f;
    auto OwnerWorldToLocal(const Eigen::Vector3f& position) const -> Eigen::Vector3f;
    bool LifetimeAlive(particle::ParticleInstance&, particle::ParticleSlot) const;
    auto InstanceState(usize index) const -> const ParticleInstanceState& {
        return m_instance_states[index];
    }
    auto InstanceStateMut(usize index) -> ParticleInstanceState& {
        return m_instance_states[index];
    }
    auto TrailKey() const noexcept
        -> Option<particle::ParticleAttributeKey<TrailHistoryAttribute>> {
        return m_trail_key;
    }
    auto RopeSequenceCount() const noexcept -> Option<u32> { return m_rope_sequence_count; }
    void SetRopeSequenceCount(u32 value) noexcept { m_rope_sequence_count = Some(value); }
    auto AnimationSpec() const noexcept -> ParticleAnimationSpec { return m_animation_spec; }
    auto TakePendingEmitCount() noexcept -> u32;
    auto RenderPosition(usize instance_index, const Eigen::Vector3f& position) const
        -> Eigen::Vector3f;
    auto Mesh() noexcept -> SceneMesh& { return *m_mesh; }
    auto System() noexcept -> particle::ParticleSystem&;

    void ProcessChildEvents(particle::ParticleEventContext&);

private:
    void Warmup(ParticleInstanceRef, ref<dyn<rstd::any::Any>>);
    bool SyncPlayback();
    void ExtractCurrentMesh();
    void Advance(f64 frame_time, f64 child_frame_time, bool update_mesh);
    void UpdateFrameInput(f64 frame_time);
    void UpdateControlpoints(ParticleInstanceRef);
    void UpdateBoundedState(ParticleInstanceRef);
    auto OwnerLocalToWorld(const Eigen::Vector3f& position) const -> Eigen::Vector3f;
    auto HasBoundInstance(particle::ParticleInstance*, usize, particle::ParticleSlot) const -> bool;
    void ReleaseBoundInstances(particle::ParticleInstance*, usize, particle::ParticleSlot);
    void SpawnChild(ParticleInstanceRef, ParticleSubSystem&, particle::ParticleSlot,
                    Eigen::Vector3f position = Eigen::Vector3f::Zero(), bool fixed = false);

    Scene&                          m_scene;
    Eigen::Matrix<double, 3, 8>     m_controlpoint_previous { Eigen::Matrix<double, 3, 8>::Zero() };
    bool                            m_has_controlpoint_previous { false };
    Arc<SceneMesh>                  m_mesh;
    SceneNode*                      m_owner_node { nullptr };
    particle::ParticleSchemaBuilder m_schema_builder;
    ParticleAttributes              m_attributes;
    ParticleSpawnPipeline           m_spawn_pipeline;
    Option<particle::ParticleAttributeKey<TrailHistoryAttribute>> m_trail_key;
    Option<u32>                                                   m_rope_sequence_count;
    particle::ParticleProgram                                     m_program;
    Option<Box<particle::ParticleSystem>>                         m_system;
    Vec<ParticleInstanceState>                                    m_instance_states;
    Vec<Box<ParticleSubSystem>>                                   m_children;
    array<ParticleControlpoint, 8>                                m_controlpoints;
    Option<i32>                                                   m_parent_controlpoint_start_index;
    Option<ParticleInstanceModifiers>                             m_instance_modifiers;
    ParticleFrame                                                 m_frame;
    ParticleAnimationSpec                                         m_animation_spec;
    ParticleFollowAnchor                                          m_follow_anchor;
    u32                                                           m_max_count;
    f64                                                           m_rate;
    f64                                                           m_time {};
    f64                                                           m_start_time {};
    bool                                                          m_world_space { false };
    u32                                                           m_max_instance_count { 1 };
    f64                                                           m_probability { 1.0 };
    SpawnType                                                m_spawn_type { SpawnType::STATIC };
    u32                                                      m_trail_length {};
    f64                                                      m_trail_sample_interval {};
    f64                                                      m_trail_sample_accumulator {};
    Option<Arc<ParticleTrailUniformState>>                   m_trail_uniform_state;
    Option<Arc<ParticlePlaybackState>>                       m_playback_state;
    u32                                                      m_seen_reset_sequence {};
    Vec<particle::ParticleSlot>                              m_pending_child_deaths;
    particle::ParticleReadIndex<particle::VelocityAttribute> m_follow_velocity;
    particle::ParticleReadIndex<particle::SizeAttribute>     m_follow_size;
    particle::ParticleReadIndex<particle::LifetimeAttribute> m_follow_lifetime;
};

class ParticleRuntime {
public:
    ParticleRuntime()                                      = default;
    ParticleRuntime(const ParticleRuntime&)                = delete;
    ParticleRuntime& operator=(const ParticleRuntime&)     = delete;
    ParticleRuntime(ParticleRuntime&&) noexcept            = default;
    ParticleRuntime& operator=(ParticleRuntime&&) noexcept = default;

    void Add(Box<ParticleSubSystem> subsystem) { m_subsystems.push(rstd::move(subsystem)); }
    void RemoveOwner(const SceneNode& owner) {
        m_subsystems.retain([&](const Box<ParticleSubSystem>& subsystem) {
            return subsystem->OwnerNode() != rstd::addressof(owner);
        });
    }
    void Update(ref<SceneFrame> frame);

private:
    void Tick(f64 delta);

    Vec<Box<ParticleSubSystem>> m_subsystems;
};

struct ParticleRuntimeSystem {
    Arc<ParticleRuntime> runtime;

    void Update(ref<SceneFrame> frame) { runtime->Update(frame); }
};

class ParticleRawGenerator {
public:
    ParticleRawGenerator(ParticleSubSystem& subsystem): m_subsystem(&subsystem) {}

    void Compile(particle::ParticleViewCompiler&);
    void Extract(particle::ParticleExtractContext&);

private:
    ParticleSubSystem*                                              m_subsystem;
    particle::ParticleReadIndex<particle::VelocityAttribute>        m_velocity;
    particle::ParticleReadIndex<particle::RotationAttribute>        m_rotation;
    particle::ParticleReadIndex<particle::ColorAttribute>           m_color;
    particle::ParticleReadIndex<particle::AlphaAttribute>           m_alpha;
    particle::ParticleReadIndex<particle::SizeAttribute>            m_size;
    particle::ParticleReadIndex<particle::LifetimeAttribute>        m_lifetime;
    particle::ParticleReadIndex<particle::RandomAttribute>          m_random;
    particle::ParticleReadIndex<particle::InitialLifetimeAttribute> m_initial_lifetime;
    particle::ParticleReadObjectIndex<TrailHistoryAttribute>        m_trail;
};

} // namespace owe
