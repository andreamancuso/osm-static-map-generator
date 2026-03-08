#include "render_worker.h"
#include "mapgenerator.h"

#include <nlohmann/json.hpp>
#include <tuple>

using json = nlohmann::json;

RenderWorker::RenderWorker(Napi::Env env,
                           Napi::Promise::Deferred deferred,
                           const std::string& optionsJson)
    : Napi::AsyncWorker(env)
    , m_deferred(deferred)
    , m_optionsJson(optionsJson)
{
}

void RenderWorker::Execute() {
    try {
        json options = json::parse(m_optionsJson);

        MapGeneratorOptions mapGeneratorOptions(options);

        mapGeneratorCallback cb = [this](void* data, size_t numBytes) {
            if (data != nullptr && numBytes > 0) {
                const auto* bytes = static_cast<const uint8_t*>(data);
                m_pngData.assign(bytes, bytes + numBytes);
            }
        };

        MapGenerator generator(mapGeneratorOptions, cb);

        if (!options.contains("center") || !options["center"].is_object()) {
            SetError("Options must contain a 'center' object with 'x' and 'y' fields");
            return;
        }
        if (!options.contains("zoom") || !options["zoom"].is_number()) {
            SetError("Options must contain a 'zoom' number");
            return;
        }

        double centerX = options["center"]["x"].get<double>();
        double centerY = options["center"]["y"].get<double>();
        int zoom = options["zoom"].get<int>();

        generator.Render(std::make_tuple(centerX, centerY), zoom);

        m_failedTileCount = generator.GetFailedTileCount();

        if (m_pngData.empty()) {
            SetError("Render failed: all tile downloads failed or partial render is disabled");
            return;
        }
    } catch (const nlohmann::json::exception& e) {
        SetError(std::string("JSON parse error: ") + e.what());
    } catch (const std::exception& e) {
        SetError(std::string("Render error: ") + e.what());
    }
}

void RenderWorker::OnOK() {
    Napi::Env env = Env();
    auto buffer = Napi::Buffer<uint8_t>::Copy(env, m_pngData.data(), m_pngData.size());

    auto result = Napi::Object::New(env);
    result.Set("buffer", buffer);
    result.Set("failedTileCount", Napi::Number::New(env, m_failedTileCount));

    m_deferred.Resolve(result);
}

void RenderWorker::OnError(const Napi::Error& error) {
    m_deferred.Reject(error.Value());
}
