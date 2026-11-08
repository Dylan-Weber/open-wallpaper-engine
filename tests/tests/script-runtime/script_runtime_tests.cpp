

// Wallpaper Engine gives a property script its own copy of the value in each
// call; only what the script returns changes the property.
TEST(ScriptValueCoercion, InitChangesWithoutReturnLeaveTheValue) {
    auto root = Arc<owe::SceneNode>::make();

    JsRuntime rt;
    auto*     script = rt.MakeFieldScript(
        R"JS(
            export function init(value) { value.x = 900; }
            export function update(value) { return value; }
        )JS"_str,
        "test/init_without_return"_str,
        FieldKind::Vec3,
        owe::MakeObject(),
        owe::IntoJson("300 200 0"));
    ASSERT_NE(script, nullptr);

    rt.SetSceneRoot(root.as_ptr());
    rt.TickAll();

    ASSERT_TRUE(script->last_value().is_Vec3());
    EXPECT_DOUBLE_EQ(script->last_value().as_Vec3().value.x, 300.0);
}

TEST(ScriptValueCoercion, UpdateChangesWithoutReturnDoNotReachTheNextCall) {
    JsRuntime rt;
    auto*     script = rt.MakeFieldScript(
        R"JS(
            let calls = 0;
            export function update(value) {
                if (calls++ === 0) {
                    value.x = 900;
                    return;
                }
                return value;
            }
        )JS"_str,
        "test/update_without_return"_str,
        FieldKind::Vec3,
        owe::MakeObject(),
        owe::IntoJson("300 200 0"));
    ASSERT_NE(script, nullptr);

    rt.TickAll();
    rt.TickAll();

    ASSERT_TRUE(script->last_value().is_Vec3());
    EXPECT_DOUBLE_EQ(script->last_value().as_Vec3().value.x, 300.0);
}

TEST(ScriptValueCoercion, ValueKeptFromInitStaysAsItWas) {
    auto root = Arc<owe::SceneNode>::make();

    JsRuntime rt;
    auto*     script = rt.MakeFieldScript(
        R"JS(
            let kept;
            export function init(value) { kept = value; return value; }
            export function update(value) {
                value.x = 900;
                return kept;
            }
        )JS"_str,
        "test/value_kept_from_init"_str,
        FieldKind::Vec3,
        owe::MakeObject(),
        owe::IntoJson("300 200 0"));
    ASSERT_NE(script, nullptr);

    rt.SetSceneRoot(root.as_ptr());
    rt.TickAll();
    rt.TickAll();

    ASSERT_TRUE(script->last_value().is_Vec3());
    EXPECT_DOUBLE_EQ(script->last_value().as_Vec3().value.x, 300.0);
}
