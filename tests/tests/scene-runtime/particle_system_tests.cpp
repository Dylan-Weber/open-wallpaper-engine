#include <rstd/test/gtest.hpp>

import rstd;
import rstd.cppstd;
import eigen;
import wescene.json;
import wescene.particle;
import wescene.particle.program;
import wescene.pkg.parse;
import wescene.scene;
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

// Wallpaper Engine at 20 frames a second or less (FUN_140236cd0): ages, emitters and initializers once with the whole
// step, then the operator pass twice with half dt and half dt2; post-updates (trails) still once a frame.
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
    rstd::vec::Vec<double>   deltas;
    rstd::vec::Vec<double>   deltas2;
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

TEST(ParticleRuntime, RemovesOnlyTheRequestedOwner) {
    owe::Scene           scene;
    owe::ParticleRuntime runtime;
    auto                 first       = Arc<owe::SceneNode>::make();
    auto                 second      = Arc<owe::SceneNode>::make();
    auto                 first_mesh  = rstd::sync::Weak<owe::SceneMesh>::make();
    auto                 second_mesh = rstd::sync::Weak<owe::SceneMesh>::make();
    auto add = [&](const Arc<owe::SceneNode>& owner, rstd::sync::Weak<owe::SceneMesh>& weak) {
        auto mesh = Arc<owe::SceneMesh>::make();
        weak      = mesh.downgrade();
        auto subsystem =
            Box<owe::ParticleSubSystem>::make(scene,
                                              mesh.clone(),
                                              u32(4),
                                              f64(1.0),
                                              u32(1),
                                              f64(1.0),
                                              owe::ParticleSubSystem::SpawnType::STATIC,
                                              owe::ParticleAnimationSpec {});
        subsystem->SetOwnerNode(owner.as_ptr());
        runtime.Add(rstd::move(subsystem));
    };
    add(first, first_mesh);
    add(second, second_mesh);
    runtime.RemoveOwner(*first);
    EXPECT_FALSE(first_mesh.upgrade());
    EXPECT_TRUE(second_mesh.upgrade());
    runtime.RemoveOwner(*first);
    runtime.RemoveOwner(*second);
    EXPECT_FALSE(second_mesh.upgrade());
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
    EXPECT_TRUE(resolved[usize(2)].offset.isApprox(Eigen::Vector3d { 3.0, 4.0, 0.0 }));
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

// Instantaneous emitters fire when the system has no particles; every one of them does, not only the first to run
// (particles-instantaneous-emitters: Wallpaper Engine shows a particle from each of several such emitters).
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

// boids (particles-boids): particles at rest with only the boids operator; returns each particle's velocity, ordered by x.
// Wallpaper Engine runs the operator on groups of four particles, and the unused places of the last group count as
// particles at rest at the system's origin: a pair at -d/2 and d/2 has two such neighbours half way between them.
struct BoidsResult {
    int             count {};
    Eigen::Vector3f velocity[4];
};

static BoidsResult BoidsVelocities(ref<str> config, std::initializer_list<float> xs, int ticks = 1,
                                   double tick = 1.0 / 30.0,
                                   ref<str> velocity = R"({"name":"velocityrandom","min":"0 0 0","max":"0 0 0"})"_str,
                                   u32 target_fps = u32()) {
    owe::Scene             scene;
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
    subsystem.AddInitializer(owe::ParticleParser::GenInitializer(owe::ParseJson(velocity).unwrap(), u32(2)));
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
                particle::ParticleSlotReader value(storage, particle::ParticleSlot { .index = slot });
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
                auto p = place[i];
                place[i] = place[j];
                place[j] = p;
                auto v = result.velocity[i];
                result.velocity[i] = result.velocity[j];
                result.velocity[j] = v;
            }
        }
    }
    EXPECT_EQ(result.count, emitter);
    return result;
}

// One frame at 30 a second moves velocities by factor x average x dt x (0.025 x 30)^0.7 (Wallpaper Engine's frame-rate
// compensation; at 40 frames a second and more the time step is used as it is).
static constexpr double kBoidsStep30 = (1.0 / 30.0) * 0.81766;

TEST(ParticleSubSystem, BoidsSeparationAveragesOverNeighboursIncludingTheEmptyPlaces) {
    auto config = R"({"name":"boids","separationthreshold":60,"neighborthreshold":200,"separationfactor":1,
                      "alignmentfactor":0,"cohesionfactor":0})"_str;
    // 40 apart: the other particle (60 - 40) and two empty places 20 away (60 - 20 each), averaged over 3
    auto near = BoidsVelocities(config, { -20.0f, 20.0f });
    EXPECT_NEAR(near.velocity[1].x(), kBoidsStep30 * 100.0 / 3.0, 0.01);
    EXPECT_NEAR(near.velocity[0].x(), -near.velocity[1].x(), 1e-4);
    // 100 apart: only the empty places 50 away (60 - 50)
    EXPECT_NEAR(BoidsVelocities(config, { -50.0f, 50.0f }).velocity[1].x(), kBoidsStep30 * 10.0, 0.005);
    // 130 apart: nothing within 60
    EXPECT_NEAR(BoidsVelocities(config, { -65.0f, 65.0f }).velocity[1].x(), 0.0, 1e-6);
}

