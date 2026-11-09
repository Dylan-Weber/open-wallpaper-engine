#include <rstd/test/gtest.hpp>

#if defined(__SSE__)
#    include <xmmintrin.h>
#endif

import rstd;
import rstd.cppstd;
import eigen;
import wescene.fs;
import wescene.json;
import wescene.particle;
import wescene.particle.program;
import wescene.pkg.parse;
import wescene.scene;
import wescene.types;
import wescene.utils;

using namespace rstd::prelude;
using rstd::sync::Arc;
using namespace rstd::literals;

namespace
{

namespace particle = owe::particle;

template<typename Attribute, typename... Args>
auto Register(particle::ParticleSchemaBuilder& builder, ref<str> name, ref<str> owner,
              Args&&... args) -> particle::ParticleAttributeKey<Attribute> {
    auto result = builder.Register<Attribute>(name, owner, rstd::forward<Args>(args)...);
    if (result.is_err()) rstd::panic { "particle test attribute registration failed" };
    return result.unwrap();
}

struct TemperatureAttribute {
    using Value = float;

    static inline usize values_mut_calls {};

    TemperatureAttribute(particle::ParticleAttributeDescriptor descriptor, Value default_value)
        : storage(rstd::move(descriptor), default_value) {}

    auto Descriptor() const -> ref<particle::ParticleAttributeDescriptor> {
        return storage.Descriptor();
    }
    auto ConcreteType() const noexcept -> rstd::any::TypeId { return storage.ConcreteType(); }
    auto ValueType() const noexcept -> rstd::any::TypeId { return storage.ValueTypeId(); }
    auto Len() const noexcept -> usize { return storage.Len(); }
    auto Capacity() const noexcept -> usize { return storage.Capacity(); }
    void Reserve(usize total_slots) { storage.Reserve(total_slots); }
    void AppendDefaults(usize count) { storage.AppendDefaults(count); }
    void ResetSlots(slice<particle::ParticleSlot> slots) { storage.ResetSlots(slots); }
    void Clear() { storage.Clear(); }
    auto Values() const noexcept -> slice<Value> { return storage.Values(); }
    auto ValuesMut() noexcept -> mut_ref<Value[]> {
        ++values_mut_calls;
        return storage.ValuesMut();
    }
    auto CloneEmpty() const -> TemperatureAttribute {
        return TemperatureAttribute(storage.CloneDescriptor(), storage.DefaultValue());
    }

    particle::ParticleValueAttributeStorage<Value> storage;
};

struct TraceEmitter {
    rstd::vec::Vec<i32>* trace;

    void Compile(particle::ParticleViewCompiler&) {}
    void Emit(particle::ParticleEmitterContext& context) {
        trace->push(i32(1));
        auto particles = context.Acquire(usize(1), f64());
        if (particles.is_empty()) rstd::panic { "particle test could not acquire slot" };
        context.Initialize(particles);
    }
};

struct TraceSpawn {
    rstd::vec::Vec<i32>*                                 trace;
    particle::ParticleAttributeKey<TemperatureAttribute> temperature;
    particle::ParticleWriteIndex<TemperatureAttribute>   index;

    void Compile(particle::ParticleViewCompiler& compiler) { index = compiler.Write(temperature); }

    void Initialize(particle::ParticleSpawnContext& context) {
        trace->push(i32(2));
        auto values = context.view.Write(index);
        for (auto request : context.particles) values[request.slot.index] = 42.0f;
    }
};

struct TraceLifecycle {
    rstd::vec::Vec<i32>* trace;

    void Compile(particle::ParticleViewCompiler&) {}
    void Update(particle::ParticleLifecycleContext&) { trace->push(i32(3)); }
};

struct LifetimeSpawn {
    particle::ParticleAttributeKey<TemperatureAttribute> lifetime;
    particle::ParticleWriteIndex<TemperatureAttribute>   index;

    void Compile(particle::ParticleViewCompiler& compiler) { index = compiler.Write(lifetime); }

    void Initialize(particle::ParticleSpawnContext& context) {
        auto values = context.view.Write(index);
        for (auto request : context.particles) values[request.slot.index] = 1.0f;
    }
};

struct ContinuousEmitter {
    void Compile(particle::ParticleViewCompiler&) {}
    void Emit(particle::ParticleEmitterContext& context) {
        auto particles = context.Acquire(usize(1), f64());
        context.Initialize(particles);
    }
};

struct ExpiringLifecycle {
    particle::ParticleAttributeKey<TemperatureAttribute> lifetime;
    particle::ParticleWriteIndex<TemperatureAttribute>   index;

    void Compile(particle::ParticleViewCompiler& compiler) { index = compiler.Write(lifetime); }

    void Update(particle::ParticleLifecycleContext& context) {
        auto values = context.view.Write(index);
        for (auto slot : context.slots) {
            values[slot.index] -= context.delta.to_primitive();
            if (values[slot.index] <= 0.0f) context.Kill(slot);
        }
    }
};

struct TransitionTraceEvent {
    rstd::vec::Vec<i32>* trace;

    void Compile(particle::ParticleViewCompiler&) {}
    void Process(particle::ParticleEventContext& context) {
        if (! context.events->died.is_empty()) trace->push(i32(-1));
        if (! context.events->spawned.is_empty()) trace->push(i32(1));
    }
};

struct TraceEvent {
    rstd::vec::Vec<i32>* trace;
    usize*               spawned;

    void Compile(particle::ParticleViewCompiler&) {}
    void Process(particle::ParticleEventContext& context) {
        trace->push(i32(4));
        *spawned = context.events->spawned.len();
    }
};

struct TraceUpdate {
    rstd::vec::Vec<i32>* trace;
    i32                  value;

    void Compile(particle::ParticleViewCompiler&) {}
    void Update(particle::ParticleUpdateContext&) { trace->emplace_back(value); }
};

struct DeltaTraceUpdate {
    rstd::vec::Vec<double>* deltas;
    rstd::vec::Vec<double>* deltas2;

    void Compile(particle::ParticleViewCompiler&) {}
    void Update(particle::ParticleUpdateContext& context) {
        deltas->push(context.delta.to_primitive());
        if (deltas2 != nullptr) deltas2->push(context.delta2.to_primitive());
    }
};

struct TraceExtract {
    rstd::vec::Vec<i32>* trace;

    void Compile(particle::ParticleViewCompiler&) {}
    void Extract(particle::ParticleExtractContext&) { trace->push(i32(7)); }
};

struct InvalidKeyProgram {
    particle::ParticleAttributeKey<particle::ColorAttribute> key;

    void Compile(particle::ParticleViewCompiler& compiler) { (void)compiler.Read(key); }
    void Update(particle::ParticleUpdateContext&) {}
};

struct EmptyFrame {};

auto MakeTimedParticleSubsystem(owe::Scene& scene, ref<str> emitter_name, i32 controlpoint = i32(),
                                float duration = 0.0f) -> Box<owe::ParticleSubSystem> {
    auto subsystem = Box<owe::ParticleSubSystem>::make(scene,
                                                       Arc<owe::SceneMesh>::make(),
                                                       u32(64),
                                                       f64(1.0),
                                                       u32(1),
                                                       f64(1.0),
                                                       owe::ParticleSubSystem::SpawnType::STATIC,
                                                       owe::ParticleAnimationSpec {});
    subsystem->AddInitializer(owe::ParticleParser::GenInitializer(
        owe::ParseJson(R"({"name":"lifetimerandom","min":0.5,"max":0.5})"_str).unwrap(), u32(64)));
    owe::wpscene::Emitter emitter;
    emitter.name         = String::make(emitter_name);
    emitter.rate         = 64.0f;
    emitter.controlpoint = controlpoint;
    emitter.duration     = duration;
    subsystem->AddEmitter(owe::ParticleParser::GenEmitter(emitter, *subsystem, usize()));
    subsystem->Finalize();
    return subsystem;
}

auto LiveParticleCount(owe::ParticleSubSystem& subsystem) -> usize {
    usize count {};
    for (usize index {}; index < subsystem.System().InstanceCount(); ++index) {
        for (const auto& state : subsystem.System().Instance(index).Binding().Read().States()) {
            if (state.active) ++count;
        }
    }
    return count;
}

} // namespace

TEST(ParticleStorage, OwnsIndependentAttributesAndReusesStableSlots) {
    particle::ParticleSchemaBuilder builder;
    auto                            position = builder.PositionKey();
    auto                            temperature =
        Register<TemperatureAttribute>(builder, "temperature"_str, "test"_str, 18.0f);
    auto schema  = rstd::move(builder).Build();
    auto storage = schema.CreateStorage();

    auto first  = storage.AcquireSlot(usize(4));
    auto second = storage.AcquireSlot(usize(4));
    ASSERT_TRUE(first.is_some());
    ASSERT_TRUE(second.is_some());
    EXPECT_EQ(first->index, usize());
    EXPECT_EQ(second->index, usize(1));
    EXPECT_EQ(storage.Len(), usize(2));
    EXPECT_EQ(storage.AttributeRef(position)->Len(), storage.Len());
    EXPECT_EQ(storage.AttributeRef(temperature)->Len(), storage.Len());

    particle::ParticleSlotWriter first_value(storage, *first);
    first_value.Write(position)    = Eigen::Vector3f { 1.0f, 2.0f, 3.0f };
    first_value.Write(temperature) = 90.0f;
    storage.ReleaseSlot(*first);

    auto reused = storage.AcquireSlot(usize(4));
    ASSERT_TRUE(reused.is_some());
    EXPECT_EQ(*reused, *first);
    particle::ParticleSlotReader reused_value(storage, *reused);
    EXPECT_EQ(reused_value.Read(position), Eigen::Vector3f::Zero());
    EXPECT_FLOAT_EQ(reused_value.Read(temperature), 18.0f);
    EXPECT_EQ(rstd::cppstd::as_string_view(
                  storage.AttributeRef(temperature)->Descriptor()->owner.as_str()),
              "test");
}

TEST(ParticleSchema, RejectsDuplicateNamesAndMismatchedTypedKeys) {
    particle::ParticleSchemaBuilder duplicate_builder;
    auto                            position = duplicate_builder.PositionKey();
    auto duplicate                           = duplicate_builder.Register<particle::ColorAttribute>(
        "position"_str, "test"_str, Eigen::Vector3f::Ones());
    EXPECT_TRUE(duplicate.is_err());

    auto schema  = rstd::move(duplicate_builder).Build();
    auto storage = schema.CreateStorage();
    (void)storage.AppendSlot();
    particle::ParticleAttributeKey<particle::ColorAttribute> wrong {
        .id          = position.id,
        .schema_slot = position.schema_slot,
    };
    EXPECT_DEATH((void)storage.Values(wrong), "particle attribute key does not match storage");
}

TEST(ParticleSchema, RejectsMissingProgramRequirementsDuringPrepare) {
    particle::ParticleSchemaBuilder builder;
    auto                            position = builder.PositionKey();
    particle::ParticleProgram       program;
    program.AddUpdate(Box<dyn<particle::ParticleUpdateProgram>>::make(InvalidKeyProgram {
        .key =
            particle::ParticleAttributeKey<particle::ColorAttribute> {
                .id          = position.id,
                .schema_slot = position.schema_slot,
            },
    }));

    auto definition = particle::ParticleDefinition::Prepare(
        rstd::move(builder).Build(), rstd::move(program), usize(4));
    EXPECT_TRUE(definition.is_err());
}

TEST(ParticleView, RefreshesBoundColumnsAfterStructuralMutation) {
    particle::ParticleSchemaBuilder builder;
    auto temperature = Register<TemperatureAttribute>(builder, "temperature"_str, "test"_str, 1.0f);
    auto schema      = rstd::move(builder).Build();
    particle::ParticleViewCompiler compiler(schema);
    auto                           temperature_index = compiler.Write(temperature);
    auto                           layout_result     = rstd::move(compiler).Finish();
    ASSERT_TRUE(layout_result.is_ok());
    auto                          layout  = rstd::move(layout_result).unwrap();
    auto                          storage = schema.CreateStorage();
    particle::ParticleViewBinding binding(layout, storage);
    TemperatureAttribute::values_mut_calls = usize();

    auto slot                                            = storage.AppendSlot();
    binding.Write().Write(temperature_index)[slot.index] = 2.0f;
    EXPECT_FLOAT_EQ(binding.Write().Write(temperature_index)[slot.index], 2.0f);
    EXPECT_EQ(TemperatureAttribute::values_mut_calls, usize(1));

    storage.ResetSlot(slot);
    EXPECT_FLOAT_EQ(binding.Write().Write(temperature_index)[slot.index], 1.0f);
    EXPECT_EQ(TemperatureAttribute::values_mut_calls, usize(1));

    (void)storage.AppendSlot();
    (void)binding.Write().Write(temperature_index);
    EXPECT_EQ(TemperatureAttribute::values_mut_calls, usize(2));
}

TEST(ParticleSlotEvents, CombinesTransitionsInStableSlotOrder) {
    particle::ParticleSlotEvents events;
    events.RecordDeath(particle::ParticleSlot { .index = usize(2) });
    events.RecordSpawn(particle::ParticleSlot { .index = usize() });
    events.RecordSpawn(particle::ParticleSlot { .index = usize(2) });

    ASSERT_EQ(events.transitions.len(), usize(3));
    EXPECT_EQ(events.transitions[usize()].slot.index, usize());
    EXPECT_TRUE(events.transitions[usize()].spawned);
    EXPECT_FALSE(events.transitions[usize()].died);
    EXPECT_EQ(events.transitions[usize(1)].slot.index, usize(1));
    EXPECT_FALSE(events.transitions[usize(1)].spawned);
    EXPECT_FALSE(events.transitions[usize(1)].died);
    EXPECT_EQ(events.transitions[usize(2)].slot.index, usize(2));
    EXPECT_TRUE(events.transitions[usize(2)].spawned);
    EXPECT_TRUE(events.transitions[usize(2)].died);
}

TEST(ParticleProgram, RunsPreparedProgramsInContractOrder) {
    particle::ParticleSchemaBuilder builder;
    auto temperature = Register<TemperatureAttribute>(builder, "temperature"_str, "test"_str, 0.0f);
    rstd::vec::Vec<i32> trace;
    usize               spawned {};

    particle::ParticleProgram program;
    program.AddEmitter(Box<dyn<particle::ParticleEmitterProgram>>::make(
        TraceEmitter { .trace = rstd::addressof(trace) }));
    program.AddSpawn(Box<dyn<particle::ParticleSpawnProgram>>::make(
        TraceSpawn { .trace = rstd::addressof(trace), .temperature = temperature }));
    program.AddLifecycle(Box<dyn<particle::ParticleLifecycleProgram>>::make(
        TraceLifecycle { .trace = rstd::addressof(trace) }));
    program.AddEvent(Box<dyn<particle::ParticleEventProgram>>::make(
        TraceEvent { .trace = rstd::addressof(trace), .spawned = rstd::addressof(spawned) }));
    program.AddUpdate(Box<dyn<particle::ParticleUpdateProgram>>::make(
        TraceUpdate { .trace = rstd::addressof(trace), .value = i32(5) }));
    program.AddPostUpdate(Box<dyn<particle::ParticleUpdateProgram>>::make(
        TraceUpdate { .trace = rstd::addressof(trace), .value = i32(6) }));
    program.AddExtractor(Box<dyn<particle::ParticleExtractProgram>>::make(
        TraceExtract { .trace = rstd::addressof(trace) }));

    auto definition = particle::ParticleDefinition::Prepare(
        rstd::move(builder).Build(), rstd::move(program), usize(4));
    ASSERT_TRUE(definition.is_ok());
    particle::ParticleSystem system(rstd::move(definition).unwrap());
    auto&                    instance = system.CreateInstance();
    EmptyFrame               frame;
    auto                     frame_ref = rstd::dyn<rstd::any::Any>::from_ref(frame).as_ref();
    system.Advance(instance, frame_ref, f64(1.0 / 60.0), f64(1.0 / 60.0));
    system.Extract(frame_ref);

    const rstd::array<i32, 7> expected {
        i32(3), i32(1), i32(2), i32(4), i32(5), i32(6), i32(7),
    };
    ASSERT_EQ(trace.len(), expected.len());
    for (usize index {}; index < trace.len(); ++index) EXPECT_EQ(trace[index], expected[index]);
    EXPECT_EQ(spawned, usize(1));
    particle::ParticleSlotReader value(instance.Storage(), particle::ParticleSlot {});
    EXPECT_FLOAT_EQ(value.Read(temperature), 42.0f);
    EXPECT_FALSE(value.Read(instance.Storage().SlotStateKey()).fresh);
}

// Wallpaper Engine at 20 frames a second or less (FUN_140236cd0): ages, emitters and initializers
// once with the whole step, then the operator pass twice with half dt and half dt2; post-updates
// (trails) still once a frame.
TEST(ParticleProgram, RunsTheOperatorPassTwiceWithHalfSteps) {
    particle::ParticleSchemaBuilder builder;
    auto temperature = Register<TemperatureAttribute>(builder, "temperature"_str, "test"_str, 0.0f);
    rstd::vec::Vec<i32>    trace;
    rstd::vec::Vec<double> deltas;
    rstd::vec::Vec<double> deltas2;
    rstd::vec::Vec<double> post;
    usize                  spawned {};

    particle::ParticleProgram program;
    program.AddEmitter(Box<dyn<particle::ParticleEmitterProgram>>::make(
        TraceEmitter { .trace = rstd::addressof(trace) }));
    program.AddSpawn(Box<dyn<particle::ParticleSpawnProgram>>::make(
        TraceSpawn { .trace = rstd::addressof(trace), .temperature = temperature }));
    program.AddLifecycle(Box<dyn<particle::ParticleLifecycleProgram>>::make(
        TraceLifecycle { .trace = rstd::addressof(trace) }));
    program.AddEvent(Box<dyn<particle::ParticleEventProgram>>::make(
        TraceEvent { .trace = rstd::addressof(trace), .spawned = rstd::addressof(spawned) }));
    program.AddUpdate(Box<dyn<particle::ParticleUpdateProgram>>::make(
        TraceUpdate { .trace = rstd::addressof(trace), .value = i32(5) }));
    program.AddUpdate(Box<dyn<particle::ParticleUpdateProgram>>::make(DeltaTraceUpdate {
        .deltas = rstd::addressof(deltas), .deltas2 = rstd::addressof(deltas2) }));
    program.AddPostUpdate(Box<dyn<particle::ParticleUpdateProgram>>::make(
        TraceUpdate { .trace = rstd::addressof(trace), .value = i32(6) }));
    program.AddPostUpdate(Box<dyn<particle::ParticleUpdateProgram>>::make(
        DeltaTraceUpdate { .deltas = rstd::addressof(post), .deltas2 = nullptr }));

    auto definition = particle::ParticleDefinition::Prepare(
        rstd::move(builder).Build(), rstd::move(program), usize(4));
    ASSERT_TRUE(definition.is_ok());
    particle::ParticleSystem system(rstd::move(definition).unwrap());
    auto&                    instance = system.CreateInstance();
    EmptyFrame               frame;
    auto                     frame_ref = rstd::dyn<rstd::any::Any>::from_ref(frame).as_ref();
    system.Advance(instance,
                   frame_ref,
                   f64(1.0 / 15.0),
                   f64(1.0 / 15.0),
                   particle::ParticleOperatorSteps { .passes = u32(2), .delta2 = Some(f64(0.05)) });

    const rstd::array<i32, 7> expected {
        i32(3), i32(1), i32(2), i32(4), i32(5), i32(5), i32(6),
    };
    ASSERT_EQ(trace.len(), expected.len());
    for (usize index {}; index < trace.len(); ++index) EXPECT_EQ(trace[index], expected[index]);
    ASSERT_EQ(deltas.len(), usize(2));
    for (usize index {}; index < usize(2); ++index) {
        EXPECT_DOUBLE_EQ(deltas[index], 1.0 / 30.0);
        EXPECT_DOUBLE_EQ(deltas2[index], 0.025);
    }
    ASSERT_EQ(post.len(), usize(1));
    EXPECT_DOUBLE_EQ(post[usize()], 1.0 / 15.0);
}

TEST(ParticleProgram, OnePassUsesTheStepAsDt2ByDefault) {
    rstd::vec::Vec<double>    deltas;
    rstd::vec::Vec<double>    deltas2;
    particle::ParticleProgram program;
    program.AddUpdate(Box<dyn<particle::ParticleUpdateProgram>>::make(DeltaTraceUpdate {
        .deltas = rstd::addressof(deltas), .deltas2 = rstd::addressof(deltas2) }));
    auto definition = particle::ParticleDefinition::Prepare(
        particle::ParticleSchemaBuilder {}.Build(), rstd::move(program), usize(4));
    ASSERT_TRUE(definition.is_ok());
    particle::ParticleSystem system(rstd::move(definition).unwrap());
    auto&                    instance = system.CreateInstance();
    EmptyFrame               frame;
    auto                     frame_ref = rstd::dyn<rstd::any::Any>::from_ref(frame).as_ref();
    system.Advance(instance, frame_ref, f64(1.0 / 60.0), f64(1.0 / 60.0));
    ASSERT_EQ(deltas.len(), usize(1));
    EXPECT_DOUBLE_EQ(deltas[usize()], 1.0 / 60.0);
    EXPECT_DOUBLE_EQ(deltas2[usize()], 1.0 / 60.0);
}

TEST(ParticleProgram, ReusesExpiredCapacityWithoutAnEmptyFrame) {
    particle::ParticleSchemaBuilder builder;
    auto lifetime = Register<TemperatureAttribute>(builder, "lifetime"_str, "test"_str, 0.0f);
    rstd::vec::Vec<i32> transitions;

    particle::ParticleProgram program;
    program.AddEmitter(Box<dyn<particle::ParticleEmitterProgram>>::make(ContinuousEmitter {}));
    program.AddSpawn(
        Box<dyn<particle::ParticleSpawnProgram>>::make(LifetimeSpawn { .lifetime = lifetime }));
    program.AddLifecycle(Box<dyn<particle::ParticleLifecycleProgram>>::make(
        ExpiringLifecycle { .lifetime = lifetime }));
    program.AddEvent(Box<dyn<particle::ParticleEventProgram>>::make(
        TransitionTraceEvent { .trace = rstd::addressof(transitions) }));

    auto definition = particle::ParticleDefinition::Prepare(
        rstd::move(builder).Build(), rstd::move(program), usize(1));
    ASSERT_TRUE(definition.is_ok());
    particle::ParticleSystem system(rstd::move(definition).unwrap());
    auto&                    instance = system.CreateInstance();
    EmptyFrame               frame;
    auto                     frame_ref = rstd::dyn<rstd::any::Any>::from_ref(frame).as_ref();

    system.Advance(instance, frame_ref, f64(1.0), f64(1.0));
    EXPECT_TRUE(instance.Storage().Values(instance.Storage().SlotStateKey())[usize()].active);
    system.Advance(instance, frame_ref, f64(1.0), f64(2.0));
    EXPECT_TRUE(instance.Storage().Values(instance.Storage().SlotStateKey())[usize()].active);
    EXPECT_FLOAT_EQ(instance.Storage().Values(lifetime)[usize()], 1.0f);

    const rstd::array<i32, 3> expected { i32(1), i32(-1), i32(1) };
    ASSERT_EQ(transitions.len(), expected.len());
    for (usize index {}; index < transitions.len(); ++index) {
        EXPECT_EQ(transitions[index], expected[index]);
    }
}

TEST(ParticleSubSystem, DerivesMeshCapacityFromItsOwnInstancePool) {
    using SubSystem = owe::ParticleSubSystem;
    using SpawnType = SubSystem::SpawnType;

    auto static_capacity = SubSystem::MaxParticleCapacity(u32(500), u32(20), SpawnType::STATIC);
    ASSERT_TRUE(static_capacity.is_some());
    EXPECT_EQ(*static_capacity, u32(500));

    auto event_capacity =
        SubSystem::MaxParticleCapacity(u32(100), u32(20), SpawnType::EVENT_FOLLOW);
    ASSERT_TRUE(event_capacity.is_some());
    EXPECT_EQ(*event_capacity, u32(2000));

    EXPECT_TRUE(
        SubSystem::MaxParticleCapacity(u32::MAX, u32(2), SpawnType::EVENT_FOLLOW).is_none());
}

