#include <memory>
#include <string>
#include <set>
#include <optional>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/bind.h>
#endif
#include <nlohmann/json.hpp>

#include "mapgenerator.h"

using json = nlohmann::json;

class WasmRunner {
    private:
        int jobCounter = 0;
        std::unordered_map<int, std::unique_ptr<MapGenerator>> jobs;

    public:
        WasmRunner() {}

        void run() {
            
        }

        int GenerateMap(const json& options) {
            jobCounter++;

            MapGeneratorOptions mapGeneratorOptions(options);

            jobs[jobCounter] = std::make_unique<MapGenerator>(mapGeneratorOptions);

            int zoom = options["zoom"].template get<int>();
            double centerX = options["center"]["x"].template get<double>();
            double centerY = options["center"]["y"].template get<double>();

            jobs[jobCounter]->Render(std::make_tuple(centerX, centerY), zoom);

            return jobCounter;
        }

        // void Exit() {
        //     emscripten_cancel_main_loop();
        //     emscripten_force_exit(0);
        // }
};

static std::unique_ptr<WasmRunner> pRunner = std::make_unique<WasmRunner>();

// int main(int argc, char* argv[]) {
//     pRunner->run();

//     return 0;
// }

// void _exit() {
//     pRunner->Exit();
// }

int generateMap(const std::string& optionsAsString) {
    return pRunner->GenerateMap(json::parse(optionsAsString));
}

EMSCRIPTEN_BINDINGS(my_module) {
    // emscripten::function("exit", &_exit);
    emscripten::function("generateMap", &generateMap);
}
