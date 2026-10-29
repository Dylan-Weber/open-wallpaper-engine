module;

#include <cmath>

module wescene.pkg.puppet;
import eigen;
import wescene.json;
import rstd;

using namespace owe;
using namespace Eigen;
using namespace rstd::prelude;
using namespace rstd::literals;

namespace
{

constexpr float kDegrees = 3.14159265358979f / 180.0f;
// The longest step: a longer pause (a hidden or paused wallpaper) is taken as
// this, so the springs do not overshoot.
constexpr float kLongestStep = 0.1f;

// Turned about x, then y, then z, as the animations are.
Quaternionf TurnOf(const Vector3f& angles) {
    return AngleAxisf(angles.z(), Vector3f::UnitZ()) * AngleAxisf(angles.y(), Vector3f::UnitY()) *
           AngleAxisf(angles.x(), Vector3f::UnitX());
}

// The angles TurnOf takes, each between -pi and pi.
Vector3f AnglesOf(const Quaternionf& turn) {
    const Matrix3f m = turn.toRotationMatrix();
    const float    z = std::atan2(m(1, 0), m(0, 0));
    const float    s = std::sin(z);
    const float    c = std::cos(z);
    const float    y = std::atan2(-m(2, 0), m(0, 0) * c + m(1, 0) * s);
    const float    x = std::atan2(s * m(0, 2) - c * m(1, 2), c * m(1, 1) - s * m(0, 1));
    return { x, y, z };
}

// Between -pi and pi.
float Wrap(float angle) {
    constexpr float pi = 3.14159265358979f;
    return angle >= 0.0f ? std::fmod(angle + pi, 2.0f * pi) - pi
                         : std::fmod(angle - pi, 2.0f * pi) + pi;
}

// The shorter turn taking direction `from` to `to`.
// Directions whose dot product is at least 0.99999988 (Wallpaper Engine's constant, 1 - 2^-23) turn not at all.
Quaternionf TurnBetween(const Vector3f& from, const Vector3f& to) {
    if (from.dot(to) >= 0.99999988f) return Quaternionf::Identity();
    return Quaternionf::FromTwoVectors(from, to);
}

Quaternionf Towards(const Quaternionf& from, const Quaternionf& to, float amount) {
    return from.slerp(rstd::cmp::min(rstd::cmp::max(amount, 0.0f), 1.0f), to).normalized();
}

// A negative amount turns away from `to`: a negative rotation stiffness pushes the bone further from its pose
// (puppet-physics-gun: Gun Girl's gun bone has -5).
Quaternionf TowardsOrAway(const Quaternionf& from, const Quaternionf& to, float amount) {
    return from.slerp(rstd::cmp::min(amount, 1.0f), to).normalized();
}

Vector3f ReadVector(const Json& json, ref<str> key, const Vector3f& fallback) {
    array<float, 3> value {};
    if (json.get(key).is_none() || ! GetJsonValue(json, key, value, false)) return fallback;
    return { value[usize(0)], value[usize(1)], value[usize(2)] };
}

} // namespace

