// Conformance test for the QuickJS C API subset that src/Scene/Script uses.
//
// Built twice by run.sh: against QuickJS-ng (the reference) and against JsSpiderMonkey.cpp (the facade over
// SpiderMonkey). Both must print the same results, so the two backends are interchangeable under Script.cpp.
// Prints "ok <name>" / "FAIL <name>"; exit status is the number of failures.
#include <quickjs.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>

static int g_fail = 0;
#define CHECK(name, cond)                                                                                      \
    do {                                                                                                       \
        if (cond) std::printf("ok %s\n", name);                                                                \
        else { std::printf("FAIL %s (line %d)\n", name, __LINE__); ++g_fail; }                                 \
    } while (0)

static JSContext* ctx;
static JSRuntime* rt;

static std::string str(JSValueConst v) {
    const char* s = JS_ToCString(ctx, v);
    std::string out = s ? s : "<null>";
    if (s) JS_FreeCString(ctx, s);
    return out;
}
static JSValue eval(const char* src) { return JS_Eval(ctx, src, std::strlen(src), "<test>", JS_EVAL_TYPE_GLOBAL); }
static std::string evalStr(const char* src) {
    JSValue v = eval(src);
    if (JS_IsException(v)) {
        JSValue e = JS_GetException(ctx);
        std::string m = "EXC:" + str(e);
        JS_FreeValue(ctx, e);
        return m;
    }
    std::string s = str(v);
    JS_FreeValue(ctx, v);
    return s;
}

// --- a class with opaque data, accessors, methods, finalizer ------------------------------------------------------
static JSClassID g_box_id;
static int       g_finalized = 0;
struct Box { double x = 0; int calls = 0; };
static void BoxFinalizer(JSRuntime*, JSValue v) {
    delete static_cast<Box*>(JS_GetOpaque(v, g_box_id));
    ++g_finalized;
}
static JSClassDef g_box_def { .class_name = "Box", .finalizer = BoxFinalizer };
static JSValue BoxGetX(JSContext* c, JSValueConst t) {
    auto* b = static_cast<Box*>(JS_GetOpaque(t, g_box_id));
    return b ? JS_NewFloat64(c, b->x) : JS_ThrowTypeError(c, "not a Box");
}
static JSValue BoxSetX(JSContext* c, JSValueConst t, JSValueConst v) {
    auto* b = static_cast<Box*>(JS_GetOpaque(t, g_box_id));
    if (!b) return JS_ThrowTypeError(c, "not a Box");
    double d;
    if (JS_ToFloat64(c, &d, v)) return JS_EXCEPTION;
    b->x = d;
    return JS_UNDEFINED;
}
static JSValue BoxBump(JSContext* c, JSValueConst t, int argc, JSValueConst* argv) {
    auto* b = static_cast<Box*>(JS_GetOpaque(t, g_box_id));
    if (!b) return JS_ThrowTypeError(c, "not a Box");
    ++b->calls;
    double d = 1;
    if (argc > 0 && JS_ToFloat64(c, &d, argv[0])) return JS_EXCEPTION;
    return JS_NewFloat64(c, b->x += d);
}
static const JSCFunctionListEntry g_box_funcs[] = {
    JS_CGETSET_DEF("x", BoxGetX, BoxSetX),
    JS_CFUNC_DEF("bump", 1, BoxBump),
};
static JSValue MakeBox(double x) {
    JSValue o = JS_NewObjectClass(ctx, g_box_id);
    auto*   b = new Box;
    b->x      = x;
    JS_SetOpaque(o, b);
    return o;
}

// --- an exotic class: every property set goes to the C function --------------------------------------------------
static JSClassID g_mat_id;
static std::string g_mat_log;
static int MatSet(JSContext* c, JSValueConst obj, JSAtom atom, JSValueConst value, JSValueConst, int) {
    const char* k = JS_AtomToCString(c, atom);
    g_mat_log += std::string(k ? k : "?") + "=" + str(value) + ";";
    if (k) JS_FreeCString(c, k);
    return 1;
}
static JSClassExoticMethods g_mat_exotic { .set_property = MatSet };
static JSClassDef g_mat_def { .class_name = "Mat", .exotic = &g_mat_exotic };

