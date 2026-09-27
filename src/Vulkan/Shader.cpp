module;

#include <rstd/macro.hpp>

#include <glslang/Public/ShaderLang.h>
#include <glslang/Public/ResourceLimits.h>

#if __has_include(<glslang/SPIRV/GlslangToSpv.h>)
#    include <glslang/SPIRV/GlslangToSpv.h>
#else
#    include <SPIRV/GlslangToSpv.h>
#endif

module wescene.shader_compile;
import wescene.core;
import wescene.types;
import wescene.utils;
import rstd;
import spirvto;
import rstd.log;
import rstd.cppstd;

using namespace rstd::prelude;
using namespace rstd::literals;
using rstd::collections::BTreeMap;
using rstd::cppstd::as_str;
using rstd::ffi::CStr;
using rstd::ffi::CString;
using rstd::path::PathBuf;
using rstd::sync::atomic::Atomic;
using namespace owe;
using namespace owe::vulkan;

namespace owe::vulkan
{
struct GlslangBackend {};
} // namespace owe::vulkan

namespace rstd
{
template<>
struct Impl<vrento::vulkan::ShaderBackend, owe::vulkan::GlslangBackend>
    : ImplBase<owe::vulkan::GlslangBackend> {
    bool Preprocess(ref<str> source, ShaderType stage, SourceLang lang, String& output) const {
        return owe::vulkan::Preprocess(source, stage, lang, output);
    }
    bool CompileAndLinkShaderUnits(slice<ShaderCompUnit> units, const ShaderCompOpt& options,
                                   Vec<Uni_ShaderSpv>& output) const {
        return owe::vulkan::CompileAndLinkShaderUnits(units, options, output);
    }
    bool GenReflect(slice<ShaderCode> codes, Vec<Uni_ShaderSpv>& stages,
                    ShaderReflected& output) const {
        return owe::vulkan::GenReflect(codes, stages, output);
    }
};
} // namespace rstd

auto owe::vulkan::MakeShaderBackend() -> Box<dyn<ShaderBackend>> {
    return Box<dyn<ShaderBackend>>::make(GlslangBackend {});
}

namespace
{

struct GlslangProcessRuntime {
    GlslangProcessRuntime(): active(glslang::InitializeProcess()) { rstd_assert(active); }
    GlslangProcessRuntime(const GlslangProcessRuntime&) = delete;
    GlslangProcessRuntime(GlslangProcessRuntime&& other) noexcept: active(other.active) {
        other.active = false;
    }
    ~GlslangProcessRuntime() {
        if (active) glslang::FinalizeProcess();
    }