auto BoneSimulation::Parse(ref<str> text) -> Option<BoneSimulation> {
    auto parsed = ParseJson(text);
    if (parsed.is_err()) return None();
    const auto json = parsed.unwrap();
    if (json.as_object().is_none()) return None();

    const auto flag = [&](ref<str> key, bool fallback) {
        bool value = fallback;
        if (json.get(key).is_some()) GetJsonValue(json, key, value, false);
        return value;
    };
    const auto number = [&](ref<str> key, float fallback) {
        float value = fallback;
        if (json.get(key).is_some()) GetJsonValue(json, key, value, false);
        return value;
    };

    BoneSimulation simulation;
    simulation.translation = flag("t"_str, false);
    simulation.rotation    = flag("r"_str, false);
    simulation.rigid       = flag("re"_str, false);
    if (! (simulation.translation || simulation.rotation) ||
        ! (flag("se"_str, false) || simulation.rigid))
        return None();

    simulation.translation_stiffness = number("ts"_str, simulation.translation_stiffness);
    simulation.translation_friction  = number("tf"_str, simulation.translation_friction);
    simulation.translation_inertia   = 1.0f - number("ti"_str, 30.0f) / 100.0f;
    simulation.rotation_stiffness    = number("rs"_str, simulation.rotation_stiffness);
    simulation.rotation_friction     = number("rf"_str, simulation.rotation_friction);
    simulation.rotation_inertia      = 1.0f - number("ri"_str, 30.0f) / 100.0f;
    simulation.mass                  = number("m"_str, simulation.mass);
    simulation.gravity               = flag("ge"_str, false);
    simulation.gravity_direction     = ReadVector(json, "gd"_str, simulation.gravity_direction);
    simulation.tip                   = ReadVector(json, "tp"_str, simulation.tip);
    simulation.max_distance          = number("tm"_str, simulation.max_distance);
    simulation.limit_angles          = flag("la"_str, false);
    simulation.angle_min             = ReadVector(json, "lamin"_str, simulation.angle_min);
    simulation.angle_max             = ReadVector(json, "lamax"_str, simulation.angle_max);
    simulation.limit_torque          = flag("lt"_str, false);
    simulation.max_torque            = number("ltmax"_str, simulation.max_torque);
    simulation.translation_axes      = { flag("tax"_str, true),
                                         flag("tay"_str, true),
                                         flag("taz"_str, true) };
    simulation.rotation_axes         = { flag("rax"_str, true),
                                         flag("ray"_str, true),
                                         flag("raz"_str, true) };
    return Some(rstd::move(simulation));
}

void BonePhysics::Resize(usize bones) {
    if (m_states.len() != bones) {
        m_states.clear();
        for (usize i {}; i < bones; ++i) m_states.push(State {});
    }
}

auto BonePhysics::Advance(double time) -> float {
    const auto last = m_last_time;
    m_last_time     = Some(double(time));
    if (last.is_none() || time <= *last) return 0.0f;
    return rstd::cmp::min(float(time - *last), kLongestStep);
}

