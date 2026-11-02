module;

export module wescene.pkg.parse:particle_parser;
import rstd;
import wescene.json;
import wescene.scene;
import wescene.fs;
import wescene.particle.program;
import :particle_runtime;

export import wescene.pkg.scene_obj;

export namespace owe

{
// The range rotationrandom draws from (radians): numbers turn the particle in its plane only,
// as the z of a vector; vectors turn it about each axis. Without a range, any turn in the plane.
struct ParticleRotationRange {
    array<float, 3> min { 0.0f, 0.0f, 0.0f };
    array<float, 3> max { 0.0f, 0.0f, 0.0f };
    float           exponent { 1.0f };
};
ParticleRotationRange ReadRotationRandom(const Json& initializer);

class ParticleParser {
public:
    static ParticleSpawnInstruction GenInitializer(const Json&, u32 implicit_sequence_count);
    static Box<dyn<particle::ParticleUpdateProgram>>
    GenOperator(const Json&, ParticleInstanceModifiers, ParticleSubSystem&, usize operator_index);
    static Box<dyn<particle::ParticleEmitterProgram>> GenEmitter(const wpscene::Emitter&,
                                                                 ParticleSubSystem&, usize);
    static ParticleSpawnInstruction                   GenOverride(ParticleInstanceModifiers);
};

// Whether a particle shader is drawn with a geometry stage: only when the device has one and the
// shader comes with a .geom. Shaders with only a .vert and a .frag expand their quads themselves.
bool ParticleShaderUsesGeometryStage(fs::VFS& vfs, ref<str> shader, bool geometry_shader_supported);
} // namespace owe
