// QuickJS C API on top of SpiderMonkey.
//
// Script.cpp, ScriptConversion.cpp and ScriptPrograms.cpp are written against quickjs.h. This file implements the
// subset of its extern functions they use, so the same sources run on SpiderMonkey (a JIT) without being rewritten:
// build with OWE_JS_SPIDERMONKEY and link libmozjs instead of libqjs. quickjs.h stays the public header, which gives
// the JSValue layout, the JS_IS_* / JS_New* inlines and the JS_CFUNC_DEF family for free.
//
// Handles. A JSValue for a number, boolean, null or undefined is the QuickJS value itself. A JSValue for an object,
// string, symbol or BigInt holds an index into a table of JS::Value roots (Fac::vals) and a reference count (Fac::rc),
// so JS_DupValue / JS_FreeValue keep their QuickJS meaning: a count that reaches zero un-roots the value.
// Ownership follows quickjs.h: functions returning JSValue hand a new reference to the caller, JSValue parameters are
// consumed, JSValueConst parameters are borrowed. Native callbacks get borrowed references that the trampoline frees
// after the call; a callback that keeps one must JS_DupValue it, as with QuickJS.
//
// Exotic classes (only set_property is used, by the material class) are plain class instances wrapped in a JS Proxy
// whose `set` trap calls the C function; JS_GetOpaque looks through the proxy.
// lito's dependency scanner has its own preprocessor whose __has_builtin does not know __builtin_ctz, so
// mozilla/MathAlgorithms.h stops it with an #error. The scanner only needs the include graph: skip that header for it
// (the compiler proper has the builtin and includes it normally).
#if defined(__clang__) && ! __has_builtin(__builtin_ctz)
#  define mozilla_MathAlgorithms_h
#endif
#include <jsapi.h>
#include <jsfriendapi.h>
#include <js/Array.h>
#include <js/ArrayBuffer.h>
#include <js/CompilationAndEvaluation.h>
#include <js/CompileOptions.h>
#include <js/Conversions.h>
#include <js/Exception.h>
#include <js/GCVector.h>
#include <js/Initialization.h>
#include <js/JSON.h>
#include <js/Modules.h>
#include <js/Object.h>
#include <js/Promise.h>
#include <js/PropertyAndElement.h>
#include <js/Proxy.h>
#include <js/Realm.h>
#include <js/RealmOptions.h>
#include <js/SourceText.h>
#include <js/String.h>
#include <js/Symbol.h>
#include <js/TypeDecls.h>
#include <js/Value.h>
#include <js/ValueArray.h>
#include <js/experimental/TypedData.h>

#include <js/Date.h>
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
#include <vector>

// quickjs.h declares JSAtom (a typedef) and JS_GetRuntime, which SpiderMonkey already has under the same names.
#define JSAtom OweQjsAtom
#define JS_GetRuntime OweQjsDeclGetRuntime
#include <quickjs.h>
#undef JSAtom
#undef JS_GetRuntime

using OweAtom = uint32_t;

extern "C" {
// Exported under the QuickJS name; the C++ declaration of SpiderMonkey's JS_GetRuntime has a different mangling.
JSRuntime* OweJsGetRuntime(JSContext* ctx) __asm__("JS_GetRuntime");
}

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

struct ClassRec {
    JSClass    clasp {};
    JSClassOps ops {};
    JSClassDef def {};
    JSClassID  id { 0 };
};

struct ModuleDef {
    std::unique_ptr<JS::PersistentRootedObject> module;
    std::string                                 name;
};

struct JobQueue final : JS::JobQueue {
    std::unique_ptr<JS::PersistentRooted<JS::GCVector<JSObject*, 0, js::SystemAllocPolicy>>> jobs;
    size_t                                                                                  head { 0 };

    bool getHostDefinedData(JSContext*, JS::MutableHandle<JSObject*> data) const override {
        data.set(nullptr);
        return true;
    }
    bool enqueuePromiseJob(JSContext* cx, JS::HandleObject, JS::HandleObject job, JS::HandleObject,
                           JS::HandleObject) override {
        if (! job) return true;
        return jobs->get().append(job);
    }
    // Runs jobs until the queue is empty; returns the number run.
    size_t drain(JSContext* cx) {
        size_t ran = 0;
        while (head < jobs->get().length()) {
            JS::RootedObject job(cx, jobs->get()[head++]);
            JS::RootedValue  rv(cx);
            if (! JS::Call(cx, JS::UndefinedHandleValue, job, JS::HandleValueArray::empty(), &rv)) {
                JS_ClearPendingException(cx);
            }
            ++ran;
        }
        jobs->get().clear();
        head = 0;
        return ran;
    }
    void runJobs(JSContext* cx) override { drain(cx); }
    bool empty() const override { return head >= jobs->get().length(); }
    bool isDrainingStopped() const override { return false; }

protected:
    js::UniquePtr<SavedJobQueue> saveJobQueue(JSContext*) override { return nullptr; }
};

using ValVec  = JS::GCVector<JS::Value, 0, js::SystemAllocPolicy>;
using KeyVec  = JS::GCVector<JS::PropertyKey, 0, js::SystemAllocPolicy>;
using ObjVec  = JS::GCVector<JSObject*, 0, js::SystemAllocPolicy>;

struct Fac {
    JSContext*                                  cx { nullptr };
    std::unique_ptr<JS::PersistentRooted<ValVec>> vals;
    std::vector<int32_t>                        rc;
    std::vector<uint32_t>                       freeSlots;
    std::unique_ptr<JS::PersistentRooted<KeyVec>> atoms;
    std::vector<uint32_t>                       freeAtoms;
    std::vector<std::unique_ptr<ClassRec>>      classes;   // by class id
    std::unique_ptr<JS::PersistentRooted<ObjVec>> protos;  // by class id
    std::vector<ModuleDef*>                     modules;
    std::unordered_map<std::string, ModuleDef*> moduleByName;
    JSModuleLoaderFunc*                         loader { nullptr };
    void*                                       loaderOpaque { nullptr };
    void*                                       opaque { nullptr };
    JobQueue                                    queue;
    std::unique_ptr<JS::PersistentRootedObject> global;
    std::unique_ptr<JS::PersistentRootedObject> proxyCtor;
    std::unique_ptr<JS::PersistentRootedObject> classHandlers;  // array: proxy handler per class id
    JS::Realm*                                  oldRealm { nullptr };
    size_t                                      stackQuota { 0 };

    // The engine thread. SpiderMonkey contexts belong to the thread that made them, but the host builds a runtime on
    // one thread, drives it from another and destroys it on a third. So every runtime owns a thread, all facade calls
    // from other threads are forwarded to it (see hop()), and calls that already come from it run directly.
    uint32_t                id { 0 };
    pthread_t               pt {};
    std::atomic<bool>       started { false };
    std::mutex              submitMu;  // one host thread at a time
    std::function<void()>   job;
    // Hand-off state: 0 idle, 1 job posted, 2 job done. Both sides spin briefly before sleeping on the futex: the host
    // makes many short calls per frame, and a sleep/wake round trip would cost more than the calls themselves.
    std::atomic<int>        state { 0 };
    std::atomic<bool>       quit { false };
};

// Several runtimes can be alive at once (tests build many, on several threads); each has its own engine thread.
std::once_flag                       g_initOnce;
bool                                 g_initOk = false;
std::mutex                           g_mu;
std::unordered_map<JSContext*, Fac*> g_byCx;
std::vector<Fac*>                    g_byId(1, nullptr);  // box pointers carry the id of the runtime they belong to
std::atomic<int>                     g_live { 0 };
thread_local Fac*                    tl_cur = nullptr;  // set on engine threads
std::atomic<uint32_t>                g_nextClassId { 1 };