TEST(ParticleSubSystem, PlaybackResetClearsAndRestartsIndependentStorage) {
    owe::Scene             scene;
    auto                   mesh = Arc<owe::SceneMesh>::make();
    owe::ParticleSubSystem subsystem(scene,
                                     mesh.clone(),
                                     u32(4),
                                     f64(1.0),
                                     u32(1),
                                     f64(1.0),
                                     owe::ParticleSubSystem::SpawnType::STATIC,
                                     owe::ParticleAnimationSpec {});
    auto                   playback = rstd::sync::Arc<owe::ParticlePlaybackState>::make();
    subsystem.SetPlaybackState(playback.clone());
    subsystem.AddInitializer(owe::ParticleParser::GenInitializer(
        owe::ParseJson(R"({"name":"lifetimerandom","min":10,"max":10})"_str).unwrap(), u32(4)));
    subsystem.AddEmitter(Box<dyn<particle::ParticleEmitterProgram>>::make(
        owe::SphereEmitterProgram(subsystem.SpawnPipeline(),
                                  owe::ParticleSphereEmitterArgs {
                                      .directions    = { 1.0f, 1.0f, 0.0f },
                                      .instantaneous = u32(1),
                                  },
                                  usize())));
    subsystem.Finalize();
    subsystem.Tick(f64(1.0 / 60.0), false);
    ASSERT_EQ(subsystem.System().InstanceCount(), usize(1));
    EXPECT_GT(subsystem.System().Instance(usize()).Storage().Len(), usize());

    playback->playing.store(false, rstd::sync::atomic::Ordering::Release);
    playback->reset_sequence.fetch_add(u32(1), rstd::sync::atomic::Ordering::AcqRel);
    subsystem.Tick(f64(1.0 / 60.0), false);
    EXPECT_EQ(subsystem.System().Instance(usize()).Storage().Len(), usize());

    playback->playing.store(true, rstd::sync::atomic::Ordering::Release);
    playback->reset_sequence.fetch_add(u32(1), rstd::sync::atomic::Ordering::AcqRel);
    subsystem.Tick(f64(1.0 / 60.0), false);
    EXPECT_GT(subsystem.System().Instance(usize()).Storage().Len(), usize());
}

TEST(ParticleSubSystem, CursorEmittersRunWhileStationaryAndStopOutsideTheOutput) {
    for (auto name : { "sphererandom"_str, "boxrandom"_str }) {
        owe::Scene scene;
        scene.SetPointerPosition({ 0.25f, 0.5f });
        auto subsystem                                    = MakeTimedParticleSubsystem(scene, name);
        subsystem->ControlpointsMut()[usize()].link_mouse = true;
        auto owner                                        = rstd::sync::Arc<owe::SceneNode>::make();
        subsystem->SetOwnerNode(owner.as_ptr());

        // No pointer has entered this output yet.
        subsystem->Tick(f64(1.0 / 64.0), false);
        EXPECT_EQ(LiveParticleCount(*subsystem), usize());
        scene.SetPointerInWindow(true);
        subsystem->Tick(f64(1.0 / 64.0), false);
        ASSERT_EQ(LiveParticleCount(*subsystem), usize(1));

        // A stationary pointer keeps emitting, including above other windows.
        for (int i = 0; i < 10; ++i) {
            scene.SetPointerPosition({ 0.25f, 0.5f });
            owner->SetTranslate({ static_cast<float>(i), 0.0f, 0.0f });
            subsystem->Tick(f64(1.0 / 64.0), false);
            EXPECT_EQ(LiveParticleCount(*subsystem), usize(static_cast<rstd::size_t>(i + 2)));
        }
        // Leaving stops emission while existing particles continue to age.
        scene.SetPointerInWindow(false);
        subsystem->Tick(f64(1.0 / 64.0), false);
        EXPECT_EQ(LiveParticleCount(*subsystem), usize(11));
        for (int i = 0; i < 256; ++i) subsystem->Tick(f64(1.0 / 64.0), false);
        EXPECT_EQ(LiveParticleCount(*subsystem), usize());
        // Re-entering at the same coordinates restarts without a backlog.
        scene.SetPointerInWindow(true);
        subsystem->Tick(f64(1.0 / 64.0), false);
        EXPECT_EQ(LiveParticleCount(*subsystem), usize(1));
        subsystem->Tick(f64(1.0 / 64.0), false);
        EXPECT_EQ(LiveParticleCount(*subsystem), usize(2));
    }
}

TEST(ParticleSubSystem, MouseForceControlpointsDoNotStopIndependentEmitters) {
    for (auto name : { "sphererandom"_str, "boxrandom"_str }) {
        owe::Scene scene;
        auto       subsystem = MakeTimedParticleSubsystem(scene, name);
        // Dust can react to the cursor at point 1 while emitting from point 0.
        subsystem->ControlpointsMut()[usize(1)].link_mouse = true;
        subsystem->Tick(f64(1.0 / 64.0), false);
        EXPECT_EQ(LiveParticleCount(*subsystem), usize(1));
        subsystem->Tick(f64(1.0 / 64.0), false);
        EXPECT_EQ(LiveParticleCount(*subsystem), usize(2));
    }
}

TEST(ParticleSubSystem, ExplicitCursorParticleEmissionsWorkOutsideTheOutput) {
    for (auto name : { "sphererandom"_str, "boxrandom"_str }) {
        owe::Scene scene;
        auto       subsystem = MakeTimedParticleSubsystem(scene, name, i32(2));
        subsystem->ControlpointsMut()[usize(2)].link_mouse = true;
        auto playback = rstd::sync::Arc<owe::ParticlePlaybackState>::make();
        subsystem->SetPlaybackState(playback.clone());
        subsystem->Tick(f64(1.0 / 64.0), false);
        EXPECT_EQ(LiveParticleCount(*subsystem), usize());
        playback->pending_emit_count.store(u32(3));
        subsystem->Tick(f64(1.0 / 64.0), false);
        EXPECT_EQ(LiveParticleCount(*subsystem), usize(3));
        subsystem->Tick(f64(1.0 / 64.0), false);
        EXPECT_EQ(LiveParticleCount(*subsystem), usize(3));
    }
}

TEST(ParticleSubSystem, CursorEmitterDurationElapsesOutsideTheOutput) {
    for (auto name : { "sphererandom"_str, "boxrandom"_str }) {
        owe::Scene scene;
        scene.SetPointerPosition({ 0.25f, 0.5f });
        auto subsystem = MakeTimedParticleSubsystem(scene, name, i32(), 0.125f);
        subsystem->ControlpointsMut()[usize()].link_mouse = true;
        subsystem->Tick(f64(0.25), false);
        scene.SetPointerInWindow(true);
        subsystem->Tick(f64(1.0 / 64.0), false);
        EXPECT_EQ(LiveParticleCount(*subsystem), usize());
    }
}

TEST(ParticleSubSystem, ConvertsWorldSpaceFollowAnchorsIntoChildLocalSpace) {
    owe::Scene scene;
    auto       parent_node =
        rstd::sync::Arc<owe::SceneNode>::make(Eigen::Vector3f { 100.0f, 200.0f, 0.0f },
                                              Eigen::Vector3f { 2.0f, 3.0f, 1.0f },
                                              Eigen::Vector3f::Zero());
    auto child_node = rstd::sync::Arc<owe::SceneNode>::make(Eigen::Vector3f { 10.0f, -20.0f, 0.0f },
                                                            Eigen::Vector3f { 0.5f, 2.0f, 1.0f },
                                                            Eigen::Vector3f::Zero());
    parent_node->AppendChild(child_node.clone());

    owe::ParticleSubSystem parent(scene,
                                  Arc<owe::SceneMesh>::make(),
                                  u32(1),
                                  f64(),
                                  u32(1),
                                  f64(1.0),
                                  owe::ParticleSubSystem::SpawnType::STATIC,
                                  owe::ParticleAnimationSpec {},
                                  {},
                                  u32(),
                                  f64(),
                                  f64(),
                                  true);
    parent.SetOwnerNode(parent_node.as_ptr());
    parent.AddInitializer(owe::ParticleParser::GenInitializer(
        owe::ParseJson(R"({"name":"lifetimerandom","min":10,"max":10})"_str).unwrap(), u32(1)));
    parent.AddEmitter(Box<dyn<particle::ParticleEmitterProgram>>::make(
        owe::SphereEmitterProgram(parent.SpawnPipeline(),
                                  owe::ParticleSphereEmitterArgs {
                                      .origin        = { 5.0f, 6.0f, 0.0f },
                                      .instantaneous = u32(1),
                                  },
                                  usize())));

    auto child = Box<owe::ParticleSubSystem>::make(scene,
                                                   Arc<owe::SceneMesh>::make(),
                                                   u32(1),
                                                   f64(),
                                                   u32(1),
                                                   f64(1.0),
                                                   owe::ParticleSubSystem::SpawnType::EVENT_FOLLOW,
                                                   owe::ParticleAnimationSpec {});
    child->SetOwnerNode(child_node.as_ptr());
    child->Finalize();
    auto* child_system = child.get();
    parent.AddChild(rstd::move(child));
    parent.Finalize();

    parent.Tick(f64(), false);

    ASSERT_EQ(parent.System().InstanceCount(), usize(1));
    ASSERT_EQ(child_system->System().InstanceCount(), usize(1));
    auto parent_position = parent.System().Instance(usize()).Binding().Read().Positions()[usize()];
    auto child_anchor    = child_system->InstanceState(usize()).bounded.position;
    EXPECT_TRUE(child_anchor.allFinite());
    child_node->UpdateTrans();
    Eigen::Vector4d resolved =
        child_node->ModelTrans() *
        Eigen::Vector4d(child_anchor.x(), child_anchor.y(), child_anchor.z(), 1.0);
    EXPECT_TRUE(resolved.head<3>().cast<float>().isApprox(parent_position));
}

// A child made by eventspawn stays where its parent's particle was born; one made by eventfollow
// goes with the particle (particles-event-children).
TEST(ParticleSubSystem, AnEventSpawnChildStaysWhereTheParticleWasBornAndAFollowChildMoves) {
    auto child_position = [](owe::ParticleSubSystem::SpawnType type) {
        owe::Scene scene;
        auto       parent_node = rstd::sync::Arc<owe::SceneNode>::make();
        auto       child_node  = rstd::sync::Arc<owe::SceneNode>::make();
        parent_node->AppendChild(child_node.clone());
        owe::ParticleSubSystem parent(scene,
                                      Arc<owe::SceneMesh>::make(),
                                      u32(1),
                                      f64(1.0),
                                      u32(1),
                                      f64(1.0),
                                      owe::ParticleSubSystem::SpawnType::STATIC,
                                      owe::ParticleAnimationSpec {});
        parent.SetOwnerNode(parent_node.as_ptr());
        parent.AddInitializer(owe::ParticleParser::GenInitializer(
            owe::ParseJson(R"({"name":"lifetimerandom","min":10,"max":10})"_str).unwrap(), u32(1)));
        parent.AddInitializer(owe::ParticleParser::GenInitializer(
            owe::ParseJson(R"({"name":"velocityrandom","min":"30 0 0","max":"30 0 0"})"_str)
                .unwrap(),
            u32(1)));
        parent.AddEmitter(Box<dyn<particle::ParticleEmitterProgram>>::make(
            owe::BoxEmitterProgram(parent.SpawnPipeline(),
                                   owe::ParticleBoxEmitterArgs { .instantaneous = u32(1) },
                                   usize())));
        owe::ParticleInstanceModifiers modifiers(
            Arc<owe::wpscene::ParticleInstanceoverride>::make(),
            owe::wpscene::Particle::EFlags { 0 },
            false);
        parent.AddOperator(
            owe::ParticleParser::GenOperator(owe::ParseJson(R"({"name":"movement"})"_str).unwrap(),
                                             modifiers.Clone(),
                                             parent,
                                             usize()));
        auto child = Box<owe::ParticleSubSystem>::make(scene,
                                                       Arc<owe::SceneMesh>::make(),
                                                       u32(1),
                                                       f64(),
                                                       u32(1),
                                                       f64(1.0),
                                                       type,
                                                       owe::ParticleAnimationSpec {});
        child->SetOwnerNode(child_node.as_ptr());
        child->Finalize();
        auto* child_system = child.get();
        parent.AddChild(rstd::move(child));
        parent.Finalize();
        for (int tick = 0; tick < 12; ++tick) parent.Tick(f64(1.0 / 4.0), false);
        EXPECT_EQ(child_system->System().InstanceCount(), usize(1));
        EXPECT_GT(parent.System().Instance(usize()).Binding().Read().Positions()[usize()].x(),
                  60.0f);
        return child_system->InstanceState(usize()).bounded.position;
    };
    // the particle has gone 30 a second for 3 seconds by the end
    EXPECT_NEAR(child_position(owe::ParticleSubSystem::SpawnType::EVENT_SPAWN).x(), 0.0f, 1.0f);
    EXPECT_GT(child_position(owe::ParticleSubSystem::SpawnType::EVENT_FOLLOW).x(), 60.0f);
}

// inheritinitialvaluefromevent and inheritvaluefromevent give a child's particle its parent
// particle's colour, and nothing else (particles-inherit-event).
TEST(ParticleSubSystem, ChildParticlesInheritTheirParentsColourFromTheEvent) {
    auto child_color =
        [](owe::ParticleSubSystem::SpawnType type, ref<str> initializer, ref<str> operator_json) {
            owe::Scene scene;
            auto       parent_node = rstd::sync::Arc<owe::SceneNode>::make();
            auto       child_node  = rstd::sync::Arc<owe::SceneNode>::make();
            parent_node->AppendChild(child_node.clone());
            owe::ParticleSubSystem parent(scene,
                                          Arc<owe::SceneMesh>::make(),
                                          u32(1),
                                          f64(),
                                          u32(1),
                                          f64(1.0),
                                          owe::ParticleSubSystem::SpawnType::STATIC,
                                          owe::ParticleAnimationSpec {});
            parent.SetOwnerNode(parent_node.as_ptr());
            parent.AddInitializer(owe::ParticleParser::GenInitializer(
                owe::ParseJson(R"({"name":"lifetimerandom","min":10,"max":10})"_str).unwrap(),
                u32(1)));
            parent.AddInitializer(owe::ParticleParser::GenInitializer(
                owe::ParseJson(R"({"name":"colorrandom","min":"255 128 0","max":"255 128 0"})"_str)
                    .unwrap(),
                u32(1)));
            parent.AddEmitter(Box<dyn<particle::ParticleEmitterProgram>>::make(
                owe::BoxEmitterProgram(parent.SpawnPipeline(),
                                       owe::ParticleBoxEmitterArgs { .instantaneous = u32(1) },
                                       usize())));
            auto child = Box<owe::ParticleSubSystem>::make(scene,
                                                           Arc<owe::SceneMesh>::make(),
                                                           u32(1),
                                                           f64(),
                                                           u32(1),
                                                           f64(1.0),
                                                           type,
                                                           owe::ParticleAnimationSpec {});
            child->SetOwnerNode(child_node.as_ptr());
            child->AddInitializer(owe::ParticleParser::GenInitializer(
                owe::ParseJson(R"({"name":"lifetimerandom","min":10,"max":10})"_str).unwrap(),
                u32(1)));
            child->AddInitializer(owe::ParticleParser::GenInitializer(
                owe::ParseJson(
                    R"({"name":"colorrandom","min":"255 255 255","max":"255 255 255"})"_str)
                    .unwrap(),
                u32(1)));
            if (initializer.len() != usize())
                child->AddInitializer(owe::ParticleParser::GenInitializer(
                    owe::ParseJson(initializer).unwrap(), u32(1)));
            child->AddEmitter(Box<dyn<particle::ParticleEmitterProgram>>::make(
                owe::BoxEmitterProgram(child->SpawnPipeline(),
                                       owe::ParticleBoxEmitterArgs { .instantaneous = u32(1) },
                                       usize())));
            if (operator_json.len() != usize()) {
                owe::ParticleInstanceModifiers modifiers(
                    Arc<owe::wpscene::ParticleInstanceoverride>::make(),
                    owe::wpscene::Particle::EFlags { 0 },
                    false);
                child->AddOperator(owe::ParticleParser::GenOperator(
                    owe::ParseJson(operator_json).unwrap(), modifiers.Clone(), *child, usize()));
            }
            child->Finalize();
            auto* child_system = child.get();
            parent.AddChild(rstd::move(child));
            parent.Finalize();
            for (int tick = 0; tick < 4; ++tick) parent.Tick(f64(1.0 / 60.0), false);
            auto& storage = child_system->System().Instance(usize()).Storage();
            particle::ParticleSlotReader value(storage, particle::ParticleSlot {});
            return Eigen::Vector3f(value.Read(child_system->Attributes().color));
        };
    const auto spawn  = owe::ParticleSubSystem::SpawnType::EVENT_SPAWN;
    const auto follow = owe::ParticleSubSystem::SpawnType::EVENT_FOLLOW;
    EXPECT_TRUE(child_color(spawn, ""_str, ""_str).isApprox(Eigen::Vector3f(1, 1, 1)));
    EXPECT_TRUE(child_color(spawn, R"({"name":"inheritinitialvaluefromevent"})"_str, ""_str)
                    .isApprox(Eigen::Vector3f(1, 128.0f / 255.0f, 0)));
    EXPECT_TRUE(child_color(follow, ""_str, R"({"name":"inheritvaluefromevent"})"_str)
                    .isApprox(Eigen::Vector3f(1, 128.0f / 255.0f, 0)));
}

// The layerimage emitter (particles-layerimage) emits from the shape of an image layer: a solid
// layer's rectangle (its size, moved with the layer), in the layer's colour, and nothing from a
// layer less than about half opaque.
TEST(ParticleSubSystem, LayerImageEmitterFillsTheLayersRectangleInItsColour) {
    struct Emitted {
        usize count {};
        bool  inside { true };
        bool  red { true };
        float min_x { 1e9f }, max_x { -1e9f }, min_y { 1e9f }, max_y { -1e9f };
    };
    auto emit = [](float alpha) {
        owe::Scene scene;
        auto layer = rstd::sync::Arc<owe::SceneNode>::make(Eigen::Vector3f { 100.0f, 50.0f, 0.0f },
                                                           Eigen::Vector3f { 1.0f, 1.0f, 1.0f },
                                                           Eigen::Vector3f::Zero());
        owe::ParticleSubSystem subsystem(scene,
                                         Arc<owe::SceneMesh>::make(),
                                         u32(64),
                                         f64(1.0),
                                         u32(1),
                                         f64(1.0),
                                         owe::ParticleSubSystem::SpawnType::STATIC,
                                         owe::ParticleAnimationSpec {});
        subsystem.AddInitializer(owe::ParticleParser::GenInitializer(
            owe::ParseJson(R"({"name":"lifetimerandom","min":10,"max":10})"_str).unwrap(),
            u32(64)));
        owe::wpscene::Emitter emitter;
        emitter.name          = String::make("layerimage"_str);
        emitter.instantaneous = u32(40);
        auto source           = Arc<owe::LayerImageSource>::make(
            owe::LayerImageSource { .node        = layer.clone(),
                                    .half_width  = 20.0,
                                    .half_height = 10.0,
                                    .color       = Eigen::Vector3f(1.0f, 0.0f, 0.0f),
                                    .alpha       = alpha });
        subsystem.AddEmitter(
            owe::ParticleParser::GenEmitter(emitter, subsystem, usize(), Some(rstd::move(source))));
        subsystem.Finalize();
        subsystem.Tick(f64(1.0 / 60.0), false);
        Emitted result;
        result.count  = LiveParticleCount(subsystem);
        auto& storage = subsystem.System().Instance(usize()).Storage();
        for (usize index {}; index < result.count; ++index) {
            particle::ParticleSlotReader value(storage, particle::ParticleSlot { .index = index });
            auto position = Eigen::Vector3f(value.Read(subsystem.Attributes().position));
            auto color    = Eigen::Vector3f(value.Read(subsystem.Attributes().color));
            result.inside = result.inside && position.x() >= 80.0f - 1e-3f &&
                            position.x() <= 120.0f + 1e-3f && position.y() >= 40.0f - 1e-3f &&
                            position.y() <= 60.0f + 1e-3f;
            result.red    = result.red && color.isApprox(Eigen::Vector3f(1, 0, 0));
            result.min_x  = std::min(result.min_x, position.x());
            result.max_x  = std::max(result.max_x, position.x());
            result.min_y  = std::min(result.min_y, position.y());
            result.max_y  = std::max(result.max_y, position.y());
        }
        return result;
    };
    auto opaque = emit(1.0f);
    EXPECT_EQ(opaque.count, usize(40));
    EXPECT_TRUE(opaque.inside);
    EXPECT_TRUE(opaque.red);
    EXPECT_GT(opaque.max_x - opaque.min_x, 20.0f);
    EXPECT_GT(opaque.min_x, 79.9f);
    EXPECT_LT(opaque.max_x, 120.1f);
    EXPECT_GT(opaque.min_y, 39.9f);
    EXPECT_LT(opaque.max_y, 60.1f);
    EXPECT_GT(opaque.max_y - opaque.min_y, 8.0f);
    EXPECT_EQ(emit(0.3f).count, usize());
    EXPECT_EQ(emit(0.5f).count, usize(40));
}

// A layer with pixels (a texture's opaque ones) emits from those: here two points, 10 left of the
// layer's centre (green) and 10 right (blue), each within half a unit; the layer's colour
// multiplies theirs.
TEST(ParticleSubSystem, LayerImageEmitterEmitsFromThePixelsOfAnImage) {
    owe::Scene scene;
    auto layer = rstd::sync::Arc<owe::SceneNode>::make(Eigen::Vector3f { 100.0f, 50.0f, 0.0f },
                                                       Eigen::Vector3f { 1.0f, 1.0f, 1.0f },
                                                       Eigen::Vector3f::Zero());
    owe::ParticleSubSystem subsystem(scene,
                                     Arc<owe::SceneMesh>::make(),
                                     u32(64),
                                     f64(1.0),
                                     u32(1),
                                     f64(1.0),
                                     owe::ParticleSubSystem::SpawnType::STATIC,
                                     owe::ParticleAnimationSpec {});
    subsystem.AddInitializer(owe::ParticleParser::GenInitializer(
        owe::ParseJson(R"({"name":"lifetimerandom","min":10,"max":10})"_str).unwrap(), u32(64)));
    owe::wpscene::Emitter emitter;
    emitter.name          = String::make("layerimage"_str);
    emitter.instantaneous = u32(40);
    owe::LayerImageSource image { .node        = layer.clone(),
                                  .half_width  = 20.0,
                                  .half_height = 10.0,
                                  .color       = Eigen::Vector3f(1.0f, 1.0f, 0.5f),
                                  .alpha       = 1.0f };
    image.points.push(
        owe::LayerImagePoint { .x = -10.0f, .y = 0.0f, .color = Eigen::Vector3f(0, 1, 0) });
    image.points.push(
        owe::LayerImagePoint { .x = 10.0f, .y = 0.0f, .color = Eigen::Vector3f(0, 0, 1) });
    subsystem.AddEmitter(owe::ParticleParser::GenEmitter(
        emitter, subsystem, usize(), Some(Arc<owe::LayerImageSource>::make(rstd::move(image)))));
    subsystem.Finalize();
    subsystem.Tick(f64(1.0 / 60.0), false);
    ASSERT_EQ(LiveParticleCount(subsystem), usize(40));
    auto& storage = subsystem.System().Instance(usize()).Storage();
    int   left {}, right {};
    for (usize index {}; index < usize(40); ++index) {
        particle::ParticleSlotReader value(storage, particle::ParticleSlot { .index = index });
        auto position = Eigen::Vector3f(value.Read(subsystem.Attributes().position));
        auto color    = Eigen::Vector3f(value.Read(subsystem.Attributes().color));
        EXPECT_NEAR(position.y(), 50.0f, 0.51f);
        if (position.x() < 100.0f) {
            ++left;
            EXPECT_NEAR(position.x(), 90.0f, 0.51f);
            EXPECT_TRUE(color.isApprox(Eigen::Vector3f(0, 1, 0)));
        } else {
            ++right;
            EXPECT_NEAR(position.x(), 110.0f, 0.51f);
            EXPECT_TRUE(color.isApprox(Eigen::Vector3f(0, 0, 0.5f)));
        }
    }
    EXPECT_GT(left, 0);
    EXPECT_GT(right, 0);
}

// A colorrandom initializer multiplies the colour the emitter gave (a layer's pixels), it does not
// replace it (particles-layerimage-color: white keeps the rainbow, red keeps only its red).
TEST(ParticleSubSystem, ColorRandomMultipliesAnImageEmittersColours) {
    owe::Scene scene;
    auto layer = rstd::sync::Arc<owe::SceneNode>::make(Eigen::Vector3f { 100.0f, 50.0f, 0.0f },
                                                       Eigen::Vector3f { 1.0f, 1.0f, 1.0f },
                                                       Eigen::Vector3f::Zero());
    owe::ParticleSubSystem subsystem(scene,
                                     Arc<owe::SceneMesh>::make(),
                                     u32(64),
                                     f64(1.0),
                                     u32(1),
                                     f64(1.0),
                                     owe::ParticleSubSystem::SpawnType::STATIC,
                                     owe::ParticleAnimationSpec {});
    subsystem.AddInitializer(owe::ParticleParser::GenInitializer(
        owe::ParseJson(R"({"name":"lifetimerandom","min":10,"max":10})"_str).unwrap(), u32(64)));
    subsystem.AddInitializer(owe::ParticleParser::GenInitializer(
        owe::ParseJson(R"({"name":"colorrandom","min":"255 128 255","max":"255 128 255"})"_str)
            .unwrap(),
        u32(64)));
    owe::wpscene::Emitter emitter;
    emitter.name          = String::make("layerimage"_str);
    emitter.instantaneous = u32(40);
    owe::LayerImageSource image { .node        = layer.clone(),
                                  .half_width  = 20.0,
                                  .half_height = 10.0,
                                  .color       = Eigen::Vector3f(1.0f, 1.0f, 1.0f),
                                  .alpha       = 1.0f };
    image.points.push(
        owe::LayerImagePoint { .x = -10.0f, .y = 0.0f, .color = Eigen::Vector3f(0, 1, 0) });
    image.points.push(
        owe::LayerImagePoint { .x = 10.0f, .y = 0.0f, .color = Eigen::Vector3f(1, 0, 1) });
    subsystem.AddEmitter(owe::ParticleParser::GenEmitter(
        emitter, subsystem, usize(), Some(Arc<owe::LayerImageSource>::make(rstd::move(image)))));
    subsystem.Finalize();
    subsystem.Tick(f64(1.0 / 60.0), false);
    ASSERT_EQ(LiveParticleCount(subsystem), usize(40));
    auto& storage = subsystem.System().Instance(usize()).Storage();
    for (usize index {}; index < usize(40); ++index) {
        particle::ParticleSlotReader value(storage, particle::ParticleSlot { .index = index });
        auto position = Eigen::Vector3f(value.Read(subsystem.Attributes().position));
        auto color    = Eigen::Vector3f(value.Read(subsystem.Attributes().color));
        const Eigen::Vector3f expected = position.x() < 100.0f
                                             ? Eigen::Vector3f(0, 128.0f / 255.0f, 0)
                                             : Eigen::Vector3f(1, 0, 1);
        EXPECT_TRUE(color.isApprox(expected, 1e-3f));
    }
}

