// scene.json parse regression net.
//
// For each observed PKGV version, every matching workshop is re-opened,
// scene.json is read via VFS, and parsed via the
// canonical version-bearing SceneMetadata::FromJson(json, pkg_version) path.
// Establishes the baseline for the upcoming SceneMetadata refactor that splits
// FromJson by version: a regression here means the refactor changed
// what was previously parseable.
//
// Deliberately does not depend on Corpus / DumpWorkshop — those exercise
// MdlParser / TexImageParser too, which can hit unrelated assertions
// on rare .mdl inputs and would mask scene.json regressions.

#include <rstd/test/gtest.hpp>

#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>

import rstd.cppstd;
import rstd;
import eigen;
import wavsen.audio;
import wescene.fs;
import wescene.json;
import wescene.pkg.parse;
import wescene.pkg_fs;
import wescene.pkg.scene_obj;
import wescene.scene;
import wescene.scene_user_property;
import wescene.script;
import wescene.spec_names;
import wescene.testing.scene_parse_probe;
import wescene.types;

using namespace rstd::literals;

TEST(FieldBindingJson, CompatibilityReaderPopulatesAnimationMetadata) {
    auto parsed = owe::ParseJson(R"({"enabled":true,"x":{"value":1.5},"y":-2.0,"magic":7})"_str);
    ASSERT_TRUE(parsed.is_ok());

    owe::wpscene::AnimKeyframeTangent tangent;
    ASSERT_TRUE(owe::wpscene::ParseAnimKeyframeTangent(parsed.unwrap(), tangent));
    EXPECT_TRUE(tangent.enabled);
    EXPECT_FLOAT_EQ(tangent.x, 1.5f);
    EXPECT_FLOAT_EQ(tangent.y, -2.0f);
    EXPECT_EQ(tangent.magic, rstd::i32(7));
}

TEST(FieldBindingJson, TangentObjectDefaultsToEnabledAndParsesStep) {
    auto parsed = owe::ParseJson(R"({
        "frame": 10,
        "value": 2.0,
        "step": true,
        "front": {"x": 0.5, "y": 1.0}
    })"_str);
    ASSERT_TRUE(parsed.is_ok());

    owe::wpscene::AnimKeyframe key;
    ASSERT_TRUE(owe::wpscene::ParseAnimKeyframe(parsed.unwrap(), key));
    EXPECT_TRUE(key.step);
    EXPECT_TRUE(key.front.enabled);
    EXPECT_FLOAT_EQ(key.front.x, 0.5f);
    EXPECT_FLOAT_EQ(key.front.y, 1.0f);
    EXPECT_FALSE(key.back.enabled);
}

TEST(SceneObjectClone, MembersProvideCloneTraitImplementation) {
    owe::wpscene::AnimCurve curve;
    curve.relative = true;
    curve.c0.push({ .frame = rstd::i32(3), .value = 1.5f });

    auto direct_curve = curve.clone();
    auto trait_curve  = rstd::as<rstd::clone::Clone>(curve).clone();
    ASSERT_EQ(direct_curve.c0.len(), rstd::usize(1));
    EXPECT_FLOAT_EQ(trait_curve.c0[rstd::usize()].value, 1.5f);

    owe::wpscene::Material material;
    material.shader      = "generic"_Str;
    auto direct_material = material.clone();
    auto trait_material  = rstd::as<rstd::clone::Clone>(material).clone();
    EXPECT_EQ(direct_material.shader, "generic"_str);
    EXPECT_EQ(trait_material.shader, "generic"_str);
}

TEST(UserBindingJson, OwnsNamesConditionsAndResetsOnReparse) {
    auto                             json = owe::ParseJson(R"({
        "visible": {"value": 0, "user": {"name": "mode", "condition": {"value": 2}}},
        "text": {"user": {"name": "title", "condition": ["first", "second"]}}
    })"_str)
                                                .unwrap();
    owe::wpscene::VisibleUserBinding visible;
    owe::wpscene::UserValueBinding   text;
    bool                             enabled = true;
    owe::wpscene::ReadVisibleProperty(json, enabled, visible);
    owe::wpscene::ReadUserValueBinding(json, "text"_str, text);
    EXPECT_FALSE(enabled);
    EXPECT_EQ(visible.name, "mode"_str);
    EXPECT_EQ(text.name, "title"_str);
    EXPECT_TRUE(visible.has_condition);
    EXPECT_TRUE(text.has_condition);
    auto visible_copy = visible.clone();
    auto text_copy    = text.clone();
    json              = owe::Json {};
    visible.name      = "changed"_Str;
    text.name         = "changed"_Str;
    visible.condition = owe::Json {};
    text.condition    = owe::Json {};
    EXPECT_EQ(visible_copy.name, "mode"_str);
    EXPECT_EQ(text_copy.name, "title"_str);
    EXPECT_TRUE(visible_copy.condition.is_object());
    EXPECT_TRUE(text_copy.condition.is_array());
    auto direct =
        owe::ParseJson(R"({"visible":{"value":1,"user":"enabled"},"text":{"user":"caption"}})"_str)
            .unwrap();
    owe::wpscene::ReadVisibleProperty(direct, enabled, visible_copy);
    owe::wpscene::ReadUserValueBinding(direct, "text"_str, text_copy);
    EXPECT_TRUE(enabled);
    EXPECT_EQ(visible_copy.name, "enabled"_str);
    EXPECT_EQ(text_copy.name, "caption"_str);
    EXPECT_FALSE(visible_copy.has_condition);
    EXPECT_TRUE(visible_copy.condition.is_null());
    EXPECT_FALSE(text_copy.has_condition);
    auto absent = owe::ParseJson("{}"_str).unwrap();
    owe::wpscene::ReadVisibleProperty(absent, enabled, visible_copy);
    owe::wpscene::ReadUserValueBinding(absent, "text"_str, text_copy);
    EXPECT_TRUE(enabled);
    EXPECT_TRUE(visible_copy.empty());
    EXPECT_TRUE(text_copy.empty());
}

TEST(SceneDocumentStorage, PreservesPathOrderAndBindingReplacement) {
    owe::wpscene::SceneCamera camera;
    auto first = owe::ParseJson(R"({"paths":["first",7,"second","first"]})"_str).unwrap();
    ASSERT_TRUE(camera.FromJson(first));
    auto second = owe::ParseJson(R"({"paths":["last"]})"_str).unwrap();
    ASSERT_TRUE(camera.FromJson(second));
    ASSERT_EQ(camera.paths.len(), rstd::usize(4));
    EXPECT_EQ(camera.paths[rstd::usize()], "first"_str);
    EXPECT_EQ(camera.paths[rstd::usize(1)], "second"_str);
    EXPECT_EQ(camera.paths[rstd::usize(2)], "first"_str);
    EXPECT_EQ(camera.paths[rstd::usize(3)], "last"_str);

    owe::wpscene::SceneGeneral general;
    auto                       bindings = owe::ParseJson(R"({
        "clearcolor":{"value":"1 1 1","user":"background"},
        "cameraparallaxamount":{"value":1,"user":"amount"},
        "camerashake":{"value":false,"user":{"name":"ignored"}}
    })"_str)
                                              .unwrap();
    ASSERT_TRUE(general.FromJson(bindings));
    bindings =
        owe::ParseJson(R"({"clearcolor":{"value":"0 0 0","user":"replacement"}})"_str).unwrap();
    ASSERT_TRUE(general.FromJson(bindings));
    ASSERT_EQ(general.user_bindings.len(), rstd::usize(2));
    ASSERT_TRUE(general.user_bindings.get("clearcolor"_str).is_some());
    EXPECT_EQ(**general.user_bindings.get("clearcolor"_str), "replacement"_str);
    EXPECT_EQ(**general.user_bindings.get("cameraparallaxamount"_str), "amount"_str);
}

TEST(SceneObjectDependencies, PreserveDuplicatesOrderAndCloneStorage) {
    owe::fs::VFS vfs;
    auto         json =
        owe::ParseJson(
            R"({"id":1,"name":"shared-name","attachment":"joint","dependencies":[3,-2,3],"sound":[]})"_str)
            .unwrap();
    owe::wpscene::ContainerObject container;
    owe::wpscene::SoundObject     sound;
    owe::wpscene::LightObject     light;
    owe::wpscene::TextObject      text;
    owe::wpscene::ModelObject     model;
    owe::wpscene::CameraObject    camera;
    ASSERT_TRUE(container.FromJson(json));
    ASSERT_TRUE(sound.FromJson(json, vfs));
    ASSERT_TRUE(light.FromJson(json, vfs));
    ASSERT_TRUE(text.FromJson(json, vfs));
    ASSERT_TRUE(model.FromJson(json, vfs));
    ASSERT_TRUE(camera.FromJson(json, vfs));
    for (const auto* values : { &container.dependencies,
                                &sound.dependencies,
                                &light.dependencies,
                                &text.dependencies,
                                &model.dependencies,
                                &camera.dependencies }) {
        ASSERT_EQ(values->len(), rstd::usize(3));
        EXPECT_EQ((*values)[rstd::usize()], rstd::i32(3));
        EXPECT_EQ((*values)[rstd::usize(1)], rstd::i32(-2));
        EXPECT_EQ((*values)[rstd::usize(2)], rstd::i32(3));
    }
    for (const auto* name :
         { &container.name, &sound.name, &light.name, &text.name, &model.name, &camera.name })
        EXPECT_EQ(*name, "shared-name"_str);
    for (const auto* attachment : { &container.attachment, &text.attachment, &model.attachment })
        EXPECT_EQ(*attachment, "joint"_str);
    owe::wpscene::ParticleObject particle;
    particle.name                        = "particle"_Str;
    particle.attachment                  = "bone"_Str;
    particle.dependencies                = container.dependencies.clone();
    auto copy                            = particle.Clone();
    particle.dependencies[rstd::usize()] = rstd::i32(99);
    particle.name                        = "changed"_Str;
    particle.attachment                  = "changed"_Str;
    EXPECT_EQ(copy.name, "particle"_str);
    EXPECT_EQ(copy.attachment, "bone"_str);
    EXPECT_EQ(copy.dependencies[rstd::usize()], rstd::i32(3));
}

TEST(SoundObjectJson, OwnsPathsAndPreservesReparseSemantics) {
    owe::fs::VFS              vfs;
    owe::wpscene::SoundObject sound;
    auto                      json = owe::ParseJson(R"({
        "sound":["first.ogg","","second.ogg","first.ogg"],
        "volume":{"value":0.25,"user":"level"},
        "playbackmode":"random",
        "queuemode":"queue",
        "visible":{"value":false,"user":"enabled"}
    })"_str)
                                         .unwrap();
    ASSERT_TRUE(sound.FromJson(json, vfs));
    json = owe::Json {};
    ASSERT_EQ(sound.sound.len(), rstd::usize(3));
    EXPECT_EQ(sound.sound[rstd::usize()], "first.ogg"_str);
    EXPECT_EQ(sound.sound[rstd::usize(1)], "second.ogg"_str);
    EXPECT_EQ(sound.sound[rstd::usize(2)], "first.ogg"_str);
    EXPECT_EQ(sound.playbackmode, "random"_str);
    EXPECT_EQ(sound.queuemode, "queue"_str);
    EXPECT_EQ(sound.volume_user_key, "level"_str);
    EXPECT_EQ(sound.visible_user_key, "enabled"_str);
    EXPECT_FALSE(sound.visible);
    auto more = owe::ParseJson(R"({"sound":["last.ogg"],"volume":0.5})"_str).unwrap();
    ASSERT_TRUE(sound.FromJson(more, vfs));
    ASSERT_EQ(sound.sound.len(), rstd::usize(4));
    EXPECT_EQ(sound.sound[rstd::usize(3)], "last.ogg"_str);
    EXPECT_EQ(sound.volume_user_key, "level"_str);
    EXPECT_TRUE(sound.visible_user_key.is_empty());
    EXPECT_FALSE(sound.visible);
}

TEST(ObjectInstanceJson, AppliesMaterialBindingOverridesBySlot) {
    auto parsed = owe::ParseJson(R"({
        "textures": [null, "linked"],
        "usertextures": [null, "replacement"],
        "combos": {"version": 2}
    })"_str);
    ASSERT_TRUE(parsed.is_ok());

    owe::wpscene::ObjectInstance instance;
    ASSERT_TRUE(instance.FromJson(parsed.unwrap()));
    owe::wpscene::Material material;
    material.textures.push("base-0"_Str);
    material.textures.push("base-1"_Str);
    instance.ApplyTo(material);

    ASSERT_EQ(material.textures.len(), rstd::usize(2));
    EXPECT_EQ(material.textures[rstd::usize(0)], "base-0"_str);
    EXPECT_EQ(material.textures[rstd::usize(1)], "linked"_str);
    ASSERT_EQ(material.usertextures.len(), rstd::usize(2));
    EXPECT_TRUE(material.usertextures[rstd::usize()].is_null());
    ASSERT_TRUE(material.usertextures[rstd::usize(1)].is_string());
    EXPECT_EQ(rstd::cppstd::to_string(*material.usertextures[rstd::usize(1)].as_str()),
              "replacement");
    EXPECT_EQ(*material.combos.get("version"_str).unwrap(), rstd::i32(2));
}

TEST(TextObjectJson, ReadsDirectUserValueBinding) {
    auto parsed = owe::ParseJson(R"({"text":{"user":"title","value":"default"}})"_str);
    ASSERT_TRUE(parsed.is_ok());

    owe::fs::VFS             vfs;
    owe::wpscene::TextObject text;
    ASSERT_TRUE(text.FromJson(parsed.unwrap(), vfs));
    EXPECT_EQ(text.text_user.name, "title"_str);
    EXPECT_TRUE(text.reflected);
}

TEST(TextObjectJson, ReadsReflectionParticipation) {
    auto parsed = owe::ParseJson(R"({"reflected":false})"_str);
    ASSERT_TRUE(parsed.is_ok());

    owe::fs::VFS             vfs;
    owe::wpscene::TextObject text;
    ASSERT_TRUE(text.FromJson(parsed.unwrap(), vfs));
    EXPECT_FALSE(text.reflected);
}

// A number pads every side alike; a pair ("x y", as Ocarina of Time's text layers have it) pads
// the left and right by x, the top and bottom by y (text-padding-pair: "40 10" grows the box by
// 80 across and 20 down, "20 20" as 20 does).
TEST(TextObjectJson, ReadsPaddingAsANumberOrAPair) {
    owe::fs::VFS vfs;
    const auto   check = [&](rstd::ref<rstd::str> json, float x, float y) {
        auto parsed = owe::ParseJson(json);
        ASSERT_TRUE(parsed.is_ok());
        owe::wpscene::TextObject text;
        ASSERT_TRUE(text.FromJson(parsed.unwrap(), vfs));
        EXPECT_FLOAT_EQ(text.padding[rstd::usize(0)], x);
        EXPECT_FLOAT_EQ(text.padding[rstd::usize(1)], y);
    };
    check(R"({"padding":20})"_str, 20.0f, 20.0f);
    check(R"({"padding":"40 10"})"_str, 40.0f, 10.0f);
    check(R"({"padding":"32.00000 32.00000"})"_str, 32.0f, 32.0f);
    check(R"({})"_str, 0.0f, 0.0f);
}

TEST(TextRenderMode, UsesDirectRenderingOnlyWithoutIndependentSurfaceRequirements) {
    EXPECT_EQ(owe::ResolveTextRenderMode({}), owe::TextRenderMode::Direct);
    EXPECT_EQ(owe::ResolveTextRenderMode({ .color_blend = true }), owe::TextRenderMode::Offscreen);
    EXPECT_EQ(owe::ResolveTextRenderMode({ .has_effect = true }), owe::TextRenderMode::Offscreen);
    EXPECT_EQ(owe::ResolveTextRenderMode({ .copy_background = true }),
              owe::TextRenderMode::Offscreen);
    EXPECT_EQ(owe::ResolveTextRenderMode({ .opaque_background = true }),
              owe::TextRenderMode::Offscreen);
    EXPECT_EQ(owe::ResolveTextRenderMode({ .linked_source = true }),
              owe::TextRenderMode::Offscreen);
    // Wallpaper Engine fades translucent outlined text as a whole, so the outline doesn't show
    // through the letters (text-outline): it is drawn opaque offscreen and faded as it is laid on.
    EXPECT_EQ(owe::ResolveTextRenderMode({ .translucent_outline = true }),
              owe::TextRenderMode::Offscreen);
}

TEST(SceneObjectExpansion, PreservesHiddenTextLayers) {
    auto parsed = owe::ParseJson(R"({
        "objects": [{
            "id": 7,
            "name": "Style1",
            "text": "progress",
            "visible": false
        }]
    })"_str);
    ASSERT_TRUE(parsed.is_ok());

    owe::fs::VFS vfs;
    auto objects = owe::ExpandObjects(parsed.unwrap(), vfs, owe::wpscene::kSceneVersionUnknown);

    ASSERT_EQ(objects.len(), rstd::usize(1));
    ASSERT_TRUE(objects[rstd::usize()].is_Text());
    EXPECT_FALSE(objects[rstd::usize()].as_Text().value.visible);
}

TEST(SceneObjectExpansion, PreservesLocalVisibilityBelowHiddenUserAncestor) {
    auto         parsed = owe::ParseJson(R"({
        "objects": [
            {"id": 1, "name": "Variant", "visible": {"value": false, "user": "enabled"}},
            {"id": 2, "name": "Nested", "parent": 1},
            {"id": 3, "name": "Label", "text": "child", "parent": 2},
            {"id": 4, "name": "Shape", "shape": "quad", "parent": 2},
            {"id": 5, "name": "Hidden Label", "text": "hidden", "parent": 2, "visible": false}
        ]
    })"_str)
                              .unwrap();
    owe::fs::VFS vfs;
    auto         objects = owe::ExpandObjects(parsed, vfs, owe::wpscene::kSceneVersionUnknown);
    ASSERT_EQ(objects.len(), rstd::usize(5));
    ASSERT_TRUE(objects[rstd::usize(0)].is_Container());
    EXPECT_FALSE(objects[rstd::usize(0)].as_Container().value.visible);
    ASSERT_TRUE(objects[rstd::usize(1)].is_Container());
    EXPECT_TRUE(objects[rstd::usize(1)].as_Container().value.visible);
    ASSERT_TRUE(objects[rstd::usize(2)].is_Text());
    EXPECT_TRUE(objects[rstd::usize(2)].as_Text().value.visible);
    ASSERT_TRUE(objects[rstd::usize(3)].is_Shape());
    EXPECT_TRUE(objects[rstd::usize(3)].as_Shape().value.visible);
    ASSERT_TRUE(objects[rstd::usize(4)].is_Text());
    EXPECT_FALSE(objects[rstd::usize(4)].as_Text().value.visible);
}

TEST(SceneObjectExpansion, UserVisibilityRestoresImageDescendants) {
    auto document = owe::wpscene::ParseSceneDocumentJson(R"({
        "camera": {},
        "general": {"orthogonalprojection": {"width": 1920, "height": 1080}},
        "objects": [
            {"id": 1, "name": "Variant", "image": "models/util/solidlayer.json",
             "size": "256 256", "visible": {"value": false, "user": {"name": "type", "condition": "2"}}},
            {"id": 2, "name": "Child", "image": "models/util/solidlayer.json", "size": "256 256", "parent": 1},
            {"id": 3, "name": "Hidden Child", "image": "models/util/solidlayer.json", "size": "256 256", "parent": 1, "visible": false}
        ]
    })"_str,
                                                         owe::wpscene::kSceneVersionUnknown);
    ASSERT_TRUE(document.is_some());
    auto assets = owe::fs::make_physical_fs(
        owe::fs::Path(rstd::cppstd::as_str(WAYWALLEN_ASSETS_DIR).unwrap()));
    ASSERT_TRUE(assets.is_ok());
    owe::fs::VFS vfs;
    ASSERT_TRUE(vfs.mount("/assets"_str, rstd::move(assets).unwrap_unchecked()).is_ok());
    wavsen::audio::SoundManager sound_manager;
    owe::SceneParser            parser;
    auto                        parsed = parser.Parse(
        "variant-visibility"_str,
        rstd::ref<owe::wpscene::SceneDocument>::from_raw_parts(rstd::addressof(*document)),
        rstd::mut_ref<owe::fs::VFS>::from_raw_parts(rstd::addressof(vfs)),
        rstd::mut_ref<wavsen::audio::SoundManager>::from_raw_parts(rstd::addressof(sound_manager)));
    ASSERT_TRUE(parsed.is_ok());
    auto  result = rstd::move(parsed).unwrap();
    auto& scene  = *result.scene;
    ASSERT_NE(scene.RootMut()->FindByName("Child"_str), nullptr);
    EXPECT_TRUE(scene.IsLayerVisibilityElidable({ .value = rstd::i32(1) }));
    EXPECT_FALSE(scene.IsLayerVisibilityElidable({ .value = rstd::i32(2) }));
    EXPECT_TRUE(scene.IsLayerVisibilityElidable({ .value = rstd::i32(3) }));
    for (auto mode : { "2"_str, "1"_str, "2"_str }) {
        EXPECT_TRUE(scene.ApplyUserNodeVisibilityBindings("type"_str, rstd::into<owe::Json>(mode)));
        EXPECT_EQ(scene.IsLayerVisibilityElidable({ .value = rstd::i32(1) }), mode != "2"_str);
        EXPECT_FALSE(scene.IsLayerVisibilityElidable({ .value = rstd::i32(2) }));
        EXPECT_TRUE(scene.IsLayerVisibilityElidable({ .value = rstd::i32(3) }));
    }
}

TEST(SceneGeneralScript, FovUpdatesPerspectiveCameraAndSharedState) {
    auto document = owe::wpscene::ParseSceneDocumentJson(R"JSON({
        "camera": {"eye":"0 0 5", "center":"0 0 0", "up":"0 1 0"},
        "general": {"orthogonalprojection": null,
          "fov": {"value": 50, "script": "shared.zooming = 0; export function update(value) { return value + 5; }"}},
        "objects": [{"id":1, "name":"Interface Root",
          "scale": {"value":"1 1 1", "script":"export function update(value) { return shared.zooming === 0 ? 1 : 0.4; }"}}]
    })JSON"_str,
                                                         owe::wpscene::kSceneVersionUnknown);
    ASSERT_TRUE(document.is_some());
    auto assets = owe::fs::make_physical_fs(
        owe::fs::Path(rstd::cppstd::as_str(WAYWALLEN_ASSETS_DIR).unwrap()));
    ASSERT_TRUE(assets.is_ok());
    owe::fs::VFS vfs;
    ASSERT_TRUE(vfs.mount("/assets"_str, rstd::move(assets).unwrap_unchecked()).is_ok());
    wavsen::audio::SoundManager sound;
    owe::SceneParser            parser;
    auto                        parsed = parser.Parse(
        "general-fov"_str,
        rstd::ref<owe::wpscene::SceneDocument>::from_raw_parts(rstd::addressof(*document)),
        rstd::mut_ref<owe::fs::VFS>::from_raw_parts(rstd::addressof(vfs)),
        rstd::mut_ref<wavsen::audio::SoundManager>::from_raw_parts(rstd::addressof(sound)));
    ASSERT_TRUE(parsed.is_ok());
    auto  scene  = rstd::move(parsed).unwrap();
    auto  camera = scene.scene->CameraHandle("global_perspective"_str);
    auto* node   = scene.scene->RootMut()->FindByName("Interface Root"_str);
    ASSERT_TRUE(camera.is_some());
    ASSERT_NE(node, nullptr);
    owe::script::TickSceneScripts(*scene.scene, owe::script::FrameInputs {});
    EXPECT_DOUBLE_EQ((**camera).Fov(), 55.0);
    scene.scene->UpdateCameraFillMode(rstd::u32(1280), rstd::u32(720), owe::FillMode::ASPECTCROP);
    EXPECT_DOUBLE_EQ((**camera).Fov(), 55.0);
    EXPECT_FLOAT_EQ(node->Scale().x(), 1.0f);
}

TEST(SceneGeneralScript, ReceivesConfiguredGeneralSettingsBeforeUpdate) {
    using namespace rstd::prelude;
    auto document = owe::wpscene::ParseSceneDocumentJson(R"JSON({
        "camera": {},
        "general": {"orthogonalprojection": null,
          "fov": {"value":50, "script":"let lang; export function applyGeneralSettings(s) { lang = s.language; } export function update() { return lang === 'zh-chs' ? 60 : 40; }"}},
        "objects": []
    })JSON"_str,
                                                         owe::wpscene::kSceneVersionUnknown);
    ASSERT_TRUE(document.is_some());
    auto assets = owe::fs::make_physical_fs(
        owe::fs::Path(rstd::cppstd::as_str(WAYWALLEN_ASSETS_DIR).unwrap()));
    ASSERT_TRUE(assets.is_ok());
    owe::fs::VFS vfs;
    ASSERT_TRUE(vfs.mount("/assets"_str, rstd::move(assets).unwrap()).is_ok());
    auto settings = rstd::json::Map::make();
    settings.insert("language"_Str, owe::Json::String("zh-chs"_Str));
    wavsen::audio::SoundManager sound;
    owe::SceneParser            parser;
    auto                        parsed = parser.Parse(
        "general-settings"_str,
        rstd::ref<owe::wpscene::SceneDocument>::from_raw_parts(rstd::addressof(*document)),
        rstd::mut_ref<owe::fs::VFS>::from_raw_parts(rstd::addressof(vfs)),
        rstd::mut_ref<wavsen::audio::SoundManager>::from_raw_parts(rstd::addressof(sound)),
        { .general_settings = Some(ref<rstd::json::Map>::from_raw_parts(&settings)) });
    ASSERT_TRUE(parsed.is_ok());
    auto scene  = rstd::move(parsed).unwrap();
    auto camera = scene.scene->CameraHandle("global_perspective"_str);
    ASSERT_TRUE(camera.is_some());
    owe::script::TickSceneScripts(*scene.scene, owe::script::FrameInputs {});
    EXPECT_DOUBLE_EQ((**camera).Fov(), 60.0);
}

TEST(SceneDynamicLayer, CreatesImageConfigurationDuringInitialization) {
    using namespace rstd::prelude;
    auto document = owe::wpscene::ParseSceneDocumentJson(R"JSON({
        "camera": {},
        "general": {"orthogonalprojection": {"width": 1920, "height": 1080}},
        "objects": [{"id": 1, "name": "creator", "scale": {
            "value": "1 1 1",
            "script": "export function init() { const created = thisScene.createLayer({image:'models/util/solidlayer.json', name:'initial-runtime', size:'32 32', origin:'0 0 0', scale:new Vec2(2, 3), color:{value:'1 1 1', user:'tint'}}); created.origin = new Vec3(10, 20, 0); }"
        }}]
    })JSON"_str,
                                                         owe::wpscene::kSceneVersionUnknown);
    ASSERT_TRUE(document.is_some());
    auto assets = owe::fs::make_physical_fs(
        owe::fs::Path(rstd::cppstd::as_str(WAYWALLEN_ASSETS_DIR).unwrap()));
    ASSERT_TRUE(assets.is_ok());
    owe::fs::VFS vfs;
    ASSERT_TRUE(vfs.mount("/assets"_str, rstd::move(assets).unwrap_unchecked()).is_ok());
    wavsen::audio::SoundManager sound;
    owe::SceneParser            parser;
    auto                        parsed =
        parser.Parse("initial-image"_str,
                     ref<owe::wpscene::SceneDocument>::from_raw_parts(rstd::addressof(*document)),
                     mut_ref<owe::fs::VFS>::from_raw_parts(rstd::addressof(vfs)),
                     mut_ref<wavsen::audio::SoundManager>::from_raw_parts(rstd::addressof(sound)));
    ASSERT_TRUE(parsed.is_ok());
    auto  scene   = rstd::move(parsed).unwrap();
    auto* created = scene.scene->RootMut()->FindByName("initial-runtime"_str);
    ASSERT_NE(created, nullptr);
    ASSERT_NE(created->Mesh(), nullptr);
    EXPECT_FLOAT_EQ(created->Translate().x(), 10.0f);
    EXPECT_FLOAT_EQ(created->Translate().y(), 20.0f);
    EXPECT_FLOAT_EQ(created->Scale().x(), 2.0f);
    EXPECT_FLOAT_EQ(created->Scale().y(), 3.0f);
    EXPECT_FLOAT_EQ(created->Scale().z(), 1.0f);
    EXPECT_TRUE(scene.scene->ResourceIndex().nodeId(*created).is_some());
    EXPECT_EQ(scene.scene->ImageColorUserBindings("tint"_str).len(), usize(1));
    EXPECT_TRUE(scene.scene->RemoveNodeMaterialBindings(created->Identity()) > usize());
    EXPECT_TRUE(scene.scene->ImageColorUserBindings("tint"_str).is_empty());
}

