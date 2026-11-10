#include "SpellTransport.h"

#include "bakkesmod/wrappers/ArrayWrapper.h"
#include "bakkesmod/wrappers/GameEvent/GameSettingPlaylistWrapper.h"
#include "bakkesmod/wrappers/GameEvent/ServerWrapper.h"

#include <charconv>
#include <cstdint>
#include <filesystem>
#include <string_view>
#include <utility>

namespace {
constexpr char IncomingCvar[] = "NETCODE_Message_In";
constexpr char OutgoingCvar[] = "NETCODE_Message_Out";
constexpr std::string_view ChannelPrefix = "[WizardRumble]";

bool IsPrintable(const std::string_view text) {
    for (const unsigned char character : text) {
        if (character < 32 || character > 126) return false;
    }
    return true;
}
}

SpellTransport::SpellTransport(std::shared_ptr<CVarManagerWrapper> cvars,
                             std::shared_ptr<GameWrapper> game, Handler handler)
    : cvars_(std::move(cvars)), game_(std::move(game)),
      handler_(std::move(handler)) {}

SpellTransport::~SpellTransport() { Stop(); }

void SpellTransport::Start() {
    if (running_) return;
    running_ = true;
    // Freeplay works without the LAN dependency. Avoid a misleading loader
    // error for a missing NetcodePlugin while this plugin itself loads fine.
    if (!cvars_->getCvar(IncomingCvar) || !cvars_->getCvar(OutgoingCvar)) {
        std::error_code error;
        const auto dependency = game_->GetBakkesModPath() / "plugins" / "NetcodePlugin.dll";
        if (std::filesystem::exists(dependency, error) && !error)
            cvars_->executeCommand("plugin load NetcodePlugin", false);
    }
    Tick();
    if (!Available()) {
        cvars_->log("WizardRumble: Freeplay is ready. For LAN multiplayer, install NetcodePlugin (Plugin Manager ID 166) on every PC.");
    }
}

void SpellTransport::Stop() {
    running_ = false;
    if (subscribed_) {
        auto incoming = cvars_->getCvar(IncomingCvar);
        if (incoming) incoming.removeOnValueChanged();
        subscribed_ = false;
    }
}

void SpellTransport::Tick() {
    if (!running_) return;
    auto incoming = cvars_->getCvar(IncomingCvar);
    auto outgoing = cvars_->getCvar(OutgoingCvar);
    if (!incoming || !outgoing) {
        if (subscribed_ && incoming) incoming.removeOnValueChanged();
        subscribed_ = false;
        return;
    }
    if (subscribed_) return;
    incoming.addOnValueChanged([this](std::string, CVarWrapper value) {
        if (running_) Receive(value.getStringValue());
    });
    subscribed_ = true;
    cvars_->log("WizardRumble: NetcodePlugin transport ready.");
}

bool SpellTransport::Available() const {
    return running_ && subscribed_ && cvars_->getCvar(IncomingCvar) &&
           cvars_->getCvar(OutgoingCvar);
}

bool SpellTransport::InLanMatch() const {
    if (game_->IsInReplay()) return false;
    auto server = game_->IsInOnlineGame() ? game_->GetOnlineGame()
                                        : game_->GetGameEventAsServer();
    if (!server) return false;
    auto playlist = server.GetPlaylist();
    return playlist && playlist.IsLanMatch();
}

bool SpellTransport::Send(const std::string& payload) {
    if (!Available() || !InLanMatch() || payload.empty() ||
        payload.size() > MaxPayloadBytes || !IsPrintable(payload)) return false;
    auto outgoing = cvars_->getCvar(OutgoingCvar);
    outgoing.setValue(std::string(ChannelPrefix) + payload);
    return true;
}

void SpellTransport::Receive(const std::string& incoming) {
    // Receive format: [PluginClass][decimal sender PRI address]payload.
    // The address is untrusted until matched to a PRI in the current match;
    // never construct a wrapper around a value supplied in a packet.
    if (!InLanMatch() || incoming.size() > 256 ||
        incoming.compare(0, ChannelPrefix.size(), ChannelPrefix) != 0) return;
    const std::size_t addressStart = ChannelPrefix.size();
    if (addressStart >= incoming.size() || incoming[addressStart] != '[') return;
    const auto addressEnd = incoming.find(']', addressStart + 1);
    if (addressEnd == std::string::npos || addressEnd <= addressStart + 1 ||
        addressEnd - addressStart > 21) return;

    std::uintptr_t senderAddress = 0;
    const char* first = incoming.data() + addressStart + 1;
    const char* last = incoming.data() + addressEnd;
    const auto parsed = std::from_chars(first, last, senderAddress, 10);
    if (parsed.ec != std::errc{} || parsed.ptr != last || senderAddress == 0) return;

    const std::string_view payload(incoming.data() + addressEnd + 1,
                                   incoming.size() - addressEnd - 1);
    if (payload.empty() || payload.size() > MaxPayloadBytes ||
        !IsPrintable(payload)) return;

    auto server = game_->IsInOnlineGame() ? game_->GetOnlineGame()
                                        : game_->GetGameEventAsServer();
    if (!server) return;
    auto players = server.GetPRIs();
    if (players.IsNull()) return;
    for (int index = 0; index < players.Count(); ++index) {
        auto player = players.Get(index);
        if (player && player.memory_address == senderAddress) {
            if (handler_) handler_(std::string(payload), player);
            return;
        }
    }
}
