module;

#include <rstd/enum.hpp>

module wescene.pkg.parse;
import :scene_context;
import eigen;
import wescene.pkg.spec_names;
import wescene.load_bench;
import wescene.core;
import wescene.types;
import rstd;
import rstd.log;
import wescene.utils;
import wescene.scene;
import wescene.text;
import wescene.script;

using namespace rstd::prelude;
using namespace rstd::literals;
using rstd::collections::HashMap;
using rstd::collections::HashSet;
using rstd::slice_::sort_by_key;
using rstd::slice_::sort_unstable_by;
using rstd::sync::Arc;
using namespace owe;
using namespace Eigen;

namespace
{

void ParseCamera(SceneParseContext& context, const wpscene::SceneMetadata& sc) {
    auto& scene   = *context.scene;
    auto& general = sc.general;
    // effect camera
    auto effect_camera = Arc<SceneCamera>::make(SceneCamera::MakeOrthographic(2, 2, -1.0, 1.0));
    context.effect_camera_node = Some(Arc<SceneNode>::make()); // at 0,0,0
    effect_camera->AttatchNode((*context.effect_camera_node).as_ptr());
    scene.RegisterCamera("effect"_Str, rstd::move(effect_camera));
    scene.RootMut()->AppendChild((*context.effect_camera_node).clone());

    // global camera
    auto     projection_extent = scene.OrthographicProjectionExtent();
    auto     global_camera     = Arc<SceneCamera>::make(SceneCamera::MakeOrthographic(
        projection_extent[usize()], projection_extent[usize(1)], -5000.0, 5000.0));
    Vector3f cori { rstd::as_cast<float>(context.ortho_w) / 2.0f,
                    rstd::as_cast<float>(context.ortho_h) / 2.0f,
                    0 },
        cscale { 1.0f, 1.0f, 1.0f }, cangle(Vector3f::Zero());

    context.global_camera_node = Some(Arc<SceneNode>::make(cori, cscale, cangle));
    global_camera->AttatchNode((*context.global_camera_node).as_ptr());
    scene.RegisterCamera("global"_Str, rstd::move(global_camera));
    (void)scene.SetActiveCamera("global"_str);
    scene.RootMut()->AppendChild((*context.global_camera_node).clone());

    const bool   override_perspective_fov = general.perspectiveoverridefov > 0.0f;
    const double perspective_fov =
        override_perspective_fov
            ? static_cast<double>(general.perspectiveoverridefov)
            : algorism::CalculatePersperctiveFov(1000.0, projection_extent[usize(1)]);
    const double perspective_distance =
        override_perspective_fov
            ? algorism::CalculatePersperctiveDistance(perspective_fov, projection_extent[usize(1)])
            : 1000.0;
    // WE's perspective camera inside an orthographic scene uses reverse-Z over 5..15000.
    const double perspective_near   = general.isOrtho ? 15000.0 : general.nearz;
    const double perspective_far    = general.isOrtho ? 5.0 : general.farz;
    auto         perspective_camera = Arc<SceneCamera>::make(SceneCamera::MakePerspective(
        rstd::as_cast<double>(context.ortho_w) / rstd::as_cast<double>(context.ortho_h),
        perspective_near,
        perspective_far,
        perspective_fov));

    Vector3f cperori                       = cori;
    cperori[2]                             = static_cast<float>(perspective_distance);
    context.global_perspective_camera_node = Some(Arc<SceneNode>::make(cperori, cscale, cangle));
    perspective_camera->AttatchNode((*context.global_perspective_camera_node).as_ptr());
    if (override_perspective_fov && general.isOrtho) {
        const Vector3d eye { cperori.x(), cperori.y(), cperori.z() };
        const Vector3d center { cori.x(), cori.y(), 0.0 };
        perspective_camera->SetLookAt(eye, center, Vector3d::UnitY());
    }
    scene.RegisterCamera("global_perspective"_Str, perspective_camera.clone());
    scene.RootMut()->AppendChild((*context.global_perspective_camera_node).clone());

    // Perspective scene (orthogonalprojection==null). The content is authored
    // in WE world units around the origin and viewed by an explicit eye/center
    // camera, not the 2D pixel-space placement above. Drive global_perspective
    // from scene.camera + general.fov and make it the active camera so every
    // layer (and its composite) renders under the same world-space view.
    if (! general.isOrtho) {
        Vector3d eye { sc.camera.eye[usize(0)], sc.camera.eye[usize(1)], sc.camera.eye[usize(2)] };
        Vector3d center { sc.camera.center[usize(0)],
                          sc.camera.center[usize(1)],
                          sc.camera.center[usize(2)] };
        Vector3d up { sc.camera.up[usize(0)], sc.camera.up[usize(1)], sc.camera.up[usize(2)] };
        perspective_camera->SetLookAt(eye, center, up);
        perspective_camera->SetFov(
            general.perspectiveoverridefov > 0.0f ? general.perspectiveoverridefov : general.fov);
        perspective_camera->SetAspect(rstd::as_cast<double>(context.ortho_w) /
                                      rstd::as_cast<double>(context.ortho_h));
        (void)scene.SetActiveCamera("global_perspective"_str);
        LoadRootCameraPaths(context, sc);
    }
}

void ParseCameraObj(SceneParseContext& context, wpscene::CameraObject& cam) {
    PrepareAnimationBindings(context, cam);
    auto& scene           = *context.scene;
    bool  use_perspective = false;
    auto  perspective     = scene.Camera("global_perspective"_str);
    auto  active          = scene.ActiveCamera();
    if (perspective.is_some() && active.is_some() &&
        (*perspective).as_raw_ptr() == (*active).as_raw_ptr())
        use_perspective = true;

    ref<str> camera_name = use_perspective ? "global_perspective"_str : "global"_str;
    auto     camera      = scene.CameraHandle(camera_name);
    if (camera.is_none()) return;

    auto       camera_owner = rstd::move(*camera);
    SceneNode* default_node =
        use_perspective
            ? (context.global_perspective_camera_node.is_some()
                   ? (*context.global_perspective_camera_node).as_ptr()
                   : nullptr)
            : (context.global_camera_node.is_some() ? (*context.global_camera_node).as_ptr()
                                                    : nullptr);
    if (default_node == nullptr) {
        auto attached = camera_owner->GetAttachedNode();
        if (attached.is_some()) default_node = attached.unwrap();
    }
    if (default_node == nullptr) return;

    double   default_width     = camera_owner->Width();
    double   default_height    = camera_owner->Height();
    double   default_fov       = camera_owner->Fov();
    // A 3D scene's camera looks from the scene's eye at its center: hiding the layer gives it
    // that view back, not the place of the camera's node.
    const bool default_lookat     = camera_owner->IsLookAt();
    const auto default_transforms = camera_owner->Transforms();
    Vector3f default_translate = default_node->Translate();
    Vector3f default_rotation  = default_node->Rotation();
    Vector3f origin { cam.origin[usize(0)], cam.origin[usize(1)], cam.origin[usize(2)] };
    Vector3f angles { cam.angles[usize(0)], cam.angles[usize(1)], cam.angles[usize(2)] };
    Vector3f path_translate_bias = use_perspective ? Vector3f::Zero() : default_translate;
    Vector3f path_rotation_bias  = use_perspective ? Vector3f::Zero() : default_rotation;

    auto node  = Arc<SceneNode>::make(path_translate_bias + origin,
                                      Vector3f::Ones(),
                                      path_rotation_bias + angles,
                                      cam.name.as_str());
    node->ID() = i32(cam.id);
    if (! cam.visible) node->SetVisible(false);
    if (! cam.visible_user.empty())
        node->SetVisibleUserBinding(ToSceneUserVisibilityBinding(cam.visible_user));

    if (cam.visible) camera_owner->AttatchNode(node.as_ptr());
    if (use_perspective) {
        // the layer's field of view is the camera's only while the layer drives it (its path
        // sets it): until then the scene's stays
        camera_owner->SetAspect(rstd::as_cast<double>(context.ortho_w) /
                                rstd::as_cast<double>(context.ortho_h));
        (void)scene.SetActiveCamera(camera_name);
    }

    auto path                 = Arc<SceneCameraPath>::make();
    path->camera_name         = String::make(camera_name);
    path->camera              = Some(camera_owner.clone());
    path->node                = node.as_ptr();
    path->default_translate   = default_translate;
    path->default_rotation    = default_rotation;
    path->path_translate_bias = path_translate_bias;
    path->path_rotation_bias  = path_rotation_bias;
    path->default_width       = default_width;
    path->default_height      = default_height;
    path->default_fov         = default_fov;
    path->default_lookat      = default_lookat;
    path->default_eye         = default_transforms.eye.cast<float>();
    path->default_center      = default_transforms.center.cast<float>();
    path->default_up          = default_transforms.up.cast<float>();
    path->origin_base         = origin;
    path->rotation_base       = angles;
    path->zoom_base           = cam.zoom;
    path->fov_base            = cam.fov;
    if (use_perspective && cam.fov > 0.0f) path->own_fov = Some(cam.fov);
    path->perspective         = use_perspective;
    path->enabled             = cam.visible;
    if (! cam.visible_user.empty())
        path->visible_user_binding = ToSceneUserVisibilityBinding(cam.visible_user);
    AssignCameraFieldAnimations(context, *node, *path, cam.field_bindings);
    LoadCameraObjectPath(context, cam, *path);
    scene.RegisterCameraPath(path.clone());
    if (! cam.visible_user_key.is_empty()) {
        scene.RegisterCameraPathUserBinding(cam.visible_user_key.clone(), path.clone());
    }

    WireCameraFieldScripts(context,
                           node,
                           camera_owner,
                           path,
                           cam.field_bindings,
                           path_translate_bias,
                           path_rotation_bias);
    RegisterNodeRef(context, cam.id, SceneParseContext::NodeRef { cam.parent, Some(node.clone()) });
}

void InitContext(SceneParseContext& context, fs::VFS& vfs, const wpscene::SceneMetadata& sc,
                 array<i32, 2> ortho_extent, Option<ImageReadyCallback> image_ready,
                 TextureMemoryOptions texture_memory) {
    context.vfs = &vfs;
    auto& scene = *context.scene;
    scene.SetImageParser(Arc<dyn<IImageParser>>::make(
        TexImageParser(&vfs, rstd::move(image_ready), rstd::move(texture_memory))));
    context.particle_runtime = Some(Arc<ParticleRuntime>::make());
    GenCardMesh(*scene.DefaultEffectMeshMut(), { 2.0f, 2.0f });

    scene.SetClearColor(sc.general.clearcolor);
    if (auto key = sc.general.user_bindings.get("clearcolor"_str); key.is_some()) {
        scene.SetClearColorUserKey((**key).clone());
    }
    scene.SetOrtho({ i32(ortho_extent[usize()]), i32(ortho_extent[usize(1)]) });
    scene.SetViewportScale(f32(sc.general.zoom));
    context.ortho_w            = ortho_extent[usize()];
    context.ortho_h            = ortho_extent[usize(1)];
    context.orthographic_scene = sc.general.isOrtho;
    context.uniform_state->SetOrthographicImplicitParallax(sc.general.isOrtho);
    context.uniform_state->SetAmbientColor(sc.general.ambientcolor);
    context.uniform_state->SetSkylightColor(sc.general.skylightcolor);

    {
        auto& gb = context.global_base_uniforms;
        (void)gb.insert(rstd::into(G_VIEWUP), ShaderValue(array<float, 3> { 0.0f, 1.0f, 0.0f }));
        (void)gb.insert(rstd::into(G_VIEWRIGHT), ShaderValue(array<float, 3> { 1.0f, 0.0f, 0.0f }));
        (void)gb.insert(rstd::into(G_VIEWFORWARD),
                        ShaderValue(array<float, 3> { 0.0f, 0.0f, -1.0f }));
        (void)gb.insert(rstd::into(G_EYEPOSITION),
                        ShaderValue(array<float, 3> { 0.0f, 0.0f, 0.0f }));
        (void)gb.insert(rstd::into(G_TEXELSIZE),
                        ShaderValue(array<float, 2> { 1.0f / 1920.0f, 1.0f / 1080.0f }));
        (void)gb.insert(
            rstd::into(G_TEXELSIZEHALF),
            ShaderValue(array<float, 2> { 1.0f / 1920.0f / 2.0f, 1.0f / 1080.0f / 2.0f }));
        (void)gb.insert(rstd::into(G_LIGHTAMBIENTCOLOR), ShaderValue(sc.general.ambientcolor));
        (void)gb.insert(rstd::into(G_LIGHTSKYLIGHTCOLOR), ShaderValue(sc.general.skylightcolor));

        if (sc.general.fogdistance) {
            context.shader_environment.fog_distance = true;
            (void)gb.insert(rstd::into(G_FOGDISTANCECOLOR),
                            ShaderValue(sc.general.fogdistancecolor));
            (void)gb.insert(
                rstd::into(G_FOGDISTANCEPARAMS),
                ShaderValue(array<float, 4> {
                    sc.general.fogdistancestart,
                    sc.general.fogdistanceend - sc.general.fogdistancestart,
                    sc.general.fogdistancestartdensity,
                    sc.general.fogdistanceenddensity - sc.general.fogdistancestartdensity,
                }));
        }
        if (sc.general.fogheight) {
            context.shader_environment.fog_height = true;
            (void)gb.insert(rstd::into(G_FOGHEIGHTCOLOR), ShaderValue(sc.general.fogheightcolor));
            (void)gb.insert(rstd::into(G_FOGHEIGHTPARAMS),
                            ShaderValue(array<float, 4> {
                                sc.general.fogheightstart,
                                sc.general.fogheightend - sc.general.fogheightstart,
                                sc.general.fogheightstartdensity,
                                sc.general.fogheightenddensity - sc.general.fogheightstartdensity,
                            }));
        }
    }

    {
        UniformCameraParallax cam_para;
        cam_para.enable                         = sc.general.cameraparallax;
        cam_para.amount                         = sc.general.cameraparallaxamount;
        cam_para.delay                          = sc.general.cameraparallaxdelay;
        cam_para.mouse_influence                = sc.general.cameraparallaxmouseinfluence;
        context.uniform_state->CameraParallax() = cam_para;
        sc.general.user_bindings.iter().for_each([&](auto entry) {
            auto [field, key] = entry;
            if (field->as_str() == "cameraparallax"_str ||
                field->as_str() == "cameraparallaxamount"_str ||
                field->as_str() == "cameraparallaxdelay"_str ||
                field->as_str() == "cameraparallaxmouseinfluence"_str) {
                auto state =
                    mut_ref<UniformSceneState>::from_raw_parts(context.uniform_state.as_ptr());
                scene.RegisterUserPropertyBinding(
                    key->clone(),
                    Box<dyn<FnMut<void(ref<Json>)>>>::make(
                        [state, field = field->clone()](ref<Json> property) mutable {
                            state->ApplyUserProperty(field.as_str(), *property);
                        }));
            }
        });
    }
    {
        UniformCameraShake cam_shake;
        cam_shake.enable                     = sc.general.camerashake;
        cam_shake.amplitude                  = sc.general.camerashakeamplitude;
        cam_shake.speed                      = sc.general.camerashakespeed;
        cam_shake.roughness                  = sc.general.camerashakeroughness;
        context.uniform_state->CameraShake() = cam_shake;
        sc.general.user_bindings.iter().for_each([&](auto entry) {
            auto [field, key] = entry;
            if (field->as_str() == "camerashake"_str ||
                field->as_str() == "camerashakeamplitude"_str ||
                field->as_str() == "camerashakespeed"_str ||
                field->as_str() == "camerashakeroughness"_str) {
                auto state =
                    mut_ref<UniformSceneState>::from_raw_parts(context.uniform_state.as_ptr());
                scene.RegisterUserPropertyBinding(
                    key->clone(),
                    Box<dyn<FnMut<void(ref<Json>)>>>::make(
                        [state, field = field->clone()](ref<Json> property) mutable {
                            state->ApplyUserProperty(field.as_str(), *property);
                        }));
            }
        });
        WireGeneralFieldScripts(context, sc.general.field_bindings);
    }
}

void ParseSoundObjImpl(SceneParseContext& context, wpscene::SoundObject& obj,
                       wavsen::audio::SoundManager& sm) {
    auto node  = Arc<SceneNode>::make(Vector3f(obj.origin.data()),
                                      Vector3f(obj.scale.data()),
                                      Vector3f(obj.angles.data()),
                                      obj.name.as_str());
    node->ID() = i32(obj.id);
    if (! obj.visible) node->SetVisible(false);
    if (! obj.visible_user.empty())
        node->SetVisibleUserBinding(ToSceneUserVisibilityBinding(obj.visible_user));

    auto control = SoundParser::Parse(obj, *context.vfs, sm, context.scene.get());
    if (! obj.volume_user_key.is_empty()) {
        context.scene->RegisterSoundVolumeBinding(obj.volume_user_key.as_str(), control.clone());
    }
    node->SetSoundControl(rstd::move(control));
    node->SetVolume(obj.volume);

    AssignNodeFieldAnimations(context, *node.as_ptr(), obj.field_bindings);
    WireFieldScripts(context, node, obj.field_bindings);
    RegisterNodeRef(context, obj.id, SceneParseContext::NodeRef { obj.parent, Some(node.clone()) });
}

void ParseLightObj(SceneParseContext& context, wpscene::LightObject& light_obj) {
    PrepareAnimationBindings(context, light_obj);
    auto node = Arc<SceneNode>::make(Vector3f(light_obj.origin.data()),
                                     Vector3f(light_obj.scale.data()),
                                     Vector3f(light_obj.angles.data()),
                                     light_obj.name.as_str());

    SceneLight::Desc desc;
    if (light_obj.light == "spot"_str || light_obj.light == "lspot"_str) {
        desc.type = SceneLightType::Spot;
    } else if (light_obj.light == "directional"_str || light_obj.light == "ldirectional"_str) {
        desc.type = SceneLightType::Directional;
    } else if (light_obj.light == "tube"_str || light_obj.light == "ltube"_str) {
        desc.type = SceneLightType::Tube;
    } else {
        desc.type = SceneLightType::Point; // default + point/lpoint
    }
    desc.old_kind    = light_obj.light == "point"_str;
    desc.cookie      = context.light_cookie_ids.contains(i32(light_obj.id));
    desc.color       = Vector3f(light_obj.color.data());
    desc.radius      = light_obj.radius;
    desc.intensity   = light_obj.intensity;
    desc.exponent    = light_obj.exponent;
    desc.attenuation = light_obj.attenuation;
    desc.mindistance = light_obj.mindistance;
    // WE evaluates spot cones against cos(full angle), as stored by the official renderer.
    const float kDegToRad     = f32::consts::PI.to_primitive() / 180.0f;
    desc.inner_cone_cos       = f32(light_obj.innercone * kDegToRad).cos().to_primitive();
    desc.outer_cone_cos       = f32(light_obj.outercone * kDegToRad).cos().to_primitive();
    desc.light_source_size    = light_obj.lightsourcesize;
    desc.cascade_distances[0] = light_obj.cascadedistance0;
    desc.cascade_distances[1] = light_obj.cascadedistance1;
    desc.cascade_distances[2] = light_obj.cascadedistance2;
    desc.cast_shadow          = light_obj.castshadow;
    desc.cast_volumetrics     = light_obj.castvolumetrics;
    if (auto view = context.shadow_views.get(light_obj.id); view.is_some())
        desc.shadow_view = **view;

    auto light = context.scene->RegisterLight(Box<SceneLight>::make(desc));
    light->setNode(node.as_ptr());
    light->setRuntimeVisible(light_obj.visible);
    node->SetBaseColor(desc.color, 1.0f);
    if (! light_obj.visible_user.empty()) {
        light->setVisibleUserBinding(ToSceneUserVisibilityBinding(light_obj.visible_user));
    }

    node->SetAnimationsFollowRelative(true);
    AssignNodeFieldAnimations(context, *node.as_ptr(), light_obj.field_bindings);
    WireFieldScripts(context, node, light_obj.field_bindings);
    RegisterNodeRef(
        context, light_obj.id, SceneParseContext::NodeRef { light_obj.parent, Some(node.clone()) });
}

void ParseModelObjImpl(SceneParseContext& context, wpscene::ModelObject& model_obj) {
    auto& vfs = *context.vfs;

    Mdl  mdl;
    auto read_span = SceneLoadSpan(context.load_bench, &SceneLoadProbeIds::model_read);
    if (! MdlParser::Parse(model_obj.model.as_str(), vfs, mdl)) {
        rstd_error("parse model failed: {}", model_obj.model);
        return;
    }
    (void)read_span.finish();

    auto node  = Arc<SceneNode>::make(Vector3f(model_obj.origin.data()),
                                      Vector3f(model_obj.scale.data()),
                                      Vector3f(model_obj.angles.data()),
                                      model_obj.name.as_str());
    node->ID() = model_obj.id;
    node->SetDrawsAsModel(true);
    node->SetPerspective(model_obj.perspective);
    node->SetReflected(model_obj.reflected);
    node->shadow.cast = context.shader_environment.shadow_mapping && model_obj.castshadow;
    if (! model_obj.visible) {
        node->SetVisible(false);
        context.scene->MarkLayerVisibilityElidable(WallpaperLayerId { .value = model_obj.id });
    }
    MarkHiddenLinkSource(context, model_obj.id);
    if (! model_obj.visible_user.empty())
        node->SetVisibleUserBinding(ToSceneUserVisibilityBinding(model_obj.visible_user));

    auto mesh = Arc<SceneMesh>::make();

    UniformNodeConfigDraft svData;
    svData.SetParallaxContract(model_obj.parallax, model_obj.id);
    svData.use_camera_eye_position = true;
    if (context.orthographic_scene) {
        svData.eye_position_override = Some(array<float, 3> {
            static_cast<float>(context.ortho_w.to_primitive()) * 0.5f,
            static_cast<float>(context.ortho_h.to_primitive()) * 0.5f,
            2000.0f,
        });
    }
    Option<Arc<PuppetLayer>> model_puppet_layer;
    if (mdl.puppet.is_some() && ! (*mdl.puppet)->bones.is_empty()) {
        model_puppet_layer =
            Some(MakePuppetLayer((*mdl.puppet).clone(), model_obj.puppet_layers.as_slice()));
        RegisterPuppetLayer(context, node.as_ptr(), (*model_puppet_layer).clone());
    }

    for (const auto& mdl_mesh : mdl.meshes) {
        if (mdl_mesh.positions.is_empty()) continue;

        if (mdl_mesh.mat_json_files.is_empty()) continue;
        auto skin_index = rstd::as_cast<usize>(model_obj.skin);
        if (skin_index >= mdl_mesh.mat_json_files.len()) {
            rstd_error("model '{}' skin {} exceeds {} material variants; using skin 0",
                       model_obj.name,
                       model_obj.skin,
                       mdl_mesh.mat_json_files.len());
            skin_index = usize();
        }
        const auto& material_ref = mdl_mesh.mat_json_files[skin_index];

        auto wpmat = MdlParser::ParseMaterial(material_ref, vfs);
        if (! wpmat) continue;
        if (mdl.puppet.is_some() && ! (*mdl.puppet)->bones.is_empty()) {
            MdlParser::AddPuppetMatInfo(*wpmat, mdl);
        }

        SceneMaterial scene_mat;
        ShaderInfo    shader_info;
        shader_info.baseConstSvs = context.global_base_uniforms.clone();
        if (mdl.puppet.is_some() && ! (*mdl.puppet)->bones.is_empty()) {
            MdlParser::AddPuppetShaderInfo(shader_info, mdl);
        }

        auto material_result = BuildMaterial(vfs,
                                             *context.shader_cache,
                                             context.shader_environment,
                                             *wpmat,
                                             *context.scene,
                                             rstd::move(shader_info),
                                             GeometryStageRequirement::None,
                                             context.load_bench);
        if (material_result.is_err()) {
            rstd_error("load model material '{}' failed for '{}'", material_ref, model_obj.name);
            continue;
        }
        auto material_build = rstd::move(material_result).unwrap_unchecked();
        scene_mat           = rstd::move(material_build.material);
        // With the scene's transparent sorting, translucent models draw far to near and write
        // depth as their material says (materials otherwise write none when they blend).
        if (context.scene->TransparentSorting() && wpmat->blending == "translucent"_str &&
            wpmat->depthwrite == "enabled"_str)
            (void)scene_mat.SetDepthWrite(true);
        shader_info         = rstd::move(material_build.shader_info);
        LoadConstvalue(context, scene_mat, *wpmat, shader_info);

        if (node->shadow.cast) {
            wpscene::Material shadow_material;
            shadow_material.shader = shader_info.shadow_pass.is_empty()
                                         ? "shadowcaster"_Str
                                         : shader_info.shadow_pass.clone();
            shadow_material.blending =
                wpmat->blending == "alphatocoverage"_str ? "alphatocoverage"_Str : "disabled"_Str;
            shadow_material.depthtest  = "enabled"_Str;
            shadow_material.depthwrite = "enabled"_Str;
            shadow_material.cullmode   = "nocull"_Str;
            shadow_material.textures.resize(rstd::cmp::min(usize(2), wpmat->textures.len()),
                                            String {});
            for (usize index {}; index < shadow_material.textures.len(); ++index) {
                shadow_material.textures[index] = wpmat->textures[index].clone();
            }
            constexpr array<ref<str>, 4> inherited_combos {
                "SKINNING"_str, "MORPHING"_str, "MORPHING_NORMALS"_str, "BONECOUNT"_str
            };
            for (auto combo : inherited_combos) {
                auto value = wpmat->combos.get(combo);
                if (value.is_some())
                    (void)shadow_material.combos.insert(rstd::into(combo), **value);
            }

            // one caster for each shadowed light, drawn with that light's views
            for (const auto& definition : context.scene->ShadowDefinitions()) {
                auto light_material = shadow_material.clone();
                (void)light_material.combos.insert("OWE_SHADOW_VIEW_OFFSET"_Str,
                                                   rstd::as_cast<i32>(definition.view_offset));
                ShaderInfo shadow_info;
                shadow_info.baseConstSvs = context.global_base_uniforms.clone();
                auto shadow_result       = BuildMaterial(vfs,
                                                         *context.shader_cache,
                                                         context.shader_environment,
                                                         light_material,
                                                         *context.scene,
                                                         rstd::move(shadow_info),
                                                         GeometryStageRequirement::None,
                                                         context.load_bench);
                if (shadow_result.is_err()) {
                    rstd_warn("load shadow material '{}' failed for '{}'",
                              light_material.shader,
                              model_obj.name);
                    break;
                }
                auto shadow_build = rstd::move(shadow_result).unwrap_unchecked();
                shadow_build.material.SetPipeline(
                    ShadowCasterPipeline(shadow_build.material.Pipeline()));
                LoadConstvalue(
                    context, shadow_build.material, light_material, shadow_build.shader_info);
                context.scene->ResolveMaterialTextureSources(shadow_build.material);
                scene_mat.shadow_variants.push(
                    Arc<SceneMaterial>::make(rstd::move(shadow_build.material)));
            }
        }

        const auto material_slot =
            rstd::as_cast<u32>(usize(mesh->MaterialSlots().len().to_primitive()));
        const auto texcoord_scale = Texture0UvScale(scene_mat);
        mesh->AddMaterial(rstd::move(scene_mat));
        RegisterMaterialBindings(*context.scene,
                                 mesh->MaterialSlots().last_mut().unwrap().get_mut(),
                                 *wpmat,
                                 shader_info);
        WireMaterialShaderValueScripts(context,
                                       node,
                                       mesh->MaterialSlots().last_mut().unwrap().get_mut(),
                                       *wpmat,
                                       shader_info);

        mesh->Submeshes().emplace_back();
        auto& submesh = mesh->Submeshes().last_mut().unwrap().get_mut();
        {
            auto span = SceneLoadSpan(context.load_bench, &SceneLoadProbeIds::model_mesh);
            MdlParser::GenMeshFromMdl(
                submesh, mdl_mesh, { texcoord_scale[usize()], texcoord_scale[usize(1)] });
        }
        submesh.material_slot = material_slot;
        if (model_puppet_layer.is_some())
            MdlParser::BindDrawOrder(submesh, mdl_mesh, (*model_puppet_layer).clone());
    }

    if (mesh->Submeshes().is_empty()) {
        rstd_error("model '{}' has no renderable mesh", model_obj.model);
        return;
    }

    // Blended model surfaces must follow opaque depth-writing geometry.
    sort_by_key(
        mesh->Submeshes().as_mut_slice().as_mut_ref(), [&](const SceneMesh::Submesh& submesh) {
            const auto blend = mesh->MaterialSlots()[rstd::as_cast<usize>(submesh.material_slot)]
                                   ->Pipeline()
                                   .blend_mode;
            return u8(blend == BlendMode::Translucent || blend == BlendMode::Additive);
        });
    node->AddMesh(mesh.clone());
    SetUniformConfig(context, node, rstd::move(svData));
    AssignNodeFieldAnimations(context, *node.as_ptr(), model_obj.field_bindings);
    WireFieldScripts(context, node, model_obj.field_bindings);
    if (model_puppet_layer.is_some())
        WirePuppetAnimationScripts(context,
                                   node.as_ptr(),
                                   (*model_puppet_layer).clone(),
                                   model_obj.puppet_layers.as_slice());
    if (model_obj.skin == u32()) {
        (void)context.dynamic_model_prototypes.insert(model_obj.model.clone(), node.clone());
    }
    RegisterNodeRef(
        context,
        model_obj.id,
        SceneParseContext::NodeRef {
            model_obj.parent,
            Some(node.clone()),
            mdl.puppet.is_some() ? Some((*mdl.puppet).clone()) : None(),
            model_obj.attachment.clone(),
            model_puppet_layer.is_some() ? Some((*model_puppet_layer).clone()) : None() });
}

} // namespace