TEST(SceneDynamicLayer, PreservesInitialUserPropertiesAfterParsing) {
    using namespace rstd::prelude;
    auto document = owe::wpscene::ParseSceneDocumentJson(
        R"({"camera":{},"general":{},"objects":[]})"_str, owe::wpscene::kSceneVersionUnknown);
    ASSERT_TRUE(document.is_some());
    auto initial = owe::ParseJson(R"({"brightness":{"type":"slider","value":0.5}})"_str).unwrap();
    auto properties = initial.as_object();
    ASSERT_TRUE(properties.is_some());
    owe::fs::VFS                vfs;
    wavsen::audio::SoundManager sound;
    owe::SceneParser            parser;
    auto                        parsed =
        parser.Parse("property-state"_str,
                     ref<owe::wpscene::SceneDocument>::from_raw_parts(rstd::addressof(*document)),
                     mut_ref<owe::fs::VFS>::from_raw_parts(rstd::addressof(vfs)),
                     mut_ref<wavsen::audio::SoundManager>::from_raw_parts(rstd::addressof(sound)),
                     { .user_properties = Some(*properties) });
    ASSERT_TRUE(parsed.is_ok());
    auto scene = rstd::move(parsed).unwrap();
    initial    = owe::ParseJson("{}"_str).unwrap();

    auto state = scene.scene->Extension<owe::SceneUserPropertyState>();
    ASSERT_TRUE(state.is_some());
    auto property = (**state).Values()->get("brightness"_str);
    ASSERT_TRUE(property.is_some());
    auto value = (**property).get("value"_str);
    ASSERT_TRUE(value.is_some());
    EXPECT_EQ((*value)->as_f64().unwrap_or(f64()), f64(0.5));
}

TEST(SceneDynamicLayer, CreatesImageConfigurationDuringUpdate) {
    using namespace rstd::prelude;
    auto document = owe::wpscene::ParseSceneDocumentJson(R"JSON({
        "camera": {},
        "general": {"orthogonalprojection": {"width": 1920, "height": 1080}},
        "objects": [{"id": 1, "name": "creator", "scale": {
            "value": "1 1 1",
            "script": "let created; export function update(value) { if (!created) { created = thisScene.createLayer({image:'models/util/solidlayer.json', name:'runtime', size:'32 32', origin:'0 0 0', scale:'1 1 1', alpha:{value:1, script:'export function init() { thisScene.createLayer({image:`models/util/solidlayer.json`, name:`nested-runtime`, size:`8 8`}); thisLayer.origin = new Vec3(3, 4, 0); return 1; }'}}); if (created.origin.x === 3) created.origin = new Vec3(10, 20, 0); thisScene.createLayer({image:'models/util/solidlayer.json', name:'hidden-runtime', size:'32 32', visible:false}); thisScene.createLayer({image:'models/util/solidlayer.json', name:'bound-runtime', size:'32 32', visible:{value:true,user:'shown'}, color:{value:'1 1 1',user:'tint'}, alpha:{value:1,user:'fade'}}); thisScene.createLayer({image:'models/util/solidlayer.json', name:'effect-runtime', size:'32 32', effects:[{file:'effects/blur/effect.json'}]}); thisScene.createLayer({image:'models/runtime-auto.json', name:'auto-runtime', origin:'0 0 0'}); thisScene.createLayer({image:'models/runtime-auto.json', name:'explicit-runtime', origin:'0 0 0', size:'20 10'}); } return value; }"
        }}, {"id":2, "name":"auto-initial", "image":"models/runtime-auto.json", "origin":"0 0 0", "color":{"value":"1 1 1","user":"tint"}}]
    })JSON"_str,
                                                         owe::wpscene::kSceneVersionUnknown);
    ASSERT_TRUE(document.is_some());
    auto assets = owe::fs::make_physical_fs(
        owe::fs::Path(rstd::cppstd::as_str(WAYWALLEN_ASSETS_DIR).unwrap()));
    ASSERT_TRUE(assets.is_ok());
    auto vfs = Box<owe::fs::VFS>::make();
    ASSERT_TRUE(vfs->mount("/assets"_str, rstd::move(assets).unwrap_unchecked()).is_ok());
    auto effect_assets = owe::fs::make_physical_fs(owe::fs::Path(
        rstd::cppstd::as_str(std::string(WAYWALLEN_ASSETS_DIR) + "/effects/blur").unwrap()));
    ASSERT_TRUE(effect_assets.is_ok());
    ASSERT_TRUE(vfs->mount("/assets"_str, rstd::move(effect_assets).unwrap()).is_ok());
    auto temporary = rstd::fs::TempDir::make("owe-runtime-auto"_str).unwrap();
    auto root      = rstd::path::PathBuf::from(temporary.path().as_os_str().to_os_string());
    ASSERT_TRUE(rstd::fs::create_dir(root.join("models"_str).as_path()).is_ok());
    ASSERT_TRUE(
        rstd::fs::write(
            root.join("models/runtime-auto.json"_str).as_path(),
            R"({"autosize":true,"material":"materials/util/solidlayer_instance_3.json"})"_bytes)
            .is_ok());
    auto local = owe::fs::make_physical_fs(temporary.path());
    ASSERT_TRUE(local.is_ok());
    ASSERT_TRUE(vfs->mount("/assets"_str, rstd::move(local).unwrap()).is_ok());
    wavsen::audio::SoundManager sound;
    owe::SceneParser            parser;
    auto                        initial_properties =
        owe::ParseJson(
            R"({"shown":{"value":false},"tint":{"value":"0.2 0.3 0.4"},"fade":{"value":0.4}})"_str)
            .unwrap();
    auto parsed =
        parser.Parse("runtime-image"_str,
                     ref<owe::wpscene::SceneDocument>::from_raw_parts(rstd::addressof(*document)),
                     mut_ref<owe::fs::VFS>::from_raw_parts(vfs.get()),
                     mut_ref<wavsen::audio::SoundManager>::from_raw_parts(rstd::addressof(sound)),
                     { .user_properties = Some(*initial_properties.as_object()) });
    ASSERT_TRUE(parsed.is_ok());
    auto scene = rstd::move(parsed).unwrap();
    scene.scene->InstallExtension(rstd::move(vfs));
    initial_properties = owe::ParseJson("{}"_str).unwrap();
    auto* initial_node = scene.scene->RootMut()->FindByName("auto-initial"_str);
    ASSERT_NE(initial_node, nullptr);
    initial_node->SetColor(Eigen::Vector3f(0.9f, 0.8f, 0.7f));

    owe::script::TickSceneScripts(*scene.scene, owe::script::FrameInputs {});
    auto* created = scene.scene->RootMut()->FindByName("runtime"_str);
    ASSERT_NE(created, nullptr);
    ASSERT_NE(created->Mesh(), nullptr);
    EXPECT_FLOAT_EQ(created->Translate().x(), 10.0f);
    EXPECT_FLOAT_EQ(created->Translate().y(), 20.0f);
    EXPECT_NE(scene.scene->RootMut()->FindByName("nested-runtime"_str), nullptr);
    auto* hidden = scene.scene->RootMut()->FindByName("hidden-runtime"_str);
    ASSERT_NE(hidden, nullptr);
    EXPECT_FALSE(hidden->Visible());
    auto* bound = scene.scene->RootMut()->FindByName("bound-runtime"_str);
    ASSERT_NE(bound, nullptr);
    EXPECT_FALSE(bound->Visible());
    EXPECT_FLOAT_EQ(bound->Color().x(), 0.2f);
    EXPECT_FLOAT_EQ(bound->Color().y(), 0.3f);
    EXPECT_FLOAT_EQ(bound->Color().z(), 0.4f);
    auto shown = owe::ParseJson(R"({"value":true})"_str).unwrap();
    (void)owe::SceneUserPropertyApplier::Apply(*scene.scene, "shown"_str, shown);
    EXPECT_TRUE(bound->Visible());
    EXPECT_FLOAT_EQ(bound->EffectiveAlpha(), 0.4f);
    EXPECT_FLOAT_EQ(initial_node->Color().x(), 0.9f);
    auto* effect = scene.scene->RootMut()->FindByName("effect-runtime"_str);
    ASSERT_NE(effect, nullptr);
    EXPECT_TRUE(effect->HasLayer());
    auto* auto_initial     = scene.scene->RootMut()->FindByName("auto-initial"_str);
    auto* auto_runtime     = scene.scene->RootMut()->FindByName("auto-runtime"_str);
    auto* explicit_runtime = scene.scene->RootMut()->FindByName("explicit-runtime"_str);
    ASSERT_NE(auto_initial, nullptr);
    ASSERT_NE(auto_runtime, nullptr);
    ASSERT_NE(explicit_runtime, nullptr);
    EXPECT_GT(auto_runtime->Size().x(), 0.0f);
    EXPECT_GT(auto_runtime->Size().y(), 0.0f);
    EXPECT_FLOAT_EQ(auto_initial->Size().x(), auto_runtime->Size().x());
    EXPECT_FLOAT_EQ(auto_initial->Size().y(), auto_runtime->Size().y());
    EXPECT_FLOAT_EQ(explicit_runtime->Size().x(), 20.0f);
    EXPECT_FLOAT_EQ(explicit_runtime->Size().y(), 10.0f);
}

TEST(SceneDynamicLayer, CreatesTextConfigurationDuringUpdate) {
    using namespace rstd::prelude;
    auto document = owe::wpscene::ParseSceneDocumentJson(R"JSON({
        "camera": {},
        "general": {"orthogonalprojection": {"width": 1920, "height": 1080}},
        "objects": [{"id": 1, "name": "creator", "scale": {
            "value": "1 1 1",
            "script": "let done = false; export function update(value) { if (!done) { let layer = thisScene.createLayer({text:'Hello', font:'systemfont_DejaVu Sans', pointsize:32, name:'runtime-text', alpha:{value:1, script:'export function init() { thisLayer.text = `Updated`; return 1; }'}}); let empty = thisScene.createLayer({text:'', font:'systemfont_DejaVu Sans', pointsize:32, name:'empty-text'}); empty.text = 'Filled'; let hidden = thisScene.createLayer({text:'Hidden', font:'systemfont_DejaVu Sans', pointsize:32, name:'hidden-text', visible:{value:true,user:'shown'}}); let effect = thisScene.createLayer({text:'Blur', font:'systemfont_DejaVu Sans', pointsize:32, name:'effect-text', effects:[{file:'effects/blur/effect.json'}]}); if (layer.text === 'Updated' && empty.text === 'Filled' && !hidden.visible && effect.text === 'Blur') thisLayer.origin = new Vec3(23, 0, 0); done = true; } return value; }"
        }}]
    })JSON"_str,
                                                         owe::wpscene::kSceneVersionUnknown);
    ASSERT_TRUE(document.is_some());
    auto assets = owe::fs::make_physical_fs(
        owe::fs::Path(rstd::cppstd::as_str(WAYWALLEN_ASSETS_DIR).unwrap()));
    ASSERT_TRUE(assets.is_ok());
    auto vfs = Box<owe::fs::VFS>::make();
    ASSERT_TRUE(vfs->mount("/assets"_str, rstd::move(assets).unwrap()).is_ok());
    auto effect_assets = owe::fs::make_physical_fs(owe::fs::Path(
        rstd::cppstd::as_str(std::string(WAYWALLEN_ASSETS_DIR) + "/effects/blur").unwrap()));
    ASSERT_TRUE(effect_assets.is_ok());
    ASSERT_TRUE(vfs->mount("/assets"_str, rstd::move(effect_assets).unwrap()).is_ok());
    auto initial = owe::ParseJson(R"({"shown":{"value":false}})"_str).unwrap();
    wavsen::audio::SoundManager sound;
    owe::SceneParser            parser;
    auto                        parsed =
        parser.Parse("runtime-text"_str,
                     ref<owe::wpscene::SceneDocument>::from_raw_parts(rstd::addressof(*document)),
                     mut_ref<owe::fs::VFS>::from_raw_parts(vfs.get()),
                     mut_ref<wavsen::audio::SoundManager>::from_raw_parts(rstd::addressof(sound)),
                     { .user_properties = Some(*initial.as_object()) });
    ASSERT_TRUE(parsed.is_ok());
    auto scene = rstd::move(parsed).unwrap();
    scene.scene->InstallExtension(rstd::move(vfs));
    auto targets = scene.scene->RenderTargetNames().len();
    auto cameras = scene.scene->CameraNames().len();
    owe::script::TickSceneScripts(*scene.scene, owe::script::FrameInputs {});
    auto* creator = scene.scene->RootMut()->FindByName("creator"_str);
    auto* text    = scene.scene->RootMut()->FindByName("runtime-text"_str);
    auto* empty   = scene.scene->RootMut()->FindByName("empty-text"_str);
    auto* hidden  = scene.scene->RootMut()->FindByName("hidden-text"_str);
    auto* effect  = scene.scene->RootMut()->FindByName("effect-text"_str);
    ASSERT_NE(creator, nullptr);
    ASSERT_NE(text, nullptr);
    ASSERT_NE(empty, nullptr);
    ASSERT_NE(hidden, nullptr);
    ASSERT_NE(effect, nullptr);
    EXPECT_FLOAT_EQ(creator->Translate().x(), 23.0f);
    EXPECT_FALSE(hidden->Visible());
    ASSERT_NE(text->Mesh(), nullptr);
    ASSERT_NE(empty->Mesh(), nullptr);
    EXPECT_GT(scene.scene->RenderTargetNames().len(), targets);
    EXPECT_GT(scene.scene->CameraNames().len(), cameras);
    auto shown = owe::ParseJson(R"({"value":true})"_str).unwrap();
    (void)owe::SceneUserPropertyApplier::Apply(*scene.scene, "shown"_str, shown);
    EXPECT_TRUE(hidden->Visible());
}

TEST(SceneDynamicLayer, RollsBackTextWhenDeferredModuleFails) {
    using namespace rstd::prelude;
    auto document = owe::wpscene::ParseSceneDocumentJson(R"JSON({
        "camera": {},
        "general": {"orthogonalprojection": {"width": 1920, "height": 1080}},
        "objects": [{"id": 1, "name": "creator", "scale": {
            "value": "1 1 1",
            "script": "let done = false; export function update(value) { if (!done) { try { thisScene.createLayer({text:'Failed', font:'systemfont_DejaVu Sans', pointsize:32, name:'failed-text', effects:[{file:'effects/blur/effect.json'}], alpha:{value:1, script:'throw new Error(7); export function update(v) { return v; }'}}); } catch (e) { thisLayer.origin = new Vec3(31, 0, 0); thisScene.createLayer({text:'After', font:'systemfont_DejaVu Sans', pointsize:32, name:'after-text'}); } done = true; } return value; }"
        }}]
    })JSON"_str,
                                                         owe::wpscene::kSceneVersionUnknown);
    ASSERT_TRUE(document.is_some());
    auto assets = owe::fs::make_physical_fs(
        owe::fs::Path(rstd::cppstd::as_str(WAYWALLEN_ASSETS_DIR).unwrap()));
    ASSERT_TRUE(assets.is_ok());
    auto effect_assets = owe::fs::make_physical_fs(owe::fs::Path(
        rstd::cppstd::as_str(std::string(WAYWALLEN_ASSETS_DIR) + "/effects/blur").unwrap()));
    ASSERT_TRUE(effect_assets.is_ok());
    auto vfs = Box<owe::fs::VFS>::make();
    ASSERT_TRUE(vfs->mount("/assets"_str, rstd::move(assets).unwrap()).is_ok());
    ASSERT_TRUE(vfs->mount("/assets"_str, rstd::move(effect_assets).unwrap()).is_ok());
    wavsen::audio::SoundManager sound;
    owe::SceneParser            parser;
    auto                        parsed =
        parser.Parse("runtime-text-rollback"_str,
                     ref<owe::wpscene::SceneDocument>::from_raw_parts(rstd::addressof(*document)),
                     mut_ref<owe::fs::VFS>::from_raw_parts(vfs.get()),
                     mut_ref<wavsen::audio::SoundManager>::from_raw_parts(rstd::addressof(sound)));
    ASSERT_TRUE(parsed.is_ok());
    auto scene = rstd::move(parsed).unwrap();
    scene.scene->InstallExtension(rstd::move(vfs));
    auto targets = scene.scene->RenderTargetNames().len();
    auto cameras = scene.scene->CameraNames().len();
    owe::script::TickSceneScripts(*scene.scene, owe::script::FrameInputs {});
    auto* creator = scene.scene->RootMut()->FindByName("creator"_str);
    ASSERT_NE(creator, nullptr);
    EXPECT_FLOAT_EQ(creator->Translate().x(), 31.0f);
    EXPECT_EQ(scene.scene->RootMut()->FindByName("failed-text"_str), nullptr);
    EXPECT_NE(scene.scene->RootMut()->FindByName("after-text"_str), nullptr);
    EXPECT_EQ(scene.scene->RenderTargetNames().len(), targets);
    EXPECT_EQ(scene.scene->CameraNames().len(), cameras);
    auto scripts = scene.scene->ExtensionMut<owe::script::ScriptScene>();
    ASSERT_TRUE(scripts.is_some());
    int count = 0;
    (**scripts).runtime().ForEachScript(
        [](owe::script::FieldScript*, void* out) {
            ++*static_cast<int*>(out);
        },
        &count);
    EXPECT_EQ(count, 1);
    owe::script::TickSceneScripts(*scene.scene, owe::script::FrameInputs {});
}

TEST(SceneDynamicLayer, DeletesCreatedAndAuthoredLayersAfterScriptTick) {
    using namespace rstd::prelude;
    auto document = owe::wpscene::ParseSceneDocumentJson(R"JSON({
        "camera": {},
        "general": {"orthogonalprojection": {"width": 1920, "height": 1080}},
        "objects": [{"id": 1, "name": "creator", "scale": {
            "value": "1 1 1",
            "script": "let stage = 0, image, text, original; export function update(value) { if (stage === 0) { image = thisScene.createLayer({image:'models/util/solidlayer.json', name:'delete-image', size:'32 32'}); text = thisScene.createLayer({text:'Delete', font:'systemfont_DejaVu Sans', pointsize:32, name:'delete-text', effects:[{file:'effects/blur/effect.json'}]}); original = thisScene.getLayer('original-text'); const ok = thisScene.destroyLayer(image) && thisScene.destroyLayer('delete-text') && thisScene.destroyLayer(1) && !thisScene.destroyLayer(image); if (ok && image.name === 'delete-image' && text.name === 'delete-text' && original.name === 'original-text') thisLayer.origin = new Vec3(47, 0, 0); stage = 1; } else if (stage === 1) { if (image.name === '' && text.name === '' && original.name === '' && !thisScene.destroyLayer(image)) thisLayer.origin = new Vec3(53, 0, 0); stage = 2; } return value; }"
        }}, {"id": 2, "name": "original-text", "text": "Original", "font":"systemfont_DejaVu Sans", "pointsize":32}]
    })JSON"_str,
                                                         owe::wpscene::kSceneVersionUnknown);
    ASSERT_TRUE(document.is_some());
    auto assets = owe::fs::make_physical_fs(
        owe::fs::Path(rstd::cppstd::as_str(WAYWALLEN_ASSETS_DIR).unwrap()));
    auto effect_assets = owe::fs::make_physical_fs(owe::fs::Path(
        rstd::cppstd::as_str(std::string(WAYWALLEN_ASSETS_DIR) + "/effects/blur").unwrap()));
    ASSERT_TRUE(assets.is_ok());
    ASSERT_TRUE(effect_assets.is_ok());
    auto vfs = Box<owe::fs::VFS>::make();
    ASSERT_TRUE(vfs->mount("/assets"_str, rstd::move(assets).unwrap()).is_ok());
    ASSERT_TRUE(vfs->mount("/assets"_str, rstd::move(effect_assets).unwrap()).is_ok());
    wavsen::audio::SoundManager sound;
    owe::SceneParser            parser;
    auto                        parsed =
        parser.Parse("delete-layers"_str,
                     ref<owe::wpscene::SceneDocument>::from_raw_parts(rstd::addressof(*document)),
                     mut_ref<owe::fs::VFS>::from_raw_parts(vfs.get()),
                     mut_ref<wavsen::audio::SoundManager>::from_raw_parts(rstd::addressof(sound)));
    ASSERT_TRUE(parsed.is_ok());
    auto scene = rstd::move(parsed).unwrap();
    scene.scene->InstallExtension(rstd::move(vfs));
    auto targets = scene.scene->RenderTargetNames().len();
    auto cameras = scene.scene->CameraNames().len();
    owe::script::TickSceneScripts(*scene.scene, owe::script::FrameInputs {});
    auto* creator = scene.scene->RootMut()->FindByName("creator"_str);
    ASSERT_NE(creator, nullptr);
    EXPECT_FLOAT_EQ(creator->Translate().x(), 47.0f);
    EXPECT_EQ(scene.scene->RootMut()->FindByName("delete-image"_str), nullptr);
    EXPECT_EQ(scene.scene->RootMut()->FindByName("delete-text"_str), nullptr);
    EXPECT_EQ(scene.scene->RootMut()->FindByName("original-text"_str), nullptr);
    EXPECT_EQ(scene.scene->RenderTargetNames().len(), targets);
    EXPECT_EQ(scene.scene->CameraNames().len(), cameras);
    owe::script::TickSceneScripts(*scene.scene, owe::script::FrameInputs {});
    EXPECT_FLOAT_EQ(creator->Translate().x(), 53.0f);
}

TEST(SceneDynamicLayer, UnmountsOnlyDeletedSoundAfterScriptTick) {
    using namespace rstd::prelude;
    auto document = owe::wpscene::ParseSceneDocumentJson(R"JSON({
        "camera": {},
        "general": {"orthogonalprojection": {"width": 1920, "height": 1080}},
        "objects": [
            {"id":1,"name":"sound-a","sound":["missing-a.ogg"],"volume":{"value":0.5,"user":"volume"},"playbackmode":"loop"},
            {"id":2,"name":"sound-b","sound":["missing-b.ogg"],"volume":{"value":0.5,"user":"volume"},"playbackmode":"loop"},
            {"id":3,"name":"creator","scale":{"value":"1 1 1","script":"let done = false; export function update(value) { if (!done) { thisScene.destroyLayer('sound-a'); done = true; } return value; }"}}
        ]
    })JSON"_str,
                                                         owe::wpscene::kSceneVersionUnknown);
    ASSERT_TRUE(document.is_some());
    auto assets = owe::fs::make_physical_fs(
        owe::fs::Path(rstd::cppstd::as_str(WAYWALLEN_ASSETS_DIR).unwrap()));
    ASSERT_TRUE(assets.is_ok());
    auto vfs = Box<owe::fs::VFS>::make();
    ASSERT_TRUE(vfs->mount("/assets"_str, rstd::move(assets).unwrap()).is_ok());
    wavsen::audio::SoundManager sound;
    usize                       unmounted {};
    owe::SceneParser            parser;
    auto                        parsed =
        parser.Parse("delete-sound"_str,
                     ref<owe::wpscene::SceneDocument>::from_raw_parts(rstd::addressof(*document)),
                     mut_ref<owe::fs::VFS>::from_raw_parts(vfs.get()),
                     mut_ref<wavsen::audio::SoundManager>::from_raw_parts(rstd::addressof(sound)),
                     { .sound_unmount = Some(owe::SceneSoundUnmountCallback::make(
                           [&sound, &unmounted](wavsen::audio::SoundStreamId id) {
                               ++unmounted;
                               (void)sound.unmount(id);
                           })) });
    ASSERT_TRUE(parsed.is_ok());
    auto scene = rstd::move(parsed).unwrap();
    scene.scene->InstallExtension(rstd::move(vfs));
    EXPECT_EQ(scene.scene->SoundVolumeBindings("volume"_str).len(), usize(2));
    owe::script::TickSceneScripts(*scene.scene, owe::script::FrameInputs {});
    EXPECT_EQ(scene.scene->RootMut()->FindByName("sound-a"_str), nullptr);
    EXPECT_NE(scene.scene->RootMut()->FindByName("sound-b"_str), nullptr);
    EXPECT_EQ(scene.scene->SoundVolumeBindings("volume"_str).len(), usize(1));
    EXPECT_EQ(unmounted, usize(1));
    owe::script::TickSceneScripts(*scene.scene, owe::script::FrameInputs {});
    EXPECT_EQ(unmounted, usize(1));
}

TEST(SceneSoundMount, CancelsBeforeOrAfterAttachExactlyOnce) {
    owe::SceneSoundMountState before;
    EXPECT_FALSE(before.IsCanceled());
    EXPECT_TRUE(before.Cancel().is_none());
    EXPECT_TRUE(before.IsCanceled());
    EXPECT_FALSE(before.Attach(wavsen::audio::SoundStreamId { rstd::u64(1) }));

    owe::SceneSoundMountState after;
    EXPECT_TRUE(after.Attach(wavsen::audio::SoundStreamId { rstd::u64(2) }));
    EXPECT_FALSE(after.Attach(wavsen::audio::SoundStreamId { rstd::u64(3) }));
    auto id = after.Cancel();
    ASSERT_TRUE(id.is_some());
    EXPECT_EQ(id->value, rstd::u64(2));
    EXPECT_TRUE(after.Cancel().is_none());
}

TEST(SceneSoundMount, ConcurrentAttachAndCancelHaveOneOwner) {
    using namespace rstd::prelude;
    for (int iteration = 0; iteration < 100; ++iteration) {
        owe::SceneSoundMountState        state;
        rstd::sync::atomic::Atomic<bool> start { false };
        auto                             worker = rstd::thread::spawn([&] {
                          while (! start.load()) rstd::thread::yield_now();
                          return state.Attach(wavsen::audio::SoundStreamId { u64(17) });
                                                  }).unwrap();
        start.store(true);
        auto canceled = state.Cancel();
        auto attached = rstd::move(worker).join().unwrap();
        EXPECT_EQ(attached, canceled.is_some());
        if (canceled.is_some()) EXPECT_EQ(canceled->value, u64(17));
        EXPECT_TRUE(state.Cancel().is_none());
    }
}

TEST(SceneSoundMount, DecodesAfterSourceVfsIsDestroyed) {
    using namespace rstd::prelude;
    auto temporary = rstd::fs::TempDir::make("owe-sound-vfs"_str).unwrap();
    auto root      = rstd::path::PathBuf::from(temporary.path().as_os_str().to_os_string());
    auto bytes     = Vec<u8>::make();
    auto tag       = [&bytes](ref<str> value) {
        bytes.extend_from_slice(value->as_bytes());
    };
    auto word = [&bytes](rstd::uint32_t value, int count) {
        for (int i = 0; i < count; ++i) bytes.push(u8((value >> (i * 8)) & 255u));
    };
    constexpr rstd::uint32_t data_size = 9600;
    tag("RIFF"_str);
    word(36 + data_size, 4);
    tag("WAVE"_str);
    tag("fmt "_str);
    word(16, 4);
    word(1, 2);
    word(1, 2);
    word(48000, 4);
    word(96000, 4);
    word(2, 2);
    word(16, 2);
    tag("data"_str);
    word(data_size, 4);
    for (rstd::uint32_t i = 0; i < data_size / 2; ++i) word(0x4000, 2);
    ASSERT_TRUE(rstd::fs::write(root.join("sound.wav"_str).as_path(), bytes.as_slice()).is_ok());
    auto prepared = [&] {
        owe::fs::VFS vfs;
        auto         local = owe::fs::make_physical_fs(temporary.path());
        EXPECT_TRUE(local.is_ok());
        EXPECT_TRUE(vfs.mount("/assets"_str, rstd::move(local).unwrap()).is_ok());
        owe::wpscene::SoundObject sound;
        sound.sound.push("sound.wav"_Str);
        return owe::SoundParser::PrepareAsync(
            sound,
            vfs,
            nullptr,
            owe::SceneSoundUnmountCallback::make([](wavsen::audio::SoundStreamId) {
            }));
    }();
    prepared.stream->stream().pass_desc({ u32(2), u32(48000) });
    float output[512] {};
    auto  frames = prepared.stream->stream().next_pcm(output, u32(256));
    EXPECT_GT(frames, u64());
    EXPECT_GT(output[0], 0.1f);
}