Fac* lookupCx(JSContext* cx) {
    std::lock_guard<std::mutex> lock(g_mu);
    auto                        it = g_byCx.find(cx);
    return it == g_byCx.end() ? nullptr : it->second;
}
Fac* lookupId(uint32_t id) {
    std::lock_guard<std::mutex> lock(g_mu);
    return id < g_byId.size() ? g_byId[id] : nullptr;
}
inline Fac* F(JSContext* cx) {
    Fac* f = tl_cur;
    return (f && f->cx == cx) ? f : lookupCx(cx);
}

inline bool onEngine(const Fac* f) { return f->started.load() && pthread_equal(f->pt, pthread_self()); }

inline void cpuRelax() {
#if defined(__x86_64__) || defined(__i386__)
    __builtin_ia32_pause();
#elif defined(__aarch64__)
    asm volatile("yield");
#endif
}
std::atomic<uint64_t> g_hops { 0 }, g_hopSleeps { 0 }, g_engineSleeps { 0 };
// OWE_JS_DEBUG=2: count cross-thread calls by the address of the host code that made them (see JS_FreeContext).
const bool                       g_traceHops = [] { const char* v = getenv("OWE_JS_DEBUG"); return v && v[0] == '2'; }();
std::mutex                       g_traceMu;
std::unordered_map<void*, uint64_t> g_hopCallers;
inline void noteCaller(void* addr, const Fac* f) {
    if (__builtin_expect(g_traceHops, 0) && ! onEngine(f)) {
        std::lock_guard<std::mutex> lock(g_traceMu);
        ++g_hopCallers[addr];
    }
}
constexpr int kSpin = 4000;  // about 100-200 us of waiting before a thread goes to sleep

void engineMain(Fac* f) {
    tl_cur = f;
    for (;;) {
        // wait for a job (or quit)
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
        f->state.store(2, std::memory_order_release);
        f->state.notify_one();
    }
}

void stopEngineThread(Fac* f) {
    f->quit.store(true, std::memory_order_release);
    f->state.store(3, std::memory_order_release);
    f->state.notify_all();
}

// Runs fn on the runtime's engine thread and returns its result; runs it inline when already there.
template <class R, class Fn>
R hop(Fac* f, Fn&& fn) {
    if (onEngine(f)) return fn();
    g_hops.fetch_add(1, std::memory_order_relaxed);
    std::lock_guard<std::mutex> submit(f->submitMu);
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

// QuickJS takes local time from libc, so a POSIX TZ such as "EST+05:00" (hours behind UTC, any name) works there.
// SpiderMonkey asks ICU, which only knows zone ids. For whole-hour POSIX offsets use the equivalent Etc/GMT zone, which
// libc reads too, so both engines show the same local time. (The parity comparer sets TZ this way.)
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
        if (tz[i] == ':') i += 3;  // seconds
    }
    if (tz[i] != '\0' || minutes != 0 || hours > 14) return;
    char id[32];
    snprintf(id, sizeof id, "Etc/GMT%c%d", sign, hours);
    char path[96];
    snprintf(path, sizeof path, "/usr/share/zoneinfo/%s", id);
    struct stat st;
    if (stat(path, &st) != 0) return;  // no tz database: leave TZ alone
    setenv("TZ", id, 1);
    JS::ResetTimeZone();
}

// JIT and GC settings. The defaults are SpiderMonkey's; each can be overridden from the environment for experiments:
// OWE_SM_BLINTERP_WARMUP, OWE_SM_BASELINE_WARMUP, OWE_SM_ION_WARMUP (calls/loop iterations before each tier),
// OWE_SM_NURSERY_KB (maximum nursery), OWE_SM_OFFTHREAD (0: compile Ion on the engine thread).
void tuneEngine(JSContext* cx) {
    auto env = [](const char* name, long& out) {
        const char* v = getenv(name);
        if (! v || ! *v) return false;
        out = strtol(v, nullptr, 10);
        return true;
    };
    // Measured on More Physics (600 frames, 3 runs each): compiling baseline code at the first call instead of after
    // ten shortens the slow start slightly (first 60 frames 14.4 -> 13.8 ms) and the other thresholds do nothing useful;
    // off-thread Ion compilation matters a lot (turning it off costs 45 %), so that stays on.
    JS_SetGlobalJitCompilerOption(cx, JSJITCOMPILER_BASELINE_WARMUP_TRIGGER, 0);
    // Measured on More Physics (shim, 600 frames, 3 runs): Ion inlines callees up to this many bytecode bytes; the default
    // 130 leaves the small vector and matrix helpers of cannon-es as calls. 200 to 500 take the mean frame from about
    // 11.5 to 10 ms; 2000 is no better.
    JS_SetGlobalJitCompilerOption(cx, JSJITCOMPILER_INLINING_BYTECODE_MAX_LENGTH, 300);
    long v;
    if (env("OWE_SM_INLINE", v)) JS_SetGlobalJitCompilerOption(cx, JSJITCOMPILER_INLINING_BYTECODE_MAX_LENGTH, uint32_t(v));
    if (env("OWE_SM_BLINTERP_WARMUP", v)) JS_SetGlobalJitCompilerOption(cx, JSJITCOMPILER_BASELINE_INTERPRETER_WARMUP_TRIGGER, uint32_t(v));
    if (env("OWE_SM_BASELINE_WARMUP", v)) JS_SetGlobalJitCompilerOption(cx, JSJITCOMPILER_BASELINE_WARMUP_TRIGGER, uint32_t(v));
    if (env("OWE_SM_ION_WARMUP", v)) JS_SetGlobalJitCompilerOption(cx, JSJITCOMPILER_ION_NORMAL_WARMUP_TRIGGER, uint32_t(v));
    if (env("OWE_SM_OFFTHREAD", v)) JS_SetGlobalJitCompilerOption(cx, JSJITCOMPILER_OFFTHREAD_COMPILATION_ENABLE, uint32_t(v));
    if (env("OWE_SM_NURSERY_KB", v)) JS_SetGCParameter(cx, JSGC_MAX_NURSERY_BYTES, uint32_t(v) * 1024);
}

// --- value conversion ----------------------------------------------------------------------------------------------

uint32_t newSlot(Fac* f, JS::Value v) {
    auto& vals = f->vals->get();
    if (! f->freeSlots.empty()) {
        uint32_t i = f->freeSlots.back();
        f->freeSlots.pop_back();
        vals[i]  = v;
        f->rc[i] = 1;
        return i;
    }
    if (! vals.append(v)) std::abort();
    f->rc.push_back(1);
    return uint32_t(vals.length() - 1);
}

constexpr int kIdShift = 40;
inline uint32_t slotOf(JSValueConst v) {
    return uint32_t(reinterpret_cast<uintptr_t>(JS_VALUE_GET_PTR(v)) & ((uintptr_t(1) << kIdShift) - 1)) - 1;
}
inline uint32_t idOf(JSValueConst v) { return uint32_t(reinterpret_cast<uintptr_t>(JS_VALUE_GET_PTR(v)) >> kIdShift); }

JSValue mkBoxed(Fac* f, int tag, JS::Value v) {
    uint32_t i = newSlot(f, v);
    return JS_MKPTR(tag, reinterpret_cast<void*>((uintptr_t(f->id) << kIdShift) | (uintptr_t(i) + 1)));
}

JSValue fromSM(Fac* f, JS::HandleValue v) {
    if (v.isInt32()) return JS_NewInt32(nullptr, v.toInt32());
    if (v.isDouble()) return JS_NewFloat64(nullptr, v.toDouble());
    if (v.isBoolean()) return JS_NewBool(nullptr, v.toBoolean());
    if (v.isNull()) return JS_NULL;
    if (v.isUndefined()) return JS_UNDEFINED;
    if (v.isObject()) return mkBoxed(f, JS_TAG_OBJECT, v);
    if (v.isString()) return mkBoxed(f, JS_TAG_STRING, v);
    if (v.isSymbol()) return mkBoxed(f, JS_TAG_SYMBOL, v);
    if (v.isBigInt()) return mkBoxed(f, JS_TAG_BIG_INT, v);
    return JS_UNDEFINED;
}

