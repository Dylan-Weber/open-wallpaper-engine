// QuickJS C API on top of V8.
//
// The same idea as JsSpiderMonkey.cpp: Script.cpp, ScriptConversion.cpp and ScriptPrograms.cpp are written against
// quickjs.h; this file implements the subset of its extern functions they use on top of V8, so the same sources run on
// a second JIT. Build with OWE_JS_V8 (and OWE_JS_SPIDERMONKEY, which selects the engine-thread paths in Script.cpp,
// SceneWallpaper.cpp and friends: it means "an engine with a thread of its own", not the engine's name) and link V8.
//
// Handles, ownership, the engine thread per runtime, hop(), the asynchronous post and the scene gate are as in
// JsSpiderMonkey.cpp (the structure is shared; read its header comment). What differs:
//  - a JSRuntime* and a JSContext* are both the runtime's Fac*;
//  - values live in v8::Global slots;
//  - a pending exception is a Global held in Fac::pending (V8 has no per-context exception state): natives that return
//    JS_EXCEPTION have it rethrown by the trampoline, and every call into JS runs under a v8::TryCatch that stores it;
//  - class instances carry a FinRec in an internal field (opaque pointer, class id, weak handle that runs the finalizer);
//  - exotic classes are wrapped in a Proxy with a `set` trap, as there.
#include <libplatform/libplatform.h>
#include <v8.h>

#include <dlfcn.h>
#include <pthread.h>
#include <sys/stat.h>

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <functional>
#include <thread>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <quickjs.h>

using OweAtom = JSAtom;

namespace
{

enum Kind : int {
    kGeneric     = 0,
    kGetter      = 1,
    kSetter      = 2,
    kGetterMagic = 3,
    kSetterMagic = 4,
    kData        = 5,
};

struct Fac;

struct ClassRec {
    JSClassDef                  def {};
    JSClassID                   id { 0 };
    v8::Global<v8::ObjectTemplate> tmpl;
    v8::Global<v8::Object>      proto;
    v8::Global<v8::Object>      handler;  // proxy handler for exotic classes
};

struct ModuleDef {
    v8::Global<v8::Module> module;
    std::string            name;
};

// The bookkeeping behind a class instance (internal field 0). The weak handle runs the class finalizer.
struct FinRec {
    Fac*                 f { nullptr };
    ClassRec*            cls { nullptr };
    void*                opaque { nullptr };
    v8::Global<v8::Object> self;
    bool                 dead { false };
};

struct NativeRec {
    void*                         fn { nullptr };
    int                           kind { kGeneric };
    int                           magic { 0 };
    std::vector<v8::Global<v8::Value>> data;
};

struct Fac {
    v8::Isolate*                                 iso { nullptr };
    v8::Global<v8::Context>                      ctx;
    std::vector<v8::Global<v8::Value>>           vals;
    std::vector<int32_t>                         rc;
    std::vector<uint32_t>                        freeSlots;
    std::vector<v8::Global<v8::Name>>            atoms;
    std::vector<uint32_t>                        freeAtoms;
    std::vector<std::unique_ptr<ClassRec>>       classes;  // by class id
    std::vector<ModuleDef*>                      modules;
    std::unordered_map<std::string, ModuleDef*>  moduleByName;
    std::unordered_map<int, ModuleDef*>          moduleByHash;
    JSModuleLoaderFunc*                          loader { nullptr };
    void*                                        loaderOpaque { nullptr };
    void*                                        opaque { nullptr };
    v8::Global<v8::Value>                        pending;
    bool                                         hasPending { false };
    bool                                         jobsMayExist { false };
    std::unordered_map<std::string, v8::Global<v8::String>> names;  // internalized property names used with the *Str calls
    std::unordered_set<FinRec*>                  finRecs;
    std::vector<std::unique_ptr<NativeRec>>      natives;
    v8::Global<v8::Function>                     proxyCtor;
    v8::Global<v8::Object>                       global;
    uintptr_t                                    stackBase { 0 };
    size_t                                       stackQuota { 0 };
    v8::ArrayBuffer::Allocator*                  allocator { nullptr };