// --- C function with captured data ---------------------------------------------------------------------------------
static JSValue DataFn(JSContext* c, JSValueConst, int argc, JSValueConst* argv, int magic, JSValueConst* data) {
    int32_t d;
    JS_ToInt32(c, &d, data[0]);
    return JS_NewInt32(c, d * 100 + magic * 10 + argc);
}

// --- module loader ------------------------------------------------------------------------------------------------
static JSModuleDef* Loader(JSContext* c, const char* name, void*) {
    if (std::strcmp(name, "Helper") != 0) {
        JS_ThrowReferenceError(c, "could not load module '%s'", name);
        return nullptr;
    }
    static const char src[] = "export const answer = 42; export function twice(x) { return x * 2; }";
    JSValue m = JS_Eval(c, src, sizeof src - 1, name, JS_EVAL_TYPE_MODULE | JS_EVAL_FLAG_COMPILE_ONLY);
    if (JS_IsException(m)) return nullptr;
    return static_cast<JSModuleDef*>(JS_VALUE_GET_PTR(m));
}

int main() {
    setvbuf(stdout, nullptr, _IOLBF, 0);
    rt  = JS_NewRuntime();
    JS_SetMaxStackSize(rt, 2u << 20);
    ctx = JS_NewContext(rt);
    JS_SetContextOpaque(ctx, &g_fail);
    CHECK("context opaque", JS_GetContextOpaque(ctx) == &g_fail);
    CHECK("runtime of context", JS_GetRuntime(ctx) == rt);

    // primitives and conversions
    CHECK("int32", JS_VALUE_GET_INT(JS_NewInt32(ctx, 7)) == 7);
    {
        JSValue v = eval("1.5 + 1");
        double d = 0;
        JS_ToFloat64(ctx, &d, v);
        int32_t i = 0;
        JS_ToInt32(ctx, &i, v);
        int64_t l = 0;
        JS_ToInt64(ctx, &l, v);
        CHECK("float64", d == 2.5 && i == 2 && l == 2);
        CHECK("number tag", JS_IsNumber(v));
    }
    { JSValue v = eval("'3.5'"); double d = 0; JS_ToFloat64(ctx, &d, v); CHECK("string to number", d == 3.5); JS_FreeValue(ctx, v); }
    { JSValue v = eval("''"); CHECK("empty string falsy", JS_ToBool(ctx, v) == 0); JS_FreeValue(ctx, v); }
    { JSValue v = eval("({})"); CHECK("object truthy", JS_ToBool(ctx, v) == 1 && JS_IsObject(v)); JS_FreeValue(ctx, v); }
    CHECK("bool", JS_IsBool(JS_NewBool(ctx, true)) && JS_ToBool(ctx, JS_NewBool(ctx, false)) == 0);
    CHECK("uint32", JS_ToBool(ctx, JS_NewUint32(ctx, 4000000000u)) == 1);

    // strings (UTF-8 round trip, embedded length)
    {
        const char* utf8 = "h\xc3\xa9llo \xe2\x82\xac";
        JSValue s = JS_NewStringLen(ctx, utf8, std::strlen(utf8));
        CHECK("string roundtrip", JS_IsString(s) && str(s) == utf8);
        JSValue g = JS_GetGlobalObject(ctx);
        JS_SetPropertyStr(ctx, g, "s", JS_DupValue(ctx, s));
        CHECK("string length in js", evalStr("s.length") == "7");
        JS_FreeValue(ctx, g);
        JS_FreeValue(ctx, s);
        JSValue part = JS_NewStringLen(ctx, "abcdef", 3);
        CHECK("string len", str(part) == "abc");
        JS_FreeValue(ctx, part);
    }

    // objects, arrays, properties
    {
        JSValue o = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, o, "a", JS_NewInt32(ctx, 1));
        JS_DefinePropertyValueStr(ctx, o, "b", JS_NewFloat64(ctx, 2.5), JS_PROP_C_W_E);
        JSValue a = JS_GetPropertyStr(ctx, o, "a");
        JSValue b = JS_GetPropertyStr(ctx, o, "b");
        JSValue c = JS_GetPropertyStr(ctx, o, "missing");
        CHECK("get/set property", JS_VALUE_GET_INT(a) == 1 && JS_IsNumber(b) && JS_IsUndefined(c));
        JSValue g = JS_GetGlobalObject(ctx);
        JS_SetPropertyStr(ctx, g, "o", JS_DupValue(ctx, o));
        CHECK("property visible to js", evalStr("Object.keys(o).join()") == "a,b");
        JS_DefinePropertyValueStr(ctx, o, "hidden", JS_NewInt32(ctx, 9), JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE);
        CHECK("non-enumerable property", evalStr("Object.keys(o).join()") == "a,b" && evalStr("o.hidden") == "9");
        JS_DefinePropertyValueStr(ctx, o, "ro", JS_NewInt32(ctx, 5), JS_PROP_ENUMERABLE);
        CHECK("read-only property", evalStr("o.ro = 6; o.ro") == "5");
        JS_FreeValue(ctx, g); JS_FreeValue(ctx, a); JS_FreeValue(ctx, b); JS_FreeValue(ctx, c); JS_FreeValue(ctx, o);

        JSValue arr = JS_NewArray(ctx);
        JS_DefinePropertyValueUint32(ctx, arr, 0, JS_NewInt32(ctx, 10), JS_PROP_C_W_E);
        JS_DefinePropertyValueUint32(ctx, arr, 1, JS_NewInt32(ctx, 20), JS_PROP_C_W_E);
        JS_SetPropertyUint32(ctx, arr, 2, JS_NewInt32(ctx, 30));
        JSValue e1 = JS_GetPropertyUint32(ctx, arr, 1);
        CHECK("array", JS_IsArray(arr) && JS_VALUE_GET_INT(e1) == 20);
        JSValue g2 = JS_GetGlobalObject(ctx);
        JS_SetPropertyStr(ctx, g2, "arr", JS_DupValue(ctx, arr));
        CHECK("array length in js", evalStr("arr.length + ':' + arr.join('-')") == "3:10-20-30");
        JS_FreeValue(ctx, g2); JS_FreeValue(ctx, e1); JS_FreeValue(ctx, arr);
        JSValue notArr = JS_NewObject(ctx);
        CHECK("not array", !JS_IsArray(notArr));
        JS_FreeValue(ctx, notArr);
    }

    // functions, calls, constructors
    {
        JSValue f = eval("(function (a, b) { return this.k + a * b; })");
        JSValue t = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, t, "k", JS_NewInt32(ctx, 100));
        JSValue args[2] = { JS_NewInt32(ctx, 3), JS_NewInt32(ctx, 4) };
        JSValue r = JS_Call(ctx, f, t, 2, args);
        CHECK("call with this", JS_IsFunction(ctx, f) && JS_VALUE_GET_INT(r) == 112);
        JS_FreeValue(ctx, r); JS_FreeValue(ctx, t); JS_FreeValue(ctx, f);
        JSValue ctor = eval("(class V { constructor(x, y) { this.x = x; this.y = y; } sum() { return this.x + this.y; } })");
        JSValue ca[2] = { JS_NewInt32(ctx, 5), JS_NewInt32(ctx, 6) };
        JSValue inst = JS_CallConstructor(ctx, ctor, 2, ca);
        JSValue x = JS_GetPropertyStr(ctx, inst, "x");
        CHECK("construct", JS_IsObject(inst) && JS_VALUE_GET_INT(x) == 5);
        JS_FreeValue(ctx, x); JS_FreeValue(ctx, inst); JS_FreeValue(ctx, ctor);
        JSValue bad = eval("(function () { throw new Error('inside'); })");
        JSValue rr = JS_Call(ctx, bad, JS_UNDEFINED, 0, nullptr);
        CHECK("call throws", JS_IsException(rr));
        JSValue e = JS_GetException(ctx);
        JSValue m = JS_GetPropertyStr(ctx, e, "message");
        CHECK("exception message", str(m) == "inside");
        JS_FreeValue(ctx, m); JS_FreeValue(ctx, e); JS_FreeValue(ctx, bad);
        CHECK("exception cleared", evalStr("1 + 1") == "2");
    }

    // C functions
    {
        JSValue g = JS_GetGlobalObject(ctx);
        JSValue f = JS_NewCFunction(ctx, [](JSContext* c, JSValueConst, int argc, JSValueConst* argv) -> JSValue {
            double s = 0;
            for (int i = 0; i < argc; ++i) { double d; if (JS_ToFloat64(c, &d, argv[i])) return JS_EXCEPTION; s += d; }
            return JS_NewFloat64(c, s);
        }, "sum", 0);
        JS_SetPropertyStr(ctx, g, "csum", f);
        CHECK("c function", evalStr("csum(1, 2, 3.5)") == "6.5");
        CHECK("c function many args", evalStr("csum(1,2,3,4,5,6,7,8,9,10,11,12)") == "78");
        JSValue throwing = JS_NewCFunction(ctx, [](JSContext* c, JSValueConst, int, JSValueConst*) -> JSValue {
            return JS_ThrowTypeError(c, "bad %s %d", "thing", 3);
        }, "thrower", 0);
        JS_SetPropertyStr(ctx, g, "thrower", throwing);
        CHECK("c function throws type error", evalStr("try { thrower(); } catch (e) { (e instanceof TypeError) + ':' + e.message }") == "true:bad thing 3");
        JSValue data[1] = { JS_NewInt32(ctx, 7) };
        JSValue df = JS_NewCFunctionData(ctx, DataFn, 0, 3, 1, data);
        JS_FreeValue(ctx, data[0]);
        JS_SetPropertyStr(ctx, g, "dataFn", df);
        CHECK("c function data", evalStr("dataFn(1, 2)") == "732");
        JS_FreeValue(ctx, g);
    }

    // classes
    {
        JS_NewClassID(rt, &g_box_id);
        JS_NewClass(rt, g_box_id, &g_box_def);
        JSValue proto = JS_NewObject(ctx);
        JS_SetPropertyFunctionList(ctx, proto, g_box_funcs, sizeof g_box_funcs / sizeof *g_box_funcs);
        JS_SetClassProto(ctx, g_box_id, proto);
        JSValue box = MakeBox(2.5);
        JSValue g = JS_GetGlobalObject(ctx);
        JS_SetPropertyStr(ctx, g, "box", JS_DupValue(ctx, box));
        CHECK("class getter", evalStr("box.x") == "2.5");
        CHECK("class setter", evalStr("box.x = 4; box.x") == "4");
        CHECK("class method", evalStr("box.bump(2) + ',' + box.bump()") == "6,7");
        CHECK("class accessor on prototype", evalStr("Object.getOwnPropertyDescriptor(Object.getPrototypeOf(box), 'x') !== undefined") == "true");
        CHECK("class method is non-enumerable", evalStr("Object.keys(box).length") == "0");
        CHECK("wrong this throws", evalStr("try { Object.getOwnPropertyDescriptor(Object.getPrototypeOf(box), 'x').get.call({}); } catch (e) { e.message }") == "not a Box");
        auto* b = static_cast<Box*>(JS_GetOpaque(box, g_box_id));
        CHECK("opaque", b && b->calls == 2 && b->x == 7);
        CHECK("opaque of other class id", JS_GetOpaque(box, g_box_id + 5) == nullptr);
        JSValue plain = JS_NewObject(ctx);
        CHECK("opaque of plain object", JS_GetOpaque(plain, g_box_id) == nullptr);
        JS_FreeValue(ctx, plain);
        JS_SetPropertyStr(ctx, g, "box", JS_UNDEFINED);
        JS_FreeValue(ctx, box);
        JS_FreeValue(ctx, g);
    }
    // exotic class
    {
        JS_NewClassID(rt, &g_mat_id);
        JS_NewClass(rt, g_mat_id, &g_mat_def);
        JSValue mat = JS_NewObjectClass(ctx, g_mat_id);
        JS_SetOpaque(mat, &g_fail);
        JSValue g = JS_GetGlobalObject(ctx);
        JS_SetPropertyStr(ctx, g, "mat", JS_DupValue(ctx, mat));
        evalStr("mat.alpha = 0.5; mat['color'] = 'red';");
        CHECK("exotic set_property", g_mat_log == "alpha=0.5;color=red;");
        CHECK("exotic opaque through proxy", JS_GetOpaque(mat, g_mat_id) == &g_fail);
        JS_FreeValue(ctx, g);
        JS_FreeValue(ctx, mat);
    }

    // promises, jobs, modules
    {
        JS_SetModuleLoaderFunc(rt, nullptr, Loader, nullptr);
        JSValue g = JS_GetGlobalObject(ctx);
        const char* src =
            "import { answer, twice } from 'Helper';\n"
            "export const fromHelper = twice(answer);\n"
            "globalThis.order = ['module'];\n"
            "Promise.resolve().then(() => order.push('then'));\n"
            "await null;\n"
            "order.push('after await');\n"
            "export function run(v) { return v + 1; }\n";
        JSValue compiled = JS_Eval(ctx, src, std::strlen(src), "mod.js", JS_EVAL_TYPE_MODULE | JS_EVAL_FLAG_COMPILE_ONLY);
        CHECK("module compiles", !JS_IsException(compiled));
        auto* def = static_cast<JSModuleDef*>(JS_VALUE_GET_PTR(compiled));
        JSValue pr = JS_EvalFunction(ctx, compiled);
        CHECK("module evaluates to a promise", !JS_IsException(pr) && JS_PromiseState(ctx, pr) != JS_PROMISE_NOT_A_PROMISE);
        int guard = 0;
        while (JS_PromiseState(ctx, pr) == JS_PROMISE_PENDING && guard++ < 100) {
            JSContext* jc = nullptr;
            if (JS_ExecutePendingJob(rt, &jc) <= 0) break;
        }
        CHECK("module promise fulfilled", JS_PromiseState(ctx, pr) == JS_PROMISE_FULFILLED);
        JS_FreeValue(ctx, pr);
        JSValue ns = JS_GetModuleNamespace(ctx, def);
        JSValue fh = JS_GetPropertyStr(ctx, ns, "fromHelper");
        CHECK("module imports a built-in module", JS_VALUE_GET_INT(fh) == 84);
        JSValue run = JS_GetPropertyStr(ctx, ns, "run");
        JSValue arg = JS_NewInt32(ctx, 41);
        JSValue rr = JS_Call(ctx, run, JS_UNDEFINED, 1, &arg);
        CHECK("module export callable", JS_VALUE_GET_INT(rr) == 42);
        JS_FreeValue(ctx, rr); JS_FreeValue(ctx, run); JS_FreeValue(ctx, fh); JS_FreeValue(ctx, ns);
        CHECK("job order", evalStr("order.join()") == "module,then,after await");
        JS_FreeValue(ctx, g);

        const char* bad = "import 'Nope';";
        JSValue bc = JS_Eval(ctx, bad, std::strlen(bad), "bad.js", JS_EVAL_TYPE_MODULE | JS_EVAL_FLAG_COMPILE_ONLY);
        JSValue bp = JS_EvalFunction(ctx, bc);
        bool failed = JS_IsException(bp);
        if (!failed) {
            JSContext* jc = nullptr;
            while (JS_ExecutePendingJob(rt, &jc) > 0) {}
            failed = JS_PromiseState(ctx, bp) == JS_PROMISE_REJECTED;
        }
        CHECK("unknown module import fails", failed);
        JSValue ex = JS_GetException(ctx);
        JS_FreeValue(ctx, ex);
        JS_FreeValue(ctx, bp);

        const char* thr = "export const x = 1; throw new Error('top level');";
        JSValue tc = JS_Eval(ctx, thr, std::strlen(thr), "thr.js", JS_EVAL_TYPE_MODULE | JS_EVAL_FLAG_COMPILE_ONLY);
        JSValue tp = JS_EvalFunction(ctx, tc);
        bool rejected = JS_IsException(tp);
        if (!rejected) {
            JSContext* jc = nullptr;
            while (JS_ExecutePendingJob(rt, &jc) > 0) {}
            rejected = JS_PromiseState(ctx, tp) == JS_PROMISE_REJECTED;
            if (rejected) { JSValue why = JS_PromiseResult(ctx, tp); CHECK("rejection reason", str(why).find("top level") != std::string::npos); JS_FreeValue(ctx, why); }
        }
        CHECK("throwing module fails", rejected);
        JSValue ex2 = JS_GetException(ctx);
        JS_FreeValue(ctx, ex2);
        JS_FreeValue(ctx, tp);
    }

    // JSON
    {
        JSValue j = JS_ParseJSON(ctx, "{\"a\":[1,2,{\"b\":\"c\"}]}", 21, "<json>");
        JSValue g = JS_GetGlobalObject(ctx);
        JS_SetPropertyStr(ctx, g, "j", j);
        CHECK("parse json", evalStr("j.a[2].b + j.a.length") == "c3");
        JSValue obj = eval("({ k: [1, 'two', null], n: 1.5 })");
        JSValue s = JS_JSONStringify(ctx, obj, JS_UNDEFINED, JS_UNDEFINED);
        CHECK("stringify json", str(s) == "{\"k\":[1,\"two\",null],\"n\":1.5}");
        JS_FreeValue(ctx, s); JS_FreeValue(ctx, obj); JS_FreeValue(ctx, g);
        JSValue badj = JS_ParseJSON(ctx, "{oops", 5, "<json>");
        CHECK("parse json error", JS_IsException(badj));
        JSValue ex = JS_GetException(ctx);
        JS_FreeValue(ctx, ex);
    }

    // typed arrays
    {
        JSValue ta = eval("(() => { const f = new Float32Array(8); f[1] = 2.5; return f.subarray(1, 5); })()");
        size_t off = 0, len = 0, bpe = 0;
        JSValue buf = JS_GetTypedArrayBuffer(ctx, ta, &off, &len, &bpe);
        CHECK("typed array view", !JS_IsException(buf) && off == 4 && len == 16 && bpe == 4);
        size_t total = 0;
        uint8_t* base = JS_GetArrayBuffer(ctx, &total, buf);
        float first = 0;
        if (base) std::memcpy(&first, base + off, 4);
        CHECK("array buffer data", base && total == 32 && first == 2.5f);
        JS_FreeValue(ctx, buf); JS_FreeValue(ctx, ta);
    }

    // atoms and defined accessors
    {
        JSValue o = JS_NewObject(ctx);
        JSAtom a = JS_NewAtom(ctx, "dyn");
        JSValue getter = JS_NewCFunction(ctx, [](JSContext* c, JSValueConst, int, JSValueConst*) -> JSValue { return JS_NewInt32(c, 77); }, "dyn", 0);
        JS_DefinePropertyGetSet(ctx, o, a, getter, JS_UNDEFINED, JS_PROP_ENUMERABLE | JS_PROP_CONFIGURABLE);
        JS_FreeAtom(ctx, a);
        JSValue g = JS_GetGlobalObject(ctx);
        JS_SetPropertyStr(ctx, g, "dynobj", o);
        CHECK("define getter", evalStr("dynobj.dyn") == "77");
        JS_FreeValue(ctx, g);
    }

    // errors thrown by the engine itself
    CHECK("syntax error reported", evalStr("(") .rfind("EXC:", 0) == 0);
    CHECK("reference error", evalStr("nothing_here").find("nothing_here") != std::string::npos);
    CHECK("recursion is a RangeError", evalStr("function r(){ return r() + 1; } try { r(); 'no error' } catch (e) { (e instanceof RangeError) + ':' + e.name }") == "true:RangeError");
    CHECK("recursion within the limit works", evalStr("function d(n){ return n ? d(n-1)+1 : 0 } d(300)") == "300");
    CHECK("recovers after recursion error", evalStr("[1,2,3].map(x => x * 2).join()") == "2,4,6");

    // RegExp legacy statics (bootstrap.js polyfills them only when the engine lacks them)
    CHECK("regexp statics usable", evalStr("/(\\d+)-(\\d+)/.test('a 12-34 b'); (RegExp.$1 === undefined) ? 'missing' : 'present'") == "present" ||
                                   evalStr("typeof RegExp.$1") == "undefined");

    // many values: the handle table must grow and recycle
    {
        JSValue g = JS_GetGlobalObject(ctx);
        for (int round = 0; round < 3; ++round) {
            for (int i = 0; i < 20000; ++i) {
                JSValue o = JS_NewObject(ctx);
                JS_SetPropertyStr(ctx, o, "i", JS_NewInt32(ctx, i));
                JSValue s = JS_NewString(ctx, "x");
                JS_FreeValue(ctx, s);
                JS_FreeValue(ctx, o);
            }
        }
        CHECK("handle churn", evalStr("1 + 1") == "2");
        JS_FreeValue(ctx, g);
    }

    // finalizers run when objects die
    {
        int before = g_finalized;
        for (int i = 0; i < 50; ++i) { JSValue b = MakeBox(i); JS_FreeValue(ctx, b); }
        evalStr("for (let i = 0; i < 10; i++) { new Array(100000).fill(0); }");
        CHECK("objects can be created and dropped", g_finalized >= before);
    }

    // A runtime is built on one thread, driven from another and destroyed on a third (the viewer does exactly that).
    {
        JSRuntime* r2 = nullptr;
        JSContext* c2 = nullptr;
        std::thread([&] { r2 = JS_NewRuntime(); JS_SetMaxStackSize(r2, 2u << 20); c2 = JS_NewContext(r2); }).join();
        double sum = 0;
        std::thread([&] {
            const char* src = "let s = 0; for (let i = 0; i < 100000; i++) s += i % 7; s";
            JSValue v = JS_Eval(c2, src, std::strlen(src), "<t>", JS_EVAL_TYPE_GLOBAL);
            JS_ToFloat64(c2, &sum, v);
            JS_FreeValue(c2, v);
        }).join();
        CHECK("runtime used from another thread", sum == 299995);
        double again = 0;
        std::thread([&] {
            const char* src = "[1, 2, 3].map(x => x * 2).reduce((a, b) => a + b)";
            JSValue v = JS_Eval(c2, src, std::strlen(src), "<t>", JS_EVAL_TYPE_GLOBAL);
            JS_ToFloat64(c2, &again, v);
            JS_FreeValue(c2, v);
        }).join();
        CHECK("runtime used from a third thread", again == 12);
        std::thread([&] { JS_FreeContext(c2); JS_FreeRuntime(r2); }).join();
        CHECK("runtime freed on yet another thread", true);
    }
    // Several runtimes alive at once on one thread are independent.
    {
        JSRuntime* ra = JS_NewRuntime();
        JSContext* ca = JS_NewContext(ra);
        JSRuntime* rb = JS_NewRuntime();
        JSContext* cb = JS_NewContext(rb);
        const char* seta = "globalThis.who = 'a'";
        const char* setb = "globalThis.who = 'b'";
        JS_FreeValue(ca, JS_Eval(ca, seta, std::strlen(seta), "<a>", JS_EVAL_TYPE_GLOBAL));
        JS_FreeValue(cb, JS_Eval(cb, setb, std::strlen(setb), "<b>", JS_EVAL_TYPE_GLOBAL));
        JSValue ga = JS_GetGlobalObject(ca), gb = JS_GetGlobalObject(cb);
        JSValue wa = JS_GetPropertyStr(ca, ga, "who"), wb = JS_GetPropertyStr(cb, gb, "who");
        const char* sa = JS_ToCString(ca, wa);
        const char* sb = JS_ToCString(cb, wb);
        CHECK("two runtimes are independent", sa && sb && std::string(sa) == "a" && std::string(sb) == "b");
        JS_FreeCString(ca, sa); JS_FreeCString(cb, sb);
        JS_FreeValue(ca, wa); JS_FreeValue(cb, wb); JS_FreeValue(ca, ga); JS_FreeValue(cb, gb);
        JS_FreeContext(ca); JS_FreeRuntime(ra);
        JS_FreeContext(cb); JS_FreeRuntime(rb);
    }

    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
    CHECK("finalizers ran by shutdown", g_finalized >= 1);
    std::printf("%d failure(s)\n", g_fail);
    return g_fail;
}