JSValue fromObj(Fac* f, JSObject* o) {
    if (! o) return JS_NULL;
    return mkBoxed(f, JS_TAG_OBJECT, JS::ObjectValue(*o));
}

// Raw object: used for finalizers, which run while the object is dying and must not root it.
constexpr int kTagRawObject = JS_TAG_UNINITIALIZED;

JS::Value toSM(Fac* f, JSValueConst v) {
    switch (JS_VALUE_GET_TAG(v)) {
    case JS_TAG_INT: return JS::Int32Value(JS_VALUE_GET_INT(v));
    case JS_TAG_BOOL: return JS::BooleanValue(JS_VALUE_GET_BOOL(v));
    case JS_TAG_NULL: return JS::NullValue();
    case JS_TAG_UNDEFINED: return JS::UndefinedValue();
    case JS_TAG_FLOAT64: return JS::DoubleValue(JS_VALUE_GET_FLOAT64(v));
    case JS_TAG_OBJECT:
    case JS_TAG_STRING:
    case JS_TAG_SYMBOL:
    case JS_TAG_BIG_INT: return f->vals->get()[slotOf(v)];
    case JS_TAG_MODULE: {
        auto* def = static_cast<ModuleDef*>(JS_VALUE_GET_PTR(v));
        return JS::ObjectValue(*def->module->get());
    }
    case kTagRawObject: return JS::ObjectValue(*static_cast<JSObject*>(JS_VALUE_GET_PTR(v)));
    default: return JS::UndefinedValue();
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
        f->vals->get()[i] = JS::UndefinedValue();
        f->freeSlots.push_back(i);
    }
}

// Reads a consumed return value and drops the reference.
JS::Value takeSM(Fac* f, JSValue v) {
    JS::Value out = toSM(f, v);
    releaseV(f, v);
    return out;
}

JSObject* objOf(Fac* f, JSValueConst v) {
    int t = JS_VALUE_GET_TAG(v);
    if (t != JS_TAG_OBJECT && t != kTagRawObject && t != JS_TAG_MODULE) return nullptr;
    return &toSM(f, v).toObject();
}

// A throw helper that builds an error of the named global constructor type.
JSValue throwNamedMsg(JSContext* cx, Fac* f, const char* ctorName, const char* buf) {
    JS::RootedValue  ctor(cx);
    JS::RootedObject global(cx, f->global->get());
    if (JS_GetProperty(cx, global, ctorName, &ctor) && ctor.isObject()) {
        JS::RootedString msg(cx, JS_NewStringCopyUTF8Z(cx, JS::ConstUTF8CharsZ(buf, strlen(buf))));
        JS::RootedValueArray<1> args(cx);
        args[0].setString(msg);
        JS::RootedObject err(cx);
        if (msg && JS::Construct(cx, ctor, args, &err)) {
            JS::RootedValue ev(cx, JS::ObjectValue(*err));
            JS_SetPendingException(cx, ev);
            return JS_EXCEPTION;
        }
        if (JS_IsExceptionPending(cx)) return JS_EXCEPTION;
    }
    JS_ReportErrorUTF8(cx, "%s", buf);
    return JS_EXCEPTION;
}

JSValue throwFmt(JSContext* ctx, const char* ctorName, const char* fmt, va_list ap) {
    char buf[1024];
    vsnprintf(buf, sizeof buf, fmt, ap);
    Fac* f = F(ctx);
    return hop<JSValue>(f, [&] { return throwNamedMsg(ctx, f, ctorName, buf); });
}

// SpiderMonkey can fail a call without leaving an exception (out of memory, termination). QuickJS callers expect
// JS_EXCEPTION to come with one, so make one.
JSValue failed(JSContext* cx) {
    if (! JS_IsExceptionPending(cx)) JS_ReportErrorASCII(cx, "script terminated (out of memory or interrupted)");
    return JS_EXCEPTION;
}

// --- native function trampolines -----------------------------------------------------------------------------------

bool tramp(JSContext* cx, unsigned argc, JS::Value* vp) {
    JS::CallArgs args   = JS::CallArgsFromVp(argc, vp);
    Fac*         f      = F(cx);
    JSObject*    callee = &args.callee();
    void*        fnp    = js::GetFunctionNativeReserved(callee, 0).toPrivate();
    const JS::Value& r1 = js::GetFunctionNativeReserved(callee, 1);

    JSValue  stackArgv[8];
    std::vector<JSValue> heapArgv;
    JSValue* argv = stackArgv;
    if (argc > 8) {
        heapArgv.resize(argc);
        argv = heapArgv.data();
    }
    JS::RootedValue tmp(cx);
    JSValue         thisv = JS_UNDEFINED;
    tmp = args.thisv();
    thisv = fromSM(f, tmp);
    for (unsigned i = 0; i < argc; ++i) {
        tmp     = args[i];
        argv[i] = fromSM(f, tmp);
    }

    JSValue ret = JS_UNDEFINED;
    int     kind = r1.isInt32() ? (r1.toInt32() & 0xff) : int(kData);
    int     magic = r1.isInt32() ? (r1.toInt32() >> 8) : 0;
    switch (kind) {
    case kGeneric: ret = reinterpret_cast<JSCFunction*>(fnp)(cx, thisv, int(argc), argv); break;
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
        // slot 1 is an array: [magic, data...]
        JS::RootedObject  arr(cx, &r1.toObject());
        uint32_t          len = 0;
        JS::GetArrayLength(cx, arr, &len);
        JS::RootedValue   mv(cx);
        JS_GetElement(cx, arr, 0, &mv);
        std::vector<JSValue> data(len > 1 ? len - 1 : 0);
        for (uint32_t i = 1; i < len; ++i) {
            JS_GetElement(cx, arr, i, &tmp);
            data[i - 1] = fromSM(f, tmp);
        }
        ret = reinterpret_cast<JSValue (*)(JSContext*, JSValueConst, int, JSValueConst*, int, JSValueConst*)>(fnp)(
            cx, thisv, int(argc), argv, mv.toInt32(), data.data());
        for (auto& d : data) releaseV(f, d);
        break;
    }
    }

    releaseV(f, thisv);
    for (unsigned i = 0; i < argc; ++i) releaseV(f, argv[i]);
    if (JS_IsException(ret)) {
        if (! JS_IsExceptionPending(cx)) JS_ReportErrorASCII(cx, "native function failed without an exception");
        return false;
    }
    args.rval().set(takeSM(f, ret));
    return true;
}

JSFunction* newNative(JSContext* cx, void* fnp, int kind, int magic, const char* name, int length) {
    JSFunction* fn = js::NewFunctionWithReserved(cx, tramp, unsigned(length), 0, name);
    if (! fn) return nullptr;
    JSObject* fo = JS_GetFunctionObject(fn);
    js::SetFunctionNativeReserved(fo, 0, JS::PrivateValue(fnp));
    js::SetFunctionNativeReserved(fo, 1, JS::Int32Value(kind | (magic << 8)));
    return fn;
}

// --- classes -------------------------------------------------------------------------------------------------------

ClassRec* classOf(Fac* f, JSClassID id) {
    if (id == 0 || id >= f->classes.size()) return nullptr;
    return f->classes[id].get();
}

void classFinalize(JS::GCContext*, JSObject* obj) {
    const JSClass* clasp = JS::GetClass(obj);
    auto*          rec   = reinterpret_cast<ClassRec*>(const_cast<JSClass*>(clasp));
    if (rec->def.finalizer) {
        Fac*    f = static_cast<Fac*>(JS::GetReservedSlot(obj, 1).toPrivate());
        JSValue v = JS_MKPTR(kTagRawObject, obj);
        rec->def.finalizer(f ? reinterpret_cast<JSRuntime*>(f->cx) : nullptr, v);
    }
}

JSObject* unwrapProxy(JSObject* o) {
    while (o && js::IsProxy(o)) o = js::GetProxyTargetObject(o);
    return o;
}