    bool active { false };
};

void EnsureGlslangProcess() {
    static const rstd::sync::OnceLock<GlslangProcessRuntime> runtime;
    (void)runtime.get_or_init([] {
        return GlslangProcessRuntime {};
    });
}

ShaderScalarKind ReflectedScalarKind(const spirvto::BlockVariable& variable) {
    switch (variable.numeric.scalar_kind) {
    case spirvto::ScalarKind::Float: return ShaderScalarKind::Float;
    case spirvto::ScalarKind::Boolean: return ShaderScalarKind::Boolean;
    case spirvto::ScalarKind::SignedInteger: return ShaderScalarKind::SignedInteger;
    case spirvto::ScalarKind::UnsignedInteger: return ShaderScalarKind::UnsignedInteger;
    default: return ShaderScalarKind::Unknown;
    }
}

ShaderMatrixMajor ReflectedMatrixMajor(const spirvto::BlockVariable& variable) {
    switch (variable.matrix_major) {
    case spirvto::MatrixMajor::Row: return ShaderMatrixMajor::Row;
    case spirvto::MatrixMajor::Column: return ShaderMatrixMajor::Column;
    default: return ShaderMatrixMajor::None;
    }
}

usize ReflectedArrayCount(slice<spirvto::Word> dimensions) {
    usize count { 1 };
    for (auto dimension : dimensions) count *= usize(dimension);
    return count;
}

auto ReflectBlockedUniform(const spirvto::BlockVariable& variable, int block_index)
    -> ShaderReflected::BlockedUniform {
    ShaderReflected::BlockedUniform uniform {};
    uniform.block_index  = block_index;
    uniform.size         = usize(variable.size);
    uniform.offset       = variable.offset;
    uniform.num          = ReflectedArrayCount(variable.array_dimensions.as_slice());
    uniform.scalar_kind  = ReflectedScalarKind(variable);
    uniform.scalar_width = variable.numeric.scalar_width;
    uniform.vector_components =
        variable.numeric.vector_components == 0 ? 1 : variable.numeric.vector_components;
    uniform.matrix_rows    = variable.numeric.matrix_rows;
    uniform.matrix_columns = variable.numeric.matrix_columns;
    uniform.matrix_major   = ReflectedMatrixMajor(variable);
    uniform.matrix_stride  = variable.matrix_stride;
    uniform.array_stride   = variable.array_stride;
    uniform.array_dimensions.reserve(variable.array_dimensions.len());
    for (auto dimension : variable.array_dimensions) uniform.array_dimensions.push(u32(dimension));
    return uniform;
}

bool SameBlockedUniformLayout(const ShaderReflected::BlockedUniform& lhs,
                              const ShaderReflected::BlockedUniform& rhs) {
    return lhs.offset == rhs.offset && lhs.size == rhs.size && lhs.num == rhs.num &&
           lhs.scalar_kind == rhs.scalar_kind && lhs.scalar_width == rhs.scalar_width &&
           lhs.vector_components == rhs.vector_components && lhs.matrix_rows == rhs.matrix_rows &&
           lhs.matrix_columns == rhs.matrix_columns && lhs.matrix_stride == rhs.matrix_stride &&
           lhs.matrix_major == rhs.matrix_major && lhs.array_stride == rhs.array_stride &&
           lhs.array_dimensions == rhs.array_dimensions;
}

bool SameUniformBlockLayout(const ShaderReflected::Block& reflected,
                            const spirvto::BlockVariable& block) {
    if (reflected.size != block.size || reflected.member_map.len() != block.members.len())
        return false;
    for (const auto& variable : block.members) {
        auto existing = reflected.member_map.get(variable.name.as_str());
        if (existing.is_none() ||
            ! SameBlockedUniformLayout(**existing,
                                       ReflectBlockedUniform(variable, reflected.index))) {
            return false;
        }
    }
    return true;
}

// Spill a payload to /tmp/<sha1> for post-mortem inspection. Returns the
// written path so callers can mention it in the error message.
PathBuf logToTmpfileWithSha1(ref<str> source) {
    auto digest   = utils::genSha1(slice<rstd::byte>::from_raw_parts(source.data(), source.len()));
    auto path     = rstd::env::temp_dir().join(digest.as_str());
    auto contents = String::make(source);
    contents.push_str("\n"_str);
    (void)rstd::fs::write(path.as_path(), contents.as_str().as_bytes());
    return path;
}

inline VkShaderStageFlagBits ToVkType(owe::ShaderType s) {
    switch (s) {
    case ShaderType::VERTEX: return VK_SHADER_STAGE_VERTEX_BIT;
    case ShaderType::FRAGMENT: return VK_SHADER_STAGE_FRAGMENT_BIT;
    case ShaderType::GEOMETRY: return VK_SHADER_STAGE_GEOMETRY_BIT;
    }
    rstd_assert(false);
    return VK_SHADER_STAGE_VERTEX_BIT;
}

auto ReflectedFormat(const spirvto::NumericType& type) -> VkFormat {
    if (type.matrix_columns || type.vector_components < 1 || type.vector_components > 4)
        return VK_FORMAT_UNDEFINED;
    const bool floating       = type.scalar_kind == spirvto::ScalarKind::Float;
    const bool signed_integer = type.scalar_kind == spirvto::ScalarKind::SignedInteger;
    if (! floating && ! signed_integer && type.scalar_kind != spirvto::ScalarKind::UnsignedInteger)
        return VK_FORMAT_UNDEFINED;
    const unsigned     category       = floating ? 2 : signed_integer ? 1 : 0;
    constexpr VkFormat formats16[][3] = {
        { VK_FORMAT_R16_UINT, VK_FORMAT_R16_SINT, VK_FORMAT_R16_SFLOAT },
        { VK_FORMAT_R16G16_UINT, VK_FORMAT_R16G16_SINT, VK_FORMAT_R16G16_SFLOAT },
        { VK_FORMAT_R16G16B16_UINT, VK_FORMAT_R16G16B16_SINT, VK_FORMAT_R16G16B16_SFLOAT },
        { VK_FORMAT_R16G16B16A16_UINT, VK_FORMAT_R16G16B16A16_SINT, VK_FORMAT_R16G16B16A16_SFLOAT },
    };
    constexpr VkFormat formats32[][3] = {
        { VK_FORMAT_R32_UINT, VK_FORMAT_R32_SINT, VK_FORMAT_R32_SFLOAT },
        { VK_FORMAT_R32G32_UINT, VK_FORMAT_R32G32_SINT, VK_FORMAT_R32G32_SFLOAT },
        { VK_FORMAT_R32G32B32_UINT, VK_FORMAT_R32G32B32_SINT, VK_FORMAT_R32G32B32_SFLOAT },
        { VK_FORMAT_R32G32B32A32_UINT, VK_FORMAT_R32G32B32A32_SINT, VK_FORMAT_R32G32B32A32_SFLOAT },
    };
    constexpr VkFormat formats64[][3] = {
        { VK_FORMAT_R64_UINT, VK_FORMAT_R64_SINT, VK_FORMAT_R64_SFLOAT },
        { VK_FORMAT_R64G64_UINT, VK_FORMAT_R64G64_SINT, VK_FORMAT_R64G64_SFLOAT },
        { VK_FORMAT_R64G64B64_UINT, VK_FORMAT_R64G64B64_SINT, VK_FORMAT_R64G64B64_SFLOAT },
        { VK_FORMAT_R64G64B64A64_UINT, VK_FORMAT_R64G64B64A64_SINT, VK_FORMAT_R64G64B64A64_SFLOAT },
    };
    if (type.scalar_width == 16) return formats16[type.vector_components - 1][category];
    if (type.scalar_width == 32) return formats32[type.vector_components - 1][category];
    if (type.scalar_width == 64) return formats64[type.vector_components - 1][category];
    return VK_FORMAT_UNDEFINED;
}

auto ReflectedStage(spirvto::spv::ExecutionModel model) -> Option<owe::ShaderType> {
    switch (model) {
    case spirvto::spv::ExecutionModel::Vertex: return Some(ShaderType::VERTEX);
    case spirvto::spv::ExecutionModel::Fragment: return Some(ShaderType::FRAGMENT);
    case spirvto::spv::ExecutionModel::Geometry: return Some(ShaderType::GEOMETRY);
    default: return None<owe::ShaderType>();
    }
}

inline EShLanguage ToEShLanguage(owe::ShaderType s) {
    switch (s) {
    case ShaderType::VERTEX: return EShLangVertex;
    case ShaderType::FRAGMENT: return EShLangFragment;
    case ShaderType::GEOMETRY: return EShLangGeometry;
    }
    rstd_assert(false);
    return EShLangVertex;
}

inline glslang::EShTargetClientVersion ToClientVersion(VulkanTarget t) {
    switch (t) {
    case VulkanTarget::Vulkan_1_0: return glslang::EShTargetVulkan_1_0;
    case VulkanTarget::Vulkan_1_1: return glslang::EShTargetVulkan_1_1;
    case VulkanTarget::Vulkan_1_2: return glslang::EShTargetVulkan_1_2;
    case VulkanTarget::Vulkan_1_3: return glslang::EShTargetVulkan_1_3;
    }
    return glslang::EShTargetVulkan_1_1;
}

inline glslang::EShTargetLanguageVersion ToSpvVersion(VulkanTarget t) {
    // Pair Vulkan target with the matching SPIR-V version. See
    // https://github.com/KhronosGroup/glslang/blob/main/StandAlone/StandAlone.cpp
    switch (t) {
    case VulkanTarget::Vulkan_1_0: return glslang::EShTargetSpv_1_0;
    case VulkanTarget::Vulkan_1_1: return glslang::EShTargetSpv_1_3;
    case VulkanTarget::Vulkan_1_2: return glslang::EShTargetSpv_1_5;
    case VulkanTarget::Vulkan_1_3: return glslang::EShTargetSpv_1_6;
    }
    return glslang::EShTargetSpv_1_3;
}

inline const char* DefaultEntryName(SourceLang lang, owe::ShaderType s) {
    if (lang == SourceLang::Glsl) return "main";
    switch (s) {
    case ShaderType::VERTEX: return "main_vs";
    case ShaderType::FRAGMENT: return "main_ps";
    case ShaderType::GEOMETRY: return "main_gs";
    }
    return "main";
}

} // namespace

