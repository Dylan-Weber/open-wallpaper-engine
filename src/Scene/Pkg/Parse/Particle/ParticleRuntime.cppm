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

export namespace owe
{

enum class ParticleAnimationMode
{
    SEQUENCE,
    RANDOMONE,
};

// remapvalue: maps an input (the distance to a control point, or how far through its life a
// particle is) from inputrangemin..max, clamped, linearly onto outputrangemin..max, and sets a
// particle's colour, opacity or size to it, or multiplies or adds it (particles-remapvalue). A size
// it sets is a radius: the particle is drawn twice as large. Other inputs, outputs and transform
// functions (noise, sine) are not read: such an operator does nothing.
// (Inputs: see Input; scalar ones give every axis the same value, so a colour of them is grey.)
struct ParticleRemap {
    enum class Input
    {
        DistanceToControlPoint,
        Lifetime,
        // the particle's speed along each axis, its opacity, rotation (about z) and angular speed (about z)
        Velocity,
        Opacity,
        Rotation,
        AngularSpeed,
        // the object's position
        LayerOrigin,
        // seconds: the particle system's, the layer's, the engine's; and the local time of day as a fraction
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
    // the shape the fraction of the input range goes through (particles-remap-transform): triangle and square repeat
    // twice over the range, sine once
    enum class Transform
    {
        Linear,
        Triangle,
        Sine,
        Square,
    };
    Input           input { Input::DistanceToControlPoint };
    i32             controlpoint { 0 };
    double          input_min { 0.0 }, input_max { 1.0 };
    Output          output { Output::Color };
    Operation       operation { Operation::Remap };
    Transform       transform { Transform::Linear };
    Eigen::Vector3d output_min { Eigen::Vector3d::Zero() }, output_max { Eigen::Vector3d::Ones() };

    static auto FromJson(const Json& json) -> Option<ParticleRemap> {
        ParticleRemap remap;
        String        text;
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
        double       t    = span != 0.0 ? (value - input_min) / span : (value >= input_max ? 1.0 : 0.0);
        t                 = t < 0.0 ? 0.0 : (t > 1.0 ? 1.0 : t);
        switch (transform) {
        case Transform::Linear: return t;
        case Transform::Sine: return 0.5 - 0.5 * std::cos(2.0 * 3.14159265358979323846 * t);
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

    auto Apply(const Eigen::Vector3d& current, const Eigen::Vector3d& value) const -> Eigen::Vector3d {
        Eigen::Vector3d mapped = Map(value);
        if (output == Output::Size && operation == Operation::Remap) mapped *= 2.0;
        switch (operation) {
        case Operation::Multiply: return current.cwiseProduct(mapped);
        case Operation::Add: return current + mapped;
        case Operation::Subtract: return current - mapped;
        case Operation::Remap: break;
        }
        return mapped;
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
    f64                      time {};
    f64                      delta {};
    f64                      emitter_delta {};
    usize                    trail_sample_steps {};
    f64                      trail_sample_remainder {};
    bool                     world_space { false };
};

auto ParticleFrameFrom(ref<dyn<rstd::any::Any>>) -> ref<ParticleFrame>;

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

    void Add(ParticleSpawnInstruction instruction) { m_instructions.push(rstd::move(instruction)); }
    void EnableWorldSpace() noexcept { m_world_space = true; }
    void Compile(particle::ParticleViewCompiler&);
    auto Bind(particle::ParticleWriteView) -> ParticleSpawnColumns;
    void Initialize(ParticleSpawnColumns&, particle::ParticleSpawnRequest,
                    ref<dyn<rstd::any::Any>>);

private:
    ParticleAttributes                                               m_attributes;
    Vec<ParticleSpawnInstruction>                                    m_instructions;
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
    ParticleAudioResponse audio_response;
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
    auto OwnerWorldToLocal(const Eigen::Vector3f& position) const -> Eigen::Vector3f;
    auto HasBoundInstance(particle::ParticleInstance*, usize, particle::ParticleSlot) const -> bool;
    void ReleaseBoundInstances(particle::ParticleInstance*, usize, particle::ParticleSlot);
    void SpawnChild(ParticleInstanceRef, ParticleSubSystem&, particle::ParticleSlot,
                    Eigen::Vector3f position = Eigen::Vector3f::Zero(), bool fixed = false);

    Scene&                                                        m_scene;
    Arc<SceneMesh>                                                m_mesh;
    SceneNode*                                                    m_owner_node { nullptr };
    particle::ParticleSchemaBuilder                               m_schema_builder;
    ParticleAttributes                                            m_attributes;
    ParticleSpawnPipeline                                         m_spawn_pipeline;
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