TEST(SceneDynamicLayer, CreatesAndCancelsAsynchronousSounds) {
    using namespace rstd::prelude;
    using rstd::sync::Arc;
    struct PendingSound {
        Box<dyn<wavsen::audio::SoundStreamObject>> stream;
        Arc<owe::SceneSoundMountState>             state;
    };
    auto document = owe::wpscene::ParseSceneDocumentJson(R"JSON({
        "camera": {},
        "general": {"orthogonalprojection": {"width": 1920, "height": 1080}},
        "objects": [{"id":1,"name":"creator","scale":{"value":"1 1 1",
            "script":"let stage=0, b; export function update(value) { if (stage === 0) { let a=thisScene.createLayer({name:'pending-a',sound:['sound/runtime.ogg'],volume:{value:0.5,user:'volume'},playbackmode:'loop',startsilent:true}); b=thisScene.createLayer('sound/runtime.ogg'); let failed=0; try { thisScene.createLayer({name:'failed-sound',sound:['sound/runtime.ogg'],volume:1,playbackmode:'loop',scale:{value:'1 1 1',script:'throw new Error(7); export function update(v) { return v; }'}}); } catch(e) { failed++; } try { thisScene.createLayer({name:'rejected-sound',sound:['sound/runtime.ogg'],volume:1,playbackmode:'loop'}); } catch(e) { failed++; } if (a.volume === 0.75 && !a.isPlaying() && b.isPlaying() && failed === 2) thisLayer.origin=new Vec3(17,0,0); thisScene.destroyLayer(a); stage=1; } else if (stage === 1) { thisScene.destroyLayer(b); stage=2; } return value; }"}}]
    })JSON"_str,
                                                         owe::wpscene::kSceneVersionUnknown);
    ASSERT_TRUE(document.is_some());
    auto assets = owe::fs::make_physical_fs(
        owe::fs::Path(rstd::cppstd::as_str(WAYWALLEN_ASSETS_DIR).unwrap()));
    ASSERT_TRUE(assets.is_ok());
    auto temporary = rstd::fs::TempDir::make("owe-runtime-sound"_str).unwrap();
    auto root      = rstd::path::PathBuf::from(temporary.path().as_os_str().to_os_string());
    ASSERT_TRUE(rstd::fs::create_dir(root.join("sound"_str).as_path()).is_ok());
    ASSERT_TRUE(
        rstd::fs::write(root.join("sound/runtime.ogg"_str).as_path(), "audio"_bytes).is_ok());
    auto local = owe::fs::make_physical_fs(temporary.path());
    ASSERT_TRUE(local.is_ok());
    auto vfs = Box<owe::fs::VFS>::make();
    ASSERT_TRUE(vfs->mount("/assets"_str, rstd::move(assets).unwrap()).is_ok());
    ASSERT_TRUE(vfs->mount("/assets"_str, rstd::move(local).unwrap()).is_ok());
    wavsen::audio::SoundManager sound;
    Vec<PendingSound>           pending;
    usize                       attempts {}, unmounted {};
    auto             properties = owe::ParseJson(R"({"volume":{"value":0.75}})"_str).unwrap();
    owe::SceneParser parser;
    auto             parsed =
        parser.Parse("runtime-sound"_str,
                     ref<owe::wpscene::SceneDocument>::from_raw_parts(rstd::addressof(*document)),
                     mut_ref<owe::fs::VFS>::from_raw_parts(vfs.get()),
                     mut_ref<wavsen::audio::SoundManager>::from_raw_parts(rstd::addressof(sound)),
                     { .user_properties = Some(*properties.as_object()),
                       .sound_unmount   = Some(owe::SceneSoundUnmountCallback::make(
                           [&sound, &unmounted](wavsen::audio::SoundStreamId id) {
                               ++unmounted;
                               (void)sound.unmount(id);
                           })),
                       .sound_mount     = Some(owe::SceneSoundMountCallback::make(
                           [&pending, &attempts](Box<dyn<wavsen::audio::SoundStreamObject>> stream,
                                                 Arc<owe::SceneSoundMountState>
                                                     state) {
                               ++attempts;
                               if (attempts == usize(3)) return false;
                               pending.push({ rstd::move(stream), rstd::move(state) });
                               return true;
                           })) });
    ASSERT_TRUE(parsed.is_ok());
    auto scene = rstd::move(parsed).unwrap();
    scene.scene->InstallExtension(rstd::move(vfs));
    owe::script::TickSceneScripts(*scene.scene, owe::script::FrameInputs {});
    auto* creator = scene.scene->RootMut()->FindByName("creator"_str);
    ASSERT_NE(creator, nullptr);
    EXPECT_FLOAT_EQ(creator->Translate().x(), 17.0f);
    EXPECT_EQ(attempts, usize(3));
    ASSERT_EQ(pending.len(), usize(2));
    EXPECT_TRUE(pending[usize()].state->IsCanceled());
    EXPECT_FALSE(pending[usize()].state->Attach(wavsen::audio::SoundStreamId { u64(99) }));
    EXPECT_FALSE(pending[usize(1)].state->IsCanceled());
    EXPECT_EQ(scene.scene->RootMut()->FindByName("pending-a"_str), nullptr);
    EXPECT_EQ(scene.scene->RootMut()->FindByName("failed-sound"_str), nullptr);
    EXPECT_EQ(scene.scene->RootMut()->FindByName("rejected-sound"_str), nullptr);
    ASSERT_NE(scene.scene->RootMut()->FindByName("sound/runtime.ogg"_str), nullptr);
    auto id = sound.mount(rstd::move(pending[usize(1)].stream));
    ASSERT_TRUE(id.is_some());
    ASSERT_TRUE(pending[usize(1)].state->Attach(*id));
    owe::script::TickSceneScripts(*scene.scene, owe::script::FrameInputs {});
    EXPECT_TRUE(pending[usize(1)].state->IsCanceled());
    EXPECT_EQ(unmounted, usize(1));
    EXPECT_EQ(scene.scene->RootMut()->FindByName("sound/runtime.ogg"_str), nullptr);
}

TEST(SceneDynamicLayer, PreservesEarlierLayersDuringRepeatedCreation) {
    using namespace rstd::prelude;
    auto document = owe::wpscene::ParseSceneDocumentJson(R"JSON({
        "camera": {},
        "general": {"orthogonalprojection": {"width": 1920, "height": 1080}},
        "objects": [{"id": 1, "name": "creator", "scale": {
            "value": "1 1 1",
            "script": "let n = 0; export function update(value) { for (let i = 0; i < 4; ++i) { let layer = thisScene.createLayer({image:'models/util/solidlayer.json', name:'point-' + n, size:'8 8'}); layer.origin = new Vec3(n++, 0, 0); } return value; }"
        }}]
    })JSON"_str,
                                                         owe::wpscene::kSceneVersionUnknown);
    ASSERT_TRUE(document.is_some());
    auto assets = owe::fs::make_physical_fs(
        owe::fs::Path(rstd::cppstd::as_str(WAYWALLEN_ASSETS_DIR).unwrap()));
    ASSERT_TRUE(assets.is_ok());
    auto vfs = Box<owe::fs::VFS>::make();
    ASSERT_TRUE(vfs->mount("/assets"_str, rstd::move(assets).unwrap()).is_ok());
    wavsen::audio::SoundManager sound;
    owe::SceneParser            parser;
    auto                        parsed =
        parser.Parse("runtime-image-repeated"_str,
                     ref<owe::wpscene::SceneDocument>::from_raw_parts(rstd::addressof(*document)),
                     mut_ref<owe::fs::VFS>::from_raw_parts(vfs.get()),
                     mut_ref<wavsen::audio::SoundManager>::from_raw_parts(rstd::addressof(sound)));
    ASSERT_TRUE(parsed.is_ok());
    auto scene = rstd::move(parsed).unwrap();
    scene.scene->InstallExtension(rstd::move(vfs));
    owe::script::TickSceneScripts(*scene.scene, owe::script::FrameInputs {});
    auto* first = scene.scene->RootMut()->FindByName("point-0"_str);
    ASSERT_NE(first, nullptr);
    auto first_id = scene.scene->ResourceIndex().nodeId(*first);
    ASSERT_TRUE(first_id.is_some());
    auto first_draw = scene.scene->ResourceIndex().drawItemFor(*first_id, u32());
    ASSERT_TRUE(first_draw.is_some());
    for (int frame = 1; frame < 400; ++frame)
        owe::script::TickSceneScripts(*scene.scene, owe::script::FrameInputs {});
    auto* last = scene.scene->RootMut()->FindByName("point-1599"_str);
    ASSERT_NE(last, nullptr);
    EXPECT_FLOAT_EQ(last->Translate().x(), 1599.0f);
    EXPECT_EQ(scene.scene->ResourceIndex().nodeId(*first), first_id);
    EXPECT_EQ(scene.scene->ResourceIndex().drawItemFor(*first_id, u32()), first_draw);
    EXPECT_EQ(scene.scene->ResourceIndex().resolve(*first_draw)->node, first);
}

TEST(SceneDynamicLayer, RollsBackImageWhenDeferredModuleFails) {
    using namespace rstd::prelude;
    auto document = owe::wpscene::ParseSceneDocumentJson(R"JSON({
        "camera": {},
        "general": {"orthogonalprojection": {"width": 1920, "height": 1080}},
        "objects": [{"id": 1, "name": "creator", "scale": {
            "value": "1 1 1",
            "script": "let done = false; export function update(value) { if (!done) { let failures = 0; try { thisScene.createLayer({image:'models/runtime-bad.json', name:'bad-material', size:'16 16'}); } catch (e) { failures++; } try { thisScene.createLayer({image:'models/util/solidlayer.json', name:'failed-runtime', size:'32 32', effects:[{file:'effects/blur/effect.json'}], alpha:{value:1, script:'export function init() { return 1; }'}, scale:{value:'1 1 1', script:'throw new Error(7); export function update(v) { return v; }'}}); } catch (e) { failures++; } if (failures === 2) { thisLayer.origin = new Vec3(17, 0, 0); thisScene.createLayer({image:'models/util/solidlayer.json', name:'after-rollback', size:'16 16'}); } done = true; } return value; }"
        }}]
    })JSON"_str,
                                                         owe::wpscene::kSceneVersionUnknown);
    ASSERT_TRUE(document.is_some());
    auto assets = owe::fs::make_physical_fs(
        owe::fs::Path(rstd::cppstd::as_str(WAYWALLEN_ASSETS_DIR).unwrap()));
    ASSERT_TRUE(assets.is_ok());
    auto vfs = Box<owe::fs::VFS>::make();
    ASSERT_TRUE(vfs->mount("/assets"_str, rstd::move(assets).unwrap()).is_ok());
    auto effect_assets = owe::fs::make_physical_fs(owe::fs::Path(
        rstd::cppstd::as_str(std::string(WAYWALLEN_ASSETS_DIR) + "/effects/blur").unwrap()));
    ASSERT_TRUE(effect_assets.is_ok());
    ASSERT_TRUE(vfs->mount("/assets"_str, rstd::move(effect_assets).unwrap()).is_ok());
    auto temporary = rstd::fs::TempDir::make("owe-runtime-failure"_str).unwrap();
    auto root      = rstd::path::PathBuf::from(temporary.path().as_os_str().to_os_string());
    ASSERT_TRUE(rstd::fs::create_dir(root.join("models"_str).as_path()).is_ok());
    ASSERT_TRUE(rstd::fs::create_dir(root.join("materials"_str).as_path()).is_ok());
    ASSERT_TRUE(
        rstd::fs::write(root.join("models/runtime-bad.json"_str).as_path(),
                        R"({"width":16,"height":16,"material":"materials/runtime-bad.json"})"_bytes)
            .is_ok());
    ASSERT_TRUE(rstd::fs::write(root.join("materials/runtime-bad.json"_str).as_path(),
                                R"({"passes":[{"shader":"missing-runtime-shader"}]})"_bytes)
                    .is_ok());
    auto local = owe::fs::make_physical_fs(temporary.path());
    ASSERT_TRUE(local.is_ok());
    ASSERT_TRUE(vfs->mount("/assets"_str, rstd::move(local).unwrap()).is_ok());
    wavsen::audio::SoundManager sound;
    owe::SceneParser            parser;
    auto                        parsed =
        parser.Parse("runtime-image-rollback"_str,
                     ref<owe::wpscene::SceneDocument>::from_raw_parts(rstd::addressof(*document)),
                     mut_ref<owe::fs::VFS>::from_raw_parts(vfs.get()),
                     mut_ref<wavsen::audio::SoundManager>::from_raw_parts(rstd::addressof(sound)));
    ASSERT_TRUE(parsed.is_ok());
    auto scene = rstd::move(parsed).unwrap();
    scene.scene->InstallExtension(rstd::move(vfs));
    auto before_targets = scene.scene->RenderTargetNames().len();
    auto before_cameras = scene.scene->CameraNames().len();
    owe::script::TickSceneScripts(*scene.scene, owe::script::FrameInputs {});
    auto* creator = scene.scene->RootMut()->FindByName("creator"_str);
    ASSERT_NE(creator, nullptr);
    EXPECT_FLOAT_EQ(creator->Translate().x(), 17.0f);
    EXPECT_EQ(scene.scene->RootMut()->FindByName("failed-runtime"_str), nullptr);
    EXPECT_EQ(scene.scene->RootMut()->FindByName("bad-material"_str), nullptr);
    EXPECT_NE(scene.scene->RootMut()->FindByName("after-rollback"_str), nullptr);
    EXPECT_EQ(scene.scene->RenderTargetNames().len(), before_targets);
    EXPECT_EQ(scene.scene->CameraNames().len(), before_cameras);
    auto scripts = scene.scene->ExtensionMut<owe::script::ScriptScene>();
    ASSERT_TRUE(scripts.is_some());
    int count = 0;
    (**scripts).runtime().ForEachScript(
        [](owe::script::FieldScript*, void* out) {
            ++*static_cast<int*>(out);
        },
        &count);
    EXPECT_EQ(count, 1);
    owe::script::TickSceneScripts(*scene.scene, owe::script::FrameInputs {});
}

TEST(SceneDynamicLayer, CreatesUnregisteredImageAssetsDuringUpdate) {
    using namespace rstd::prelude;
    auto document = owe::wpscene::ParseSceneDocumentJson(R"JSON({
        "camera": {},
        "general": {"orthogonalprojection": {"width": 1920, "height": 1080}},
        "objects": [{"id": 1, "name": "creator", "scale": {
            "value": "1 1 1",
            "script": "export const __workshopId = '123'; let done = false; export function update(value) { if (!done) { let a = thisScene.createLayer('models/runtime-unregistered.json'); let b = thisScene.createLayer('models/runtime-unregistered.json'); a.origin = new Vec3(10, 0, 0); b.origin = new Vec3(20, 0, 0); let automatic = thisScene.createLayer('models/runtime-unregistered-auto.json'); let cross = thisScene.createLayer('models/runtime-cross.json'); let missing = false; try { thisScene.createLayer('models/missing-runtime.json'); } catch (e) { missing = true; } if (a.origin.x === 10 && b.origin.x === 20 && automatic && cross && missing) thisLayer.origin = new Vec3(29, 0, 0); done = true; } return value; }"
        }}]
    })JSON"_str,
                                                         owe::wpscene::kSceneVersionUnknown);
    ASSERT_TRUE(document.is_some());
    auto assets = owe::fs::make_physical_fs(
        owe::fs::Path(rstd::cppstd::as_str(WAYWALLEN_ASSETS_DIR).unwrap()));
    ASSERT_TRUE(assets.is_ok());
    auto temporary = rstd::fs::TempDir::make("owe-runtime-unregistered"_str).unwrap();
    auto root      = rstd::path::PathBuf::from(temporary.path().as_os_str().to_os_string());
    ASSERT_TRUE(rstd::fs::create_dir(root.join("models"_str).as_path()).is_ok());
    ASSERT_TRUE(rstd::fs::create_dir(root.join("models/workshop"_str).as_path()).is_ok());
    ASSERT_TRUE(rstd::fs::create_dir(root.join("models/workshop/123"_str).as_path()).is_ok());
    ASSERT_TRUE(
        rstd::fs::write(
            root.join("models/runtime-unregistered.json"_str).as_path(),
            R"({"width":48,"height":24,"material":"materials/util/solidlayer_instance_3.json"})"_bytes)
            .is_ok());
    ASSERT_TRUE(
        rstd::fs::write(
            root.join("models/runtime-unregistered-auto.json"_str).as_path(),
            R"({"autosize":true,"material":"materials/util/solidlayer_instance_3.json"})"_bytes)
            .is_ok());
    ASSERT_TRUE(
        rstd::fs::write(
            root.join("models/workshop/123/runtime-cross.json"_str).as_path(),
            R"({"width":16,"height":12,"material":"materials/util/solidlayer_instance_3.json"})"_bytes)
            .is_ok());
    auto local = owe::fs::make_physical_fs(temporary.path());
    ASSERT_TRUE(local.is_ok());
    auto vfs = Box<owe::fs::VFS>::make();
    ASSERT_TRUE(vfs->mount("/assets"_str, rstd::move(assets).unwrap()).is_ok());
    ASSERT_TRUE(vfs->mount("/assets"_str, rstd::move(local).unwrap()).is_ok());
    wavsen::audio::SoundManager sound;
    owe::SceneParser            parser;
    auto                        parsed =
        parser.Parse("unregistered-image"_str,
                     ref<owe::wpscene::SceneDocument>::from_raw_parts(rstd::addressof(*document)),
                     mut_ref<owe::fs::VFS>::from_raw_parts(vfs.get()),
                     mut_ref<wavsen::audio::SoundManager>::from_raw_parts(rstd::addressof(sound)));
    ASSERT_TRUE(parsed.is_ok());
    auto scene = rstd::move(parsed).unwrap();
    scene.scene->InstallExtension(rstd::move(vfs));
    owe::script::TickSceneScripts(*scene.scene, owe::script::FrameInputs {});
    auto* creator = scene.scene->RootMut()->FindByName("creator"_str);
    auto* image   = scene.scene->RootMut()->FindByName("models/runtime-unregistered.json"_str);
    auto* automatic =
        scene.scene->RootMut()->FindByName("models/runtime-unregistered-auto.json"_str);
    auto* cross = scene.scene->RootMut()->FindByName("models/workshop/123/runtime-cross.json"_str);
    ASSERT_NE(creator, nullptr);
    ASSERT_NE(image, nullptr);
    ASSERT_NE(automatic, nullptr);
    ASSERT_NE(cross, nullptr);
    EXPECT_FLOAT_EQ(creator->Translate().x(), 29.0f);
    EXPECT_FLOAT_EQ(image->Size().x(), 48.0f);
    EXPECT_FLOAT_EQ(image->Size().y(), 24.0f);
    EXPECT_GT(automatic->Size().x(), 0.0f);
    EXPECT_GT(automatic->Size().y(), 0.0f);
    EXPECT_FLOAT_EQ(cross->Size().x(), 16.0f);
    EXPECT_FLOAT_EQ(cross->Size().y(), 12.0f);
    EXPECT_TRUE(scene.scene->ResourceIndex().nodeId(*image).is_some());
    EXPECT_TRUE(scene.scene->ResourceIndex().nodeId(*automatic).is_some());
}

TEST(SceneDynamicLayer, CreatesShapeWithEffectsDuringUpdate) {
    using namespace rstd::prelude;
    auto document = owe::wpscene::ParseSceneDocumentJson(R"JSON({
        "camera": {},
        "general": {"orthogonalprojection": {"width": 1920, "height": 1080}},
        "objects": [{"id": 1, "name": "creator", "scale": {
            "value": "1 1 1",
            "script": "let shape, stage = 0; export function update(value) { if (stage === 0) { shape = thisScene.createLayer({name:'runtime-shape', shape:'quad', origin:[0,0,0], angles:[0,0,0], scale:[1,1,1], effects:[{file:'effects/lightshafts/effect.json', visible:true, passes:[{combos:{DIRECTDRAW:1,RAYMODE:1}, constantshadervalues:{point0:'-1 -1',point1:'-1 1',point2:'1 1',point3:'1 -1'}}]}]}); shape.origin = new Vec3(17, 0, 0); if (shape.origin.x === 17) thisLayer.origin = new Vec3(29, 0, 0); stage = 1; } else if (stage === 1) { if (thisScene.destroyLayer(shape)) thisLayer.origin = new Vec3(31, 0, 0); stage = 2; } return value; }"
        }}]
    })JSON"_str,
                                                         owe::wpscene::kSceneVersionUnknown);
    ASSERT_TRUE(document.is_some());
    auto assets = owe::fs::make_physical_fs(
        owe::fs::Path(rstd::cppstd::as_str(WAYWALLEN_ASSETS_DIR).unwrap()));
    auto effect_assets = owe::fs::make_physical_fs(owe::fs::Path(
        rstd::cppstd::as_str(std::string(WAYWALLEN_ASSETS_DIR) + "/effects/lightshafts").unwrap()));
    ASSERT_TRUE(assets.is_ok());
    ASSERT_TRUE(effect_assets.is_ok());
    auto vfs = Box<owe::fs::VFS>::make();
    ASSERT_TRUE(vfs->mount("/assets"_str, rstd::move(assets).unwrap()).is_ok());
    ASSERT_TRUE(vfs->mount("/assets"_str, rstd::move(effect_assets).unwrap()).is_ok());
    wavsen::audio::SoundManager sound;
    owe::SceneParser            parser;
    auto                        parsed =
        parser.Parse("runtime-shape"_str,
                     ref<owe::wpscene::SceneDocument>::from_raw_parts(rstd::addressof(*document)),
                     mut_ref<owe::fs::VFS>::from_raw_parts(vfs.get()),
                     mut_ref<wavsen::audio::SoundManager>::from_raw_parts(rstd::addressof(sound)));
    ASSERT_TRUE(parsed.is_ok());
    auto scene = rstd::move(parsed).unwrap();
    scene.scene->InstallExtension(rstd::move(vfs));
    auto target_count = scene.scene->RenderTargetNames().len();
    auto camera_count = scene.scene->CameraNames().len();
    owe::script::TickSceneScripts(*scene.scene, owe::script::FrameInputs {});
    auto* creator = scene.scene->RootMut()->FindByName("creator"_str);
    auto* shape   = scene.scene->RootMut()->FindByName("runtime-shape"_str);
    ASSERT_NE(creator, nullptr);
    ASSERT_NE(shape, nullptr);
    EXPECT_FLOAT_EQ(creator->Translate().x(), 29.0f);
    auto id = scene.scene->ResourceIndex().nodeId(*shape);
    ASSERT_TRUE(id.is_some());
    EXPECT_GE(scene.scene->NodeSources(*id).len(), usize(1));
    owe::script::TickSceneScripts(*scene.scene, owe::script::FrameInputs {});
    EXPECT_FLOAT_EQ(creator->Translate().x(), 31.0f);
    EXPECT_EQ(scene.scene->RootMut()->FindByName("runtime-shape"_str), nullptr);
    EXPECT_EQ(scene.scene->RenderTargetNames().len(), target_count);
    EXPECT_EQ(scene.scene->CameraNames().len(), camera_count);
}

TEST(SceneDynamicLayer, CreatesModelAssetsAndConfigurations) {
    using namespace rstd::prelude;
    auto document = owe::wpscene::ParseSceneDocumentJson(R"JSON({
        "camera": {},
        "general": {"orthogonalprojection": {"width": 1920, "height": 1080}},
        "objects": [{"id": 1, "name": "creator", "scale": {
            "value": "1 1 1",
            "script": "const asset = engine.registerAsset('models/editor/camera/camera.mdl'); let done = false; export function init(value) { let initial = thisScene.createLayer(asset); initial.origin = new Vec3(33, 0, 0); return value; } export function update(value) { if (!done) { let a = thisScene.createLayer(asset); let b = thisScene.createLayer({model:'models/editor/camera/camera.mdl', name:'configured-model'}); a.origin = new Vec3(11, 0, 0); b.origin = new Vec3(22, 0, 0); if (a.origin.x === 11 && b.origin.x === 22) thisLayer.origin = new Vec3(37, 0, 0); done = true; } return value; }"
        }}]
    })JSON"_str,
                                                         owe::wpscene::kSceneVersionUnknown);
    ASSERT_TRUE(document.is_some());
    auto assets = owe::fs::make_physical_fs(
        owe::fs::Path(rstd::cppstd::as_str(WAYWALLEN_ASSETS_DIR).unwrap()));
    ASSERT_TRUE(assets.is_ok());
    auto temporary = rstd::fs::TempDir::make("owe-runtime-model"_str).unwrap();
    auto root      = rstd::path::PathBuf::from(temporary.path().as_os_str().to_os_string());
    ASSERT_TRUE(rstd::fs::create_dir(root.join("materials"_str).as_path()).is_ok());
    ASSERT_TRUE(rstd::fs::create_dir(root.join("materials/models"_str).as_path()).is_ok());
    ASSERT_TRUE(
        rstd::fs::create_dir(root.join("materials/models/editorcamera"_str).as_path()).is_ok());
    ASSERT_TRUE(
        rstd::fs::write(
            root.join("materials/models/editorcamera/editorcamera.json"_str).as_path(),
            R"({"passes":[{"shader":"flat","cullmode":"nocull","depthtest":"disabled","depthwrite":"disabled","blending":"translucent"}]})"_bytes)
            .is_ok());
    auto local = owe::fs::make_physical_fs(temporary.path());
    ASSERT_TRUE(local.is_ok());
    auto vfs = Box<owe::fs::VFS>::make();
    ASSERT_TRUE(vfs->mount("/assets"_str, rstd::move(assets).unwrap()).is_ok());
    ASSERT_TRUE(vfs->mount("/assets"_str, rstd::move(local).unwrap()).is_ok());
    wavsen::audio::SoundManager sound;
    owe::SceneParser            parser;
    auto                        parsed =
        parser.Parse("runtime-model"_str,
                     ref<owe::wpscene::SceneDocument>::from_raw_parts(rstd::addressof(*document)),
                     mut_ref<owe::fs::VFS>::from_raw_parts(vfs.get()),
                     mut_ref<wavsen::audio::SoundManager>::from_raw_parts(rstd::addressof(sound)));
    ASSERT_TRUE(parsed.is_ok());
    auto scene = rstd::move(parsed).unwrap();
    scene.scene->InstallExtension(rstd::move(vfs));
    auto* initial_model = scene.scene->RootMut()->FindByName("models/editor/camera/camera.mdl"_str);
    ASSERT_NE(initial_model, nullptr);
    ASSERT_NE(initial_model->Mesh(), nullptr);
    EXPECT_FLOAT_EQ(initial_model->Translate().x(), 33.0f);
    owe::script::TickSceneScripts(*scene.scene, owe::script::FrameInputs {});
    auto*           creator = scene.scene->RootMut()->FindByName("creator"_str);
    owe::SceneNode* asset   = nullptr;
    for (const auto& child : scene.scene->Root()->GetChildren())
        if (child.as_ptr().as_raw_ptr() != initial_model &&
            child->Name() == "models/editor/camera/camera.mdl"_str)
            asset = child.as_ptr();
    auto* configured = scene.scene->RootMut()->FindByName("configured-model"_str);
    ASSERT_NE(creator, nullptr);
    ASSERT_NE(asset, nullptr);
    ASSERT_NE(configured, nullptr);
    ASSERT_NE(asset->Mesh(), nullptr);
    ASSERT_NE(configured->Mesh(), nullptr);
    EXPECT_FLOAT_EQ(creator->Translate().x(), 37.0f);
}

TEST(SceneDynamicLayer, CreatesUnregisteredParticleWithoutInitialParticles) {
    using namespace rstd::prelude;
    auto document = owe::wpscene::ParseSceneDocumentJson(R"JSON({
        "camera": {},
        "general": {"orthogonalprojection": {"width": 1920, "height": 1080}},
        "objects": [{"id": 1, "name": "creator", "scale": {
            "value": "1 1 1",
            "script": "let done = false; export function update(value) { if (!done) { let a = thisScene.createLayer('particles/runtime.json'); let b = thisScene.createLayer('particles/runtime.json'); a.origin = new Vec3(11, 0, 0); b.origin = new Vec3(22, 0, 0); let configured = thisScene.createLayer({particle:'particles/runtime.json', name:'configured-particle', origin:'0 0 0', scale:{value:'1 1 1', script:'export function init(value) { thisLayer.origin = new Vec3(33, 0, 0); return value; }'}}); let missing = false; try { thisScene.createLayer('particles/missing.json'); } catch (e) { missing = true; } let failed = false; try { thisScene.createLayer({particle:'particles/runtime.json', name:'failed-particle', scale:{value:'1 1 1', script:'throw new Error(7); export function update(v) { return v; }'}}); } catch (e) { failed = true; } if (a.origin.x === 11 && b.origin.x === 22 && configured.origin.x === 33 && missing && failed) thisLayer.origin = new Vec3(37, 0, 0); done = true; } return value; }"
        }}]
    })JSON"_str,
                                                         owe::wpscene::kSceneVersionUnknown);
    ASSERT_TRUE(document.is_some());
    auto assets = owe::fs::make_physical_fs(
        owe::fs::Path(rstd::cppstd::as_str(WAYWALLEN_ASSETS_DIR).unwrap()));
    ASSERT_TRUE(assets.is_ok());
    auto temporary = rstd::fs::TempDir::make("owe-runtime-particle"_str).unwrap();
    auto root      = rstd::path::PathBuf::from(temporary.path().as_os_str().to_os_string());
    ASSERT_TRUE(rstd::fs::create_dir(root.join("particles"_str).as_path()).is_ok());
    ASSERT_TRUE(rstd::fs::create_dir(root.join("materials"_str).as_path()).is_ok());
    ASSERT_TRUE(
        rstd::fs::write(
            root.join("particles/runtime.json"_str).as_path(),
            R"({"material":"materials/runtime-particle.json","maxcount":16,"emitter":[{"name":"sphererandom","rate":1}],"initializer":[{"name":"lifetimerandom","min":1,"max":2}]})"_bytes)
            .is_ok());
    ASSERT_TRUE(
        rstd::fs::write(
            root.join("materials/runtime-particle.json"_str).as_path(),
            R"({"passes":[{"shader":"genericparticle","depthtest":"disabled","depthwrite":"disabled","blending":"additive","cullmode":"nocull","textures":["particle/halo"]}]})"_bytes)
            .is_ok());
    auto local = owe::fs::make_physical_fs(temporary.path());
    ASSERT_TRUE(local.is_ok());
    auto vfs = Box<owe::fs::VFS>::make();
    ASSERT_TRUE(vfs->mount("/assets"_str, rstd::move(assets).unwrap()).is_ok());
    ASSERT_TRUE(vfs->mount("/assets"_str, rstd::move(local).unwrap()).is_ok());
    wavsen::audio::SoundManager sound;
    owe::SceneParser            parser;
    auto                        parsed =
        parser.Parse("unregistered-particle"_str,
                     ref<owe::wpscene::SceneDocument>::from_raw_parts(rstd::addressof(*document)),
                     mut_ref<owe::fs::VFS>::from_raw_parts(vfs.get()),
                     mut_ref<wavsen::audio::SoundManager>::from_raw_parts(rstd::addressof(sound)));
    ASSERT_TRUE(parsed.is_ok());
    auto scene = rstd::move(parsed).unwrap();
    scene.scene->InstallExtension(rstd::move(vfs));
    owe::script::TickSceneScripts(*scene.scene, owe::script::FrameInputs {});
    auto* creator    = scene.scene->RootMut()->FindByName("creator"_str);
    auto* particle   = scene.scene->RootMut()->FindByName("particles/runtime.json"_str);
    auto* configured = scene.scene->RootMut()->FindByName("configured-particle"_str);
    ASSERT_NE(creator, nullptr);
    ASSERT_NE(particle, nullptr);
    ASSERT_NE(configured, nullptr);
    EXPECT_EQ(scene.scene->RootMut()->FindByName("failed-particle"_str), nullptr);
    EXPECT_FLOAT_EQ(creator->Translate().x(), 37.0f);
    ASSERT_NE(particle->Mesh(), nullptr);
    auto id = scene.scene->ResourceIndex().nodeId(*particle);
    ASSERT_TRUE(id.is_some());
    EXPECT_GE(scene.scene->NodeSources(*id).len(), usize(3));
}