// A solid layer emits from the same lattice, as a white image of its size
// (particles-layerimage-alpha: Wallpaper Engine's 300x150 rectangles end 2 units short at the
// bottom, where 150 leaves half a block).
TEST(LayerImageLattice, ASolidLayerIsAWhiteImageOfItsSize) {
    auto points = owe::SolidLayerLattice(300, 150);
    ASSERT_EQ(points.len(), usize(75 * 37));
    float min_x = 1e9f, max_x = -1e9f, min_y = 1e9f, max_y = -1e9f;
    for (const auto& point : points) {
        min_x = std::min(min_x, point.x);
        max_x = std::max(max_x, point.x);
        min_y = std::min(min_y, point.y);
        max_y = std::max(max_y, point.y);
        EXPECT_TRUE(point.color.isApprox(Eigen::Vector3f(1, 1, 1)));
    }
    EXPECT_EQ(min_x, -148.0f);
    EXPECT_EQ(max_x, 148.0f);
    EXPECT_EQ(max_y, 73.0f);
    EXPECT_EQ(min_y, -71.0f);
    // thinner than a block: one block across (as a texture)
    auto thin = owe::SolidLayerLattice(2, 10);
    ASSERT_EQ(thin.len(), usize(2));
    EXPECT_EQ(thin[usize()].x, 0.0f);
    EXPECT_EQ(thin[usize()].y, 3.0f);
    EXPECT_EQ(thin[usize(1)].y, -1.0f);
    EXPECT_TRUE(owe::SolidLayerLattice(0, 10).is_empty());
}

// A texture thinner than a block (Wallpaper Engine's gradient_rainbow is 64x2) still emits: its rows make one block
// row, one point every 4 texels across, in the average colour (particles-layerimage-alpha: a rainbow bar).
TEST(LayerImageLattice, ATextureThinnerThanABlockIsOneBlockAcross) {
    std::vector<unsigned char> thin;
    for (int y {}; y < 2; ++y)
        for (int x {}; x < 64; ++x)
            for (int v : { x < 32 ? 255 : 0, 0, x < 32 ? 0 : 255, 255 }) thin.push_back(static_cast<unsigned char>(v));
    auto points = owe::LayerImageLattice(thin.data(), 64, 2, 4, false, 1.0f);
    ASSERT_EQ(points.len(), usize(16));
    EXPECT_EQ(points[usize()].x, -30.0f);
    EXPECT_EQ(points[usize()].y, 0.0f);
    EXPECT_NEAR(points[usize()].color.x(), 1.0f, 1e-4f);
    EXPECT_NEAR(points[usize(15)].color.z(), 1.0f, 1e-4f);
}

// Wallpaper Engine reads a layer's image in 4x4 texel blocks (particles-layerimage-pitch, -block):
// one point at the centre of each block (texel 4i+2 from the top left, no jitter) when the block's
// average opacity times the layer's is half or more, in the block's average colour; the lattice
// follows the texture, whatever the scene's size.
TEST(LayerImageLattice, OnePointPerFourByFourBlockAtItsCentre) {
    auto image = [](int width, int height, auto texel) {
        std::vector<unsigned char> data;
        for (int y {}; y < height; ++y)
            for (int x {}; x < width; ++x)
                for (auto v : texel(x, y)) data.push_back(static_cast<unsigned char>(v));
        return data;
    };
    // 8x8 white, the top right block transparent: three points, at the block centres from the
    // image's centre (y up)
    auto white  = image(8, 8, [](int x, int y) {
        return rstd::array<int, 4> { 255, 255, 255, x >= 4 && y < 4 ? 0 : 255 };
    });
    auto points = owe::LayerImageLattice(white.data(), 8, 8, 4, false, 1.0f);
    ASSERT_EQ(points.len(), usize(3));
    EXPECT_EQ(points[usize()].x, -2.0f);
    EXPECT_EQ(points[usize()].y, 2.0f);
    EXPECT_EQ(points[usize(1)].x, -2.0f);
    EXPECT_EQ(points[usize(1)].y, -2.0f);
    EXPECT_EQ(points[usize(2)].x, 2.0f);
    EXPECT_EQ(points[usize(2)].y, -2.0f);
    // 40x24: 10x6 blocks
    auto wide = image(40, 24, [](int, int) {
        return rstd::array<int, 4> { 255, 255, 255, 255 };
    });
    EXPECT_EQ(owe::LayerImageLattice(wide.data(), 40, 24, 4, false, 1.0f).len(), usize(60));
    // the layer's opacity counts: half opaque still emits, below half nothing
    EXPECT_EQ(owe::LayerImageLattice(wide.data(), 40, 24, 4, false, 0.5f).len(), usize(60));
    EXPECT_EQ(owe::LayerImageLattice(wide.data(), 40, 24, 4, false, 0.4f).len(), usize());
    // only one texel in sixteen opaque: the block averages 1/16 and emits nothing
    auto sparse = image(32, 32, [](int x, int y) {
        return rstd::array<int, 4> { 255, 255, 255, x % 4 == 2 && y % 4 == 2 ? 255 : 0 };
    });
    EXPECT_EQ(owe::LayerImageLattice(sparse.data(), 32, 32, 4, false, 1.0f).len(), usize());
    // a block half red, half blue is purple
    auto halves = image(4, 4, [](int x, int) {
        return x < 2 ? rstd::array<int, 4> { 255, 0, 0, 255 }
                     : rstd::array<int, 4> { 0, 0, 255, 255 };
    });
    auto purple = owe::LayerImageLattice(halves.data(), 4, 4, 4, false, 1.0f);
    ASSERT_EQ(purple.len(), usize(1));
    EXPECT_NEAR(purple[usize()].color.x(), 0.5f, 0.01f);
    EXPECT_NEAR(purple[usize()].color.y(), 0.0f, 0.01f);
    EXPECT_NEAR(purple[usize()].color.z(), 0.5f, 0.01f);
}

TEST(ParticleInstanceOverride, TracksProvidedControlpoints) {
    auto json = owe::ParseJson(R"({"size":2,"controlpoint1":"120 240 0"})"_str).unwrap();
    owe::wpscene::ParticleInstanceoverride override;

    ASSERT_TRUE(override.FromJosn(json));
    EXPECT_TRUE(override.controlpoint[usize(0)].is_none());
    ASSERT_TRUE(override.controlpoint[usize(1)].is_some());
    EXPECT_FLOAT_EQ((*override.controlpoint[usize(1)])[usize(0)], 120.0f);
    EXPECT_FLOAT_EQ((*override.controlpoint[usize(1)])[usize(1)], 240.0f);
    EXPECT_TRUE(override.controlpoint[usize(2)].is_none());
}

TEST(ParticleInstanceModifiers, SharesStateAndFiltersDisabledOverrides) {
    auto state        = rstd::sync::Arc<owe::wpscene::ParticleInstanceoverride>::make();
    state->enabled    = true;
    state->overColorn = true;
    state->colorn     = { 0.25f, 0.5f, 0.75f };
    state->count      = 2.0f;
    state->lifetime   = 3.0f;
    state->rate       = 4.0f;
    state->size       = 5.0f;
    state->speed      = 6.0f;

    owe::ParticleInstanceModifiers filtered(
        state.clone(), owe::wpscene::Particle::EFlags { 248 }, false);
    EXPECT_TRUE(filtered.Enabled());
    EXPECT_FLOAT_EQ(filtered.Count(), 1.0f);
    EXPECT_FLOAT_EQ(filtered.Lifetime(), 1.0f);
    EXPECT_FLOAT_EQ(filtered.Rate(), 4.0f);
    EXPECT_FLOAT_EQ(filtered.Size(), 1.0f);
    EXPECT_FLOAT_EQ(filtered.Speed(), 1.0f);
    EXPECT_FALSE(filtered.HasColorOverride());
    EXPECT_FALSE(filtered.ControlpointsEnabled());

    owe::ParticleInstanceModifiers unfiltered(
        state.clone(), owe::wpscene::Particle::EFlags { 0 }, true);
    state->size = 7.0f;
    EXPECT_FLOAT_EQ(unfiltered.Size(), 7.0f);
    EXPECT_TRUE(unfiltered.HasColorOverride());
    EXPECT_TRUE(unfiltered.ControlpointsEnabled());
}

TEST(ParticleSubSystem, ResolvesWorldControlpointOverridesThroughOwnerTransform) {
    owe::Scene             scene;
    auto                   mesh = Arc<owe::SceneMesh>::make();
    owe::ParticleSubSystem subsystem(scene,
                                     mesh.clone(),
                                     u32(1),
                                     f64(),
                                     u32(1),
                                     f64(1.0),
                                     owe::ParticleSubSystem::SpawnType::STATIC,
                                     owe::ParticleAnimationSpec {});

    auto points                  = subsystem.ControlpointsMut();
    points[usize(1)].base_offset = Eigen::Vector3d { 0.0, -450.0, 0.0 };
    points[usize(1)].worldspace  = true;
    points[usize(2)].base_offset = Eigen::Vector3d { 3.0, 4.0, 0.0 };
    points[usize(2)].worldspace  = true;
    points[usize(3)].base_offset = Eigen::Vector3d { 1.0, 2.0, 0.0 };

    auto owner    = rstd::sync::Arc<owe::SceneNode>::make(Eigen::Vector3f { 100.0f, 200.0f, 0.0f },
                                                          Eigen::Vector3f { 2.0f, 4.0f, 1.0f },
                                                          Eigen::Vector3f::Zero());
    auto override = rstd::sync::Arc<owe::wpscene::ParticleInstanceoverride>::make();
    override->enabled                = true;
    override->controlpoint[usize(1)] = Some(rstd::array<float, 3> { 120.0f, 240.0f, 0.0f });
    override->controlpoint[usize(3)] = Some(rstd::array<float, 3> { 3.0f, 4.0f, 0.0f });
    subsystem.SetOwnerNode(owner.as_ptr());
    subsystem.SetInstanceModifiers(owe::ParticleInstanceModifiers(
        override.clone(), owe::wpscene::Particle::EFlags { 0 }, true));
    subsystem.Finalize();
    subsystem.Tick(f64(), false);

    auto resolved = subsystem.Controlpoints();
    EXPECT_TRUE(resolved[usize(1)].offset.isApprox(Eigen::Vector3d { 10.0, 10.0, 0.0 }));
    // a worldspace control point's own offset is a world position too, override or not
    // (particles-cp-worldspace: layers anywhere, at any scale, with offset 960 540 all reach the
    // middle of the picture; thunderbolt's control point 1 "0 -450 0" points off the bottom left)
    EXPECT_TRUE(resolved[usize(2)].offset.isApprox(Eigen::Vector3d { -48.5, -49.0, 0.0 }));
    EXPECT_TRUE(resolved[usize(3)].offset.isApprox(Eigen::Vector3d { 4.0, 6.0, 0.0 }));
}

TEST(ParticleSubSystem, ResolvesWorldSpaceControlpointsForSimulation) {
    owe::Scene             scene;
    auto                   mesh = Arc<owe::SceneMesh>::make();
    owe::ParticleSubSystem subsystem(scene,
                                     mesh.clone(),
                                     u32(1),
                                     f64(),
                                     u32(1),
                                     f64(1.0),
                                     owe::ParticleSubSystem::SpawnType::STATIC,
                                     owe::ParticleAnimationSpec {},
                                     {},
                                     u32(),
                                     f64(),
                                     f64(),
                                     true);
    auto owner = rstd::sync::Arc<owe::SceneNode>::make(Eigen::Vector3f { 100.0f, 200.0f, 0.0f },
                                                       Eigen::Vector3f { 2.0f, 4.0f, 1.0f },
                                                       Eigen::Vector3f::Zero());
    subsystem.SetOwnerNode(owner.as_ptr());
    subsystem.ControlpointsMut()[usize(1)].base_offset = Eigen::Vector3d { 3.0, 4.0, 0.0 };
    subsystem.Finalize();
    subsystem.Tick(f64(), false);

    auto resolved = subsystem.SimulationControlpoint(usize(1));
    EXPECT_TRUE(resolved.center.isApprox(Eigen::Vector3d { 106.0, 216.0, 0.0 }));
    Eigen::Matrix3d expected_basis = Eigen::Vector3d { 2.0, 4.0, 1.0 }.asDiagonal();
    EXPECT_TRUE(resolved.basis.isApprox(expected_basis));
}

TEST(ParticleSubSystem, AppliesVortexAroundWorldSpaceOwner) {
    owe::Scene             scene;
    auto                   mesh = Arc<owe::SceneMesh>::make();
    owe::ParticleSubSystem subsystem(scene,
                                     mesh.clone(),
                                     u32(1),
                                     f64(),
                                     u32(1),
                                     f64(1.0),
                                     owe::ParticleSubSystem::SpawnType::STATIC,
                                     owe::ParticleAnimationSpec {},
                                     {},
                                     u32(),
                                     f64(),
                                     f64(),
                                     true);
    auto owner    = rstd::sync::Arc<owe::SceneNode>::make(Eigen::Vector3f { 1000.0f, 800.0f, 0.0f },
                                                          Eigen::Vector3f::Ones(),
                                                          Eigen::Vector3f::Zero());
    auto override = rstd::sync::Arc<owe::wpscene::ParticleInstanceoverride>::make();
    override->enabled                     = true;
    override->controlpointangle[usize(1)] = rstd::array<float, 3> { 1.57079632679f, 0.0f, 0.0f };
    subsystem.SetOwnerNode(owner.as_ptr());
    auto modifiers = owe::ParticleInstanceModifiers(
        override.clone(), owe::wpscene::Particle::EFlags { 0 }, true);
    subsystem.SetInstanceModifiers(modifiers.Clone());
    subsystem.AddInitializer(owe::ParticleParser::GenInitializer(
        owe::ParseJson(R"({"name":"lifetimerandom","min":10,"max":10})"_str).unwrap(), u32(1)));
    subsystem.AddEmitter(Box<dyn<particle::ParticleEmitterProgram>>::make(
        owe::BoxEmitterProgram(subsystem.SpawnPipeline(),
                               owe::ParticleBoxEmitterArgs {
                                   .origin        = { 200.0f, 0.0f, 0.0f },
                                   .instantaneous = u32(1),
                               },
                               usize())));
    subsystem.AddOperator(
        owe::ParticleParser::GenOperator(owe::ParseJson(R"({"name":"movement"})"_str).unwrap(),
                                         modifiers.Clone(),
                                         subsystem,
                                         usize()));
    subsystem.AddOperator(owe::ParticleParser::GenOperator(
        owe::ParseJson(
            R"({"name":"vortex_v2","controlpoint":1,"flags":2,"ringpulldistance":250,"ringradius":256,"ringwidth":5,"speedinner":0,"speedouter":2500})"_str)
            .unwrap(),
        modifiers.Clone(),
        subsystem,
        usize(1)));
    subsystem.Finalize();
    subsystem.Tick(f64(1.0 / 60.0), false);
    subsystem.Tick(f64(1.0 / 60.0), false);

    auto positions = subsystem.System().Instance(usize()).Binding().Read().Positions();
    ASSERT_EQ(positions.len(), usize(1));
    EXPECT_GT(std::abs(positions[usize()].z()), 0.01f);
}

// capvelocity keeps a particle at or under maxspeed (particles-capvelocity: a stream thrown at 200
// units/s and capped at 100 is spaced 25 units at 4 a second; owe ignored the operator).
TEST(ParticleSubSystem, CapVelocityLimitsTheSpeed) {
    owe::Scene             scene;
    auto                   mesh = Arc<owe::SceneMesh>::make();
    owe::ParticleSubSystem subsystem(scene,
                                     mesh.clone(),
                                     u32(1),
                                     f64(),
                                     u32(1),
                                     f64(1.0),
                                     owe::ParticleSubSystem::SpawnType::STATIC,
                                     owe::ParticleAnimationSpec {},
                                     {},
                                     u32(),
                                     f64(),
                                     f64(),
                                     true);
    auto                   owner = rstd::sync::Arc<owe::SceneNode>::make(
        Eigen::Vector3f::Zero(), Eigen::Vector3f::Ones(), Eigen::Vector3f::Zero());
    auto override = rstd::sync::Arc<owe::wpscene::ParticleInstanceoverride>::make();
    subsystem.SetOwnerNode(owner.as_ptr());
    auto modifiers = owe::ParticleInstanceModifiers(
        override.clone(), owe::wpscene::Particle::EFlags { 0 }, true);
    subsystem.SetInstanceModifiers(modifiers.Clone());
    subsystem.AddInitializer(owe::ParticleParser::GenInitializer(
        owe::ParseJson(R"({"name":"lifetimerandom","min":1000,"max":1000})"_str).unwrap(), u32(1)));
    subsystem.AddInitializer(owe::ParticleParser::GenInitializer(
        owe::ParseJson(R"({"name":"velocityrandom","min":"200 0 0","max":"200 0 0"})"_str).unwrap(),
        u32(2)));
    subsystem.AddEmitter(Box<dyn<particle::ParticleEmitterProgram>>::make(
        owe::BoxEmitterProgram(subsystem.SpawnPipeline(),
                               owe::ParticleBoxEmitterArgs {
                                   .instantaneous = u32(1),
                               },
                               usize())));
    subsystem.AddOperator(owe::ParticleParser::GenOperator(
        owe::ParseJson(R"({"name":"movement","drag":0,"gravity":"0 0 0"})"_str).unwrap(),
        modifiers.Clone(),
        subsystem,
        usize()));
    subsystem.AddOperator(owe::ParticleParser::GenOperator(
        owe::ParseJson(R"({"name":"capvelocity","maxspeed":100})"_str).unwrap(),
        modifiers.Clone(),
        subsystem,
        usize(1)));
    subsystem.Finalize();
    for (int frame = 0; frame < 120; ++frame) subsystem.Tick(f64(1.0 / 60.0), false);

    // the first frame moves at 200, the rest at 100
    auto positions = subsystem.System().Instance(usize()).Binding().Read().Positions();
    ASSERT_EQ(positions.len(), usize(1));
    EXPECT_NEAR(positions[usize()].x(), 200.0f / 60.0f + 100.0f * 119.0f / 60.0f, 0.5f);
}

// reducemovementnearcontrolpoint slows particles near a control point like a drag, as Wallpaper Engine runs it
// (FUN_14023fbc0 case 13): the reduction goes linearly with the distance from reductioninner at distanceinner to
// reductionouter at distanceouter (and stays at the end values beyond them), and a frame takes
// min(reduction dt, 1) of the speed. A particle that stays within distanceinner at reduction 10 goes
// 200/60 (1 - r^60) / (1 - r) = 20.0 units in a second of 60 frames, r = 1 - 10/60.
TEST(ParticleSubSystem, ReduceMovementNearControlPointSlowsLikeADrag) {
    owe::Scene             scene;
    auto                   mesh = Arc<owe::SceneMesh>::make();
    owe::ParticleSubSystem subsystem(scene,
                                     mesh.clone(),
                                     u32(1),
                                     f64(),
                                     u32(1),
                                     f64(1.0),
                                     owe::ParticleSubSystem::SpawnType::STATIC,
                                     owe::ParticleAnimationSpec {},
                                     {},
                                     u32(),
                                     f64(),
                                     f64(),
                                     true);
    auto                   owner = rstd::sync::Arc<owe::SceneNode>::make(
        Eigen::Vector3f::Zero(), Eigen::Vector3f::Ones(), Eigen::Vector3f::Zero());
    auto override = rstd::sync::Arc<owe::wpscene::ParticleInstanceoverride>::make();
    subsystem.SetOwnerNode(owner.as_ptr());
    auto modifiers = owe::ParticleInstanceModifiers(
        override.clone(), owe::wpscene::Particle::EFlags { 0 }, true);
    subsystem.SetInstanceModifiers(modifiers.Clone());
    subsystem.AddInitializer(owe::ParticleParser::GenInitializer(
        owe::ParseJson(R"({"name":"lifetimerandom","min":1000,"max":1000})"_str).unwrap(), u32(1)));
    subsystem.AddInitializer(owe::ParticleParser::GenInitializer(
        owe::ParseJson(R"({"name":"velocityrandom","min":"200 0 0","max":"200 0 0"})"_str).unwrap(),
        u32(2)));
    subsystem.AddEmitter(Box<dyn<particle::ParticleEmitterProgram>>::make(
        owe::BoxEmitterProgram(subsystem.SpawnPipeline(),
                               owe::ParticleBoxEmitterArgs {
                                   .instantaneous = u32(1),
                               },
                               usize())));
    subsystem.AddOperator(owe::ParticleParser::GenOperator(
        owe::ParseJson(R"({"name":"movement","drag":0,"gravity":"0 0 0"})"_str).unwrap(),
        modifiers.Clone(),
        subsystem,
        usize()));
    subsystem.AddOperator(owe::ParticleParser::GenOperator(
        owe::ParseJson(
            R"({"name":"reducemovementnearcontrolpoint","controlpoint":0,"distanceinner":50,"distanceouter":150,"reductioninner":10,"reductionouter":0})"_str)
            .unwrap(),
        modifiers.Clone(),
        subsystem,
        usize(1)));
    subsystem.Finalize();
    for (int frame = 0; frame < 60; ++frame) subsystem.Tick(f64(1.0 / 60.0), false);

    auto positions = subsystem.System().Instance(usize()).Binding().Read().Positions();
    ASSERT_EQ(positions.len(), usize(1));
    EXPECT_NEAR(positions[usize()].x(), 20.0f, 0.3f);
}

// one frame of reducemovementnearcontrolpoint on a particle crossing at 300 units/s, `distance` from the point
static float ReducedSpeed(float distance, ref<str> reduce) {
    owe::Scene             scene;
    auto                   mesh = Arc<owe::SceneMesh>::make();
    owe::ParticleSubSystem subsystem(scene,
                                     mesh.clone(),
                                     u32(1),
                                     f64(1.0),
                                     u32(1),
                                     f64(1.0),
                                     owe::ParticleSubSystem::SpawnType::STATIC,
                                     owe::ParticleAnimationSpec {});
    auto modifiers = owe::ParticleInstanceModifiers(Arc<owe::wpscene::ParticleInstanceoverride>::make(),
                                                    owe::wpscene::Particle::EFlags { 0 },
                                                    false);
    subsystem.AddInitializer(owe::ParticleParser::GenInitializer(
        owe::ParseJson(R"({"name":"lifetimerandom","min":100,"max":100})"_str).unwrap(), u32(1)));
    subsystem.AddInitializer(owe::ParticleParser::GenInitializer(
        owe::ParseJson(R"({"name":"velocityrandom","min":"0 300 0","max":"0 300 0"})"_str).unwrap(), u32(2)));
    subsystem.AddEmitter(Box<dyn<particle::ParticleEmitterProgram>>::make(owe::BoxEmitterProgram(
        subsystem.SpawnPipeline(),
        owe::ParticleBoxEmitterArgs { .origin = { distance, 0.0f, 0.0f }, .instantaneous = u32(1) },
        usize())));
    subsystem.AddOperator(owe::ParticleParser::GenOperator(owe::ParseJson(reduce).unwrap(), modifiers.Clone(),
                                                           subsystem, usize()));
    subsystem.Finalize();
    subsystem.Tick(f64(1.0 / 30.0), false);
    auto& storage = subsystem.System().Instance(usize()).Storage();
    particle::ParticleSlotReader value(storage, particle::ParticleSlot {});
    return value.Read(subsystem.Attributes().velocity).y();
}

TEST(ParticleSubSystem, ReduceMovementNearControlPointGoesLinearlyWithTheDistance) {
    auto reduce =
        R"({"name":"reducemovementnearcontrolpoint","controlpoint":0,"distanceinner":50,"distanceouter":150,"reductioninner":10,"reductionouter":2})"_str;
    EXPECT_NEAR(ReducedSpeed(40.0f, reduce), 300.0f * (1.0f - 10.0f / 30.0f), 0.05f);  // within: the inner value
    EXPECT_NEAR(ReducedSpeed(100.0f, reduce), 300.0f * (1.0f - 6.0f / 30.0f), 0.05f);  // half way: half way
    EXPECT_NEAR(ReducedSpeed(200.0f, reduce), 300.0f * (1.0f - 2.0f / 30.0f), 0.05f);  // beyond: the outer value
}

