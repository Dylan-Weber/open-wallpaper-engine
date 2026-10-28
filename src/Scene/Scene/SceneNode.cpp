module;

module wescene.scene;
import eigen;
import rstd;

using namespace owe;
using namespace Eigen;

Matrix4d SceneNode::GetLocalTrans() const {
    Affine3d trans = Affine3d::Identity();
    trans.prescale(m_scale.cast<double>());

    // m_rotation is in radians. Static scene.json `angles` are already radians;
    // the JS scripting API uses degrees and converts at the boundary (Script.cpp
    // NodeSetAngles / the transform actuator), so everything stored here is rad.
    trans.prerotate(AngleAxis<double>(m_rotation.x(), Vector3d::UnitX())); // x
    trans.prerotate(AngleAxis<double>(m_rotation.y(), Vector3d::UnitY())); // y
    trans.prerotate(AngleAxis<double>(m_rotation.z(), Vector3d::UnitZ())); // z

    trans.pretranslate(m_translate.cast<double>());

    return m_local_frame * trans.matrix();
}

void SceneNode::RotateObjectSpace(const Vector3f& rotation) {
    const Quaternionf current = AngleAxisf(m_rotation.z(), Vector3f::UnitZ()) *
                                AngleAxisf(m_rotation.y(), Vector3f::UnitY()) *
                                AngleAxisf(m_rotation.x(), Vector3f::UnitX());
    const Quaternionf local   = AngleAxisf(rotation.z(), Vector3f::UnitZ()) *
                                AngleAxisf(rotation.y(), Vector3f::UnitY()) *
                                AngleAxisf(rotation.x(), Vector3f::UnitX());
    const Vector3f    zyx     = (current * local).toRotationMatrix().canonicalEulerAngles(2, 1, 0);
    SetRotation({ zyx.z(), zyx.y(), zyx.x() });
}

namespace
{
// The rotation of a transform without its scale (the first axis flipped for a mirrored one).
Matrix3d RotationOf(const Matrix3d& linear) {
    Matrix3d rotation;
    for (int axis = 0; axis < 3; ++axis) rotation.col(axis) = linear.col(axis).normalized();
    if (linear.determinant() < 0.0) rotation.col(0) = -rotation.col(0);
    return rotation;
}

// The angles GetLocalTrans turns by (z, then y, then x) for that rotation.
Vector3f AnglesOf(const Matrix3d& rotation) {
    const Vector3d zyx = rotation.canonicalEulerAngles(2, 1, 0);
    return Vector3d(zyx.z(), zyx.y(), zyx.x()).cast<float>();
}
} // namespace

bool SceneNode::SetParent(SceneNode* parent, bool keep_world) {
    UpdateTrans();
    const Matrix4d world = ModelTrans();
    if (! m_node.SetTransformParent(parent)) return false;
    if (! keep_world) return true;

    Matrix4d parent_world = Matrix4d::Identity();
    if (parent != nullptr) {
        parent->UpdateTrans();
        parent_world = parent->ModelTrans();
    }
    const Affine3d local((parent_world * m_local_frame).inverse() * world);
    const Matrix3d linear = local.linear();
    Vector3d       scale(linear.col(0).norm(), linear.col(1).norm(), linear.col(2).norm());
    if (linear.determinant() < 0.0) scale.x() = -scale.x();
    m_translate = local.translation().cast<float>();
    m_rotation  = AnglesOf(RotationOf(linear));
    m_scale     = scale.cast<float>();
    MarkTransDirty();
    return true;
}

void SceneNode::LookAt(const Vector3d& center, const Vector3d& up, bool yaw_only) {
    UpdateTrans();
    const Vector3d up_axis = up.normalized();
    Vector3d       forward = center - ModelTrans().block<3, 1>(0, 3);
    if (yaw_only) forward -= up_axis * forward.dot(up_axis);
    // looking along the up axis gives NaN, as in Wallpaper Engine
    forward /= forward.norm();
    Vector3d right = up_axis.cross(forward);
    right /= right.norm();
    Matrix3d turned;
    turned.col(0) = right;
    turned.col(1) = forward.cross(right);
    turned.col(2) = forward;

    // the angles are the node's own, after its parent's and its local frame's turns
    Matrix4d base = m_local_frame;
    if (auto* parent = Parent()) {
        parent->UpdateTrans();
        base = parent->ModelTrans() * m_local_frame;
    }
    m_rotation = AnglesOf(RotationOf(base.block<3, 3>(0, 0)).transpose() * turned);
    MarkTransDirty();
}

void SceneNode::UpdateTrans() { m_node.UpdateWorld(); }

void SceneNode::MarkTransDirty() { m_node.SetLocalMatrix(GetLocalTrans()); }

auto SceneNode::ChildIndex(const SceneNode& child) const -> Option<usize> {
    return m_node.ChildIndex(child);
}

bool SceneNode::MoveChild(SceneNode& child, usize index) { return m_node.MoveChild(child, index); }

SceneNode* SceneNode::FindByName(ref<str> name) {
    if (m_name.as_str() == name) return this;
    for (auto& child : GetChildren()) {
        if (auto* hit = child->FindByName(name)) return hit;
    }
    return nullptr;
}