TEST(SceneDynamicLayer, CreatesRegisteredImageAndParticleDuringUpdate) {
    using namespace rstd::prelude;
    auto document = owe::wpscene::ParseSceneDocumentJson(R"JSON({
        "camera": {},
        "general": {"orthogonalprojection": {"width": 1920, "height": 1080}},
        "objects": [{"id": 1, "name": "creator", "scale": {
            "value": "1 1 1",
            "script": "const asset = engine.registerAsset('models/runtime-image.json'); const particle = engine.registerAsset('particles/runtime.json'); let initial, created; export function init(value) { initial = thisScene.createLayer(asset); initial.origin = new Vec3(33, 0, 0); initial.alignment = 'top'; return value; } export function update(value) { if (!created) { created = thisScene.createLayer(asset); created.origin = new Vec3(10, 20, 0); created.alignment = 'bottom'; if (initial.alignment === 'top' && created.alignment === 'bottom') thisLayer.origin = new Vec3(47, 0, 0); thisScene.createLayer(particle); } return value; }"
        }}, {"id": 2, "name": "initial-particle", "particle": "particles/runtime.json"}]
    })JSON"_str,
                                                         owe::wpscene::kSceneVersionUnknown);
    ASSERT_TRUE(document.is_some());
    auto assets = owe::fs::make_physical_fs(
        owe::fs::Path(rstd::cppstd::as_str(WAYWALLEN_ASSETS_DIR).unwrap()));
    ASSERT_TRUE(assets.is_ok());
    auto temporary = rstd::fs::TempDir::make("owe-runtime-layer"_str).unwrap();
    auto root      = rstd::path::PathBuf::from(temporary.path().as_os_str().to_os_string());
    ASSERT_TRUE(rstd::fs::create_dir(root.join("models"_str).as_path()).is_ok());
    ASSERT_TRUE(rstd::fs::create_dir(root.join("particles"_str).as_path()).is_ok());
    ASSERT_TRUE(rstd::fs::create_dir(root.join("materials"_str).as_path()).is_ok());
    ASSERT_TRUE(
        rstd::fs::write(
            root.join("models/runtime-image.json"_str).as_path(),
            R"({"width":32,"height":32,"material":"materials/util/solidlayer.json","solidlayer":true})"_bytes)
            .is_ok());
    ASSERT_TRUE(
        rstd::fs::write(
            root.join("particles/runtime.json"_str).as_path(),
            R"({"material":"materials/runtime-particle.json","maxcount":16,"emitter":[{"name":"sphererandom","rate":1}],"initializer":[{"name":"lifetimerandom","min":1,"max":2}]})"_bytes)
            .is_ok());
    ASSERT_TRUE(
        rstd::fs::write(
            root.join("materials/runtime-particle.json"_str).as_path(),
            R"({"passes":[{"shader":"genericparticle","depthtest":"disabled","depthwrite":"disabled","blending":"additive","cullmode":"nocull","textures":["particle/halo"],"constantshadervalues":{"ui_editor_properties_overbright":{"value":2.5,"animation":{"c0":[{"frame":0,"value":2.5},{"frame":60,"value":3.0}]}}}}]})"_bytes)
            .is_ok());
    auto local = owe::fs::make_physical_fs(temporary.path());
    ASSERT_TRUE(local.is_ok());
    auto vfs = Box<owe::fs::VFS>::make();
    ASSERT_TRUE(vfs->mount("/assets"_str, rstd::move(assets).unwrap_unchecked()).is_ok());
    ASSERT_TRUE(vfs->mount("/assets"_str, rstd::move(local).unwrap_unchecked()).is_ok());
    wavsen::audio::SoundManager sound;
    owe::SceneParser            parser;
    auto                        parsed =
        parser.Parse("registered-image"_str,
                     ref<owe::wpscene::SceneDocument>::from_raw_parts(rstd::addressof(*document)),
                     mut_ref<owe::fs::VFS>::from_raw_parts(vfs.get()),
                     mut_ref<wavsen::audio::SoundManager>::from_raw_parts(rstd::addressof(sound)));
    ASSERT_TRUE(parsed.is_ok());
    auto scene = rstd::move(parsed).unwrap();
    scene.scene->InstallExtension(rstd::move(vfs));
    auto* initial_image = scene.scene->RootMut()->FindByName("models/runtime-image.json"_str);
    ASSERT_NE(initial_image, nullptr);
    ASSERT_NE(initial_image->Mesh(), nullptr);
    EXPECT_FLOAT_EQ(initial_image->Translate().x(), 33.0f);
    auto initial_image_id = scene.scene->ResourceIndex().nodeId(*initial_image);
    ASSERT_TRUE(initial_image_id.is_some());
    EXPECT_GE(scene.scene->NodeSources(*initial_image_id).len(), usize(1));
    owe::script::TickSceneScripts(*scene.scene, owe::script::FrameInputs {});

    owe::SceneNode* created = nullptr;
    for (const auto& child : scene.scene->Root()->GetChildren())
        if (child.as_ptr().as_raw_ptr() != initial_image &&
            child->Name() == "models/runtime-image.json"_str)
            created = child.as_ptr();
    ASSERT_NE(created, nullptr);
    ASSERT_NE(created->Mesh(), nullptr);
    auto* creator = scene.scene->RootMut()->FindByName("creator"_str);
    ASSERT_NE(creator, nullptr);
    EXPECT_FLOAT_EQ(creator->Translate().x(), 47.0f);
    EXPECT_FLOAT_EQ(created->Translate().x(), 10.0f);
    EXPECT_FLOAT_EQ(created->Translate().y(), 20.0f);
    EXPECT_TRUE(scene.scene->ResourceIndex().nodeId(*created).is_some());
    auto* particle = scene.scene->RootMut()->FindByName("particles/runtime.json"_str);
    ASSERT_NE(particle, nullptr);
    auto particle_id = scene.scene->ResourceIndex().nodeId(*particle);
    ASSERT_TRUE(particle_id.is_some());
    EXPECT_GE(scene.scene->NodeSources(*particle_id).len(), usize(3));
    ASSERT_NE(particle->Mesh(), nullptr);
    ASSERT_NE(particle->Mesh()->Material(), nullptr);
    auto animation = particle->Mesh()->Material()->ShaderValueAnimation("g_Overbright"_str);
    ASSERT_TRUE(animation.is_some());
    auto* initial_particle = scene.scene->RootMut()->FindByName("initial-particle"_str);
    ASSERT_NE(initial_particle, nullptr);
    ASSERT_NE(initial_particle->Mesh(), nullptr);
    ASSERT_NE(initial_particle->Mesh()->Material(), nullptr);
    auto initial_animation =
        initial_particle->Mesh()->Material()->ShaderValueAnimation("g_Overbright"_str);
    ASSERT_TRUE(initial_animation.is_some());
    EXPECT_NE((*animation).as_ptr(), (*initial_animation).as_ptr());
    scene.scene->TickNodeFieldAnimations();
    scene.scene->PassFrameTime(0.25);
    scene.scene->TickNodeFieldAnimations();
    EXPECT_GT((*animation)->PositionSeconds(), 0.0);
    EXPECT_GT((*initial_animation)->PositionSeconds(), 0.0);
    auto layer_count = scene.scene->Root()->GetChildren().len();
    owe::script::TickSceneScripts(*scene.scene, owe::script::FrameInputs {});
    EXPECT_EQ(scene.scene->Root()->GetChildren().len(), layer_count);
}

TEST(SceneShaderReflection, InitialMaterialsRetainSharedReflectionAfterFinalize) {
    using namespace rstd::prelude;
    auto document = owe::wpscene::ParseSceneDocumentJson(R"({
        "camera": {},
        "general": {"orthogonalprojection": {"width": 1920, "height": 1080}},
        "objects": [
            {"id": 1, "name": "First", "image": "models/util/solidlayer.json", "size": "256 256"},
            {"id": 2, "name": "Second", "image": "models/util/solidlayer.json", "size": "256 256"}
        ]
    })"_str,
                                                         owe::wpscene::kSceneVersionUnknown);
    ASSERT_TRUE(document.is_some());
    auto assets = owe::fs::make_physical_fs(
        owe::fs::Path(rstd::cppstd::as_str(WAYWALLEN_ASSETS_DIR).unwrap()));
    ASSERT_TRUE(assets.is_ok());
    owe::fs::VFS vfs;
    ASSERT_TRUE(vfs.mount("/assets"_str, rstd::move(assets).unwrap()).is_ok());
    wavsen::audio::SoundManager sound;
    owe::SceneParser            parser;
    auto                        parsed =
        parser.Parse("initial-reflection"_str,
                     ref<owe::wpscene::SceneDocument>::from_raw_parts(rstd::addressof(*document)),
                     mut_ref<owe::fs::VFS>::from_raw_parts(rstd::addressof(vfs)),
                     mut_ref<wavsen::audio::SoundManager>::from_raw_parts(rstd::addressof(sound)));
    ASSERT_TRUE(parsed.is_ok());
    auto  scene  = rstd::move(parsed).unwrap();
    auto* first  = scene.scene->RootMut()->FindByName("First"_str);
    auto* second = scene.scene->RootMut()->FindByName("Second"_str);
    ASSERT_NE(first, nullptr);
    ASSERT_NE(second, nullptr);
    ASSERT_NE(first->Mesh(), nullptr);
    ASSERT_NE(second->Mesh(), nullptr);
    auto& a = first->Mesh()->Material()->customShader;
    auto& b = second->Mesh()->Material()->customShader;
    ASSERT_TRUE(a.shader.is_some());
    ASSERT_TRUE(b.shader.is_some());
    ASSERT_TRUE((*a.shader)->reflection.is_some());
    ASSERT_TRUE((*b.shader)->reflection.is_some());
    EXPECT_NE(&**a.shader, &**b.shader);
    EXPECT_EQ(&**(*a.shader)->reflection, &**(*b.shader)->reflection);
    EXPECT_TRUE((*(*a.shader)->reflection)->Matches((*a.shader)->codes.as_slice()));
    (*a.shader)->codes.clear();
    EXPECT_FALSE((*b.shader)->codes.is_empty());
    EXPECT_TRUE((*(*b.shader)->reflection)->Matches((*b.shader)->codes.as_slice()));
}

TEST(SceneDocumentObjects, PreservesDeclarationOrderAndObjectKinds) {
    auto document = owe::wpscene::ParseSceneDocumentJson(
        R"JSON({
            "camera": {},
            "general": {},
            "objects": [
                {"id": 10, "name": "Group", "parent": 0},
                {"id": 11, "name": "Shape", "shape": "quad", "parent": 10},
                {"id": 12, "name": "Text", "text": "hello", "parent": 10}
            ]
        })JSON"_str,
        owe::wpscene::kSceneVersionUnknown);
    ASSERT_TRUE(document.is_some());
    ASSERT_EQ(document->objects.len(), rstd::usize(3));
    EXPECT_EQ(document->objects[rstd::usize()].metadata.id, rstd::i32(10));
    EXPECT_EQ(document->objects[rstd::usize()].metadata.kind,
              owe::wpscene::SceneObjectKind::Container);
    EXPECT_EQ(document->objects[rstd::usize(1)].metadata.id, rstd::i32(11));
    EXPECT_EQ(document->objects[rstd::usize(1)].metadata.kind,
              owe::wpscene::SceneObjectKind::Shape);
    EXPECT_EQ(document->objects[rstd::usize(2)].metadata.id, rstd::i32(12));
    EXPECT_EQ(document->objects[rstd::usize(2)].metadata.kind, owe::wpscene::SceneObjectKind::Text);
}

TEST(SceneDocumentObjects, RejectsNonArrayObjectsAtTheCanonicalParseEntry) {
    auto document = owe::wpscene::ParseSceneDocumentJson(
        R"JSON({"camera": {}, "general": {}, "objects": {}})JSON"_str,
        owe::wpscene::kSceneVersionUnknown);
    ASSERT_TRUE(document.is_some());
    ASSERT_FALSE(document->objects_are_array);

    owe::fs::VFS                vfs;
    wavsen::audio::SoundManager sound_manager;
    owe::SceneParser            parser;
    auto                        parsed = parser.Parse(
        "invalid-objects"_str,
        rstd::ref<owe::wpscene::SceneDocument>::from_raw_parts(rstd::addressof(*document)),
        rstd::mut_ref<owe::fs::VFS>::from_raw_parts(rstd::addressof(vfs)),
        rstd::mut_ref<wavsen::audio::SoundManager>::from_raw_parts(rstd::addressof(sound_manager)));
    ASSERT_TRUE(parsed.is_err());
    EXPECT_EQ(parsed.unwrap_err().kind, owe::SceneParseErrorKind::ObjectExpansion);
}

TEST(SceneDocumentObjects, CapturesAuthoredAutoOrthoCanvas) {
    auto document = owe::wpscene::ParseSceneDocumentJson(
        R"JSON({
            "camera": {},
            "general": {"orthogonalprojection": {"auto": true}},
            "objects": [
                {"id": 1, "image": "small", "size": "640 480"},
                {"id": 2, "image": "canvas", "size": "1920 1080"}
            ]
        })JSON"_str,
        owe::wpscene::kSceneVersionUnknown);
    ASSERT_TRUE(document.is_some());
    ASSERT_TRUE(document->metadata.canvas_extent.is_some());
    EXPECT_EQ((*document->metadata.canvas_extent)[rstd::usize(0)], rstd::u32(1920));
    EXPECT_EQ((*document->metadata.canvas_extent)[rstd::usize(1)], rstd::u32(1080));
}

TEST(SceneObjectExpansion, AutoOrthoExtentUsesDecodedImageSize) {
    owe::wpscene::SceneMetadata metadata;
    metadata.general.orthogonalprojection.auto_ = true;
    metadata.canvas_extent =
        rstd::Some(rstd::array<rstd::u32, 2> { rstd::u32(640), rstd::u32(480) });

    owe::wpscene::ImageObject image;
    image.size = { 1920.0f, 1080.0f };
    rstd::vec::Vec<owe::SceneObjectVar> objects;
    objects.push(owe::SceneObjectVar::Image(rstd::move(image)));

    auto extent = owe::ResolveOrthoProjectionExtent(metadata, objects.as_slice());
    EXPECT_EQ(extent[rstd::usize()], rstd::i32(1920));
    EXPECT_EQ(extent[rstd::usize(1)], rstd::i32(1080));
}

TEST(SceneObjectExpansion, IgnoresContainerWithoutAuthoredId) {
    auto document = owe::wpscene::ParseSceneDocumentJson(
        R"JSON({
            "camera": {},
            "general": {},
            "objects": [
                {"name": "Missing Id"},
                {"id": 7, "name": "Valid Container"}
            ]
        })JSON"_str,
        owe::wpscene::kSceneVersionUnknown);
    ASSERT_TRUE(document.is_some());

    owe::fs::VFS vfs;
    auto         objects = owe::wpscene::DecodeSceneObjects(
        rstd::ref<owe::wpscene::SceneDocument>::from_raw_parts(rstd::addressof(*document)),
        rstd::mut_ref<owe::fs::VFS>::from_raw_parts(rstd::addressof(vfs)));

    ASSERT_EQ(objects.len(), rstd::usize(1));
    ASSERT_TRUE(objects[rstd::usize()].is_Container());
    EXPECT_EQ(objects[rstd::usize()].as_Container().value.id, rstd::i32(7));
}

TEST(SceneObjectExpansion, SingleObjectDecodeUsesDocumentSchema) {
    auto document = owe::wpscene::ParseSceneDocumentJson(
        R"JSON({
            "camera": {},
            "general": {},
            "objects": [
                {"id": 7, "name": "container", "visible": false},
                {"id": 8, "name": "audio", "sound": [], "visible": false},
                {"id": 9, "name": "image", "image": "models/util/solidlayer.json",
                 "size": "32 32", "visible": false}
            ]
        })JSON"_str,
        owe::wpscene::kSceneVersionUnknown);
    ASSERT_TRUE(document.is_some());
    owe::fs::VFS vfs;
    auto         assets = owe::fs::make_physical_fs(
        owe::fs::Path(rstd::cppstd::as_str(WAYWALLEN_ASSETS_DIR).unwrap()));
    ASSERT_TRUE(assets.is_ok());
    ASSERT_TRUE(vfs.mount("/assets"_str, rstd::move(assets).unwrap_unchecked()).is_ok());
    auto vfs_ref = rstd::mut_ref<owe::fs::VFS>::from_raw_parts(rstd::addressof(vfs));
    auto objects = owe::wpscene::DecodeSceneObjects(
        rstd::ref<owe::wpscene::SceneDocument>::from_raw_parts(rstd::addressof(*document)),
        vfs_ref);
    ASSERT_EQ(objects.len(), rstd::usize(3));
    for (rstd::usize i {}; i < objects.len(); ++i) {
        const auto& record = document->objects[i];
        EXPECT_EQ(owe::wpscene::ClassifySceneObject(record.authored), record.metadata.kind);
        auto decoded = owe::wpscene::DecodeSceneObject(
            record.authored, vfs_ref, document->metadata.pkg_version);
        ASSERT_TRUE(decoded.is_some());
        if (objects[i].is_Container()) {
            ASSERT_TRUE(decoded->is_Container());
            EXPECT_EQ(decoded->as_Container().value.id, objects[i].as_Container().value.id);
            EXPECT_FALSE(decoded->as_Container().value.visible);
        } else if (objects[i].is_Sound()) {
            ASSERT_TRUE(decoded->is_Sound());
            EXPECT_EQ(decoded->as_Sound().value.id, objects[i].as_Sound().value.id);
            EXPECT_EQ(decoded->as_Sound().value.visible, objects[i].as_Sound().value.visible);
        } else {
            ASSERT_TRUE(decoded->is_Image());
            EXPECT_EQ(decoded->as_Image().value.id, objects[i].as_Image().value.id);
            EXPECT_FALSE(decoded->as_Image().value.visible);
        }
    }
}

TEST(SceneObjectExpansion, PreservesHiddenSourceReferencedByContainer) {
    auto parsed = owe::ParseJson(R"JSON({
        "objects": [
            {"id": 7, "name": "Hidden Source", "sound": [], "visible": false},
            {"id": 8, "name": "Container", "dependencies": [7]}
        ]
    })JSON"_str);
    ASSERT_TRUE(parsed.is_ok());

    owe::fs::VFS vfs;
    auto objects = owe::ExpandObjects(parsed.unwrap(), vfs, owe::wpscene::kSceneVersionUnknown);

    ASSERT_EQ(objects.len(), rstd::usize(2));
    ASSERT_TRUE(objects[rstd::usize()].is_Sound());
    EXPECT_TRUE(objects[rstd::usize()].as_Sound().value.visible);
    EXPECT_TRUE(objects[rstd::usize(1)].is_Container());
}

TEST(SceneObjectExpansion, ShapeOwnsItsWallpaperLayerIdentity) {
    auto document = owe::wpscene::ParseSceneDocumentJson(
        R"JSON({
            "camera": {},
            "general": {"orthogonalprojection": {"width": 1920, "height": 1080}},
            "objects": [{
                "id": 42,
                "name": "Direct Draw Shape",
                "shape": "quad",
                "origin": [0.0, 0.0, 0.0],
                "angles": [0.0, 0.0, 0.0],
                "scale": [1.0, 1.0, 1.0],
                "effects": [{
                    "file": "effects/lightshafts/effect.json",
                    "visible": true,
                    "passes": [{
                        "combos": {"DIRECTDRAW": 1, "RAYMODE": 1},
                        "constantshadervalues": {
                            "point0": "-1.0 -1.0",
                            "point1": "-1.0 1.0",
                            "point2": "1.0 1.0",
                            "point3": "1.0 -1.0"
                        }
                    }]
                }]
            }]
        })JSON"_str,
        owe::wpscene::kSceneVersionUnknown);
    ASSERT_TRUE(document.is_some());

    auto assets = owe::fs::make_physical_fs(
        owe::fs::Path(rstd::cppstd::as_str(WAYWALLEN_ASSETS_DIR).unwrap()));
    ASSERT_TRUE(assets.is_ok());
    auto effect_assets = owe::fs::make_physical_fs(owe::fs::Path(
        rstd::cppstd::as_str(std::string(WAYWALLEN_ASSETS_DIR) + "/effects/lightshafts").unwrap()));
    ASSERT_TRUE(effect_assets.is_ok());
    owe::fs::VFS vfs;
    ASSERT_TRUE(vfs.mount("/assets"_str, std::move(assets).unwrap_unchecked()).is_ok());
    ASSERT_TRUE(vfs.mount("/assets"_str, std::move(effect_assets).unwrap_unchecked()).is_ok());

    wavsen::audio::SoundManager sound_manager;
    owe::SceneParser            parser;
    auto                        parsed = parser.Parse(
        "shape-layer-identity"_str,
        rstd::ref<owe::wpscene::SceneDocument>::from_raw_parts(rstd::addressof(*document)),
        rstd::mut_ref<owe::fs::VFS>::from_raw_parts(rstd::addressof(vfs)),
        rstd::mut_ref<wavsen::audio::SoundManager>::from_raw_parts(rstd::addressof(sound_manager)));
    ASSERT_TRUE(parsed.is_ok());

    auto scene = rstd::move(parsed).unwrap();
    auto shape = scene.scene->RootMut()->FindByName("Direct Draw Shape"_str);
    ASSERT_NE(shape, nullptr);
    ASSERT_TRUE(shape->WallpaperIdentity().is_some());
    EXPECT_EQ(shape->WallpaperIdentity()->value, rstd::i32(42));
}

TEST(PuppetScriptParsing, VisibilityScriptsInitializeTheirOwnPlayback) {
    const auto pkg_path =
        std::filesystem::path(WAYWALLEN_WORKSHOP_DIR) / "3462491575" / "scene.pkg";
    if (! std::filesystem::exists(pkg_path)) GTEST_SKIP() << "workshop 3462491575 is not available";
    auto document = owe::wpscene::ParseSceneDocumentJson(
        R"JSON({
            "camera": {},
            "general": {"orthogonalprojection": {"width": 1920, "height": 1080}},
            "objects": [
                {"id": 1, "name": "First", "image": "models/身体.json",
                 "animationlayers": [{"id": 101, "animation": 3835, "name": "Offset",
                    "visible": {"value": true, "scriptproperties": {"offset": 0.8},
                    "script": "export var scriptProperties = createScriptProperties().addSlider({name:'offset', value:0}).finish(); export function init(value) { if (thisObject === thisLayer) throw new Error('wrong owner'); const animation = thisObject.getAnimation(); animation.play(); animation.setFrame(animation.frameCount * scriptProperties.offset); return value; }"}}]},
                {"id": 2, "name": "Second", "image": "models/身体.json",
                 "animationlayers": [{"id": 201, "animation": 3835, "name": "Offset",
                    "visible": {"value": false, "scriptproperties": {"offset": 0.36},
                    "script": "export var scriptProperties = createScriptProperties().addSlider({name:'offset', value:0}).finish(); export function init(value) { const animation = thisObject.getAnimation(); animation.play(); animation.setFrame(animation.frameCount * scriptProperties.offset); return true; }"}}]}
            ]
        })JSON"_str,
        owe::wpscene::kSceneVersionUnknown);
    ASSERT_TRUE(document.is_some());
    auto assets = owe::fs::make_physical_fs(
        owe::fs::Path(rstd::cppstd::as_str(WAYWALLEN_ASSETS_DIR).unwrap()));
    ASSERT_TRUE(assets.is_ok());
    auto pkg =
        owe::fs::WPPkgFs::open(owe::fs::Path(rstd::cppstd::as_str(pkg_path.string()).unwrap()));
    ASSERT_TRUE(pkg.is_ok());
    owe::fs::VFS vfs;
    ASSERT_TRUE(vfs.mount("/assets"_str, std::move(assets).unwrap_unchecked()).is_ok());
    ASSERT_TRUE(vfs.mount("/assets"_str, pkg->mount_handle()).is_ok());
    wavsen::audio::SoundManager sound;
    owe::SceneParser            parser;
    auto                        parsed = parser.Parse(
        "puppet-script"_str,
        rstd::ref<owe::wpscene::SceneDocument>::from_raw_parts(rstd::addressof(*document)),
        rstd::mut_ref<owe::fs::VFS>::from_raw_parts(rstd::addressof(vfs)),
        rstd::mut_ref<wavsen::audio::SoundManager>::from_raw_parts(rstd::addressof(sound)));
    ASSERT_TRUE(parsed.is_ok());
    auto scene  = rstd::move(parsed).unwrap();
    auto first  = scene.scene->RootMut()->FindByName("First"_str);
    auto second = scene.scene->RootMut()->FindByName("Second"_str);
    ASSERT_NE(first, nullptr);
    ASSERT_NE(second, nullptr);
    auto first_playback  = first->NamedAnimation("Offset"_str);
    auto second_playback = second->NamedAnimation("Offset"_str);
    ASSERT_TRUE(first_playback.is_some());
    ASSERT_TRUE(second_playback.is_some());
    EXPECT_EQ((*first_playback)->Frame(), rstd::i32(96));
    EXPECT_EQ((*second_playback)->Frame(), rstd::i32(43));
    EXPECT_NEAR((*second_playback)->Sample().current, 43.2f, 0.0001f);
    EXPECT_NE((*first_playback).as_ptr(), (*second_playback).as_ptr());
}

TEST(ImageAlignmentParsing, DisabledEffectsPreserveSourceDrawAnchor) {
    using namespace rstd::prelude;
    auto assets = owe::fs::make_physical_fs(
        owe::fs::Path(rstd::cppstd::as_str(WAYWALLEN_ASSETS_DIR).unwrap()));
    ASSERT_TRUE(assets.is_ok());
    auto effects = owe::fs::make_physical_fs(owe::fs::Path(
        rstd::format("{}/effects/tint", rstd::cppstd::as_str(WAYWALLEN_ASSETS_DIR).unwrap())
            .as_str()));
    ASSERT_TRUE(effects.is_ok());
    owe::fs::VFS vfs;
    ASSERT_TRUE(vfs.mount("/assets"_str, rstd::move(assets).unwrap()).is_ok());
    ASSERT_TRUE(vfs.mount("/assets"_str, rstd::move(effects).unwrap()).is_ok());
    auto document = owe::wpscene::ParseSceneDocumentJson(
        R"JSON({
            "camera": {},
            "general": {"orthogonalprojection": {"width": 8192, "height": 2576}},
            "objects": [{
                "id": 49, "name": "Clock housing", "image": "models/util/solidlayer.json",
                "origin": "5070 2127 0", "size": "296 118", "scale": "3.5 3.5 3.5",
                "alignment": "bottom",
                "effects": [{"file": "effects/tint/effect.json", "visible": false}]
            }]
        })JSON"_str,
        owe::wpscene::kSceneVersionUnknown);
    ASSERT_TRUE(document.is_some());
    wavsen::audio::SoundManager sound;
    owe::SceneParser            parser;
    auto                        parsed =
        parser.Parse("image-alignment"_str,
                     ref<owe::wpscene::SceneDocument>::from_raw_parts(rstd::addressof(*document)),
                     mut_ref<owe::fs::VFS>::from_raw_parts(rstd::addressof(vfs)),
                     mut_ref<wavsen::audio::SoundManager>::from_raw_parts(rstd::addressof(sound)));
    ASSERT_TRUE(parsed.is_ok());
    auto  scene = rstd::move(parsed).unwrap();
    auto* node  = scene.scene->RootMut()->FindByName("Clock housing"_str);
    ASSERT_NE(node, nullptr);
    ASSERT_TRUE(node->HasLayer());
    auto& layer = node->Layer();
    ASSERT_EQ(layer->EffectCount(), usize(1));
    auto& effect = layer->GetEffect(usize());
    node->UpdateTrans();
    auto bottom = [&](const owe::SceneMesh& mesh) {
        const auto& vertices = mesh.GetVertexArray(usize());
        const auto  offset =
            vertices.AttributeOffset(owe::VAttr::Position.name).unwrap() / usize(sizeof(float));
        const auto*           data = vertices.Data() + (offset + vertices.OneSize()).to_primitive();
        const Eigen::Vector4d point { data[0], data[1], data[2], 1.0 };
        return (node->ModelTrans() * node->GeometryTransform() * mesh.GeometryTransform() * point)
            .y();
    };
    for (bool enabled : { false, true, false, true, false }) {
        layer->SetEffectRuntimeVisible(*effect, enabled);
        const bool intermediate = layer->RequiresIntermediateTarget();
        EXPECT_EQ(intermediate, enabled);
        layer->ConfigureSourceDraw(intermediate);
        EXPECT_NEAR(bottom(*node->Mesh()), enabled ? 1920.5 : 2127.0, 1e-5);
        if (enabled) {
            layer->ResolveEffect(*scene.scene->DefaultEffectMesh(), "effect"_str);
            EXPECT_NEAR(bottom(layer->FinalMesh()), 2127.0, 1e-5);
        }
        EXPECT_FLOAT_EQ(node->Translate().y(), 2127.0f);
    }
}

