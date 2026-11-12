module;

#include <rstd/macro.hpp>
module wescene.scene;
import eigen;
import rstd;
import rstd.log;
import wescene.utils;

using rstd::f64;

using namespace owe;
using namespace Eigen;
using namespace rstd::prelude;
using namespace rstd::literals;

namespace
{
Matrix4d NodeCameraFrame(SceneNode& node) {
    node.UpdateTrans();

    Matrix4d frame = node.ModelTrans();
    if (! frame.allFinite()) return Matrix4d::Identity();

    constexpr double kAxisEps = 1e-10;
    // a layer scaled to no area still has its frame: a collapsed x or y axis counts as scale 1 (the
    // sky's gradient layer is 0 x 0)
    for (int column = 0; column < 2; ++column) {
        if (frame.block<3, 1>(0, column).squaredNorm() <= kAxisEps)
            frame.block<3, 1>(0, column) = Vector3d::Unit(column);
    }
    Vector3d x = frame.block<3, 1>(0, 0);
    Vector3d y = frame.block<3, 1>(0, 1);
    Vector3d z = frame.block<3, 1>(0, 2);

    if (! z.allFinite() || z.squaredNorm() <= kAxisEps) {
        z = x.cross(y);
        if (! z.allFinite() || z.squaredNorm() <= kAxisEps) z = Vector3d::UnitZ();
        frame.block<3, 1>(0, 2) = z.normalized();
    }

    if (! frame.allFinite() || f64(frame.determinant()).abs().to_primitive() <= kAxisEps)
        return Matrix4d::Identity();
    return frame;
}
} // namespace

Vector3d SceneCamera::GetPosition(SceneRenderViewKind view) const {
    return RenderTransforms(view).eye;
}

Vector3d SceneCamera::GetDirection() const {
    const auto transforms = Transforms();
    return (transforms.center - transforms.eye).normalized();
}

auto SceneCamera::Transforms() const -> SceneCameraTransforms {
    if (m_lookat) return SceneCameraTransforms { .eye = m_eye, .center = m_center, .up = m_up };
    if (m_node) {
        const Matrix4d frame = NodeCameraFrame(*m_node);
        const Vector3d eye   = frame.block<3, 1>(0, 3);
        return SceneCameraTransforms {
            .eye    = eye,
            .center = eye - frame.block<3, 1>(0, 2),
            .up     = frame.block<3, 1>(0, 1),
        };
    }
    return {};
}

auto SceneCamera::RenderTransforms(SceneRenderViewKind view) const -> SceneCameraTransforms {
    auto transforms = Transforms();
    transforms.eye += m_view_offset;
    transforms.center += m_view_offset;
    if (view == SceneRenderViewKind::Reflection) {
        // the mirror image in the plane y 0: the up too (only a tilted up changes)
        transforms.eye.y()    = -transforms.eye.y();
        transforms.center.y() = -transforms.center.y();
        transforms.up.x()     = -transforms.up.x();
        transforms.up.z()     = -transforms.up.z();
    }
    return transforms;
}

bool SceneCamera::SetTransforms(const SceneCameraTransforms& transforms) {
    const Vector3d direction = transforms.center - transforms.eye;
    if (! transforms.eye.allFinite() || ! transforms.center.allFinite() ||
        ! transforms.up.allFinite() || direction.squaredNorm() <= 1e-20 ||
        transforms.up.squaredNorm() <= 1e-20 ||
        direction.cross(transforms.up).squaredNorm() <= 1e-20)
        return false;
    SetLookAt(transforms.eye, transforms.center, transforms.up);
    Update();
    return true;
}

void SceneCamera::FitFovToHeightAtCenter(double height) {
    if (! m_lookat || m_fit_height.is_none() || *m_fit_height == height) return;
    const double distance = (m_center - m_eye).norm();
    if (! (distance > 0.0) || ! (height > 0.0)) return;
    m_fov        = algorism::CalculatePersperctiveFov(distance, height);
    m_fit_height = Some(height);
}

