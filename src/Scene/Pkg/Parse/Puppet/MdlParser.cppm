module;

export module wescene.pkg.parse:mdl_parser;
import eigen;
import wescene.core;
import rstd;
import wescene.fs;
import wescene.scene;
import wescene.pkg.scene_obj;

export import wescene.pkg.puppet;

import :shader_parser; // ShaderInfo

using namespace rstd::prelude;
using rstd::sync::Arc;

export namespace owe

{

// File header preceding the per-mesh body. Per hexpat: 4-byte version tag +
// u32 vertex layout flag + u32 skin_count + u32 mesh_count.
struct MdlHeader {
    rstd::int32_t  mdlv { 13 };
    rstd::uint32_t mdl_flag { 0 }; // vertex layout bitmask; mdlv<=14 meshes inherit this
    rstd::uint32_t skin_count { 1 };
    rstd::uint32_t mesh_count { 1 };
};

struct Mdl {
    MdlHeader header;

    // One element per header.mesh_count. Image puppets may use later meshes
    // for material-specific overlays such as texture-channel animation.
    struct Mesh {
        Vec<String>     mat_json_files;
        rstd::uint32_t  flag_a { 0 }; // hexpat Mesh.flag_a (usually 0; 2 has trailing 1)
        bool            has_flag_a2_one { false };
        rstd::uint32_t  flag { 0 }; // per-mesh vertex layout flag (mdlv>14); 0 = inherit header
        array<float, 3> aabb_min {};
        array<float, 3> aabb_max {};
        bool            has_aabb { false }; // mdlv>=17

        // SoA attributes; empty means the bit was not set in `flag`.
        Vec<array<float, 3>>          positions;
        Vec<array<float, 3>>          normals;
        Vec<array<float, 4>>          tangents; // tangent[3] + tangent_sign
        // The fourth position float (vertex flag 0x10000): the vertex's blend shape index, 1..V
        // for the vertices the mesh's blend shapes move, 0 for the others.
        Vec<float>                    morph_indices;

        // Blend shapes (MDMP): each moves the `morph_vertex_count` vertices whose morph index is
        // set, by its offsets (over 32767) times `morph_scale` times its weight.
        struct MorphShape {
            rstd::uint32_t               id { 0 };
            String                       name;
            Vec<array<rstd::int16_t, 3>> offsets;
            Vec<rstd::int16_t>           alpha; // flag_a 0x1000: over 32767
        };
        float                         morph_scale { 0.0f };
        rstd::uint32_t                morph_vertex_count { 0 };
        Vec<MorphShape>               morph_shapes;
        Vec<array<rstd::uint32_t, 4>> blend_indices;
        Vec<array<float, 4>>          blend_weights;
        Vec<array<float, 2>>          texcoords;
        Vec<array<float, 2>>          texcoord2;

        Vec<array<rstd::uint32_t, 3>> indices;

        // V21+ Parts sub-block — uv2 region per vertex + part draw ranges.
        struct Part {
            rstd::uint32_t id;
            rstd::uint32_t start;
            rstd::uint32_t size;
            rstd::int32_t  draw_order_offset { 0 };
        };
        Vec<array<float, 2>> part_uv2;
        Vec<rstd::uint32_t>  part_uv2_pad;
        Vec<Part>            parts;

        // V23+ Mask blocks attached to a single-puppet mesh.
        struct MaskBlock {
            rstd::uint32_t      leading_a;
            String              mat_json;
            Vec<rstd::uint32_t> part_ids_a;
            Vec<rstd::uint32_t> part_ids_b;
        };
        Vec<MaskBlock> masks;
    };
    Vec<Mesh> meshes;

    rstd::int32_t mdls { 1 };
    rstd::int32_t mdla { 1 };
    rstd::int32_t mdle { 0 }; // 0 = section not present
    rstd::int32_t mdmp { 0 };

    // MDMP morph sections — present when an animation drives shape blends.

    Option<Arc<Puppet>> puppet;
    // combo
    // SKINNING = 1
    // BONECOUNT

    // input
    // uvec4 a_BlendIndices
    // vec4 a_BlendWeights
    // uniform mat4x3 g_Bones[BONECOUNT]
};

// What a mesh's blend shapes move: each vertex's rest position (with the submesh's offset), its
// morph index (0 for none), and each shape's offsets.
struct MdlMorph {
    Vec<array<float, 3>>              rest;
    Vec<rstd::uint32_t>               index;
    float                             scale { 0.0f };
    rstd::uint32_t                    vertex_count { 0 };
    Vec<Vec<array<rstd::int16_t, 3>>> offsets;
};

class MdlParser {
public:
    // Reads only the bytes preceding mat_json_files. Cheap; safe to call
    // over the whole corpus even on mdls that would hang full Parse.
    static bool ParseHeader(ref<str> path, fs::VFS&, MdlHeader&);

    static bool                      Parse(ref<str> path, fs::VFS&, Mdl&);
    static Option<wpscene::Material> ParseMaterial(ref<str> material_ref, fs::VFS&);
    static Option<usize>             FindMeshByMaterial(const Mdl&, ref<str> material_ref);

    static void AddPuppetShaderInfo(ShaderInfo& info, const Mdl& mdl);
    static void AddPuppetMatInfo(wpscene::Material& mat, const Mdl& mdl);

    // Emit vertex/index arrays for any Mdl::Mesh, sending only the vertex
    // attribute streams whose SoA vectors are populated. Skinning combos must
    // be wired separately via AddPuppetShaderInfo / AddPuppetMatInfo when the
    // mesh has bone weights.
    static void GenMeshFromMdl(SceneMesh::Submesh& submesh, const Mdl::Mesh& src,
                               array<float, 2> texcoord_scale  = { 1.0f, 1.0f },
                               array<float, 3> position_offset = {});

    // Like GenMeshFromMdl, but the submesh draws only the parts whose `id` is
    // in `clip_part_ids` — used for clipping-mask submeshes that only cover the
    // affected (e.g. iris) parts. Material slot is the caller's responsibility.
    static void GenMaskSubmeshFromMdl(SceneMesh::Submesh& submesh, const Mdl::Mesh& src,
                                      slice<rstd::uint32_t> clip_part_ids,
                                      array<float, 2>       texcoord_scale = { 1.0f, 1.0f });
    static void BindDrawOrder(SceneMesh::Submesh&, const Mdl::Mesh&, Arc<PuppetLayer>);
    // A mesh's blend shapes, or None without any.
    static auto MakeMorph(const Mdl::Mesh&, array<float, 3> position_offset)
        -> Option<Arc<MdlMorph>>;
    // The positions (three floats a vertex) with the shapes at these weights.
    static auto MorphPositions(const MdlMorph&, slice<float> weights) -> Vec<float>;
    // Moves this submesh's vertices by the mesh's blend shapes every frame, as the puppet's
    // animation layers weigh them. Nothing for a mesh without blend shapes.
    static void BindMorph(Scene& scene, SceneMesh& mesh, usize submesh, const Mdl::Mesh& src,
                          Arc<PuppetLayer> layer, array<float, 3> position_offset = {});
};

} // namespace owe