TEST(PuppetUvParsing, ImageAndEffectMasksUseTheirBoundTextureSpace) {
    using namespace rstd::prelude;
    auto path = rstd::format("{}/3493636545/scene.pkg",
                             rstd::cppstd::as_str(WAYWALLEN_WORKSHOP_DIR).unwrap());
    auto pkg  = owe::fs::WPPkgFs::open(owe::fs::Path(path.as_str()));
    if (pkg.is_err()) GTEST_SKIP() << "workshop 3493636545 is not available";
    auto assets = owe::fs::make_physical_fs(
        owe::fs::Path(rstd::cppstd::as_str(WAYWALLEN_ASSETS_DIR).unwrap()));
    ASSERT_TRUE(assets.is_ok());
    owe::fs::VFS vfs;
    ASSERT_TRUE(vfs.mount("/assets"_str, rstd::move(assets).unwrap()).is_ok());
    ASSERT_TRUE(vfs.mount("/assets"_str, pkg->mount_handle()).is_ok());
    auto padded_path = rstd::format("{}/3757177441/scene.pkg",
                                    rstd::cppstd::as_str(WAYWALLEN_WORKSHOP_DIR).unwrap());
    auto padded_pkg  = owe::fs::WPPkgFs::open(owe::fs::Path(padded_path.as_str()));
    if (padded_pkg.is_err()) GTEST_SKIP() << "workshop 3757177441 is not available";
    ASSERT_TRUE(vfs.mount("/assets"_str, padded_pkg->mount_handle()).is_ok());
    auto effect_assets = owe::fs::make_physical_fs(owe::fs::Path(
        rstd::format("{}/effects/scroll", rstd::cppstd::as_str(WAYWALLEN_ASSETS_DIR).unwrap())
            .as_str()));
    ASSERT_TRUE(effect_assets.is_ok());
    ASSERT_TRUE(vfs.mount("/assets"_str, rstd::move(effect_assets).unwrap()).is_ok());
    auto temporary = rstd::fs::TempDir::make("owe-puppet-uv"_str).unwrap();
    auto root      = rstd::path::PathBuf::from(temporary.path().as_os_str().to_os_string());
    ASSERT_TRUE(
        rstd::fs::write(
            root.join("direct.json"_str).as_path(),
            R"({"autosize":true,"material":"materials/睫毛.json","puppet":"models/图层 1_puppet.mdl"})"_bytes)
            .is_ok());
    ASSERT_TRUE(
        rstd::fs::write(
            root.join("unpadded.json"_str).as_path(),
            R"({"autosize":true,"nopadding":true,"material":"materials/睫毛.json","puppet":"models/图层 1_puppet.mdl"})"_bytes)
            .is_ok());
    auto mount = owe::fs::make_physical_fs(temporary.path());
    ASSERT_TRUE(mount.is_ok());
    ASSERT_TRUE(vfs.mount("/assets"_str, rstd::move(mount).unwrap()).is_ok());
    owe::Mdl mdl;
    ASSERT_TRUE(owe::MdlParser::Parse("models/图层 1_puppet.mdl"_str, vfs, mdl));
    ASSERT_FALSE(mdl.meshes.is_empty());
    const auto& source = mdl.meshes[usize()];
    ASSERT_FALSE(source.masks.is_empty());

    auto document = owe::wpscene::ParseSceneDocumentJson(
        R"JSON({
            "camera": {},
            "general": {"orthogonalprojection": {"width": 1920, "height": 1080}},
            "objects": [
                {"id": 1, "name": "Direct", "image": "direct.json"},
                {"id": 2, "name": "Unpadded", "image": "unpadded.json"},
                {"id": 3, "name": "Effect", "image": "direct.json",
                 "effects": [{"file": "effects/scroll/effect.json", "visible": true}]}
            ]
        })JSON"_str,
        owe::wpscene::kSceneVersionUnknown);
    ASSERT_TRUE(document.is_some());
    wavsen::audio::SoundManager sound;
    owe::SceneParser            parser;
    auto                        parsed =
        parser.Parse("puppet-uv"_str,
                     ref<owe::wpscene::SceneDocument>::from_raw_parts(rstd::addressof(*document)),
                     mut_ref<owe::fs::VFS>::from_raw_parts(rstd::addressof(vfs)),
                     mut_ref<wavsen::audio::SoundManager>::from_raw_parts(rstd::addressof(sound)));
    ASSERT_TRUE(parsed.is_ok());
    auto scene = rstd::move(parsed).unwrap();
    for (ref<str> name : { "Direct"_str, "Unpadded"_str, "Effect"_str }) {
        auto* node = scene.scene->RootMut()->FindByName(name);
        ASSERT_NE(node, nullptr);
        owe::SceneMesh* mesh = node->Mesh();
        if (name == "Effect"_str) {
            ASSERT_TRUE(node->HasLayer());
            auto& layer = node->Layer();
            layer->ResolveEffect(*scene.scene->DefaultEffectMesh(), "effect"_str);
            ASSERT_FALSE(layer->ResolvedEffects().is_empty());
            auto* effect = layer->ResolvedEffects().last().unwrap().get();
            ASSERT_FALSE(effect->Nodes().is_empty());
            mesh = effect->Nodes().last().unwrap().get()->sceneNode->Mesh();
        }
        ASSERT_NE(mesh, nullptr);
        ASSERT_EQ(mesh->Submeshes().len(), usize(1) + source.masks.len() * usize(2));
        array<float, 2> scale { 1.0f, 1.0f };
        if (name == "Direct"_str) {
            auto resolution =
                mesh->Material()->customShader.constValues.get("g_Texture0Resolution"_str);
            ASSERT_TRUE(resolution.is_some());
            const auto& value = **resolution;
            scale = { value[usize(2)] / value[usize()], value[usize(3)] / value[usize(1)] };
            EXPECT_TRUE(scale[usize()] != 1.0f || scale[usize(1)] != 1.0f);
        }
        for (const auto& submesh : mesh->Submeshes()) {
            ASSERT_EQ(submesh.vertex_arrays.len(), usize(1));
            const auto& vertices = submesh.vertex_arrays[usize()];
            const auto  uv =
                vertices.AttributeOffset(owe::VAttr::TexCoord.name).unwrap() / usize(sizeof(float));
            EXPECT_FLOAT_EQ(vertices.Data()[uv.to_primitive()],
                            source.texcoords[usize()][usize()] * scale[usize()]);
            EXPECT_FLOAT_EQ(vertices.Data()[uv.to_primitive() + 1],
                            source.texcoords[usize()][usize(1)] * scale[usize(1)]);
        }
    }
}

TEST(TextColorBlendParsing, PreservesOverlayAdditiveAndDirectModes) {
    auto document = owe::wpscene::ParseSceneDocumentJson(
        R"JSON({
            "camera": {},
            "general": {"orthogonalprojection": {"width": 1920, "height": 1080}},
            "objects": [
                {"id": 11, "name": "Overlay", "text": "12:34",
                 "font": "systemfont_DejaVu Sans", "pointsize": 96, "colorBlendMode": 11},
                {"id": 31, "name": "Additive", "text": "12:34",
                 "font": "systemfont_DejaVu Sans", "pointsize": 96, "colorBlendMode": 31},
                {"id": 1, "name": "Direct", "text": "12:34",
                 "font": "systemfont_DejaVu Sans", "pointsize": 96}
            ]
        })JSON"_str,
        owe::wpscene::kSceneVersionUnknown);
    ASSERT_TRUE(document.is_some());
    auto assets = owe::fs::make_physical_fs(
        owe::fs::Path(rstd::cppstd::as_str(WAYWALLEN_ASSETS_DIR).unwrap()));
    ASSERT_TRUE(assets.is_ok());
    owe::fs::VFS vfs;
    ASSERT_TRUE(vfs.mount("/assets"_str, std::move(assets).unwrap_unchecked()).is_ok());
    wavsen::audio::SoundManager sound_manager;
    owe::SceneParser            parser;
    auto                        parsed = parser.Parse(
        "text-color-blend"_str,
        rstd::ref<owe::wpscene::SceneDocument>::from_raw_parts(rstd::addressof(*document)),
        rstd::mut_ref<owe::fs::VFS>::from_raw_parts(rstd::addressof(vfs)),
        rstd::mut_ref<wavsen::audio::SoundManager>::from_raw_parts(rstd::addressof(sound_manager)));
    ASSERT_TRUE(parsed.is_ok());
    auto scene   = rstd::move(parsed).unwrap();
    auto overlay = scene.scene->RootMut()->FindByName("Overlay"_str);
    ASSERT_NE(overlay, nullptr);
    ASSERT_NE(overlay->Mesh(), nullptr);
    auto* material = overlay->Mesh()->Material();
    ASSERT_NE(material, nullptr);
    ASSERT_TRUE(material->customShader.variant.is_some());
    EXPECT_EQ(
        *material->customShader.variant->Description().input_combos.get("BLENDMODE"_str).unwrap(),
        "11"_str);
    EXPECT_EQ(material->Pipeline().blend_mode, owe::BlendMode::Translucent);
    auto additive = scene.scene->RootMut()->FindByName("Additive"_str);
    ASSERT_NE(additive, nullptr);
    ASSERT_NE(additive->Mesh(), nullptr);
    ASSERT_NE(additive->Mesh()->Material(), nullptr);
    EXPECT_EQ(additive->Mesh()->Material()->Pipeline().blend_mode, owe::BlendMode::Additive);
    auto direct = scene.scene->RootMut()->FindByName("Direct"_str);
    ASSERT_NE(direct, nullptr);
    ASSERT_NE(direct->Mesh(), nullptr);
    ASSERT_NE(direct->Mesh()->Material(), nullptr);
    EXPECT_EQ(direct->Mesh()->Material()->name, "text"_str);
}

TEST(ImageColorBlendParsing, LinearDodgeUsesAdditiveAttachmentOwner) {
    auto document = owe::wpscene::ParseSceneDocumentJson(
        R"JSON({
            "camera": {},
            "general": {"orthogonalprojection": {"width": 1920, "height": 1080}},
            "objects": [{
                "id": 31,
                "name": "Linear Dodge",
                "image": "models/util/fullscreenlayer.json",
                "colorBlendMode": 31,
                "visible": true
            }]
        })JSON"_str,
        owe::wpscene::kSceneVersionUnknown);
    ASSERT_TRUE(document.is_some());

    auto assets = owe::fs::make_physical_fs(
        owe::fs::Path(rstd::cppstd::as_str(WAYWALLEN_ASSETS_DIR).unwrap()));
    ASSERT_TRUE(assets.is_ok());
    owe::fs::VFS vfs;
    ASSERT_TRUE(vfs.mount("/assets"_str, std::move(assets).unwrap_unchecked()).is_ok());

    wavsen::audio::SoundManager sound_manager;
    owe::SceneParser            parser;
    auto                        parsed = parser.Parse(
        "linear-dodge-owner"_str,
        rstd::ref<owe::wpscene::SceneDocument>::from_raw_parts(rstd::addressof(*document)),
        rstd::mut_ref<owe::fs::VFS>::from_raw_parts(rstd::addressof(vfs)),
        rstd::mut_ref<wavsen::audio::SoundManager>::from_raw_parts(rstd::addressof(sound_manager)));
    ASSERT_TRUE(parsed.is_ok());

    auto scene = rstd::move(parsed).unwrap();
    auto node  = scene.scene->RootMut()->FindByName("Linear Dodge"_str);
    ASSERT_NE(node, nullptr);
    ASSERT_NE(node->Mesh(), nullptr);
    ASSERT_NE(node->Mesh()->Material(), nullptr);
    EXPECT_EQ(node->Mesh()->Material()->Pipeline().blend_mode, owe::BlendMode::Additive);
    ASSERT_TRUE(node->Mesh()->Material()->customShader.variant.is_some());
    EXPECT_EQ(*node->Mesh()
                   ->Material()
                   ->customShader.variant->Description()
                   .input_combos.get("SCENE_ORTHO"_str)
                   .unwrap(),
              "1"_str);
    EXPECT_EQ(*node->Mesh()
                   ->Material()
                   ->customShader.variant->Description()
                   .input_combos.get("OWE_IMAGE_LAYER"_str)
                   .unwrap(),
              "1"_str);
}

TEST(ImageColorBlendParsing, EffectLayerPreservesLinearDodgeAttachmentOwner) {
    auto document = owe::wpscene::ParseSceneDocumentJson(
        R"JSON({
            "camera": {},
            "general": {"orthogonalprojection": {"width": 1920, "height": 1080}},
            "objects": [{
                "id": 31,
                "name": "Linear Dodge Effect",
                "image": "models/util/solidlayer.json",
                "colorBlendMode": 31,
                "effects": [{
                    "file": "effects/scroll/effect.json",
                    "visible": true
                }],
                "visible": true
            }]
        })JSON"_str,
        owe::wpscene::kSceneVersionUnknown);
    ASSERT_TRUE(document.is_some());

    auto assets = owe::fs::make_physical_fs(
        owe::fs::Path(rstd::cppstd::as_str(WAYWALLEN_ASSETS_DIR).unwrap()));
    ASSERT_TRUE(assets.is_ok());
    owe::fs::VFS vfs;
    ASSERT_TRUE(vfs.mount("/assets"_str, std::move(assets).unwrap_unchecked()).is_ok());

    wavsen::audio::SoundManager sound_manager;
    owe::SceneParser            parser;
    auto                        parsed = parser.Parse(
        "linear-dodge-effect-owner"_str,
        rstd::ref<owe::wpscene::SceneDocument>::from_raw_parts(rstd::addressof(*document)),
        rstd::mut_ref<owe::fs::VFS>::from_raw_parts(rstd::addressof(vfs)),
        rstd::mut_ref<wavsen::audio::SoundManager>::from_raw_parts(rstd::addressof(sound_manager)));
    ASSERT_TRUE(parsed.is_ok());

    auto scene = rstd::move(parsed).unwrap();
    auto node  = scene.scene->RootMut()->FindByName("Linear Dodge Effect"_str);
    ASSERT_NE(node, nullptr);
    ASSERT_TRUE(node->HasLayer());

    auto& layer = node->Layer();
    layer->ResolveEffect(*scene.scene->DefaultEffectMesh(), "effect"_str);
    ASSERT_FALSE(layer->ResolvedEffects().is_empty());
    auto* final_effect = layer->ResolvedEffects().last().unwrap().get();
    ASSERT_NE(final_effect, nullptr);
    ASSERT_FALSE(final_effect->Nodes().is_empty());
    auto* final_material =
        final_effect->Nodes().last().unwrap().get()->sceneNode->Mesh()->Material();
    ASSERT_NE(final_material, nullptr);
    EXPECT_EQ(final_material->Pipeline().blend_mode, owe::BlendMode::Additive);
    auto target_key = scene.scene->NodeResourceKey(node->Identity(), "layer_composite"_str);
    ASSERT_TRUE(scene.scene->RenderTarget(target_key.as_str()).is_some());
    rstd::array<owe::SceneNodeId, 1> owners { node->Identity() };
    EXPECT_TRUE(scene.scene->RemoveNodeRenderTargets(owners.as_slice()) > rstd::usize());
    EXPECT_TRUE(scene.scene->RenderTarget(target_key.as_str()).is_none());
    EXPECT_TRUE(scene.scene->RenderTarget("_rt_default"_str).is_some());
    auto camera_key = scene.scene->NodeResourceKey(node->Identity(), "layer_camera"_str);
    auto camera     = scene.scene->CameraHandle(camera_key.as_str());
    auto active     = scene.scene->ActiveCameraHandle();
    ASSERT_TRUE(camera.is_some());
    ASSERT_TRUE(active.is_some());
    owe::UniformCameraResolver resolver((*active).clone(), scene.scene->CameraRegistry());
    EXPECT_TRUE(resolver.Resolve(*node).is_some());
    EXPECT_EQ(scene.scene->RemoveNodeCameras(owners.as_slice()).len(), rstd::usize(1));
    EXPECT_TRUE(scene.scene->Camera(camera_key.as_str()).is_none());
    EXPECT_TRUE(resolver.Resolve(*node).is_none());
    EXPECT_TRUE((*camera)->CameraSnapshot().projection.allFinite());
}

TEST(ModelMaterialParsing, OpaqueSubmeshesPrecedeBlendedSubmeshes) {
    const auto path =
        rstd::path::PathBuf::from(rstd::cppstd::as_str(WAYWALLEN_WORKSHOP_DIR).unwrap())
            .join("3047596375/scene.pkg"_str);
    if (! rstd::fs::exists(path.as_path()).unwrap())
        GTEST_SKIP() << "workshop 3047596375 is not available";
    auto pkg = owe::fs::WPPkgFs::open(path.as_path());
    ASSERT_TRUE(pkg.is_ok());
    auto assets = owe::fs::make_physical_fs(
        owe::fs::Path(rstd::cppstd::as_str(WAYWALLEN_ASSETS_DIR).unwrap()));
    ASSERT_TRUE(assets.is_ok());
    owe::fs::VFS vfs;
    ASSERT_TRUE(vfs.mount("/assets"_str, rstd::move(assets).unwrap()).is_ok());
    ASSERT_TRUE(vfs.mount("/assets"_str, pkg->mount_handle()).is_ok());
    auto document = owe::wpscene::ParseSceneDocumentJson(R"({
        "camera": {}, "general": {"orthogonalprojection": {"width":128,"height":128}},
        "objects": [{"id":1,"name":"Model","model":"models/space boi/space boi.mdl"}]
    })"_str,
                                                         owe::wpscene::kSceneVersionUnknown);
    ASSERT_TRUE(document.is_some());
    wavsen::audio::SoundManager sound;
    owe::SceneParser            parser;
    auto                        parsed = parser.Parse(
        "model-material-order"_str,
        rstd::ref<owe::wpscene::SceneDocument>::from_raw_parts(rstd::addressof(*document)),
        rstd::mut_ref<owe::fs::VFS>::from_raw_parts(rstd::addressof(vfs)),
        rstd::mut_ref<wavsen::audio::SoundManager>::from_raw_parts(rstd::addressof(sound)));
    ASSERT_TRUE(parsed.is_ok());
    auto  scene = rstd::move(parsed).unwrap();
    auto* node  = scene.scene->RootMut()->FindByName("Model"_str);
    ASSERT_NE(node, nullptr);
    auto* mesh = node->Mesh();
    ASSERT_NE(mesh, nullptr);
    ASSERT_EQ(mesh->Submeshes().len(), rstd::usize(2));
    EXPECT_EQ(mesh->Submeshes()[rstd::usize()].material_slot, rstd::u32(1));
    EXPECT_EQ(mesh->Submeshes()[rstd::usize(1)].material_slot, rstd::u32());
    EXPECT_EQ(mesh->MaterialSlots()[rstd::usize(1)]->Pipeline().blend_mode, owe::BlendMode::Normal);
    EXPECT_EQ(mesh->MaterialSlots()[rstd::usize()]->Pipeline().blend_mode,
              owe::BlendMode::Translucent);
}

TEST(SceneLinkedSources, PublishedImagesRespectClampUvs) {
    auto document = owe::wpscene::ParseSceneDocumentJson(
        R"JSON({
            "camera": {},
            "general": {"orthogonalprojection": {"width": 1920, "height": 1080}},
            "objects": [
                {"id": 1, "name": "Repeated", "image": "models/util/fullscreenlayer.json",
                 "visible": false, "clampuvs": false},
                {"id": 2, "name": "Clamped", "image": "models/util/fullscreenlayer.json",
                 "visible": false, "clampuvs": true},
                {"id": 3, "name": "Consumer", "image": "models/util/fullscreenlayer.json",
                 "dependencies": [1, 2]}
            ]
        })JSON"_str,
        owe::wpscene::kSceneVersionUnknown);
    ASSERT_TRUE(document.is_some());
    auto assets = owe::fs::make_physical_fs(
        owe::fs::Path(rstd::cppstd::as_str(WAYWALLEN_ASSETS_DIR).unwrap()));
    ASSERT_TRUE(assets.is_ok());
    owe::fs::VFS vfs;
    ASSERT_TRUE(vfs.mount("/assets"_str, rstd::move(assets).unwrap()).is_ok());
    wavsen::audio::SoundManager sound;
    owe::SceneParser            parser;
    auto                        parsed = parser.Parse(
        "linked-image-sampling"_str,
        rstd::ref<owe::wpscene::SceneDocument>::from_raw_parts(rstd::addressof(*document)),
        rstd::mut_ref<owe::fs::VFS>::from_raw_parts(rstd::addressof(vfs)),
        rstd::mut_ref<wavsen::audio::SoundManager>::from_raw_parts(rstd::addressof(sound)));
    ASSERT_TRUE(parsed.is_ok());
    auto scene    = rstd::move(parsed).unwrap();
    auto repeated = scene.scene->RenderTarget("_rt_link_1"_str);
    auto clamped  = scene.scene->RenderTarget("_rt_link_2"_str);
    ASSERT_TRUE(repeated.is_some());
    ASSERT_TRUE(clamped.is_some());
    EXPECT_EQ((**repeated).sample.wrapS, owe::TextureWrap::REPEAT);
    EXPECT_EQ((**repeated).sample.wrapT, owe::TextureWrap::REPEAT);
    EXPECT_EQ((**clamped).sample.wrapS, owe::TextureWrap::CLAMP_TO_EDGE);
    EXPECT_EQ((**clamped).sample.wrapT, owe::TextureWrap::CLAMP_TO_EDGE);
    auto screen = scene.scene->RenderTarget(owe::SpecTex_Default);
    ASSERT_TRUE(screen.is_some());
    EXPECT_EQ((**screen).sample.wrapS, owe::TextureWrap::CLAMP_TO_EDGE);
    EXPECT_EQ((**screen).sample.wrapT, owe::TextureWrap::CLAMP_TO_EDGE);
}

TEST(SceneLinkedSources, EffectSelfCompositeStaysInOwningLayer) {
    auto document = owe::wpscene::ParseSceneDocumentJson(
        R"JSON({
            "camera": {},
            "general": {"orthogonalprojection": {"width": 1920, "height": 1080}},
            "objects": [{
                "id": 568,
                "name": "Self Composite",
                "image": "models/util/fullscreenlayer.json",
                "copybackground": true,
                "dependencies": [568],
                "effects": [{
                    "file": "effects/godrays/effect.json",
                    "visible": true,
                    "passes": [{}, {}, {}, {}, {
                        "textures": [null, "_rt_imageLayerComposite_568_a"]
                    }]
                }],
                "visible": true
            }]
        })JSON"_str,
        owe::wpscene::kSceneVersionUnknown);
    ASSERT_TRUE(document.is_some());

    auto assets = owe::fs::make_physical_fs(
        owe::fs::Path(rstd::cppstd::as_str(WAYWALLEN_ASSETS_DIR).unwrap()));
    ASSERT_TRUE(assets.is_ok());
    auto effect_assets = owe::fs::make_physical_fs(owe::fs::Path(
        rstd::cppstd::as_str(std::string(WAYWALLEN_ASSETS_DIR) + "/effects/godrays").unwrap()));
    ASSERT_TRUE(effect_assets.is_ok());
    owe::fs::VFS vfs;
    ASSERT_TRUE(vfs.mount("/assets"_str, std::move(assets).unwrap_unchecked()).is_ok());
    ASSERT_TRUE(vfs.mount("/assets"_str, std::move(effect_assets).unwrap_unchecked()).is_ok());

    wavsen::audio::SoundManager sound_manager;
    owe::SceneParser            parser;
    auto                        parsed = parser.Parse(
        "self-composite"_str,
        rstd::ref<owe::wpscene::SceneDocument>::from_raw_parts(rstd::addressof(*document)),
        rstd::mut_ref<owe::fs::VFS>::from_raw_parts(rstd::addressof(vfs)),
        rstd::mut_ref<wavsen::audio::SoundManager>::from_raw_parts(rstd::addressof(sound_manager)));
    ASSERT_TRUE(parsed.is_ok());

    auto scene = rstd::move(parsed).unwrap();
    auto node  = scene.scene->RootMut()->FindByName("Self Composite"_str);
    ASSERT_NE(node, nullptr);
    ASSERT_TRUE(node->HasLayer());
    EXPECT_EQ(
        scene.scene->RegisteredLayerLinkSource(owe::WallpaperLayerId { .value = rstd::i32(568) }),
        nullptr);
    EXPECT_TRUE(scene.scene->RenderTarget("_rt_link_568"_str).is_none());
}

TEST(SceneLightParsing, RecognizesPrefixedKindsAndFullConeAngles) {
    auto document = owe::wpscene::ParseSceneDocumentJson(
        R"JSON({
            "camera": {},
            "general": {"orthogonalprojection": {"width": 1920, "height": 1080}},
            "objects": [{
                "id": 1,
                "name": "Spot",
                "light": "lspot",
                "origin": [0.0, 0.0, 0.0],
                "angles": [0.0, 0.0, 0.0],
                "scale": [1.0, 1.0, 1.0],
                "innercone": 72.29,
                "outercone": 77.94
            }, {
                "id": 2,
                "name": "Directional",
                "light": "ldirectional",
                "origin": [0.0, 0.0, 0.0],
                "angles": [0.0, 0.0, 0.0],
                "scale": [1.0, 1.0, 1.0]
            }]
        })JSON"_str,
        owe::wpscene::kSceneVersionUnknown);
    ASSERT_TRUE(document.is_some());

    auto assets = owe::fs::make_physical_fs(
        owe::fs::Path(rstd::cppstd::as_str(WAYWALLEN_ASSETS_DIR).unwrap()));
    ASSERT_TRUE(assets.is_ok());
    owe::fs::VFS vfs;
    ASSERT_TRUE(vfs.mount("/assets"_str, std::move(assets).unwrap_unchecked()).is_ok());

    wavsen::audio::SoundManager sound_manager;
    owe::SceneParser            parser;
    auto                        parsed = parser.Parse(
        "prefixed-light-kinds"_str,
        rstd::ref<owe::wpscene::SceneDocument>::from_raw_parts(rstd::addressof(*document)),
        rstd::mut_ref<owe::fs::VFS>::from_raw_parts(rstd::addressof(vfs)),
        rstd::mut_ref<wavsen::audio::SoundManager>::from_raw_parts(rstd::addressof(sound_manager)));
    ASSERT_TRUE(parsed.is_ok());

    auto scene  = rstd::move(parsed).unwrap();
    auto lights = scene.scene->Lights();
    ASSERT_EQ(lights.len(), rstd::usize(2));
    EXPECT_EQ(lights[rstd::usize()]->type(), owe::SceneLightType::Spot);
    EXPECT_EQ(lights[rstd::usize(1)]->type(), owe::SceneLightType::Directional);
    const float deg_to_rad = rstd::f32::consts::PI.to_primitive() / 180.0f;
    EXPECT_NEAR(lights[rstd::usize()]->desc().inner_cone_cos, std::cos(72.29f * deg_to_rad), 1e-5f);
    EXPECT_NEAR(lights[rstd::usize()]->desc().outer_cone_cos, std::cos(77.94f * deg_to_rad), 1e-5f);
}

// Measured in Wallpaper Engine with ceilings over a lit floor under a sun: a model whose material is blended
// translucent or additive casts no shadow; a normal one does.
TEST(SceneShadowParsing, OnlyModelsWithoutBlendingCastShadows) {
    EXPECT_TRUE(owe::BlendingCastsShadow("normal"_str));
    EXPECT_TRUE(owe::BlendingCastsShadow("disabled"_str));
    EXPECT_TRUE(owe::BlendingCastsShadow(""_str));
    EXPECT_FALSE(owe::BlendingCastsShadow("translucent"_str));
    EXPECT_FALSE(owe::BlendingCastsShadow("additive"_str));
}