    uint32_t                id { 0 };
    pthread_t               pt {};
    std::atomic<bool>       started { false };
    std::mutex              submitMu;
    std::function<void()>   job;
    std::atomic<int>        state { 0 };
    std::atomic<bool>       quit { false };
    bool                    asyncPending { false };
    void (*gateLock)(void*) { nullptr };
    void (*gateUnlock)(void*) { nullptr };
    void (*gateReleaseAll)(void*) { nullptr };
    void (*gateNotify)(void*, bool) { nullptr };
    void*             gateArg { nullptr };
    std::unordered_set<void*> gateExempt;
    std::atomic<bool> gateActive { false };
};

std::once_flag                       g_initOnce;
bool                                 g_initOk = false;
std::unique_ptr<v8::Platform>        g_platform;
std::mutex                           g_mu;
std::unordered_map<JSContext*, Fac*> g_byCx;
std::vector<Fac*>                    g_byId(1, nullptr);
std::atomic<int>                     g_live { 0 };
thread_local Fac*                    tl_cur = nullptr;
std::atomic<uint32_t>                g_nextClassId { 1 };

inline JSContext* ctxOf(Fac* f) { return reinterpret_cast<JSContext*>(f); }
Fac* lookupCx(JSContext* cx) {
    std::lock_guard<std::mutex> lock(g_mu);
    auto                        it = g_byCx.find(cx);
    return it == g_byCx.end() ? nullptr : it->second;
}
Fac* lookupId(uint32_t id) {
    std::lock_guard<std::mutex> lock(g_mu);
    return id < g_byId.size() ? g_byId[id] : nullptr;
}
inline Fac* F(JSContext* cx) { return reinterpret_cast<Fac*>(cx); }

inline bool onEngine(const Fac* f) { return f->started.load() && pthread_equal(f->pt, pthread_self()); }

inline void cpuRelax() {
#if defined(__x86_64__) || defined(__i386__)
    __builtin_ia32_pause();
#elif defined(__aarch64__)
    asm volatile("yield");
#endif
}
std::atomic<uint64_t> g_hops { 0 }, g_hopSleeps { 0 }, g_engineSleeps { 0 }, g_asyncPosts { 0 }, g_gateBlocked { 0 };
const bool                       g_traceHops = [] { const char* v = getenv("OWE_JS_DEBUG"); return v && v[0] == '2'; }();
std::mutex                       g_traceMu;
std::unordered_map<void*, uint64_t> g_hopCallers;
const bool g_traceNatives = [] { const char* v = getenv("OWE_JS_DEBUG"); return v && v[0] == '3'; }();
std::vector<void*>                  g_nativeOrder;
std::unordered_map<void*, uint64_t> g_nativeCalls;
inline void noteCaller(void* addr, const Fac* f) {
    if (__builtin_expect(g_traceHops, 0) && ! onEngine(f)) {
        std::lock_guard<std::mutex> lock(g_traceMu);
        ++g_hopCallers[addr];
    }
}
constexpr int kSpin = 4000;

void engineMain(Fac* f) {
    tl_cur = f;
    {
        char marker;
        f->stackBase = reinterpret_cast<uintptr_t>(&marker);
    }
    for (;;) {
        int v = f->state.load(std::memory_order_acquire);
        for (int i = 0; v != 1 && i < kSpin; ++i) {
            if (f->quit.load(std::memory_order_relaxed)) return;
            cpuRelax();
            v = f->state.load(std::memory_order_acquire);
        }
        while (v != 1) {
            if (f->quit.load(std::memory_order_acquire)) return;
            g_engineSleeps.fetch_add(1, std::memory_order_relaxed);
            f->state.wait(v, std::memory_order_acquire);
            v = f->state.load(std::memory_order_acquire);
        }
        auto job = std::move(f->job);
        job();
        // V8 posts GC and compilation follow-up work to the isolate's foreground task runner; run what is due.
        if (f->iso && g_platform) while (v8::platform::PumpMessageLoop(g_platform.get(), f->iso)) {}
        f->state.store(2, std::memory_order_release);
        f->state.notify_one();
    }
}

void stopEngineThread(Fac* f) {
    f->quit.store(true, std::memory_order_release);
    f->state.store(3, std::memory_order_release);
    f->state.notify_all();
}

void collectAsync(Fac* f) {
    if (f->gateReleaseAll) f->gateReleaseAll(f->gateArg);
    int v = f->state.load(std::memory_order_acquire);
    while (v != 2) {
        for (int i = 0; v != 2 && i < kSpin; ++i) { cpuRelax(); v = f->state.load(std::memory_order_acquire); }
        if (v != 2) { f->state.wait(v, std::memory_order_acquire); v = f->state.load(std::memory_order_acquire); }
    }
    f->state.store(0, std::memory_order_release);
    f->asyncPending = false;
    f->gateActive.store(false, std::memory_order_release);
    if (f->gateNotify) f->gateNotify(f->gateArg, false);
}

template <class R, class Fn>
R hop(Fac* f, Fn&& fn) {
    if (onEngine(f)) return fn();
    g_hops.fetch_add(1, std::memory_order_relaxed);
    if (f->gateActive.load(std::memory_order_acquire) && f->gateReleaseAll) f->gateReleaseAll(f->gateArg);
    std::lock_guard<std::mutex> submit(f->submitMu);
    if (f->asyncPending) collectAsync(f);
    auto                        wait = [&] {
        int v = f->state.load(std::memory_order_acquire);
        for (int i = 0; v != 2 && i < kSpin; ++i) {
            cpuRelax();
            v = f->state.load(std::memory_order_acquire);
        }
        while (v != 2) {
            g_hopSleeps.fetch_add(1, std::memory_order_relaxed);
            f->state.wait(v, std::memory_order_acquire);
            v = f->state.load(std::memory_order_acquire);
        }
    };
    if constexpr (std::is_void_v<R>) {
        f->job = [&] { fn(); };
        f->state.store(1, std::memory_order_release);
        f->state.notify_one();
        wait();
        f->state.store(0, std::memory_order_release);
    } else {
        R result {};
        f->job = [&] { result = fn(); };
        f->state.store(1, std::memory_order_release);
        f->state.notify_one();
        wait();
        f->state.store(0, std::memory_order_release);
        return result;
    }
}

// See JsSpiderMonkey.cpp: a POSIX TZ such as "EST+05:00" is read by libc but not by ICU; use the equivalent Etc/GMT zone.
void normalizeTimeZone() {
    const char* tz = getenv("TZ");
    if (! tz) return;
    size_t i = 0;
    while (tz[i] && ((tz[i] >= 'A' && tz[i] <= 'Z') || (tz[i] >= 'a' && tz[i] <= 'z'))) ++i;
    if (i < 3 || (tz[i] != '+' && tz[i] != '-')) return;
    char        sign = tz[i++];
    int         hours = 0, minutes = 0, digits = 0;
    while (tz[i] >= '0' && tz[i] <= '9' && digits < 2) { hours = hours * 10 + (tz[i++] - '0'); ++digits; }
    if (digits == 0) return;
    if (tz[i] == ':') {
        ++i;
        minutes = (tz[i] - '0') * 10 + (tz[i + 1] - '0');
        i += 2;
        if (tz[i] == ':') i += 3;
    }
    if (tz[i] != '\0' || minutes != 0 || hours > 14) return;
    char id[32];
    snprintf(id, sizeof id, "Etc/GMT%c%d", sign, hours);
    char path[96];
    snprintf(path, sizeof path, "/usr/share/zoneinfo/%s", id);
    struct stat st;
    if (stat(path, &st) != 0) return;
    setenv("TZ", id, 1);
}

// V8 flags. Measured on the real More Physics script (600 frames, shim): V8's default inlines at most 920 bytes of callee
// bytecode per optimized function in total, which leaves cannon-es' small vector helpers as calls; 3000 takes the
// collapse phase (frames 60-300) from 8.8 to 7.1 ms and the mean from 7.1 to 5.8 ms, 6000 and 12000 are no better.
// OWE_V8_FLAGS adds flags for experiments.
void setV8Flags() {
    v8::V8::SetFlagsFromString("--max-inlined-bytecode-size-cumulative=3000");
    if (const char* extra = getenv("OWE_V8_FLAGS")) v8::V8::SetFlagsFromString(extra);
}

// --- value conversion ----------------------------------------------------------------------------------------------

uint32_t newSlot(Fac* f, v8::Local<v8::Value> v) {
    if (! f->freeSlots.empty()) {
        uint32_t i = f->freeSlots.back();
        f->freeSlots.pop_back();
        f->vals[i].Reset(f->iso, v);
        f->rc[i] = 1;
        return i;
    }
    f->vals.emplace_back(f->iso, v);
    f->rc.push_back(1);
    return uint32_t(f->vals.size() - 1);
}

constexpr int kIdShift = 40;
inline uint32_t slotOf(JSValueConst v) {
    return uint32_t(reinterpret_cast<uintptr_t>(JS_VALUE_GET_PTR(v)) & ((uintptr_t(1) << kIdShift) - 1)) - 1;
}
inline uint32_t idOf(JSValueConst v) { return uint32_t(reinterpret_cast<uintptr_t>(JS_VALUE_GET_PTR(v)) >> kIdShift); }

JSValue mkBoxed(Fac* f, int tag, v8::Local<v8::Value> v) {
    uint32_t i = newSlot(f, v);
    return JS_MKPTR(tag, reinterpret_cast<void*>((uintptr_t(f->id) << kIdShift) | (uintptr_t(i) + 1)));
}

JSValue fromV8(Fac* f, v8::Local<v8::Value> v) {
    if (v->IsInt32()) return JS_NewInt32(nullptr, v.As<v8::Int32>()->Value());
    if (v->IsNumber()) return JS_NewFloat64(nullptr, v.As<v8::Number>()->Value());
    if (v->IsBoolean()) return JS_NewBool(nullptr, v->IsTrue());
    if (v->IsNull()) return JS_NULL;
    if (v->IsUndefined()) return JS_UNDEFINED;
    if (v->IsObject()) return mkBoxed(f, JS_TAG_OBJECT, v);
    if (v->IsString()) return mkBoxed(f, JS_TAG_STRING, v);
    if (v->IsSymbol()) return mkBoxed(f, JS_TAG_SYMBOL, v);
    if (v->IsBigInt()) return mkBoxed(f, JS_TAG_BIG_INT, v);
    return JS_UNDEFINED;
}

// A raw handle for a finalizer: the object is already dying, so it carries the FinRec rather than a slot.
constexpr int kTagRawObject = JS_TAG_UNINITIALIZED;

v8::Local<v8::Value> toV8(Fac* f, JSValueConst v) {
    switch (JS_VALUE_GET_TAG(v)) {
    case JS_TAG_INT: return v8::Integer::New(f->iso, JS_VALUE_GET_INT(v));
    case JS_TAG_BOOL: return v8::Boolean::New(f->iso, JS_VALUE_GET_BOOL(v));
    case JS_TAG_NULL: return v8::Null(f->iso);
    case JS_TAG_UNDEFINED: return v8::Undefined(f->iso);
    case JS_TAG_FLOAT64: return v8::Number::New(f->iso, JS_VALUE_GET_FLOAT64(v));
    case JS_TAG_OBJECT:
    case JS_TAG_STRING:
    case JS_TAG_SYMBOL:
    case JS_TAG_BIG_INT: return f->vals[slotOf(v)].Get(f->iso);
    case JS_TAG_MODULE: {
        auto* def = static_cast<ModuleDef*>(JS_VALUE_GET_PTR(v));
        return def->module.Get(f->iso)->GetModuleNamespace();
    }
    case kTagRawObject: return static_cast<FinRec*>(JS_VALUE_GET_PTR(v))->self.Get(f->iso);
    default: return v8::Undefined(f->iso);
    }
}

inline bool isBoxed(JSValueConst v) {
    int t = JS_VALUE_GET_TAG(v);
    return t == JS_TAG_OBJECT || t == JS_TAG_STRING || t == JS_TAG_SYMBOL || t == JS_TAG_BIG_INT;
}

void releaseV(Fac* f, JSValueConst v) {
    if (! isBoxed(v)) return;
    uint32_t i = slotOf(v);
    if (--f->rc[i] == 0) {
        f->vals[i].Reset();
        f->freeSlots.push_back(i);
    }
}

v8::Local<v8::Value> takeV8(Fac* f, JSValue v) {
    v8::Local<v8::Value> out = toV8(f, v);
    releaseV(f, v);
    return out;
}

v8::Local<v8::Object> objOf(Fac* f, JSValueConst v) {
    int t = JS_VALUE_GET_TAG(v);
    if (t != JS_TAG_OBJECT && t != kTagRawObject) return {};
    v8::Local<v8::Value> x = toV8(f, v);
    return x->IsObject() ? x.As<v8::Object>() : v8::Local<v8::Object>();
}

// A property name as an internalized string, cached: creating and internalizing a string on every call dominates the
// cost of a property access made through the C++ API.
inline v8::Local<v8::String> nameOf(Fac* f, const char* s) {
    auto it = f->names.find(s);
    if (it != f->names.end()) return it->second.Get(f->iso);
    v8::Local<v8::String> n = v8::String::NewFromUtf8(f->iso, s, v8::NewStringType::kInternalized).ToLocalChecked();
    f->names.emplace(s, v8::Global<v8::String>(f->iso, n));
    return n;
}

// Every entry point that touches V8 values opens a handle scope; the engine thread has entered the isolate and context.
#define ENTER(f) v8::HandleScope hs_(f->iso)

inline v8::Local<v8::Context> cxOf(Fac* f) { return f->ctx.Get(f->iso); }
inline v8::Local<v8::String> str(Fac* f, const char* s, size_t n = size_t(-1)) {
    return v8::String::NewFromUtf8(f->iso, s, v8::NewStringType::kNormal, n == size_t(-1) ? -1 : int(n)).ToLocalChecked();
}

void setPending(Fac* f, v8::Local<v8::Value> e) {
    f->pending.Reset(f->iso, e);
    f->hasPending = true;
}

void clearPending(Fac* f) {
    f->pending.Reset();
    f->hasPending = false;
}

// Records the exception a TryCatch caught and returns JS_EXCEPTION.
JSValue caught(Fac* f, v8::TryCatch& tc) {
    if (tc.HasCaught() && ! tc.Exception().IsEmpty()) {
        setPending(f, tc.Exception());
    } else {
        setPending(f, v8::Exception::Error(str(f, "script terminated (out of memory or interrupted)")));
    }
    return JS_EXCEPTION;
}

JSValue throwNamedMsg(Fac* f, const char* ctorName, const char* buf) {
    v8::Local<v8::Context> cx = cxOf(f);
    v8::Local<v8::Value>   ctor;
    v8::Local<v8::Value>   err;
    if (strcmp(ctorName, "InternalError") != 0 && f->global.Get(f->iso)->Get(cx, str(f, ctorName)).ToLocal(&ctor) &&
        ctor->IsFunction()) {
        v8::Local<v8::Value> arg = str(f, buf);
        if (ctor.As<v8::Function>()->NewInstance(cx, 1, &arg).ToLocal(&err)) {
            setPending(f, err);
            return JS_EXCEPTION;
        }
    }
    v8::Local<v8::Value> e = v8::Exception::Error(str(f, buf));
    if (strcmp(ctorName, "InternalError") == 0 && e->IsObject())
        (void)e.As<v8::Object>()->Set(cx, str(f, "name"), str(f, "InternalError"));
    setPending(f, e);
    return JS_EXCEPTION;
}

JSValue throwFmt(JSContext* ctx, const char* ctorName, const char* fmt, va_list ap) {
    char buf[1024];
    vsnprintf(buf, sizeof buf, fmt, ap);
    Fac* f = F(ctx);
    return hop<JSValue>(f, [&] { ENTER(f); return throwNamedMsg(f, ctorName, buf); });
}

// --- native function trampolines -----------------------------------------------------------------------------------

void tramp(const v8::FunctionCallbackInfo<v8::Value>& info) {
    Fac*       f   = tl_cur;
    ENTER(f);
    auto*      rec = static_cast<NativeRec*>(info.Data().As<v8::External>()->Value());
    int        argc = info.Length();
    void*      fnp = rec->fn;

    JSValue  stackArgv[8];
    std::vector<JSValue> heapArgv;
    JSValue* argv = stackArgv;
    if (argc > 8) {
        heapArgv.resize(argc);
        argv = heapArgv.data();
    }
    JSValue thisv = fromV8(f, info.This());
    for (int i = 0; i < argc; ++i) argv[i] = fromV8(f, info[i]);

    const bool gated = f->gateLock && f->gateActive.load(std::memory_order_acquire) && ! f->gateExempt.count(fnp);
    if (gated) f->gateLock(f->gateArg);
    if (gated && g_traceNatives) {
        std::lock_guard<std::mutex> lock(g_traceMu);
        if (g_nativeCalls[fnp]++ == 0) g_nativeOrder.push_back(fnp);
    }
    JSContext* cx = ctxOf(f);
    JSValue    ret = JS_UNDEFINED;
    int        kind = rec->kind;
    int        magic = rec->magic;
    switch (kind) {
    case kGeneric: ret = reinterpret_cast<JSCFunction*>(fnp)(cx, thisv, argc, argv); break;
    case kGetter: ret = reinterpret_cast<JSValue (*)(JSContext*, JSValueConst)>(fnp)(cx, thisv); break;
    case kSetter:
        ret = reinterpret_cast<JSValue (*)(JSContext*, JSValueConst, JSValueConst)>(fnp)(
            cx, thisv, argc > 0 ? argv[0] : JS_UNDEFINED);
        break;
    case kGetterMagic:
        ret = reinterpret_cast<JSValue (*)(JSContext*, JSValueConst, int)>(fnp)(cx, thisv, magic);
        break;
    case kSetterMagic:
        ret = reinterpret_cast<JSValue (*)(JSContext*, JSValueConst, JSValueConst, int)>(fnp)(
            cx, thisv, argc > 0 ? argv[0] : JS_UNDEFINED, magic);
        break;
    case kData: {
        std::vector<JSValue> data(rec->data.size());
        for (size_t i = 0; i < data.size(); ++i) data[i] = fromV8(f, rec->data[i].Get(f->iso));
        ret = reinterpret_cast<JSValue (*)(JSContext*, JSValueConst, int, JSValueConst*, int, JSValueConst*)>(fnp)(
            cx, thisv, argc, argv, magic, data.data());
        for (auto& d : data) releaseV(f, d);
        break;
    }
    }

    if (gated) f->gateUnlock(f->gateArg);
    releaseV(f, thisv);
    for (int i = 0; i < argc; ++i) releaseV(f, argv[i]);
    if (JS_IsException(ret)) {
        if (! f->hasPending) setPending(f, v8::Exception::Error(str(f, "native function failed without an exception")));
        v8::Local<v8::Value> e = f->pending.Get(f->iso);
        clearPending(f);
        f->iso->ThrowException(e);
        return;
    }
    info.GetReturnValue().Set(takeV8(f, ret));
}

v8::Local<v8::Function> newNative(Fac* f, void* fnp, int kind, int magic, const char* name, int length) {
    auto rec   = std::make_unique<NativeRec>();
    rec->fn    = fnp;
    rec->kind  = kind;
    rec->magic = magic;
    NativeRec* raw = rec.get();
    f->natives.push_back(std::move(rec));
    v8::Local<v8::Function> fn;
    if (! v8::Function::New(cxOf(f), tramp, v8::External::New(f->iso, raw), length).ToLocal(&fn)) return {};
    fn->SetName(str(f, name ? name : ""));
    return fn;
}

// --- classes -------------------------------------------------------------------------------------------------------

ClassRec* classOf(Fac* f, JSClassID id) {
    if (id == 0 || id >= f->classes.size()) return nullptr;
    return f->classes[id].get();
}

void finalizeRec(FinRec* r) {
    if (r->dead) return;
    r->dead = true;
    Fac* f = r->f;
    if (r->cls && r->cls->def.finalizer) {
        JSValue v = JS_MKPTR(kTagRawObject, r);
        r->cls->def.finalizer(reinterpret_cast<JSRuntime*>(f), v);
    }
    f->finRecs.erase(r);
    r->self.Reset();
    delete r;
}

v8::Local<v8::Object> unwrapProxy(v8::Local<v8::Object> o) {
    while (! o.IsEmpty() && o->IsProxy()) {
        v8::Local<v8::Value> t = o.As<v8::Proxy>()->GetTarget();
        if (! t->IsObject()) return {};
        o = t.As<v8::Object>();
    }
    return o;
}

FinRec* finOf(v8::Local<v8::Object> o) {
    o = unwrapProxy(o);
    if (o.IsEmpty() || o->InternalFieldCount() < 1) return nullptr;
    auto* r = static_cast<FinRec*>(o->GetAlignedPointerFromInternalField(0));
    if (! r) return nullptr;
    Fac* f = tl_cur;
    return (f && f->finRecs.count(r)) ? r : nullptr;
}

uint32_t atomOfName(Fac* f, v8::Local<v8::Name> n) {
    uint32_t atom;
    if (! f->freeAtoms.empty()) {
        atom = f->freeAtoms.back();
        f->freeAtoms.pop_back();
        f->atoms[atom].Reset(f->iso, n);
    } else {
        f->atoms.emplace_back(f->iso, n);
        atom = uint32_t(f->atoms.size() - 1);
    }
    return atom;
}

void proxySetTrap(const v8::FunctionCallbackInfo<v8::Value>& info) {
    Fac* f = tl_cur;
    ENTER(f);
    auto*    rec = static_cast<ClassRec*>(info.Data().As<v8::External>()->Value());
    uint32_t atom = 0;
    v8::Local<v8::Value> key = info[1];
    if (key->IsName()) atom = atomOfName(f, key.As<v8::Name>());
    else atom = atomOfName(f, key->ToString(cxOf(f)).ToLocalChecked());
    JSValue tv = fromV8(f, info[0]), vv = fromV8(f, info[2]), rv = fromV8(f, info[3]);
    int     r  = rec->def.exotic->set_property(ctxOf(f), tv, OweAtom(atom + 1), vv, rv, 0);
    releaseV(f, tv);
    releaseV(f, vv);
    releaseV(f, rv);
    f->freeAtoms.push_back(atom);
    if (r < 0) {
        if (! f->hasPending) setPending(f, v8::Exception::Error(str(f, "set failed")));
        v8::Local<v8::Value> e = f->pending.Get(f->iso);
        clearPending(f);
        f->iso->ThrowException(e);
        return;
    }
    info.GetReturnValue().Set(true);
}

}  // namespace