// remapvalue maps the distance to a control point, clamped to its input range, linearly onto its
// output range (particles-remapvalue: rows 0 to 400 units out remap colour red to blue, size 10 to
// 80 - drawn 20 to 160, a radius - and opacity 1 to 0.1, and multiply size 40 by 0.2 to 2).
TEST(ParticleRemap, MapsTheDistanceOntoTheOutputRange) {
    auto color =
        owe::ParticleRemap::FromJson(
            owe::ParseJson(
                R"({"name":"remapvalue","input":"distancetocontrolpoint","inputcontrolpoint0":1,"inputrangemin":0,"inputrangemax":400,"operation":"remap","output":"color","outputrangemin":"1 0 0","outputrangemax":"0 0 1"})"_str)
                .unwrap())
            .unwrap();
    EXPECT_EQ(color.controlpoint, i32(1));
    EXPECT_TRUE(color.Apply(Eigen::Vector3d::Ones(), 100).isApprox(Eigen::Vector3d(0.75, 0, 0.25)));
    EXPECT_TRUE(color.Apply(Eigen::Vector3d::Ones(), 900).isApprox(Eigen::Vector3d(0, 0, 1)));
    EXPECT_TRUE(color.Apply(Eigen::Vector3d::Ones(), -5).isApprox(Eigen::Vector3d(1, 0, 0)));

    auto size =
        owe::ParticleRemap::FromJson(
            owe::ParseJson(
                R"({"name":"remapvalue","input":"distancetocontrolpoint","inputrangemax":400,"output":"size","outputrangemin":10,"outputrangemax":80})"_str)
                .unwrap())
            .unwrap();
    EXPECT_NEAR(size.Apply(Eigen::Vector3d::Constant(40), 200).x(), 90.0, 1e-4);

    auto multiply =
        owe::ParticleRemap::FromJson(
            owe::ParseJson(
                R"({"name":"remapvalue","input":"distancetocontrolpoint","inputrangemax":400,"operation":"multiply","output":"size","outputrangemin":0.2,"outputrangemax":2})"_str)
                .unwrap())
            .unwrap();
    EXPECT_NEAR(multiply.Apply(Eigen::Vector3d::Constant(40), 400).x(), 80.0, 1e-4);
    EXPECT_NEAR(multiply.Apply(Eigen::Vector3d::Constant(40), 0).x(), 8.0, 1e-4);

    // noise and sine transforms aren't read
    EXPECT_TRUE(
        owe::ParticleRemap::FromJson(
            owe::ParseJson(
                R"({"name":"remapvalue","output":"velocity","transformfunction":"simplexnoise"})"_str)
                .unwrap())
            .is_none());
}

// remapinitialvalue reads the same settings as remapvalue (particles-remapinitialvalue); one
// without an output it can set (thunderbolt_beam_child's, distance to control point 1 with no
// output) does nothing.
TEST(ParticleRemap, TheInitialValueFormReadsTheSameSettings) {
    auto size =
        owe::ParticleRemap::FromJson(
            owe::ParseJson(
                R"({"name":"remapinitialvalue","input":"distancetocontrolpoint","inputcontrolpoint0":0,"inputrangemin":0,"inputrangemax":400,"operation":"remap","output":"size","outputrangemin":10,"outputrangemax":80})"_str)
                .unwrap())
            .unwrap();
    EXPECT_NEAR(size.Apply(Eigen::Vector3d::Constant(40), 400).x(), 160.0, 1e-4);
    EXPECT_TRUE(
        owe::ParticleRemap::FromJson(
            owe::ParseJson(
                R"({"name":"remapinitialvalue","input":"distancetocontrolpoint","inputcontrolpoint0":1,"inputrangemax":50,"operation":"multiply"})"_str)
                .unwrap())
            .is_none());
}

// hsvcolorrandom's colours: hue around the circle, saturation and value (particles-hsvcolor:
// Wallpaper Engine draws hue 0.6 at saturation 0.5, value 0.5 as 0.25 0.35 0.5).
TEST(ParticleColor, HsvToRgbGoesAroundTheHueCircle) {
    EXPECT_TRUE(owe::HsvToRgb(0, 1, 1).isApprox(Eigen::Vector3d(1, 0, 0)));
    EXPECT_TRUE(owe::HsvToRgb(1.0 / 3.0, 1, 1).isApprox(Eigen::Vector3d(0, 1, 0)));
    EXPECT_TRUE(owe::HsvToRgb(2.0 / 3.0, 1, 1).isApprox(Eigen::Vector3d(0, 0, 1)));
    EXPECT_TRUE(owe::HsvToRgb(1.0, 1, 1).isApprox(Eigen::Vector3d(1, 0, 0)));
    EXPECT_TRUE(owe::HsvToRgb(0.6, 0.5, 0.5).isApprox(Eigen::Vector3d(0.25, 0.35, 0.5)));
}

// A plain rope's vertex shader takes the length of the rope, in points, from the fourth component
// of each segment's end point, and spreads its texture over length - 1 segments: a rope of n
// particles has n - 1 segments and says n (particles-rope-count: owe said n - 1, so the last
// segment's texture ran off the end and a rope of two particles, whose length - 1 came to 0, drew
// nothing).
TEST(ParticleSubSystem, ARopeSaysItsLengthInPoints) {
    owe::Scene scene;
    auto       mesh = Arc<owe::SceneMesh>::make();
    // the plain rope's geometry-shader layout (SetRopeParticleMesh's)
    const rstd::initializer_list<owe::SceneVertexArray::SceneVertexAttribute> attributes {
        { .name = "a_PositionVec4"_Str, .type = owe::VertexType::FLOAT4 },
        { .name = "a_TexCoordVec4"_Str, .type = owe::VertexType::FLOAT4 },
        { .name = "a_TexCoordVec4C1"_Str, .type = owe::VertexType::FLOAT4 },
        { .name = "a_TexCoordVec3C2"_Str, .type = owe::VertexType::FLOAT3, .padding = false },
        { .name = "a_Color"_Str, .type = owe::VertexType::FLOAT4 },
    };
    mesh->AddVertexArray(owe::SceneVertexArray(attributes, rstd::usize(10)));
    mesh->GetVertexArray(usize(0)).SetOption("PRENDER_ROPE"_str, true);
    mesh->GetVertexArray(usize(0)).SetOption("THICKFORMAT"_str, false);
    mesh->GetVertexArray(usize(0)).SetOption("GS_ENABLED"_str, true);
    owe::ParticleSubSystem subsystem(scene,
                                     mesh.clone(),
                                     u32(10),
                                     f64(),
                                     u32(1),
                                     f64(1.0),
                                     owe::ParticleSubSystem::SpawnType::STATIC,
                                     owe::ParticleAnimationSpec {},
                                     {},
                                     u32(),
                                     f64(),
                                     f64(),
                                     true);
    auto                   owner = rstd::sync::Arc<owe::SceneNode>::make(
        Eigen::Vector3f::Zero(), Eigen::Vector3f::Ones(), Eigen::Vector3f::Zero());
    auto override = rstd::sync::Arc<owe::wpscene::ParticleInstanceoverride>::make();
    subsystem.SetOwnerNode(owner.as_ptr());
    auto modifiers = owe::ParticleInstanceModifiers(
        override.clone(), owe::wpscene::Particle::EFlags { 0 }, true);
    subsystem.SetInstanceModifiers(modifiers.Clone());
    subsystem.AddInitializer(owe::ParticleParser::GenInitializer(
        owe::ParseJson(R"({"name":"lifetimerandom","min":1000,"max":1000})"_str).unwrap(), u32(1)));
    subsystem.AddEmitter(Box<dyn<particle::ParticleEmitterProgram>>::make(
        owe::BoxEmitterProgram(subsystem.SpawnPipeline(),
                               owe::ParticleBoxEmitterArgs {
                                   .max_distance  = { 100.0f, 0.0f, 0.0f },
                                   .instantaneous = u32(4),
                               },
                               usize())));
    subsystem.Finalize();
    subsystem.Tick(f64(1.0 / 60.0), true);
    subsystem.Tick(f64(1.0 / 60.0), true);

    // position (4 floats), end point (4), previous point (4), next point (3), colour (4)
    const auto& vertices = mesh->GetVertexArray(usize());
    ASSERT_EQ(vertices.OneSize(), usize(19));
    ASSERT_EQ(vertices.VertexCount(), usize(3));
    for (std::size_t v = 0; v < 3; ++v) EXPECT_FLOAT_EQ(vertices.Data()[v * 19 + 7], 4.0f) << v;
}

// remapvalue's other inputs (particles-remap-inputs, grey = the input over its range): the
// particle's speed per axis (a vector, so a colour of 100 0 0 over 0..200 is 0.5 0 0), its opacity,
// rotation and angular speed (one number), the time, and "lifetimefraction", the name Wallpaper
// Engine uses for how far through its life it is.
TEST(ParticleRemap, ReadsTheInputsWallpaperEngineOffers) {
    auto read = [](ref<str> input) {
        auto json = rstd::format(
            R"({{"name":"remapvalue","input":"{}","inputrangemax":4,"output":"color"}})", input);
        return owe::ParticleRemap::FromJson(owe::ParseJson(json.as_str()).unwrap());
    };
    EXPECT_EQ(read("lifetimefraction"_str).unwrap().input, owe::ParticleRemap::Input::Lifetime);
    EXPECT_EQ(read("particlelifetime"_str).unwrap().input, owe::ParticleRemap::Input::Lifetime);
    EXPECT_EQ(read("velocity"_str).unwrap().input, owe::ParticleRemap::Input::Velocity);
    EXPECT_EQ(read("opacity"_str).unwrap().input, owe::ParticleRemap::Input::Opacity);
    EXPECT_EQ(read("rotation"_str).unwrap().input, owe::ParticleRemap::Input::Rotation);
    EXPECT_EQ(read("angularspeed"_str).unwrap().input, owe::ParticleRemap::Input::AngularSpeed);
    EXPECT_EQ(read("layerorigin"_str).unwrap().input, owe::ParticleRemap::Input::LayerOrigin);
    EXPECT_EQ(read("particlesystemtime"_str).unwrap().input, owe::ParticleRemap::Input::SystemTime);
    EXPECT_EQ(read("layertime"_str).unwrap().input, owe::ParticleRemap::Input::LayerTime);
    EXPECT_EQ(read("runtime"_str).unwrap().input, owe::ParticleRemap::Input::Runtime);
    EXPECT_EQ(read("timeofday"_str).unwrap().input, owe::ParticleRemap::Input::TimeOfDay);
    EXPECT_EQ(read("deltatocontrolpoint"_str).unwrap().input,
              owe::ParticleRemap::Input::DeltaToControlPoint);
    EXPECT_EQ(read("directiontocontrolpoint"_str).unwrap().input,
              owe::ParticleRemap::Input::DirectionToControlPoint);
    EXPECT_EQ(read("positionbetweentwocontrolpoints"_str).unwrap().input,
              owe::ParticleRemap::Input::PositionBetweenControlPoints);
    EXPECT_TRUE(read("nothing"_str).is_none());
    // a vector input maps each axis over the range
    auto velocity      = read("velocity"_str).unwrap();
    velocity.input_max = 200;
    EXPECT_TRUE(velocity.Apply(Eigen::Vector3d::Ones(), Eigen::Vector3d(100, 0, 400))
                    .isApprox(Eigen::Vector3d(0.5, 0, 1)));
}

// remapvalue's operations and transform functions (particles-remap-outputs, -remap-transform):
// "subtract" takes the mapped value from the particle's; triangle and square repeat twice over the
// input range (square low first), sine once (0 at the ends, 1 in the middle); a noise function
// isn't read.
TEST(ParticleRemap, SubtractsAndShapesTheInput) {
    auto read = [](ref<str> extra) {
        auto json = rstd::format(
            R"({{"name":"remapvalue","input":"lifetimefraction","inputrangemax":1,"output":"color","outputrangemax":"1 1 1"{}}})",
            extra);
        return owe::ParticleRemap::FromJson(owe::ParseJson(json.as_str()).unwrap());
    };
    auto subtract = read(R"(,"operation":"subtract")"_str).unwrap();
    EXPECT_TRUE(subtract.Apply(Eigen::Vector3d(1, 0.5, 0.25), 0.56)
                    .isApprox(Eigen::Vector3d(0.44, -0.06, -0.31)));
    auto triangle = read(R"(,"transformfunction":"triangle")"_str).unwrap();
    EXPECT_NEAR(triangle.Map(0.12).x(), 0.48, 1e-9);
    EXPECT_NEAR(triangle.Map(0.36).x(), 0.56, 1e-9);
    EXPECT_NEAR(triangle.Map(0.72).x(), 0.88, 1e-9);
    auto sine = read(R"(,"transformfunction":"sine")"_str).unwrap();
    EXPECT_NEAR(sine.Map(0.0).x(), 0.0, 1e-9);
    EXPECT_NEAR(sine.Map(0.25).x(), 0.5, 1e-9);
    EXPECT_NEAR(sine.Map(0.5).x(), 1.0, 1e-9);
    auto square = read(R"(,"transformfunction":"square")"_str).unwrap();
    EXPECT_EQ(square.Map(0.12).x(), 0.0);
    EXPECT_EQ(square.Map(0.36).x(), 1.0);
    EXPECT_EQ(square.Map(0.72).x(), 0.0);
    EXPECT_EQ(square.Map(0.9).x(), 1.0);
    EXPECT_TRUE(read(R"(,"transformfunction":"simplexnoise")"_str).is_none());
}

// transforminputscale (default 2) sets how far round the sine goes: 0.5 - 0.5 cos(pi * scale *
// fraction) (particles-remap-timescale-range: scale 6 is 0.009 at a fraction of 0.344 and 0.99 at
// 0.5075, scale 0.5 is 0.071 at 0.344; -timescale: past the range, scale 0.5 holds 0.5 and scale 6
// holds 0). Linear ignores it.
TEST(ParticleRemap, TheInputScaleSetsTheSinesTurns) {
    auto read = [](ref<str> extra) {
        auto json = rstd::format(
            R"({{"name":"remapvalue","input":"particlesystemtime","output":"opacity","transformfunction":"sine"{}}})",
            extra);
        return owe::ParticleRemap::FromJson(owe::ParseJson(json.as_str()).unwrap()).unwrap();
    };
    auto six = read(R"(,"transforminputscale":6,"inputrangemax":40)"_str);
    EXPECT_NEAR(six.Map(0.3442 * 40).x(), 0.0090, 0.01);
    EXPECT_NEAR(six.Map(0.4358 * 40).x(), 0.6746, 0.01);
    EXPECT_NEAR(six.Map(0.5075 * 40).x(), 0.9924, 0.01);
    auto half = read(R"(,"transforminputscale":0.5,"inputrangemax":40)"_str);
    EXPECT_NEAR(half.Map(0.3442 * 40).x(), 0.0708, 0.01);
    EXPECT_NEAR(half.Map(0.5567 * 40).x(), 0.1793, 0.01);
    EXPECT_NEAR(read(R"(,"transforminputscale":0.5)"_str).Map(14.0).x(), 0.5, 1e-9);
    EXPECT_NEAR(read(R"(,"transforminputscale":6)"_str).Map(14.0).x(), 0.0, 1e-9);
    EXPECT_NEAR(read(R"(,"transforminputscale":2,"inputrangemax":40)"_str).Map(0.3442 * 40).x(),
                read(R"(,"inputrangemax":40)"_str).Map(0.3442 * 40).x(),
                1e-9);
    auto linear =
        owe::ParticleRemap::FromJson(
            owe::ParseJson(
                R"({"name":"remapvalue","input":"particlesystemtime","output":"opacity","inputrangemax":40,"transforminputscale":0.5})"_str)
                .unwrap())
            .unwrap();
    EXPECT_NEAR(linear.Map(0.3442 * 40).x(), 0.3442, 1e-9);
}

// blendinstart..blendinend (fractions of the particle's life) blend the remapped value in from the
// particle's own, linearly; before the window the operator does nothing
// (particles-remap-timescale: a 30 s particle remapped to 0 over 0.3..0.5 had 0.207 of its opacity
// left at 13.77 s and 0.0845 at 14.5 s; one 0.5..0.6 into a 100000 s life kept all of it).
// blendoutstart..blendoutend blend it back out.
TEST(ParticleRemap, TheBlendWindowFollowsTheParticlesLife) {
    auto read = [](ref<str> extra) {
        auto json = rstd::format(
            R"({{"name":"remapvalue","input":"particlesystemtime","output":"opacity","outputrangemin":0,"outputrangemax":0{}}})",
            extra);
        return owe::ParticleRemap::FromJson(owe::ParseJson(json.as_str()).unwrap()).unwrap();
    };
    auto plain = read(""_str);
    EXPECT_EQ(plain.BlendWeight(0.0), 1.0);
    EXPECT_EQ(plain.BlendWeight(0.99), 1.0);
    auto in = read(R"(,"blendinstart":0.3,"blendinend":0.5)"_str);
    EXPECT_EQ(in.BlendWeight(0.1), 0.0);
    EXPECT_NEAR(in.BlendWeight(13.77 / 30.0), 0.795, 1e-3);
    EXPECT_EQ(in.BlendWeight(0.6), 1.0);
    EXPECT_NEAR(in.Blend(Eigen::Vector3d::Ones(), 13.77, 13.77 / 30.0).x(), 0.205, 1e-3);
    EXPECT_NEAR(in.Blend(Eigen::Vector3d::Ones(), 14.5, 14.5 / 30.0).x(), 0.0833, 2e-3);
    auto out = read(R"(,"blendoutstart":0.6,"blendoutend":0.8)"_str);
    EXPECT_EQ(out.BlendWeight(0.5), 1.0);
    EXPECT_NEAR(out.BlendWeight(0.7), 0.5, 1e-9);
    EXPECT_EQ(out.BlendWeight(0.9), 0.0);
    // thunderbolt: in over 0.5..0.6 of the life only
    auto bolt = read(R"(,"blendinstart":0.5,"blendinend":0.6)"_str);
    EXPECT_EQ(bolt.Blend(Eigen::Vector3d::Ones(), 20.0, 0.4).x(), 1.0);
    EXPECT_NEAR(bolt.Blend(Eigen::Vector3d::Ones(), 20.0, 0.55).x(), 0.5, 1e-9);
    EXPECT_EQ(bolt.Blend(Eigen::Vector3d::Ones(), 20.0, 0.7).x(), 0.0);
}

// A size a remapvalue sets, adds or takes is a radius, drawn twice as large
// (particles-remap-outputs: adding up to 40 to a size of 60 draws it 105, taking 22.5 draws 15);
// one it multiplies is a factor.
TEST(ParticleRemap, SizeAddsAndSubtractsRadii) {
    auto read = [](ref<str> operation) {
        auto json = rstd::format(
            R"({{"name":"remapvalue","input":"lifetimefraction","inputrangemax":1,"output":"size","outputrangemax":40,"operation":"{}"}})",
            operation);
        return owe::ParticleRemap::FromJson(owe::ParseJson(json.as_str()).unwrap()).unwrap();
    };
    EXPECT_NEAR(read("add"_str).Apply(Eigen::Vector3d::Constant(60), 0.5625).x(), 105.0, 1e-9);
    EXPECT_NEAR(read("subtract"_str).Apply(Eigen::Vector3d::Constant(60), 0.5625).x(), 15.0, 1e-9);
    EXPECT_NEAR(read("multiply"_str).Apply(Eigen::Vector3d::Constant(60), 0.5).x(), 1200.0, 1e-9);
}

TEST(ParticleSubSystem, RemapValueMapsTheParticlesVelocityOpacityAndRotation) {
    auto remap =
        [](ref<str> initializer, ref<str> operator_json, bool read_velocity = false, int ticks = 1)
        -> Eigen::Vector3f {
        owe::Scene             scene;
        auto                   mesh = Arc<owe::SceneMesh>::make();
        owe::ParticleSubSystem subsystem(scene,
                                         mesh.clone(),
                                         u32(1),
                                         f64(1.0),
                                         u32(1),
                                         f64(1.0),
                                         owe::ParticleSubSystem::SpawnType::STATIC,
                                         owe::ParticleAnimationSpec {});
        subsystem.AddInitializer(owe::ParticleParser::GenInitializer(
            owe::ParseJson(R"({"name":"lifetimerandom","min":10,"max":10})"_str).unwrap(), u32(1)));
        subsystem.AddInitializer(
            owe::ParticleParser::GenInitializer(owe::ParseJson(initializer).unwrap(), u32(1)));
        subsystem.AddEmitter(Box<dyn<particle::ParticleEmitterProgram>>::make(
            owe::BoxEmitterProgram(subsystem.SpawnPipeline(),
                                   owe::ParticleBoxEmitterArgs { .instantaneous = u32(1) },
                                   usize())));
        owe::ParticleInstanceModifiers modifiers(
            Arc<owe::wpscene::ParticleInstanceoverride>::make(),
            owe::wpscene::Particle::EFlags { 0 },
            false);
        subsystem.AddOperator(owe::ParticleParser::GenOperator(
            owe::ParseJson(operator_json).unwrap(), modifiers.Clone(), subsystem, usize()));
        subsystem.Finalize();
        for (int tick = 0; tick < ticks; ++tick) subsystem.Tick(f64(1.0 / 60.0), false);
        auto&                        storage = subsystem.System().Instance(usize()).Storage();
        particle::ParticleSlotReader value(storage, particle::ParticleSlot {});
        return read_velocity ? value.Read(subsystem.Attributes().velocity)
                             : value.Read(subsystem.Attributes().color);
    };
    auto velocity = remap(
        R"({"name":"velocityrandom","min":"100 0 0","max":"100 0 0"})"_str,
        R"({"name":"remapvalue","input":"velocity","inputrangemax":200,"output":"color"})"_str);
    EXPECT_NEAR(velocity.x(), 0.5f, 1e-4f);
    EXPECT_NEAR(velocity.y(), 0.0f, 1e-4f);
    auto opacity =
        remap(R"({"name":"alpharandom","min":0.3,"max":0.3})"_str,
              R"({"name":"remapvalue","input":"opacity","inputrangemax":1,"output":"color"})"_str);
    EXPECT_NEAR(opacity.x(), 0.3f, 1e-4f);
    EXPECT_NEAR(opacity.z(), 0.3f, 1e-4f);
    auto rotation =
        remap(R"({"name":"rotationrandom","min":"0 0 1","max":"0 0 1"})"_str,
              R"({"name":"remapvalue","input":"rotation","inputrangemax":4,"output":"color"})"_str);
    EXPECT_NEAR(rotation.x(), 0.25f, 1e-4f);
    EXPECT_NEAR(rotation.y(), 0.25f, 1e-4f);
    // the velocity output sets, adds and subtracts on each frame (particles-remap-velocity); its
    // input is always 1 here
    auto velocity_after = [&](ref<str> operation, ref<str> start) {
        auto json = rstd::format(
            R"({{"name":"remapvalue","input":"lifetimefraction","inputrangemax":0.0001,"output":"velocity","operation":"{}","outputrangemax":"0.5 0 0"}})",
            operation);
        auto init =
            rstd::format(R"({{"name":"velocityrandom","min":"{}","max":"{}"}})", start, start);
        // the first step finds the particle at 0 of its life: the operator's second is the one that
        // counts
        return remap(init.as_str(), json.as_str(), true, 2);
    };
    EXPECT_NEAR(velocity_after("remap"_str, "10 0 0"_str).x(), 0.5f, 1e-4f);
    EXPECT_NEAR(velocity_after("add"_str, "10 0 0"_str).x(), 10.5f, 1e-4f);
    EXPECT_NEAR(velocity_after("subtract"_str, "10 0 0"_str).x(), 9.5f, 1e-4f);
    auto spin = remap(
        R"({"name":"angularvelocityrandom","min":"0 0 1","max":"0 0 1"})"_str,
        R"({"name":"remapvalue","input":"angularspeed","inputrangemax":4,"output":"color"})"_str);
    EXPECT_NEAR(spin.x(), 0.25f, 1e-4f);
}