TEST(SceneShadowParsing, RequiresRendererCapabilityBeforeRegisteringDerivedResources) {
    auto document = owe::wpscene::ParseSceneDocumentJson(
        R"JSON({
            "camera": {},
            "general": {
                "lightconfig": {"directionalshadow": 1},
                "orthogonalprojection": {"width": 1920, "height": 1080}
            },
            "objects": [{
                "id": 1,
                "name": "Directional",
                "light": "ldirectional",
                "castshadow": true,
                "origin": [0.0, 0.0, 0.0],
                "angles": [0.0, 0.0, 0.0],
                "scale": [1.0, 1.0, 1.0]
            }, {
                "id": 2,
                "name": "Caster",
                "model": "models/editor/camera/camera.mdl",
                "castshadow": true,
                "origin": [0.0, 0.0, 0.0],
                "angles": [0.0, 0.0, 0.0],
                "scale": [1.0, 1.0, 1.0]
            }]
        })JSON"_str,
        owe::wpscene::kSceneVersionUnknown);
    ASSERT_TRUE(document.is_some());

    auto assets = owe::fs::make_physical_fs(
        owe::fs::Path(rstd::cppstd::as_str(WAYWALLEN_ASSETS_DIR).unwrap()));
    ASSERT_TRUE(assets.is_ok());
    owe::fs::VFS vfs;
    ASSERT_TRUE(vfs.mount("/assets"_str, std::move(assets).unwrap_unchecked()).is_ok());

    wavsen::audio::SoundManager sound_manager;
    owe::SceneParser            parser;
    auto                        unsupported = parser.Parse(
        "shadow-capability-disabled"_str,
        rstd::ref<owe::wpscene::SceneDocument>::from_raw_parts(rstd::addressof(*document)),
        rstd::mut_ref<owe::fs::VFS>::from_raw_parts(rstd::addressof(vfs)),
        rstd::mut_ref<wavsen::audio::SoundManager>::from_raw_parts(rstd::addressof(sound_manager)));
    ASSERT_TRUE(unsupported.is_ok());
    auto unsupported_scene = rstd::move(unsupported).unwrap();
    EXPECT_TRUE(unsupported_scene.scene->ShadowDefinitions().is_empty());
    EXPECT_TRUE(unsupported_scene.scene->RenderTarget("_rt_shadowAtlas"_str).is_none());

    auto supported = parser.Parse(
        "shadow-capability-enabled"_str,
        rstd::ref<owe::wpscene::SceneDocument>::from_raw_parts(rstd::addressof(*document)),
        rstd::mut_ref<owe::fs::VFS>::from_raw_parts(rstd::addressof(vfs)),
        rstd::mut_ref<wavsen::audio::SoundManager>::from_raw_parts(rstd::addressof(sound_manager)),
        owe::SceneParseOptions {
            .capabilities = { .directional_shadow = true },
        });
    ASSERT_TRUE(supported.is_ok());
    auto supported_scene = rstd::move(supported).unwrap();
    ASSERT_EQ(supported_scene.scene->ShadowDefinitions().len(), rstd::usize(1));
    auto atlas = supported_scene.scene->RenderTarget("_rt_shadowAtlas"_str);
    ASSERT_TRUE(atlas.is_some());
    EXPECT_EQ((**atlas).kind, owe::SceneRenderTargetKind::DepthSampled);
    EXPECT_FLOAT_EQ((**atlas).depth_clear_value, 0.0f);
    EXPECT_TRUE((**atlas).sample.compare_enable);
    EXPECT_EQ((**atlas).sample.compare_op, owe::CompareOp::Greater);
    // Wallpaper Engine's "medium" shadows: a 256-pixel square for each cascade, in rows of a near
    // square (two columns for three squares)
    EXPECT_EQ((**atlas).width, rstd::i32(512));
    EXPECT_EQ((**atlas).height, rstd::i32(512));
    const auto& definition = supported_scene.scene->ShadowDefinitions()[rstd::usize()];
    EXPECT_EQ(definition.light_id, rstd::i32(1));
    EXPECT_EQ(definition.view_offset, rstd::u32());
    const auto& viewports = definition.viewports;
    ASSERT_EQ(viewports.len(), rstd::usize(3));
    const float corners[3][2] { { 0, 0 }, { 256, 0 }, { 0, 256 } };
    for (rstd::usize index {}; index < viewports.len(); ++index) {
        const auto& viewport = viewports[index];
        const auto  corner   = corners[index.to_primitive()];
        EXPECT_FLOAT_EQ(viewport.width.to_primitive(), 256.0f);
        EXPECT_FLOAT_EQ(viewport.height.to_primitive(), -256.0f);
        EXPECT_FLOAT_EQ(viewport.x.to_primitive(), corner[0]);
        EXPECT_FLOAT_EQ(viewport.y.to_primitive(), corner[1] + 256.0f);
        EXPECT_EQ(viewport.scissor_x, rstd::i32(static_cast<int>(corner[0])));
        EXPECT_EQ(viewport.scissor_y, rstd::i32(static_cast<int>(corner[1])));
        EXPECT_EQ(viewport.scissor_width, rstd::u32(256));
        EXPECT_EQ(viewport.scissor_height, rstd::u32(256));
    }
    // the light knows where its views start
    auto lights = supported_scene.scene->Lights();
    ASSERT_EQ(lights.len(), rstd::usize(1));
    EXPECT_EQ(lights[rstd::usize()]->desc().shadow_view, rstd::i32());
    EXPECT_EQ(unsupported_scene.scene->Lights()[rstd::usize()]->desc().shadow_view, rstd::i32(-1));
}

// Spot lights cast shadow maps too, as many as the light config's `spotshadow`; their squares
// come first in the atlas, before the cascades (Wallpaper Engine's order).
// Wallpaper Engine's shadow quality setting sizes the squares: 256 pixels at medium (the default),
// 512 at high, 1024 at ultra.
TEST(SceneShadowParsing, QualitySizesTheSquares) {
    auto document = owe::wpscene::ParseSceneDocumentJson(
        R"JSON({
            "camera": {},
            "general": {
                "lightconfig": {"directionalshadow": 1},
                "orthogonalprojection": {"width": 1920, "height": 1080}
            },
            "objects": [{
                "id": 1, "name": "Directional", "light": "ldirectional", "castshadow": true,
                "origin": [0.0, 0.0, 0.0], "angles": [0.0, 0.0, 0.0], "scale": [1.0, 1.0, 1.0]
            }, {
                "id": 2, "name": "Caster", "model": "models/editor/camera/camera.mdl", "castshadow": true,
                "origin": [0.0, 0.0, 0.0], "angles": [0.0, 0.0, 0.0], "scale": [1.0, 1.0, 1.0]
            }]
        })JSON"_str,
        owe::wpscene::kSceneVersionUnknown);
    ASSERT_TRUE(document.is_some());

    auto assets = owe::fs::make_physical_fs(
        owe::fs::Path(rstd::cppstd::as_str(WAYWALLEN_ASSETS_DIR).unwrap()));
    ASSERT_TRUE(assets.is_ok());
    owe::fs::VFS vfs;
    ASSERT_TRUE(vfs.mount("/assets"_str, std::move(assets).unwrap_unchecked()).is_ok());
    wavsen::audio::SoundManager sound_manager;
    owe::SceneParser            parser;
    struct Case {
        owe::ShadowQuality quality;
        int                tile;
    };
    const Case cases[] { { owe::ShadowQuality::Medium, 256 },
                         { owe::ShadowQuality::High, 512 },
                         { owe::ShadowQuality::Ultra, 1024 } };
    for (const auto& c : cases) {
        auto parsed = parser.Parse(
            "shadow-quality"_str,
            rstd::ref<owe::wpscene::SceneDocument>::from_raw_parts(rstd::addressof(*document)),
            rstd::mut_ref<owe::fs::VFS>::from_raw_parts(rstd::addressof(vfs)),
            rstd::mut_ref<wavsen::audio::SoundManager>::from_raw_parts(
                rstd::addressof(sound_manager)),
            owe::SceneParseOptions {
                .capabilities   = { .directional_shadow = true },
                .shadow_quality = c.quality,
            });
        ASSERT_TRUE(parsed.is_ok());
        auto scene = rstd::move(parsed).unwrap();
        auto atlas = scene.scene->RenderTarget("_rt_shadowAtlas"_str);
        ASSERT_TRUE(atlas.is_some());
        // three cascades in two columns
        EXPECT_EQ((**atlas).width, rstd::i32(2 * c.tile)) << c.tile;
        EXPECT_EQ((**atlas).height, rstd::i32(2 * c.tile)) << c.tile;
        const auto& viewports = scene.scene->ShadowDefinitions()[rstd::usize()].viewports;
        EXPECT_FLOAT_EQ(viewports[rstd::usize(1)].x.to_primitive(), static_cast<float>(c.tile));
        EXPECT_EQ(viewports[rstd::usize()].scissor_width, rstd::u32(static_cast<unsigned>(c.tile)));
    }
}

TEST(SceneShadowParsing, SpotLightsHaveTheFirstSquares) {
    auto document = owe::wpscene::ParseSceneDocumentJson(
        R"JSON({
            "camera": {},
            "general": {
                "lightconfig": {"directional": 1, "spot": 2, "directionalshadow": 1, "spotshadow": 1},
                "orthogonalprojection": {"width": 1920, "height": 1080}
            },
            "objects": [{
                "id": 1, "name": "Directional", "light": "ldirectional", "castshadow": true,
                "origin": [0.0, 0.0, 0.0], "angles": [0.0, 0.0, 0.0], "scale": [1.0, 1.0, 1.0]
            }, {
                "id": 3, "name": "Spot", "light": "lspot", "castshadow": true,
                "origin": [0.0, 0.0, 0.0], "angles": [0.0, 0.0, 0.0], "scale": [1.0, 1.0, 1.0]
            }, {
                "id": 4, "name": "Second spot", "light": "lspot", "castshadow": true,
                "origin": [0.0, 0.0, 0.0], "angles": [0.0, 0.0, 0.0], "scale": [1.0, 1.0, 1.0]
            }, {
                "id": 2, "name": "Caster", "model": "models/editor/camera/camera.mdl", "castshadow": true,
                "origin": [0.0, 0.0, 0.0], "angles": [0.0, 0.0, 0.0], "scale": [1.0, 1.0, 1.0]
            }]
        })JSON"_str,
        owe::wpscene::kSceneVersionUnknown);
    ASSERT_TRUE(document.is_some());

    auto assets = owe::fs::make_physical_fs(
        owe::fs::Path(rstd::cppstd::as_str(WAYWALLEN_ASSETS_DIR).unwrap()));
    ASSERT_TRUE(assets.is_ok());
    owe::fs::VFS vfs;
    ASSERT_TRUE(vfs.mount("/assets"_str, std::move(assets).unwrap_unchecked()).is_ok());

    wavsen::audio::SoundManager sound_manager;
    owe::SceneParser            parser;
    auto                        parsed = parser.Parse(
        "shadow-spot"_str,
        rstd::ref<owe::wpscene::SceneDocument>::from_raw_parts(rstd::addressof(*document)),
        rstd::mut_ref<owe::fs::VFS>::from_raw_parts(rstd::addressof(vfs)),
        rstd::mut_ref<wavsen::audio::SoundManager>::from_raw_parts(rstd::addressof(sound_manager)),
        owe::SceneParseOptions {
            .capabilities = { .directional_shadow = true },
        });
    ASSERT_TRUE(parsed.is_ok());
    auto scene = rstd::move(parsed).unwrap();
    // four squares: the spot's and three cascades
    auto atlas = scene.scene->RenderTarget("_rt_shadowAtlas"_str);
    ASSERT_TRUE(atlas.is_some());
    EXPECT_EQ((**atlas).width, rstd::i32(512));
    EXPECT_EQ((**atlas).height, rstd::i32(512));
    const auto definitions = scene.scene->ShadowDefinitions();
    ASSERT_EQ(definitions.len(), rstd::usize(2));
    EXPECT_EQ(definitions[rstd::usize()].light_id, rstd::i32(3));
    EXPECT_EQ(definitions[rstd::usize()].view_offset, rstd::u32());
    ASSERT_EQ(definitions[rstd::usize()].viewports.len(), rstd::usize(1));
    EXPECT_EQ(definitions[rstd::usize()].viewports[rstd::usize()].scissor_x, rstd::i32());
    EXPECT_EQ(definitions[rstd::usize()].viewports[rstd::usize()].scissor_y, rstd::i32());
    EXPECT_EQ(definitions[rstd::usize(1)].light_id, rstd::i32(1));
    EXPECT_EQ(definitions[rstd::usize(1)].view_offset, rstd::u32(1));
    ASSERT_EQ(definitions[rstd::usize(1)].viewports.len(), rstd::usize(3));
    EXPECT_EQ(definitions[rstd::usize(1)].viewports[rstd::usize()].scissor_x, rstd::i32(256));

    auto lights = scene.scene->Lights();
    ASSERT_EQ(lights.len(), rstd::usize(3));
    EXPECT_EQ(lights[rstd::usize()]->desc().shadow_view, rstd::i32(1));
    EXPECT_EQ(lights[rstd::usize(1)]->desc().shadow_view, rstd::i32());
    // the config shadows one spot light
    EXPECT_EQ(lights[rstd::usize(2)]->desc().shadow_view, rstd::i32(-1));
}

// Point lights cast shadow maps too, as many as the light config's `pointshadow`: one square each,
// after the spots' and the cascades', holding the six views (+x -x, +y -y, +z -z) in two columns
// and three rows.
TEST(SceneShadowParsing, PointLightsHaveSixFacesInTheirSquare) {
    auto document = owe::wpscene::ParseSceneDocumentJson(
        R"JSON({
            "camera": {},
            "general": {
                "lightconfig": {"point": 1, "spot": 1, "pointshadow": 1, "spotshadow": 1},
                "orthogonalprojection": {"width": 1920, "height": 1080}
            },
            "objects": [{
                "id": 5, "name": "Point", "light": "lpoint", "castshadow": true,
                "origin": [0.0, 0.0, 0.0], "angles": [0.0, 0.0, 0.0], "scale": [1.0, 1.0, 1.0]
            }, {
                "id": 3, "name": "Spot", "light": "lspot", "castshadow": true,
                "origin": [0.0, 0.0, 0.0], "angles": [0.0, 0.0, 0.0], "scale": [1.0, 1.0, 1.0]
            }, {
                "id": 2, "name": "Caster", "model": "models/editor/camera/camera.mdl", "castshadow": true,
                "origin": [0.0, 0.0, 0.0], "angles": [0.0, 0.0, 0.0], "scale": [1.0, 1.0, 1.0]
            }]
        })JSON"_str,
        owe::wpscene::kSceneVersionUnknown);
    ASSERT_TRUE(document.is_some());

    auto assets = owe::fs::make_physical_fs(
        owe::fs::Path(rstd::cppstd::as_str(WAYWALLEN_ASSETS_DIR).unwrap()));
    ASSERT_TRUE(assets.is_ok());
    owe::fs::VFS vfs;
    ASSERT_TRUE(vfs.mount("/assets"_str, std::move(assets).unwrap_unchecked()).is_ok());

    wavsen::audio::SoundManager sound_manager;
    owe::SceneParser            parser;
    auto                        parsed = parser.Parse(
        "shadow-point"_str,
        rstd::ref<owe::wpscene::SceneDocument>::from_raw_parts(rstd::addressof(*document)),
        rstd::mut_ref<owe::fs::VFS>::from_raw_parts(rstd::addressof(vfs)),
        rstd::mut_ref<wavsen::audio::SoundManager>::from_raw_parts(rstd::addressof(sound_manager)),
        owe::SceneParseOptions {
            .capabilities = { .directional_shadow = true },
        });
    ASSERT_TRUE(parsed.is_ok());
    auto scene = rstd::move(parsed).unwrap();
    // two squares: the spot's, then the point's
    auto atlas = scene.scene->RenderTarget("_rt_shadowAtlas"_str);
    ASSERT_TRUE(atlas.is_some());
    EXPECT_EQ((**atlas).width, rstd::i32(512));
    EXPECT_EQ((**atlas).height, rstd::i32(256));
    const auto definitions = scene.scene->ShadowDefinitions();
    ASSERT_EQ(definitions.len(), rstd::usize(2));
    EXPECT_EQ(definitions[rstd::usize()].light_id, rstd::i32(3));
    const auto& point = definitions[rstd::usize(1)];
    EXPECT_EQ(point.light_id, rstd::i32(5));
    EXPECT_EQ(point.type, owe::SceneLightType::Point);
    EXPECT_EQ(point.view_offset, rstd::u32(1));
    ASSERT_EQ(point.viewports.len(), rstd::usize(6));
    for (rstd::usize face {}; face < point.viewports.len(); ++face) {
        const auto& viewport = point.viewports[face];
        const int   index    = static_cast<int>(face.to_primitive());
        EXPECT_EQ(viewport.scissor_x, rstd::i32(256 + (index % 2) * 128)) << index;
        EXPECT_EQ(viewport.scissor_y, rstd::i32((index / 2) * 85)) << index;
        EXPECT_EQ(viewport.scissor_width, rstd::u32(128));
        EXPECT_EQ(viewport.scissor_height, rstd::u32(85));
    }
    auto lights = scene.scene->Lights();
    ASSERT_EQ(lights.len(), rstd::usize(2));
    EXPECT_EQ(lights[rstd::usize()]->desc().shadow_view, rstd::i32(1));
    EXPECT_EQ(lights[rstd::usize(1)]->desc().shadow_view, rstd::i32());
}

TEST(ModelObjectJson, ReadsMaterialSkin) {
    auto parsed = owe::ParseJson(R"({"model":"models/prism.mdl","skin":2,"perspective":true})"_str);
    ASSERT_TRUE(parsed.is_ok());

    owe::fs::VFS              vfs;
    owe::wpscene::ModelObject model;
    ASSERT_TRUE(model.FromJson(parsed.unwrap(), vfs));
    EXPECT_EQ(model.skin, rstd::u32(2));
    EXPECT_TRUE(model.perspective);
}

TEST(SceneCameraParsing, PerspectiveOverridePreservesTheOrthographicReferencePlane) {
    auto document = owe::wpscene::ParseSceneDocumentJson(
        R"JSON({
            "camera": {},
            "general": {
                "nearz": 0.01,
                "farz": 10000.0,
                "perspectiveoverridefov": 21.0,
                "orthogonalprojection": {"width": 7680, "height": 4320}
            },
            "objects": []
        })JSON"_str,
        owe::wpscene::kSceneVersionUnknown);
    ASSERT_TRUE(document.is_some());

    auto assets = owe::fs::make_physical_fs(
        owe::fs::Path(rstd::cppstd::as_str(WAYWALLEN_ASSETS_DIR).unwrap()));
    ASSERT_TRUE(assets.is_ok());
    owe::fs::VFS vfs;
    ASSERT_TRUE(vfs.mount("/assets"_str, std::move(assets).unwrap_unchecked()).is_ok());

    wavsen::audio::SoundManager sound_manager;
    owe::SceneParser            parser;
    auto                        parsed = parser.Parse(
        "perspective-override"_str,
        rstd::ref<owe::wpscene::SceneDocument>::from_raw_parts(rstd::addressof(*document)),
        rstd::mut_ref<owe::fs::VFS>::from_raw_parts(rstd::addressof(vfs)),
        rstd::mut_ref<wavsen::audio::SoundManager>::from_raw_parts(rstd::addressof(sound_manager)));
    ASSERT_TRUE(parsed.is_ok());

    auto scene  = rstd::move(parsed).unwrap();
    auto camera = scene.scene->CameraMut("global_perspective"_str);
    ASSERT_TRUE(camera.is_some());
    EXPECT_DOUBLE_EQ((**camera).Fov(), 21.0);
    scene.scene->UpdateCameraFillMode(rstd::u32(1920), rstd::u32(1080), owe::FillMode::ASPECTCROP);
    EXPECT_DOUBLE_EQ((**camera).Fov(), 21.0);
    const double expected_distance =
        4320.0 / (2.0 * std::tan(21.0 * rstd::f64::consts::PI.to_primitive() / 360.0));
    EXPECT_NEAR((**camera).GetPosition().z(), expected_distance, 1e-3);

    const auto view_projection = (**camera).GetViewProjectionMatrix();
    EXPECT_NEAR(view_projection(0, 0), 3.03497815, 1e-6);
    EXPECT_NEAR(view_projection(1, 1), 5.39551687, 1e-6);
    EXPECT_NEAR(view_projection(2, 2), 0.000333444477, 1e-9);
    EXPECT_NEAR(view_projection(2, 3), 1.11559916, 1e-6);
    EXPECT_NEAR(view_projection(3, 3), expected_distance, 1e-3);
}

TEST(SceneCameraParsing, OrthographicSceneDerivesPerspectiveFovFromViewport) {
    auto document = owe::wpscene::ParseSceneDocumentJson(
        R"JSON({
            "camera": {},
            "general": {"orthogonalprojection": {"width": 1920, "height": 1080}},
            "objects": []
        })JSON"_str,
        owe::wpscene::kSceneVersionUnknown);
    ASSERT_TRUE(document.is_some());
    auto assets = owe::fs::make_physical_fs(
        owe::fs::Path(rstd::cppstd::as_str(WAYWALLEN_ASSETS_DIR).unwrap()));
    ASSERT_TRUE(assets.is_ok());
    owe::fs::VFS vfs;
    ASSERT_TRUE(vfs.mount("/assets"_str, rstd::move(assets).unwrap_unchecked()).is_ok());
    wavsen::audio::SoundManager sound_manager;
    owe::SceneParser            parser;
    auto                        parsed = parser.Parse(
        "ortho-perspective"_str,
        rstd::ref<owe::wpscene::SceneDocument>::from_raw_parts(rstd::addressof(*document)),
        rstd::mut_ref<owe::fs::VFS>::from_raw_parts(rstd::addressof(vfs)),
        rstd::mut_ref<wavsen::audio::SoundManager>::from_raw_parts(rstd::addressof(sound_manager)));
    ASSERT_TRUE(parsed.is_ok());
    auto scene  = rstd::move(parsed).unwrap();
    auto camera = scene.scene->Camera("global_perspective"_str);
    ASSERT_TRUE(camera.is_some());
    EXPECT_TRUE((**camera).FovPolicy() == owe::SceneCameraFovPolicy::OrthographicProjection);
    scene.scene->UpdateCameraFillMode(rstd::u32(1280), rstd::u32(720), owe::FillMode::ASPECTCROP);
    EXPECT_NEAR((**camera).Fov(), 56.738092586, 0.0001);
}

TEST(BloomParsing, PreservesPassBindingsAndAuthoredConstants) {
    auto document = owe::wpscene::ParseSceneDocumentJson(R"({
        "camera":{}, "general":{
            "orthogonalprojection":{"width":128,"height":128},
            "bloom":true,"bloomstrength":1.5,"bloomthreshold":0.25,
            "bloomtint":[0.2,0.4,0.6]
        }, "objects":[]
    })"_str,
                                                         owe::wpscene::kSceneVersionUnknown);
    ASSERT_TRUE(document.is_some());
    auto assets = owe::fs::make_physical_fs(
        owe::fs::Path(rstd::cppstd::as_str(WAYWALLEN_ASSETS_DIR).unwrap()));
    ASSERT_TRUE(assets.is_ok());
    owe::fs::VFS vfs;
    ASSERT_TRUE(vfs.mount("/assets"_str, rstd::move(assets).unwrap()).is_ok());
    wavsen::audio::SoundManager sound;
    owe::SceneParser            parser;
    auto                        parsed = parser.Parse(
        "bloom-material"_str,
        rstd::ref<owe::wpscene::SceneDocument>::from_raw_parts(rstd::addressof(*document)),
        rstd::mut_ref<owe::fs::VFS>::from_raw_parts(rstd::addressof(vfs)),
        rstd::mut_ref<wavsen::audio::SoundManager>::from_raw_parts(rstd::addressof(sound)));
    ASSERT_TRUE(parsed.is_ok());
    auto chains = parsed->scene->PostProcesses();
    ASSERT_EQ(chains.len(), rstd::usize(1));
    const auto& steps = chains[rstd::usize()]->steps;
    ASSERT_EQ(steps.len(), rstd::usize(5));
    for (rstd::usize i {}; i < rstd::usize(4); ++i) ASSERT_TRUE(steps[i].is_Pass());
    const auto& first = steps[rstd::usize()].as_Pass().value;
    EXPECT_EQ(first.output, "_rt_bloom_mip1"_str);
    const auto* material = first.node->Mesh()->Material();
    ASSERT_NE(material, nullptr);
    ASSERT_EQ(material->textures.len(), rstd::usize(1));
    EXPECT_EQ(material->textures[rstd::usize()], "_rt_default"_str);
    const auto& values = material->customShader.constValues;
    ASSERT_TRUE(values.contains_key("g_BloomStrength"_str));
    ASSERT_TRUE(values.contains_key("g_BloomThreshold"_str));
    ASSERT_TRUE(values.contains_key("g_BloomTint"_str));
    EXPECT_FLOAT_EQ(values.get("g_BloomStrength"_str).unwrap()->data()[0], 1.5f);
    EXPECT_FLOAT_EQ(values.get("g_BloomThreshold"_str).unwrap()->data()[0], 0.25f);
    auto tint = values.get("g_BloomTint"_str).unwrap();
    ASSERT_EQ(tint->size(), rstd::usize(3));
    EXPECT_FLOAT_EQ(tint->data()[0], 0.2f);
    EXPECT_FLOAT_EQ(tint->data()[1], 0.4f);
    EXPECT_FLOAT_EQ(tint->data()[2], 0.6f);
    const auto* combine = steps[rstd::usize(3)].as_Pass().value.node->Mesh()->Material();
    ASSERT_NE(combine, nullptr);
    ASSERT_EQ(combine->textures.len(), rstd::usize(2));
    EXPECT_EQ(combine->textures[rstd::usize()], "_rt_default"_str);
    EXPECT_EQ(combine->textures[rstd::usize(1)], "_rt_bloom_mip3"_str);
    ASSERT_TRUE(steps[rstd::usize(4)].is_Copy());
    EXPECT_EQ(steps[rstd::usize(4)].as_Copy().value.src, "_rt_bloom_combine"_str);
    EXPECT_EQ(steps[rstd::usize(4)].as_Copy().value.dst, "_rt_default"_str);
}

// An effect's render target can have a size of its own (glitter's tiles: 256 x 256) and say how its
// texture coordinates wrap ("uvs"; they repeat unless it says "clamp"): the picture's size and
// clamped edges it got before left streaks where the effect samples past the first tile
// (effect-fbo-wrap, effect-glitter).
TEST(EffectFboJson, ReadsItsFixedSizeAndWrap) {
    owe::wpscene::EffectFbo tiles;
    auto                    json = owe::ParseJson(
        R"({"name":"_rt_GlitterTiles","format":"r8","width":256,"height":128,"uvs":"repeat"})"_str);
    ASSERT_TRUE(json.is_ok());
    ASSERT_TRUE(tiles.FromJson(*json));
    EXPECT_EQ(tiles.width, rstd::u32(256));
    EXPECT_EQ(tiles.height, rstd::u32(128));
    EXPECT_EQ(tiles.uvs, "repeat"_str);

    owe::wpscene::EffectFbo plain;
    auto                    other = owe::ParseJson(R"({"name":"_rt_x","format":"rgba8","scale":2})"_str);
    ASSERT_TRUE(other.is_ok());
    ASSERT_TRUE(plain.FromJson(*other));
    EXPECT_EQ(plain.width, rstd::u32());
    EXPECT_EQ(plain.height, rstd::u32());
    EXPECT_TRUE(plain.uvs.is_empty());
}

TEST(ImageEffectJson, PreservesCopyPositionsAndComposeBindings) {
    auto json = owe::ParseJson(R"({
        "name":"compose-test", "version":1,
        "fbos":[{"name":"scratch","format":"rgba8","scale":2}],
        "passes":[
            {"command":"copy","source":"previous","target":"scratch"},
            {"material":"materials/util/passthrough.json"},
            {"command":"copy","source":"scratch","target":"previous"},
            {"material":"materials/util/passthrough.json","compose":true}
        ]
    })"_str);
    ASSERT_TRUE(json.is_ok());
    auto assets = owe::fs::make_physical_fs(
        owe::fs::Path(rstd::cppstd::as_str(WAYWALLEN_ASSETS_DIR).unwrap()));
    ASSERT_TRUE(assets.is_ok());
    owe::fs::VFS vfs;
    ASSERT_TRUE(vfs.mount("/assets"_str, rstd::move(assets).unwrap()).is_ok());
    owe::wpscene::ImageEffect effect;
    ASSERT_TRUE(effect.FromFileJson(*json, vfs));
    EXPECT_EQ(effect.name, "compose-test"_str);
    ASSERT_EQ(effect.commands.len(), rstd::usize(2));
    EXPECT_EQ(effect.commands[rstd::usize()].afterpos, rstd::i32());
    EXPECT_EQ(effect.commands[rstd::usize(1)].afterpos, rstd::i32(1));
    EXPECT_EQ(effect.commands[rstd::usize()].source, "previous"_str);
    ASSERT_EQ(effect.materials.len(), rstd::usize(2));
    ASSERT_EQ(effect.passes.len(), rstd::usize(2));
    ASSERT_EQ(effect.fbos.len(), rstd::usize(2));
    EXPECT_EQ(effect.fbos[rstd::usize()].name, "scratch"_str);
    EXPECT_EQ(effect.fbos[rstd::usize()].scale, rstd::u32(2));
    EXPECT_EQ(effect.fbos[rstd::usize(1)].name, "_rt_FullCompoBuffer1"_str);
    EXPECT_EQ(effect.passes[rstd::usize()].target, "_rt_FullCompoBuffer1"_str);
    ASSERT_EQ(effect.passes[rstd::usize()].bind.len(), rstd::usize(1));
    ASSERT_EQ(effect.passes[rstd::usize(1)].bind.len(), rstd::usize(1));
    EXPECT_EQ(effect.passes[rstd::usize()].bind[rstd::usize()].name, "previous"_str);
    EXPECT_EQ(effect.passes[rstd::usize(1)].bind[rstd::usize()].name, "_rt_FullCompoBuffer1"_str);
}

TEST(ImageEffectJson, FailedEffectsAreAbsentFromParsedObjects) {
    auto image_json = owe::ParseJson(R"({
        "image": "models/util/fullscreenlayer.json",
        "effects": [
            {"file": "effects/_empty/effect.json", "visible": true},
            {"file": "effects/missing/effect.json", "visible": true}
        ]
    })"_str);
    auto shape_json = owe::ParseJson(R"({
        "shape": "rectangle",
        "origin": [0.0, 0.0, 0.0],
        "angles": [0.0, 0.0, 0.0],
        "scale": [1.0, 1.0, 1.0],
        "effects": [
            {"file": "effects/_empty/effect.json", "visible": true},
            {"file": "effects/missing/effect.json", "visible": true}
        ]
    })"_str);
    ASSERT_TRUE(image_json.is_ok());
    ASSERT_TRUE(shape_json.is_ok());

    auto assets = owe::fs::make_physical_fs(
        owe::fs::Path(rstd::cppstd::as_str(WAYWALLEN_ASSETS_DIR).unwrap()));
    ASSERT_TRUE(assets.is_ok());
    auto effect_assets = owe::fs::make_physical_fs(owe::fs::Path(
        rstd::cppstd::as_str(std::string(WAYWALLEN_ASSETS_DIR) + "/effects/_empty").unwrap()));
    ASSERT_TRUE(effect_assets.is_ok());
    owe::fs::VFS vfs;
    ASSERT_TRUE(vfs.mount("/assets"_str, std::move(assets).unwrap_unchecked()).is_ok());
    ASSERT_TRUE(vfs.mount("/assets"_str, std::move(effect_assets).unwrap_unchecked()).is_ok());

    owe::wpscene::ImageObject image;
    ASSERT_TRUE(image.FromJson(image_json.unwrap(), vfs));
    ASSERT_EQ(image.effects.len(), rstd::usize(1));
    EXPECT_FALSE(image.effects[rstd::usize()].materials.is_empty());

    owe::wpscene::ShapeObject shape;
    ASSERT_TRUE(shape.FromJson(shape_json.unwrap(), vfs, owe::wpscene::kSceneVersionUnknown));
    ASSERT_EQ(shape.effects.len(), rstd::usize(1));
    EXPECT_FALSE(shape.effects[rstd::usize()].materials.is_empty());
}