// A proxy `set` trap that forwards to the class's C set_property.
bool proxySetTrap(JSContext* cx, unsigned argc, JS::Value* vp) {
    JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
    Fac*         f    = F(cx);
    auto*        rec  = static_cast<ClassRec*>(js::GetFunctionNativeReserved(&args.callee(), 0).toPrivate());
    JS::RootedValue target(cx, args.get(0)), key(cx, args.get(1)), value(cx, args.get(2)), receiver(cx, args.get(3));
    JS::RootedId    id(cx);
    if (! JS_ValueToId(cx, key, &id)) return false;
    uint32_t atom;
    if (! f->freeAtoms.empty()) {
        atom = f->freeAtoms.back();
        f->freeAtoms.pop_back();
        f->atoms->get()[atom] = id;
    } else {
        if (! f->atoms->get().append(id.get())) return false;
        atom = uint32_t(f->atoms->get().length() - 1);
    }
    JSValue tv = fromSM(f, target), vv = fromSM(f, value), rv = fromSM(f, receiver);
    int     r  = rec->def.exotic->set_property(cx, tv, OweAtom(atom + 1), vv, rv, 0);
    releaseV(f, tv);
    releaseV(f, vv);
    releaseV(f, rv);
    f->freeAtoms.push_back(atom);
    if (r < 0) return false;
    args.rval().setBoolean(true);
    return true;
}

}  // namespace

// ===================================================================================================================
// extern "C" surface (names and signatures from quickjs.h)
// ===================================================================================================================