auto SceneCamera::AuthoredTransforms() const -> SceneCameraTransforms {
    auto transforms = Transforms();
    if (m_perspective || ! m_node) return transforms;

    const Matrix4d frame_inverse = NodeCameraFrame(*m_node).inverse();
    transforms.eye               = (frame_inverse * transforms.eye.homogeneous()).head<3>();
    transforms.center            = (frame_inverse * transforms.center.homogeneous()).head<3>();
    transforms.up                = frame_inverse.block<3, 3>(0, 0) * transforms.up;
    return transforms;
}

bool SceneCamera::SetAuthoredTransforms(const SceneCameraTransforms& transforms) {
    if (m_perspective || ! m_node) return SetTransforms(transforms);

    const Matrix4d frame = NodeCameraFrame(*m_node);
    return SetTransforms(SceneCameraTransforms {
        .eye    = (frame * transforms.eye.homogeneous()).head<3>(),
        .center = (frame * transforms.center.homogeneous()).head<3>(),
        .up     = frame.block<3, 3>(0, 0) * transforms.up,
    });
}

Matrix4d SceneCamera::GetViewMatrix() {
    CalculateViewProjectionMatrix();
    return m_camera.View();
}

Matrix4d SceneCamera::GetViewProjectionMatrix(SceneRenderViewKind view) {
    if (view == SceneRenderViewKind::Reflection) return CalculateReflectionViewProjectionMatrix();
    CalculateViewProjectionMatrix();
    return m_camera.ViewProjection();
}

auto SceneCamera::CameraSnapshot(SceneRenderViewKind view) -> vrento::CameraSnapshot<Matrix4d> {
    if (view == SceneRenderViewKind::Reflection) {
        CalculateReflectionViewProjectionMatrix();
        return m_reflection_camera.Snapshot();
    }
    CalculateViewProjectionMatrix();
    return m_camera.Snapshot();
}

Matrix4d SceneCamera::CalculateReflectionViewProjectionMatrix() {
    const auto transforms = RenderTransforms(SceneRenderViewKind::Reflection);
    // WE preserves camera-up so the reflection texture remains screen-upright.

    const Matrix4d view = LookAt(transforms.eye, transforms.center, transforms.up);
    m_reflection_camera.Set(view, ProjectionMatrix());
    return m_reflection_camera.ViewProjection();
}

Matrix4d SceneCamera::ProjectionMatrix() const {
    if (m_perspective) {
        return Perspective(Radians(m_fov), m_aspect, m_nearClip, m_farClip);
    }
    return Ortho(
        -m_width / 2.0, m_width / 2.0, -m_height / 2.0, m_height / 2.0, m_nearClip, m_farClip);
}

auto SceneCamera::MakeViewKey() const -> ViewKey {
    ViewKey key;
    key.width       = m_width;
    key.height      = m_height;
    key.aspect      = m_aspect;
    key.near_clip   = m_nearClip;
    key.far_clip    = m_farClip;
    key.fov         = m_fov;
    key.eye         = m_eye;
    key.center      = m_center;
    key.up          = m_up;
    key.offset      = m_view_offset;
    key.perspective = m_perspective;
    key.lookat      = m_lookat;
    if (! m_lookat && m_node != nullptr) {
        m_node->UpdateTrans();
        key.node     = m_node;
        key.revision = m_node->TransRevision();
    }
    return key;
}

bool SceneCamera::SameViewKey(const ViewKey& a, const ViewKey& b) {
    auto same = [](double x, double y) { return x == y || (x != x && y != y); };
    return same(a.width, b.width) && same(a.height, b.height) && same(a.aspect, b.aspect) &&
           same(a.near_clip, b.near_clip) && same(a.far_clip, b.far_clip) && same(a.fov, b.fov) &&
           a.eye == b.eye && a.center == b.center && a.up == b.up && a.offset == b.offset &&
           a.node == b.node && a.revision == b.revision && a.perspective == b.perspective &&
           a.lookat == b.lookat;
}