// ===================================================================================================================
// extern "C" surface (names and signatures from quickjs.h)
// ===================================================================================================================

extern "C" {

JSRuntime* JS_NewRuntime(void) {
    std::call_once(g_initOnce, [] {
        normalizeTimeZone();
        setV8Flags();
        int workers = 4;
        if (const char* w = getenv("OWE_V8_WORKERS")) workers = atoi(w);
        g_platform = v8::platform::NewDefaultPlatform(workers);
        v8::V8::InitializePlatform(g_platform.get());
        g_initOk = v8::V8::Initialize();
    });
    if (! g_initOk) return nullptr;
    auto* f = new Fac();
    {
        std::lock_guard<std::mutex> lock(g_mu);
        uint32_t                    id = 1;
        while (id < g_byId.size() && g_byId[id]) ++id;
        if (id == g_byId.size()) g_byId.push_back(nullptr);
        f->id     = id;
        g_byId[id] = f;
        g_byCx[ctxOf(f)] = f;
    }
    {
        pthread_attr_t attr;
        pthread_attr_init(&attr);
        pthread_attr_setstacksize(&attr, size_t(16) << 20);
        pthread_create(
            &f->pt, &attr, [](void* a) -> void* { engineMain(static_cast<Fac*>(a)); return nullptr; }, f);
        pthread_attr_destroy(&attr);
        f->started.store(true);
    }
    bool ok = hop<bool>(f, [&] {
        f->allocator = v8::ArrayBuffer::Allocator::NewDefaultAllocator();
        v8::Isolate::CreateParams cp;
        cp.array_buffer_allocator = f->allocator;
        f->iso = v8::Isolate::New(cp);
        if (! f->iso) return false;
        f->iso->Enter();
        f->iso->SetMicrotasksPolicy(v8::MicrotasksPolicy::kExplicit);
        f->vals.emplace_back();  // slot 0 unused
        f->rc.push_back(0);
        f->classes.emplace_back(nullptr);
        f->atoms.emplace_back();  // atom 0 unused
        return true;
    });
    if (! ok) {
        stopEngineThread(f);
        pthread_join(f->pt, nullptr);
        std::lock_guard<std::mutex> lock(g_mu);
        g_byId[f->id] = nullptr;
        g_byCx.erase(ctxOf(f));
        delete f;
        return nullptr;
    }
    g_live.fetch_add(1);
    return reinterpret_cast<JSRuntime*>(f);
}

JSContext* impl_JS_NewContext(JSRuntime* rt) {
    Fac* f = reinterpret_cast<Fac*>(rt);
    if (! f->ctx.IsEmpty()) return nullptr;  // one context per runtime
    ENTER(f);
    v8::Local<v8::Context> cx = v8::Context::New(f->iso);
    if (cx.IsEmpty()) return nullptr;
    cx->Enter();
    f->ctx.Reset(f->iso, cx);
    f->global.Reset(f->iso, cx->Global());
    v8::Local<v8::Value> pc;
    if (cx->Global()->Get(cx, str(f, "Proxy")).ToLocal(&pc) && pc->IsFunction()) f->proxyCtor.Reset(f->iso, pc.As<v8::Function>());
    f->iso->DateTimeConfigurationChangeNotification(v8::Isolate::TimeZoneDetection::kRedetect);
    return ctxOf(f);
}

static void teardownOnEngine(Fac* f) {
    {
        ENTER(f);
        // Class finalizers run for every instance still alive, as QuickJS does when the runtime is freed.
        std::vector<FinRec*> recs(f->finRecs.begin(), f->finRecs.end());
        for (FinRec* r : recs) finalizeRec(r);
        for (ModuleDef* m : f->modules) delete m;
        f->modules.clear();
        f->moduleByName.clear();
        clearPending(f);
        for (auto& g : f->vals) g.Reset();
        for (auto& g : f->atoms) g.Reset();
        f->names.clear();
        for (auto& c : f->classes) if (c) { c->tmpl.Reset(); c->proto.Reset(); c->handler.Reset(); }
        for (auto& n : f->natives) for (auto& g : n->data) g.Reset();
        f->proxyCtor.Reset();
        f->global.Reset();
        if (! f->ctx.IsEmpty()) {
            f->ctx.Get(f->iso)->Exit();
            f->ctx.Reset();
        }
    }
    f->iso->Exit();
    f->iso->Dispose();
    f->iso = nullptr;
    delete f->allocator;
    f->allocator = nullptr;
}

void JS_FreeContext(JSContext* ctx) {
    Fac* f = lookupCx(ctx);
    if (! f) return;
    if (getenv("OWE_JS_DEBUG")) fprintf(stderr, "[js] hops %lu, caller sleeps %lu, engine sleeps %lu, async ticks %lu, natives that waited for the gate %lu\n", (unsigned long)g_hops.load(), (unsigned long)g_hopSleeps.load(), (unsigned long)g_engineSleeps.load(), (unsigned long)g_asyncPosts.load(), (unsigned long)g_gateBlocked.load());
    hop<void>(f, [&] { teardownOnEngine(f); });
    stopEngineThread(f);
    if (pthread_equal(f->pt, pthread_self())) {
        pthread_detach(f->pt);
    } else {
        pthread_join(f->pt, nullptr);
    }
    {
        std::lock_guard<std::mutex> lock(g_mu);
        g_byCx.erase(ctx);
        g_byId[f->id] = nullptr;
    }
    delete f;
    g_live.fetch_sub(1);
}

void JS_FreeRuntime(JSRuntime*) {}

JSRuntime* JS_GetRuntime(JSContext* ctx) { return reinterpret_cast<JSRuntime*>(ctx); }

void JS_SetContextOpaque(JSContext* ctx, void* opaque) { F(ctx)->opaque = opaque; }
void* JS_GetContextOpaque(JSContext* ctx) { return F(ctx)->opaque; }

void impl_JS_SetMaxStackSize(JSRuntime* rt, size_t size) {
    Fac* f = reinterpret_cast<Fac*>(rt);
    // "unlimited" (0) is not available: the engine thread has 16 MiB, take most of it.
    if (size == 0 || size > (size_t(12) << 20)) size = size_t(12) << 20;
    f->stackQuota = size;
    f->iso->SetStackLimit(f->stackBase - size);
}

// --- values ---------------------------------------------------------------------------------------------------------

JSValue JS_DupValue(JSContext* ctx, JSValueConst v) {
    if (isBoxed(v)) ++F(ctx)->rc[slotOf(v)];
    return v;
}

void JS_FreeValue(JSContext* ctx, JSValue v) {
    if (isBoxed(v)) releaseV(F(ctx), v);
}

JSValue JS_GetGlobalObject(JSContext* ctx) {
    Fac* f = F(ctx);
    ENTER(f);
    return fromV8(f, f->global.Get(f->iso));
}

JSValue impl_JS_NewObject(JSContext* ctx) {
    Fac* f = F(ctx);
    ENTER(f);
    return fromV8(f, v8::Object::New(f->iso));
}

JSValue impl_JS_NewArray(JSContext* ctx) {
    Fac* f = F(ctx);
    ENTER(f);
    return fromV8(f, v8::Array::New(f->iso, 0));
}

static bool implIsArray(Fac* f, JSValueConst v) {
    if (JS_VALUE_GET_TAG(v) != JS_TAG_OBJECT) return false;
    ENTER(f);
    return toV8(f, v)->IsArray();
}

bool impl_JS_IsFunction(JSContext* ctx, JSValueConst v) {
    Fac* f = F(ctx);
    if (JS_VALUE_GET_TAG(v) != JS_TAG_OBJECT) return false;
    ENTER(f);
    return toV8(f, v)->IsFunction();
}

JSValue impl_JS_NewStringLen(JSContext* ctx, const char* s, size_t len) {
    Fac* f = F(ctx);
    ENTER(f);
    return fromV8(f, str(f, s, len));
}

const char* impl_JS_ToCStringLen2(JSContext* ctx, size_t* plen, JSValueConst val, bool /*cesu8*/) {
    Fac* f = F(ctx);
    ENTER(f);
    v8::TryCatch tc(f->iso);
    v8::Local<v8::String> s;
    v8::Local<v8::Value> v = toV8(f, val);
    if (v->IsString()) s = v.As<v8::String>();
    else if (v->IsSymbol()) s = v.As<v8::Symbol>()->Description(f->iso)->ToString(cxOf(f)).ToLocalChecked();
    else if (! v->ToString(cxOf(f)).ToLocal(&s)) { caught(f, tc); return nullptr; }
    v8::String::Utf8Value u(f->iso, s);
    size_t n = size_t(u.length());
    if (plen) *plen = n;
    char* out = static_cast<char*>(malloc(n + 1));
    memcpy(out, *u ? *u : "", n);
    out[n] = 0;
    return out;
}

void JS_FreeCString(JSContext*, const char* p) { free(const_cast<char*>(p)); }

int impl_JS_ToBool(JSContext* ctx, JSValueConst v) {
    Fac* f = F(ctx);
    ENTER(f);
    return toV8(f, v)->BooleanValue(f->iso) ? 1 : 0;
}

int impl_JS_ToFloat64(JSContext* ctx, double* pres, JSValueConst v) {
    switch (JS_VALUE_GET_TAG(v)) {
    case JS_TAG_INT: *pres = JS_VALUE_GET_INT(v); return 0;
    case JS_TAG_FLOAT64: *pres = JS_VALUE_GET_FLOAT64(v); return 0;
    default: break;
    }
    Fac* f = F(ctx);
    ENTER(f);
    v8::TryCatch tc(f->iso);
    double       d;
    if (! toV8(f, v)->NumberValue(cxOf(f)).To(&d)) { caught(f, tc); return -1; }
    *pres = d;
    return 0;
}

int impl_JS_ToInt32(JSContext* ctx, int32_t* pres, JSValueConst v) {
    if (JS_VALUE_GET_TAG(v) == JS_TAG_INT) {
        *pres = JS_VALUE_GET_INT(v);
        return 0;
    }
    Fac* f = F(ctx);
    ENTER(f);
    v8::TryCatch tc(f->iso);
    int32_t      i;
    if (! toV8(f, v)->Int32Value(cxOf(f)).To(&i)) { caught(f, tc); return -1; }
    *pres = i;
    return 0;
}

int impl_JS_ToInt64(JSContext* ctx, int64_t* pres, JSValueConst v) {
    if (JS_VALUE_GET_TAG(v) == JS_TAG_INT) {
        *pres = JS_VALUE_GET_INT(v);
        return 0;
    }
    Fac* f = F(ctx);
    ENTER(f);
    v8::TryCatch tc(f->iso);
    int64_t      i;
    if (! toV8(f, v)->IntegerValue(cxOf(f)).To(&i)) { caught(f, tc); return -1; }
    *pres = i;
    return 0;
}

// --- properties ----------------------------------------------------------------------------------------------------

static v8::PropertyAttribute attrsOf(int flags) {
    int a = v8::None;
    if (! (flags & JS_PROP_ENUMERABLE)) a |= v8::DontEnum;
    if (! (flags & JS_PROP_WRITABLE)) a |= v8::ReadOnly;
    if (! (flags & JS_PROP_CONFIGURABLE)) a |= v8::DontDelete;
    return v8::PropertyAttribute(a);
}

JSValue impl_JS_GetPropertyStr(JSContext* ctx, JSValueConst obj, const char* prop) {
    Fac* f = F(ctx);
    ENTER(f);
    v8::TryCatch         tc(f->iso);
    v8::Local<v8::Value> ov = toV8(f, obj);
    v8::Local<v8::Object> o;
    if (ov->IsObject()) o = ov.As<v8::Object>();
    else if (! ov->ToObject(cxOf(f)).ToLocal(&o)) return caught(f, tc);
    v8::Local<v8::Value> out;
    if (! o->Get(cxOf(f), nameOf(f, prop)).ToLocal(&out)) return caught(f, tc);
    return fromV8(f, out);
}

JSValue impl_JS_GetPropertyUint32(JSContext* ctx, JSValueConst obj, uint32_t idx) {
    Fac* f = F(ctx);
    ENTER(f);
    v8::Local<v8::Object> o = objOf(f, obj);
    if (o.IsEmpty()) return JS_UNDEFINED;
    v8::TryCatch         tc(f->iso);
    v8::Local<v8::Value> out;
    if (! o->Get(cxOf(f), idx).ToLocal(&out)) return caught(f, tc);
    return fromV8(f, out);
}

int impl_JS_SetPropertyStr(JSContext* ctx, JSValueConst obj, const char* prop, JSValue val) {
    Fac* f = F(ctx);
    ENTER(f);
    v8::Local<v8::Object> o = objOf(f, obj);
    v8::Local<v8::Value>  v = takeV8(f, val);
    if (o.IsEmpty()) return -1;
    v8::TryCatch tc(f->iso);
    bool         ok;
    if (! o->Set(cxOf(f), nameOf(f, prop), v).To(&ok)) { caught(f, tc); return -1; }
    return ok ? 1 : -1;
}

int impl_JS_SetPropertyUint32(JSContext* ctx, JSValueConst obj, uint32_t idx, JSValue val) {
    Fac* f = F(ctx);
    ENTER(f);
    v8::Local<v8::Object> o = objOf(f, obj);
    v8::Local<v8::Value>  v = takeV8(f, val);
    if (o.IsEmpty()) return -1;
    v8::TryCatch tc(f->iso);
    bool         ok;
    if (! o->Set(cxOf(f), idx, v).To(&ok)) { caught(f, tc); return -1; }
    return ok ? 1 : -1;
}

int impl_JS_DefinePropertyValueStr(JSContext* ctx, JSValueConst obj, const char* prop, JSValue val, int flags) {
    Fac* f = F(ctx);
    ENTER(f);
    v8::Local<v8::Object> o = objOf(f, obj);
    v8::Local<v8::Value>  v = takeV8(f, val);
    if (o.IsEmpty()) return -1;
    v8::TryCatch tc(f->iso);
    bool         ok;
    v8::PropertyDescriptor d(v, (flags & JS_PROP_WRITABLE) != 0);
    d.set_enumerable((flags & JS_PROP_ENUMERABLE) != 0);
    d.set_configurable((flags & JS_PROP_CONFIGURABLE) != 0);
    if (! o->DefineProperty(cxOf(f), nameOf(f, prop), d).To(&ok)) { caught(f, tc); return -1; }
    return ok ? 1 : -1;
}

int impl_JS_DefinePropertyValueUint32(JSContext* ctx, JSValueConst obj, uint32_t idx, JSValue val, int flags) {
    Fac* f = F(ctx);
    ENTER(f);
    v8::Local<v8::Object> o = objOf(f, obj);
    v8::Local<v8::Value>  v = takeV8(f, val);
    if (o.IsEmpty()) return -1;
    v8::TryCatch tc(f->iso);
    bool         ok;
    v8::PropertyDescriptor d(v, (flags & JS_PROP_WRITABLE) != 0);
    d.set_enumerable((flags & JS_PROP_ENUMERABLE) != 0);
    d.set_configurable((flags & JS_PROP_CONFIGURABLE) != 0);
    v8::Local<v8::Value> key = v8::Integer::NewFromUnsigned(f->iso, idx);
    if (! o->DefineProperty(cxOf(f), key->ToString(cxOf(f)).ToLocalChecked(), d).To(&ok)) { caught(f, tc); return -1; }
    return ok ? 1 : -1;
}

// --- atoms ---------------------------------------------------------------------------------------------------------

OweAtom impl_JS_NewAtom(JSContext* ctx, const char* str_) {
    Fac* f = F(ctx);
    ENTER(f);
    v8::Local<v8::String> s = v8::String::NewFromUtf8(f->iso, str_, v8::NewStringType::kInternalized).ToLocalChecked();
    return OweAtom(atomOfName(f, s) + 1);
}

void impl_JS_FreeAtom(JSContext* ctx, OweAtom a) {
    if (a == 0) return;
    F(ctx)->freeAtoms.push_back(a - 1);
}

const char* impl_JS_AtomToCStringLen(JSContext* ctx, size_t* plen, OweAtom atom) {
    Fac* f = F(ctx);
    if (atom == 0) return nullptr;
    ENTER(f);
    v8::Local<v8::Name> n = f->atoms[atom - 1].Get(f->iso);
    v8::Local<v8::String> s = n->IsString() ? n.As<v8::String>() : n.As<v8::Symbol>()->Description(f->iso)->ToString(cxOf(f)).ToLocalChecked();
    v8::String::Utf8Value u(f->iso, s);
    size_t len = size_t(u.length());
    if (plen) *plen = len;
    char* out = static_cast<char*>(malloc(len + 1));
    memcpy(out, *u ? *u : "", len);
    out[len] = 0;
    return out;
}

int impl_JS_DefinePropertyGetSet(JSContext* ctx, JSValueConst obj, OweAtom prop, JSValue getter, JSValue setter, int flags) {
    Fac* f = F(ctx);
    ENTER(f);
    v8::Local<v8::Object> o  = objOf(f, obj);
    v8::Local<v8::Value>  gv = takeV8(f, getter), sv = takeV8(f, setter);
    if (o.IsEmpty()) return -1;
    v8::Local<v8::Function> g = gv->IsFunction() ? gv.As<v8::Function>() : v8::Local<v8::Function>();
    v8::Local<v8::Function> s = sv->IsFunction() ? sv.As<v8::Function>() : v8::Local<v8::Function>();
    v8::Local<v8::Name>     name = f->atoms[prop - 1].Get(f->iso);
    v8::PropertyDescriptor  d(g.IsEmpty() ? v8::Undefined(f->iso).As<v8::Value>() : g.As<v8::Value>(),
                              s.IsEmpty() ? v8::Undefined(f->iso).As<v8::Value>() : s.As<v8::Value>());
    d.set_enumerable((flags & JS_PROP_ENUMERABLE) != 0);
    d.set_configurable((flags & JS_PROP_CONFIGURABLE) != 0);
    v8::TryCatch tc(f->iso);
    bool         ok;
    if (! o->DefineProperty(cxOf(f), name, d).To(&ok)) { caught(f, tc); return -1; }
    return ok ? 1 : -1;
}

// --- functions, calls ----------------------------------------------------------------------------------------------

JSValue impl_JS_NewCFunction2(JSContext* ctx, JSCFunction* func, const char* name, int length, JSCFunctionEnum, int magic) {
    Fac* f = F(ctx);
    ENTER(f);
    v8::Local<v8::Function> fn = newNative(f, reinterpret_cast<void*>(func), kGeneric, magic, name, length);
    return fn.IsEmpty() ? JS_EXCEPTION : fromV8(f, fn);
}

JSValue impl_JS_NewCFunctionData(JSContext* ctx, JSCFunctionData* func, int length, int magic, int data_len, JSValueConst* data) {
    Fac* f = F(ctx);
    ENTER(f);
    v8::Local<v8::Function> fn = newNative(f, reinterpret_cast<void*>(func), kData, magic, "", length);
    if (fn.IsEmpty()) return JS_EXCEPTION;
    NativeRec* rec = f->natives.back().get();
    for (int i = 0; i < data_len; ++i) rec->data.emplace_back(f->iso, toV8(f, data[i]));
    return fromV8(f, fn);
}

JSValue impl_JS_Call(JSContext* ctx, JSValueConst func, JSValueConst thisv, int argc, JSValueConst* argv) {
    Fac* f = F(ctx);
    ENTER(f);
    f->jobsMayExist = true;
    v8::Local<v8::Value> fv = toV8(f, func);
    if (! fv->IsFunction()) return JS_ThrowTypeError(ctx, "not a function");
    std::vector<v8::Local<v8::Value>> args(argc);
    for (int i = 0; i < argc; ++i) args[i] = toV8(f, argv[i]);
    v8::TryCatch         tc(f->iso);
    v8::Local<v8::Value> out;
    if (! fv.As<v8::Function>()->Call(cxOf(f), toV8(f, thisv), argc, args.data()).ToLocal(&out)) return caught(f, tc);
    return fromV8(f, out);
}

JSValue impl_JS_CallConstructor(JSContext* ctx, JSValueConst func, int argc, JSValueConst* argv) {
    Fac* f = F(ctx);
    ENTER(f);
    f->jobsMayExist = true;
    v8::Local<v8::Value> fv = toV8(f, func);
    if (! fv->IsFunction()) return JS_ThrowTypeError(ctx, "not a constructor");
    std::vector<v8::Local<v8::Value>> args(argc);
    for (int i = 0; i < argc; ++i) args[i] = toV8(f, argv[i]);
    v8::TryCatch          tc(f->iso);
    v8::Local<v8::Object> out;
    if (! fv.As<v8::Function>()->NewInstance(cxOf(f), argc, args.data()).ToLocal(&out)) return caught(f, tc);
    return fromV8(f, out);
}

int impl_JS_SetPropertyFunctionList(JSContext* ctx, JSValueConst obj, const JSCFunctionListEntry* tab, int len) {
    Fac* f = F(ctx);
    ENTER(f);
    v8::Local<v8::Object> o = objOf(f, obj);
    if (o.IsEmpty()) return -1;
    v8::Local<v8::Context> cx = cxOf(f);
    for (int i = 0; i < len; ++i) {
        const JSCFunctionListEntry& e = tab[i];
        v8::Local<v8::Name> name = v8::String::NewFromUtf8(f->iso, e.name, v8::NewStringType::kInternalized).ToLocalChecked();
        auto define = [&](v8::Local<v8::Value> v) {
            v8::PropertyDescriptor d(v, (e.prop_flags & JS_PROP_WRITABLE) != 0);
            d.set_enumerable((e.prop_flags & JS_PROP_ENUMERABLE) != 0);
            d.set_configurable((e.prop_flags & JS_PROP_CONFIGURABLE) != 0);
            bool ok;
            return o->DefineProperty(cx, name, d).To(&ok) && ok;
        };
        switch (e.def_type) {
        case JS_DEF_CFUNC: {
            v8::Local<v8::Function> fn = newNative(f, reinterpret_cast<void*>(e.u.func.cfunc.generic), kGeneric, e.magic, e.name, e.u.func.length);
            if (fn.IsEmpty() || ! define(fn)) return -1;
            break;
        }
        case JS_DEF_CGETSET:
        case JS_DEF_CGETSET_MAGIC: {
            bool magic = e.def_type == JS_DEF_CGETSET_MAGIC;
            v8::Local<v8::Value> g = v8::Undefined(f->iso), s = v8::Undefined(f->iso);
            if (e.u.getset.get.generic) {
                auto gf = newNative(f, reinterpret_cast<void*>(e.u.getset.get.generic), magic ? kGetterMagic : kGetter, e.magic, e.name, 0);
                if (gf.IsEmpty()) return -1;
                g = gf;
            }
            if (e.u.getset.set.generic) {
                auto sf = newNative(f, reinterpret_cast<void*>(e.u.getset.set.generic), magic ? kSetterMagic : kSetter, e.magic, e.name, 1);
                if (sf.IsEmpty()) return -1;
                s = sf;
            }
            v8::PropertyDescriptor d(g, s);
            d.set_enumerable((e.prop_flags & JS_PROP_ENUMERABLE) != 0);
            d.set_configurable((e.prop_flags & JS_PROP_CONFIGURABLE) != 0);
            bool ok;
            if (! o->DefineProperty(cx, name, d).To(&ok) || ! ok) return -1;
            break;
        }
        case JS_DEF_PROP_STRING:
            if (! define(str(f, e.u.str))) return -1;
            break;
        case JS_DEF_PROP_INT32:
            if (! define(v8::Integer::New(f->iso, e.u.i32))) return -1;
            break;
        case JS_DEF_PROP_INT64:
            if (! define(v8::Number::New(f->iso, double(e.u.i64)))) return -1;
            break;
        case JS_DEF_PROP_DOUBLE:
            if (! define(v8::Number::New(f->iso, e.u.f64))) return -1;
            break;
        case JS_DEF_PROP_UNDEFINED:
            if (! define(v8::Undefined(f->iso))) return -1;
            break;
        default: setPending(f, v8::Exception::Error(str(f, "unsupported JSCFunctionListEntry type"))); return -1;
        }
    }
    return 0;
}

// --- classes -------------------------------------------------------------------------------------------------------

JSClassID JS_NewClassID(JSRuntime*, JSClassID* pid) {
    if (*pid == 0) *pid = g_nextClassId.fetch_add(1);
    return *pid;
}

int impl_JS_NewClass(JSRuntime* rt, JSClassID id, const JSClassDef* def) {
    Fac* f = reinterpret_cast<Fac*>(rt);
    if (id >= f->classes.size()) f->classes.resize(id + 1);
    auto rec = std::make_unique<ClassRec>();
    rec->def = *def;
    rec->id  = id;
    f->classes[id] = std::move(rec);
    return 0;
}

void impl_JS_SetClassProto(JSContext* ctx, JSClassID id, JSValue obj) {
    Fac* f = F(ctx);
    ENTER(f);
    v8::Local<v8::Value> v = takeV8(f, obj);
    ClassRec* rec = classOf(f, id);
    if (rec && v->IsObject()) rec->proto.Reset(f->iso, v.As<v8::Object>());
}

static void weakFinalize(const v8::WeakCallbackInfo<FinRec>& data) {
    FinRec* r = data.GetParameter();
    r->self.Reset();
    data.SetSecondPassCallback([](const v8::WeakCallbackInfo<FinRec>& d) { finalizeRec(d.GetParameter()); });
}

JSValue impl_JS_NewObjectClass(JSContext* ctx, JSClassID id) {
    Fac*      f   = F(ctx);
    ENTER(f);
    ClassRec* rec = classOf(f, id);
    if (! rec) return JS_ThrowTypeError(ctx, "unknown class id %d", int(id));
    if (rec->tmpl.IsEmpty()) {
        v8::Local<v8::ObjectTemplate> t = v8::ObjectTemplate::New(f->iso);
        t->SetInternalFieldCount(1);
        rec->tmpl.Reset(f->iso, t);
    }
    v8::Local<v8::Object> o;
    if (! rec->tmpl.Get(f->iso)->NewInstance(cxOf(f)).ToLocal(&o)) return JS_EXCEPTION;
    if (! rec->proto.IsEmpty()) (void)o->SetPrototype(cxOf(f), rec->proto.Get(f->iso));
    auto* fr = new FinRec();
    fr->f   = f;
    fr->cls = rec;
    fr->self.Reset(f->iso, o);
    fr->self.SetWeak(fr, weakFinalize, v8::WeakCallbackType::kParameter);
    f->finRecs.insert(fr);
    o->SetAlignedPointerInInternalField(0, fr);
    if (rec->def.exotic && rec->def.exotic->set_property && ! f->proxyCtor.IsEmpty()) {
        if (rec->handler.IsEmpty()) {
            v8::Local<v8::Object>   handler = v8::Object::New(f->iso);
            v8::Local<v8::Function> trap;
            if (! v8::Function::New(cxOf(f), proxySetTrap, v8::External::New(f->iso, rec), 4).ToLocal(&trap)) return JS_EXCEPTION;
            (void)handler->Set(cxOf(f), str(f, "set"), trap);
            rec->handler.Reset(f->iso, handler);
        }
        v8::Local<v8::Proxy> proxy;
        if (! v8::Proxy::New(cxOf(f), o, rec->handler.Get(f->iso)).ToLocal(&proxy)) return JS_EXCEPTION;
        return fromV8(f, proxy);
    }
    return fromV8(f, o);
}

static int implSetOpaque(Fac* f, JSValueConst obj, void* opaque) {
    ENTER(f);
    v8::Local<v8::Object> o = objOf(f, obj);
    if (o.IsEmpty()) return -1;
    FinRec* r = finOf(o);
    if (! r) return -1;
    r->opaque = opaque;
    return 0;
}

static void* implGetOpaque(Fac* f, JSValueConst obj, JSClassID id) {
    int t = JS_VALUE_GET_TAG(obj);
    if (t == kTagRawObject) {
        auto* r = static_cast<FinRec*>(JS_VALUE_GET_PTR(obj));
        return (r->cls && r->cls->id == id) ? r->opaque : nullptr;
    }
    if (t != JS_TAG_OBJECT) return nullptr;
    ENTER(f);
    v8::Local<v8::Object> o = objOf(f, obj);
    if (o.IsEmpty()) return nullptr;
    FinRec* r = finOf(o);
    return (r && r->cls && r->cls->id == id) ? r->opaque : nullptr;
}

// --- exceptions ----------------------------------------------------------------------------------------------------

static Fac* facOfValue(JSValueConst v) {
    if (Fac* f = tl_cur) return f;
    return isBoxed(v) ? lookupId(idOf(v)) : nullptr;
}
bool JS_IsArray(JSValueConst v) {
    Fac* f = facOfValue(v);
    return f && hop<bool>(f, [&] { return implIsArray(f, v); });
}
int JS_SetOpaque(JSValueConst obj, void* opaque) {
    Fac* f = facOfValue(obj);
    return f ? hop<int>(f, [&] { return implSetOpaque(f, obj, opaque); }) : -1;
}
void* JS_GetOpaque(JSValueConst obj, JSClassID id) {
    if (JS_VALUE_GET_TAG(obj) == kTagRawObject) return implGetOpaque(nullptr, obj, id);  // finalizer: no engine access
    Fac* f = facOfValue(obj);
    return f ? hop<void*>(f, [&] { return implGetOpaque(f, obj, id); }) : nullptr;
}

JSValue JS_ThrowTypeError(JSContext* ctx, const char* fmt, ...) {
    va_list ap; va_start(ap, fmt); JSValue r = throwFmt(ctx, "TypeError", fmt, ap); va_end(ap); return r;
}
JSValue JS_ThrowReferenceError(JSContext* ctx, const char* fmt, ...) {
    va_list ap; va_start(ap, fmt); JSValue r = throwFmt(ctx, "ReferenceError", fmt, ap); va_end(ap); return r;
}
JSValue JS_ThrowRangeError(JSContext* ctx, const char* fmt, ...) {
    va_list ap; va_start(ap, fmt); JSValue r = throwFmt(ctx, "RangeError", fmt, ap); va_end(ap); return r;
}
JSValue JS_ThrowInternalError(JSContext* ctx, const char* fmt, ...) {
    va_list ap; va_start(ap, fmt); JSValue r = throwFmt(ctx, "InternalError", fmt, ap); va_end(ap); return r;
}
JSValue JS_ThrowSyntaxError(JSContext* ctx, const char* fmt, ...) {
    va_list ap; va_start(ap, fmt); JSValue r = throwFmt(ctx, "SyntaxError", fmt, ap); va_end(ap); return r;
}

JSValue impl_JS_Throw(JSContext* ctx, JSValue obj) {
    Fac* f = F(ctx);
    ENTER(f);
    setPending(f, takeV8(f, obj));
    return JS_EXCEPTION;
}

JSValue impl_JS_GetException(JSContext* ctx) {
    Fac* f = F(ctx);
    ENTER(f);
    if (! f->hasPending) return JS_NULL;
    v8::Local<v8::Value> v = f->pending.Get(f->iso);
    clearPending(f);
    if (getenv("OWE_JS_DEBUG")) {
        v8::TryCatch tc(f->iso);
        v8::Local<v8::String> s;
        if (v->ToString(cxOf(f)).ToLocal(&s)) {
            v8::String::Utf8Value u(f->iso, s);
            fprintf(stderr, "[js] exception: %s\n", *u ? *u : "<unprintable>");
        }
    }
    return fromV8(f, v);
}

// --- JSON, array buffers --------------------------------------------------------------------------------------------

JSValue impl_JS_ParseJSON(JSContext* ctx, const char* buf, size_t len, const char* /*filename*/) {
    Fac* f = F(ctx);
    ENTER(f);
    v8::TryCatch         tc(f->iso);
    v8::Local<v8::Value> out;
    if (! v8::JSON::Parse(cxOf(f), str(f, buf, len)).ToLocal(&out)) return caught(f, tc);
    return fromV8(f, out);
}

JSValue impl_JS_JSONStringify(JSContext* ctx, JSValueConst obj, JSValueConst replacer, JSValueConst space) {
    Fac* f = F(ctx);
    ENTER(f);
    v8::TryCatch         tc(f->iso);
    v8::Local<v8::Value> json, fn, out;
    if (! f->global.Get(f->iso)->Get(cxOf(f), str(f, "JSON")).ToLocal(&json) || ! json->IsObject()) return caught(f, tc);
    if (! json.As<v8::Object>()->Get(cxOf(f), str(f, "stringify")).ToLocal(&fn) || ! fn->IsFunction()) return caught(f, tc);
    v8::Local<v8::Value> args[3] = { toV8(f, obj), toV8(f, replacer), toV8(f, space) };
    if (! fn.As<v8::Function>()->Call(cxOf(f), json, 3, args).ToLocal(&out)) return caught(f, tc);
    return fromV8(f, out);
}

JSValue impl_JS_GetTypedArrayBuffer(JSContext* ctx, JSValueConst obj, size_t* poff, size_t* plen, size_t* pbpe) {
    Fac* f = F(ctx);
    ENTER(f);
    v8::Local<v8::Object> o = objOf(f, obj);
    if (o.IsEmpty() || ! o->IsArrayBufferView()) return JS_ThrowTypeError(ctx, "not a TypedArray");
    auto view = o.As<v8::ArrayBufferView>();
    if (poff) *poff = view->ByteOffset();
    if (plen) *plen = view->ByteLength();
    if (pbpe) {
        size_t bpe = 1;
        if (o->IsTypedArray()) {
            size_t n = o.As<v8::TypedArray>()->Length();
            bpe      = n ? view->ByteLength() / n : 1;
        }
        *pbpe = bpe ? bpe : 1;
    }
    return fromV8(f, view->Buffer());
}

uint8_t* impl_JS_GetArrayBuffer(JSContext* ctx, size_t* psize, JSValueConst obj) {
    Fac* f = F(ctx);
    ENTER(f);
    v8::Local<v8::Object> o = objOf(f, obj);
    if (o.IsEmpty() || ! o->IsArrayBuffer()) {
        JS_ThrowTypeError(ctx, "not an ArrayBuffer");
        return nullptr;
    }
    auto ab = o.As<v8::ArrayBuffer>();
    if (psize) *psize = ab->ByteLength();
    return static_cast<uint8_t*>(ab->Data());
}

// --- promises, jobs ------------------------------------------------------------------------------------------------

JSPromiseStateEnum impl_JS_PromiseState(JSContext* ctx, JSValueConst promise) {
    Fac* f = F(ctx);
    ENTER(f);
    v8::Local<v8::Object> o = objOf(f, promise);
    if (o.IsEmpty() || ! o->IsPromise()) return JS_PROMISE_NOT_A_PROMISE;
    switch (o.As<v8::Promise>()->State()) {
    case v8::Promise::kPending: return JS_PROMISE_PENDING;
    case v8::Promise::kFulfilled: return JS_PROMISE_FULFILLED;
    default: return JS_PROMISE_REJECTED;
    }
}

JSValue impl_JS_PromiseResult(JSContext* ctx, JSValueConst promise) {
    Fac* f = F(ctx);
    ENTER(f);
    v8::Local<v8::Object> o = objOf(f, promise);
    if (o.IsEmpty() || ! o->IsPromise()) return JS_UNDEFINED;
    return fromV8(f, o.As<v8::Promise>()->Result());
}

int impl_JS_ExecutePendingJob(JSRuntime* rt, JSContext** pctx) {
    Fac* f = reinterpret_cast<Fac*>(rt);
    if (pctx) *pctx = ctxOf(f);
    ENTER(f);
    // V8 drains the whole microtask queue at once and does not say whether it ran anything: report "ran" once after
    // anything that could have queued a job, then "nothing" until more JS has run.
    if (! f->jobsMayExist) return 0;
    f->jobsMayExist = false;
    v8::TryCatch tc(f->iso);
    f->iso->PerformMicrotaskCheckpoint();
    return 1;
}

// --- eval, modules -------------------------------------------------------------------------------------------------

static v8::MaybeLocal<v8::Module> resolveHook(v8::Local<v8::Context>, v8::Local<v8::String> specifier,
                                              v8::Local<v8::FixedArray>, v8::Local<v8::Module>) {
    Fac* f = tl_cur;
    v8::String::Utf8Value name(f->iso, specifier);
    auto it = f->moduleByName.find(*name);
    if (it != f->moduleByName.end()) return it->second->module.Get(f->iso);
    if (! f->loader) {
        f->iso->ThrowException(v8::Exception::ReferenceError(str(f, "no module loader")));
        return {};
    }
    JSModuleDef* def = f->loader(ctxOf(f), *name, f->loaderOpaque);
    if (! def) {
        if (f->hasPending) {
            v8::Local<v8::Value> e = f->pending.Get(f->iso);
            clearPending(f);
            f->iso->ThrowException(e);
        } else {
            f->iso->ThrowException(v8::Exception::ReferenceError(str(f, (std::string("could not load module '") + *name + "'").c_str())));
        }
        return {};
    }
    auto* md = reinterpret_cast<ModuleDef*>(def);
    f->moduleByName[*name] = md;
    return md->module.Get(f->iso);
}

void impl_JS_SetModuleLoaderFunc(JSRuntime* rt, JSModuleNormalizeFunc*, JSModuleLoaderFunc* loader, void* opaque) {
    Fac* f = reinterpret_cast<Fac*>(rt);
    f->loader       = loader;
    f->loaderOpaque = opaque;
}

JSValue impl_JS_Eval(JSContext* ctx, const char* input, size_t len, const char* filename, int flags) {
    Fac* f = F(ctx);
    ENTER(f);
    f->jobsMayExist = true;
    v8::TryCatch         tc(f->iso);
    const bool           module = (flags & JS_EVAL_TYPE_MASK) == JS_EVAL_TYPE_MODULE;
    v8::ScriptOrigin     origin(str(f, filename ? filename : "<eval>"), 0, 0, false, -1, v8::Local<v8::Value>(), false, false, module);
    v8::ScriptCompiler::Source src(str(f, input, len), origin);
    if (! module) {
        v8::Local<v8::Script> script;
        if (! v8::ScriptCompiler::Compile(cxOf(f), &src).ToLocal(&script)) return caught(f, tc);
        v8::Local<v8::Value> rv;
        if (! script->Run(cxOf(f)).ToLocal(&rv)) return caught(f, tc);
        return fromV8(f, rv);
    }
    v8::Local<v8::Module> mod;
    if (! v8::ScriptCompiler::CompileModule(f->iso, &src).ToLocal(&mod)) return caught(f, tc);
    auto* def   = new ModuleDef();
    def->module.Reset(f->iso, mod);
    def->name   = filename ? filename : "";
    f->modules.push_back(def);
    JSValue mv = JS_MKPTR(JS_TAG_MODULE, def);
    if (flags & JS_EVAL_FLAG_COMPILE_ONLY) return mv;
    return JS_EvalFunction(ctx, mv);
}

JSValue impl_JS_EvalFunction(JSContext* ctx, JSValue fun) {
    Fac* f = F(ctx);
    if (JS_VALUE_GET_TAG(fun) != JS_TAG_MODULE) return fun;
    ENTER(f);
    f->jobsMayExist = true;
    auto*                 def = static_cast<ModuleDef*>(JS_VALUE_GET_PTR(fun));
    v8::Local<v8::Module> mod = def->module.Get(f->iso);
    v8::TryCatch          tc(f->iso);
    if (mod->GetStatus() == v8::Module::kUninstantiated) {
        bool ok;
        if (! mod->InstantiateModule(cxOf(f), resolveHook).To(&ok) || ! ok) return caught(f, tc);
    }
    v8::Local<v8::Value> rv;
    if (! mod->Evaluate(cxOf(f)).ToLocal(&rv)) return caught(f, tc);
    return fromV8(f, rv);
}

JSValue impl_JS_GetModuleNamespace(JSContext* ctx, JSModuleDef* m) {
    Fac* f = F(ctx);
    ENTER(f);
    auto* def = reinterpret_cast<ModuleDef*>(m);
    return fromV8(f, def->module.Get(f->iso)->GetModuleNamespace());
}

}  // extern "C"