TEST(ParticleSubSystem, AppliesTurbulenceMaskAsAxisWeights) {
    auto simulate = [](ref<str> config) -> Eigen::Vector3f {
        owe::Scene             scene;
        auto                   mesh = Arc<owe::SceneMesh>::make();
        owe::ParticleSubSystem subsystem(scene,
                                         mesh.clone(),
                                         u32(1),
                                         f64(1.0),
                                         u32(1),
                                         f64(1.0),
                                         owe::ParticleSubSystem::SpawnType::STATIC,
                                         owe::ParticleAnimationSpec {});
        subsystem.AddInitializer(owe::ParticleParser::GenInitializer(
            owe::ParseJson(R"({"name":"lifetimerandom","min":10,"max":10})"_str).unwrap(), u32(1)));
        subsystem.AddEmitter(Box<dyn<particle::ParticleEmitterProgram>>::make(
            owe::BoxEmitterProgram(subsystem.SpawnPipeline(),
                                   owe::ParticleBoxEmitterArgs {
                                       .origin        = { 31.0f, 73.0f, 17.0f },
                                       .instantaneous = u32(1),
                                   },
                                   usize())));
        subsystem.AddOperator(owe::ParticleParser::GenOperator(
            owe::ParseJson(config).unwrap(),
            owe::ParticleInstanceModifiers(Arc<owe::wpscene::ParticleInstanceoverride>::make(),
                                           owe::wpscene::Particle::EFlags { 0 },
                                           false),
            subsystem,
            usize()));
        subsystem.Finalize();
        subsystem.Tick(f64(1.0 / 60.0), false);
        subsystem.Tick(f64(1.0 / 60.0), false);
        auto&                        storage = subsystem.System().Instance(usize()).Storage();
        particle::ParticleSlotReader value(storage, particle::ParticleSlot {});
        return value.Read(subsystem.Attributes().velocity);
    };
    auto full =
        simulate(R"({"name":"turbulence","mask":"1 1 1","speedmin":100,"speedmax":100})"_str);
    ASSERT_GT(full.norm(), 0.01f);
    ASSERT_GT(f32(full.y()).abs().to_primitive(), 0.001f);
    auto fractional =
        simulate(R"({"name":"turbulence","mask":"0 0.01 0","speedmin":100,"speedmax":100})"_str);
    EXPECT_FLOAT_EQ(fractional.x(), 0.0f);
    EXPECT_NEAR(fractional.y(), full.y() * 0.01f, 1e-6f);
    EXPECT_FLOAT_EQ(fractional.z(), 0.0f);
    auto scaled =
        simulate(R"({"name":"turbulence","mask":"0.5 2 -1","speedmin":100,"speedmax":100})"_str);
    EXPECT_NEAR(scaled.x(), full.x() * 0.5f, 1e-6f);
    EXPECT_NEAR(scaled.y(), full.y() * 2.0f, 1e-6f);
    EXPECT_NEAR(scaled.z(), -full.z(), 1e-6f);
    auto defaults = simulate(R"({"name":"turbulence","speedmin":100,"speedmax":100})"_str);
    EXPECT_FLOAT_EQ(defaults.x(), full.x());
    EXPECT_FLOAT_EQ(defaults.y(), full.y());
    EXPECT_FLOAT_EQ(defaults.z(), 0.0f);
}

// colorlist (particles-colorlist): the particle's colour is multiplied by a colour picked from the
// list, whatever order the initializers come in (Wallpaper Engine paints a yellow colorrandom +
// list as yellow, red or dark green).
TEST(ParticleSubSystem, ColorlistMultipliesTheColourWhateverTheOrder) {
    auto spawn = [](ref<str> first, ref<str> second) -> Eigen::Vector3f {
        owe::Scene             scene;
        auto                   mesh = Arc<owe::SceneMesh>::make();
        owe::ParticleSubSystem subsystem(scene,
                                         mesh.clone(),
                                         u32(1),
                                         f64(1.0),
                                         u32(1),
                                         f64(1.0),
                                         owe::ParticleSubSystem::SpawnType::STATIC,
                                         owe::ParticleAnimationSpec {});
        subsystem.AddInitializer(owe::ParticleParser::GenInitializer(
            owe::ParseJson(R"({"name":"lifetimerandom","min":10,"max":10})"_str).unwrap(), u32(1)));
        subsystem.AddInitializer(
            owe::ParticleParser::GenInitializer(owe::ParseJson(first).unwrap(), u32(1)));
        subsystem.AddInitializer(
            owe::ParticleParser::GenInitializer(owe::ParseJson(second).unwrap(), u32(1)));
        subsystem.AddEmitter(Box<dyn<particle::ParticleEmitterProgram>>::make(
            owe::BoxEmitterProgram(subsystem.SpawnPipeline(),
                                   owe::ParticleBoxEmitterArgs { .instantaneous = u32(1) },
                                   usize())));
        subsystem.Finalize();
        subsystem.Tick(f64(1.0 / 60.0), false);
        auto&                        storage = subsystem.System().Instance(usize()).Storage();
        particle::ParticleSlotReader value(storage, particle::ParticleSlot {});
        return value.Read(subsystem.Attributes().color);
    };
    const auto list   = R"({"name":"colorlist","colors":["0.5 1 0.25"]})"_str;
    const auto yellow = R"({"name":"colorrandom","min":"255 255 0","max":"255 255 0"})"_str;
    auto       alone  = spawn(list, R"({"name":"lifetimerandom","min":10,"max":10})"_str);
    EXPECT_NEAR(alone.x(), 0.5f, 1e-5f);
    EXPECT_NEAR(alone.y(), 1.0f, 1e-5f);
    EXPECT_NEAR(alone.z(), 0.25f, 1e-5f);
    for (auto colour : { spawn(yellow, list), spawn(list, yellow) }) {
        EXPECT_NEAR(colour.x(), 0.5f, 1e-5f);
        EXPECT_NEAR(colour.y(), 1.0f, 1e-5f);
        EXPECT_NEAR(colour.z(), 0.0f, 1e-5f);
    }
}

// collisionplane (particles-collisionplane): a particle may not go below the plane n . x = distance
// (n is the "plane" key, up by default). One below it is put on the plane and its speed along n
// becomes -bouncefactor * v_n, whichever way it moves: Wallpaper Engine's kernel (0x140251f30) adds
// n (v . n) x factor under the plane without looking at the sign, so one already moving away is
// turned back into the plane (ft-collisionplane, plane 5: born under it). "slide" zeroes v_n,
// "delete" removes the particle. The sideways speed is kept.
struct CollidedParticle {
    bool            alive { false };
    Eigen::Vector3f position;
    Eigen::Vector3f velocity;
    float           alpha { 1.0f };
};

static CollidedParticle CollideWithPlane(ref<str> collision, Eigen::Vector3f origin,
                                         ref<str> velocity, int ticks = 1,
                                         bool scene_bounds = false) {
    owe::Scene scene;
    if (scene_bounds) scene.SetOrtho({ i32(1920), i32(1080) });
    auto                   mesh = Arc<owe::SceneMesh>::make();
    owe::ParticleSubSystem subsystem(scene,
                                     mesh.clone(),
                                     u32(1),
                                     f64(1.0),
                                     u32(1),
                                     f64(1.0),
                                     owe::ParticleSubSystem::SpawnType::STATIC,
                                     owe::ParticleAnimationSpec {});
    subsystem.AddInitializer(owe::ParticleParser::GenInitializer(
        owe::ParseJson(R"({"name":"lifetimerandom","min":10,"max":10})"_str).unwrap(), u32(1)));
    subsystem.AddInitializer(
        owe::ParticleParser::GenInitializer(owe::ParseJson(velocity).unwrap(), u32(1)));
    subsystem.AddEmitter(Box<dyn<particle::ParticleEmitterProgram>>::make(
        owe::BoxEmitterProgram(subsystem.SpawnPipeline(),
                               owe::ParticleBoxEmitterArgs {
                                   .origin        = { origin.x(), origin.y(), origin.z() },
                                   .instantaneous = u32(1),
                               },
                               usize())));
    owe::ParticleInstanceModifiers modifiers(Arc<owe::wpscene::ParticleInstanceoverride>::make(),
                                             owe::wpscene::Particle::EFlags { 0 },
                                             false);
    subsystem.AddOperator(
        owe::ParticleParser::GenOperator(owe::ParseJson(R"({"name":"movement"})"_str).unwrap(),
                                         modifiers.Clone(),
                                         subsystem,
                                         usize()));
    subsystem.AddOperator(owe::ParticleParser::GenOperator(
        owe::ParseJson(collision).unwrap(), modifiers.Clone(), subsystem, usize(1)));
    subsystem.Finalize();
    for (int tick = 0; tick < ticks; ++tick) subsystem.Tick(f64(1.0 / 60.0), false);
    CollidedParticle result;
    result.alive =
        subsystem.System().Instance(usize()).Binding().Read().Positions().len() != usize();
    if (! result.alive) return result;
    auto&                        storage = subsystem.System().Instance(usize()).Storage();
    particle::ParticleSlotReader value(storage, particle::ParticleSlot {});
    result.position = value.Read(subsystem.Attributes().position);
    result.velocity = value.Read(subsystem.Attributes().velocity);
    result.alpha    = value.Read(subsystem.Attributes().alpha);
    return result;
}

TEST(ParticleSubSystem, CollisionPlaneBouncesAParticleThatFellThrough) {
    const auto bounce = R"({"name":"collisionplane","distance":-100,"bouncefactor":0.5})"_str;
    auto       hit =
        CollideWithPlane(bounce,
                         { 0, -100, 0 },
                         R"({"name":"velocityrandom","min":"20 -60 0","max":"20 -60 0"})"_str);
    ASSERT_TRUE(hit.alive);
    EXPECT_NEAR(hit.position.y(), -100.0f, 1e-4f);
    EXPECT_NEAR(hit.velocity.y(), 30.0f, 1e-3f);
    EXPECT_NEAR(hit.velocity.x(), 20.0f, 1e-3f);
    auto above = CollideWithPlane(
        bounce, { 0, -50, 0 }, R"({"name":"velocityrandom","min":"0 -60 0","max":"0 -60 0"})"_str);
    EXPECT_NEAR(above.position.y(), -51.0f, 1e-4f);
    EXPECT_NEAR(above.velocity.y(), -60.0f, 1e-3f);
    auto under = CollideWithPlane(
        bounce, { 0, -150, 0 }, R"({"name":"velocityrandom","min":"0 40 0","max":"0 40 0"})"_str);
    EXPECT_NEAR(under.position.y(), -100.0f, 1e-4f);
    EXPECT_NEAR(under.velocity.y(), -20.0f, 1e-3f);
}

TEST(ParticleSubSystem, CollisionPlaneSlidesDeletesAndTakesAnyNormal) {
    auto slide = CollideWithPlane(
        R"({"name":"collisionplane","distance":-100,"bouncefactor":0.5,"collisionbehavior":"slide"})"_str,
        { 0, -100, 0 },
        R"({"name":"velocityrandom","min":"20 -60 0","max":"20 -60 0"})"_str);
    EXPECT_NEAR(slide.velocity.y(), 0.0f, 1e-3f);
    EXPECT_NEAR(slide.velocity.x(), 20.0f, 1e-3f);
    EXPECT_NEAR(slide.position.y(), -100.0f, 1e-4f);
    auto gone = CollideWithPlane(
        R"({"name":"collisionplane","distance":-100,"collisionbehavior":"delete"})"_str,
        { 0, -100, 0 },
        R"({"name":"velocityrandom","min":"0 -60 0","max":"0 -60 0"})"_str);
    EXPECT_FLOAT_EQ(gone.alive ? gone.alpha : 0.0f,
                    0.0f); // hidden at once, removed with the lifecycle
    auto wall = CollideWithPlane(
        R"({"name":"collisionplane","plane":"1 0 0","distance":-100,"bouncefactor":0.5})"_str,
        { -100, 0, 0 },
        R"({"name":"velocityrandom","min":"-60 7 0","max":"-60 7 0"})"_str);
    EXPECT_NEAR(wall.position.x(), -100.0f, 1e-4f);
    EXPECT_NEAR(wall.velocity.x(), 30.0f, 1e-3f);
    EXPECT_NEAR(wall.velocity.y(), 7.0f, 1e-3f);
}

// collisionbehavior "stop" (particles-collisionbounds-single): Wallpaper Engine's fourth behaviour puts the particle on
// the surface and takes all its speed, the sideways part too (a particle that hits the right edge at 40, 40 stays where
// it hit).
TEST(ParticleSubSystem, CollisionStopHoldsAParticleWhereItHits) {
    auto stopped = CollideWithPlane(
        R"({"name":"collisionplane","distance":-100,"bouncefactor":0.5,"collisionbehavior":"stop"})"_str,
        { 0, -100, 0 },
        R"({"name":"velocityrandom","min":"20 -60 0","max":"20 -60 0"})"_str);
    ASSERT_TRUE(stopped.alive);
    EXPECT_NEAR(stopped.position.y(), -100.0f, 1e-4f);
    EXPECT_NEAR(stopped.velocity.norm(), 0.0f, 1e-4f);
    auto free = CollideWithPlane(
        R"({"name":"collisionplane","distance":-100,"collisionbehavior":"stop"})"_str,
        { 0, -50, 0 },
        R"({"name":"velocityrandom","min":"20 -60 0","max":"20 -60 0"})"_str);
    EXPECT_NEAR(free.velocity.x(), 20.0f, 1e-3f); // not touching: left alone
}

// Without "bouncefactor" a collision bounces at 0.5: Wallpaper Engine's collision operators all
// fill a missing bouncefactor with 0.5 (0x1401c00a0) before reading it (ft-collisionbounds under
// replay: its default systems match only at 0.5).
TEST(ParticleSubSystem, CollisionsBounceHalfWithoutABounceFactor) {
    auto plane = CollideWithPlane(
        R"({"name":"collisionplane","distance":-100})"_str,
        { 0, -100, 0 },
        R"({"name":"velocityrandom","min":"0 -60 0","max":"0 -60 0"})"_str);
    EXPECT_NEAR(plane.velocity.y(), 30.0f, 1e-3f);
    auto bounds = CollideWithPlane(R"({"name":"collisionbounds"})"_str,
                                   { 1919, 500, 0 },
                                   R"({"name":"velocityrandom","min":"120 0 0","max":"120 0 0"})"_str,
                                   1,
                                   true);
    EXPECT_NEAR(bounds.velocity.x(), -60.0f, 1e-3f);
}

// collisionbounds (particles-collisionbounds-rest): the scene's edges, 0..1920 across and 0..1080
// up
TEST(ParticleSubSystem, CollisionBoundsKeepParticlesInsideTheScene) {
    const auto bounds   = R"({"name":"collisionbounds","bouncefactor":0.5})"_str;
    auto       velocity = [](ref<str> v) {
        return rstd::format(R"({{"name":"velocityrandom","min":"{}","max":"{}"}})", v, v);
    };
    auto run = [&](Eigen::Vector3f origin,
                   ref<str>
                            v,
                   ref<str> operator_json =
                       R"({"name":"collisionbounds","bouncefactor":0.5})"_str) {
        auto json = velocity(v);
        return CollideWithPlane(operator_json, origin, json.as_str(), 1, true);
    };
    auto right = run({ 1919, 500, 0 }, "120 0 0"_str);
    EXPECT_NEAR(right.position.x(), 1920.0f, 1e-3f);
    EXPECT_NEAR(right.velocity.x(), -60.0f, 1e-3f);
    auto left = run({ 1, 500, 0 }, "-120 30 0"_str);
    EXPECT_NEAR(left.position.x(), 0.0f, 1e-3f);
    EXPECT_NEAR(left.velocity.x(), 60.0f, 1e-3f);
    EXPECT_NEAR(left.velocity.y(), 30.0f, 1e-3f);
    auto floor = run({ 500, 1, 0 }, "0 -120 0"_str);
    EXPECT_NEAR(floor.position.y(), 0.0f, 1e-3f);
    EXPECT_NEAR(floor.velocity.y(), 60.0f, 1e-3f);
    auto ceiling = run({ 500, 1079, 0 }, "0 120 0"_str);
    EXPECT_NEAR(ceiling.position.y(), 1080.0f, 1e-3f);
    EXPECT_NEAR(ceiling.velocity.y(), -60.0f, 1e-3f);
    auto inside = run({ 500, 500, 0 }, "120 0 0"_str);
    EXPECT_NEAR(inside.velocity.x(), 120.0f, 1e-3f);
    auto gone = run({ 1919, 500, 0 },
                    "120 0 0"_str,
                    R"({"name":"collisionbounds","collisionbehavior":"delete"})"_str);
    EXPECT_FLOAT_EQ(gone.alive ? gone.alpha : 0.0f, 0.0f);
}

// collisionsphere / collisionquad (particles-collisionshapes, particles-collisionquad-sides)
TEST(ParticleSubSystem, CollisionSphereKeepsParticlesOutAlongTheSurfaceNormal) {
    const auto sphere =
        R"({"name":"collisionsphere","origin":"0 0 0","radius":100,"bouncefactor":0.5})"_str;
    auto top =
        CollideWithPlane(sphere,
                         { 0, 101, 0 },
                         R"({"name":"velocityrandom","min":"0 -120 0","max":"0 -120 0"})"_str);
    EXPECT_NEAR(top.position.y(), 100.0f, 1e-3f);
    EXPECT_NEAR(top.velocity.y(), 60.0f, 1e-3f);
    auto side = CollideWithPlane(
        sphere, { 60, 80, 0 }, R"({"name":"velocityrandom","min":"0 -60 0","max":"0 -60 0"})"_str);
    const Eigen::Vector3f moved(60.0f, 80.0f - 1.0f, 0.0f);
    const Eigen::Vector3f normal = moved.normalized();
    EXPECT_NEAR((side.position - normal * 100.0f).norm(), 0.0f, 1e-3f);
    const float           along    = normal.dot(Eigen::Vector3f(0, -60, 0));
    const Eigen::Vector3f expected = Eigen::Vector3f(0, -60, 0) + normal * (-along * 0.5f - along);
    EXPECT_NEAR((side.velocity - expected).norm(), 0.0f, 1e-3f);
    auto away = CollideWithPlane(
        sphere, { 0, 150, 0 }, R"({"name":"velocityrandom","min":"0 -60 0","max":"0 -60 0"})"_str);
    EXPECT_NEAR(away.velocity.y(), -60.0f, 1e-3f);
}

TEST(ParticleSubSystem, CollisionQuadCatchesAParticleFallingOnItButNotOneRisingThroughIt) {
    const auto quad =
        R"({"name":"collisionquad","origin":"0 -100 0","size":"200 100","bouncefactor":0.5})"_str;
    auto landed = CollideWithPlane(
        quad, { 50, -100, 0 }, R"({"name":"velocityrandom","min":"0 -60 0","max":"0 -60 0"})"_str);
    EXPECT_NEAR(landed.position.y(), -100.0f, 1e-4f);
    EXPECT_NEAR(landed.velocity.y(), 30.0f, 1e-3f);
    auto rising = CollideWithPlane(
        quad, { 50, -100.5f, 0 }, R"({"name":"velocityrandom","min":"0 60 0","max":"0 60 0"})"_str);
    EXPECT_NEAR(rising.velocity.y(), 60.0f, 1e-3f);
    auto beside = CollideWithPlane(
        quad, { 150, -100, 0 }, R"({"name":"velocityrandom","min":"0 -60 0","max":"0 -60 0"})"_str);
    EXPECT_NEAR(beside.velocity.y(), -60.0f, 1e-3f);
    auto angled = CollideWithPlane(
        R"({"name":"collisionquad","origin":"0 -100 0","size":"200 100","angles":"0 0 30","bouncefactor":0.5})"_str,
        { 50, -100, 0 },
        R"({"name":"velocityrandom","min":"0 -60 0","max":"0 -60 0"})"_str);
    EXPECT_NEAR(angled.velocity.y(), 30.0f, 1e-3f);
}

// inheritcontrolpointvelocity (particles-controlpoint-velocity): a particle starts with its control
// point's velocity (how fast the object moves, per second) times a factor between min and max. The
// first frame has no earlier position to measure from, so a particle born then gets none.
TEST(ParticleSubSystem, InheritControlpointVelocityTakesTheObjectsVelocity) {
    auto last_velocity = [](bool world_space) {
        owe::Scene             scene;
        owe::ParticleSubSystem subsystem(scene,
                                         Arc<owe::SceneMesh>::make(),
                                         u32(64),
                                         f64(1.0),
                                         u32(1),
                                         f64(1.0),
                                         owe::ParticleSubSystem::SpawnType::STATIC,
                                         owe::ParticleAnimationSpec {},
                                         {},
                                         u32(),
                                         f64(),
                                         f64(),
                                         world_space);
        subsystem.AddInitializer(owe::ParticleParser::GenInitializer(
            owe::ParseJson(R"({"name":"lifetimerandom","min":10,"max":10})"_str).unwrap(),
            u32(64)));
        subsystem.AddInitializer(owe::ParticleParser::GenInitializer(
            owe::ParseJson(
                R"({"name":"inheritcontrolpointvelocity","controlpoint":0,"min":0.5,"max":0.5})"_str)
                .unwrap(),
            u32(64)));
        owe::wpscene::Emitter emitter;
        emitter.name = String::make("boxrandom"_str);
        emitter.rate = 64.0f;
        subsystem.AddEmitter(owe::ParticleParser::GenEmitter(emitter, subsystem, usize()));
        subsystem.Finalize();
        auto owner = rstd::sync::Arc<owe::SceneNode>::make();
        subsystem.SetOwnerNode(owner.as_ptr());
        for (int tick = 0; tick < 4; ++tick) {
            owner->SetTranslate({ static_cast<float>(tick), 0.0f, 0.0f });
            subsystem.Tick(f64(1.0 / 64.0), false);
        }
        auto&                        storage = subsystem.System().Instance(usize()).Storage();
        particle::ParticleSlotReader first(storage, particle::ParticleSlot { .index = usize() });
        particle::ParticleSlotReader last(storage, particle::ParticleSlot { .index = usize(3) });
        EXPECT_NEAR(first.Read(subsystem.Attributes().velocity).x(), 0.0f, 1e-3f);
        return last.Read(subsystem.Attributes().velocity).x();
    };
    // in the world, 1 unit a 1/64 s tick is 64 a second; half of it
    EXPECT_NEAR(last_velocity(true), 32.0f, 1e-2f);
    // in the object's own space the control point doesn't move
    EXPECT_NEAR(last_velocity(false), 0.0f, 1e-3f);
}

TEST(ParticleSubSystem, UsesEmitterPeriodLimitForImplicitControlpointSequenceCount) {
    owe::Scene             scene;
    auto                   mesh = Arc<owe::SceneMesh>::make();
    owe::ParticleSubSystem subsystem(scene,
                                     mesh.clone(),
                                     u32(4),
                                     f64(),
                                     u32(1),
                                     f64(1.0),
                                     owe::ParticleSubSystem::SpawnType::STATIC,
                                     owe::ParticleAnimationSpec {});

    subsystem.ControlpointsMut()[usize(1)].base_offset = Eigen::Vector3d { 300.0, 0.0, 0.0 };
    subsystem.AddInitializer(owe::ParticleParser::GenInitializer(
        owe::ParseJson(R"({"name":"lifetimerandom","min":1,"max":1})"_str).unwrap(), u32(4)));
    auto sequence = owe::ParticleParser::GenInitializer(
        owe::ParseJson(R"({"name":"mapsequencebetweencontrolpoints"})"_str).unwrap(), u32(4));
    ASSERT_EQ(sequence.SequenceCount(), Some(u32(4)));
    subsystem.SetRopeSequenceCount(*sequence.SequenceCount());
    subsystem.AddInitializer(rstd::move(sequence));
    auto explicit_sequence = owe::ParticleParser::GenInitializer(
        owe::ParseJson(R"({"name":"mapsequencebetweencontrolpoints","count":3})"_str).unwrap(),
        u32(4));
    EXPECT_EQ(explicit_sequence.SequenceCount(), Some(u32(3)));
    EXPECT_EQ(subsystem.RopeSequenceCount(), Some(u32(4)));
    subsystem.AddEmitter(Box<dyn<particle::ParticleEmitterProgram>>::make(
        owe::SphereEmitterProgram(subsystem.SpawnPipeline(),
                                  owe::ParticleSphereEmitterArgs {
                                      .directions    = { 1.0f, 1.0f, 0.0f },
                                      .instantaneous = u32(4),
                                  },
                                  usize())));
    subsystem.Finalize();
    subsystem.Tick(f64(), false);

    auto positions = subsystem.System().Instance(usize()).Binding().Read().Positions();
    ASSERT_EQ(positions.len(), usize(4));
    EXPECT_TRUE(positions[usize()].isApprox(Eigen::Vector3f { 0.0f, 0.0f, 0.0f }));
    EXPECT_TRUE(positions[usize(1)].isApprox(Eigen::Vector3f { 100.0f, 0.0f, 0.0f }));
    EXPECT_TRUE(positions[usize(2)].isApprox(Eigen::Vector3f { 200.0f, 0.0f, 0.0f }));
    EXPECT_TRUE(positions[usize(3)].isApprox(Eigen::Vector3f { 300.0f, 0.0f, 0.0f }));
}

