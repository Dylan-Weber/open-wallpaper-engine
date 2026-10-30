#include <rstd/test/gtest.hpp>

import rstd;
import rstd.cppstd;
import eigen;
import wescene.json;
import wescene.particle;
import wescene.particle.program;
import wescene.pkg.parse;
import wescene.scene;

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

// collisionplane (particles-collisionplane): a particle may not go below the plane n . x = distance (n is the
// "plane" key, up by default). One below it is put on the plane and its speed along n becomes |v_n| * bouncefactor
// (so one already moving away is sent up the same); "slide" zeroes v_n, "delete" removes the particle. The sideways
// speed is kept.
struct CollidedParticle {
    bool            alive { false };
    Eigen::Vector3f position;
    Eigen::Vector3f velocity;
    float           alpha { 1.0f };
};

static CollidedParticle CollideWithPlane(ref<str> collision, Eigen::Vector3f origin, ref<str> velocity,
                                        int ticks = 1, bool scene_bounds = false) {
    owe::Scene             scene;
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
    subsystem.AddInitializer(owe::ParticleParser::GenInitializer(
        owe::ParseJson(velocity).unwrap(), u32(1)));
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
        owe::ParseJson(R"({"name":"movement"})"_str).unwrap(), modifiers.Clone(), subsystem, usize()));
    subsystem.AddOperator(owe::ParticleParser::GenOperator(
        owe::ParseJson(collision).unwrap(), modifiers.Clone(), subsystem, usize(1)));
    subsystem.Finalize();
    for (int tick = 0; tick < ticks; ++tick) subsystem.Tick(f64(1.0 / 60.0), false);
    CollidedParticle result;
    result.alive = subsystem.System().Instance(usize()).Binding().Read().Positions().len() != usize();
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
    auto       hit    = CollideWithPlane(bounce, { 0, -100, 0 }, R"({"name":"velocityrandom","min":"20 -60 0","max":"20 -60 0"})"_str);
    ASSERT_TRUE(hit.alive);
    EXPECT_NEAR(hit.position.y(), -100.0f, 1e-4f);
    EXPECT_NEAR(hit.velocity.y(), 30.0f, 1e-3f);
    EXPECT_NEAR(hit.velocity.x(), 20.0f, 1e-3f);
    auto above = CollideWithPlane(bounce, { 0, -50, 0 }, R"({"name":"velocityrandom","min":"0 -60 0","max":"0 -60 0"})"_str);
    EXPECT_NEAR(above.position.y(), -51.0f, 1e-4f);
    EXPECT_NEAR(above.velocity.y(), -60.0f, 1e-3f);
    auto under = CollideWithPlane(bounce, { 0, -150, 0 }, R"({"name":"velocityrandom","min":"0 40 0","max":"0 40 0"})"_str);
    EXPECT_NEAR(under.position.y(), -100.0f, 1e-4f);
    EXPECT_NEAR(under.velocity.y(), 20.0f, 1e-3f);
}

TEST(ParticleSubSystem, CollisionPlaneSlidesDeletesAndTakesAnyNormal) {
    auto slide = CollideWithPlane(
        R"({"name":"collisionplane","distance":-100,"bouncefactor":0.5,"collisionbehavior":"slide"})"_str,
        { 0, -100, 0 }, R"({"name":"velocityrandom","min":"20 -60 0","max":"20 -60 0"})"_str);
    EXPECT_NEAR(slide.velocity.y(), 0.0f, 1e-3f);
    EXPECT_NEAR(slide.velocity.x(), 20.0f, 1e-3f);
    EXPECT_NEAR(slide.position.y(), -100.0f, 1e-4f);
    auto gone = CollideWithPlane(
        R"({"name":"collisionplane","distance":-100,"collisionbehavior":"delete"})"_str, { 0, -100, 0 },
        R"({"name":"velocityrandom","min":"0 -60 0","max":"0 -60 0"})"_str);
    EXPECT_FLOAT_EQ(gone.alive ? gone.alpha : 0.0f, 0.0f); // hidden at once, removed with the lifecycle
    auto wall = CollideWithPlane(R"({"name":"collisionplane","plane":"1 0 0","distance":-100,"bouncefactor":0.5})"_str,
                                 { -100, 0, 0 }, R"({"name":"velocityrandom","min":"-60 7 0","max":"-60 7 0"})"_str);
    EXPECT_NEAR(wall.position.x(), -100.0f, 1e-4f);
    EXPECT_NEAR(wall.velocity.x(), 30.0f, 1e-3f);
    EXPECT_NEAR(wall.velocity.y(), 7.0f, 1e-3f);
}