bool owe::vulkan::GenReflect(slice<ShaderCode> codes, Vec<Uni_ShaderSpv>& spvs,
                             ShaderReflected& output) {
    Vec<Uni_ShaderSpv>      stages;
    ShaderReflected         ref;
    BTreeMap<String, usize> uniform_block_indices;
    for (const auto& code : codes) {
        auto reflection = spirvto::reflect(code.as_slice());
        if (reflection.is_err()) {
            const auto error = reflection.unwrap_err();
            rstd_error("SPIR-V reflection failed at word {}: {}",
                       error.word_offset,
                       static_cast<unsigned>(error.kind));
            return false;
        }
        auto& reflected    = *reflection;
        auto  shader_stage = ReflectedStage(reflected.execution_model);
        if (shader_stage.is_none()) return false;
        VkShaderStageFlagBits stage = ::ToVkType(*shader_stage);
        {
            auto spv         = Box<ShaderSpv>::make();
            spv->stage       = *shader_stage;
            spv->spirv       = code.clone();
            spv->entry_point = reflected.entry_point.clone();
            stages.push(rstd::move(spv));
        }

        VkDescriptorSetLayoutBinding vkbinding {};
        vkbinding.stageFlags = stage;

        for (const auto& b : reflected.bindings) {
            if (! b.accessed && b.type != spirvto::DescriptorType::UniformBuffer) continue;
            if (b.count != 1) {
                rstd_error("descriptor arrays are unsupported: {}", b.name);
                return false;
            }
            if (b.type == spirvto::DescriptorType::Sampler) {
                bool paired = false;
                for (const auto& image : reflected.bindings) {
                    if (image.accessed && image.set == b.set && image.binding == b.binding &&
                        image.count == b.count &&
                        (image.type == spirvto::DescriptorType::SampledImage ||
                         image.type == spirvto::DescriptorType::CombinedImageSampler))
                        paired = true;
                }
                if (! paired) {
                    rstd_error("unpaired sampler is unsupported: {}", b.name);
                    return false;
                }
                continue;
            }

            const auto bind_name =
                b.name.as_str().is_empty() ? b.type_name.as_str() : b.name.as_str();
            if (b.type != spirvto::DescriptorType::UniformBuffer &&
                b.type != spirvto::DescriptorType::SampledImage &&
                b.type != spirvto::DescriptorType::CombinedImageSampler &&
                b.type != spirvto::DescriptorType::Sampler)
                return false;

            if (auto existing = ref.binding_map.get_mut(bind_name); existing.is_some()) {
                auto&      bind            = **existing;
                const auto descriptor_type = b.type == spirvto::DescriptorType::UniformBuffer
                                                 ? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER
                                             : b.type == spirvto::DescriptorType::Sampler
                                                 ? VK_DESCRIPTOR_TYPE_SAMPLER
                                                 : VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                if (bind.set != b.set || bind.layout.binding != b.binding ||
                    bind.layout.descriptorType != descriptor_type ||
                    bind.layout.descriptorCount != b.count) {
                    rstd_error("descriptor {} differs across shader stages", bind_name);
                    return false;
                }
                if (b.type == spirvto::DescriptorType::UniformBuffer) {
                    auto block_index = uniform_block_indices.get(bind_name);
                    if (block_index.is_none() ||
                        ! SameUniformBlockLayout(ref.blocks[**block_index], b.block)) {
                        rstd_error("uniform block {} layout differs across shader stages",
                                   bind_name);
                        return false;
                    }
                }
                bind.layout.stageFlags |= stage;
                continue;
            }
            if (b.type == spirvto::DescriptorType::UniformBuffer) {
                auto& block      = b.block;
                auto  block_name = block.name.as_str();
                ref.blocks.push(ShaderReflected::Block {
                    .index      = static_cast<int>(ref.blocks.len().to_primitive()),
                    .size       = block.size,
                    .name       = rstd::into(block_name.is_empty() ? bind_name : block_name),
                    .set        = b.set,
                    .binding    = b.binding,
                    .member_map = {} });
                auto& ref_block = ref.blocks.last_mut().unwrap().get_mut();
                (void)uniform_block_indices.insert(rstd::into(bind_name),
                                                   ref.blocks.len() - usize(1));

                vkbinding.binding         = b.binding;
                vkbinding.descriptorCount = b.count;
                vkbinding.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;

                for (const auto& unif : block.members) {
                    if (! unif.members.is_empty() || unif.array_dimensions.len() > usize(1)) {
                        rstd_error("nested uniform layouts are unsupported: {}", unif.name);
                        return false;
                    }
                    (void)ref_block.member_map.insert(unif.name.clone(),
                                                      ReflectBlockedUniform(unif, ref_block.index));
                }
            } else if (b.type == spirvto::DescriptorType::CombinedImageSampler ||
                       b.type == spirvto::DescriptorType::SampledImage) {
                // HLSL uses separate image/sampler variables at the same binding.
                vkbinding.binding         = b.binding;
                vkbinding.descriptorCount = b.count;
                vkbinding.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            } else {
                rstd_error("unknown DescriptorBinding {}", (int)b.type);
                return false;
            }

            (void)ref.binding_map.insert(rstd::into(bind_name),
                                         ShaderReflected::Binding {
                                             .set    = b.set,
                                             .layout = vkbinding,
                                         });
        }

        if (stage == VK_SHADER_STAGE_VERTEX_BIT) {
            for (const auto& input : reflected.inputs) {
                if (input.built_in) continue;
                if (! input.members.is_empty() || ! input.array_dimensions.is_empty() ||
                    input.numeric.matrix_columns != 0 || input.component != 0)
                    return false;

                if (input.location == u32::MAX.to_primitive()) {
                    rstd_error("shader input {} no location", input.name);
                    return false;
                }
                ShaderReflected::Input rinput;
                rinput.location = input.location;
                rinput.format   = ReflectedFormat(input.numeric);

                // Strip HLSL struct-prefixed names. glslang HLSL emits VS
                // inputs as `<entry_param_name>.<field>` (e.g. our wrapper's
                // `_ww_in.a_Position`). DXC used `in.var.<SEMANTIC>`. C++
                // vertex-buffer layout matches by bare attribute name
                // (`a_Position`), so strip everything up to the final `.`.
                if (rinput.format == VK_FORMAT_UNDEFINED) return false;
                auto name = input.name.as_str();
                if (auto split = name.rsplit_once("."_str)) name = split->template get<1>();
                (void)ref.input_location_map.insert(String::make(name), rstd::move(rinput));
            }
        }
    }
    spvs   = rstd::move(stages);
    output = rstd::move(ref);
    return true;
}