// A model that says "autosize" is drawn at its first texture's size, whatever the layer's "size" (Wallpaper Engine draws
// script-workshop-path's 256 x 256 layer of an autosize model at the gradient texture's 64 x 2).
TEST(ImageObjectJson, AutosizeModelTakesItsTexturesSize) {
    const auto dir = std::filesystem::temp_directory_path() / "owe-autosize-test";
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir / "models");
    std::filesystem::create_directories(dir / "materials");
    std::ofstream(dir / "models/sq.json") << R"({"autosize": true, "material": "materials/sq.json"})";
    std::ofstream(dir / "models/plain.json") << R"({"material": "materials/sq.json"})";
    std::ofstream(dir / "materials/sq.json")
        << R"({"passes": [{"shader": "genericimage2", "textures": ["strip"]}]})";
    {
        std::ofstream tex(dir / "materials/strip.tex", std::ios::binary);
        tex.write("TEXV0005\0TEXI0001\0", 18);
        const std::uint32_t header[] = { 0, 2, 64, 4, 50, 3, 0 };  // format, flags, texture and image sizes
        tex.write(reinterpret_cast<const char*>(header), sizeof(header));
    }
    auto assets = owe::fs::make_physical_fs(owe::fs::Path(rstd::cppstd::as_str(dir.string()).unwrap()));
    ASSERT_TRUE(assets.is_ok());
    owe::fs::VFS vfs;
    ASSERT_TRUE(vfs.mount("/assets"_str, std::move(assets).unwrap_unchecked()).is_ok());

    auto layer = [&](const char* model) {
        auto json = owe::ParseJson(rstd::cppstd::as_str(std::string(R"({"image": ")") + model +
                                                        R"(", "size": "256 256", "origin": "0 0 0"})").unwrap());
        owe::wpscene::ImageObject image;
        EXPECT_TRUE(image.FromJson(json.unwrap(), vfs));
        return image.size;
    };
    const auto autosized = layer("models/sq.json");
    EXPECT_EQ(autosized[rstd::usize(0)], 50.0f);
    EXPECT_EQ(autosized[rstd::usize(1)], 3.0f);
    const auto sized = layer("models/plain.json");
    EXPECT_EQ(sized[rstd::usize(0)], 256.0f);
    EXPECT_EQ(sized[rstd::usize(1)], 256.0f);
    // a sprite sheet's image holds all its frames: autosize doesn't take it (Dino Run's 24 x 24 dinos are 144 x 24 sheets);
    // the layer is drawn at one frame's size, which the image parser reads from the sheet's frames
    std::ofstream(dir / "models/sprite.json") << R"({"autosize": true, "material": "materials/sprite.json"})";
    std::ofstream(dir / "materials/sprite.json")
        << R"({"passes": [{"shader": "genericimage2", "textures": ["sheet"]}]})";
    {
        std::ofstream tex(dir / "materials/sheet.tex", std::ios::binary);
        tex.write("TEXV0005\0TEXI0001\0", 18);
        const std::uint32_t header[] = { 0, 5, 256, 32, 144, 24, 0 };  // flags 4: a sprite sheet
        tex.write(reinterpret_cast<const char*>(header), sizeof(header));
    }
    const auto sprite = layer("models/sprite.json");
    EXPECT_EQ(sprite[rstd::usize(0)], 256.0f);
    EXPECT_EQ(sprite[rstd::usize(1)], 256.0f);
    {
        auto json = owe::ParseJson(
            R"({"image": "models/sprite.json", "size": "256 256", "origin": "0 0 0"})"_str);
        owe::wpscene::ImageObject image;
        EXPECT_TRUE(image.FromJson(json.unwrap(), vfs));
        EXPECT_TRUE(image.autosize_sprite_sheet);
        auto plain_json = owe::ParseJson(
            R"({"image": "models/sq.json", "size": "256 256", "origin": "0 0 0"})"_str);
        owe::wpscene::ImageObject plain;
        EXPECT_TRUE(plain.FromJson(plain_json.unwrap(), vfs));
        EXPECT_FALSE(plain.autosize_sprite_sheet);
    }
    std::filesystem::remove_all(dir);
}

namespace
{

// A 2x2 texture of `format` (0: RGBA8888, 8: RG88), uncompressed.
void WriteFormatTexture(const std::filesystem::path& path, std::uint32_t format) {
    const std::uint32_t channels = format == 8 ? 2 : 4;
    std::ofstream       output(path, std::ios::binary);
    auto                u32 = [&](std::uint32_t value) {
        output.write(reinterpret_cast<const char*>(&value), sizeof(value));
    };
    output.write("TEXV0005", 9);
    output.write("TEXI0001", 9);
    for (std::uint32_t value : { format, 0u, 2u, 2u, 2u, 2u, 0u }) u32(value);
    output.write("TEXB0003", 9);
    for (std::uint32_t value : { 1u, 0xffffffffu, 1u, 2u, 2u, 0u, 4 * channels, 4 * channels })
        u32(value);
    for (std::uint32_t byte = 0; byte < 4 * channels; ++byte) output.put(char(0x80));
}

} // namespace

// Wallpaper Engine gives a shader every texture's format as TEXnFORMAT: generic4 decodes an RG88
// normal map (g_Texture1) from .rg only when TEX1FORMAT says so, and otherwise from .wy, which is
// 1 in an RG88 texture (LittleBigPlanet's clouds lit as if every normal leaned the same way).
namespace
{

const std::vector<owe::testing::WorkshopProbe>& AllWorkshopProbes() {
    static const auto v = owe::testing::EnumerateWorkshopProbes(WAYWALLEN_WORKSHOP_DIR);
    return v;
}

const std::vector<std::string>& ObservedPkgVersionStamps() {
    static const auto v = [] {
        std::set<std::string> uniq;
        for (const auto& p : AllWorkshopProbes())
            if (! p.pkg_stamp.empty()) uniq.insert(p.pkg_stamp);
        return std::vector<std::string>(uniq.begin(), uniq.end());
    }();
    return v;
}

} // namespace

TEST(MaterialTextureFormats, EverySlotGetsItsFormat) {
    auto root = std::filesystem::temp_directory_path() /
                ("owe-texture-formats-" + std::to_string(rstd::process::id().to_primitive()));
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root / "models");
    std::filesystem::create_directories(root / "materials");
    std::ofstream(root / "models" / "cloud.json") << R"({"material": "materials/cloud.json"})";
    std::ofstream(root / "materials" / "cloud.json")
        << R"({"passes": [{"shader": "generic4", "textures": ["cloud_d", "cloud_n"]}]})";
    WriteFormatTexture(root / "materials" / "cloud_d.tex", 0);
    WriteFormatTexture(root / "materials" / "cloud_n.tex", 8);

    auto document = owe::wpscene::ParseSceneDocumentJson(
        R"JSON({
            "camera": {},
            "general": {"orthogonalprojection": {"width": 1920, "height": 1080}},
            "objects": [{"id": 1, "name": "Cloud", "image": "models/cloud.json", "visible": true}]
        })JSON"_str,
        owe::wpscene::kSceneVersionUnknown);
    ASSERT_TRUE(document.is_some());

    auto assets = owe::fs::make_physical_fs(
        owe::fs::Path(rstd::cppstd::as_str(WAYWALLEN_ASSETS_DIR).unwrap()));
    ASSERT_TRUE(assets.is_ok());
    auto own =
        owe::fs::make_physical_fs(owe::fs::Path(rstd::cppstd::as_str(root.string()).unwrap()));
    ASSERT_TRUE(own.is_ok());
    owe::fs::VFS vfs;
    ASSERT_TRUE(vfs.mount("/assets"_str, std::move(assets).unwrap_unchecked()).is_ok());
    ASSERT_TRUE(vfs.mount("/assets"_str, std::move(own).unwrap_unchecked()).is_ok());

    wavsen::audio::SoundManager sound_manager;
    owe::SceneParser            parser;
    auto                        parsed = parser.Parse(
        "texture-formats"_str,
        rstd::ref<owe::wpscene::SceneDocument>::from_raw_parts(rstd::addressof(*document)),
        rstd::mut_ref<owe::fs::VFS>::from_raw_parts(rstd::addressof(vfs)),
        rstd::mut_ref<wavsen::audio::SoundManager>::from_raw_parts(rstd::addressof(sound_manager)));
    ASSERT_TRUE(parsed.is_ok());
    auto scene = rstd::move(parsed).unwrap();
    auto cloud = scene.scene->RootMut()->FindByName("Cloud"_str);
    ASSERT_NE(cloud, nullptr);
    ASSERT_NE(cloud->Mesh(), nullptr);
    auto* material = cloud->Mesh()->Material();
    ASSERT_NE(material, nullptr);
    ASSERT_TRUE(material->customShader.variant.is_some());
    const auto& combos = material->customShader.variant->Description().input_combos;
    ASSERT_TRUE(combos.get("TEX1FORMAT"_str).is_some());
    EXPECT_EQ(*combos.get("TEX1FORMAT"_str).unwrap(), "FORMAT_RG88"_str);
    // an RGBA8888 colour map needs no combo (0, like a missing one)
    EXPECT_TRUE(combos.get("TEX0FORMAT"_str).is_none());
    std::filesystem::remove_all(root);
}
// namespace

void CheckPkgVersionParse(const std::string& stamp) {
    std::size_t hits = 0;
    for (const auto& p : AllWorkshopProbes()) {
        if (p.pkg_stamp != stamp) continue;
        ++hits;
        SCOPED_TRACE("workshop " + p.id + " " + p.pkg_stamp);

        auto r = owe::testing::ProbeSceneParse(p.dir);
        EXPECT_TRUE(r.ok) << "ProbeSceneParse failed: " << r.error;
        EXPECT_EQ(r.pkg_version, p.pkg_version) << "pkg_version mismatch for " << p.id;
    }
    EXPECT_GT(hits, 0u) << "no workshops for " << stamp;
}

TEST(ScenePkgVersionParseTest, AllWorkshopsParseAtExplicitVersion) {
    for (const auto& stamp : ObservedPkgVersionStamps()) CheckPkgVersionParse(stamp);
}

TEST(SceneParseSmoke, EnumeratesNonEmpty) {
    EXPECT_FALSE(AllWorkshopProbes().empty());
    EXPECT_FALSE(ObservedPkgVersionStamps().empty());
}

TEST(SceneDocumentLoading, VersionStampPreservesStrictUnsignedGrammar) {
    using owe::wpscene::ParsePkgVersionStamp;
    EXPECT_EQ(ParsePkgVersionStamp("PKGV0001"_str), 1);
    EXPECT_EQ(ParsePkgVersionStamp("PKGV65535"_str), 65535);
    EXPECT_EQ(ParsePkgVersionStamp("PKGV00000023"_str), 23);
    for (auto invalid : rstd::array<rstd::ref<rstd::str>, 11> { ""_str,
                                                                "PKGV"_str,
                                                                "pkgv23"_str,
                                                                " PKGV23"_str,
                                                                "PKGV+23"_str,
                                                                "PKGV-1"_str,
                                                                "PKGV23 "_str,
                                                                "PKGV23x"_str,
                                                                "PKGV65536"_str,
                                                                "PKGV999999999999999999999"_str,
                                                                "PKGV23\0tail"_str }) {
        EXPECT_EQ(ParsePkgVersionStamp(invalid), owe::wpscene::kSceneVersionUnknown);
    }
}

TEST(SceneDocumentLoading, NativePathsShareDocumentParsingAndRejectInvalidInputs) {
    auto temporary = rstd::fs::TempDir::make("owe-scene-document"_str).unwrap();
    auto root      = rstd::path::PathBuf::from(temporary.path().as_os_str().to_os_string());
    auto source    = root.join("scene.JsOn"_str);
    auto content   = R"({"camera":{},"general":{},"objects":[{"id":7,"name":"kept"}]})"_str;
    ASSERT_TRUE(rstd::fs::write(source.as_path(), content->as_bytes()).is_ok());

    auto direct = owe::wpscene::LoadSceneDocumentFromSource(source.as_path());
    ASSERT_TRUE(direct.is_some());
    ASSERT_EQ(direct->objects.len(), rstd::usize(1));
    EXPECT_EQ(direct->objects[rstd::usize()].metadata.name, "kept"_str);
    EXPECT_EQ(direct->metadata.pkg_version, owe::wpscene::kSceneVersionUnknown);

    owe::fs::VFS vfs;
    auto         mount = owe::fs::make_physical_fs(temporary.path());
    ASSERT_TRUE(mount.is_ok());
    ASSERT_TRUE(vfs.mount("/assets"_str, rstd::move(mount).unwrap()).is_ok());
    auto mounted =
        owe::wpscene::LoadSceneDocumentFromVfs(vfs, owe::fs::Path("/assets/scene.JsOn"_str), 23);
    ASSERT_TRUE(mounted.is_some());
    EXPECT_EQ(mounted->metadata.pkg_version, 23);
    ASSERT_EQ(mounted->objects.len(), rstd::usize(1));
    EXPECT_EQ(mounted->objects[rstd::usize()].metadata.id, rstd::i32(7));

    auto unsupported = root.join("scene.txt"_str);
    ASSERT_TRUE(rstd::fs::write(unsupported.as_path(), content->as_bytes()).is_ok());
    EXPECT_TRUE(owe::wpscene::LoadSceneDocumentFromSource(unsupported.as_path()).is_none());
    EXPECT_TRUE(owe::wpscene::LoadSceneDocumentFromSource(root.join("missing.json"_str).as_path())
                    .is_none());
    ASSERT_TRUE(rstd::fs::write(source.as_path(), "{"_bytes).is_ok());
    EXPECT_TRUE(owe::wpscene::LoadSceneDocumentFromSource(source.as_path()).is_none());
    EXPECT_TRUE(
        owe::wpscene::LoadSceneDocumentFromVfs(vfs, owe::fs::Path("/assets/scene.JsOn"_str), 23)
            .is_none());
}

TEST(SceneAssetPaths, ImageMetadataAndObjectOwnNativeResourceNames) {
    auto temporary     = rstd::fs::TempDir::make("owe-scene-assets"_str).unwrap();
    auto root          = rstd::path::PathBuf::from(temporary.path().as_os_str().to_os_string());
    auto image_path    = root.join("image.json"_str);
    auto material_path = root.join("material.json"_str);
    ASSERT_TRUE(rstd::fs::write(
                    image_path.as_path(),
                    R"({"material":"material.json","puppet":"model.mdl","solidlayer":true})"_bytes)
                    .is_ok());
    ASSERT_TRUE(rstd::fs::write(
                    material_path.as_path(),
                    R"({"passes":[{"shader":"genericimage","textures":["texture-name"]}]})"_bytes)
                    .is_ok());
    owe::fs::VFS vfs;
    auto         mount = owe::fs::make_physical_fs(temporary.path());
    ASSERT_TRUE(mount.is_ok());
    ASSERT_TRUE(vfs.mount("/assets"_str, rstd::move(mount).unwrap()).is_ok());

    auto info = owe::wpscene::LoadImageAssetInfo(vfs, "image.json"_str);
    ASSERT_TRUE(info.is_some());
    EXPECT_TRUE(info->solid_layer);
    EXPECT_TRUE(info->size.is_none());
    EXPECT_EQ(info->first_texture, "texture-name"_str);

    owe::wpscene::ImageObject image;
    ASSERT_TRUE(image.FromAsset("image.json"_str, { 4.0f, 8.0f }, vfs, 23));
    EXPECT_EQ(image.image, "image.json"_str);
    EXPECT_EQ(image.material_path, "material.json"_str);
    EXPECT_EQ(image.puppet, "model.mdl"_str);

    ASSERT_TRUE(rstd::fs::write(image_path.as_path(),
                                R"({"width":16,"height":32,"material":"missing.json"})"_bytes)
                    .is_ok());
    info = owe::wpscene::LoadImageAssetInfo(vfs, "image.json"_str);
    ASSERT_TRUE(info.is_some());
    ASSERT_TRUE(info->size.is_some());
    EXPECT_FLOAT_EQ((*info->size)[rstd::usize(0)], 16.0f);
    EXPECT_FLOAT_EQ((*info->size)[rstd::usize(1)], 32.0f);
    EXPECT_TRUE(info->first_texture.is_empty());
    EXPECT_EQ(image.material_path, "material.json"_str);
}

TEST(SceneAssetPaths, ModelCameraAndClipRetainTheirReparseSemantics) {
    owe::fs::VFS vfs;
    auto         source =
        rstd::json::from_str(
            R"({"model":"mesh.mdl","camera":"main","path":"motion.json","name":"clip"})"_str)
            .unwrap();
    owe::wpscene::ModelObject    model;
    owe::wpscene::CameraObject   camera;
    owe::wpscene::CameraPathClip clip;
    ASSERT_TRUE(model.FromJson(source, vfs));
    ASSERT_TRUE(camera.FromJson(source, vfs));
    ASSERT_TRUE(clip.FromJson(source));
    source = owe::Json {};
    EXPECT_EQ(model.model, "mesh.mdl"_str);
    EXPECT_EQ(camera.camera, "main"_str);
    EXPECT_EQ(camera.path, "motion.json"_str);
    EXPECT_EQ(clip.name, "clip"_str);

    auto empty = rstd::json::from_str("{}"_str).unwrap();
    ASSERT_TRUE(model.FromJson(empty, vfs));
    ASSERT_TRUE(camera.FromJson(empty, vfs));
    ASSERT_TRUE(clip.FromJson(empty));
    EXPECT_EQ(model.model, "mesh.mdl"_str);
    EXPECT_EQ(camera.path, "motion.json"_str);
    EXPECT_TRUE(clip.name.is_empty());
}

// Wallpaper Engine draws the last pass of a layer's last effect straight onto the scene, at the screen's resolution,
// whatever its shader (probes effect-repeat-scaled and effect-pass-resolution): a passthrough of the layer's effect
// target is only for a layer none of whose effects loaded.
TEST(EffectFinalPassthrough, NotNeededAfterAnyLoadedEffect) {
    EXPECT_FALSE(owe::NeedsEffectFinalPassthrough({ .last_effect_loaded = true }));
}

TEST(EffectFinalPassthrough, NeededWhenNoEffectLoaded) {
    EXPECT_TRUE(owe::NeedsEffectFinalPassthrough({ .last_effect_loaded = false }));
}

TEST(EffectFinalPassthrough, NeverForLayersDrawnAnotherWay) {
    EXPECT_FALSE(owe::NeedsEffectFinalPassthrough({ .linked_source = true }));
    EXPECT_FALSE(owe::NeedsEffectFinalPassthrough({ .fullscreen = true }));
    EXPECT_FALSE(owe::NeedsEffectFinalPassthrough({ .copybackground = true }));
    EXPECT_FALSE(owe::NeedsEffectFinalPassthrough({ .source_composites_final = true }));
}

// Wallpaper Engine draws an image layer's texture 0.15 texel in from the quad's edges at each end, counted in the
// texture's own texels (probes texture-uv-inset, -big and -size).
TEST(LayerTexCoordInset, IsAFixedShareOfATextureTexel) {
    auto inset = owe::LayerTexCoordInset(4.0f, 1.0f);
    EXPECT_FLOAT_EQ(inset[rstd::usize(0)], 0.15f / 4.0f);
    EXPECT_FLOAT_EQ(inset[rstd::usize(1)], 0.15f);
    auto padded = owe::LayerTexCoordInset(128.0f, 32.0f);
    EXPECT_FLOAT_EQ(padded[rstd::usize(0)], 0.15f / 128.0f);
    EXPECT_FLOAT_EQ(padded[rstd::usize(1)], 0.15f / 32.0f);
}

TEST(LayerTexCoordInset, PutsTexelEdgesWhereWallpaperEngineDraws) {
    // a 4-texel texture across a quad: Wallpaper Engine's texel edges at 0.919, 2.000 and 3.081 texels of the quad
    auto       inset = owe::LayerTexCoordInset(4.0f, 4.0f)[rstd::usize(0)];
    const auto edge  = [&](float u) { return (u - inset) / (1.0f - 2.0f * inset) * 4.0f; };
    EXPECT_NEAR(edge(0.25f), 0.919f, 0.003f);
    EXPECT_NEAR(edge(0.5f), 2.0f, 0.003f);
    EXPECT_NEAR(edge(0.75f), 3.081f, 0.003f);
}

TEST(LayerTexCoordInset, NoneForAnEmptyTexture) {
    auto inset = owe::LayerTexCoordInset(0.0f, 0.0f);
    EXPECT_EQ(inset[rstd::usize(0)], 0.0f);
    EXPECT_EQ(inset[rstd::usize(1)], 0.0f);
}

// As in Wallpaper Engine (model-depth-sorted): with the scene's transparentsorting, translucent
// models write depth as their material says; without it they write none.
TEST(ModelParsing, SortedTranslucentModelsWriteDepth) {
    auto root = std::filesystem::temp_directory_path() /
                ("owe-sorted-depth-" + std::to_string(rstd::process::id().to_primitive()));
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root / "models");
    std::filesystem::create_directories(root / "materials");
    std::ofstream(root / "materials" / "glass.json") << R"({"passes": [{
        "shader": "generic4", "blending": "translucent", "cullmode": "normal",
        "depthtest": "enabled", "depthwrite": "enabled",
        "combos": {"LIGHTING": 0, "REFLECTION": 0, "RIMLIGHTING": 0, "SHADINGGRADIENT": 0},
        "constantshadervalues": {"color": "0 1 0", "alpha": 0.5}, "textures": [null]}]})";
    {
        // one triangle (MDLV0017: position, normal, tangent and sign, uv per vertex)
        std::string mdl("MDLV0017", 9);
        auto        put = [&](auto value) {
            mdl.append(reinterpret_cast<const char*>(&value), sizeof value);
        };
        put(std::int32_t(15));
        put(std::int32_t(1));
        put(std::uint32_t(1));
        mdl += "materials/glass.json";
        mdl.push_back('\0');
        put(std::int32_t(0));
        for (float bound : { -1.0f, -1.0f, 0.0f, 1.0f, 1.0f, 0.0f }) put(bound);
        put(std::int32_t(15));
        const float vertices[3][12] = {
            { -1, -1, 0, 0, 0, 1, 1, 0, 0, 1, 0, 0 },
            { 1, -1, 0, 0, 0, 1, 1, 0, 0, 1, 1, 0 },
            { 0, 1, 0, 0, 0, 1, 1, 0, 0, 1, 0.5f, 1 },
        };
        put(std::uint32_t(sizeof vertices));
        for (const auto& vertex : vertices)
            for (float value : vertex) put(value);
        put(std::uint32_t(6));
        for (std::uint16_t index : { 0, 1, 2 }) put(index);
        std::ofstream(root / "models" / "glass.mdl", std::ios::binary) << mdl;
    }

    for (bool sorted : { false, true }) {
        auto json     = std::string(R"JSON({
            "camera": {"center": "0 0 0", "eye": "0 0 1", "up": "0 1 0"},
            "general": {"orthogonalprojection": null, "fov": 50, "nearz": 0.1, "farz": 100,
                        "transparentsorting": )JSON") +
                        (sorted ? "true" : "false") + R"JSON(},
            "objects": [{"id": 1, "name": "Glass", "model": "models/glass.mdl",
                         "origin": "0 0 -3", "visible": true}]
        })JSON";
        auto document = owe::wpscene::ParseSceneDocumentJson(rstd::cppstd::as_str(json).unwrap(),
                                                             owe::wpscene::kSceneVersionUnknown);
        ASSERT_TRUE(document.is_some());

        auto assets = owe::fs::make_physical_fs(
            owe::fs::Path(rstd::cppstd::as_str(WAYWALLEN_ASSETS_DIR).unwrap()));
        ASSERT_TRUE(assets.is_ok());
        auto own =
            owe::fs::make_physical_fs(owe::fs::Path(rstd::cppstd::as_str(root.string()).unwrap()));
        ASSERT_TRUE(own.is_ok());
        owe::fs::VFS vfs;
        ASSERT_TRUE(vfs.mount("/assets"_str, std::move(assets).unwrap_unchecked()).is_ok());
        ASSERT_TRUE(vfs.mount("/assets"_str, std::move(own).unwrap_unchecked()).is_ok());

        wavsen::audio::SoundManager sound_manager;
        owe::SceneParser            parser;
        auto                        parsed = parser.Parse(
            "sorted-depth"_str,
            rstd::ref<owe::wpscene::SceneDocument>::from_raw_parts(rstd::addressof(*document)),
            rstd::mut_ref<owe::fs::VFS>::from_raw_parts(rstd::addressof(vfs)),
            rstd::mut_ref<wavsen::audio::SoundManager>::from_raw_parts(
                rstd::addressof(sound_manager)));
        ASSERT_TRUE(parsed.is_ok());
        auto  scene = rstd::move(parsed).unwrap();
        auto* glass = scene.scene->RootMut()->FindByName("Glass"_str);
        ASSERT_NE(glass, nullptr);
        ASSERT_NE(glass->Mesh(), nullptr);
        ASSERT_NE(glass->Mesh()->Material(), nullptr);
        EXPECT_EQ(glass->Mesh()->Material()->Pipeline().depth_write, sorted);
    }

    std::filesystem::remove_all(root);
}

// A light without an exponent falls off as (1 - distance / radius) squared in Wallpaper Engine
// (model-light-point, model-light-spot): its exponent is 2, an authored one is kept.
TEST(LightParsing, LightsWithoutAnExponentFallOffSquared) {
    auto document = owe::wpscene::ParseSceneDocumentJson(
        R"JSON({
            "camera": {}, "general": {},
            "objects": [
                {"id": 1, "name": "Default", "light": "lpoint", "radius": 30},
                {"id": 2, "name": "Authored", "light": "lpoint", "radius": 30, "exponent": 3}
            ]
        })JSON"_str,
        owe::wpscene::kSceneVersionUnknown);
    ASSERT_TRUE(document.is_some());
    owe::fs::VFS vfs;
    auto         decoded = owe::wpscene::DecodeSceneObjects(
        rstd::ref<owe::wpscene::SceneDocument>::from_raw_parts(rstd::addressof(*document)),
        rstd::mut_ref<owe::fs::VFS>::from_raw_parts(rstd::addressof(vfs)));
    ASSERT_EQ(decoded.len(), rstd::usize(2));
    ASSERT_TRUE(decoded[rstd::usize(0)].is_Light());
    ASSERT_TRUE(decoded[rstd::usize(1)].is_Light());
    EXPECT_FLOAT_EQ(decoded[rstd::usize(0)].as_Light().value.exponent, 2.0f);
    EXPECT_FLOAT_EQ(decoded[rstd::usize(1)].as_Light().value.exponent, 3.0f);
}

// A spot light's usecookie projects its cookie over its cone, for as many lights as the light
// config counts under spotcookie; there is one cookie sampler, so all of them use the last such
// light's texture (model-light-cookie, model-light-cookies).
TEST(LightParsing, CookieLightsTakeTheSpotCookieSlots) {
    auto document = owe::wpscene::ParseSceneDocumentJson(
        R"JSON({
            "camera": {"center": "0 0 0", "eye": "0 0 12", "up": "0 1 0"},
            "general": {"orthogonalprojection": null,
                        "lightconfig": {"point": 0, "spot": 3, "tube": 0, "directional": 0,
                                        "spotcookie": 1}},
            "objects": [
                {"id": 1, "name": "First", "light": "lspot", "outercone": 30, "innercone": 10,
                 "usecookie": true, "cookie": "cookie/flashlight3"},
                {"id": 2, "name": "Second", "light": "lspot", "outercone": 30, "innercone": 10,
                 "usecookie": true},
                {"id": 3, "name": "Plain", "light": "lspot", "outercone": 30, "innercone": 10}
            ]
        })JSON"_str,
        owe::wpscene::kSceneVersionUnknown);
    ASSERT_TRUE(document.is_some());
    owe::fs::VFS                vfs;
    wavsen::audio::SoundManager sound_manager;
    owe::SceneParser            parser;
    auto                        parsed = parser.Parse(
        "light-cookies"_str,
        rstd::ref<owe::wpscene::SceneDocument>::from_raw_parts(rstd::addressof(*document)),
        rstd::mut_ref<owe::fs::VFS>::from_raw_parts(rstd::addressof(vfs)),
        rstd::mut_ref<wavsen::audio::SoundManager>::from_raw_parts(rstd::addressof(sound_manager)));
    ASSERT_TRUE(parsed.is_ok());
    auto scene  = rstd::move(parsed).unwrap();
    auto lights = scene.scene->Lights();
    ASSERT_EQ(lights.len(), rstd::usize(3));
    EXPECT_TRUE(lights[rstd::usize(0)]->desc().cookie);
    EXPECT_FALSE(lights[rstd::usize(1)]->desc().cookie);
    EXPECT_FALSE(lights[rstd::usize(2)]->desc().cookie);
    ASSERT_TRUE(scene.scene->LightCookieTexture().is_some());
    EXPECT_EQ(*scene.scene->LightCookieTexture(), "cookie/flashlight3"_str);
}

