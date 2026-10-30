module;

export module wescene.pkg.puppet:bone_physics;
import eigen;
import rstd;

using namespace rstd::prelude;

export namespace owe
{

// A bone's physics settings, from the bone_simulation JSON of its MDLS entry.
// Distances are in puppet units, angles in radians unless noted.
struct BoneSimulation {
    // t and r: whether it moves and whether it turns
    bool translation { false };
    bool rotation { false };
    // re: the rigid simulation, which has no springs; what an impulse gives the
    // bone stays (se is the spring one)
    bool rigid { false };

    // ts, tf and ti: how strongly it is pulled back to its place, how fast its
    // movement dies down, and how much of its place it keeps when its parent
    // moves (stored as 1 - ti / 100)
    float translation_stiffness { 200.0f };
    float translation_friction { 20.0f };
    float translation_inertia { 0.7f };
    // rs, rf and ri: the same for its turn (stiffness in degrees)
    float rotation_stiffness { 300.0f };
    float rotation_friction { 20.0f };
    float rotation_inertia { 0.7f };

    // m, ge and gd: its mass, and the gravity pulling it
    float           mass { 20.0f };
    bool            gravity { false };
    Eigen::Vector3f gravity_direction { 0.0f, -1.0f, 0.0f };

    // tp: its tip in its own space, which its turn follows
    Eigen::Vector3f tip { 0.0f, 0.0f, 0.0f };
    // tm: how far it may move from its place
    float max_distance { 2.0f };

    // la, lamin and lamax: the limits of its turn
    bool            limit_angles { false };
    Eigen::Vector3f angle_min { -3.14159265f, -3.14159265f, -3.14159265f };
    Eigen::Vector3f angle_max { 3.14159265f, 3.14159265f, 3.14159265f };
    // lt and ltmax: the most it may turn in a sixtieth of a second (degrees)
    bool  limit_torque { false };
    float max_torque { 100.0f };

    // tax..taz and rax..raz: the axes it may move along and turn about
    array<bool, 3> translation_axes { true, true, true };
    array<bool, 3> rotation_axes { true, true, true };

    // None when the bone has no physics: it neither moves nor turns, or it has
    // neither simulation.
    static auto Parse(ref<str> json) -> Option<BoneSimulation>;
};

// The spring physics Wallpaper Engine runs for puppet bones. Each frame, after the
// animation layers pose a bone, it is moved and turned from that pose by springs
// pulling it back, by gravity, and by how its parent carried it along.
class BonePhysics {
public:
    void Resize(usize bones);

    // Seconds since the last step, at most a tenth of a second; 0 when `time`
    // did not move forward (the frame is evaluated again, or playback restarted).
    auto Advance(double time) -> float;

    // The bone's local transform with its physics applied, given its parent's
    // world transform (with the parent's physics) and the pose the animation
    // gave it. Steps the bone's simulation by `dt`; with dt 0 it only applies
    // the current state. `scale` is the object's: the springs, gravity and
    // largest distance are in the scene's units, so a bone of an object scaled
    // 2 along y moves half as far along y in its own (puppet-physics-scale-axes).
    auto Apply(usize bone, const BoneSimulation&, const Eigen::Affine3f& parent,
               const Eigen::Affine3f& local, float dt,
               const Eigen::Vector3f& scale = Eigen::Vector3f::Ones()) -> Eigen::Affine3f;

    // Adds to the bone's movement (units a second) and turn (degrees a sixtieth
    // of a second), as applyBonePhysicsImpulse does.
    void Impulse(usize bone, const BoneSimulation&, const Eigen::Vector3f& directional,
                 const Eigen::Vector3f& angular);
    // Back to rest, as resetBonePhysicsSimulation does.
    void Reset(usize bone);

private:
    struct State {
        // The translation spring: how far the bone is from its place (in its
        // parent's space) and how fast it moves.
        Eigen::Vector3f offset { Eigen::Vector3f::Zero() };
        Eigen::Vector3f velocity { Eigen::Vector3f::Zero() };
        // The rotation spring: its turn from its pose, and its turn a sixtieth
        // of a second.
        Eigen::Vector3f    angles { Eigen::Vector3f::Zero() };
        Eigen::Quaternionf spin { Eigen::Quaternionf::Identity() };
        // Where its place and its tip were the step before.
        Eigen::Vector3f last_place { Eigen::Vector3f::Zero() };
        Eigen::Vector3f last_tip { Eigen::Vector3f::Zero() };
        bool            started { false };
        // What the physics adds to the pose.
        Eigen::Vector3f    position_offset { Eigen::Vector3f::Zero() };
        Eigen::Quaternionf rotation_offset { Eigen::Quaternionf::Identity() };
    };

    Vec<State>     m_states;
    Option<double> m_last_time;
};

} // namespace owe