namespace owe
{

void ParseSoundObj(SceneParseContext& context, wpscene::SoundObject& sound,
                   wavsen::audio::SoundManager& manager) {
    PrepareAnimationBindings(context, sound);
    ParseSoundObjImpl(context, sound, manager);
}

void ParseModelObj(SceneParseContext& context, wpscene::ModelObject& model) {
    PrepareAnimationBindings(context, model);
    ParseModelObjImpl(context, model);
}

void IndexSceneDocument(SceneParseContext& context, ref<wpscene::SceneDocument> document,
                        slice<SceneObjectVar> objects) {
    context.scene_has_scripts       = SceneHasScripts(objects);
    context.scene_layer_text_writes = SceneWritesLayerText(objects);
    HashSet<String> layer_names;
    // Cookie lights are known before any material is built: their texture and LIGHTS_COOKIE
    // reach every lit material.
    auto           cookie_slots = document->metadata.general.lightconfig.spotcookie;
    Option<String> cookie_texture;
    for (const auto& object : objects) {
        if (! object.is_Light() || cookie_slots == u32()) continue;
        const auto& light = object.as_Light().value;
        if (! light.usecookie || (light.light != "spot"_str && light.light != "lspot"_str))
            continue;
        (void)context.light_cookie_ids.insert(i32(light.id));
        cookie_texture = Some(light.cookie.clone());
        cookie_slots   = cookie_slots - u32(1);
    }
    context.scene->SetLightCookieTexture(rstd::move(cookie_texture));
    for (const auto& record : document->objects) {
        const auto& metadata = record.metadata;
        if (metadata.kind == wpscene::SceneObjectKind::Unknown || ! metadata.has_id) continue;
        (void)layer_names.insert(metadata.name.clone());
        (void)context.initial_layer_configs.insert(metadata.id, record.authored.clone());
        (void)context.script_initialization_orders.insert(
            metadata.id, static_cast<rstd::uint64_t>(context.node_id_order.len().to_primitive()));
        context.node_id_order.emplace_back(metadata.id);
        (void)context.object_parent_ids.insert(metadata.id, metadata.parent);
        if (metadata.solid) context.solid_layer_ids.insert(i32(metadata.id));
    }
    // Scripts of the general settings may have started the runtime already.
    if (context.script_scene.is_some())
        (*context.script_scene)->runtime().SetAuthoredLayerNames(layer_names.clone());
    context.authored_layer_names = Some(rstd::move(layer_names));
}

SceneParseContext
BuildContext(fs::VFS& vfs, ref<str> scene_id, const wpscene::SceneMetadata& sc,
             array<i32, 2> ortho_extent, Option<ref<rstd::json::Map>> user_properties,
             Option<rstd::path::PathBuf> shader_cache_dir, GeometryShaderLimits geometry_limits,
             slice<SceneShadowLight> shadow_lights, ShadowQuality shadow_quality,
             bool geometry_shader_supported,
             Option<ImageReadyCallback> image_ready, TextureMemoryOptions texture_memory) {
    SceneParseContext context;
    PrepareAnimationBindings(context, sc.general.field_bindings);
    InitContext(
        context, vfs, sc, ortho_extent, rstd::move(image_ready), rstd::move(texture_memory));
    ParseCamera(context, sc);
    context.pkg_version               = sc.pkg_version;
    context.scene->SetTransparentSorting(sc.general.transparentsorting);
    context.user_properties           = user_properties;
    context.shader_cache              = Arc<ShaderCache>::make(rstd::move(shader_cache_dir));
    context.geometry_shader_limits    = geometry_limits;
    context.geometry_shader_supported = geometry_shader_supported;
    context.shader_environment.shadow_mapping     = ! shadow_lights.is_empty();
    context.shader_environment.point_lights       = sc.general.lightconfig.point;
    context.shader_environment.spot_lights        = sc.general.lightconfig.spot;
    context.shader_environment.directional_lights = sc.general.lightconfig.directional;
    context.shader_environment.tube_lights        = sc.general.lightconfig.tube;
    context.shader_environment.shadow_quality     = shadow_quality;

    context.scene->RegisterRenderTarget(String::make(SpecTex_Default),
                                        SceneRenderTarget {
                                            .width             = context.ortho_w,
                                            .height            = context.ortho_h,
                                            .withDepth         = true,
                                            .bind              = { .enable = true, .screen = true },
                                            .preserve_on_write = true,
                                        });
    context.scene->RegisterRenderTarget(
        String::make(WE_MIP_MAPPED_FRAME_BUFFER),
        SceneRenderTarget {
            .width                  = context.ortho_w,
            .height                 = context.ortho_h,
            .has_mipmap             = true,
            .bind                   = { .enable = true, .name = rstd::into(SpecTex_Default) },
            .initialize_transparent = true,
        });

    if (context.shader_environment.shadow_mapping) {
        // a square of the atlas for each spot light, each cascade and each point light, in rows
        // of a near square: the spots' first, the points' last
        const int tile = ShadowTileSize(shadow_quality);
        const auto    squares_of = [](SceneLightType type) {
            return type == SceneLightType::Directional ? 3 : 1;
        };
        int squares {};
        for (const auto& light : shadow_lights) squares += squares_of(light.type);
        const int columns =
            static_cast<int>(f32(static_cast<float>(squares)).sqrt().ceil().to_primitive());
        const int rows = (squares + columns - 1) / columns;
        context.scene->RegisterRenderTarget(
            String::make(WE_SHADOW_ATLAS_PREFIX),
            SceneRenderTarget {
                .width             = i32(columns * tile),
                .height            = i32(rows * tile),
                .kind              = SceneRenderTargetKind::DepthSampled,
                .depth_clear_value = 0.0f,
                .sample =
                    TextureSample {
                        .wrapS          = TextureWrap::CLAMP_TO_BORDER,
                        .wrapT          = TextureWrap::CLAMP_TO_BORDER,
                        .magFilter      = TextureFilter::LINEAR,
                        .minFilter      = TextureFilter::LINEAR,
                        .compare_enable = true,
                        .compare_op     = CompareOp::Greater,
                        .border_color   = TextureBorderColor::TransparentBlack,
                    },
            });
        struct Corner {
            int x;
            int y;
        };
        int        next_square {};
        const auto place = [&] {
            const Corner corner { (next_square % columns) * tile, (next_square / columns) * tile };
            ++next_square;
            return corner;
        };
        // a region of the atlas, drawn upside down as the lookups read it
        const auto viewport = [](int x, int y, int width, int height) {
            return SceneShadowViewport {
                .x              = f32(static_cast<float>(x)),
                .y              = f32(static_cast<float>(y + height)),
                .width          = f32(static_cast<float>(width)),
                .height         = f32(-static_cast<float>(height)),
                .scissor_x      = i32(x),
                .scissor_y      = i32(y),
                .scissor_width  = u32(static_cast<unsigned>(width)),
                .scissor_height = u32(static_cast<unsigned>(height)),
            };
        };
        int        next_view {};
        const auto by_kind = [&](SceneLightType kind) {
            auto lights = Vec<SceneShadowLight>::make();
            for (const auto& light : shadow_lights)
                if (light.type == kind) lights.push(SceneShadowLight(light));
            return lights;
        };
        auto ordered = by_kind(SceneLightType::Spot);
        for (auto& light : by_kind(SceneLightType::Directional)) ordered.push(rstd::move(light));
        for (auto& light : by_kind(SceneLightType::Point)) ordered.push(rstd::move(light));
        for (const auto& light : ordered) {
            auto viewports = Vec<SceneShadowViewport>::make();
            if (light.type == SceneLightType::Spot) {
                const auto [x, y] = place();
                viewports.push(viewport(x, y, tile, tile));
            } else if (light.type == SceneLightType::Directional) {
                for (int cascade = 0; cascade < 3; ++cascade) {
                    const auto [x, y] = place();
                    viewports.push(viewport(x, y, tile, tile));
                }
            } else {
                // +x -x, +y -y, +z -z in two columns and three rows
                const auto [x, y] = place();
                const int width   = tile / 2;
                const int height  = tile / 3;
                for (int face = 0; face < 6; ++face)
                    viewports.push(
                        viewport(x + (face % 2) * width, y + (face / 2) * height, width, height));
            }
            const int views = static_cast<int>(viewports.len().to_primitive());
            if (views == 0 || next_view + views > static_cast<int>(kShadowViewCount.to_primitive()))
                continue;
            (void)context.shadow_views.insert(light.id, i32(next_view));
            context.scene->RegisterShadowDefinition(SceneShadowDefinition {
                .target      = String::make(WE_SHADOW_ATLAS_PREFIX),
                .viewports   = rstd::move(viewports),
                .light_id    = light.id,
                .view_offset = u32(static_cast<unsigned>(next_view)),
                .type        = light.type,
            });
            next_view += views;
        }
    }

    context.scene->SetSceneId(String::make(scene_id));
    return context;
}

void ParseContainerObj(SceneParseContext& context, const wpscene::ContainerObject& obj) {
    PrepareAnimationBindings(context, obj);
    auto node  = Arc<SceneNode>::make(Vector3f(obj.origin.data()),
                                      Vector3f(obj.scale.data()),
                                      Vector3f(obj.angles.data()),
                                      obj.name.as_str());
    node->ID() = i32(obj.id);
    if (obj.parallax.authored || obj.disable_propagation ||
        ! wpscene::IsZeroParallaxDepth(obj.parallax.depth)) {
        ApplyParallaxUniformConfig(context, node, obj.parallax, obj.id, ! obj.disable_propagation);
    }
    if (! obj.visible) (void)context.scene->SetNodeVisible(*node, false);
    if (! obj.visible_user.empty())
        node->SetVisibleUserBinding(ToSceneUserVisibilityBinding(obj.visible_user));
    AssignNodeFieldAnimations(context, *node, obj.field_bindings);
    WireFieldScripts(context, node, obj.field_bindings);
    RegisterNodeRef(context,
                    obj.id,
                    SceneParseContext::NodeRef {
                        obj.parent,
                        Some(node.clone()),
                        None(),
                        obj.attachment.clone(),
                        None(),
                    });
}

void ProcessContainers(SceneParseContext& context, mut_ref<SceneObjectVar[]> scene_objs) {
    for (usize index {}; index < scene_objs.len(); ++index) {
        auto& object = scene_objs[index];
        if (object.is_Container()) ParseContainerObj(context, object.as_Container().value);
    }
}

void ProcessObjects(SceneParseContext& context, mut_ref<SceneObjectVar[]> scene_objs,
                    wavsen::audio::SoundManager* sm, ProcessOpts opts,
                    SceneLoadBenchRecorderView load_bench) {
    context.sound_manager = sm;
    IndexSystemMediaImageFallbacks(context, scene_objs.as_ref());

    for (usize index {}; index < scene_objs.len(); ++index) {
        auto& object = scene_objs[index];
        RSTD_MATCH(object) {
            RSTD_CASE(Container) { continue; }
            RSTD_CASE(Image, value) {
                if (! (opts.kinds & ProcessOpts::Image)) continue;
                auto span = SceneLoadSpan(load_bench, &SceneLoadProbeIds::parse_object_image);
                ParseImageObj(context, value);
            }
            RSTD_CASE(Shape, value) {
                if (! (opts.kinds & ProcessOpts::Image)) continue;
                auto span = SceneLoadSpan(load_bench, &SceneLoadProbeIds::parse_object_image);
                ParseShapeObj(context, value);
            }
            RSTD_CASE(Particle, value) {
                if (! (opts.kinds & ProcessOpts::Particle)) continue;
                auto span = SceneLoadSpan(load_bench, &SceneLoadProbeIds::parse_object_particle);
                ParseParticleObj(context, value);
            }
            RSTD_CASE(Sound, value) {
                if (! (opts.kinds & ProcessOpts::Sound) || ! sm) continue;
                auto span = SceneLoadSpan(load_bench, &SceneLoadProbeIds::parse_object_sound);
                ParseSoundObj(context, value, *sm);
            }
            RSTD_CASE(Light, value) {
                if (! (opts.kinds & ProcessOpts::Light)) continue;
                auto span = SceneLoadSpan(load_bench, &SceneLoadProbeIds::parse_object_light);
                ParseLightObj(context, value);
            }
            RSTD_CASE(Text, value) {
                if (! (opts.kinds & ProcessOpts::Text)) continue;
                auto span = SceneLoadSpan(load_bench, &SceneLoadProbeIds::parse_object_text);
                ParseTextObj(context, value);
            }
            RSTD_CASE(Model, value) {
                if (! (opts.kinds & ProcessOpts::Model)) continue;
                auto span = SceneLoadSpan(load_bench, &SceneLoadProbeIds::parse_object_model);
                ParseModelObj(context, value);
            }
            RSTD_CASE(Camera, value) {
                auto span = SceneLoadSpan(load_bench, &SceneLoadProbeIds::parse_object_camera);
                ParseCameraObj(context, value);
            }
        }
    }

    ResolveRegisteredAssets(context);
}

} // namespace owe