extern "C" {

JSRuntime* JS_NewRuntime(void) {
    std::call_once(g_initOnce, [] {
        g_initOk = JS_Init();
        normalizeTimeZone();
        // libmozjs's own static destructors fault if the engine is still initialized at exit. atexit handlers run
        // before them, so shut it down there (only when no context is left; SpiderMonkey requires that).
        std::atexit([] {
            if (g_live.load() == 0) JS_ShutDown();
        });
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
    }
    {
        // A generous stack of our own: the thread default can be as small as the shell's ulimit says.
        pthread_attr_t attr;
        pthread_attr_init(&attr);
        pthread_attr_setstacksize(&attr, size_t(16) << 20);
        pthread_create(
            &f->pt, &attr, [](void* a) -> void* { engineMain(static_cast<Fac*>(a)); return nullptr; }, f);
        pthread_attr_destroy(&attr);
        f->started.store(true);
    }
    // Contexts are created by JS_NewRuntime and handed out by JS_NewContext: SpiderMonkey has no runtime without one.
    JSContext* cx = hop<JSContext*>(f, [] {
        JSContext* c = JS_NewContext(JS::DefaultHeapMaxBytes);
        // The default heap limit (32 MB) is an uncatchable out-of-memory; QuickJS has no limit unless asked. Leave
        // the size to the machine (the viewer runs in a memory cgroup anyway).
        if (c) JS_SetGCParameter(c, JSGC_MAX_BYTES, 0xffffffffu);
        return c;
    });
    if (! cx) {
        stopEngineThread(f);
        pthread_join(f->pt, nullptr);
        std::lock_guard<std::mutex> lock(g_mu);
        g_byId[f->id] = nullptr;
        delete f;
        return nullptr;
    }
    f->cx = cx;
    {
        std::lock_guard<std::mutex> lock(g_mu);
        g_byCx[cx] = f;
    }
    g_live.fetch_add(1);
    return reinterpret_cast<JSRuntime*>(cx);
}

JSContext* impl_JS_NewContext(JSRuntime* rt) {
    JSContext* cx = reinterpret_cast<JSContext*>(rt);
    Fac*       f  = lookupCx(cx);
    if (! f || f->global) return nullptr;  // one context per runtime
    // Our own queue, so JS_ExecutePendingJob can report whether it ran anything.
    f->queue.jobs = std::make_unique<JS::PersistentRooted<ObjVec>>(cx);
    JS::SetJobQueue(cx, &f->queue);
    if (! JS::InitSelfHostedCode(cx)) return nullptr;
    f->vals    = std::make_unique<JS::PersistentRooted<ValVec>>(cx);
    f->atoms   = std::make_unique<JS::PersistentRooted<KeyVec>>(cx);
    f->protos  = std::make_unique<JS::PersistentRooted<ObjVec>>(cx);
    f->vals->get().append(JS::UndefinedValue());  // slot 0 unused
    f->rc.push_back(0);
    f->protos->get().append(nullptr);
    f->classes.emplace_back(nullptr);
    f->atoms->get().append(JS::PropertyKey::Void());  // atom 0 unused
    static const JSClassOps globalOps = { nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                                          JS_GlobalObjectTraceHook };
    static const JSClass    globalClass = { "global", JSCLASS_GLOBAL_FLAGS, &globalOps };
    JS::RealmOptions        opts;
    JS::RootedObject        global(cx, JS_NewGlobalObject(cx, &globalClass, nullptr, JS::FireOnNewGlobalHook, opts));
    if (! global) return nullptr;
    f->oldRealm = JS::EnterRealm(cx, global);
    f->global   = std::make_unique<JS::PersistentRootedObject>(cx, global);
    if (! JS::InitRealmStandardClasses(cx)) return nullptr;
    tuneEngine(cx);
    JS::RootedValue proxyCtor(cx);
    JS_GetProperty(cx, global, "Proxy", &proxyCtor);
    f->proxyCtor    = std::make_unique<JS::PersistentRootedObject>(cx, proxyCtor.isObject() ? &proxyCtor.toObject() : nullptr);
    f->classHandlers = std::make_unique<JS::PersistentRootedObject>(cx, JS::NewArrayObject(cx, 0));
    // Where QuickJS throws RangeError for runaway recursion, SpiderMonkey throws InternalError("too much recursion").
    // Scripts catch RangeError (or look at e.name), so make InternalError one.
    const char* patch = "Object.setPrototypeOf(InternalError.prototype, RangeError.prototype);"
                        "InternalError.prototype.name = 'RangeError';";
    JS::CompileOptions       co(cx);
    co.setFileAndLine("owe-patch", 1);
    JS::SourceText<mozilla::Utf8Unit> st;
    if (st.init(cx, patch, strlen(patch), JS::SourceOwnership::Borrowed)) {
        JS::RootedValue rv(cx);
        if (! JS::Evaluate(cx, co, st, &rv)) JS_ClearPendingException(cx);
    }
    return cx;
}

static void teardownOnEngine(Fac* f, JSContext* ctx) {
    // Drop every root before the context goes: finalizers run during JS_DestroyContext.
    for (ModuleDef* m : f->modules) delete m;
    f->modules.clear();
    f->moduleByName.clear();
    f->classHandlers.reset();
    f->proxyCtor.reset();
    f->global.reset();
    f->protos.reset();
    f->atoms.reset();
    f->vals.reset();
    f->queue.jobs.reset();
    JS::SetJobQueue(ctx, nullptr);
    JS::LeaveRealm(ctx, f->oldRealm);
    JS_DestroyContext(ctx);
}

void JS_FreeContext(JSContext* ctx) {
    Fac* f = lookupCx(ctx);
    if (! f) return;
    if (g_traceHops) {
        std::vector<std::pair<uint64_t, void*>> top;
        {
            std::lock_guard<std::mutex> lock(g_traceMu);
            for (auto& kv : g_hopCallers) top.emplace_back(kv.second, kv.first);
        }
        std::sort(top.rbegin(), top.rend());
        for (size_t i = 0; i < top.size() && i < 25; ++i) {
            // file offset of the caller in its mapping: resolve with `addr2line -e <binary> -f 0x<offset>`
            unsigned long addr = (unsigned long)top[i].second, off = 0;
            char          path[256] = "?";
            if (FILE* maps = fopen("/proc/self/maps", "r")) {
                char line[512];
                while (fgets(line, sizeof line, maps)) {
                    unsigned long lo, hi, fo;
                    if (sscanf(line, "%lx-%lx %*s %lx", &lo, &hi, &fo) == 3 && addr >= lo && addr < hi) {
                        off = addr - lo + fo;
                        const char* slash = strchr(line, '/');
                        if (slash) { snprintf(path, sizeof path, "%s", slash); path[strcspn(path, "\n")] = 0; }
                        break;
                    }
                }
                fclose(maps);
            }
            fprintf(stderr, "[js] hop caller %8lu  %s 0x%lx\n", (unsigned long)top[i].first, path, off);
        }
    }
    if (getenv("OWE_JS_DEBUG")) fprintf(stderr, "[js] hops %lu, caller sleeps %lu, engine sleeps %lu\n", (unsigned long)g_hops.load(), (unsigned long)g_hopSleeps.load(), (unsigned long)g_engineSleeps.load());
    hop<void>(f, [&] { teardownOnEngine(f, ctx); });
    stopEngineThread(f);
    if (pthread_equal(f->pt, pthread_self())) {
        pthread_detach(f->pt);  // freed from inside its own callback (does not happen in the host): let it wind down
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

JSRuntime* OweJsGetRuntime(JSContext* ctx) { return reinterpret_cast<JSRuntime*>(ctx); }

void JS_SetContextOpaque(JSContext* ctx, void* opaque) { F(ctx)->opaque = opaque; }
void* JS_GetContextOpaque(JSContext* ctx) { return F(ctx)->opaque; }

void impl_JS_SetMaxStackSize(JSRuntime* rt, size_t size) {
    JSContext* cx = reinterpret_cast<JSContext*>(rt);
    // "unlimited" (0) is not available: the engine thread has 16 MiB, take most of it.
    if (size == 0 || size > (size_t(12) << 20)) size = size_t(12) << 20;
    F(cx)->stackQuota = size;
    JS_SetNativeStackQuota(cx, size);
}

// --- values ---------------------------------------------------------------------------------------------------------

JSValue JS_DupValue(JSContext* ctx, JSValueConst v) {
    if (isBoxed(v)) ++F(ctx)->rc[slotOf(v)];
    return v;
}

void JS_FreeValue(JSContext* ctx, JSValue v) {
    if (isBoxed(v)) releaseV(F(ctx), v);
}

JSValue JS_GetGlobalObject(JSContext* ctx) { return fromObj(F(ctx), F(ctx)->global->get()); }

JSValue impl_JS_NewObject(JSContext* ctx) {
    JSObject* o = JS_NewPlainObject(ctx);
    return o ? fromObj(F(ctx), o) : JS_EXCEPTION;
}

JSValue impl_JS_NewArray(JSContext* ctx) {
    JSObject* o = JS::NewArrayObject(ctx, 0);
    return o ? fromObj(F(ctx), o) : JS_EXCEPTION;
}

static bool implIsArray(Fac* f, JSValueConst v) {
    if (JS_VALUE_GET_TAG(v) != JS_TAG_OBJECT) return false;
    JS::RootedObject o(f->cx, &toSM(f, v).toObject());
    bool             is = false;
    return JS::IsArray(f->cx, o, &is) && is;
}

bool impl_JS_IsFunction(JSContext* ctx, JSValueConst v) {
    Fac* f = F(ctx);
    if (JS_VALUE_GET_TAG(v) != JS_TAG_OBJECT) return false;
    return JS::IsCallable(&toSM(f, v).toObject());
}

JSValue impl_JS_NewStringLen(JSContext* ctx, const char* s, size_t len) {
    JSString* str = JS_NewStringCopyUTF8N(ctx, JS::UTF8Chars(s, len));
    if (! str) return JS_EXCEPTION;
    JS::RootedValue v(ctx, JS::StringValue(str));
    return fromSM(F(ctx), v);
}

const char* impl_JS_ToCStringLen2(JSContext* ctx, size_t* plen, JSValueConst val, bool /*cesu8*/) {
    Fac*            f = F(ctx);
    JS::RootedValue v(ctx, toSM(f, val));
    JS::RootedString s(ctx, v.isString() ? v.toString() : JS::ToString(ctx, v));
    if (! s) return nullptr;
    JS::UniqueChars u = JS_EncodeStringToUTF8(ctx, s);
    if (! u) return nullptr;
    size_t n = strlen(u.get());
    if (plen) *plen = n;
    char* out = static_cast<char*>(malloc(n + 1));
    memcpy(out, u.get(), n + 1);
    return out;
}

void JS_FreeCString(JSContext*, const char* p) { free(const_cast<char*>(p)); }

int impl_JS_ToBool(JSContext* ctx, JSValueConst v) {
    JS::RootedValue sv(ctx, toSM(F(ctx), v));
    return JS::ToBoolean(sv) ? 1 : 0;
}

int impl_JS_ToFloat64(JSContext* ctx, double* pres, JSValueConst v) {
    switch (JS_VALUE_GET_TAG(v)) {
    case JS_TAG_INT: *pres = JS_VALUE_GET_INT(v); return 0;
    case JS_TAG_FLOAT64: *pres = JS_VALUE_GET_FLOAT64(v); return 0;
    default: break;
    }
    JS::RootedValue sv(ctx, toSM(F(ctx), v));
    return JS::ToNumber(ctx, sv, pres) ? 0 : -1;
}

int impl_JS_ToInt32(JSContext* ctx, int32_t* pres, JSValueConst v) {
    if (JS_VALUE_GET_TAG(v) == JS_TAG_INT) {
        *pres = JS_VALUE_GET_INT(v);
        return 0;
    }
    JS::RootedValue sv(ctx, toSM(F(ctx), v));
    return JS::ToInt32(ctx, sv, pres) ? 0 : -1;
}

int impl_JS_ToInt64(JSContext* ctx, int64_t* pres, JSValueConst v) {
    if (JS_VALUE_GET_TAG(v) == JS_TAG_INT) {
        *pres = JS_VALUE_GET_INT(v);
        return 0;
    }
    JS::RootedValue sv(ctx, toSM(F(ctx), v));
    return JS::ToInt64(ctx, sv, pres) ? 0 : -1;
}

// --- properties ----------------------------------------------------------------------------------------------------

static unsigned attrsOf(int flags) {
    unsigned a = 0;
    if (flags & JS_PROP_ENUMERABLE) a |= JSPROP_ENUMERATE;
    if (! (flags & JS_PROP_WRITABLE)) a |= JSPROP_READONLY;
    if (! (flags & JS_PROP_CONFIGURABLE)) a |= JSPROP_PERMANENT;
    return a;
}

JSValue impl_JS_GetPropertyStr(JSContext* ctx, JSValueConst obj, const char* prop) {
    Fac*             f = F(ctx);
    JS::RootedValue  ov(ctx, toSM(f, obj));
    JS::RootedObject o(ctx);
    if (ov.isObject()) o = &ov.toObject();
    else if (! JS_ValueToObject(ctx, ov, &o) || ! o) return JS_EXCEPTION;
    JS::RootedValue out(ctx);
    if (! JS_GetProperty(ctx, o, prop, &out)) return JS_EXCEPTION;
    return fromSM(f, out);
}

JSValue impl_JS_GetPropertyUint32(JSContext* ctx, JSValueConst obj, uint32_t idx) {
    Fac*             f = F(ctx);
    JSObject*        o = objOf(f, obj);
    if (! o) return JS_UNDEFINED;
    JS::RootedObject ro(ctx, o);
    JS::RootedValue  out(ctx);
    if (! JS_GetElement(ctx, ro, idx, &out)) return JS_EXCEPTION;
    return fromSM(f, out);
}

int impl_JS_SetPropertyStr(JSContext* ctx, JSValueConst obj, const char* prop, JSValue val) {
    Fac*             f = F(ctx);
    JSObject*        o = objOf(f, obj);
    JS::RootedValue  v(ctx, takeSM(f, val));
    if (! o) return -1;
    JS::RootedObject ro(ctx, o);
    return JS_SetProperty(ctx, ro, prop, v) ? 1 : -1;
}

int impl_JS_SetPropertyUint32(JSContext* ctx, JSValueConst obj, uint32_t idx, JSValue val) {
    Fac*             f = F(ctx);
    JSObject*        o = objOf(f, obj);
    JS::RootedValue  v(ctx, takeSM(f, val));
    if (! o) return -1;
    JS::RootedObject ro(ctx, o);
    return JS_SetElement(ctx, ro, idx, v) ? 1 : -1;
}

int impl_JS_DefinePropertyValueStr(JSContext* ctx, JSValueConst obj, const char* prop, JSValue val, int flags) {
    Fac*             f = F(ctx);
    JSObject*        o = objOf(f, obj);
    JS::RootedValue  v(ctx, takeSM(f, val));
    if (! o) return -1;
    JS::RootedObject ro(ctx, o);
    return JS_DefineProperty(ctx, ro, prop, v, attrsOf(flags)) ? 1 : -1;
}

int impl_JS_DefinePropertyValueUint32(JSContext* ctx, JSValueConst obj, uint32_t idx, JSValue val, int flags) {
    Fac*             f = F(ctx);
    JSObject*        o = objOf(f, obj);
    JS::RootedValue  v(ctx, takeSM(f, val));
    if (! o) return -1;
    JS::RootedObject ro(ctx, o);
    return JS_DefineElement(ctx, ro, idx, v, attrsOf(flags)) ? 1 : -1;
}

// --- atoms ---------------------------------------------------------------------------------------------------------

OweAtom impl_JS_NewAtom(JSContext* ctx, const char* str) {
    Fac*          f = F(ctx);
    JS::RootedId  id(ctx);
    JS::RootedString s(ctx, JS_NewStringCopyUTF8Z(ctx, JS::ConstUTF8CharsZ(str, strlen(str))));
    if (! s || ! JS_StringToId(ctx, s, &id)) return 0;
    uint32_t atom;
    if (! f->freeAtoms.empty()) {
        atom = f->freeAtoms.back();
        f->freeAtoms.pop_back();
        f->atoms->get()[atom] = id;
    } else {
        if (! f->atoms->get().append(id.get())) return 0;
        atom = uint32_t(f->atoms->get().length() - 1);
    }
    return OweAtom(atom + 1);
}

void impl_JS_FreeAtom(JSContext* ctx, OweAtom a) {
    if (a == 0) return;
    F(ctx)->freeAtoms.push_back(a - 1);
}

const char* impl_JS_AtomToCStringLen(JSContext* ctx, size_t* plen, OweAtom atom) {
    Fac*            f = F(ctx);
    if (atom == 0) return nullptr;
    JS::RootedId    id(ctx, f->atoms->get()[atom - 1]);
    JS::RootedValue v(ctx);
    if (! JS_IdToValue(ctx, id, &v)) return nullptr;
    JSValue boxed = fromSM(f, v);
    const char* s = JS_ToCStringLen2(ctx, plen, boxed, false);
    releaseV(f, boxed);
    return s;
}

int impl_JS_DefinePropertyGetSet(JSContext* ctx, JSValueConst obj, OweAtom prop, JSValue getter, JSValue setter, int flags) {
    Fac*             f = F(ctx);
    JSObject*        o = objOf(f, obj);
    JS::RootedValue  gv(ctx, takeSM(f, getter)), sv(ctx, takeSM(f, setter));
    if (! o) return -1;
    JS::RootedObject ro(ctx, o), go(ctx, gv.isObject() ? &gv.toObject() : nullptr), so(ctx, sv.isObject() ? &sv.toObject() : nullptr);
    JS::RootedId     id(ctx, f->atoms->get()[prop - 1]);
    unsigned         attrs = attrsOf(flags) & ~unsigned(JSPROP_READONLY);
    return JS_DefinePropertyById(ctx, ro, id, go, so, attrs) ? 1 : -1;
}

// --- functions, calls ----------------------------------------------------------------------------------------------

JSValue impl_JS_NewCFunction2(JSContext* ctx, JSCFunction* func, const char* name, int length, JSCFunctionEnum, int magic) {
    JSFunction* fn = newNative(ctx, reinterpret_cast<void*>(func), kGeneric, magic, name, length);
    return fn ? fromObj(F(ctx), JS_GetFunctionObject(fn)) : JS_EXCEPTION;
}

JSValue impl_JS_NewCFunctionData(JSContext* ctx, JSCFunctionData* func, int length, int magic, int data_len, JSValueConst* data) {
    Fac*             f   = F(ctx);
    JSFunction*      fn  = newNative(ctx, reinterpret_cast<void*>(func), kData, 0, "", length);
    if (! fn) return JS_EXCEPTION;
    JS::RootedObject arr(ctx, JS::NewArrayObject(ctx, 0));
    JS::RootedValue  v(ctx, JS::Int32Value(magic));
    JS_SetElement(ctx, arr, 0, v);
    for (int i = 0; i < data_len; ++i) {
        v = toSM(f, data[i]);
        JS_SetElement(ctx, arr, uint32_t(i + 1), v);
    }
    js::SetFunctionNativeReserved(JS_GetFunctionObject(fn), 1, JS::ObjectValue(*arr));
    return fromObj(f, JS_GetFunctionObject(fn));
}

JSValue impl_JS_Call(JSContext* ctx, JSValueConst func, JSValueConst thisv, int argc, JSValueConst* argv) {
    Fac*                 f = F(ctx);
    JS::RootedValue      fv(ctx, toSM(f, func)), tv(ctx, toSM(f, thisv)), rv(ctx);
    JS::RootedValueVector args(ctx);
    if (! args.reserve(size_t(argc))) return JS_EXCEPTION;
    for (int i = 0; i < argc; ++i) args.infallibleAppend(toSM(f, argv[i]));
    JS::RootedObject thisObj(ctx);
    if (tv.isObject()) thisObj = &tv.toObject();
    bool ok = tv.isObject() ? JS_CallFunctionValue(ctx, thisObj, fv, args, &rv)
                            : JS::Call(ctx, tv, fv, args, &rv);
    return ok ? fromSM(f, rv) : failed(ctx);
}

JSValue impl_JS_CallConstructor(JSContext* ctx, JSValueConst func, int argc, JSValueConst* argv) {
    Fac*                  f = F(ctx);
    JS::RootedValue       fv(ctx, toSM(f, func));
    JS::RootedValueVector args(ctx);
    if (! args.reserve(size_t(argc))) return JS_EXCEPTION;
    for (int i = 0; i < argc; ++i) args.infallibleAppend(toSM(f, argv[i]));
    JS::RootedObject out(ctx);
    if (! JS::Construct(ctx, fv, args, &out)) return failed(ctx);
    return fromObj(f, out);
}

int impl_JS_SetPropertyFunctionList(JSContext* ctx, JSValueConst obj, const JSCFunctionListEntry* tab, int len) {
    Fac*             f = F(ctx);
    JSObject*        o = objOf(f, obj);
    if (! o) return -1;
    JS::RootedObject ro(ctx, o);
    for (int i = 0; i < len; ++i) {
        const JSCFunctionListEntry& e = tab[i];
        unsigned attrs = attrsOf(e.prop_flags);
        switch (e.def_type) {
        case JS_DEF_CFUNC: {
            JSFunction* fn = newNative(ctx, reinterpret_cast<void*>(e.u.func.cfunc.generic), kGeneric, e.magic, e.name, e.u.func.length);
            if (! fn) return -1;
            JS::RootedValue fv(ctx, JS::ObjectValue(*JS_GetFunctionObject(fn)));
            if (! JS_DefineProperty(ctx, ro, e.name, fv, attrs)) return -1;
            break;
        }
        case JS_DEF_CGETSET:
        case JS_DEF_CGETSET_MAGIC: {
            bool                  magic = e.def_type == JS_DEF_CGETSET_MAGIC;
            JS::RootedObject      g(ctx), s(ctx);
            if (e.u.getset.get.generic) {
                JSFunction* gf = newNative(ctx, reinterpret_cast<void*>(e.u.getset.get.generic), magic ? kGetterMagic : kGetter, e.magic, e.name, 0);
                if (! gf) return -1;
                g = JS_GetFunctionObject(gf);
            }
            if (e.u.getset.set.generic) {
                JSFunction* sf = newNative(ctx, reinterpret_cast<void*>(e.u.getset.set.generic), magic ? kSetterMagic : kSetter, e.magic, e.name, 1);
                if (! sf) return -1;
                s = JS_GetFunctionObject(sf);
            }
            if (! JS_DefineProperty(ctx, ro, e.name, g, s, (attrs & ~unsigned(JSPROP_READONLY)))) return -1;
            break;
        }
        case JS_DEF_PROP_STRING: {
            JS::RootedString s(ctx, JS_NewStringCopyUTF8Z(ctx, JS::ConstUTF8CharsZ(e.u.str, strlen(e.u.str))));
            if (! s || ! JS_DefineProperty(ctx, ro, e.name, s, attrs)) return -1;
            break;
        }
        case JS_DEF_PROP_INT32:
            if (! JS_DefineProperty(ctx, ro, e.name, e.u.i32, attrs)) return -1;
            break;
        case JS_DEF_PROP_INT64:
            if (! JS_DefineProperty(ctx, ro, e.name, double(e.u.i64), attrs)) return -1;
            break;
        case JS_DEF_PROP_DOUBLE:
            if (! JS_DefineProperty(ctx, ro, e.name, e.u.f64, attrs)) return -1;
            break;
        case JS_DEF_PROP_UNDEFINED: {
            JS::RootedValue u(ctx);
            if (! JS_DefineProperty(ctx, ro, e.name, u, attrs)) return -1;
            break;
        }
        default: JS_ReportErrorASCII(ctx, "unsupported JSCFunctionListEntry type %d", e.def_type); return -1;
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
    Fac* f = lookupCx(reinterpret_cast<JSContext*>(rt));
    if (! f) return -1;
    if (id >= f->classes.size()) {
        f->classes.resize(id + 1);
        while (f->protos->get().length() <= id) f->protos->get().append(nullptr);
    }
    auto rec         = std::make_unique<ClassRec>();
    rec->def         = *def;
    rec->id          = id;
    rec->ops.finalize = classFinalize;
    rec->clasp.name  = rec->def.class_name;
    rec->clasp.flags = JSCLASS_HAS_RESERVED_SLOTS(2) | JSCLASS_FOREGROUND_FINALIZE;
    rec->clasp.cOps  = &rec->ops;
    f->classes[id]   = std::move(rec);
    return 0;
}

void impl_JS_SetClassProto(JSContext* ctx, JSClassID id, JSValue obj) {
    Fac* f = F(ctx);
    JS::Value v = takeSM(f, obj);
    f->protos->get()[id] = v.isObject() ? &v.toObject() : nullptr;
}

JSValue impl_JS_NewObjectClass(JSContext* ctx, JSClassID id) {
    Fac*      f   = F(ctx);
    ClassRec* rec = classOf(f, id);
    if (! rec) return JS_ThrowTypeError(ctx, "unknown class id %d", int(id));
    JS::RootedObject proto(ctx, f->protos->get()[id]);
    JS::RootedObject o(ctx, proto ? JS_NewObjectWithGivenProto(ctx, &rec->clasp, proto) : JS_NewObject(ctx, &rec->clasp));
    if (! o) return JS_EXCEPTION;
    JS_SetReservedSlot(o, 0, JS::PrivateValue(nullptr));
    JS_SetReservedSlot(o, 1, JS::PrivateValue(f));
    if (rec->def.exotic && rec->def.exotic->set_property && f->proxyCtor->get()) {
        JS::RootedObject handlers(ctx, f->classHandlers->get());
        JS::RootedValue  hv(ctx);
        JS_GetElement(ctx, handlers, id, &hv);
        if (! hv.isObject()) {
            JS::RootedObject handler(ctx, JS_NewPlainObject(ctx));
            JSFunction*      trap = js::NewFunctionWithReserved(ctx, proxySetTrap, 4, 0, "set");
            if (! handler || ! trap) return JS_EXCEPTION;
            js::SetFunctionNativeReserved(JS_GetFunctionObject(trap), 0, JS::PrivateValue(rec));
            JS::RootedValue tv(ctx, JS::ObjectValue(*JS_GetFunctionObject(trap)));
            JS_SetProperty(ctx, handler, "set", tv);
            hv = JS::ObjectValue(*handler);
            JS_SetElement(ctx, handlers, id, hv);
        }
        JS::RootedValueArray<2> pargs(ctx);
        pargs[0].setObject(*o);
        pargs[1].set(hv);
        JS::RootedValue pc(ctx, JS::ObjectValue(*f->proxyCtor->get()));
        JS::RootedObject proxy(ctx);
        if (! JS::Construct(ctx, pc, pargs, &proxy)) return JS_EXCEPTION;
        return fromObj(f, proxy);
    }
    return fromObj(f, o);
}

static int implSetOpaque(Fac* f, JSValueConst obj, void* opaque) {
    JSObject* o = unwrapProxy(objOf(f, obj));
    if (! o || JS::GetClass(o)->cOps == nullptr || JS::GetClass(o)->cOps->finalize != classFinalize) return -1;
    JS_SetReservedSlot(o, 0, JS::PrivateValue(opaque));
    return 0;
}

static void* implGetOpaque(Fac* f, JSValueConst obj, JSClassID id) {
    int t = JS_VALUE_GET_TAG(obj);
    if (t != JS_TAG_OBJECT && t != kTagRawObject) return nullptr;
    JSObject* o = unwrapProxy(objOf(f, obj));
    ClassRec* rec = classOf(f, id);
    if (! o || ! rec || JS::GetClass(o) != &rec->clasp) return nullptr;
    return JS::GetReservedSlot(o, 0).toPrivate();
}

// --- exceptions ----------------------------------------------------------------------------------------------------

// The runtime a value belongs to: the engine thread's own, or the one named in a box pointer.
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
    Fac*            f = F(ctx);
    JS::RootedValue v(ctx, takeSM(f, obj));
    JS_SetPendingException(ctx, v);
    return JS_EXCEPTION;
}

JSValue impl_JS_GetException(JSContext* ctx) {
    Fac*            f = F(ctx);
    JS::RootedValue v(ctx);
    if (! JS_IsExceptionPending(ctx) || ! JS_GetPendingException(ctx, &v)) return JS_NULL;
    JS_ClearPendingException(ctx);
    if (getenv("OWE_JS_DEBUG")) {
        JS::RootedString str(ctx, JS::ToString(ctx, v));
        JS::UniqueChars  u = str ? JS_EncodeStringToUTF8(ctx, str) : nullptr;
        fprintf(stderr, "[js] exception: %s (%s%s%s)\n", u ? u.get() : "<unprintable>", JS::InformalValueTypeName(v),
                v.isObject() ? " of class " : "", v.isObject() ? JS::GetClass(&v.toObject())->name : "");
        if (JS_IsExceptionPending(ctx)) JS_ClearPendingException(ctx);
    }
    return fromSM(f, v);
}

// --- JSON, array buffers --------------------------------------------------------------------------------------------

JSValue impl_JS_ParseJSON(JSContext* ctx, const char* buf, size_t len, const char* /*filename*/) {
    Fac*             f = F(ctx);
    JS::RootedString s(ctx, JS_NewStringCopyUTF8N(ctx, JS::UTF8Chars(buf, len)));
    if (! s) return JS_EXCEPTION;
    JS::RootedValue out(ctx);
    if (! JS_ParseJSON(ctx, s, &out)) return JS_EXCEPTION;
    return fromSM(f, out);
}

JSValue impl_JS_JSONStringify(JSContext* ctx, JSValueConst obj, JSValueConst replacer, JSValueConst space) {
    Fac*             f = F(ctx);
    JS::RootedObject global(ctx, f->global->get());
    JS::RootedValue  json(ctx), fn(ctx);
    if (! JS_GetProperty(ctx, global, "JSON", &json) || ! json.isObject()) return JS_EXCEPTION;
    JS::RootedObject jo(ctx, &json.toObject());
    if (! JS_GetProperty(ctx, jo, "stringify", &fn)) return JS_EXCEPTION;
    JS::RootedValueArray<3> args(ctx);
    args[0].set(toSM(f, obj));
    args[1].set(toSM(f, replacer));
    args[2].set(toSM(f, space));
    JS::RootedValue out(ctx);
    if (! JS_CallFunctionValue(ctx, jo, fn, args, &out)) return JS_EXCEPTION;
    return fromSM(f, out);
}

JSValue impl_JS_GetTypedArrayBuffer(JSContext* ctx, JSValueConst obj, size_t* poff, size_t* plen, size_t* pbpe) {
    Fac*      f = F(ctx);
    JSObject* o = objOf(f, obj);
    if (! o || ! JS_IsArrayBufferViewObject(o)) return JS_ThrowTypeError(ctx, "not a TypedArray");
    JS::RootedObject ro(ctx, o);
    bool             shared = false;
    JSObject*        buf = JS_GetArrayBufferViewBuffer(ctx, ro, &shared);
    if (! buf) return JS_EXCEPTION;
    if (poff) *poff = JS_GetArrayBufferViewByteOffset(o);
    if (plen) *plen = JS_GetArrayBufferViewByteLength(o);
    if (pbpe) {
        size_t bpe = 1;
        if (JS_IsTypedArrayObject(o)) bpe = JS_GetTypedArrayByteLength(o) / (JS_GetTypedArrayLength(o) ? JS_GetTypedArrayLength(o) : 1);
        *pbpe = bpe ? bpe : 1;
    }
    return fromObj(f, buf);
}

uint8_t* impl_JS_GetArrayBuffer(JSContext* ctx, size_t* psize, JSValueConst obj) {
    Fac*      f = F(ctx);
    JSObject* o = objOf(f, obj);
    if (! o || ! JS::IsArrayBufferObject(o)) {
        JS_ThrowTypeError(ctx, "not an ArrayBuffer");
        return nullptr;
    }
    bool     shared = false;
    uint8_t* data   = nullptr;
    JS::GetArrayBufferLengthAndData(o, psize, &shared, &data);
    return data;
}

// --- promises, jobs ------------------------------------------------------------------------------------------------

JSPromiseStateEnum impl_JS_PromiseState(JSContext* ctx, JSValueConst promise) {
    Fac*      f = F(ctx);
    JS::RootedObject o(ctx, objOf(f, promise));
    if (! o || ! JS::IsPromiseObject(o)) return JS_PROMISE_NOT_A_PROMISE;
    switch (JS::GetPromiseState(o)) {
    case JS::PromiseState::Pending: return JS_PROMISE_PENDING;
    case JS::PromiseState::Fulfilled: return JS_PROMISE_FULFILLED;
    default: return JS_PROMISE_REJECTED;
    }
}

JSValue impl_JS_PromiseResult(JSContext* ctx, JSValueConst promise) {
    Fac*      f = F(ctx);
    JS::RootedObject o(ctx, objOf(f, promise));
    if (! o || ! JS::IsPromiseObject(o)) return JS_UNDEFINED;
    JS::RootedValue v(ctx, JS::GetPromiseResult(o));
    return fromSM(f, v);
}

int impl_JS_ExecutePendingJob(JSRuntime* rt, JSContext** pctx) {
    JSContext* cx = reinterpret_cast<JSContext*>(rt);
    Fac*       f  = F(cx);
    if (pctx) *pctx = cx;
    return f->queue.drain(cx) > 0 ? 1 : 0;
}

// --- eval, modules -------------------------------------------------------------------------------------------------

static JSObject* resolveHook(JSContext* cx, JS::HandleValue, JS::HandleObject request) {
    Fac*             f = F(cx);
    JS::RootedString spec(cx, JS::GetModuleRequestSpecifier(cx, request));
    if (! spec) return nullptr;
    JS::UniqueChars name = JS_EncodeStringToUTF8(cx, spec);
    if (! name) return nullptr;
    auto it = f->moduleByName.find(name.get());
    if (it != f->moduleByName.end()) return it->second->module->get();
    if (! f->loader) {
        JS_ReportErrorASCII(cx, "no module loader for '%s'", name.get());
        return nullptr;
    }
    JSModuleDef* def = f->loader(cx, name.get(), f->loaderOpaque);
    if (! def) {
        if (! JS_IsExceptionPending(cx)) JS_ReportErrorASCII(cx, "could not load module '%s'", name.get());
        return nullptr;
    }
    auto* md = reinterpret_cast<ModuleDef*>(def);
    f->moduleByName[name.get()] = md;
    return md->module->get();
}

void impl_JS_SetModuleLoaderFunc(JSRuntime* rt, JSModuleNormalizeFunc*, JSModuleLoaderFunc* loader, void* opaque) {
    JSContext* cx = reinterpret_cast<JSContext*>(rt);
    Fac*       f  = F(cx);
    f->loader       = loader;
    f->loaderOpaque = opaque;
    JS::SetModuleResolveHook(JS_GetRuntime(cx), resolveHook);
}

JSValue impl_JS_Eval(JSContext* ctx, const char* input, size_t len, const char* filename, int flags) {
    Fac* f = F(ctx);
    JS::CompileOptions co(ctx);
    co.setFileAndLine(filename ? filename : "<eval>", 1);
    JS::SourceText<mozilla::Utf8Unit> st;
    if (! st.init(ctx, input, len, JS::SourceOwnership::Borrowed)) return JS_EXCEPTION;
    const bool module = (flags & JS_EVAL_TYPE_MASK) == JS_EVAL_TYPE_MODULE;
    if (! module) {
        JS::RootedValue rv(ctx);
        if (! JS::Evaluate(ctx, co, st, &rv)) return failed(ctx);
        return fromSM(f, rv);
    }
    JS::RootedObject mod(ctx, JS::CompileModule(ctx, co, st));
    if (! mod) return JS_EXCEPTION;
    auto* def   = new ModuleDef();
    def->module = std::make_unique<JS::PersistentRootedObject>(ctx, mod);
    def->name   = filename ? filename : "";
    f->modules.push_back(def);
    JSValue mv = JS_MKPTR(JS_TAG_MODULE, def);
    if (flags & JS_EVAL_FLAG_COMPILE_ONLY) return mv;
    return JS_EvalFunction(ctx, mv);
}

JSValue impl_JS_EvalFunction(JSContext* ctx, JSValue fun) {
    Fac* f = F(ctx);
    if (JS_VALUE_GET_TAG(fun) != JS_TAG_MODULE) {
        // Script bytecode is not produced by this facade (global code runs immediately), so there is nothing else to run.
        return fun;
    }
    auto*            def = static_cast<ModuleDef*>(JS_VALUE_GET_PTR(fun));
    JS::RootedObject mod(ctx, def->module->get());
    if (! JS::ModuleLink(ctx, mod)) return failed(ctx);
    JS::RootedValue rv(ctx);
    if (! JS::ModuleEvaluate(ctx, mod, &rv)) return failed(ctx);
    return fromSM(f, rv);
}

JSValue impl_JS_GetModuleNamespace(JSContext* ctx, JSModuleDef* m) {
    auto*            def = reinterpret_cast<ModuleDef*>(m);
    JS::RootedObject mod(ctx, def->module->get());
    JSObject*        ns = JS::GetModuleNamespace(ctx, mod);
    return ns ? fromObj(F(ctx), ns) : JS_EXCEPTION;
}

}  // extern "C"

// For Script.cpp: run fn(arg) on the runtime's engine thread (see OnEngine there).
extern "C" void owe_js_run_on_engine(JSRuntime* rt, void (*fn)(void*), void* arg) {
    Fac* f = lookupCx(reinterpret_cast<JSContext*>(rt));
    if (! f) return fn(arg);
    hop<void>(f, [&] { fn(arg); });
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
