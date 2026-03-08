#include <napi.h>
#include "render_worker.h"

static Napi::Value GenerateMap(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();

    if (info.Length() < 1) {
        Napi::Error::New(env, "Expected one argument (options JSON string)")
            .ThrowAsJavaScriptException();
        return env.Undefined();
    }

    if (!info[0].IsString()) {
        Napi::Error::New(env, "Expected a string argument (JSON)")
            .ThrowAsJavaScriptException();
        return env.Undefined();
    }

    std::string optionsJson = info[0].As<Napi::String>().Utf8Value();

    auto deferred = Napi::Promise::Deferred::New(env);
    auto* worker = new RenderWorker(env, deferred, optionsJson);
    worker->Queue();

    return deferred.Promise();
}

static Napi::Object Init(Napi::Env env, Napi::Object exports) {
    exports["generateMap"] = Napi::Function::New(env, GenerateMap);
    return exports;
}

NODE_API_MODULE(osm_static_map_generator, Init)