namespace
{

// Configure a TShader for our Vulkan target. The user shader has already
// gone through PreShaderHeader (combo `#define`s + GLSL prologue + the
// __SHADER_PLACEHOLD__ slot for synthesized layouts) so glslang sees
// fully resolved source.
void ConfigureShader(glslang::TShader& shader, SourceLang lang, VulkanTarget target,
                     const char* entry) {
    glslang::EShSource src_lang =
        (lang == SourceLang::Hlsl) ? glslang::EShSourceHlsl : glslang::EShSourceGlsl;
    shader.setEnvInput(src_lang, shader.getStage(), glslang::EShClientVulkan, 100);
    shader.setEnvClient(glslang::EShClientVulkan, ToClientVersion(target));
    shader.setEnvTarget(glslang::EShTargetSpv, ToSpvVersion(target));
    shader.setEntryPoint(entry);
    shader.setSourceEntryPoint(entry);
    // We emit explicit `layout(location=N)` and `layout(set=B, binding=K)`
    // ourselves from Finalprocessor, so glslang's auto-binding stays off.
    shader.setAutoMapLocations(false);
    shader.setAutoMapBindings(false);
    // Be lenient about combinations of #version / unset clip ranges in WE
    // shaders — they mostly look like GLSL 110 desktop with Vulkan semantics
    // grafted on top.
    shader.setEnvInputVulkanRulesRelaxed();
}

constexpr EShMessages kCompileMessages =
    static_cast<EShMessages>(EShMsgSpvRules | EShMsgVulkanRules | EShMsgRelaxedErrors |
                             EShMsgSuppressWarnings | EShMsgKeepUncalled);

} // namespace

