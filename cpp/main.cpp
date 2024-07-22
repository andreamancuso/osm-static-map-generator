#include <memory>
#include <string>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/bind.h>
#endif
#include <nlohmann/json.hpp>

#include "mapgenerator.h"

using json = nlohmann::json;

static auto cb = [](void* data, size_t numBytes) {
    EM_ASM_ARGS(
        { Module.eventHandlers.onMapGeneratorJobDone($0, $1); },
        data,
        numBytes
    );
};

class WasmRunner {
    private:
        int m_jobCounter = 0;
        std::unordered_map<int, std::unique_ptr<MapGenerator>> m_jobs;

    public:
        WasmRunner() = default;

        int GenerateMap(const json& options) {
            try {
                m_jobCounter++;
                MapGeneratorOptions mapGeneratorOptions(options);

                m_jobs[m_jobCounter] = std::make_unique<MapGenerator>(mapGeneratorOptions, cb);

                int zoom = options["zoom"].template get<int>();
                double centerX = options["center"]["x"].template get<double>();
                double centerY = options["center"]["y"].template get<double>();
                m_jobs[m_jobCounter]->Render(std::make_tuple(centerX, centerY), zoom);

                return m_jobCounter;
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