TEST(ParticleSubSystem, MapsParentParticlesIntoStaticChildControlpoints) {
    owe::Scene             scene;
    auto                   parent_mesh = Arc<owe::SceneMesh>::make();
    owe::ParticleSubSystem parent(scene,
                                  parent_mesh.clone(),
                                  u32(1),
                                  f64(),
                                  u32(1),
                                  f64(1.0),
                                  owe::ParticleSubSystem::SpawnType::STATIC,
                                  owe::ParticleAnimationSpec {});
    parent.AddInitializer(owe::ParticleParser::GenInitializer(
        owe::ParseJson(R"({"name":"lifetimerandom","min":1,"max":1})"_str).unwrap(), u32(2)));
    parent.AddEmitter(Box<dyn<particle::ParticleEmitterProgram>>::make(
        owe::SphereEmitterProgram(parent.SpawnPipeline(),
                                  owe::ParticleSphereEmitterArgs {
                                      .origin        = { 50.0f, 0.0f, 0.0f },
                                      .instantaneous = u32(1),
                                  },
                                  usize())));

    auto child =
        Box<owe::ParticleSubSystem>::make(scene,
                                          Arc<owe::SceneMesh>::make(),
                                          u32(1),
                                          f64(),
                                          u32(1),
                                          f64(1.0),
                                          owe::ParticleSubSystem::SpawnType::STATIC_CONTROLPOINT,
                                          owe::ParticleAnimationSpec {});
    child->SetParentControlpointStartIndex(i32(1));
    child->Finalize();
    auto* child_system = child.get();
    parent.AddChild(rstd::move(child));
    parent.Finalize();

    parent.Tick(f64(1.0 / 60.0), false);

    ASSERT_EQ(child_system->System().InstanceCount(), usize(1));
    EXPECT_TRUE(child_system->Controlpoints()[usize()].offset.isZero());
    EXPECT_TRUE(child_system->Controlpoints()[usize(1)].offset.isApprox(
        Eigen::Vector3d { 50.0, 0.0, 0.0 }));
}

TEST(ParticleSubSystem, ParentOverriddenMouseControlpointKeepsEmittingOutside) {
    for (auto name : { "sphererandom"_str, "boxrandom"_str }) {
        owe::Scene scene;
        scene.SetPointerPosition({ 0.25f, 0.5f });
        ASSERT_FALSE(scene.PointerInWindow());
        owe::ParticleSubSystem parent(scene,
                                      Arc<owe::SceneMesh>::make(),
                                      u32(1),
                                      f64(),
                                      u32(1),
                                      f64(1.0),
                                      owe::ParticleSubSystem::SpawnType::STATIC,
                                      owe::ParticleAnimationSpec {});
        parent.AddInitializer(owe::ParticleParser::GenInitializer(
            owe::ParseJson(R"({"name":"lifetimerandom","min":1,"max":1})"_str).unwrap(), u32(1)));
        parent.AddEmitter(Box<dyn<particle::ParticleEmitterProgram>>::make(
            owe::SphereEmitterProgram(parent.SpawnPipeline(),
                                      owe::ParticleSphereEmitterArgs {
                                          .origin        = { 50.0f, 0.0f, 0.0f },
                                          .instantaneous = u32(1),
                                      },
                                      usize())));
        auto child = Box<owe::ParticleSubSystem>::make(
            scene,
            Arc<owe::SceneMesh>::make(),
            u32(64),
            f64(1.0),
            u32(1),
            f64(1.0),
            owe::ParticleSubSystem::SpawnType::STATIC_CONTROLPOINT,
            owe::ParticleAnimationSpec {});
        child->SetParentControlpointStartIndex(i32());
        child->ControlpointsMut()[usize()].link_mouse = true;
        child->AddInitializer(owe::ParticleParser::GenInitializer(
            owe::ParseJson(R"({"name":"lifetimerandom","min":0.5,"max":0.5})"_str).unwrap(),
            u32(64)));
        owe::wpscene::Emitter emitter;
        emitter.name         = String::make(name);
        emitter.rate         = 64.0f;
        emitter.controlpoint = i32();
        child->AddEmitter(owe::ParticleParser::GenEmitter(emitter, *child, usize()));
        child->Finalize();
        auto* child_system = child.get();
        parent.AddChild(rstd::move(child));
        parent.Finalize();

        parent.Tick(f64(1.0 / 64.0), false);
        ASSERT_EQ(child_system->System().InstanceCount(), usize(1));
        ASSERT_TRUE(child_system->Controlpoints()[usize()].offset.isApprox(
            Eigen::Vector3d { 50.0, 0.0, 0.0 }));
        EXPECT_EQ(LiveParticleCount(*child_system), usize(1));
        parent.Tick(f64(1.0 / 64.0), false);
        EXPECT_EQ(LiveParticleCount(*child_system), usize(2));

        // Once this point is no longer parent-overridden, it must obey the
        // absent pointer again; moving the override back enables emission.
        child_system->SetParentControlpointStartIndex(i32(1));
        parent.Tick(f64(1.0 / 64.0), false);
        EXPECT_EQ(LiveParticleCount(*child_system), usize(2));
        child_system->SetParentControlpointStartIndex(i32());
        parent.Tick(f64(1.0 / 64.0), false);
        EXPECT_EQ(LiveParticleCount(*child_system), usize(3));
    }
}

TEST(ParticleInstanceModifiers, CloneRetainsReadOnlyBindingsAfterSourceReplacement) {
    owe::wpscene::ParticleInstanceoverride source;
    auto                                   json = rstd::json::from_str(R"({
        "controlpointangle0":{"animation":{"c0":[{"frame":0,"value":7}]}}
    })"_str)
                                                      .unwrap();
    ASSERT_TRUE(source.FromJosn(json));
    auto copy = source.clone();
    ASSERT_NE(copy.FieldBindingsView(), nullptr);
    EXPECT_EQ(copy.FieldBindingsView(), source.FieldBindingsView());
    auto identity = copy.FieldBindingsView()->Get("controlpointangle0"_str).unwrap()->identity;
    ASSERT_TRUE(source.FromJosn(owe::Json::Object(rstd::json::Map::make())));
    EXPECT_NE(copy.FieldBindingsView(), source.FieldBindingsView());
    EXPECT_TRUE(source.FieldBindingsView()->IsEmpty());
    auto binding = copy.FieldBindingsView()->Get("controlpointangle0"_str).unwrap();
    EXPECT_EQ(binding->identity, identity);
    ASSERT_TRUE(binding->animation.is_some());
    EXPECT_FLOAT_EQ(binding->animation->c0[usize()].value, 7.0f);
}

// Shimmering Particles (and other custom particle shaders) ship only a .vert and a .frag: their
// vertex shader expands each particle into a quad itself, so they are drawn without a geometry
// stage.
TEST(ParticleShaders, ShadersWithoutAGeometryStageExpandTheirQuadsInTheVertexShader) {
    auto root = std::filesystem::temp_directory_path() /
                ("owe-particle-shaders-" + std::to_string(rstd::process::id().to_primitive()));
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root / "shaders");
    for (auto file : { "with_geometry.vert",
                       "with_geometry.geom",
                       "with_geometry.frag",
                       "without_geometry.vert",
                       "without_geometry.frag" }) {
        std::ofstream(root / "shaders" / file) << "void main() {}\n";
    }

    owe::fs::VFS vfs;
    auto         physical =
        owe::fs::make_physical_fs(owe::fs::Path(rstd::cppstd::as_str(root.string()).unwrap()));
    ASSERT_TRUE(physical.is_ok());
    ASSERT_TRUE(vfs.mount("/assets"_str, std::move(physical).unwrap_unchecked()).is_ok());

    EXPECT_TRUE(owe::ParticleShaderUsesGeometryStage(vfs, "with_geometry"_str, true));
    EXPECT_FALSE(owe::ParticleShaderUsesGeometryStage(vfs, "without_geometry"_str, true));
    // without geometry shaders on the device, none uses one
    EXPECT_FALSE(owe::ParticleShaderUsesGeometryStage(vfs, "with_geometry"_str, false));

    std::filesystem::remove_all(root);
}

// maintaindistancebetweencontrolpoints (particles-maintain-between): a particle stays between
// control points 0 and 1 along the axis joining them (clamped to the nearer end), drifts a little
// towards control point 0 (about 1.1 % a second), and keeps its sideways place.
static Eigen::Vector3f KeepBetweenControlpoints(Eigen::Vector3f origin, int ticks = 1,
                                                Eigen::Vector3d second = { 200.0, 0.0, 0.0 },
                                                double          tick   = 1.0 / 60.0) {
    owe::Scene             scene;
    auto                   mesh = Arc<owe::SceneMesh>::make();
    owe::ParticleSubSystem subsystem(scene,
                                     mesh.clone(),
                                     u32(1),
                                     f64(1.0),
                                     u32(1),
                                     f64(1.0),
                                     owe::ParticleSubSystem::SpawnType::STATIC,
                                     owe::ParticleAnimationSpec {});
    subsystem.ControlpointsMut()[usize(1)].base_offset = second;
    subsystem.AddInitializer(owe::ParticleParser::GenInitializer(
        owe::ParseJson(R"({"name":"lifetimerandom","min":100,"max":100})"_str).unwrap(), u32(1)));
    subsystem.AddEmitter(Box<dyn<particle::ParticleEmitterProgram>>::make(
        owe::BoxEmitterProgram(subsystem.SpawnPipeline(),
                               owe::ParticleBoxEmitterArgs {
                                   .origin        = { origin.x(), origin.y(), origin.z() },
                                   .instantaneous = u32(1),
                               },
                               usize())));
    owe::ParticleInstanceModifiers modifiers(Arc<owe::wpscene::ParticleInstanceoverride>::make(),
                                             owe::wpscene::Particle::EFlags { 0 },
                                             false);
    subsystem.AddOperator(owe::ParticleParser::GenOperator(
        owe::ParseJson(R"({"name":"maintaindistancebetweencontrolpoints"})"_str).unwrap(),
        modifiers.Clone(),
        subsystem,
        usize()));
    subsystem.Finalize();
    for (int count = 0; count < ticks; ++count) subsystem.Tick(f64(tick), false);
    auto&                        storage = subsystem.System().Instance(usize()).Storage();
    particle::ParticleSlotReader value(storage, particle::ParticleSlot {});
    return value.Read(subsystem.Attributes().position);
}

TEST(ParticleSubSystem, MaintainDistanceBetweenControlpointsClampsAlongTheAxis) {
    EXPECT_NEAR(
        KeepBetweenControlpoints({ 260, 100, 0 }).x(), 200.0f, 0.1f); // past control point 1
    EXPECT_NEAR(
        KeepBetweenControlpoints({ -50, 100, 0 }).x(), 0.0f, 0.1f); // behind control point 0
    EXPECT_NEAR(KeepBetweenControlpoints({ 100, 100, 0 }).x(),
                100.0f,
                0.1f); // between: left (barely drifting)
    EXPECT_NEAR(
        KeepBetweenControlpoints({ 100, 100, 0 }).y(), 100.0f, 1e-4f); // sideways place kept
}

// Wallpaper Engine normalises the axis with the CPU's approximate reciprocal square root (rsqrtps),
// so every frame a particle's place along the axis is scaled by |axis|^2, a little under or over 1
// depending on the control points' distance (particles-maintain-drift: 1 - 3.36e-4 for 100, 200 and
// 400 apart, 1 - 1.45e-4 for 150 and 300, and 1 + 4.22e-4 for a 45 degree axis of length 200, which
// pushes particles away from control point 0).
static float AxisScalePerFrame(Eigen::Vector3f span) {
#if defined(__SSE__)
    float inverse = _mm_cvtss_f32(_mm_rsqrt_ss(_mm_set_ss(span.squaredNorm())));
#else
    float inverse = 1.0f / span.norm();
#endif
    return (span * inverse).squaredNorm();
}

TEST(ParticleSubSystem, MaintainDistanceBetweenControlpointsDriftsByFramesNotSeconds) {
    // the same number of frames moves a particle as far at 30 and at 60 frames a second
    auto at30 = KeepBetweenControlpoints({ 100, 0, 0 }, 360, { 200.0, 0.0, 0.0 }, 1.0 / 30.0);
    auto at60 = KeepBetweenControlpoints({ 100, 0, 0 }, 360, { 200.0, 0.0, 0.0 }, 1.0 / 60.0);
    EXPECT_NEAR(at30.x(), at60.x(), 1e-3f);
}

TEST(ParticleSubSystem, MaintainDistanceBetweenControlpointsScalesByTheApproximateAxis) {
    for (Eigen::Vector3f second : { Eigen::Vector3f { 200, 0, 0 },
                                    Eigen::Vector3f { 150, 0, 0 },
                                    Eigen::Vector3f { 141.421f, 141.421f, 0 } }) {
        double expected = 100.0;
        for (int frame = 0; frame < 600; ++frame) expected *= AxisScalePerFrame(second);
        auto after =
            KeepBetweenControlpoints(second.normalized() * 100.0f, 600, second.cast<double>());
        EXPECT_NEAR(after.norm(), expected, 0.02) << second.transpose();
    }
}

// Instantaneous emitters fire when the system has no particles; every one of them does, not only
// the first to run (particles-instantaneous-emitters: Wallpaper Engine shows a particle from each
// of several such emitters).
TEST(ParticleSubSystem, EveryInstantaneousEmitterFiresIntoAnEmptySystem) {
    owe::Scene             scene;
    auto                   mesh = Arc<owe::SceneMesh>::make();
    owe::ParticleSubSystem subsystem(scene,
                                     mesh.clone(),
                                     u32(8),
                                     f64(1.0),
                                     u32(1),
                                     f64(1.0),
                                     owe::ParticleSubSystem::SpawnType::STATIC,
                                     owe::ParticleAnimationSpec {});
    subsystem.AddInitializer(owe::ParticleParser::GenInitializer(
        owe::ParseJson(R"({"name":"lifetimerandom","min":100,"max":100})"_str).unwrap(), u32(1)));
    for (usize index {}; index < usize(3); ++index) {
        subsystem.AddEmitter(Box<dyn<particle::ParticleEmitterProgram>>::make(
            owe::BoxEmitterProgram(subsystem.SpawnPipeline(),
                                   owe::ParticleBoxEmitterArgs {
                                       .origin        = { 10.0f, 0.0f, 0.0f },
                                       .instantaneous = u32(2),
                                   },
                                   index)));
    }
    subsystem.Finalize();
    subsystem.Tick(f64(1.0 / 30.0), false);
    EXPECT_EQ(LiveParticleCount(subsystem), usize(6));
    subsystem.Tick(f64(1.0 / 30.0), false);
    EXPECT_EQ(LiveParticleCount(subsystem), usize(6)); // not again while particles live
}

// boids (particles-boids): particles at rest with only the boids operator; returns each particle's
// velocity, ordered by x. Wallpaper Engine runs the operator on groups of four particles, and the
// unused places of the last group count as particles at rest at the system's origin: a pair at -d/2
// and d/2 has two such neighbours half way between them.
struct BoidsResult {
    int             count {};
    Eigen::Vector3f velocity[4];
};

static BoidsResult
BoidsVelocities(ref<str> config, std::initializer_list<float> xs, int ticks = 1,
                double   tick     = 1.0 / 30.0,
                ref<str> velocity = R"({"name":"velocityrandom","min":"0 0 0","max":"0 0 0"})"_str,
                u32      target_fps = u32()) {
    owe::Scene scene;
    if (target_fps > u32()) scene.SetTargetFps(target_fps);
    auto                   mesh = Arc<owe::SceneMesh>::make();
    owe::ParticleSubSystem subsystem(scene,
                                     mesh.clone(),
                                     u32(8),
                                     f64(1.0),
                                     u32(1),
                                     f64(1.0),
                                     owe::ParticleSubSystem::SpawnType::STATIC,
                                     owe::ParticleAnimationSpec {});
    subsystem.AddInitializer(owe::ParticleParser::GenInitializer(
        owe::ParseJson(R"({"name":"lifetimerandom","min":100,"max":100})"_str).unwrap(), u32(1)));
    subsystem.AddInitializer(
        owe::ParticleParser::GenInitializer(owe::ParseJson(velocity).unwrap(), u32(2)));
    int emitter {};
    for (float x : xs) {
        subsystem.AddEmitter(Box<dyn<particle::ParticleEmitterProgram>>::make(
            owe::BoxEmitterProgram(subsystem.SpawnPipeline(),
                                   owe::ParticleBoxEmitterArgs {
                                       .origin        = { x, 0.0f, 0.0f },
                                       .instantaneous = u32(1),
                                   },
                                   usize(emitter++))));
    }
    subsystem.AddOperator(owe::ParticleParser::GenOperator(
        owe::ParseJson(config).unwrap(),
        owe::ParticleInstanceModifiers(Arc<owe::wpscene::ParticleInstanceoverride>::make(),
                                       owe::wpscene::Particle::EFlags { 0 },
                                       false),
        subsystem,
        usize()));
    subsystem.Finalize();
    for (int count = 0; count < ticks; ++count) subsystem.Tick(f64(tick), false);
    BoidsResult result {};
    float       place[4] {};
    for (usize instance {}; instance < subsystem.System().InstanceCount(); ++instance) {
        auto& storage = subsystem.System().Instance(instance).Storage();
        usize slot {};
        for (const auto& state : subsystem.System().Instance(instance).Binding().Read().States()) {
            if (state.active && result.count < 4) {
                particle::ParticleSlotReader value(storage,
                                                   particle::ParticleSlot { .index = slot });
                place[result.count]           = value.Read(subsystem.Attributes().position).x();
                result.velocity[result.count] = value.Read(subsystem.Attributes().velocity);
                ++result.count;
            }
            ++slot;
        }
    }
    for (int i = 0; i < result.count; ++i) {
        for (int j = i + 1; j < result.count; ++j) {
            if (place[j] < place[i]) {
                auto p             = place[i];
                place[i]           = place[j];
                place[j]           = p;
                auto v             = result.velocity[i];
                result.velocity[i] = result.velocity[j];
                result.velocity[j] = v;
            }
        }
    }
    EXPECT_EQ(result.count, emitter);
    return result;
}

// One frame at 30 a second moves velocities by factor x average x dt x (0.025 x 30)^0.7 (Wallpaper
// Engine's frame-rate compensation; at 40 frames a second and more the time step is used as it is).
static constexpr double kBoidsStep30 = (1.0 / 30.0) * 0.81766;

TEST(ParticleSubSystem, BoidsSeparationAveragesOverNeighboursIncludingTheEmptyPlaces) {
    auto config =
        R"({"name":"boids","separationthreshold":60,"neighborthreshold":200,"separationfactor":1,
                      "alignmentfactor":0,"cohesionfactor":0})"_str;
    // 40 apart: the other particle (60 - 40) and two empty places 20 away (60 - 20 each), averaged
    // over 3
    auto near = BoidsVelocities(config, { -20.0f, 20.0f });
    EXPECT_NEAR(near.velocity[1].x(), kBoidsStep30 * 100.0 / 3.0, 0.01);
    EXPECT_NEAR(near.velocity[0].x(), -near.velocity[1].x(), 1e-4);
    // 100 apart: only the empty places 50 away (60 - 50)
    EXPECT_NEAR(
        BoidsVelocities(config, { -50.0f, 50.0f }).velocity[1].x(), kBoidsStep30 * 10.0, 0.005);
    // 130 apart: nothing within 60
    EXPECT_NEAR(BoidsVelocities(config, { -65.0f, 65.0f }).velocity[1].x(), 0.0, 1e-6);
}

TEST(ParticleSubSystem, BoidsCohesionPullsTowardsTheNeighboursMeanPosition) {
    auto config =
        R"({"name":"boids","separationthreshold":60,"neighborthreshold":200,"separationfactor":0,
                      "alignmentfactor":0,"cohesionfactor":1})"_str;
    // 150 apart: the mean of the other particle and the two empty places is 25 from the origin, 100
    // from -75
    EXPECT_NEAR(
        BoidsVelocities(config, { -75.0f, 75.0f }).velocity[0].x(), kBoidsStep30 * 100.0, 0.05);
    // 250 apart: the other particle is out of reach, the empty places pull to the origin
    EXPECT_NEAR(
        BoidsVelocities(config, { -125.0f, 125.0f }).velocity[0].x(), kBoidsStep30 * 125.0, 0.05);
    // three particles: one empty place
    auto three = BoidsVelocities(config, { -60.0f, -40.0f, 60.0f });
    EXPECT_NEAR(three.velocity[0].x(), kBoidsStep30 * (20.0 / 3.0 + 60.0), 0.05);
}

TEST(ParticleSubSystem, BoidsAlignmentSteersTowardsTheNeighboursMeanVelocity) {
    auto config =
        R"({"name":"boids","separationthreshold":1,"neighborthreshold":200,"separationfactor":0,
                      "alignmentfactor":1,"cohesionfactor":0})"_str;
    // one particle moving at 10 with three empty places at rest: the mean velocity is 0
    auto alone = BoidsVelocities(config,
                                 { 30.0f },
                                 1,
                                 1.0 / 30.0,
                                 R"({"name":"velocityrandom","min":"10 0 0","max":"10 0 0"})"_str);
    EXPECT_NEAR(alone.velocity[0].x(), 10.0 - kBoidsStep30 * 10.0, 0.005);
}

TEST(ParticleSubSystem, BoidsCompensatesForTheFrameRate) {
    auto config =
        R"({"name":"boids","separationthreshold":60,"neighborthreshold":200,"separationfactor":1,
                      "alignmentfactor":0,"cohesionfactor":0})"_str;
    // at 60 frames a second each step uses the whole time step: one second gains 1 / 0.81766 times
    // what it does at 30
    auto at30 = BoidsVelocities(config, { -50.0f, 50.0f }, 30, 1.0 / 30.0).velocity[1].x();
    auto at60 = BoidsVelocities(config, { -50.0f, 50.0f }, 60, 1.0 / 60.0).velocity[1].x();
    EXPECT_NEAR(at60 / at30, 1.0 / 0.81766, 0.01);
}

// Low frame rates (particles-lowfps-substeps): the subsystem reads the scene's target frame rate.
// dt2 = dt x min(0.025 / target frame time, 1)^0.7, from the target frame time, not the substep;
// at 1..20 frames a second the operators run twice with half of both, the emitters and ages once
// with the whole step.
struct OperatorTrace {
    rstd::vec::Vec<double> deltas;
    rstd::vec::Vec<double> deltas2;
    usize                  live {};
    float                  lifetime {};
};

static OperatorTrace TraceOperators(u32 target_fps, double tick) {
    owe::Scene scene;
    if (target_fps > u32()) scene.SetTargetFps(target_fps);
    auto                   mesh = Arc<owe::SceneMesh>::make();
    owe::ParticleSubSystem subsystem(scene,
                                     mesh.clone(),
                                     u32(8),
                                     f64(1.0),
                                     u32(1),
                                     f64(1.0),
                                     owe::ParticleSubSystem::SpawnType::STATIC,
                                     owe::ParticleAnimationSpec {});
    subsystem.AddInitializer(owe::ParticleParser::GenInitializer(
        owe::ParseJson(R"({"name":"lifetimerandom","min":10,"max":10})"_str).unwrap(), u32(1)));
    subsystem.AddEmitter(Box<dyn<particle::ParticleEmitterProgram>>::make(
        owe::BoxEmitterProgram(subsystem.SpawnPipeline(),
                               owe::ParticleBoxEmitterArgs { .instantaneous = u32(1) },
                               usize())));
    OperatorTrace trace;
    subsystem.AddOperator(Box<dyn<particle::ParticleUpdateProgram>>::make(DeltaTraceUpdate {
        .deltas = rstd::addressof(trace.deltas), .deltas2 = rstd::addressof(trace.deltas2) }));
    subsystem.Finalize();
    subsystem.Tick(f64(tick), false); // spawns
    trace.deltas.clear();
    trace.deltas2.clear();
    subsystem.Tick(f64(tick), false);
    trace.live = LiveParticleCount(subsystem);
    for (usize instance {}; instance < subsystem.System().InstanceCount(); ++instance) {
        auto& storage = subsystem.System().Instance(instance).Storage();
        usize slot {};
        for (const auto& state : subsystem.System().Instance(instance).Binding().Read().States()) {
            if (state.active) {
                particle::ParticleSlotReader value(storage,
                                                   particle::ParticleSlot { .index = slot });
                trace.lifetime = value.Read(subsystem.Attributes().lifetime);
            }
            ++slot;
        }
    }
    return trace;
}

static double Dt2Scale(double fps) {
    return f64(rstd::cmp::min(0.025 * fps, 1.0)).powf(f64(0.7)).to_primitive();
}

TEST(ParticleSubSystem, OperatorsRunTwiceWithHalfStepsAtTwentyFramesASecondOrLess) {
    for (u32 fps : { u32(15), u32(20), u32(1) }) {
        const double tick  = 1.0 / static_cast<double>(fps.to_primitive());
        auto         trace = TraceOperators(fps, tick);
        ASSERT_EQ(trace.deltas.len(), usize(2)) << fps.to_primitive();
        for (usize index {}; index < usize(2); ++index) {
            EXPECT_NEAR(trace.deltas[index], tick / 2.0, 1e-9);
            EXPECT_NEAR(trace.deltas2[index],
                        tick / 2.0 * Dt2Scale(static_cast<double>(fps.to_primitive())),
                        1e-6);
        }
        // emitters and ages once a frame with the whole step
        EXPECT_EQ(trace.live, usize(1));
        // spawned after the first tick's ages, so one whole step older
        if (fps > u32(1)) EXPECT_NEAR(trace.lifetime, 10.0 - tick, 1e-4);
    }
}

TEST(ParticleSubSystem, OperatorsRunOnceFromTwentyOneFramesASecond) {
    for (u32 fps : { u32(21), u32(30), u32(60) }) {
        const double tick  = 1.0 / static_cast<double>(fps.to_primitive());
        auto         trace = TraceOperators(fps, tick);
        ASSERT_EQ(trace.deltas.len(), usize(1)) << fps.to_primitive();
        EXPECT_NEAR(trace.deltas[usize()], tick, 1e-9);
        EXPECT_NEAR(
            trace.deltas2[usize()], tick * Dt2Scale(static_cast<double>(fps.to_primitive())), 1e-6);
    }
}