auto BonePhysics::Apply(usize bone, const BoneSimulation& simulation, const Affine3f& parent,
                        const Affine3f& local, float dt) -> Affine3f {
    auto& state = m_states[bone];
    if (dt > 0.0f) {
        // where the animation (and the parents' physics) put the bone and its tip
        const Affine3f animated = parent * local;
        const Vector3f place    = animated.translation();
        const Vector3f tip      = animated * simulation.tip;
        if (! state.started) {
            state.last_place = place;
            state.last_tip   = tip;
            state.started    = true;
        }

        if (simulation.translation) {
            // It keeps (inertia) some of where it was as its place moves, is
            // pulled by gravity and back by its spring, along the axes it may
            // move along, no further than its largest distance, and its
            // movement dies down.
            state.offset -= (place - state.last_place) * simulation.translation_inertia;
            if (simulation.gravity) {
                state.velocity += simulation.mass * dt * 1000.0f * simulation.gravity_direction;
            }
            if (! simulation.rigid) {
                state.velocity -= dt * simulation.translation_stiffness * state.offset;
            }
            state.offset += dt * state.velocity;
            for (int axis = 0; axis < 3; ++axis) {
                if (simulation.translation_axes[usize(axis)]) continue;
                const Vector3f direction = parent.linear().col(axis).normalized();
                state.offset -= state.offset.dot(direction) * direction;
            }
            if (const float distance = state.offset.norm(); distance > simulation.max_distance) {
                state.offset *= simulation.max_distance / distance;
            }
            state.velocity -=
                state.velocity * rstd::cmp::min(dt * simulation.translation_friction, 1.0f);
            state.position_offset = parent.linear().inverse() * state.offset;
        }

        if (simulation.rotation) {
            // It turns back (inertia) towards where its tip pointed as the tip is
            // carried along, is turned back to its pose by its spring, turns by
            // its spin within its limits, and its spin dies down.
            const Vector3f last_direction = state.last_tip - state.last_place;
            const Vector3f direction      = tip - place;
            const float    moved          = (tip - state.last_tip).norm();
            if (moved > 0.0f && direction.norm() > 1e-6f && last_direction.norm() > 1e-6f) {
                const Quaternionf back =
                    TurnBetween(direction.normalized(), last_direction.normalized());
                const Quaternionf turn(animated.rotation());
                const Quaternionf own     = turn.conjugate() * back * turn;
                const float       inertia = moved * simulation.rotation_inertia * kDegrees;

                state.spin = state.spin * Towards(Quaternionf::Identity(), own, inertia);
            }
            if (simulation.gravity) {
                // Gravity turns the bone: its tip, as turned now, is moved against the part of gravity across it (the mass,
                // times the object scale: no frame time) and turned back, blended in by how far it moved x the inertia x
                // pi / 180, so the turn goes with the mass squared and a tiny one is none (see TurnBetween).
                const Vector3f turned = state.rotation_offset * simulation.tip;
                if (turned.norm() > 1e-6f) {
                    const Vector3f along = turned.normalized();
                    const Vector3f pull =
                        animated.linear().inverse() * simulation.gravity_direction * simulation.mass;
                    const Vector3f across = pull - pull.dot(along) * along;
                    const float    moved  = across.norm();
                    if (moved > 0.0f) {
                        const Vector3f    moved_tip = turned - across;
                        const Quaternionf back      = TurnBetween(moved_tip.normalized(), along);
                        state.spin                  = state.spin * Towards(Quaternionf::Identity(),
                                                          back,
                                                          moved * simulation.rotation_inertia * kDegrees);
                    }
                }
            }
            if (! simulation.rigid) {
                const Quaternionf pose   = TurnOf(-state.angles);
                const float       spring = simulation.rotation_stiffness * kDegrees * dt;

                state.spin = state.spin * TowardsOrAway(Quaternionf::Identity(), pose, spring);
            }
            if (simulation.limit_torque) {
                const Vector3f most = Vector3f::Constant(simulation.max_torque * kDegrees);
                state.spin          = TurnOf(AnglesOf(state.spin).cwiseMax(-most).cwiseMin(most));
            }
            const Quaternionf applied = Towards(Quaternionf::Identity(), state.spin, dt * 60.0f);
            Vector3f          angles  = AnglesOf(applied * TurnOf(state.angles));
            for (int axis = 0; axis < 3; ++axis) {
                angles[axis] = simulation.rotation_axes[usize(axis)] ? Wrap(angles[axis]) : 0.0f;
            }
            if (simulation.limit_angles) {
                const Vector3f limited =
                    angles.cwiseMax(simulation.angle_min).cwiseMin(simulation.angle_max);
                state.spin = TurnOf(angles - limited).conjugate() * state.spin;
                angles     = limited;
            }
            const float friction  = dt * simulation.rotation_friction;
            state.angles          = angles;
            state.spin            = Towards(state.spin, Quaternionf::Identity(), friction);
            state.rotation_offset = TurnOf(state.angles);
        }

        state.last_place = place;
        state.last_tip   = tip;
    }
    return Translation3f(state.position_offset) * local * state.rotation_offset;
}

void BonePhysics::Impulse(usize bone, const BoneSimulation& simulation, const Vector3f& directional,
                          const Vector3f& angular) {
    auto& state = m_states[bone];
    if (simulation.translation) state.velocity += directional;
    if (simulation.rotation) state.spin = state.spin * TurnOf(angular * kDegrees);
}

void BonePhysics::Reset(usize bone) { m_states[bone] = State {}; }
