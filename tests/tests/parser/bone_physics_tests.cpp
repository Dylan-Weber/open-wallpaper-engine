#include <rstd/test/gtest.hpp>

import eigen;
import rstd.cppstd;
import wescene.pkg.puppet;

using namespace rstd::prelude;
using namespace rstd::literals;
using rstd::sync::Arc;

namespace
{

// The settings Wallpaper Engine's "bouncy rotation" preset writes for a bone. The
// expected places below were measured in Wallpaper Engine on a leg with these
// settings: a script printed the child bone's place every frame (the frames are
// the frame times that script read).
std::string Settings(bool rotation, bool translation) {
    const std::string r = rotation ? "true" : "false";
    const std::string t = translation ? "true" : "false";
    return R"({"a":"1 0 0","gd":"0 -1 0","ge":false,"ik":false,"la":false,)"
           R"("lamax":"3.14159 3.14159 3.14159","lamin":"-3.14159 -3.14159 -3.14159",)"
           R"("lt":false,"ltmax":100,"m":20,"preset":"bouncyrotation","r":)" +
           r + R"(,"rax":true,"ray":true,"raz":true,"re":false,"rf":20,"ri":30,"rs":300,)" +
           R"("s":0,"se":true,"t":)" + t +
           R"(,"tax":true,"tay":true,"taz":true,"tf":20,"ti":30,"tm":2.0,)" +
           R"("tp":"0.00000 -22.00000 0.00000","ts":300})";
}

owe::BoneSimulation Parse(const std::string& settings) {
    auto simulation = owe::BoneSimulation::Parse(rstd::cppstd::as_str(settings).unwrap());
    EXPECT_TRUE(simulation.is_some());
    return simulation.is_some() ? *simulation : owe::BoneSimulation {};
}

// A leg: the bone with physics at the origin, and its child 0.5 right of it and
// 22.5 below it. Returns where the child is in each frame, from the frame the
// impulse is given in.
std::vector<Eigen::Vector3f> Run(const owe::BoneSimulation& simulation,
                                 const std::vector<float>&  frames,
                                 const Eigen::Vector3f&     directional,
                                 const Eigen::Vector3f&     angular) {
    const Eigen::Affine3f child(Eigen::Translation3f(0.5f, -22.5f, 0.0f));
    owe::BonePhysics      physics;
    physics.Resize(usize(2));
    std::vector<Eigen::Vector3f> places;
    for (usize i {}; i < usize(frames.size()); ++i) {
        float dt = frames[i.to_primitive()];
        if (i == usize()) {
            physics.Impulse(usize(), simulation, directional, angular);
            dt = 0.0f;
        }
        const auto leg = physics.Apply(
            usize(), simulation, Eigen::Affine3f::Identity(), Eigen::Affine3f::Identity(), dt);
        places.push_back((leg * child).translation());
    }
    return places;
}

} // namespace

TEST(BoneSimulation, ReadsSpringSettings) {
    const auto rotation = Parse(Settings(true, false));
    EXPECT_TRUE(rotation.rotation);
    EXPECT_FALSE(rotation.translation);
    EXPECT_FALSE(rotation.rigid);
    EXPECT_FLOAT_EQ(rotation.rotation_stiffness, 300.0f);
    EXPECT_FLOAT_EQ(rotation.rotation_friction, 20.0f);
    // stored as how much of the place the bone keeps: 1 - ri / 100
    EXPECT_NEAR(rotation.rotation_inertia, 0.7f, 1e-6f);
    EXPECT_TRUE(rotation.tip.isApprox(Eigen::Vector3f(0.0f, -22.0f, 0.0f)));
    EXPECT_FLOAT_EQ(rotation.max_distance, 2.0f);
}

TEST(BoneSimulation, NeedsAMovementAndASimulation) {
    EXPECT_TRUE(owe::BoneSimulation::Parse(rstd::cppstd::as_str(Settings(false, false)).unwrap())
                    .is_none());
    EXPECT_TRUE(owe::BoneSimulation::Parse(""_str).is_none());
    EXPECT_TRUE(owe::BoneSimulation::Parse(R"({"tm":100.0,"tp":"1 2 0"})"_str).is_none());

    // the rigid simulation (re) instead of the spring one (se)
    std::string rigid = Settings(true, false);
    rigid.replace(rigid.find("\"re\":false"), 10, "\"re\":true");
    rigid.replace(rigid.find("\"se\":true"), 9, "\"se\":false");
    EXPECT_TRUE(Parse(rigid).rigid);
}

TEST(BonePhysics, AngularImpulseTurnsTheBoneAndTheSpringBringsItBack) {
    const std::vector<float> frames     = { 0.061f, 0.088f, 0.073f, 0.060f, 0.070f,
                                            0.058f, 0.092f, 0.038f, 0.040f, 0.049f };
    const auto               places     = Run(Parse(Settings(true, false)),
                                              frames,
                                              Eigen::Vector3f::Zero(),
                                              Eigen::Vector3f(0.0f, 0.0f, 10.0f));
    const float              measured[] = { 0.0f,   3.908f, 2.423f, 1.663f, 1.055f,
                                            0.734f, 0.381f, 0.305f, 0.223f, 0.149f };
    for (size_t i = 0; i < frames.size(); ++i) {
        EXPECT_NEAR(places[i].x() - 0.5f, measured[i], 0.03f) << "frame " << i;
    }
}