// collisionbounds (particles-collisionbounds-rest): the scene's edges, 0..1920 across and 0..1080 up
TEST(ParticleSubSystem, CollisionBoundsKeepParticlesInsideTheScene) {
    const auto bounds = R"({"name":"collisionbounds","bouncefactor":0.5})"_str;
    auto       velocity = [](ref<str> v) { return rstd::format(R"({{"name":"velocityrandom","min":"{}","max":"{}"}})", v, v); };
    auto       run      = [&](Eigen::Vector3f origin, ref<str> v, ref<str> operator_json = R"({"name":"collisionbounds","bouncefactor":0.5})"_str) {
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
    auto gone = run({ 1919, 500, 0 }, "120 0 0"_str, R"({"name":"collisionbounds","collisionbehavior":"delete"})"_str);
    EXPECT_FLOAT_EQ(gone.alive ? gone.alpha : 0.0f, 0.0f);
}

// collisionsphere / collisionquad (particles-collisionshapes, particles-collisionquad-sides)
TEST(ParticleSubSystem, CollisionSphereKeepsParticlesOutAlongTheSurfaceNormal) {
    const auto sphere = R"({"name":"collisionsphere","origin":"0 0 0","radius":100,"bouncefactor":0.5})"_str;
    auto       top    = CollideWithPlane(sphere, { 0, 101, 0 },
                                         R"({"name":"velocityrandom","min":"0 -120 0","max":"0 -120 0"})"_str);
    EXPECT_NEAR(top.position.y(), 100.0f, 1e-3f);
    EXPECT_NEAR(top.velocity.y(), 60.0f, 1e-3f);
    auto side = CollideWithPlane(sphere, { 60, 80, 0 },
                                 R"({"name":"velocityrandom","min":"0 -60 0","max":"0 -60 0"})"_str);
    const Eigen::Vector3f moved(60.0f, 80.0f - 1.0f, 0.0f);
    const Eigen::Vector3f normal = moved.normalized();
    EXPECT_NEAR((side.position - normal * 100.0f).norm(), 0.0f, 1e-3f);
    const float along = normal.dot(Eigen::Vector3f(0, -60, 0));
    const Eigen::Vector3f expected = Eigen::Vector3f(0, -60, 0) + normal * (-along * 0.5f - along);
    EXPECT_NEAR((side.velocity - expected).norm(), 0.0f, 1e-3f);
    auto away = CollideWithPlane(sphere, { 0, 150, 0 },
                                 R"({"name":"velocityrandom","min":"0 -60 0","max":"0 -60 0"})"_str);
    EXPECT_NEAR(away.velocity.y(), -60.0f, 1e-3f);
}

TEST(ParticleSubSystem, CollisionQuadCatchesAParticleFallingOnItButNotOneRisingThroughIt) {
    const auto quad =
        R"({"name":"collisionquad","origin":"0 -100 0","size":"200 100","bouncefactor":0.5})"_str;
    auto landed = CollideWithPlane(quad, { 50, -100, 0 },
                                   R"({"name":"velocityrandom","min":"0 -60 0","max":"0 -60 0"})"_str);
    EXPECT_NEAR(landed.position.y(), -100.0f, 1e-4f);
    EXPECT_NEAR(landed.velocity.y(), 30.0f, 1e-3f);
    auto rising = CollideWithPlane(quad, { 50, -100.5f, 0 },
                                   R"({"name":"velocityrandom","min":"0 60 0","max":"0 60 0"})"_str);
    EXPECT_NEAR(rising.velocity.y(), 60.0f, 1e-3f);
    auto beside = CollideWithPlane(quad, { 150, -100, 0 },
                                   R"({"name":"velocityrandom","min":"0 -60 0","max":"0 -60 0"})"_str);
    EXPECT_NEAR(beside.velocity.y(), -60.0f, 1e-3f);
    auto angled = CollideWithPlane(
        R"({"name":"collisionquad","origin":"0 -100 0","size":"200 100","angles":"0 0 30","bouncefactor":0.5})"_str,
        { 50, -100, 0 }, R"({"name":"velocityrandom","min":"0 -60 0","max":"0 -60 0"})"_str);
    EXPECT_NEAR(angled.velocity.y(), 30.0f, 1e-3f);
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