bool owe::vulkan::Preprocess(ref<str> src, ShaderType stage, SourceLang lang, String& out) {
    EnsureGlslangProcess();
    glslang::TShader shader(ToEShLanguage(stage));
    const auto       source = src;
    if (source.len() > usize(i32::MAX.to_primitive())) return false;
    const char* data = reinterpret_cast<const char*>(source.data());
    const int   len  = static_cast<int>(source.len().to_primitive());
    const char* name = "ww";
    shader.setStringsWithLengthsAndNames(&data, &len, &name, 1);
    ConfigureShader(shader, lang, VulkanTarget::Vulkan_1_1, DefaultEntryName(lang, stage));

    const int                        default_version = 110;
    const EProfile                   profile         = ECoreProfile;
    const bool                       forward_compat  = false;
    glslang::TShader::ForbidIncluder includer;

    std::string preprocessed;
    bool        ok = shader.preprocess(GetDefaultResources(),
                                       default_version,
                                       profile,
                                       false,
                                       forward_compat,
                                       kCompileMessages,
                                       &preprocessed,
                                       includer);
    if (! ok) {
        auto tmp = logToTmpfileWithSha1(source);
        rstd_error("glslang(preprocess): {}", shader.getInfoLog());
        rstd_error("shader source is at {}", tmp.as_path().to_string_lossy());
        return false;
    }
    auto text = as_str(preprocessed);
    if (text.is_err()) return false;
    out = rstd::into(text.unwrap());
    return true;
}