// A negative rotation stiffness (Gun Girl's gun bone has -5) is not none: it pushes the bone further from its pose, so a kick
// grows (puppet-physics-gun) where it would otherwise swing back and forth.
TEST(BonePhysics, ANegativeRotationStiffnessPushesTheBoneFurtherAway) {
    const std::vector<float> frames(40, 1.0f / 30.0f);
    std::string              text = Settings(true, false);
    text.replace(text.find("\"rs\":300"), 8, "\"rs\":-50");
    text.replace(text.find("\"rf\":20"), 7, "\"rf\":0");
    const auto away = Run(Parse(text), frames, Eigen::Vector3f::Zero(), Eigen::Vector3f(0.0f, 0.0f, 10.0f));
    // the tip points along -y, so its x is how far it has turned
    EXPECT_GT(std::abs(away[39].x() - 0.5f), 1.5f * std::abs(away[10].x() - 0.5f));
}

TEST(BonePhysics, DirectionalImpulseMovesTheBoneWhichSwingsBack) {
    const std::vector<float> frames = { 0.074f, 0.066f, 0.082f, 0.082f, 0.061f, 0.072f, 0.066f };
    const auto               places = Run(Parse(Settings(false, true)),
                                          frames,
                                          Eigen::Vector3f(10.0f, 0.0f, 0.0f),
                                          Eigen::Vector3f::Zero());
    const float measured[]          = { 0.0f, 0.662f, -0.670f, 0.678f, -0.082f, 0.045f, -0.014f };
    for (size_t i = 0; i < frames.size(); ++i) {
        EXPECT_NEAR(places[i].x() - 0.5f, measured[i], 0.01f) << "frame " << i;
    }
}

TEST(BonePhysics, BoneGoesNoFurtherThanItsLargestDistance) {
    const std::vector<float> frames     = { 0.093f, 0.081f, 0.072f, 0.092f, 0.055f };
    const auto               places     = Run(Parse(Settings(false, true)),
                                              frames,
                                              Eigen::Vector3f(100.0f, 0.0f, 0.0f),
                                              Eigen::Vector3f::Zero());
    const float              measured[] = { 0.0f, 2.000f, -1.108f, 1.703f, 0.157f };
    for (size_t i = 0; i < frames.size(); ++i) {
        EXPECT_NEAR(places[i].x() - 0.5f, measured[i], 0.01f) << "frame " << i;
    }
}

TEST(BonePhysics, RigidBoneKeepsWhatAnImpulseGivesIt) {
    auto simulation  = Parse(Settings(true, true));
    simulation.rigid = true;
    // turned 10 degrees and left there
    const auto turned = Run(simulation,
                            { 0.067f, 0.087f, 0.062f, 0.075f, 0.078f },
                            Eigen::Vector3f::Zero(),
                            Eigen::Vector3f(0.0f, 0.0f, 10.0f));
    for (size_t i = 1; i < turned.size(); ++i) {
        EXPECT_NEAR(turned[i].x() - 0.5f, 3.908f, 0.01f) << "frame " << i;
    }
    // moved for one frame (dt x 10) and left there
    simulation.rotation = false;
    const auto moved    = Run(simulation,
                              { 0.089f, 0.088f, 0.062f, 0.067f },
                              Eigen::Vector3f(10.0f, 0.0f, 0.0f),
                              Eigen::Vector3f::Zero());
    for (size_t i = 1; i < moved.size(); ++i) {
        EXPECT_NEAR(moved[i].x() - 0.5f, 0.879f, 0.01f) << "frame " << i;
    }
}

TEST(BonePhysics, RestingBoneStaysWhereItIsPosed) {
    const auto       simulation = Parse(Settings(true, true));
    owe::BonePhysics physics;
    physics.Resize(usize(1));
    const Eigen::Affine3f posed(Eigen::Translation3f(3.0f, 4.0f, 0.0f));
    for (int frame = 0; frame < 10; ++frame) {
        EXPECT_TRUE(physics.Apply(usize(), simulation, Eigen::Affine3f::Identity(), posed, 0.016f)
                        .matrix()
                        .isApprox(posed.matrix()));
    }
}