TEST(ParticleSubSystem, Dt2ComesFromTheStepAndThePassesFromTheTarget) {
    // a 15 fps target with a longer step: two half steps (the target), dt2 scaled by the step's own rate (10 fps)
    auto trace = TraceOperators(u32(15), 0.1);
    ASSERT_EQ(trace.deltas.len(), usize(2));
    EXPECT_NEAR(trace.deltas2[usize()], 0.05 * Dt2Scale(10.0), 1e-6);
    // without a target frame rate the step stands in for the frame time, in one pass
    auto unknown = TraceOperators(u32(), 1.0 / 15.0);
    ASSERT_EQ(unknown.deltas.len(), usize(1));
    EXPECT_NEAR(unknown.deltas2[usize()], (1.0 / 15.0) * Dt2Scale(15.0), 1e-6);
}

TEST(ParticleSubSystem, BoidsTakesTwoHalfStepsAtFifteenFramesASecond) {
    auto config =
        R"({"name":"boids","separationthreshold":1,"neighborthreshold":200,"separationfactor":0,
                      "alignmentfactor":1,"cohesionfactor":0})"_str;
    // one particle moving at 10 with three empty places at rest steers to 0 twice a frame by half
    // of dt2
    const double step = (1.0 / 15.0) * Dt2Scale(15.0);
    auto alone = BoidsVelocities(config,
                                 { 30.0f },
                                 1,
                                 1.0 / 15.0,
                                 R"({"name":"velocityrandom","min":"10 0 0","max":"10 0 0"})"_str,
                                 u32(15));
    EXPECT_NEAR(alone.velocity[0].x(), 10.0 * (1.0 - step / 2.0) * (1.0 - step / 2.0), 1e-3);
}

// Emitter flag 4 makes emission periodic: on for minperiodicduration..maxperiodicduration, off
// for minperiodicdelay..maxperiodicdelay, again and again; maxtoemitperperiod caps each on
// phase, and a phase switch drops the rest of the frame (particles-emitter-periodic, GPU VM fps
// 30: 1 s on and 1 s off repeat every 2.06 s, 1 on and 0.5 off every 1.55 s; at most 5 a period
// gives 5 particles at the start of each; the keys without flag 4 change nothing). "delay" starts
// the emitter that much later.
TEST(ParticleEmitterTiming, PeriodicEmissionSwitchesOnAndOff) {
    auto read = [](ref<str> json) {
        owe::wpscene::Emitter emitter;
        EXPECT_TRUE(emitter.FromJson(owe::ParseJson(json).unwrap()));
        return owe::ParticleEmitterTiming::From(emitter);
    };
    auto periodic = read(
        R"({"name":"boxrandom","rate":10,"flags":4,"minperiodicdelay":1,"maxperiodicdelay":1,"minperiodicduration":1,"maxperiodicduration":1})"_str);
    EXPECT_TRUE(periodic.periodic);
    auto unflagged = read(
        R"({"name":"boxrandom","rate":10,"minperiodicdelay":1,"maxperiodicdelay":1,"minperiodicduration":1,"maxperiodicduration":1})"_str);
    EXPECT_FALSE(unflagged.periodic);

    // which of `frames` frames at 30 a second an emitter may emit in
    auto run = [](const owe::ParticleEmitterTiming& timing, int frames) {
        owe::ParticleInstanceState::EmitterState state;
        Vec<bool>                                on;
        for (int k = 0; k < frames; ++k)
            on.push(owe::AdvanceEmitterTiming(state, timing, 1.0 / 30.0));
        return on;
    };
    // the frames the on phases start at
    auto starts = [](const Vec<bool>& on) {
        Vec<int> out;
        for (usize k {}; k < on.len(); ++k)
            if (on[k] && (k == usize() || ! on[k - usize(1)]))
                out.push(rstd::as_cast<int>(k.to_primitive()));
        return out;
    };
    auto cycle = starts(run(periodic, 200));
    ASSERT_GE(cycle.len(), usize(3));
    EXPECT_EQ(cycle[usize(0)], 0);
    // 1 s on and 1 s off lose a frame at each switch: 62 frames, 2.07 s
    EXPECT_NEAR(cycle[usize(1)] - cycle[usize(0)], 62, 1);
    EXPECT_NEAR(cycle[usize(2)] - cycle[usize(1)], 62, 1);
    auto half  = periodic;
    half.min_delay = half.max_delay = 0.5f;
    auto quick = starts(run(half, 200));
    ASSERT_GE(quick.len(), usize(2));
    EXPECT_NEAR(quick[usize(1)] - quick[usize(0)], 47, 1);
    // no flag: always on
    for (bool on : run(unflagged, 90)) EXPECT_TRUE(on);
    // a start delay of 2 s
    auto delayed = run(read(R"({"name":"boxrandom","rate":10,"delay":2})"_str), 90);
    EXPECT_FALSE(delayed[usize(30)]);
    EXPECT_FALSE(delayed[usize(58)]);
    EXPECT_TRUE(delayed[usize(62)]);
    EXPECT_TRUE(delayed[usize(89)]);

    // at most 5 a period
    auto capped           = periodic;
    capped.max_per_period = u32(5);
    owe::ParticleInstanceState::EmitterState state;
    EXPECT_TRUE(owe::AdvanceEmitterTiming(state, capped, 1.0 / 30.0));
    EXPECT_EQ(owe::EmitterTimingAllowance(state, capped, u32(3)), u32(3));
    state.period_emitted = u32(3);
    EXPECT_EQ(owe::EmitterTimingAllowance(state, capped, u32(3)), u32(2));
    state.period_emitted = u32(5);
    EXPECT_EQ(owe::EmitterTimingAllowance(state, capped, u32(3)), u32());
    EXPECT_EQ(owe::EmitterTimingAllowance(state, periodic, u32(7)), u32(7));
}

// An operator's blendinstart..blendinend and blendoutstart..blendoutend (fractions of the
// particle's life) blend its effect in from nothing and back out, linearly
// (particles-operator-blend: turbulence and oscillateposition blended in over 0.5..0.75 leave a
// stream straight for its first half; particles-remap-timescale measured the same ramp for
// remapvalue).
TEST(ParticleOperatorWindow, WeighsAnOperatorByTheParticlesLife) {
    auto window = [](ref<str> json) {
        return owe::ParticleOperatorWindow::FromJson(owe::ParseJson(json).unwrap());
    };
    auto none = window(R"({"name":"turbulence"})"_str);
    EXPECT_EQ(none.Weight(0.0), 1.0);
    EXPECT_EQ(none.Weight(0.9), 1.0);
    auto in = window(R"({"name":"turbulence","blendinstart":0.5,"blendinend":0.75})"_str);
    EXPECT_EQ(in.Weight(0.4), 0.0);
    EXPECT_NEAR(in.Weight(0.625), 0.5, 1e-6);
    EXPECT_EQ(in.Weight(0.8), 1.0);
    auto both = window(
        R"({"name":"turbulence","blendinstart":0.1,"blendinend":0.5,"blendoutstart":0.6,"blendoutend":0.7})"_str);
    EXPECT_NEAR(both.Weight(0.3), 0.5, 1e-6);
    EXPECT_EQ(both.Weight(0.55), 1.0);
    EXPECT_NEAR(both.Weight(0.65), 0.5, 1e-6);
    EXPECT_EQ(both.Weight(0.75), 0.0);
}

TEST(ParticleSubSystem, TurbulenceAndOscillationWaitForTheirBlendWindow) {
    auto simulate = [](ref<str> config) -> array<Eigen::Vector3f, 2> {
        owe::Scene             scene;
        auto                   mesh = Arc<owe::SceneMesh>::make();
        owe::ParticleSubSystem subsystem(scene,
                                         mesh.clone(),
                                         u32(1),
                                         f64(1.0),
                                         u32(1),
                                         f64(1.0),
                                         owe::ParticleSubSystem::SpawnType::STATIC,
                                         owe::ParticleAnimationSpec {});
        subsystem.AddInitializer(owe::ParticleParser::GenInitializer(
            owe::ParseJson(R"({"name":"lifetimerandom","min":10,"max":10})"_str).unwrap(), u32(1)));
        subsystem.AddEmitter(Box<dyn<particle::ParticleEmitterProgram>>::make(
            owe::BoxEmitterProgram(subsystem.SpawnPipeline(),
                                   owe::ParticleBoxEmitterArgs {
                                       .origin        = { 31.0f, 73.0f, 17.0f },
                                       .instantaneous = u32(1),
                                   },
                                   usize())));
        subsystem.AddOperator(owe::ParticleParser::GenOperator(
            owe::ParseJson(config).unwrap(),
            owe::ParticleInstanceModifiers(Arc<owe::wpscene::ParticleInstanceoverride>::make(),
                                           owe::wpscene::Particle::EFlags { 0 },
                                           false),
            subsystem,
            usize()));
        subsystem.Finalize();
        for (int k = 0; k < 6; ++k) subsystem.Tick(f64(1.0 / 60.0), false);
        auto&                        storage = subsystem.System().Instance(usize()).Storage();
        particle::ParticleSlotReader value(storage, particle::ParticleSlot {});
        return { value.Read(subsystem.Attributes().velocity),
                 value.Read(subsystem.Attributes().position) };
    };
    const Eigen::Vector3f origin { 31.0f, 73.0f, 17.0f };
    // the particle is 0.01 through its life: before a window from 0.5, inside one from 0
    auto turbulence = simulate(R"({"name":"turbulence","speedmin":100,"speedmax":100})"_str);
    ASSERT_GT(turbulence[usize()].norm(), 0.01f);
    auto waiting = simulate(
        R"({"name":"turbulence","speedmin":100,"speedmax":100,"blendinstart":0.5,"blendinend":0.75})"_str);
    EXPECT_EQ(waiting[usize()].norm(), 0.0f);
    auto over = simulate(
        R"({"name":"turbulence","speedmin":100,"speedmax":100,"blendoutstart":-1,"blendoutend":0})"_str);
    EXPECT_EQ(over[usize()].norm(), 0.0f);
    auto oscillation = simulate(
        R"({"name":"oscillateposition","frequencymin":1,"frequencymax":1,"scalemin":30,"scalemax":30})"_str);
    ASSERT_GT((oscillation[usize(1)] - origin).norm(), 0.001f);
    auto still = simulate(
        R"({"name":"oscillateposition","frequencymin":1,"frequencymax":1,"scalemin":30,"scalemax":30,"blendinstart":0.5,"blendinend":0.7})"_str);
    EXPECT_LT((still[usize(1)] - origin).norm(), 1e-4f);
}

// Wallpaper Engine adds rate x dt to a float (single precision) count and emits its whole part
// (particles-emission-phase): at 4 a second and 30 frames a second the second particle comes on frame 15, where a
// double-precision sum of the frame times (0.49999...) still waits a frame.
TEST(ParticleSubSystem, EmitsByASinglePrecisionCountOfRateTimesFrameTime) {
    auto emitted_after = [](int frames, bool sphere) {
        owe::Scene             scene;
        auto                   mesh = Arc<owe::SceneMesh>::make();
        owe::ParticleSubSystem subsystem(scene,
                                         mesh.clone(),
                                         u32(64),
                                         f64(1.0),
                                         u32(1),
                                         f64(1.0),
                                         owe::ParticleSubSystem::SpawnType::STATIC,
                                         owe::ParticleAnimationSpec {});
        subsystem.AddInitializer(owe::ParticleParser::GenInitializer(
            owe::ParseJson(R"({"name":"lifetimerandom","min":100,"max":100})"_str).unwrap(), u32(1)));
        if (sphere) {
            subsystem.AddEmitter(Box<dyn<particle::ParticleEmitterProgram>>::make(owe::SphereEmitterProgram(
                subsystem.SpawnPipeline(), owe::ParticleSphereEmitterArgs { .emit_speed = 4.0f }, usize())));
        } else {
            subsystem.AddEmitter(Box<dyn<particle::ParticleEmitterProgram>>::make(owe::BoxEmitterProgram(
                subsystem.SpawnPipeline(), owe::ParticleBoxEmitterArgs { .emit_speed = 4.0f }, usize())));
        }
        subsystem.Finalize();
        for (int frame = 0; frame < frames; ++frame) subsystem.Tick(f64(1.0 / 30.0), false);
        return LiveParticleCount(subsystem);
    };
    for (bool sphere : { false, true }) {
        EXPECT_EQ(emitted_after(7, sphere), usize(0));
        EXPECT_EQ(emitted_after(8, sphere), usize(1));
        EXPECT_EQ(emitted_after(14, sphere), usize(1));
        EXPECT_EQ(emitted_after(15, sphere), usize(2)); // a double sum reaches 2 on frame 16
        EXPECT_EQ(emitted_after(120, sphere), usize(16));
    }
}

// mapsequencebetweencontrolpoints' flag 4 tapers the sequence: the particle at index i of n is
// sin(pi (i + 0.5) / n) of its size (particles-sequence-arc, GPU VM: 8 particles of size 16 are
// 0.16, 0.54, 0.83 and 0.98 of it from the ends in; arcamount and flags 1, 2 and 16 change nothing
// for particles at rest).
TEST(ParticleSubSystem, SequenceFlagFourTapersTheEnds) {
    auto sizes = [](ref<str> sequence) -> Vec<float> {
        owe::Scene             scene;
        auto                   mesh = Arc<owe::SceneMesh>::make();
        owe::ParticleSubSystem subsystem(scene,
                                         mesh.clone(),
                                         u32(8),
                                         f64(1.0),
                                         u32(1),
                                         f64(1.0),
                                         owe::ParticleSubSystem::SpawnType::STATIC,
                                         owe::ParticleAnimationSpec {});
        subsystem.ControlpointsMut()[usize(1)].base_offset = Eigen::Vector3d { 400.0, 0.0, 0.0 };
        subsystem.AddInitializer(owe::ParticleParser::GenInitializer(
            owe::ParseJson(R"({"name":"lifetimerandom","min":10,"max":10})"_str).unwrap(), u32(8)));
        subsystem.AddInitializer(owe::ParticleParser::GenInitializer(
            owe::ParseJson(R"({"name":"sizerandom","min":16,"max":16})"_str).unwrap(), u32(8)));
        subsystem.AddInitializer(
            owe::ParticleParser::GenInitializer(owe::ParseJson(sequence).unwrap(), u32(8)));
        subsystem.AddEmitter(Box<dyn<particle::ParticleEmitterProgram>>::make(
            owe::BoxEmitterProgram(subsystem.SpawnPipeline(),
                                   owe::ParticleBoxEmitterArgs { .instantaneous = u32(8) },
                                   usize())));
        subsystem.Finalize();
        subsystem.Tick(f64(1.0 / 30.0), false);
        auto&      storage = subsystem.System().Instance(usize()).Storage();
        // in the order of their place along the sequence
        float xs[8], size[8];
        for (rstd::uint32_t k = 0; k < 8; ++k) {
            particle::ParticleSlotReader value(storage, particle::ParticleSlot { .index = usize(k) });
            xs[k]   = value.Read(subsystem.Attributes().position).x();
            size[k] = value.Read(subsystem.Attributes().size);
        }
        for (int a = 1; a < 8; ++a)
            for (int b = a; b > 0 && xs[b - 1] > xs[b]; --b) {
                float t = xs[b]; xs[b] = xs[b - 1]; xs[b - 1] = t;
                t = size[b]; size[b] = size[b - 1]; size[b - 1] = t;
            }
        Vec<float> out;
        for (int k = 0; k < 8; ++k) out.push(float(size[k]));
        return out;
    };
    auto plain = sizes(R"({"name":"mapsequencebetweencontrolpoints","count":8})"_str);
    ASSERT_EQ(plain.len(), usize(8));
    for (float size : plain) EXPECT_FLOAT_EQ(size, 16.0f);
    auto tapered =
        sizes(R"({"name":"mapsequencebetweencontrolpoints","count":8,"flags":4,"arcamount":0.1})"_str);
    ASSERT_EQ(tapered.len(), usize(8));
    for (usize i {}; i < usize(8); ++i) {
        const double expected =
            16.0 * std::sin(3.14159265358979323846 * (static_cast<double>(i.to_primitive()) + 0.5) / 8.0);
        EXPECT_NEAR(tapered[i], expected, 1e-3);
    }
    // thunderbolt's flags 23 include 4
    auto bolt = sizes(R"({"name":"mapsequencebetweencontrolpoints","count":8,"flags":23})"_str);
    EXPECT_NEAR(bolt[usize()], 16.0 * std::sin(3.14159265358979323846 * 0.5 / 8.0), 1e-3);
}

// A new on phase of a periodic emitter fires its system's static children's instantaneous
// emitters again (particles-child-static, GPU VM: a 0.3 s halo in a static child flashes every
// 2.06 s under a parent emitting 1 s on, 1 s off, and never again under a plain parent or one
// with no particles); thunderbolt's glow flashes with each bolt.
TEST(ParticleSubSystem, PeriodicEmissionRestartsStaticChildren) {
    owe::Scene             scene;
    owe::ParticleSubSystem parent(scene,
                                  Arc<owe::SceneMesh>::make(),
                                  u32(8),
                                  f64(1.0),
                                  u32(1),
                                  f64(1.0),
                                  owe::ParticleSubSystem::SpawnType::STATIC,
                                  owe::ParticleAnimationSpec {});
    parent.AddInitializer(owe::ParticleParser::GenInitializer(
        owe::ParseJson(R"({"name":"lifetimerandom","min":0.5,"max":0.5})"_str).unwrap(), u32(8)));
    owe::wpscene::Emitter periodic;
    EXPECT_TRUE(periodic.FromJson(
        owe::ParseJson(
            R"({"name":"boxrandom","rate":5,"flags":4,"minperiodicdelay":1,"maxperiodicdelay":1,"minperiodicduration":1,"maxperiodicduration":1})"_str)
            .unwrap()));
    parent.AddEmitter(Box<dyn<particle::ParticleEmitterProgram>>::make(
        owe::BoxEmitterProgram(parent.SpawnPipeline(),
                               owe::ParticleBoxEmitterArgs {
                                   .emit_speed = 5.0f,
                                   .timing     = owe::ParticleEmitterTiming::From(periodic),
                               },
                               usize())));

    auto child = Box<owe::ParticleSubSystem>::make(scene,
                                                   Arc<owe::SceneMesh>::make(),
                                                   u32(4),
                                                   f64(1.0),
                                                   u32(1),
                                                   f64(1.0),
                                                   owe::ParticleSubSystem::SpawnType::STATIC,
                                                   owe::ParticleAnimationSpec {});
    child->AddInitializer(owe::ParticleParser::GenInitializer(
        owe::ParseJson(R"({"name":"lifetimerandom","min":0.3,"max":0.3})"_str).unwrap(), u32(4)));
    child->AddEmitter(Box<dyn<particle::ParticleEmitterProgram>>::make(
        owe::BoxEmitterProgram(child->SpawnPipeline(),
                               owe::ParticleBoxEmitterArgs { .instantaneous = u32(1) },
                               usize())));
    child->Finalize();
    auto* glow = child.get();
    parent.AddChild(rstd::move(child));
    parent.Finalize();

    // whether the child has a live particle after each of 200 frames at 30 a second
    Vec<bool> lit;
    for (int k = 0; k < 200; ++k) {
        parent.Tick(f64(1.0 / 30.0), false);
        lit.push(glow->System().InstanceCount() > usize() && ! glow->InstanceState(usize()).no_live_particle);
    }
    EXPECT_TRUE(lit[usize(2)]);
    // dead by 0.5 s, and nothing fires it until the next on phase (at about 61 frames)
    for (int k = 20; k < 58; ++k) EXPECT_FALSE(lit[usize(k)]) << k;
    bool again = false;
    for (int k = 60; k < 72; ++k) again = again || lit[usize(k)];
    EXPECT_TRUE(again);
    for (int k = 85; k < 118; ++k) EXPECT_FALSE(lit[usize(k)]) << k;
    bool third = false;
    for (int k = 121; k < 134; ++k) third = third || lit[usize(k)];
    EXPECT_TRUE(third);
}

// Wallpaper Engine's turbulence samples Gustavson's 3D simplex noise (F3 1/3, G3 1/6, falloff
// 0.6 - r^2, times 32), with Ken Perlin's permutation and the 12 edge gradients, as read from
// wallpaper64.exe (the noise object's tables at 0x1404833a0, 0x140483140/170, 0x1404835a0). The
// expected values come from an independent implementation of the same algorithm.
TEST(ParticleNoise, SimplexIsWallpaperEnginesNoise) {
    EXPECT_NEAR(owe::algorism::SimplexNoise3(0.1, 0.2, 0.3), 0.635890368, 2e-5);
    EXPECT_NEAR(owe::algorism::SimplexNoise3(1.5, -2.25, 0.75), 0.312508333, 2e-5);
    EXPECT_NEAR(owe::algorism::SimplexNoise3(3.3, 4.4, -5.5), 0.658686337, 2e-5);
    EXPECT_NEAR(owe::algorism::SimplexNoise3(-0.6, 0.0, 12.1), -0.845244011, 2e-5);
    EXPECT_NEAR(owe::algorism::SimplexNoise3(31.0, 0.5, 0.25), 0.158448871, 2e-5);
}

// turbulence (operator case 14 of FUN_14023fbc0): the velocity gains n * mask * speed * dt2, n the
// noise at (position + phase) * scale for x, and at the same point with its coordinates turned
// (z x y) and (y z x) for y and z; not normalized. speed = speedmin + r (speedmax - speedmin) and
// phase = r (phasemax - phasemin) + time * timescale, r a random number per particle.
TEST(ParticleSubSystem, TurbulenceFollowsSimplexNoise) {
    auto simulate = [](ref<str> config) -> Eigen::Vector3f {
        owe::Scene             scene;
        auto                   mesh = Arc<owe::SceneMesh>::make();
        owe::ParticleSubSystem subsystem(scene,
                                         mesh.clone(),
                                         u32(1),
                                         f64(1.0),
                                         u32(1),
                                         f64(1.0),
                                         owe::ParticleSubSystem::SpawnType::STATIC,
                                         owe::ParticleAnimationSpec {});
        subsystem.AddInitializer(owe::ParticleParser::GenInitializer(
            owe::ParseJson(R"({"name":"lifetimerandom","min":10,"max":10})"_str).unwrap(), u32(1)));
        subsystem.AddEmitter(Box<dyn<particle::ParticleEmitterProgram>>::make(
            owe::BoxEmitterProgram(subsystem.SpawnPipeline(),
                                   owe::ParticleBoxEmitterArgs {
                                       .origin        = { 31.0f, 73.0f, 17.0f },
                                       .instantaneous = u32(1),
                                   },
                                   usize())));
        subsystem.AddOperator(owe::ParticleParser::GenOperator(
            owe::ParseJson(config).unwrap(),
            owe::ParticleInstanceModifiers(Arc<owe::wpscene::ParticleInstanceoverride>::make(),
                                           owe::wpscene::Particle::EFlags { 0 },
                                           false),
            subsystem,
            usize()));
        subsystem.Finalize();
        subsystem.Tick(f64(1.0 / 60.0), false);
        auto&                        storage = subsystem.System().Instance(usize()).Storage();
        particle::ParticleSlotReader value(storage, particle::ParticleSlot {});
        return value.Read(subsystem.Attributes().velocity);
    };
    // the noise at the particle, (31 73 17) * 0.01, and turned
    const Eigen::Vector3d n(owe::algorism::SimplexNoise3(0.31, 0.73, 0.17),
                            owe::algorism::SimplexNoise3(0.17, 0.31, 0.73),
                            owe::algorism::SimplexNoise3(0.73, 0.17, 0.31));
    auto v = simulate(
        R"({"name":"turbulence","mask":"1 1 1","speedmin":100,"speedmax":100,"scale":0.01,"timescale":0})"_str);
    ASSERT_GT(v.norm(), 1e-6f);
    EXPECT_TRUE(v.cast<double>().normalized().isApprox(n.normalized(), 1e-3));
    // linear in speed, and not a unit vector times the speed
    auto fast = simulate(
        R"({"name":"turbulence","mask":"1 1 1","speedmin":300,"speedmax":300,"scale":0.01,"timescale":0})"_str);
    EXPECT_NEAR(fast.norm() / v.norm(), 3.0f, 1e-3f);
    auto x_only = simulate(
        R"({"name":"turbulence","mask":"1 0 0","speedmin":100,"speedmax":100,"scale":0.01,"timescale":0})"_str);
    EXPECT_FLOAT_EQ(x_only.y(), 0.0f);
    EXPECT_NEAR(x_only.x(), v.x(), 1e-5f);
    EXPECT_NEAR(v.norm() / v.x(), static_cast<float>(n.norm() / n.x()), 1e-3f);
}

