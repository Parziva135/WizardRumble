#pragma once

#include "bakkesmod/plugin/bakkesmodplugin.h"

#include <functional>
#include <memory>
#include <string>

// Adapter for the public NetcodePlugin cvar interface. Call from the game thread.
// The dependency uses Rocket League's LAN chat channel; it must be installed by
// every participant. This adapter never installs dependencies or opens sockets.
class SpellTransport final {
public:
    using Handler = std::function<void(const std::string&, PriWrapper)>;
    static constexpr std::size_t MaxPayloadBytes = 110;

    SpellTransport(std::shared_ptr<CVarManagerWrapper> cvars,
                   std::shared_ptr<GameWrapper> game, Handler handler);
    ~SpellTransport();

    SpellTransport(const SpellTransport&) = delete;
    SpellTransport& operator=(const SpellTransport&) = delete;

    void Start();
    void Stop();
    void Tick();
    bool Available() const;
    bool Send(const std::string& payload);

private:
    void Receive(const std::string& incoming);
    bool InLanMatch() const;

    std::shared_ptr<CVarManagerWrapper> cvars_;
    std::shared_ptr<GameWrapper> game_;
    Handler handler_;
    bool running_ = false;
    bool subscribed_ = false;
};