namespace
{
// A bone with only its rotation spring (the tip 62.5 along x, mass `mass`) under gravity `direction`, left to settle for
// 4 seconds at 30 frames a second: how far it has turned (radians, 0 when it hasn't).
float SettledTurn(float mass, const std::string& direction, bool gravity = true) {
    std::string settings = Settings(true, false);
    const auto  replace  = [&](const std::string& from, const std::string& to) {
        const auto at = settings.find(from);
        ASSERT_NE(at, std::string::npos) << from;
        settings.replace(at, from.size(), to);
    };
    replace("\"ge\":false", gravity ? "\"ge\":true" : "\"ge\":false");
    replace("\"gd\":\"0 -1 0\"", "\"gd\":\"" + direction + "\"");
    replace("\"m\":20", "\"m\":" + std::to_string(mass));
    replace("\"rs\":300", "\"rs\":250");
    replace("\"rf\":20", "\"rf\":35");
    replace("\"tp\":\"0.00000 -22.00000 0.00000\"", "\"tp\":\"62.5 0 0\"");
    const auto       simulation = Parse(settings);
    owe::BonePhysics physics;
    physics.Resize(usize(1));
    Eigen::Affine3f turned = Eigen::Affine3f::Identity();
    for (int frame = 0; frame < 120; ++frame) {
        turned = physics.Apply(
            usize(), simulation, Eigen::Affine3f::Identity(), Eigen::Affine3f::Identity(), 1.0f / 30.0f);
    }
    return Eigen::AngleAxisf(Eigen::Quaternionf(turned.rotation())).angle();
}
} // namespace

// In Wallpaper Engine gravity turns a bone whose rotation spring is on (a puppet's hair, gun and hands hang from it): the tip
// is moved against gravity's part across it by the mass (no frame time) and turned back, blended in by min(how far it moved x the
// inertia x pi / 180, 1), so the turn goes with the mass squared, and a turn below the turn-between routine's threshold
// (the directions' dot product at least 0.99999988) is none (puppet-physics-hang2 and -locked: 0.9946 to 1.0000;
// the factor 10 on the pull is fitted, scores flat from 10 to 16).
TEST(BonePhysics, GravityTurnsABoneWithARotationSpring) {
    EXPECT_GT(SettledTurn(100.0f, "0 -1 0"), 0.5f);
    EXPECT_GT(SettledTurn(20.0f, "0 -1 0"), 0.03f);
    EXPECT_GT(SettledTurn(4.0f, "0 -1 0"), 3.0f * SettledTurn(2.0f, "0 -1 0"));
}

TEST(BonePhysics, ATinyGravityTurnsAlmostNothing) {
    EXPECT_LT(SettledTurn(1.0f, "0 -1 0"), 0.01f);
}

TEST(BonePhysics, NothingTurnsABoneWithoutGravityOrAlongItsTip) {
    EXPECT_NEAR(SettledTurn(100.0f, "0 -1 0", false), 0.0f, 1e-6f);
    EXPECT_NEAR(SettledTurn(100.0f, "1 0 0"), 0.0f, 1e-4f);
}

TEST(Puppet, GravitySettlesABoneBelowItsPlace) {
    auto  puppet  = Arc<owe::Puppet>::make();
    auto& hanging = puppet->bones.emplace_back();
    // pulled down by m x 1000 and back by ts: at rest 20 x 1000 / 300 below its place
    std::string settings = Settings(false, true);
    settings.replace(settings.find("\"ge\":false"), 10, "\"ge\":true");
    settings.replace(settings.find("\"tm\":2.0"), 8, "\"tm\":100");
    hanging.simulation_json = String::make(rstd::cppstd::as_str(settings).unwrap());
    puppet->prepared();
    ASSERT_TRUE(puppet->bones[usize()].simulation.is_some());

    owe::PuppetLayer layer(puppet.clone());
    layer.prepared({});
    double time = 0.0;
    for (int frame = 0; frame < 600; ++frame, time += 1.0 / 60.0) {
        (void)layer.genFrame(time);
    }
    // asking again in the same frame leaves the bone where it is
    const auto settled = layer.boneTransform(1, time - 1.0 / 60.0)->translation();
    EXPECT_NEAR(settled.y(), -20.0f * 1000.0f / 300.0f, 0.1f);
    EXPECT_NEAR(settled.x(), 0.0f, 1e-4f);
    EXPECT_TRUE(layer.boneTransform(1, time - 1.0 / 60.0)->translation().isApprox(settled));
}

TEST(Puppet, LayerImpulseMovesTheBoneAndResetPutsItBack) {
    auto  puppet        = Arc<owe::Puppet>::make();
    auto& leg           = puppet->bones.emplace_back();
    leg.name            = String::make("leg"_str);
    leg.simulation_json = String::make(rstd::cppstd::as_str(Settings(false, true)).unwrap());
    puppet->prepared();

    owe::PuppetLayer layer(puppet.clone());
    layer.prepared({});
    (void)layer.genFrame(0.0);
    // bone 1 as boneIndex numbers it; None would be every bone with physics
    layer.ApplyBoneImpulse(
        Some(rstd::uint32_t(1)), Eigen::Vector3f(10.0f, 0.0f, 0.0f), Eigen::Vector3f::Zero());
    (void)layer.genFrame(0.066);
    EXPECT_GT(layer.boneTransform(1, 0.066)->translation().x(), 0.1f);

    layer.ResetBonePhysics(None());
    (void)layer.genFrame(0.132);
    EXPECT_NEAR(layer.boneTransform(1, 0.132)->translation().x(), 0.0f, 1e-5f);
}