// The emitter's own instantaneous count fires again at each new on phase too: wallpaper64.exe's
// emitter pass (FUN_1402378a0, 0x140237a8f..) copies the instantaneous count back into the pending
// count ([emitter+0x44] = [emitter+0x48]) when a periodic delay ends.
TEST(ParticleSubSystem, PeriodicEmissionRefiresItsOwnInstantaneousCount) {
    owe::Scene             scene;
    owe::ParticleSubSystem system(scene,
                                  Arc<owe::SceneMesh>::make(),
                                  u32(8),
                                  f64(1.0),
                                  u32(1),
                                  f64(1.0),
                                  owe::ParticleSubSystem::SpawnType::STATIC,
                                  owe::ParticleAnimationSpec {});
    system.AddInitializer(owe::ParticleParser::GenInitializer(
        owe::ParseJson(R"({"name":"lifetimerandom","min":0.3,"max":0.3})"_str).unwrap(), u32(8)));
    owe::wpscene::Emitter periodic;
    EXPECT_TRUE(periodic.FromJson(
        owe::ParseJson(
            R"({"name":"boxrandom","rate":0,"instantaneous":2,"flags":4,"minperiodicdelay":1,"maxperiodicdelay":1,"minperiodicduration":1,"maxperiodicduration":1})"_str)
            .unwrap()));
    system.AddEmitter(Box<dyn<particle::ParticleEmitterProgram>>::make(
        owe::BoxEmitterProgram(system.SpawnPipeline(),
                               owe::ParticleBoxEmitterArgs {
                                   .instantaneous = u32(2),
                                   .timing        = owe::ParticleEmitterTiming::From(periodic),
                               },
                               usize())));
    system.Finalize();
    Vec<bool> lit;
    for (int k = 0; k < 140; ++k) {
        system.Tick(f64(1.0 / 30.0), false);
        lit.push(! system.InstanceState(usize()).no_live_particle);
    }
    EXPECT_TRUE(lit[usize(2)]);
    for (int k = 20; k < 58; ++k) EXPECT_FALSE(lit[usize(k)]) << k;
    bool again = false;
    for (int k = 60; k < 72; ++k) again = again || lit[usize(k)];
    EXPECT_TRUE(again);
}

// capvelocity follows its blend window like the other operators (particles-operator-blend: capped at
// 50 and blended in over 0.25..0.5 of a 4 s life, a stream keeps its 200 units/s for its first second
// and slows after it): the velocity is moved that much of the way towards the capped one.
TEST(ParticleSubSystem, CapVelocityWaitsForItsBlendWindow) {
    auto speed_at = [](ref<str> cap, int frames) -> float {
        owe::Scene             scene;
        owe::ParticleSubSystem subsystem(scene,
                                         Arc<owe::SceneMesh>::make(),
                                         u32(1),
                                         f64(1.0),
                                         u32(1),
                                         f64(1.0),
                                         owe::ParticleSubSystem::SpawnType::STATIC,
                                         owe::ParticleAnimationSpec {});
        auto modifiers = owe::ParticleInstanceModifiers(
            Arc<owe::wpscene::ParticleInstanceoverride>::make(),
            owe::wpscene::Particle::EFlags { 0 },
            false);
        subsystem.AddInitializer(owe::ParticleParser::GenInitializer(
            owe::ParseJson(R"({"name":"lifetimerandom","min":4,"max":4})"_str).unwrap(), u32(1)));
        subsystem.AddInitializer(owe::ParticleParser::GenInitializer(
            owe::ParseJson(R"({"name":"velocityrandom","min":"200 0 0","max":"200 0 0"})"_str)
                .unwrap(),
            u32(2)));
        subsystem.AddEmitter(Box<dyn<particle::ParticleEmitterProgram>>::make(
            owe::BoxEmitterProgram(subsystem.SpawnPipeline(),
                                   owe::ParticleBoxEmitterArgs { .instantaneous = u32(1) },
                                   usize())));
        subsystem.AddOperator(owe::ParticleParser::GenOperator(
            owe::ParseJson(cap).unwrap(), modifiers.Clone(), subsystem, usize()));
        subsystem.Finalize();
        for (int frame = 0; frame < frames; ++frame) subsystem.Tick(f64(1.0 / 30.0), false);
        auto& storage = subsystem.System().Instance(usize()).Storage();
        particle::ParticleSlotReader value(storage, particle::ParticleSlot {});
        return value.Read(subsystem.Attributes().velocity).norm();
    };
    const auto window = R"({"name":"capvelocity","maxspeed":100,"blendinstart":0.25,"blendinend":0.5})"_str;
    EXPECT_NEAR(speed_at(window, 27), 200.0f, 1e-3f);   // 0.9 s: before the window
    EXPECT_NEAR(speed_at(window, 66), 100.0f, 1e-3f);   // 2.2 s: after it
    const float halfway = speed_at(window, 45);          // 1.5 s: on the way
    EXPECT_LT(halfway, 200.0f);
    EXPECT_GT(halfway, 100.0f);
    // thunderbolt's spawner: a step at 0.2 of the life
    const auto step = R"({"name":"capvelocity","maxspeed":100,"blendinstart":0.2,"blendinend":0.2})"_str;
    EXPECT_NEAR(speed_at(step, 20), 200.0f, 1e-3f);
    EXPECT_NEAR(speed_at(step, 30), 100.0f, 1e-3f);
    // no window: capped from the start
    EXPECT_NEAR(speed_at(R"({"name":"capvelocity","maxspeed":100})"_str, 3), 100.0f, 1e-3f);
}

// movement, as Wallpaper Engine runs it (FUN_14023fbc0 case 1): gravity is added (v += g dt), the particle moves
// (p += v dt), then drag takes min(drag dt2, 1) of the speed, dt2 = dt x min(0.025 / frame time, 1)^0.7.
// particles-drag-rest: a thrown particle stops at 0.503, 0.818 and 1.0 x (speed / drag) at 15, 30 and 60 fps.
static Eigen::Vector3f MovedVelocity(ref<str> movement, ref<str> velocity, int ticks, double tick,
                                     Eigen::Vector3f* place = nullptr) {
    owe::Scene             scene;
    auto                   mesh = Arc<owe::SceneMesh>::make();
    owe::ParticleSubSystem subsystem(scene,
                                     mesh.clone(),
                                     u32(1),
                                     f64(1.0),
                                     u32(1),
                                     f64(1.0),
                                     owe::ParticleSubSystem::SpawnType::STATIC,
                                     owe::ParticleAnimationSpec {});
    subsystem.AddInitializer(owe::ParticleParser::GenInitializer(
        owe::ParseJson(R"({"name":"lifetimerandom","min":100,"max":100})"_str).unwrap(), u32(1)));
    subsystem.AddInitializer(owe::ParticleParser::GenInitializer(owe::ParseJson(velocity).unwrap(), u32(2)));
    subsystem.AddEmitter(Box<dyn<particle::ParticleEmitterProgram>>::make(owe::BoxEmitterProgram(
        subsystem.SpawnPipeline(), owe::ParticleBoxEmitterArgs { .instantaneous = u32(1) }, usize())));
    subsystem.AddOperator(owe::ParticleParser::GenOperator(
        owe::ParseJson(movement).unwrap(),
        owe::ParticleInstanceModifiers(Arc<owe::wpscene::ParticleInstanceoverride>::make(),
                                       owe::wpscene::Particle::EFlags { 0 },
                                       false),
        subsystem,
        usize()));
    subsystem.Finalize();
    for (int count = 0; count < ticks; ++count) subsystem.Tick(f64(tick), false);
    auto&                        storage = subsystem.System().Instance(usize()).Storage();
    particle::ParticleSlotReader value(storage, particle::ParticleSlot {});
    if (place) *place = value.Read(subsystem.Attributes().position);
    return value.Read(subsystem.Attributes().velocity);
}

TEST(ParticleSubSystem, MovementDragTakesDragTimesTheCompensatedStep) {
    auto thrown = R"({"name":"velocityrandom","min":"300 0 0","max":"300 0 0"})"_str;
    // 30 fps: 1 - 2 x (1/30 x 0.81766) a frame
    EXPECT_NEAR(MovedVelocity(R"({"name":"movement","drag":2})"_str, thrown, 1, 1.0 / 30.0).x(),
                300.0 * (1.0 - 2.0 * (1.0 / 30.0) * 0.81766), 0.02);
    // 60 fps: the whole step, 1 - 2/60 a frame
    EXPECT_NEAR(MovedVelocity(R"({"name":"movement","drag":2})"_str, thrown, 2, 1.0 / 60.0).x(),
                300.0 * (1.0 - 2.0 / 60.0) * (1.0 - 2.0 / 60.0), 0.02);
}

TEST(ParticleSubSystem, MovementMovesBeforeTheDragAndAddsGravityFirst) {
    Eigen::Vector3f place;
    auto            thrown = R"({"name":"velocityrandom","min":"300 0 0","max":"300 0 0"})"_str;
    MovedVelocity(R"({"name":"movement","drag":2,"gravity":"0 -600 0"})"_str, thrown, 1, 1.0 / 60.0, &place);
    EXPECT_NEAR(place.x(), 300.0 / 60.0, 1e-3);          // moved at the speed before the drag
    EXPECT_NEAR(place.y(), -600.0 / 60.0 / 60.0, 1e-3);  // and with this frame's gravity already added
}

// Wallpaper Engine compensates each frame by its own time, not the target frame rate's (particles-frametime-f15/-f30/
// -f60: the stops of thrown particles follow the frame times its script recorded in the same run, at every rate).
// A 33 ms frame at a 60 fps target is shortened like a 30 fps frame; the two passes still follow the target rate.
TEST(ParticleSubSystem, CompensatedStepUsesTheFramesOwnTime) {
    auto slow = owe::ParticleOperatorStepsFor(u32(60), f64(1.0 / 30.0));
    EXPECT_NEAR(slow.delta2.unwrap().to_primitive(), (1.0 / 30.0) * 0.81766, 1e-5);
    EXPECT_TRUE(slow.passes == u32(1));
    auto quick = owe::ParticleOperatorStepsFor(u32(15), f64(1.0 / 60.0));
    EXPECT_NEAR(quick.delta2.unwrap().to_primitive(), 1.0 / 60.0, 1e-6);
    EXPECT_TRUE(quick.passes == u32(2));
}

// Wallpaper Engine's frame times in the particles-frametime-f60 and -f15 references (ms, printed by their script) and
// where its thrown particles stopped in the same takes, x speed / drag, for drag 0.5, 1 and 2. owe given the same
// frames stops them there too.
static constexpr double kWeFrames60[] = {
    0.1, 57.2, 16.4, 17.0, 16.0, 16.6, 16.4, 16.0, 16.0, 16.0, 16.0, 16.0,
    16.0, 16.0, 16.6, 16.4, 16.0, 16.1, 15.9, 16.0, 16.0, 16.0, 16.0, 18.3,
    16.3, 16.3, 16.0, 20.5, 16.5, 16.0, 16.0, 16.0, 16.0, 16.0, 16.0, 16.0,
    16.0, 16.0, 16.0, 16.0, 19.1, 16.8, 16.0, 16.0, 15.0, 26.9, 15.1, 15.4,
    29.6, 34.6, 27.9, 29.6, 14.5, 17.6, 29.0, 18.6, 16.5, 16.3, 16.0, 15.8,
    16.3, 16.0, 17.0, 17.0, 17.0, 17.0, 17.2, 16.8, 17.0, 17.0, 17.0, 17.0,
    17.0, 17.0, 18.2, 16.8, 17.0, 17.0, 17.0, 17.0, 17.0, 17.0, 17.0, 17.9,
    17.9, 17.2, 17.0, 17.0, 17.0, 16.0, 17.0, 21.0, 15.2, 16.7, 17.0, 16.5,
    17.5, 17.0, 20.2, 16.8, 16.0, 17.0, 17.0, 16.0, 16.0, 17.0, 18.8, 16.2,
    16.0, 17.0, 16.0, 17.0, 13.0, 29.4, 30.1, 32.7, 29.3, 27.8, 16.7, 16.0,
    16.0, 16.0, 16.0, 16.0, 16.0, 16.0, 16.0, 16.0, 16.5, 16.0, 17.0, 17.5,
    16.5, 16.0, 16.0, 16.0, 16.0, 16.0, 16.1, 16.0, 16.0, 18.7, 16.3, 16.6,
    16.4, 16.0, 16.0, 16.0, 26.9, 16.5, 17.8, 15.8, 16.0, 15.0, 15.0, 15.0,
    16.0, 15.0, 16.0, 16.0, 16.0, 16.6, 15.4, 16.0, 15.0, 15.0, 16.0, 15.0,
    16.0, 14.5, 30.0, 33.4, 30.3, 32.9, 28.9, 31.7, 32.5, 27.7, 15.5, 15.9,
    16.7, 15.8, 32.9, 30.2, 29.8, 33.4, 14.5, 14.0, 15.0, 14.0, 15.0, 14.0,
    15.0, 15.9, 14.1, 14.0, 14.0, 14.0, 14.0, 14.0, 14.0, 14.0, 14.0, 14.0,
    14.0, 14.0, 14.0, 14.0, 14.0, 15.0, 14.0, 15.0, 17.9, 14.1, 14.0, 14.0,
    14.0, 14.0, 14.0, 14.0, 14.0, 18.6, 13.4, 20.6, 24.5, 35.8, 13.5, 5.7,
    5.7, 5.3, 5.0, 6.0, 22.5, 32.0, 31.3, 16.2, 16.0, 16.0, 16.0, 16.0,
    16.0, 17.0, 16.0, 16.0, 16.0, 17.3, 16.7, 18.0, 16.0, 18.5, 16.5, 17.0,
    16.0, 17.0, 16.0, 16.0, 16.0, 17.0, 16.0, 16.0, 16.0, 17.0, 16.0, 16.0,
    17.0, 16.9, 16.1, 16.0, 17.0, 16.0, 17.0, 17.0, 17.0, 17.0, 17.0, 16.0,
    16.0, 16.0, 16.0, 17.0, 16.0, 16.0, 16.0, 17.0, 17.0, 17.0, 16.0, 15.0,
    16.0, 20.1, 12.9, 15.0, 16.0, 16.0, 17.0, 17.0, 17.0, 16.0, 17.0, 17.0,
    16.0, 17.0, 16.0, 16.0, 16.0, 16.0, 17.0, 17.0, 17.0, 17.0, 17.0, 17.0,
    16.0, 16.0, 17.0, 16.0, 16.0, 17.0, 17.0, 17.0, 17.0, 17.9, 16.1, 16.0,
    17.0, 17.0, 17.0, 18.4, 16.6, 17.0, 17.0, 17.0, 17.0, 17.0, 17.0, 17.0,
    16.0, 17.0, 17.0, 16.0, 16.0, 17.0, 17.8, 17.3, 17.0, 16.0, 16.0, 16.0,
    16.0, 17.0, 15.0, 17.0, 17.0, 16.0, 18.8, 17.2, 17.0, 17.0, 17.0, 17.0,
    17.0, 16.0, 17.0, 17.0, 16.0, 17.0, 17.0, 17.5, 16.5, 17.0, 17.0, 17.0,
    16.0, 16.0, 16.0, 16.0, 16.0, 16.0, 16.0, 16.0, 16.0, 16.0, 17.0, 17.0,
    16.0, 17.0, 17.0, 18.6, 16.4, 17.0, 16.0, 17.0, 17.0, 17.0, 16.0, 16.0,
    17.0, 17.0, 19.7, 16.3,
};
static constexpr double kWeFrames15[] = {
    0.1, 72.2, 70.8, 71.0, 71.0, 71.0, 71.0, 72.0, 71.0, 72.0, 71.0, 71.0,
    72.0, 72.0, 71.0, 66.0, 67.0, 67.0, 67.0, 66.0, 67.0, 67.0, 66.0, 67.0,
    67.0, 67.0, 66.0, 67.0, 67.0, 66.0, 67.0, 67.0, 66.0, 67.0, 66.0, 67.0,
    67.0, 67.0, 67.0, 66.0, 66.0, 67.0, 67.0, 66.0, 67.0, 66.0, 67.0, 66.0,
    67.0, 67.0, 67.0, 67.0, 67.0, 66.0, 67.0, 67.0, 66.0, 66.0, 67.0, 67.0,
    67.0, 66.0, 67.0, 67.0, 66.0, 67.0, 67.0, 67.0, 66.0, 67.0, 67.0, 66.0,
    67.0, 67.0, 66.0, 66.0, 67.0, 67.0, 66.0, 67.0, 67.0, 67.0, 67.0, 66.0,
    67.0, 65.7, 67.0, 66.0, 67.0, 67.0, 67.0, 66.0, 66.0, 67.0, 67.0, 66.0,
    67.0, 67.0, 67.0, 66.0, 67.0, 66.0, 67.0, 67.0, 67.0, 67.0, 66.0, 67.0,
    66.0, 71.6, 66.4, 66.0, 66.0, 66.0, 66.0, 66.0, 66.0, 68.4, 65.6, 65.0,
    66.0, 67.0, 67.2, 66.9, 66.0, 68.2, 65.8, 66.0, 67.0, 66.0, 67.0, 67.0,
    67.0, 66.0, 67.0, 66.0, 67.0, 66.0, 67.0, 66.0, 67.0, 66.0, 67.0, 67.0,
    67.0, 66.0, 67.0, 68.7, 66.3, 66.0, 67.0, 66.0, 68.0, 66.0, 67.0, 66.0,
    66.0, 67.0, 67.0, 66.8, 66.2, 66.0, 67.0, 67.0, 67.0, 66.7, 66.3, 66.0,
    66.0, 67.0, 68.2, 67.1, 66.7, 66.0, 66.0, 67.0, 67.0, 66.0, 67.0, 66.0,
    66.4, 66.6, 66.0, 67.0, 67.3, 66.7, 67.2, 67.7, 66.2, 67.2, 66.8, 67.1,
    67.4, 65.6, 65.1, 66.9, 67.0, 66.1, 66.0, 66.0, 67.0, 68.0, 67.6, 66.5,
    67.8, 67.3, 66.3, 68.3, 66.3, 63.0, 66.0, 66.0, 67.0, 66.0, 66.0, 66.0,
    67.9, 67.1, 67.0, 66.0, 67.0, 66.0, 67.0, 68.0, 66.0, 67.0, 66.0, 67.0,
    67.0, 67.0, 67.0, 66.0,
};

static double ThrownStop(double drag, u32 target_fps, const double* frames, int count, double runtime) {
    owe::Scene scene;
    scene.SetTargetFps(target_fps);
    auto                   mesh = Arc<owe::SceneMesh>::make();
    owe::ParticleSubSystem subsystem(scene,
                                     mesh.clone(),
                                     u32(1),
                                     f64(1.0),
                                     u32(1),
                                     f64(1.0),
                                     owe::ParticleSubSystem::SpawnType::STATIC,
                                     owe::ParticleAnimationSpec {});
    subsystem.AddInitializer(owe::ParticleParser::GenInitializer(
        owe::ParseJson(R"({"name":"lifetimerandom","min":100,"max":100})"_str).unwrap(), u32(1)));
    subsystem.AddInitializer(owe::ParticleParser::GenInitializer(
        owe::ParseJson(R"({"name":"velocityrandom","min":"100 0 0","max":"100 0 0"})"_str).unwrap(), u32(2)));
    subsystem.AddEmitter(Box<dyn<particle::ParticleEmitterProgram>>::make(owe::BoxEmitterProgram(
        subsystem.SpawnPipeline(), owe::ParticleBoxEmitterArgs { .instantaneous = u32(1) }, usize())));
    auto movement = drag == 0.5   ? R"({"name":"movement","drag":0.5})"_str
                    : drag == 1.0 ? R"({"name":"movement","drag":1})"_str
                                  : R"({"name":"movement","drag":2})"_str;
    subsystem.AddOperator(owe::ParticleParser::GenOperator(
        owe::ParseJson(movement).unwrap(),
        owe::ParticleInstanceModifiers(Arc<owe::wpscene::ParticleInstanceoverride>::make(),
                                       owe::wpscene::Particle::EFlags { 0 },
                                       false),
        subsystem,
        usize()));
    subsystem.Finalize();
    double time {};
    for (int index = 0; index < count; ++index) {
        subsystem.Tick(f64(frames[index] / 1000.0), false);
        time += frames[index] / 1000.0;
    }
    const double rest = 1.0 / static_cast<double>(target_fps.to_primitive());
    for (; time < runtime; time += rest) subsystem.Tick(f64(rest), false);
    auto&                        storage = subsystem.System().Instance(usize()).Storage();
    particle::ParticleSlotReader value(storage, particle::ParticleSlot {});
    return value.Read(subsystem.Attributes().position).x() / (100.0 / drag);
}

TEST(ParticleSubSystem, ThrownParticlesStopWhereWallpaperEngineStoppedThemGivenItsFrames) {
    EXPECT_NEAR(ThrownStop(0.5, u32(60), kWeFrames60, int(sizeof(kWeFrames60) / sizeof(double)), 15.417), 1.0291, 0.002);
    EXPECT_NEAR(ThrownStop(1.0, u32(60), kWeFrames60, int(sizeof(kWeFrames60) / sizeof(double)), 15.417), 1.0392, 0.002);
    EXPECT_NEAR(ThrownStop(2.0, u32(60), kWeFrames60, int(sizeof(kWeFrames60) / sizeof(double)), 15.417), 1.0584, 0.002);
    EXPECT_NEAR(ThrownStop(0.5, u32(15), kWeFrames15, int(sizeof(kWeFrames15) / sizeof(double)), 15.463), 1.9674, 0.003);
    EXPECT_NEAR(ThrownStop(1.0, u32(15), kWeFrames15, int(sizeof(kWeFrames15) / sizeof(double)), 15.463), 2.0235, 0.003);
    EXPECT_NEAR(ThrownStop(2.0, u32(15), kWeFrames15, int(sizeof(kWeFrames15) / sizeof(double)), 15.463), 2.0469, 0.003);
}

// A burst is born in Wallpaper Engine's first frame, which passes almost no time (0.1 ms in particles-frametime-*), and the
// operators run on it there: a capped particle is slowed before it ever moves (particles-capvelocity-single), and a thrown
// one's extra travel in the drag references comes from the long frames after it, not from its birth.
TEST(ParticleSubSystem, ABurstIsCappedBeforeItsFirstMove) {
    owe::Scene             scene;
    auto                   mesh = Arc<owe::SceneMesh>::make();
    owe::ParticleSubSystem subsystem(scene,
                                     mesh.clone(),
                                     u32(1),
                                     f64(1.0),
                                     u32(1),
                                     f64(1.0),
                                     owe::ParticleSubSystem::SpawnType::STATIC,
                                     owe::ParticleAnimationSpec {});
    subsystem.AddInitializer(owe::ParticleParser::GenInitializer(
        owe::ParseJson(R"({"name":"lifetimerandom","min":100,"max":100})"_str).unwrap(), u32(1)));
    subsystem.AddInitializer(owe::ParticleParser::GenInitializer(
        owe::ParseJson(R"({"name":"velocityrandom","min":"400 0 0","max":"400 0 0"})"_str).unwrap(), u32(2)));
    subsystem.AddEmitter(Box<dyn<particle::ParticleEmitterProgram>>::make(owe::BoxEmitterProgram(
        subsystem.SpawnPipeline(), owe::ParticleBoxEmitterArgs { .instantaneous = u32(1) }, usize())));
    auto modifiers = [] {
        return owe::ParticleInstanceModifiers(Arc<owe::wpscene::ParticleInstanceoverride>::make(),
                                              owe::wpscene::Particle::EFlags { 0 },
                                              false);
    };
    subsystem.AddOperator(owe::ParticleParser::GenOperator(
        owe::ParseJson(R"({"name":"movement","drag":0})"_str).unwrap(), modifiers(), subsystem, usize()));
    subsystem.AddOperator(owe::ParticleParser::GenOperator(
        owe::ParseJson(R"({"name":"capvelocity","maxspeed":40})"_str).unwrap(), modifiers(), subsystem, usize(1)));
    subsystem.Finalize();
    subsystem.Tick(f64(), false);
    for (int frame = 0; frame < 3; ++frame) subsystem.Tick(f64(1.0 / 30.0), false);
    auto&                        storage = subsystem.System().Instance(usize()).Storage();
    particle::ParticleSlotReader value(storage, particle::ParticleSlot {});
    EXPECT_NEAR(value.Read(subsystem.Attributes().position).x(), 3.0f * 40.0f / 30.0f, 1e-3f);
}

// A plain rope is drawn in the thick vertex format, like a sprite trail: each segment carries the
// colour of both its particles, and the rope shader blends between them (THICKFORMAT). Only a rope
// trail uses the thin format. particles-rope-color: Wallpaper Engine shades a rope of six particles
// coloured red to blue (or opacity 1 to 0.1) as one smooth gradient, owe drew five flat blocks;
// wallpaper64.exe's rope writer (FUN_1402308a0) fills the 96-byte thick vertex, end colour at +0x40.
TEST(ParticleRender, RopesUseTheThickFormat) {
    EXPECT_TRUE(owe::ParticleRendererThickFormat("rope"_str, false));
    EXPECT_TRUE(owe::ParticleRendererThickFormat("spritetrail"_str, false));
    EXPECT_FALSE(owe::ParticleRendererThickFormat("ropetrail"_str, false));
    EXPECT_FALSE(owe::ParticleRendererThickFormat("sprite"_str, false));
    EXPECT_TRUE(owe::ParticleRendererThickFormat("sprite"_str, true));
}