// For Script.cpp: run fn(arg) on the runtime's engine thread (see OnEngine there).
extern "C" void owe_js_run_on_engine(JSRuntime* rt, void (*fn)(void*), void* arg) {
    Fac* f = lookupCx(reinterpret_cast<JSContext*>(rt));
    if (! f) return fn(arg);
    hop<void>(f, [&] { fn(arg); });
}

// Oracle only: post a job without waiting. Any later engine call from the host waits for it first.
extern "C" void owe_js_post_on_engine(JSRuntime* rt, void (*fn)(void*), void* arg) {
    Fac* f = lookupCx(reinterpret_cast<JSContext*>(rt));
    if (! f || onEngine(f)) return fn(arg);
    std::lock_guard<std::mutex> submit(f->submitMu);
    if (f->asyncPending) collectAsync(f);
    g_asyncPosts.fetch_add(1, std::memory_order_relaxed);
    f->job = [fn, arg] { fn(arg); };
    f->asyncPending = true;
    f->gateActive.store(f->gateLock != nullptr, std::memory_order_release);
    if (f->gateNotify && f->gateLock) f->gateNotify(f->gateArg, true);
    f->state.store(1, std::memory_order_release);
    f->state.notify_one();
}

extern "C" void owe_js_gate_exempt(JSRuntime* rt, void* fn) {
    Fac* f = lookupCx(reinterpret_cast<JSContext*>(rt));
    if (f) hop<void>(f, [&] { f->gateExempt.insert(fn); });
}