bool owe::vulkan::CompileAndLinkShaderUnits(slice<ShaderCompUnit> compUnits,
                                            const ShaderCompOpt& opt, Vec<Uni_ShaderSpv>& spvs) {
    EnsureGlslangProcess();
    spvs.clear();
    spvs.reserve(compUnits.len());

    for (const auto& unit : compUnits) {
        auto entry_name =
            unit.entry_point.is_empty()
                ? CStr::from_ptr(DefaultEntryName(unit.lang, unit.stage)).to_str().unwrap()
                : unit.entry_point.as_str();
        auto encoded_entry = CString::make(String::make(entry_name));
        if (encoded_entry.is_err()) return false;
        auto        entry_c = rstd::move(encoded_entry).unwrap();
        const char* entry   = entry_c.as_ptr();

        glslang::TShader shader(ToEShLanguage(unit.stage));
        const auto       source = unit.src.as_str();
        if (source.len() > usize(i32::MAX.to_primitive())) return false;
        const char* data = reinterpret_cast<const char*>(source.data());
        const int   len  = static_cast<int>(source.len().to_primitive());
        const char* name = "ww";
        shader.setStringsWithLengthsAndNames(&data, &len, &name, 1);
        ConfigureShader(shader, unit.lang, opt.target, entry);

        const int                        default_version = 110;
        const EProfile                   profile         = ECoreProfile;
        const bool                       forward_compat  = false;
        glslang::TShader::ForbidIncluder includer;

        if (! shader.parse(GetDefaultResources(),
                           default_version,
                           profile,
                           false,
                           forward_compat,
                           kCompileMessages,
                           includer)) {
            auto tmp = logToTmpfileWithSha1(source);
            // Strip WARNING lines; EShMsgSuppressWarnings doesn't actually
            // omit them from the info log on this glslang build.
            auto   log       = CStr::from_ptr(shader.getInfoLog()).to_str();
            auto   remaining = log.unwrap_or(""_str);
            String filtered;
            while (! remaining.is_empty()) {
                auto split = remaining.split_once("\n"_str);
                auto line  = split ? split->template get<0>() : remaining;
                if (! line.contains("WARNING"_str)) {
                    filtered.push_str(line);
                    filtered.push_str("\n"_str);
                }
                remaining = split ? split->template get<1>() : ""_str;
            }
            if (log.is_ok())
                rstd_error("glslang(parse): {}", filtered);
            else
                rstd_error("glslang(parse): {}", shader.getInfoLog());
            if (const char* d = shader.getInfoDebugLog(); d && d[0])
                rstd_error("glslang(parse debug): {}", d);
            rstd_error("shader source is at {}", tmp.as_path().to_string_lossy());
            return false;
        }

        glslang::TProgram program;
        program.addShader(&shader);
        if (! program.link(kCompileMessages)) {
            auto tmp = logToTmpfileWithSha1(source);
            rstd_error("glslang(link): {}", program.getInfoLog());
            rstd_error("shader source is at {}", tmp.as_path().to_string_lossy());
            return false;
        }

        glslang::TIntermediate* intermediate = program.getIntermediate(ToEShLanguage(unit.stage));
        if (! intermediate) {
            rstd_error("glslang(intermediate): no intermediate for stage");
            return false;
        }

        glslang::SpvOptions spv_opts;
        spv_opts.validate          = true;
        spv_opts.generateDebugInfo = false;
        spv_opts.disableOptimizer  = ! opt.optimize;
        spv::SpvBuildLogger logger;

        auto spv         = Box<ShaderSpv>::make();
        spv->stage       = unit.stage;
        spv->entry_point = String::make(entry_name);
        std::vector<unsigned int> words;
        glslang::GlslangToSpv(*intermediate, words, &logger, &spv_opts);
        spv->spirv = ShaderCode::from(
            slice<rstd::uint32_t>::from_raw_parts(words.data(), usize(words.size())));

        if (auto msgs = logger.getAllMessages(); ! msgs.empty()) {
            rstd_warn("glslang(spv): {}", msgs);
        }
        if (spv->spirv.is_empty()) {
            rstd_error("glslang(spv): no SPIR-V output produced");
            return false;
        }

        if (rstd::env::var_os("WP_DUMP_SPIRV"_str).is_some()) {
            static Atomic<u64> dump_idx {};
            auto base = rstd::format("/tmp/ww_dump_{}_{}", dump_idx.fetch_add(u64(1)), entry_name);
            auto spv_path = PathBuf::from(rstd::format("{}.spv", base).as_str());
            auto src_path = PathBuf::from(rstd::format("{}.glsl", base).as_str());
            (void)rstd::fs::write(
                spv_path.as_path(),
                slice<u8>::from_raw_parts(reinterpret_cast<const rstd::byte*>(spv->spirv.data()),
                                          spv->spirv.len() * usize(sizeof(rstd::uint32_t))));
            (void)rstd::fs::write(src_path.as_path(), source.as_bytes());
            rstd_info("dumped SPIR-V + source: {}.{{spv,glsl}}", base);
        }

        spvs.push(rstd::move(spv));
    }

    return true;
}