TEST(ParticleSubSystem, BoidsCohesionPullsTowardsTheNeighboursMeanPosition) {
    auto config = R"({"name":"boids","separationthreshold":60,"neighborthreshold":200,"separationfactor":0,
                      "alignmentfactor":0,"cohesionfactor":1})"_str;
    // 150 apart: the mean of the other particle and the two empty places is 25 from the origin, 100 from -75
    EXPECT_NEAR(BoidsVelocities(config, { -75.0f, 75.0f }).velocity[0].x(), kBoidsStep30 * 100.0, 0.05);
    // 250 apart: the other particle is out of reach, the empty places pull to the origin
    EXPECT_NEAR(BoidsVelocities(config, { -125.0f, 125.0f }).velocity[0].x(), kBoidsStep30 * 125.0, 0.05);
    // three particles: one empty place
    auto three = BoidsVelocities(config, { -60.0f, -40.0f, 60.0f });
    EXPECT_NEAR(three.velocity[0].x(), kBoidsStep30 * (20.0 / 3.0 + 60.0), 0.05);
}

TEST(ParticleSubSystem, BoidsAlignmentSteersTowardsTheNeighboursMeanVelocity) {
    auto config = R"({"name":"boids","separationthreshold":1,"neighborthreshold":200,"separationfactor":0,
                      "alignmentfactor":1,"cohesionfactor":0})"_str;
    // one particle moving at 10 with three empty places at rest: the mean velocity is 0
    auto alone = BoidsVelocities(config, { 30.0f }, 1, 1.0 / 30.0,
                                 R"({"name":"velocityrandom","min":"10 0 0","max":"10 0 0"})"_str);
    EXPECT_NEAR(alone.velocity[0].x(), 10.0 - kBoidsStep30 * 10.0, 0.005);
}

TEST(ParticleSubSystem, BoidsCompensatesForTheFrameRate) {
    auto config = R"({"name":"boids","separationthreshold":60,"neighborthreshold":200,"separationfactor":1,
                      "alignmentfactor":0,"cohesionfactor":0})"_str;
    // at 60 frames a second each step uses the whole time step: one second gains 1 / 0.81766 times what it does at 30
    auto at30 = BoidsVelocities(config, { -50.0f, 50.0f }, 30, 1.0 / 30.0).velocity[1].x();
    auto at60 = BoidsVelocities(config, { -50.0f, 50.0f }, 60, 1.0 / 60.0).velocity[1].x();
    EXPECT_NEAR(at60 / at30, 1.0 / 0.81766, 0.01);
}

// Low frame rates (particles-lowfps-substeps): the subsystem reads the scene's target frame rate. dt2 = dt x
// min(0.025 / target frame time, 1)^0.7, from the target frame time, not the substep; at 1..20 frames a second the
// operators run twice with half of both, the emitters and ages once with the whole step.
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
                particle::ParticleSlotReader value(storage, particle::ParticleSlot { .index = slot });
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
        EXPECT_NEAR(trace.deltas2[usize()],
                    tick * Dt2Scale(static_cast<double>(fps.to_primitive())),
                    1e-6);
    }
}

TEST(ParticleSubSystem, Dt2ComesFromTheTargetFrameTimeNotTheStep) {
    // a 15 fps target with a longer step (speed, rate): two half steps, dt2 still scaled by 15 fps's factor
    auto trace = TraceOperators(u32(15), 0.1);
    ASSERT_EQ(trace.deltas.len(), usize(2));
    EXPECT_NEAR(trace.deltas2[usize()], 0.05 * Dt2Scale(15.0), 1e-6);
    // without a target frame rate the step stands in for the frame time, in one pass
    auto unknown = TraceOperators(u32(), 1.0 / 15.0);
    ASSERT_EQ(unknown.deltas.len(), usize(1));
    EXPECT_NEAR(unknown.deltas2[usize()], (1.0 / 15.0) * Dt2Scale(15.0), 1e-6);
}

TEST(ParticleSubSystem, BoidsTakesTwoHalfStepsAtFifteenFramesASecond) {
    auto config = R"({"name":"boids","separationthreshold":1,"neighborthreshold":200,"separationfactor":0,
                      "alignmentfactor":1,"cohesionfactor":0})"_str;
    // one particle moving at 10 with three empty places at rest steers to 0 twice a frame by half of dt2
    const double step = (1.0 / 15.0) * Dt2Scale(15.0);
    auto alone = BoidsVelocities(config, { 30.0f }, 1, 1.0 / 15.0,
                                 R"({"name":"velocityrandom","min":"10 0 0","max":"10 0 0"})"_str, u32(15));
    EXPECT_NEAR(alone.velocity[0].x(), 10.0 * (1.0 - step / 2.0) * (1.0 - step / 2.0), 1e-3);
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
