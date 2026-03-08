#include <memory>
#include <string>
#include <emscripten.h>
#include <emscripten/bind.h>
#include <nlohmann/json.hpp>

#include "mapgenerator.h"

using json = nlohmann::json;

class WasmRunner {
    private:
        int m_jobCounter = 0;
        std::unordered_map<int, std::unique_ptr<MapGenerator>> m_jobs;

    public:
        WasmRunner() = default;

        int GenerateMap(const json& options) {
            try {
                int jobId = ++m_jobCounter;
                MapGeneratorOptions mapGeneratorOptions(options);

                auto jobCb = [this, jobId](void* data, size_t numBytes) {
                    int failedTileCount = 0;
                    auto it = m_jobs.find(jobId);
                    if (it != m_jobs.end()) {
                        failedTileCount = it->second->GetFailedTileCount();
                    }

                    if (data == nullptr || numBytes == 0) {
                        EM_ASM(
                            { Module.eventHandlers.onMapGeneratorJobError($0, UTF8ToString($1)); },
                            jobId,
                            "Render failed: tile downloads failed and partial render is disabled"
                        );
                    } else {
                        EM_ASM(
                            { Module.eventHandlers.onMapGeneratorJobDone($0, $1, $2, $3); },
                            jobId, data, numBytes, failedTileCount
                        );
                    }
                };

                m_jobs[jobId] = std::make_unique<MapGenerator>(mapGeneratorOptions, jobCb);

                int zoom = options["zoom"].template get<int>();
                double centerX = options["center"]["x"].template get<double>();
                double centerY = options["center"]["y"].template get<double>();
                m_jobs[jobId]->Render(std::make_tuple(centerX, centerY), zoom);

                return jobId;
            } catch (const std::exception& e) {
                printf("Error: %s\n", e.what());
                return -1;
            }
        }
};

static std::unique_ptr<WasmRunner> pRunner = std::make_unique<WasmRunner>();

int generateMap(const std::string& optionsAsString) {
    try {
       return pRunner->GenerateMap(json::parse(optionsAsString));
    } catch (const std::exception& e) {
        printf("Error: %s\n", e.what());
        return -1;
    }
}

EMSCRIPTEN_BINDINGS(my_module) {
    // emscripten::function("exit", &_exit);
    emscripten::function("generateMap", &generateMap);
}
