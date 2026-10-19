export module wescene.scene:uniform;
export import vrento;
import :id;
import :runtime;
import eigen;
import rstd;
import wescene.core;

using namespace rstd::prelude;
using rstd::collections::BTreeMap;
using rstd::collections::HashMap;
using rstd::sync::Arc;

export namespace owe
{

using vrento::UniformMatrixStorage;
using vrento::UniformScalarType;
using vrento::UniformValueKind;
using vrento::UniformValueLayout;
using vrento::UniformValueView;

class ShaderValue : public vrento::UniformValue {
public:
    using vrento::UniformValue::UniformValue;
    ShaderValue() = default;
    ShaderValue clone() const { return *this; }
    void        clone_from(const ShaderValue& other) { *this = other; }
    ShaderValue(vrento::UniformValue value): vrento::UniformValue(rstd::move(value)) {}

    static ShaderValue fromMatrix(const Eigen::Ref<const Eigen::MatrixXf>& mat) {
        return vrento::UniformValue::fromMatrixArray(mat.data(),
                                                     u32(static_cast<rstd::uint32_t>(mat.rows())),
                                                     u32(static_cast<rstd::uint32_t>(mat.cols())),
                                                     usize(1),
                                                     UniformMatrixStorage::ColumnMajor);
    }
    static ShaderValue fromMatrix(const Eigen::Ref<const Eigen::MatrixXd>& mat) {
        Eigen::MatrixXf converted = mat.cast<float>();
        return fromMatrix(converted);
    }
};

using UniformValue   = ShaderValue;
using ShaderValues   = BTreeMap<String, ShaderValue>;
using ShaderValueMap = ShaderValues;

class SceneShaderDefaults {
    struct Storage {
        explicit Storage(ShaderValues values): values(rstd::move(values)) {}
        const ShaderValues values;
    };

public:
    SceneShaderDefaults() = default;
    SceneShaderDefaults(SceneShaderDefaults&& other) noexcept: m_storage(other.m_storage.take()) {}
    auto operator=(SceneShaderDefaults&& other) noexcept -> SceneShaderDefaults& {
        if (this != &other) m_storage = other.m_storage.take();
        return *this;
    }

    static auto from(ShaderValues values) -> SceneShaderDefaults {
        if (values.is_empty()) return {};
        return SceneShaderDefaults(Arc<Storage>::make(rstd::move(values)));
    }
    auto Values() const -> const ShaderValues& {
        if (m_storage) return (**m_storage).values;
        static const ShaderValues empty;
        return empty;
    }
    auto clone() const -> SceneShaderDefaults {
        return m_storage ? SceneShaderDefaults(m_storage->clone()) : SceneShaderDefaults();
    }
    void clone_from(const SceneShaderDefaults& other) { *this = other.clone(); }
    auto get(ref<str> name) const { return Values().get(name); }
    auto iter() const { return Values().iter(); }
    auto len() const -> usize { return Values().len(); }
    bool is_empty() const { return Values().is_empty(); }
    bool contains_key(ref<str> name) const { return Values().contains_key(name); }

    friend bool operator==(const SceneShaderDefaults& lhs, const SceneShaderDefaults& rhs) {
        if (&lhs.Values() == &rhs.Values()) return true;
        if (lhs.len() != rhs.len()) return false;
        for (const auto& [name, value] : lhs.iter()) {
            auto other = rhs.get(name->as_str());
            if (! other || value->size() != (**other).size() ||
                value->View().layout != (**other).View().layout)
                return false;
            for (usize index {}; index < value->size(); ++index) {
                if (rstd::bit_cast<rstd::uint32_t>((*value)[index]) !=
                    rstd::bit_cast<rstd::uint32_t>((**other)[index]))
                    return false;
            }
        }
        return true;
    }

private:
    explicit SceneShaderDefaults(Arc<Storage> storage): m_storage(Some(rstd::move(storage))) {}
    Option<Arc<Storage>> m_storage;
};

using vrento::UniformBlockDefinition;
using vrento::UniformBlockScope;
using vrento::UniformError;
using vrento::UniformOutputId;
using vrento::UniformSourceAttachment;
using vrento::UniformSourceId;
using vrento::UniformValueShape;
using SceneRenderViewKind = vrento::RenderViewKind;
using vrento::UniformBindingLease;
using vrento::UniformBindingSink;
using vrento::UniformResourceView;
using vrento::UniformSource;
using vrento::UniformSourceRegistrar;
using vrento::UniformTextureView;
using vrento::UniformUpdateContext;
using vrento::UniformValueSink;

struct UniformAttachmentWriter {
    using Trait                  = UniformAttachmentWriter;
    static constexpr bool direct = false;

    template<typename Self, typename = void>
    struct Api {
        using Trait = UniformAttachmentWriter;

        bool AttachGlobal(UniformSourceId source, i32 priority = i32()) {
            return rstd::trait_call<0>(this, source, priority);
        }
        bool AttachNode(SceneNodeId node, UniformSourceId source, i32 priority = i32()) {
            return rstd::trait_call<1>(this, node, source, priority);
        }
    };

    template<typename T>
    using Funcs = TraitFuncs<&T::AttachGlobal, &T::AttachNode>;
};

struct UniformSourceCatalog {
    using Trait                  = UniformSourceCatalog;
    static constexpr bool direct = false;

    template<typename Self, typename = void>
    struct Api {
        using Trait = UniformSourceCatalog;

        auto Resolve(UniformSourceId id) const -> Option<ref<dyn<UniformSource>>> {
            return rstd::trait_call<0>(this, id);
        }
    };

    template<typename T>
    using Funcs = TraitFuncs<&T::Resolve>;
};

struct UniformAttachmentCatalog {
    using Trait                  = UniformAttachmentCatalog;
    static constexpr bool direct = false;

    template<typename Self, typename = void>
    struct Api {
        using Trait = UniformAttachmentCatalog;

        auto GlobalSources() const -> slice<UniformSourceAttachment> {
            return rstd::trait_call<0>(this);
        }
        auto NodeSources(SceneNodeId node) const -> slice<UniformSourceAttachment> {
            return rstd::trait_call<1>(this, node);
        }
    };

    template<typename T>
    using Funcs = TraitFuncs<&T::GlobalSources, &T::NodeSources>;
};

using SceneUniformRegistry = vrento::UniformRegistry<SceneNodeId>;

} // namespace owe