// genericimage2 takes g_UserAlpha and g_Brightness from its material settings ("Alpha",
// "Brightness"); a layer's own alpha and brightness don't reach them (image-alpha-variants:
// Wallpaper Engine draws a layer of alpha 0.5 and brightness 0.5 as if both were 1).
TEST(ImageParsing, LayerAlphaAndBrightnessDoNotReachGenericImage2Settings) {
    auto root = std::filesystem::temp_directory_path() /
                ("owe-material-alpha-" + std::to_string(rstd::process::id().to_primitive()));
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root / "models");
    std::filesystem::create_directories(root / "materials");
    for (auto [name, constants] :
         { std::pair { "plain", "{}" }, std::pair { "setting", R"({"Alpha": 0.5})" } }) {
        std::ofstream(root / "models" / (std::string(name) + ".json"))
            << R"({"material": "materials/)" << name << R"(.json"})";
        std::ofstream(root / "materials" / (std::string(name) + ".json"))
            << R"({"passes": [{"shader": "genericimage2", "blending": "translucent",
                "cullmode": "nocull", "depthtest": "disabled", "depthwrite": "disabled",
                "textures": ["gradient/gradient_rainbow"], "constantshadervalues": )"
            << constants << "}]}";
    }

    auto document = owe::wpscene::ParseSceneDocumentJson(
        R"JSON({
            "camera": {},
            "general": {"orthogonalprojection": {"width": 1920, "height": 1080}},
            "objects": [
                {"id": 1, "name": "Layer", "image": "models/plain.json", "size": "64 2",
                 "alpha": 0.5, "brightness": 0.5, "visible": true},
                {"id": 2, "name": "Setting", "image": "models/setting.json", "size": "64 2",
                 "visible": true}
            ]
        })JSON"_str,
        owe::wpscene::kSceneVersionUnknown);
    ASSERT_TRUE(document.is_some());

    auto assets = owe::fs::make_physical_fs(
        owe::fs::Path(rstd::cppstd::as_str(WAYWALLEN_ASSETS_DIR).unwrap()));
    ASSERT_TRUE(assets.is_ok());
    auto own =
        owe::fs::make_physical_fs(owe::fs::Path(rstd::cppstd::as_str(root.string()).unwrap()));
    ASSERT_TRUE(own.is_ok());
    owe::fs::VFS vfs;
    ASSERT_TRUE(vfs.mount("/assets"_str, std::move(assets).unwrap_unchecked()).is_ok());
    ASSERT_TRUE(vfs.mount("/assets"_str, std::move(own).unwrap_unchecked()).is_ok());
    wavsen::audio::SoundManager sound_manager;
    owe::SceneParser            parser;
    auto                        parsed = parser.Parse(
        "material-alpha"_str,
        rstd::ref<owe::wpscene::SceneDocument>::from_raw_parts(rstd::addressof(*document)),
        rstd::mut_ref<owe::fs::VFS>::from_raw_parts(rstd::addressof(vfs)),
        rstd::mut_ref<wavsen::audio::SoundManager>::from_raw_parts(rstd::addressof(sound_manager)));
    ASSERT_TRUE(parsed.is_ok());
    auto scene = rstd::move(parsed).unwrap();

    auto setting_value = [&](rstd::ref<rstd::str> layer, rstd::ref<rstd::str> uniform) -> float {
        auto node = scene.scene->RootMut()->FindByName(layer);
        if (node == nullptr || node->Mesh() == nullptr || node->Mesh()->Material() == nullptr)
            return -1.0f;
        auto value = node->Mesh()->Material()->customShader.constValues.get(uniform);
        return value.is_some() ? (*value)->data()[0] : 1.0f;
    };
    EXPECT_FLOAT_EQ(setting_value("Layer"_str, "g_UserAlpha"_str), 1.0f);
    EXPECT_FLOAT_EQ(setting_value("Layer"_str, "g_Brightness"_str), 1.0f);
    EXPECT_FLOAT_EQ(setting_value("Setting"_str, "g_UserAlpha"_str), 0.5f);

    std::filesystem::remove_all(root);
}

// Neon Sunset's sky is a fullscreen layer with a material of its own (models/cloudsbg.json has
// "fullscreen" but no "passthrough"): it draws, so it can't be elided like the stock fullscreen
// layer, which only passes the frame through.
TEST(FullscreenImageParsing, OnlyPassthroughLayersAreElidable) {
    auto root = std::filesystem::temp_directory_path() /
                ("owe-fullscreen-" + std::to_string(rstd::process::id().to_primitive()));
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root / "models");
    std::ofstream(root / "models" / "sky.json")
        << R"({"material": "materials/util/fullscreenlayer.json", "fullscreen": true})";

    auto document = owe::wpscene::ParseSceneDocumentJson(
        R"JSON({
            "camera": {},
            "general": {"orthogonalprojection": {"width": 1920, "height": 1080}},
            "objects": [
                {"id": 1, "name": "Passthrough", "image": "models/util/fullscreenlayer.json", "visible": true},
                {"id": 2, "name": "Sky", "image": "models/sky.json", "visible": true}
            ]
        })JSON"_str,
        owe::wpscene::kSceneVersionUnknown);
    ASSERT_TRUE(document.is_some());

    auto assets = owe::fs::make_physical_fs(
        owe::fs::Path(rstd::cppstd::as_str(WAYWALLEN_ASSETS_DIR).unwrap()));
    ASSERT_TRUE(assets.is_ok());
    auto own =
        owe::fs::make_physical_fs(owe::fs::Path(rstd::cppstd::as_str(root.string()).unwrap()));
    ASSERT_TRUE(own.is_ok());
    owe::fs::VFS vfs;
    ASSERT_TRUE(vfs.mount("/assets"_str, std::move(assets).unwrap_unchecked()).is_ok());
    ASSERT_TRUE(vfs.mount("/assets"_str, std::move(own).unwrap_unchecked()).is_ok());

    wavsen::audio::SoundManager sound_manager;
    owe::SceneParser            parser;
    auto                        parsed = parser.Parse(
        "fullscreen-elision"_str,
        rstd::ref<owe::wpscene::SceneDocument>::from_raw_parts(rstd::addressof(*document)),
        rstd::mut_ref<owe::fs::VFS>::from_raw_parts(rstd::addressof(vfs)),
        rstd::mut_ref<wavsen::audio::SoundManager>::from_raw_parts(rstd::addressof(sound_manager)));
    ASSERT_TRUE(parsed.is_ok());
    auto scene = rstd::move(parsed).unwrap();

    EXPECT_TRUE(
        scene.scene->IsLayerStaticElidable(owe::WallpaperLayerId { .value = rstd::i32(1) }));
    EXPECT_FALSE(
        scene.scene->IsLayerStaticElidable(owe::WallpaperLayerId { .value = rstd::i32(2) }));

    std::filesystem::remove_all(root);
}

// The same holds for a fullscreen layer whose effects are all turned off while it runs: only a
// passthrough layer has nothing left to draw then.
TEST(FullscreenImageParsing, OnlyPassthroughLayersSkipWithoutEffects) {
    auto root = std::filesystem::temp_directory_path() /
                ("owe-fullscreen-effects-" + std::to_string(rstd::process::id().to_primitive()));
    std::filesystem::remove_all(root);
    for (auto folder : { "models", "materials/effects", "effects/copy", "shaders/effects" })
        std::filesystem::create_directories(root / folder);
    std::ofstream(root / "models" / "sky.json")
        << R"({"material": "materials/util/fullscreenlayer.json", "fullscreen": true})";
    std::ofstream(root / "effects" / "copy" / "effect.json")
        << R"({"version": 1, "name": "copy", "passes": [{"material": "materials/effects/copy.json"}]})";
    std::ofstream(root / "materials" / "effects" / "copy.json") << R"({"passes": [{
        "shader": "effects/copy", "blending": "normal", "cullmode": "nocull",
        "depthtest": "disabled", "depthwrite": "disabled"}]})";
    std::ofstream(root / "shaders" / "effects" / "copy.vert") << R"(
uniform mat4 g_ModelViewProjectionMatrix;
attribute vec3 a_Position;
attribute vec2 a_TexCoord;
varying vec2 v_TexCoord;
void main() {
    gl_Position = mul(vec4(a_Position, 1.0), g_ModelViewProjectionMatrix);
    v_TexCoord = a_TexCoord;
})";
    std::ofstream(root / "shaders" / "effects" / "copy.frag") << R"(
varying vec2 v_TexCoord;
uniform sampler2D g_Texture0; // {"hidden":true}
void main() { gl_FragColor = texSample2D(g_Texture0, v_TexCoord); })";

    auto document = owe::wpscene::ParseSceneDocumentJson(
        R"JSON({
            "camera": {},
            "general": {"orthogonalprojection": {"width": 1920, "height": 1080}},
            "objects": [
                {"id": 1, "name": "Passthrough", "image": "models/util/fullscreenlayer.json",
                 "visible": true, "effects": [{"file": "effects/copy/effect.json", "visible": true}]},
                {"id": 2, "name": "Sky", "image": "models/sky.json", "visible": true,
                 "effects": [{"file": "effects/copy/effect.json", "visible": true}]}
            ]
        })JSON"_str,
        owe::wpscene::kSceneVersionUnknown);
    ASSERT_TRUE(document.is_some());

    auto assets = owe::fs::make_physical_fs(
        owe::fs::Path(rstd::cppstd::as_str(WAYWALLEN_ASSETS_DIR).unwrap()));
    ASSERT_TRUE(assets.is_ok());
    auto own =
        owe::fs::make_physical_fs(owe::fs::Path(rstd::cppstd::as_str(root.string()).unwrap()));
    ASSERT_TRUE(own.is_ok());
    owe::fs::VFS vfs;
    ASSERT_TRUE(vfs.mount("/assets"_str, std::move(assets).unwrap_unchecked()).is_ok());
    ASSERT_TRUE(vfs.mount("/assets"_str, std::move(own).unwrap_unchecked()).is_ok());

    wavsen::audio::SoundManager sound_manager;
    owe::SceneParser            parser;
    auto                        parsed = parser.Parse(
        "fullscreen-effects"_str,
        rstd::ref<owe::wpscene::SceneDocument>::from_raw_parts(rstd::addressof(*document)),
        rstd::mut_ref<owe::fs::VFS>::from_raw_parts(rstd::addressof(vfs)),
        rstd::mut_ref<wavsen::audio::SoundManager>::from_raw_parts(rstd::addressof(sound_manager)));
    ASSERT_TRUE(parsed.is_ok());
    auto scene = rstd::move(parsed).unwrap();

    auto* passthrough = scene.scene->RootMut()->FindByName("Passthrough"_str);
    auto* sky         = scene.scene->RootMut()->FindByName("Sky"_str);
    ASSERT_NE(passthrough, nullptr);
    ASSERT_NE(sky, nullptr);
    ASSERT_TRUE(passthrough->HasLayer());
    ASSERT_TRUE(sky->HasLayer());
    EXPECT_TRUE(passthrough->Layer()->SkipWhenNoRuntimeEffect());
    EXPECT_FALSE(sky->Layer()->SkipWhenNoRuntimeEffect());

    std::filesystem::remove_all(root);
}

// The first pass of a layer's effects reads the layer's image in Wallpaper Engine, so its
// g_Texture0Resolution is the image texture's: its size padded to a multiple of 4, and the
// picture's size in zw (shader-uniforms: a 64x2 gradient reads 64x4, not the layer's 320x200).
TEST(EffectParsing, FirstPassSeesTheLayerImageResolution) {
    auto root = std::filesystem::temp_directory_path() /
                ("owe-effect-resolution-" + std::to_string(rstd::process::id().to_primitive()));
    std::filesystem::remove_all(root);
    for (auto folder : { "models", "materials/effects", "effects/copy", "shaders/effects" })
        std::filesystem::create_directories(root / folder);
    std::ofstream(root / "models" / "rainbow.json") << R"({"material": "materials/rainbow.json"})";
    std::ofstream(root / "materials" / "rainbow.json") << R"({"passes": [{
        "shader": "genericimage2", "blending": "translucent", "cullmode": "nocull",
        "depthtest": "disabled", "depthwrite": "disabled",
        "textures": ["gradient/gradient_rainbow"]}]})";
    std::ofstream(root / "effects" / "copy" / "effect.json")
        << R"({"version": 1, "name": "copy", "passes": [{"material": "materials/effects/copy.json"}]})";
    std::ofstream(root / "materials" / "effects" / "copy.json") << R"({"passes": [{
        "shader": "effects/copy", "blending": "normal", "cullmode": "nocull",
        "depthtest": "disabled", "depthwrite": "disabled"}]})";
    std::ofstream(root / "shaders" / "effects" / "copy.vert") << R"(
uniform mat4 g_ModelViewProjectionMatrix;
attribute vec3 a_Position;
attribute vec2 a_TexCoord;
varying vec4 v_TexCoord;
void main() {
    gl_Position = mul(vec4(a_Position, 1.0), g_ModelViewProjectionMatrix);
    v_TexCoord = a_TexCoord.xyxy;
})";
    std::ofstream(root / "shaders" / "effects" / "copy.frag") << R"(
varying vec4 v_TexCoord;
uniform sampler2D g_Texture0; // {"hidden":true}
uniform vec4 g_Texture0Resolution;
void main() {
    gl_FragColor = texSample2D(g_Texture0, v_TexCoord.xy) + g_Texture0Resolution * 0.0;
})";

    auto document = owe::wpscene::ParseSceneDocumentJson(
        R"JSON({
            "camera": {},
            "general": {"orthogonalprojection": {"width": 1920, "height": 1080}},
            "objects": [{
                "id": 1, "name": "Rainbow", "image": "models/rainbow.json", "size": "320 200",
                "visible": true,
                "effects": [{"file": "effects/copy/effect.json", "visible": true}]
            }]
        })JSON"_str,
        owe::wpscene::kSceneVersionUnknown);
    ASSERT_TRUE(document.is_some());

    auto assets = owe::fs::make_physical_fs(
        owe::fs::Path(rstd::cppstd::as_str(WAYWALLEN_ASSETS_DIR).unwrap()));
    ASSERT_TRUE(assets.is_ok());
    auto own =
        owe::fs::make_physical_fs(owe::fs::Path(rstd::cppstd::as_str(root.string()).unwrap()));
    ASSERT_TRUE(own.is_ok());
    owe::fs::VFS vfs;
    ASSERT_TRUE(vfs.mount("/assets"_str, std::move(assets).unwrap_unchecked()).is_ok());
    ASSERT_TRUE(vfs.mount("/assets"_str, std::move(own).unwrap_unchecked()).is_ok());
    wavsen::audio::SoundManager sound_manager;
    owe::SceneParser            parser;
    auto                        parsed = parser.Parse(
        "effect-resolution"_str,
        rstd::ref<owe::wpscene::SceneDocument>::from_raw_parts(rstd::addressof(*document)),
        rstd::mut_ref<owe::fs::VFS>::from_raw_parts(rstd::addressof(vfs)),
        rstd::mut_ref<wavsen::audio::SoundManager>::from_raw_parts(rstd::addressof(sound_manager)));
    ASSERT_TRUE(parsed.is_ok());
    auto scene = rstd::move(parsed).unwrap();
    auto node  = scene.scene->RootMut()->FindByName("Rainbow"_str);
    ASSERT_NE(node, nullptr);
    ASSERT_TRUE(node->HasLayer());
    auto& layer = node->Layer();
    layer->ResolveEffect(*scene.scene->DefaultEffectMesh(), "effect"_str);
    ASSERT_FALSE(layer->ResolvedEffects().is_empty());
    auto* first_effect = layer->ResolvedEffects().first().unwrap().get();
    ASSERT_FALSE(first_effect->Nodes().is_empty());
    auto* material = first_effect->Nodes().first().unwrap().get()->sceneNode->Mesh()->Material();
    ASSERT_NE(material, nullptr);
    ASSERT_FALSE(material->texture_metadata.is_empty());
    const auto& metadata = material->texture_metadata[rstd::usize()];
    EXPECT_TRUE(metadata.has_extent);
    EXPECT_FLOAT_EQ(metadata.source_extent[rstd::usize(0)], 64.0f);
    EXPECT_FLOAT_EQ(metadata.source_extent[rstd::usize(1)], 4.0f);
    EXPECT_FLOAT_EQ(metadata.sample_extent[rstd::usize(0)], 64.0f);
    EXPECT_FLOAT_EQ(metadata.sample_extent[rstd::usize(1)], 2.0f);

    std::filesystem::remove_all(root);
}

// Demon Core turns bloom on without a strength or threshold; Wallpaper Engine then blooms with the
// editor's defaults, which the shipped scenes write out as 2.0 and 0.65.
TEST(BloomParsing, MissingStrengthAndThresholdUseTheEditorDefaults) {
    auto document = owe::wpscene::ParseSceneDocumentJson(R"({
        "camera":{}, "general":{
            "orthogonalprojection":{"width":128,"height":128},
            "bloom":true
        }, "objects":[]
    })"_str,
                                                         owe::wpscene::kSceneVersionUnknown);
    ASSERT_TRUE(document.is_some());
    auto assets = owe::fs::make_physical_fs(
        owe::fs::Path(rstd::cppstd::as_str(WAYWALLEN_ASSETS_DIR).unwrap()));
    ASSERT_TRUE(assets.is_ok());
    owe::fs::VFS vfs;
    ASSERT_TRUE(vfs.mount("/assets"_str, rstd::move(assets).unwrap()).is_ok());
    wavsen::audio::SoundManager sound;
    owe::SceneParser            parser;
    auto                        parsed = parser.Parse(
        "bloom-defaults"_str,
        rstd::ref<owe::wpscene::SceneDocument>::from_raw_parts(rstd::addressof(*document)),
        rstd::mut_ref<owe::fs::VFS>::from_raw_parts(rstd::addressof(vfs)),
        rstd::mut_ref<wavsen::audio::SoundManager>::from_raw_parts(rstd::addressof(sound)));
    ASSERT_TRUE(parsed.is_ok());
    auto chains = parsed->scene->PostProcesses();
    ASSERT_EQ(chains.len(), rstd::usize(1));
    const auto& first  = chains[rstd::usize()]->steps[rstd::usize()].as_Pass().value;
    const auto& values = first.node->Mesh()->Material()->customShader.constValues;
    ASSERT_TRUE(values.contains_key("g_BloomStrength"_str));
    ASSERT_TRUE(values.contains_key("g_BloomThreshold"_str));
    EXPECT_FLOAT_EQ(values.get("g_BloomStrength"_str).unwrap()->data()[0], 2.0f);
    EXPECT_FLOAT_EQ(values.get("g_BloomThreshold"_str).unwrap()->data()[0], 0.65f);
}

// A layer's color can be animated like its origin or alpha (animation-final's "colour" layer goes
// from red to (0, 1, 0.5) and holds there).
TEST(AnimationParsing, ColorAnimationDrivesTheLayerColor) {
    auto document = owe::wpscene::ParseSceneDocumentJson(
        R"JSON({
            "camera": {},
            "general": {"orthogonalprojection": {"width": 1920, "height": 1080}},
            "objects": [{
                "id": 1, "name": "colour", "image": "models/util/solidlayer.json", "size": "200 120",
                "origin": "400 200 0", "visible": true,
                "color": {"value": "0 0 0", "animation": {
                    "c0": [{"frame": 0, "value": 1}, {"frame": 6, "value": 0}],
                    "c1": [{"frame": 0, "value": 0}, {"frame": 6, "value": 1}],
                    "c2": [{"frame": 0, "value": 0}, {"frame": 6, "value": 0.5}],
                    "options": {"fps": 30, "length": 6, "mode": "single"},
                    "relative": false}}
            }]
        })JSON"_str,
        owe::wpscene::kSceneVersionUnknown);
    ASSERT_TRUE(document.is_some());

    auto assets = owe::fs::make_physical_fs(
        owe::fs::Path(rstd::cppstd::as_str(WAYWALLEN_ASSETS_DIR).unwrap()));
    ASSERT_TRUE(assets.is_ok());
    owe::fs::VFS vfs;
    ASSERT_TRUE(vfs.mount("/assets"_str, std::move(assets).unwrap_unchecked()).is_ok());
    wavsen::audio::SoundManager sound_manager;
    owe::SceneParser            parser;
    auto                        parsed = parser.Parse(
        "color-animation"_str,
        rstd::ref<owe::wpscene::SceneDocument>::from_raw_parts(rstd::addressof(*document)),
        rstd::mut_ref<owe::fs::VFS>::from_raw_parts(rstd::addressof(vfs)),
        rstd::mut_ref<wavsen::audio::SoundManager>::from_raw_parts(rstd::addressof(sound_manager)));
    ASSERT_TRUE(parsed.is_ok());
    auto scene = rstd::move(parsed).unwrap();
    auto node  = scene.scene->RootMut()->FindByName("colour"_str);
    ASSERT_NE(node, nullptr);

    scene.scene->TickNodeFieldAnimations();
    EXPECT_FLOAT_EQ(node->Color().x(), 1.0f);
    EXPECT_FLOAT_EQ(node->Color().y(), 0.0f);

    scene.scene->Runtime().Advance(rstd::f64(1.0));
    scene.scene->TickNodeFieldAnimations();
    EXPECT_FLOAT_EQ(node->Color().x(), 0.0f);
    EXPECT_FLOAT_EQ(node->Color().y(), 1.0f);
    EXPECT_FLOAT_EQ(node->Color().z(), 0.5f);
}

// Measured with animation-base-value: an animated origin, scale, angles or colour is added to the
// property's value, whether the animation says "relative" or not; an animated alpha is the
// animation's value, relative or not.
TEST(AnimationParsing, VectorAnimationsAddToTheValueAndAlphaReplacesIt) {
    // Each animation holds one value: 100 added to x for the origins, 0.25 for the alphas.
    auto document = owe::wpscene::ParseSceneDocumentJson(
        R"JSON({
            "camera": {},
            "general": {"orthogonalprojection": {"width": 1920, "height": 1080}},
            "objects": [
                {"id": 1, "name": "origin", "image": "models/util/solidlayer.json", "size": "100 100",
                 "visible": true, "origin": {"value": "300 800 0", "animation": {
                     "c0": [{"frame": 0, "value": 100}, {"frame": 6, "value": 100}],
                     "c1": [{"frame": 0, "value": 0}, {"frame": 6, "value": 0}],
                     "c2": [{"frame": 0, "value": 0}, {"frame": 6, "value": 0}],
                     "options": {"fps": 30, "length": 6, "mode": "single"}, "relative": false}}},
                {"id": 2, "name": "origin relative", "image": "models/util/solidlayer.json",
                 "size": "100 100", "visible": true, "origin": {"value": "300 600 0", "animation": {
                     "c0": [{"frame": 0, "value": 100}, {"frame": 6, "value": 100}],
                     "c1": [{"frame": 0, "value": 0}, {"frame": 6, "value": 0}],
                     "c2": [{"frame": 0, "value": 0}, {"frame": 6, "value": 0}],
                     "options": {"fps": 30, "length": 6, "mode": "single"}, "relative": true}}},
                {"id": 3, "name": "alpha", "image": "models/util/solidlayer.json", "size": "100 100",
                 "visible": true, "alpha": {"value": 0.5, "animation": {
                     "c0": [{"frame": 0, "value": 0.25}, {"frame": 6, "value": 0.25}],
                     "options": {"fps": 30, "length": 6, "mode": "single"}, "relative": false}}},
                {"id": 4, "name": "alpha relative", "image": "models/util/solidlayer.json",
                 "size": "100 100", "visible": true, "alpha": {"value": 0.5, "animation": {
                     "c0": [{"frame": 0, "value": 0.25}, {"frame": 6, "value": 0.25}],
                     "options": {"fps": 30, "length": 6, "mode": "single"}, "relative": true}}}
            ]
        })JSON"_str,
        owe::wpscene::kSceneVersionUnknown);
    ASSERT_TRUE(document.is_some());

    auto assets = owe::fs::make_physical_fs(
        owe::fs::Path(rstd::cppstd::as_str(WAYWALLEN_ASSETS_DIR).unwrap()));
    ASSERT_TRUE(assets.is_ok());
    owe::fs::VFS vfs;
    ASSERT_TRUE(vfs.mount("/assets"_str, std::move(assets).unwrap_unchecked()).is_ok());
    wavsen::audio::SoundManager sound_manager;
    owe::SceneParser            parser;
    auto                        parsed = parser.Parse(
        "animation-base-value"_str,
        rstd::ref<owe::wpscene::SceneDocument>::from_raw_parts(rstd::addressof(*document)),
        rstd::mut_ref<owe::fs::VFS>::from_raw_parts(rstd::addressof(vfs)),
        rstd::mut_ref<wavsen::audio::SoundManager>::from_raw_parts(rstd::addressof(sound_manager)));
    ASSERT_TRUE(parsed.is_ok());
    auto scene = rstd::move(parsed).unwrap();
    scene.scene->Runtime().Advance(rstd::f64(1.0));
    scene.scene->TickNodeFieldAnimations();

    auto node = [&](rstd::ref<rstd::str> name) {
        return scene.scene->RootMut()->FindByName(name);
    };
    ASSERT_NE(node("origin"_str), nullptr);
    ASSERT_NE(node("origin relative"_str), nullptr);
    ASSERT_NE(node("alpha"_str), nullptr);
    ASSERT_NE(node("alpha relative"_str), nullptr);
    EXPECT_FLOAT_EQ(node("origin"_str)->Translate().x(), 400.0f);
    EXPECT_FLOAT_EQ(node("origin"_str)->Translate().y(), 800.0f);
    EXPECT_FLOAT_EQ(node("origin relative"_str)->Translate().x(), 400.0f);
    EXPECT_FLOAT_EQ(node("alpha"_str)->EffectiveAlpha(), 0.25f);
    EXPECT_FLOAT_EQ(node("alpha relative"_str)->EffectiveAlpha(), 0.25f);
}

// A light's animations follow the curve's `relative` (Wallpaper Engine's pictures of light-paused-animation: a colour
// animation holding white keeps a white light white, where adding it to the light's colour doubles it).
TEST(AnimationParsing, ALightsAnimationsFollowRelative) {
    auto document = owe::wpscene::ParseSceneDocumentJson(
        R"JSON({
            "camera": {"center": "0 0 0", "eye": "0 0 10", "up": "0 1 0"},
            "general": {"lightconfig": {"directional": 2}},
            "objects": [
                {"id": 1, "name": "absolute", "light": "ldirectional", "intensity": 1, "radius": 10,
                 "origin": "0 0 0", "angles": "0 0 0", "color": {"value": "1 1 1", "animation": {
                     "c0": [{"frame": 0, "value": 0.5}, {"frame": 6, "value": 0.5}],
                     "c1": [{"frame": 0, "value": 0.5}, {"frame": 6, "value": 0.5}],
                     "c2": [{"frame": 0, "value": 0.5}, {"frame": 6, "value": 0.5}],
                     "options": {"fps": 30, "length": 6, "mode": "single"}, "relative": false}}},
                {"id": 2, "name": "relative", "light": "ldirectional", "intensity": 1, "radius": 10,
                 "origin": "0 0 0", "angles": "0 0 0", "color": {"value": "0.5 0.5 0.5", "animation": {
                     "c0": [{"frame": 0, "value": 0.25}, {"frame": 6, "value": 0.25}],
                     "c1": [{"frame": 0, "value": 0.25}, {"frame": 6, "value": 0.25}],
                     "c2": [{"frame": 0, "value": 0.25}, {"frame": 6, "value": 0.25}],
                     "options": {"fps": 30, "length": 6, "mode": "single"}, "relative": true}}}
            ]
        })JSON"_str,
        owe::wpscene::kSceneVersionUnknown);
    ASSERT_TRUE(document.is_some());

    auto assets = owe::fs::make_physical_fs(
        owe::fs::Path(rstd::cppstd::as_str(WAYWALLEN_ASSETS_DIR).unwrap()));
    ASSERT_TRUE(assets.is_ok());
    owe::fs::VFS vfs;
    ASSERT_TRUE(vfs.mount("/assets"_str, std::move(assets).unwrap_unchecked()).is_ok());
    wavsen::audio::SoundManager sound_manager;
    owe::SceneParser            parser;
    auto                        parsed = parser.Parse(
        "light-animation"_str,
        rstd::ref<owe::wpscene::SceneDocument>::from_raw_parts(rstd::addressof(*document)),
        rstd::mut_ref<owe::fs::VFS>::from_raw_parts(rstd::addressof(vfs)),
        rstd::mut_ref<wavsen::audio::SoundManager>::from_raw_parts(rstd::addressof(sound_manager)));
    ASSERT_TRUE(parsed.is_ok());
    auto scene = rstd::move(parsed).unwrap();
    scene.scene->Runtime().Advance(rstd::f64(1.0));
    scene.scene->TickNodeFieldAnimations();

    auto absolute = scene.scene->RootMut()->FindByName("absolute"_str);
    auto relative = scene.scene->RootMut()->FindByName("relative"_str);
    ASSERT_NE(absolute, nullptr);
    ASSERT_NE(relative, nullptr);
    EXPECT_FLOAT_EQ(absolute->Color().x(), 0.5f);
    EXPECT_FLOAT_EQ(relative->Color().x(), 0.75f);
}