void SceneCamera::CalculateViewProjectionMatrix() {
    const auto key = MakeViewKey();
    if (m_view_key_valid && SameViewKey(key, m_view_key)) return;
    Matrix4d view;
    if (m_lookat) {
        view = LookAt(m_eye, m_center, m_up);
    } else if (m_node) {
        // view = inv(node.ModelTrans()) so the layer-local frame maps to
        // view origin regardless of where the node sits in the world (parent
        // chain + local translate / scale / rotate). With LookAt-only the
        // node's local scale would leak into clip space and a 9× scaled
        // layer would only see 1/9 of its quad inside the ortho viewport.
        view = NodeCameraFrame(*m_node).inverse();
    } else
        view = Matrix4d::Identity();
    view = view * Affine3d(Translation3d(-m_view_offset)).matrix();
    m_camera.Set(view, ProjectionMatrix());
    m_view_key       = key;
    m_view_key_valid = true;
}

void SceneCamera::Update() { CalculateViewProjectionMatrix(); }

void Scene::UpdateCameraFillMode(u32 width, u32 height, FillMode fillmode) {
    if (width == u32() || height == u32()) return;
    auto         extent = OrthographicProjectionExtent();
    const double sw = extent[usize()], sh = extent[usize(1)];
    if (sw <= 0.0 || sh <= 0.0) return;
    const double output_aspect = rstd::as_cast<double>(width) / rstd::as_cast<double>(height);
    const double scene_aspect  = sw / sh;
    auto         global        = CameraMut("global"_str);
    auto         perspective   = CameraMut("global_perspective"_str);
    if (global.is_none() || perspective.is_none()) return;
    auto& gcam = **global;
    auto& pcam = **perspective;

    switch (fillmode) {
    case FillMode::STRETCH:
        gcam.SetWidth(sw);
        gcam.SetHeight(sh);
        pcam.SetAspect(scene_aspect);
        break;
    case FillMode::ASPECTFIT:
        if (output_aspect < scene_aspect) {
            gcam.SetWidth(sw);
            gcam.SetHeight(sw / output_aspect);
        } else {
            gcam.SetWidth(sh * output_aspect);
            gcam.SetHeight(sh);
        }
        pcam.SetAspect(output_aspect);
        break;
    case FillMode::ASPECTCROP:
    default:
        if (output_aspect > scene_aspect) {
            gcam.SetWidth(sw);
            gcam.SetHeight(sw / output_aspect);
        } else {
            gcam.SetWidth(sh * output_aspect);
            gcam.SetHeight(sh);
        }
        pcam.SetAspect(output_aspect);
        break;
    }
    if (pcam.FovPolicy() == SceneCameraFovPolicy::OrthographicProjection) {
        pcam.SetFov(algorism::CalculatePersperctiveFov(1000.0f, gcam.Height()));
    } else {
        // An orthographic scene that overrides the perspective fov looks from a distance: its
        // perspective camera follows the cropped projection at its look-at distance. Other cameras
        // keep their fov.
        pcam.FitFovToHeightAtCenter(gcam.Height());
    }
    gcam.Update();
    pcam.Update();
    UpdateLinkedCamera("global"_str);
    CaptureCameraPathViewports();
}

void SceneCamera::AttatchNode(SceneNode* node) {
    if (! node) {
        rstd_error("Attach a null node to camera");
        return;
    }
    if (m_owned_node.is_some() && (*m_owned_node).as_ptr().as_raw_ptr() != node)
        m_owned_node = None();
    m_node   = node;
    m_lookat = false; // node-based view takes over from any explicit LookAt
}

void SceneCamera::AttachOwnedNode(Arc<SceneNode> node) {
    m_node       = node.as_ptr().as_raw_ptr();
    m_owned_node = Some(rstd::move(node));
    m_lookat     = false;
}