// Installs the scene gate (all four functions or none).
extern "C" void owe_js_set_gate(JSRuntime* rt, void (*lock)(void*), void (*unlock)(void*), void (*release_all)(void*),
                                void (*notify)(void*, bool), void* arg) {
    Fac* f = lookupCx(reinterpret_cast<JSContext*>(rt));
    if (! f) return;
    f->gateLock = lock; f->gateUnlock = unlock; f->gateReleaseAll = release_all; f->gateNotify = notify; f->gateArg = arg;
}
extern "C" void owe_js_sync_engine(JSRuntime* rt) {
    Fac* f = lookupCx(reinterpret_cast<JSContext*>(rt));
    if (! f || onEngine(f)) return;
    std::lock_guard<std::mutex> submit(f->submitMu);
    if (f->asyncPending) collectAsync(f);
}

// ---- host-facing wrappers: forward to the runtime's engine thread ----------------------------------------------------
extern "C" {
JSContext* JS_NewContext(JSRuntime* rt) {
    Fac* f = lookupCx(reinterpret_cast<JSContext*>(rt));
    return f ? hop<JSContext*>(f, [&] { return impl_JS_NewContext(rt); }) : nullptr;
}
void JS_SetMaxStackSize(JSRuntime* rt, size_t size) {
    Fac* f = lookupCx(reinterpret_cast<JSContext*>(rt));
    noteCaller(__builtin_return_address(0), f);
    return hop<void>(f, [&]() -> void { return impl_JS_SetMaxStackSize(rt, size); });
}
JSValue JS_NewObject(JSContext* ctx) {
    Fac* f = F(ctx);
    noteCaller(__builtin_return_address(0), f);
    return hop<JSValue>(f, [&]() -> JSValue { return impl_JS_NewObject(ctx); });
}
JSValue JS_NewArray(JSContext* ctx) {
    Fac* f = F(ctx);
    noteCaller(__builtin_return_address(0), f);
    return hop<JSValue>(f, [&]() -> JSValue { return impl_JS_NewArray(ctx); });
}
bool JS_IsFunction(JSContext* ctx, JSValueConst v) {
    Fac* f = F(ctx);
    noteCaller(__builtin_return_address(0), f);
    return hop<bool>(f, [&]() -> bool { return impl_JS_IsFunction(ctx, v); });
}
JSValue JS_NewStringLen(JSContext* ctx, const char* s, size_t len) {
    Fac* f = F(ctx);
    noteCaller(__builtin_return_address(0), f);
    return hop<JSValue>(f, [&]() -> JSValue { return impl_JS_NewStringLen(ctx, s, len); });
}
const char* JS_ToCStringLen2(JSContext* ctx, size_t* plen, JSValueConst val, bool arg3) {
    Fac* f = F(ctx);
    noteCaller(__builtin_return_address(0), f);
    return hop<const char*>(f, [&]() -> const char* { return impl_JS_ToCStringLen2(ctx, plen, val, arg3); });
}
int JS_ToBool(JSContext* ctx, JSValueConst v) {
    Fac* f = F(ctx);
    noteCaller(__builtin_return_address(0), f);
    return hop<int>(f, [&]() -> int { return impl_JS_ToBool(ctx, v); });
}
int JS_ToFloat64(JSContext* ctx, double* pres, JSValueConst v) {
    Fac* f = F(ctx);
    noteCaller(__builtin_return_address(0), f);
    return hop<int>(f, [&]() -> int { return impl_JS_ToFloat64(ctx, pres, v); });
}
int JS_ToInt32(JSContext* ctx, int32_t* pres, JSValueConst v) {
    Fac* f = F(ctx);
    noteCaller(__builtin_return_address(0), f);
    return hop<int>(f, [&]() -> int { return impl_JS_ToInt32(ctx, pres, v); });
}
int JS_ToInt64(JSContext* ctx, int64_t* pres, JSValueConst v) {
    Fac* f = F(ctx);
    noteCaller(__builtin_return_address(0), f);
    return hop<int>(f, [&]() -> int { return impl_JS_ToInt64(ctx, pres, v); });
}
JSValue JS_GetPropertyStr(JSContext* ctx, JSValueConst obj, const char* prop) {
    Fac* f = F(ctx);
    noteCaller(__builtin_return_address(0), f);
    return hop<JSValue>(f, [&]() -> JSValue { return impl_JS_GetPropertyStr(ctx, obj, prop); });
}
JSValue JS_GetPropertyUint32(JSContext* ctx, JSValueConst obj, uint32_t idx) {
    Fac* f = F(ctx);
    noteCaller(__builtin_return_address(0), f);
    return hop<JSValue>(f, [&]() -> JSValue { return impl_JS_GetPropertyUint32(ctx, obj, idx); });
}
int JS_SetPropertyStr(JSContext* ctx, JSValueConst obj, const char* prop, JSValue val) {
    Fac* f = F(ctx);
    noteCaller(__builtin_return_address(0), f);
    return hop<int>(f, [&]() -> int { return impl_JS_SetPropertyStr(ctx, obj, prop, val); });
}
int JS_SetPropertyUint32(JSContext* ctx, JSValueConst obj, uint32_t idx, JSValue val) {
    Fac* f = F(ctx);
    noteCaller(__builtin_return_address(0), f);
    return hop<int>(f, [&]() -> int { return impl_JS_SetPropertyUint32(ctx, obj, idx, val); });
}
int JS_DefinePropertyValueStr(JSContext* ctx, JSValueConst obj, const char* prop, JSValue val, int flags) {
    Fac* f = F(ctx);
    noteCaller(__builtin_return_address(0), f);
    return hop<int>(f, [&]() -> int { return impl_JS_DefinePropertyValueStr(ctx, obj, prop, val, flags); });
}
int JS_DefinePropertyValueUint32(JSContext* ctx, JSValueConst obj, uint32_t idx, JSValue val, int flags) {
    Fac* f = F(ctx);
    noteCaller(__builtin_return_address(0), f);
    return hop<int>(f, [&]() -> int { return impl_JS_DefinePropertyValueUint32(ctx, obj, idx, val, flags); });
}
OweAtom JS_NewAtom(JSContext* ctx, const char* str) {
    Fac* f = F(ctx);
    noteCaller(__builtin_return_address(0), f);
    return hop<OweAtom>(f, [&]() -> OweAtom { return impl_JS_NewAtom(ctx, str); });
}
void JS_FreeAtom(JSContext* ctx, OweAtom a) {
    Fac* f = F(ctx);
    noteCaller(__builtin_return_address(0), f);
    return hop<void>(f, [&]() -> void { return impl_JS_FreeAtom(ctx, a); });
}
const char* JS_AtomToCStringLen(JSContext* ctx, size_t* plen, OweAtom atom) {
    Fac* f = F(ctx);
    noteCaller(__builtin_return_address(0), f);
    return hop<const char*>(f, [&]() -> const char* { return impl_JS_AtomToCStringLen(ctx, plen, atom); });
}
int JS_DefinePropertyGetSet(JSContext* ctx, JSValueConst obj, OweAtom prop, JSValue getter, JSValue setter, int flags) {
    Fac* f = F(ctx);
    noteCaller(__builtin_return_address(0), f);
    return hop<int>(f, [&]() -> int { return impl_JS_DefinePropertyGetSet(ctx, obj, prop, getter, setter, flags); });
}
JSValue JS_NewCFunction2(JSContext* ctx, JSCFunction* func, const char* name, int length, JSCFunctionEnum arg4, int magic) {
    Fac* f = F(ctx);
    noteCaller(__builtin_return_address(0), f);
    return hop<JSValue>(f, [&]() -> JSValue { return impl_JS_NewCFunction2(ctx, func, name, length, arg4, magic); });
}
JSValue JS_NewCFunctionData(JSContext* ctx, JSCFunctionData* func, int length, int magic, int data_len, JSValueConst* data) {
    Fac* f = F(ctx);
    noteCaller(__builtin_return_address(0), f);
    return hop<JSValue>(f, [&]() -> JSValue { return impl_JS_NewCFunctionData(ctx, func, length, magic, data_len, data); });
}
JSValue JS_Call(JSContext* ctx, JSValueConst func, JSValueConst thisv, int argc, JSValueConst* argv) {
    Fac* f = F(ctx);
    noteCaller(__builtin_return_address(0), f);
    return hop<JSValue>(f, [&]() -> JSValue { return impl_JS_Call(ctx, func, thisv, argc, argv); });
}
JSValue JS_CallConstructor(JSContext* ctx, JSValueConst func, int argc, JSValueConst* argv) {
    Fac* f = F(ctx);
    noteCaller(__builtin_return_address(0), f);
    return hop<JSValue>(f, [&]() -> JSValue { return impl_JS_CallConstructor(ctx, func, argc, argv); });
}
int JS_SetPropertyFunctionList(JSContext* ctx, JSValueConst obj, const JSCFunctionListEntry* tab, int len) {
    Fac* f = F(ctx);
    noteCaller(__builtin_return_address(0), f);
    return hop<int>(f, [&]() -> int { return impl_JS_SetPropertyFunctionList(ctx, obj, tab, len); });
}
int JS_NewClass(JSRuntime* rt, JSClassID id, const JSClassDef* def) {
    Fac* f = lookupCx(reinterpret_cast<JSContext*>(rt));
    noteCaller(__builtin_return_address(0), f);
    return hop<int>(f, [&]() -> int { return impl_JS_NewClass(rt, id, def); });
}
void JS_SetClassProto(JSContext* ctx, JSClassID id, JSValue obj) {
    Fac* f = F(ctx);
    noteCaller(__builtin_return_address(0), f);
    return hop<void>(f, [&]() -> void { return impl_JS_SetClassProto(ctx, id, obj); });
}
JSValue JS_NewObjectClass(JSContext* ctx, JSClassID id) {
    Fac* f = F(ctx);
    noteCaller(__builtin_return_address(0), f);
    return hop<JSValue>(f, [&]() -> JSValue { return impl_JS_NewObjectClass(ctx, id); });
}
JSValue JS_Throw(JSContext* ctx, JSValue obj) {
    Fac* f = F(ctx);
    noteCaller(__builtin_return_address(0), f);
    return hop<JSValue>(f, [&]() -> JSValue { return impl_JS_Throw(ctx, obj); });
}
JSValue JS_GetException(JSContext* ctx) {
    Fac* f = F(ctx);
    noteCaller(__builtin_return_address(0), f);
    return hop<JSValue>(f, [&]() -> JSValue { return impl_JS_GetException(ctx); });
}
JSValue JS_ParseJSON(JSContext* ctx, const char* buf, size_t len, const char* arg3) {
    Fac* f = F(ctx);
    noteCaller(__builtin_return_address(0), f);
    return hop<JSValue>(f, [&]() -> JSValue { return impl_JS_ParseJSON(ctx, buf, len, arg3); });
}
JSValue JS_JSONStringify(JSContext* ctx, JSValueConst obj, JSValueConst replacer, JSValueConst space) {
    Fac* f = F(ctx);
    noteCaller(__builtin_return_address(0), f);
    return hop<JSValue>(f, [&]() -> JSValue { return impl_JS_JSONStringify(ctx, obj, replacer, space); });
}
JSValue JS_GetTypedArrayBuffer(JSContext* ctx, JSValueConst obj, size_t* poff, size_t* plen, size_t* pbpe) {
    Fac* f = F(ctx);
    noteCaller(__builtin_return_address(0), f);
    return hop<JSValue>(f, [&]() -> JSValue { return impl_JS_GetTypedArrayBuffer(ctx, obj, poff, plen, pbpe); });
}
uint8_t* JS_GetArrayBuffer(JSContext* ctx, size_t* psize, JSValueConst obj) {
    Fac* f = F(ctx);
    noteCaller(__builtin_return_address(0), f);
    return hop<uint8_t*>(f, [&]() -> uint8_t* { return impl_JS_GetArrayBuffer(ctx, psize, obj); });
}
JSPromiseStateEnum JS_PromiseState(JSContext* ctx, JSValueConst promise) {
    Fac* f = F(ctx);
    noteCaller(__builtin_return_address(0), f);
    return hop<JSPromiseStateEnum>(f, [&]() -> JSPromiseStateEnum { return impl_JS_PromiseState(ctx, promise); });
}
JSValue JS_PromiseResult(JSContext* ctx, JSValueConst promise) {
    Fac* f = F(ctx);
    noteCaller(__builtin_return_address(0), f);
    return hop<JSValue>(f, [&]() -> JSValue { return impl_JS_PromiseResult(ctx, promise); });
}
int JS_ExecutePendingJob(JSRuntime* rt, JSContext** pctx) {
    Fac* f = lookupCx(reinterpret_cast<JSContext*>(rt));
    noteCaller(__builtin_return_address(0), f);
    return hop<int>(f, [&]() -> int { return impl_JS_ExecutePendingJob(rt, pctx); });
}
void JS_SetModuleLoaderFunc(JSRuntime* rt, JSModuleNormalizeFunc* arg1, JSModuleLoaderFunc* loader, void* opaque) {
    Fac* f = lookupCx(reinterpret_cast<JSContext*>(rt));
    noteCaller(__builtin_return_address(0), f);
    return hop<void>(f, [&]() -> void { return impl_JS_SetModuleLoaderFunc(rt, arg1, loader, opaque); });
}
JSValue JS_Eval(JSContext* ctx, const char* input, size_t len, const char* filename, int flags) {
    Fac* f = F(ctx);
    noteCaller(__builtin_return_address(0), f);
    return hop<JSValue>(f, [&]() -> JSValue { return impl_JS_Eval(ctx, input, len, filename, flags); });
}
JSValue JS_EvalFunction(JSContext* ctx, JSValue fun) {
    Fac* f = F(ctx);
    noteCaller(__builtin_return_address(0), f);
    return hop<JSValue>(f, [&]() -> JSValue { return impl_JS_EvalFunction(ctx, fun); });
}
JSValue JS_GetModuleNamespace(JSContext* ctx, JSModuleDef* m) {
    Fac* f = F(ctx);
    noteCaller(__builtin_return_address(0), f);
    return hop<JSValue>(f, [&]() -> JSValue { return impl_JS_GetModuleNamespace(ctx, m); });
}
}  // extern "C"
