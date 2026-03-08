#pragma once

#include <napi.h>
#include <string>
#include <vector>
#include <cstdint>

class RenderWorker : public Napi::AsyncWorker {
public:
    RenderWorker(Napi::Env env,
                 Napi::Promise::Deferred deferred,
                 const std::string& optionsJson);

    void Execute() override;
    void OnOK() override;
    void OnError(const Napi::Error& error) override;

private:
    Napi::Promise::Deferred m_deferred;
    std::string m_optionsJson;
    std::vector<uint8_t> m_pngData;
    int m_failedTileCount = 0;
};
