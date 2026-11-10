#include "WizardRumble.h"
#include "SpellReplayScene.h"

#include "bakkesmod/wrappers/ArrayWrapper.h"
#include "bakkesmod/wrappers/GameEvent/GameSettingPlaylistWrapper.h"
#include "bakkesmod/wrappers/GameEvent/ServerWrapper.h"
#include "bakkesmod/wrappers/GameEvent/ReplayDirectorWrapper.h"
#include "bakkesmod/wrappers/GameEvent/ReplaySoccarWrapper.h"
#include "bakkesmod/wrappers/GameObject/BallWrapper.h"
#include "bakkesmod/wrappers/GameObject/GoalWrapper.h"
#include "bakkesmod/wrappers/GameObject/CarWrapper.h"
#include "bakkesmod/wrappers/GameObject/PriWrapper.h"
#include "bakkesmod/wrappers/PlayerControllerWrapper.h"
#include "bakkesmod/wrappers/Engine/UnrealStringWrapper.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

BAKKESMOD_PLUGIN(WizardRumble, "Wizard Rumble", "0.9.2", PLUGINTYPE_FREEPLAY)

namespace {
constexpr char InputHook[] = "Function TAGame.Car_TA.SetVehicleInput";
constexpr char DemoHook[] = "Function TAGame.Car_TA.Demolish";
constexpr char MatchTickHook[] = "Function GameEvent_Soccar_TA.Active.Tick";
constexpr char GoalReplayBeginHook[] = "Function GameEvent_Soccar_TA.ReplayPlayback.BeginState";
constexpr char GoalReplayEndHook[] = "Function GameEvent_Soccar_TA.ReplayPlayback.EndState";
wizard::Vec3 Core(Vector v) { return {v.X, v.Y, v.Z}; }
Vector Engine(wizard::Vec3 v) { return {v.x, v.y, v.z}; }
double Elapsed(std::chrono::steady_clock::time_point since) {
    return std::chrono::duration<double>(std::chrono::steady_clock::now() - since).count();
}
bool Live(CarWrapper car) {
    return car && !car.GetbHidden() && !car.GetbDeleteMe();
}
int Milliseconds(double seconds) {
    return std::clamp(static_cast<int>(std::round(seconds * 1000)), 1, 10000);
}
std::string CanonicalSpell(const std::string& identifier) {
    if (identifier == "arresto_momentum") return "petrificus_totalus";
    if (identifier == "expecto_petronum") return "expecto_patronum";
    if (identifier == "depulso") return "bombarda_maxima";
    if (identifier == "confundo") return "obliviate";
    return identifier;
}
}

void WizardRumble::onLoad() {
    cvarManager->registerCvar("wr_enabled", "1", "Host: enable spells in freeplay and LAN matches", true, true, 0, true, 1);
    cvarManager->registerCvar("wr_cooldown", "10", "Host: seconds between random spells (new cooldowns)", true, true, 1, true, 60);
    cvarManager->registerCvar("wr_range", std::to_string(wizard::DefaultSpellRange), "Host: maximum spell range in Unreal units", true, true, wizard::MinSpellRange, true, wizard::MaxSpellRange);
    cvarManager->registerCvar("wr_range_revision", "0", "Wizard Rumble range default migration", false, true, 0, true, 1);
    // Accept old saved configuration without restoring the former fixed hold.
    cvarManager->registerCvar("wr_lift_seconds", "3", "Legacy Leviosa duration (unused)", false, true, 1, true, 6);
    cvarManager->registerCvar("wr_lift_height", "650", "Legacy Leviosa height (unused)", false, true, 200, true, 1100);
    cvarManager->registerCvar("wr_hud", "1", "Show the spell HUD", true, true, 0, true, 1);
    cvarManager->registerCvar("wr_hud_scale", "1", "Scale the Rumble-style spell slot", true, true, 0.7f, true, 1.6f);
    cvarManager->registerCvar("wr_vfx", "1", "Show animated spell and freeze effects", true, true, 0, true, 1);
    cvarManager->registerCvar("wr_debug", "0", "Log timed hold poses and release reasons", true, true, 0, true, 1);
    cvarManager->registerCvar("wr_sfx", "1", "Play local spell sound effects", true, true, 0, true, 1);
    cvarManager->registerCvar("wr_sfx_volume", "1", "Local spell sound volume (1 is full loudness)", true, true, 0, true, 1);
    const auto loadGlow = [this]() {
        if (!glow_.Load(gameWrapper->GetDataFolder() / "wizardrumble" / "textures"))
            cvarManager->log("Wizard Rumble glow textures: " + glow_.Error());
    };
    loadGlow();
    cvarManager->registerNotifier("wr_glow_reload", [loadGlow](std::vector<std::string>) { loadGlow(); },
        "Reload the soft glow texture library from data/wizardrumble/textures", PERMISSION_ALL);
    const auto loadAudio = [this]() {
        StopSounds();
        const auto folder = gameWrapper->GetDataFolder() / "wizardrumble" / "audio";
        if (!audio_.Load(folder / "avada_kedavra.wav")) cvarManager->log("Avada audio: " + audio_.Error());
        if (!leviosaVoice_.Load(folder / "leviosa_cast.wav")) cvarManager->log("Leviosa voice: " + leviosaVoice_.Error());
        if (!leviosaFloat_.Load(folder / "leviosa_float.wav")) cvarManager->log("Leviosa floating: " + leviosaFloat_.Error());
        if (!slugsVoice_.Load(folder / "eat_slugs.wav")) cvarManager->log("Eat Slugs voice: " + slugsVoice_.Error());
        if (!stupefyVoice_.Load(folder / "stupefy.wav")) cvarManager->log("Stupefy voice: " + stupefyVoice_.Error());
        if (!petrificusVoice_.Load(folder / "petrificus_totalus.wav")) cvarManager->log("Petrificus voice: " + petrificusVoice_.Error());
        if (!protegoVoice_.Load(folder / "protego.wav")) cvarManager->log("Protego voice: " + protegoVoice_.Error());
        if (!bombardaVoice_.Load(folder / "bombarda_maxima.wav")) cvarManager->log("Bombarda voice: " + bombardaVoice_.Error());
        if (!patronumVoice_.Load(folder / "expecto_patronum.wav")) cvarManager->log("Patronum voice: " + patronumVoice_.Error());
        if (!obliviateVoice_.Load(folder / "obliviate.wav")) cvarManager->log("Obliviate voice: " + obliviateVoice_.Error());
        if (!accioVoice_.Load(folder / "accio.wav")) cvarManager->log("Accio voice: " + accioVoice_.Error());
        if (!imperioVoice_.Load(folder / "imperio.wav")) cvarManager->log("Imperio voice: " + imperioVoice_.Error());
        cvarManager->log("Wizard Rumble audio ready: Avada=" + std::to_string(audio_.Ready()) +
            " Leviosa voice=" + std::to_string(leviosaVoice_.Ready()) +
            " Leviosa floating=" + std::to_string(leviosaFloat_.Ready()) +
            " Eat Slugs=" + std::to_string(slugsVoice_.Ready()) + " Stupefy=" + std::to_string(stupefyVoice_.Ready()) +
            " Petrificus=" + std::to_string(petrificusVoice_.Ready()) + " Protego=" + std::to_string(protegoVoice_.Ready()) +
            " Bombarda=" + std::to_string(bombardaVoice_.Ready()) + " Patronum=" + std::to_string(patronumVoice_.Ready()) +
            " Obliviate=" + std::to_string(obliviateVoice_.Ready()) +
            " Accio=" + std::to_string(accioVoice_.Ready()) + " Imperio=" + std::to_string(imperioVoice_.Ready()));
    };
    loadAudio();
    cvarManager->registerNotifier("wr_audio_reload", [loadAudio](std::vector<std::string>) { loadAudio(); },
        "Reload all spell WAV files from data/wizardrumble/audio", PERMISSION_ALL);
    cvarManager->registerNotifier("wr_audio_test", [this](std::vector<std::string> arguments) {
        const std::string sound = arguments.size() > 1 ? arguments[1] : "avada_kedavra";
        SpellAudio* clip = sound == "avada_kedavra" ? &audio_ : sound == "leviosa" ? &leviosaVoice_ :
            sound == "leviosa_float" ? &leviosaFloat_ : sound == "eat_slugs" ? &slugsVoice_ :
            sound == "stupefy" ? &stupefyVoice_ : sound == "petrificus_totalus" ? &petrificusVoice_ :
            sound == "protego" ? &protegoVoice_ : sound == "bombarda_maxima" ? &bombardaVoice_ :
            sound == "expecto_patronum" ? &patronumVoice_ : sound == "obliviate" ? &obliviateVoice_ :
            sound == "accio" ? &accioVoice_ : sound == "imperio" ? &imperioVoice_ : nullptr;
        if (!clip) { cvarManager->log("wr_audio_test avada_kedavra | leviosa | leviosa_float | eat_slugs | stupefy | petrificus_totalus | protego | bombarda_maxima | expecto_patronum | obliviate | accio | imperio"); return; }
        if (!clip->Play(std::clamp(Setting("wr_sfx_volume", 1), 0.0f, 1.0f)))
            cvarManager->log("Wizard Rumble audio: " + clip->Error());
    }, "Preview one spell clip locally", PERMISSION_ALL);
    cvarManager->registerNotifier("wr_cast", [this](std::vector<std::string>) { RequestCast(); }, "Cast the held spell", PERMISSION_ALL);
    cvarManager->registerNotifier("wr_status", [this](std::vector<std::string>) { Status(); }, "Print spell and multiplayer diagnostics", PERMISSION_ALL);
    cvarManager->registerNotifier("wr_practice", [this](std::vector<std::string> arguments) { Practice(arguments); },
        "Freeplay only: wr_practice <spell identifier> grants a spell for testing", PERMISSION_ALL);
    cvarManager->registerNotifier("wr_selftest", [this](std::vector<std::string> arguments) { SelfTest(arguments); },
        "Freeplay only: apply leviosa, petrificus_totalus, obliviate or eat_slugs to yourself", PERMISSION_ALL);
    cvarManager->registerNotifier("wr_bot_test", [this](std::vector<std::string> arguments) { BotTest(arguments); },
        "Host-and-bots LAN only: test leviosa, petrificus_totalus or imperio on an opposing bot", PERMISSION_ALL);
    transport_ = std::make_unique<SpellTransport>(cvarManager, gameWrapper,
        [this](const std::string& payload, PriWrapper sender) { Receive(payload, sender); });
    transport_->Start();
    // R is the user's chosen cast key. Restore its previous assignment on
    // unload, unless the user changes it while the plugin is loaded.
    previousRBind_ = cvarManager->getBindStringForKey("R");
    cvarManager->setBind("R", "wr_cast");
    // Migrate only this plugin's old Q assignment; preserve unrelated binds.
    if (cvarManager->getBindStringForKey("Q") == "wr_cast") cvarManager->removeBind("Q");
    gameWrapper->HookEventWithCaller<CarWrapper>(InputHook,
        [this](CarWrapper car, void* parameters, std::string) { VehicleInput(car, parameters); });
    gameWrapper->HookEventPost(MatchTickHook, [this](std::string) { Tick(); });
    gameWrapper->HookEventWithCallerPost<ServerWrapper>(GoalReplayBeginHook,
        [this](ServerWrapper server, void*, std::string) { BeginGoalReplay(server); });
    gameWrapper->HookEventPost(GoalReplayEndHook, [this](std::string) { EndGoalReplay(); });
    gameWrapper->HookEventWithCaller<CarWrapper>(DemoHook,
        [this](CarWrapper car, void*, std::string) { BeforeDemolish(car); });
    // The drawable callback also runs on the game thread. It maintains state
    // while car-input callbacks are stopped by pause or spectator transitions.
    gameWrapper->RegisterDrawable([this](CanvasWrapper canvas) { Tick(); Draw(canvas); });
    cvarManager->log("Wizard Rumble 0.9.2 loaded: voiced Accio and Imperio charges, solid luminous Octane Patronus. R / wr_cast casts. Every PC needs NetcodePlugin 166 for multiplayer.");
}

void WizardRumble::onUnload() {
    StopSounds();
    gameWrapper->UnhookEvent(InputHook);
    gameWrapper->UnhookEvent(DemoHook);
    gameWrapper->UnhookEventPost(MatchTickHook);
    gameWrapper->UnhookEventPost(GoalReplayBeginHook);
    gameWrapper->UnhookEventPost(GoalReplayEndHook);
    gameWrapper->UnregisterDrawables();
    auto server = gameWrapper->IsInOnlineGame() ? gameWrapper->GetOnlineGame() : gameWrapper->GetGameEventAsServer();
    if (server && server.memory_address == session_) {
        StopRicochet(server, false);
        if (Authoritative(server)) ClearEffects(server);
    }
    if (transport_) { transport_->Stop(); transport_.reset(); }
    if (cvarManager->getBindStringForKey("R") == "wr_cast") {
        if (previousRBind_.empty()) cvarManager->removeBind("R");
        else cvarManager->setBind("R", previousRBind_);
    }
    glow_.Shutdown();
}

float WizardRumble::Setting(const std::string& name, float fallback) {
    auto cvar = cvarManager->getCvar(name);
    const float value = cvar ? cvar.getFloatValue() : fallback;
    return std::isfinite(value) ? value : fallback;
}

float WizardRumble::SpellRange(ServerWrapper server) {
    return Authoritative(server) ? std::clamp(Setting("wr_range", wizard::DefaultSpellRange),
        wizard::MinSpellRange, wizard::MaxSpellRange) : hostRange_;
}

ServerWrapper WizardRumble::CurrentServer() {
    if (gameWrapper->IsInReplay() || gameWrapper->IsInCustomTraining()) return ServerWrapper(0);
    if (gameWrapper->IsInOnlineGame()) return gameWrapper->GetOnlineGame();
    if (gameWrapper->IsInGame() || gameWrapper->IsInFreeplay()) return gameWrapper->GetGameEventAsServer();
    return ServerWrapper(0);
}

bool WizardRumble::Supported(ServerWrapper server) {
    if (!server || gameWrapper->IsInReplay() || gameWrapper->IsInCustomTraining()) return false;
    if (gameWrapper->IsInFreeplay() && !gameWrapper->IsInOnlineGame()) return true;
    auto playlist = server.GetPlaylist();
    return playlist && playlist.IsLanMatch();
}

bool WizardRumble::Authoritative(ServerWrapper server) {
    return Supported(server) && !gameWrapper->IsInOnlineGame() && server.HasAuthority();
}

bool WizardRumble::Active(ServerWrapper server) {
    return !goalReplay_ && server && !server.GetbMatchEnded() && !gameWrapper->IsPaused() &&
        (gameWrapper->IsInFreeplay() || server.GetbRoundActive());
}

PriWrapper WizardRumble::LocalPlayer() {
    auto controller = gameWrapper->GetPlayerController();
    return controller ? controller.GetPRI() : PriWrapper(0);
}

std::vector<PriWrapper> WizardRumble::Players(ServerWrapper server) {
    std::vector<PriWrapper> result;
    if (!server) return result;
    auto players = server.GetPRIs();
    if (players.IsNull()) return result;
    for (int i = 0; i < players.Count(); ++i) {
        auto player = players.Get(i);
        if (player && !player.IsSpectator() && player.GetPlayerID() >= 0 &&
            player.GetPlayerID() <= wizard::MaxPlayerId) result.push_back(player);
    }
    return result;
}

PriWrapper WizardRumble::FindPlayer(ServerWrapper server, int id) {
    if (!server) return PriWrapper(0);
    auto players = server.GetPRIs();
    if (players.IsNull()) return PriWrapper(0);
    // Cleanup must also find a player who has just switched to spectator.
    for (int i = 0; i < players.Count(); ++i) {
        auto player = players.Get(i);
        if (player && player.GetPlayerID() == id) return player;
    }
    return PriWrapper(0);
}

PriWrapper WizardRumble::FindPlayerByAddress(ServerWrapper server, std::uintptr_t address) {
    if (!server || !address) return PriWrapper(0);
    auto players = server.GetPRIs();
    if (players.IsNull()) return PriWrapper(0);
    for (int i = 0; i < players.Count(); ++i) {
        auto player = players.Get(i);
        if (player && player.memory_address == address) return player;
    }
    return PriWrapper(0);
}

PriWrapper WizardRumble::SceneTarget(ServerWrapper server, int id, wizard::Vec3 position) {
    PriWrapper closest(0);
    float distance = std::numeric_limits<float>::max();
    for (auto player : Players(server)) {
        auto car = player.GetCar();
        if (player.GetPlayerID() != id || !Live(car)) continue;
        const float candidate = (Core(car.GetLocation()) - position).LengthSquared();
        if (candidate < distance) { distance = candidate; closest = player; }
    }
    return closest;
}

PriWrapper WizardRumble::FindPlayerByCar(ServerWrapper server, std::uintptr_t address) {
    if (!address) return PriWrapper(0);
    for (auto player : Players(server)) {
        auto car = player.GetCar();
        if (Live(car) && car.memory_address == address) return player;
    }
    return PriWrapper(0);
}

PriWrapper WizardRumble::Target(ServerWrapper server, PriWrapper caster) {
    auto car = caster.GetCar();
    if (!Live(car)) return PriWrapper(0);
    std::vector<wizard::TargetCandidate> candidates;
    std::vector<PriWrapper> targets;
    for (auto player : Players(server)) {
        auto other = player.GetCar();
        if (!Live(other) || player.memory_address == caster.memory_address || player.GetTeamNum() == caster.GetTeamNum()) continue;
        candidates.push_back({static_cast<int>(targets.size()), Core(other.GetLocation()), true});
        targets.push_back(player);
    }
    auto selected = wizard::SelectTarget(Core(car.GetLocation()), Core(RotatorToVector(car.GetRotation())),
        candidates, SpellRange(server));
    return selected ? targets.at(*selected) : PriWrapper(0);
}

void WizardRumble::ResetSession() {
    ricochet_.reset(); // The old world's ball must never be dereferenced here.
    replayHistory_.Clear(); replayAudio_.Clear(); goalReplay_ = false; replayDebugAt_ = -1;
    deck_.Clear(); identities_.clear(); peers_.clear(); sentSpells_.clear();
    lastRequest_.clear(); lifts_.clear(); confusions_.clear(); imperios_.clear(); walls_.clear(); patronums_.clear(); visuals_.Reset();
    liftProbe_.reset();
    summon_.reset(); charges_.clear(); chargeViews_.clear(); slugs_.clear(); StopSounds();
    previousBall_.reset(); previousBallActor_ = 0;
    gameClock_ = 0; lastHoldUpdate_ = -1; wasActive_ = false; wasEnabled_ = false;
    lastTick_ = Clock::now();
    localSpell_ = wizard::Spell::None; localRemaining_ = 0; localCooldown_ = 10; stateClock_ = 0;
    hostRange_ = wizard::DefaultSpellRange; sentRange_ = -1;
    hostEnabled_ = false; synchronized_ = false; feedback_.clear();
    lastHello_ = Clock::time_point{};
}

void WizardRumble::Heartbeat() {
    if (Elapsed(lastHeartbeat_) < 0.25) return;
    lastHeartbeat_ = Clock::now();
    if (transport_) transport_->Tick();
    auto server = CurrentServer();
    const auto address = Supported(server) ? server.memory_address : 0;
    if (address != session_) {
        // An old world has been destroyed; never dereference its saved actors.
        ResetSession(); session_ = address;
    }
    if (!address || Authoritative(server)) return;
    if (synchronized_ && Elapsed(stateReceived_) > 12) {
        synchronized_ = false;
        hostEnabled_ = false;
        localSpell_ = wizard::Spell::None;
        visuals_.ClearEffects(); walls_.clear(); patronums_.clear(); confusions_.clear(); imperios_.clear(); chargeViews_.clear(); slugs_.clear(); StopSounds();
    }
    if (Elapsed(lastHello_) >= 5 && transport_ && transport_->Available()) {
        lastHello_ = Clock::now();
        wizard::Message hello; hello.kind = wizard::MessageKind::Hello;
        transport_->Send(wizard::SerializeMessage(hello));
    }
}

void WizardRumble::Tick() {
    if (ticking_ || goalReplay_) return;
    ticking_ = true;
    struct TickGuard {
        bool& active;
        ~TickGuard() { active = false; }
    } guard{ticking_};
    Heartbeat();
    auto server = CurrentServer();
    if (!Supported(server)) return;
    // Wait until a supported session so startup config has restored saved cvars.
    // Migrate only the old default once; later deliberate 4,500/custom choices survive.
    if (!rangeDefaultsChecked_) {
        auto revision = cvarManager->getCvar("wr_range_revision");
        auto range = cvarManager->getCvar("wr_range");
        if (revision && range && revision.getIntValue() < 1) {
            if (range.getFloatValue() == wizard::LegacySpellRange)
                range.setValue(wizard::DefaultSpellRange);
            revision.setValue(1);
        }
        rangeDefaultsChecked_ = true;
    }
    if (session_ != server.memory_address) { ResetSession(); session_ = server.memory_address; }
    const auto now = Clock::now();
    const double delta = lastTick_ == Clock::time_point{} ? 0 :
        std::chrono::duration<double>(now - lastTick_).count();
    lastTick_ = now;
    const bool host = Authoritative(server);
    const bool enabled = host ? Setting("wr_enabled", 1) > 0.5f : hostEnabled_;
    const bool active = Active(server);
    const float range = SpellRange(server);
    const bool rangeChanged = host && range != sentRange_;
    // Actor/world clock stalls and a slow frame must not strand a held car.
    // Pauses and inactive rounds still stop the active-match clock.
    gameClock_ = wizard::AdvanceActiveClock(gameClock_, delta, wasActive_, active);
    if ((wasActive_ && !active) || (wasEnabled_ && !enabled)) StopRecordedSounds(server);
    UpdateLocalEffects(server, enabled && active);
    if (!host) {
        if (enabled && active) { UpdateImperios(server); UpdatePatronums(server); }
        if (enabled && (active || wasActive_)) RecordScene(server, !active);
        wasEnabled_ = enabled; wasActive_ = active;
        if (!enabled || !active) {
            confusions_.clear(); imperios_.clear(); chargeViews_.clear(); patronums_.clear(); visuals_.ClearEffects();
        }
        for (auto it = chargeViews_.begin(); it != chargeViews_.end();) {
            if (gameClock_ >= it->second.ends + 0.3) it = chargeViews_.erase(it); else ++it;
        }
        for (auto it = confusions_.begin(); it != confusions_.end();) {
            auto player = FindPlayerByAddress(server, it->first);
            auto car = player ? player.GetCar() : CarWrapper(0);
            if (gameClock_ >= it->second.ends || !player || !Live(car) ||
                player.memory_address != it->second.pri || car.memory_address != it->second.car)
                it = confusions_.erase(it);
            else ++it;
        }
        for (auto it = walls_.begin(); it != walls_.end();) {
            if (gameClock_ >= it->second.ends) it = walls_.erase(it); else ++it;
        }
        return;
    }
    if ((!enabled && wasEnabled_) || (!active && wasActive_)) {
        if (enabled && !active) RecordScene(server, true);
        ClearEffects(server); deck_.Clear(); sentSpells_.clear();
    }
    const bool enabledChanged = enabled != wasEnabled_;
    const bool activeChanged = active != wasActive_;
    wasEnabled_ = enabled; wasActive_ = active;
    deck_.SetCooldown(Setting("wr_cooldown", 10));
    std::vector<int> ids;
    for (auto player : Players(server)) {
        const int id = player.GetPlayerID();
        ids.push_back(id);
        auto previous = identities_.find(id);
        if (previous != identities_.end() && previous->second != player.memory_address &&
            !FindPlayerByAddress(server, previous->second)) {
            std::vector<int> keep;
            for (auto existing : Players(server)) if (existing.GetPlayerID() != id) keep.push_back(existing.GetPlayerID());
            deck_.RemoveMissing(keep);
            peers_.erase(id); sentSpells_.erase(id);
            // Holds are owned by the actual PRI/car pair, not a roster ID.
            // UpdateEffects checks whether that exact actor still exists.
            ReleaseConfusion(previous->second);
            if (walls_.count(id)) { CancelScene(wizard::Spell::Protego, id, -1); walls_.erase(id); }
        }
        identities_[id] = player.memory_address;
        if (enabled && active) deck_.EnsurePlayer(id, gameClock_);
    }
    deck_.RemoveMissing(ids);
    for (auto it = identities_.begin(); it != identities_.end();) {
        if (std::find(ids.begin(), ids.end(), it->first) == ids.end()) {
            peers_.erase(it->first); sentSpells_.erase(it->first); lastRequest_.erase(it->first);
            ReleaseConfusion(it->second);
            if (walls_.count(it->first)) {
                CancelScene(wizard::Spell::Protego, it->first, -1); walls_.erase(it->first);
            }
            it = identities_.erase(it);
        } else ++it;
    }
    if (enabled && active) { deck_.Update(gameClock_); UpdateCharges(server); UpdateEffects(server); UpdatePatronums(server); CheckLiftProbe(server); }
    if (enabled && active) RecordScene(server);
    for (auto player : Players(server)) {
        const int id = player.GetPlayerID();
        const auto spell = deck_.ReadyFor(id);
        const auto sent = sentSpells_.find(id);
        if (enabledChanged || activeChanged || rangeChanged || sent == sentSpells_.end() || sent->second != spell)
            SendState(server, player);
    }
    sentRange_ = range;
}

void WizardRumble::ClearEffects(ServerWrapper server) {
    if (!server || !Authoritative(server)) return;
    StopRicochet(server, false);
    StopRecordedSounds(server);
    while (!lifts_.empty()) ReleaseHold(server, lifts_.begin()->first, "match cleared/paused/disabled/unloaded");
    confusions_.clear(); imperios_.clear();
    liftProbe_.reset();
    summon_.reset(); charges_.clear(); chargeViews_.clear(); slugs_.clear(); StopSounds();
    walls_.clear(); patronums_.clear(); previousBall_.reset(); previousBallActor_ = 0;
    visuals_.ClearEffects();
    wizard::Message clear; clear.kind = wizard::MessageKind::Clear;
    if (transport_) transport_->Send(wizard::SerializeMessage(clear));
}

void WizardRumble::UpdateEffects(ServerWrapper server) {
    UpdateRicochet(server);
    UpdateImperios(server);
    UpdateSummon(server);
    const bool moveHolds = lastHoldUpdate_ < 0 || gameClock_ - lastHoldUpdate_ >= 1.0 / 120.0;
    if (moveHolds) lastHoldUpdate_ = gameClock_;
    for (auto it = lifts_.begin(); it != lifts_.end();) {
        auto player = FindPlayerByAddress(server, it->second.pri);
        auto car = player ? player.GetCar() : CarWrapper(0);
        auto& lift = it->second;
        if (!player || !car || player.memory_address != lift.pri || car.memory_address != lift.car) {
            cvarManager->log("Hold cancelled: target actor changed/missing, id=" + std::to_string(lift.id));
            CancelScene(lift.spell, lift.caster, lift.id, true, 0, lift.car);
            it = lifts_.erase(it); continue;
        }
        if (player.IsSpectator() || !Live(car) || gameClock_ >= lift.ends) {
            const auto pri = it->first;
            ++it;
            ReleaseHold(server, pri, gameClock_ >= lift.ends ? "deadline" : "spectator/hidden");
            continue;
        }
        if (lift.spell == wizard::Spell::Leviosa && LeviosaRoofContact(car)) {
            const auto pri = it->first;
            ++it;
            ReleaseHold(server, pri, "roof contact");
            continue;
        }
        if (moveHolds) {
            lift.observedPosition = Core(car.GetCurrentRBLocation());
            lift.observedSpeed = Core(car.GetVelocity()).Length();
            lift.observedAt = gameClock_;
            HoldPhysics(car, lift);
        }
        if (Setting("wr_debug", 0) > 0.5f && gameClock_ >= lift.nextLog) {
            const auto desired = wizard::SuspensionPosition(Core(lift.start), lift.rise, gameClock_ - lift.began);
            const auto actual = Core(car.GetCurrentRBLocation());
            cvarManager->log("Hold sample: id=" + std::to_string(lift.id) + " elapsed=" +
                std::to_string(gameClock_ - lift.began) + " left=" + std::to_string(lift.ends - gameClock_) +
                " desiredZ=" + std::to_string(desired.z) + " actualZ=" + std::to_string(actual.z) +
                " before_error=" + std::to_string((lift.observedPosition - desired).Length()) +
                " after_error=" + std::to_string((actual - desired).Length()) + " nativeFrozen=" +
                std::to_string(car.GetbFrozen() != 0));
            lift.nextLog = gameClock_ + 0.45;
        }
        ++it;
    }
    for (auto it = confusions_.begin(); it != confusions_.end();) {
        auto player = FindPlayerByAddress(server, it->first);
        auto car = player ? player.GetCar() : CarWrapper(0);
        if (gameClock_ >= it->second.ends || !player || player.IsSpectator() || !Live(car) ||
            player.memory_address != it->second.pri || car.memory_address != it->second.car) {
            const auto pri = it->first;
            ++it;
            ReleaseConfusion(pri);
        } else {
            ++it;
        }
    }
    for (auto it = walls_.begin(); it != walls_.end();) {
        if (gameClock_ >= it->second.ends) {
            // Clients already own this deadline. Do not cancel a newer
            // Protego wind-up when its caster's older wall expires.
            it = walls_.erase(it);
        } else ++it;
    }
    auto ball = server.GetBall();
    if (!ball) { previousBall_.reset(); previousBallActor_ = 0; return; }
    auto position = Core(ball.GetLocation());
    if (previousBallActor_ != ball.memory_address) previousBall_.reset();
    if (previousBall_ && !ball.GetbFrozen() && !walls_.empty()) {
        float radius = ball.GetRadius();
        if (!std::isfinite(radius) || radius < 10 || radius > 1000) radius = 93;
        std::optional<wizard::BarrierHit> first;
        WallState* struck = nullptr;
        for (auto& entry : walls_) {
            auto hit = wizard::SweepBarrier(*previousBall_, position, Core(ball.GetVelocity()), entry.second.geometry, radius);
            if (hit && (!first || hit->fraction < first->fraction)) { first = hit; struck = &entry.second; }
        }
        if (first && struck) {
            // A ward wins over the magnetic approach instead of being pulled through.
            StopSummon();
            ball.SetLocation(Engine(first->position)); ball.SetVelocity(Engine(first->velocity));
            ball.ForceNetUpdate2();
            position = first->position;
            if (struck->impact < 0 || gameClock_ - struck->impact >= 0.12) {
                struck->impact = gameClock_;
                wizard::Message impact; impact.kind = wizard::MessageKind::Impact;
                impact.casterId = struck->caster; impact.source = first->contact;
                if (transport_) transport_->Send(wizard::SerializeMessage(impact));
            }
        }
    }
    previousBall_ = position; previousBallActor_ = ball.memory_address;
}

std::optional<wizard::Vec3> WizardRumble::OpponentGoal(ServerWrapper server, int team) {
    if (!server || team < 0 || team > 1) return std::nullopt;
    auto goals = server.GetGoals();
    if (goals.IsNull()) return std::nullopt;
    for (int i = 0; i < goals.Count(); ++i) {
        auto goal = goals.Get(i);
        if (!goal || goal.GetTeamNum() > 1 || goal.GetTeamNum() == team) continue;
        const auto center = Core(goal.GetWorldCenter());
        if (center.IsFinite()) return center;
    }
    return std::nullopt;
}

void WizardRumble::UpdatePatronums(ServerWrapper server) {
    if (goalReplay_ || !Supported(server) || !Active(server)) return;
    const bool host = Authoritative(server);
    std::vector<int> keys;
    for (const auto& entry : patronums_) keys.push_back(entry.first);
    for (const int key : keys) {
        auto found = patronums_.find(key);
        if (found == patronums_.end()) continue;
        auto& ghost = found->second;
        if (ghost.impact >= 0) {
            if (gameClock_ >= ghost.ends) patronums_.erase(found);
            continue; // A completed ghost can never strike a second time.
        }
        auto ball = server.GetBall();
        auto caster = host ? FindPlayerByAddress(server, ghost.pri) : FindPlayerByCar(server, ghost.car);
        auto car = caster ? caster.GetCar() : CarWrapper(0);
        const bool invalidCaster = host && (!caster || caster.IsSpectator() || !Live(car) ||
            car.memory_address != ghost.car || caster.GetTeamNum() != ghost.team);
        if (!ball || ball.memory_address != ghost.ball || ball.GetbFrozen() ||
            invalidCaster || gameClock_ >= ghost.ends) {
            CancelScene(wizard::Spell::ExpectoPatronum, key, -1, host, ghost.car);
            continue;
        }
        const double dt = gameClock_ - ghost.lastUpdate;
        if (dt <= 0) continue;
        const auto position = Core(ball.GetLocation());
        float radius = ball.GetRadius();
        if (!std::isfinite(radius) || radius < 10 || radius > 1000) radius = 93;
        const auto next = wizard::AdvancePatronum(ghost.position, ghost.previousBall, position, dt, radius);
        if (!next) { CancelScene(wizard::Spell::ExpectoPatronum, key, -1, host, ghost.car); continue; }
        ghost.position = next->position;
        if (next->forward.LengthSquared() > 0.5f) ghost.forward = next->forward;
        ghost.previousBall = position; ghost.lastUpdate = gameClock_;
        if (!next->hit || !host) continue;
        const auto shot = wizard::PatronumShotVelocity(position, ghost.goal);
        if (!shot) { CancelScene(wizard::Spell::ExpectoPatronum, key, -1, true, ghost.car); continue; }
        // Mark completion before touching native physics or notifying peers.
        // A replay/draw/next tick is cosmetic and cannot repeat the impulse.
        ghost.impact = gameClock_; ghost.contact = position; ghost.ends = gameClock_ + 0.45;
        StopSummon();
        StopRicochet(server);
        ball.SetVelocity(Engine(*shot)); ball.ForceNetUpdate2();
        wizard::Message hit; hit.kind = wizard::MessageKind::PatronumImpact;
        hit.spell = wizard::Spell::ExpectoPatronum; hit.casterId = key;
        hit.source = position; hit.normal = shot->Normalized();
        PatronumImpact(hit);
        if (transport_) transport_->Send(wizard::SerializeMessage(hit));
    }
}

void WizardRumble::PatronumImpact(const wizard::Message& message) {
    if (goalReplay_) return;
    const auto direction = message.normal.Normalized();
    if (!message.source.IsFinite() || direction.LengthSquared() < 0.5f) return;
    auto found = patronums_.find(message.casterId);
    if (found == patronums_.end()) {
        if (patronums_.size() >= 16) return;
        patronums_.emplace(message.casterId, PatronumState{message.casterId, 0, 0, 0, -1,
            message.source - direction * 90, direction, message.source, message.source + direction * 1000,
            gameClock_ - 0.25, gameClock_ + 0.45, gameClock_, gameClock_, message.source});
    } else {
        found->second.impact = gameClock_; found->second.contact = message.source;
        found->second.ends = gameClock_ + 0.45;
    }
    RecordScene(CurrentServer(), true);
}

void WizardRumble::StopSounds() {
    audio_.Stop(); leviosaVoice_.Stop(); leviosaFloat_.Stop(); slugsVoice_.Stop(); stupefyVoice_.Stop(); petrificusVoice_.Stop(); protegoVoice_.Stop();
    bombardaVoice_.Stop(); patronumVoice_.Stop(); obliviateVoice_.Stop(); accioVoice_.Stop(); imperioVoice_.Stop();
    leviosaFloatCasterCar_ = leviosaFloatTargetCar_ = 0;
    audioCaster_ = -1; leviosaVoiceCaster_ = -1; leviosaVoiceTarget_ = -1;
    slugsVoiceCaster_ = -1; slugsVoiceTarget_ = -1;
    audioCasterCar_ = leviosaVoiceCasterCar_ = leviosaVoiceTargetCar_ = 0;
    slugsVoiceCasterCar_ = slugsVoiceTargetCar_ = 0;
    stupefyVoiceCaster_ = stupefyVoiceTarget_ = -1;
    stupefyVoiceCasterCar_ = stupefyVoiceTargetCar_ = 0;
    petrificusVoiceCaster_ = petrificusVoiceTarget_ = -1;
    petrificusVoiceCasterCar_ = petrificusVoiceTargetCar_ = 0;
    protegoVoiceCaster_ = -1; protegoVoiceCasterCar_ = 0;
    bombardaVoiceCaster_ = -1; bombardaVoiceCasterCar_ = 0;
    patronumVoiceCaster_ = -1; patronumVoiceCasterCar_ = 0;
    obliviateVoiceCaster_ = obliviateVoiceTarget_ = -1;
    obliviateVoiceCasterCar_ = obliviateVoiceTargetCar_ = 0;
    accioVoiceCaster_ = imperioVoiceCaster_ = imperioVoiceTarget_ = -1;
    accioVoiceCasterCar_ = imperioVoiceCasterCar_ = imperioVoiceTargetCar_ = 0;
}

SpellAudio& WizardRumble::Sound(wizard::SpellCue cue) {
    switch (cue) {
    case wizard::SpellCue::LeviosaCast: return leviosaVoice_;
    case wizard::SpellCue::LeviosaFloat: return leviosaFloat_;
    case wizard::SpellCue::EatSlugs: return slugsVoice_;
    case wizard::SpellCue::Stupefy: return stupefyVoice_;
    case wizard::SpellCue::PetrificusCast: return petrificusVoice_;
    case wizard::SpellCue::ProtegoCast: return protegoVoice_;
    case wizard::SpellCue::BombardaCast: return bombardaVoice_;
    case wizard::SpellCue::PatronumCast: return patronumVoice_;
    case wizard::SpellCue::ObliviateCast: return obliviateVoice_;
    case wizard::SpellCue::AccioCast: return accioVoice_;
    case wizard::SpellCue::ImperioCast: return imperioVoice_;
    default: return audio_;
    }
}

bool WizardRumble::PlaySound(wizard::SpellCue cue, std::uintptr_t casterCar, std::uintptr_t targetCar) {
    if (goalReplay_) return false;
    auto& clip = Sound(cue);
    auto server = CurrentServer();
    if (server && server.memory_address == session_ && !gameWrapper->IsInFreeplay()) {
        auto director = server.GetReplayDirector();
        auto replay = director ? director.GetReplay() : ReplaySoccarWrapper(0);
        if (replay) replayAudio_.Record(replay.GetCurrentTime(), cue, clip.Duration(), casterCar, targetCar);
    }
    // Record accepted cues even while muted; replay preferences can change.
    if (Setting("wr_sfx", 1) < 0.5f || Setting("wr_sfx_volume", 1) <= 0) return true;
    if (clip.Play(std::clamp(Setting("wr_sfx_volume", 1), 0.0f, 1.0f))) return true;
    cvarManager->log("Wizard Rumble audio: " + clip.Error());
    return false;
}

void WizardRumble::StopRecordedSounds(ServerWrapper server, wizard::SpellCue cue,
    std::uintptr_t casterCar, std::uintptr_t targetCar) {
    if (goalReplay_ || !server || server.memory_address != session_ || gameWrapper->IsInFreeplay()) return;
    auto director = server.GetReplayDirector();
    auto replay = director ? director.GetReplay() : ReplaySoccarWrapper(0);
    if (replay) replayAudio_.Stop(replay.GetCurrentTime(), cue, casterCar, targetCar);
}

void WizardRumble::ReplaySounds(double timeline) {
    const float volume = std::clamp(Setting("wr_sfx_volume", 1), 0.0f, 1.0f);
    const auto actions = replayAudio_.Advance(timeline, Setting("wr_sfx", 1) > 0.5f && volume > 0,
        gameWrapper->IsPaused());
    for (const auto& action : actions) {
        auto& clip = Sound(action.cue);
        if (!action.play) clip.Stop();
        else if (!clip.Play(volume, false, action.offsetSeconds)) cvarManager->log("Replay audio: " + clip.Error());
    }
}

void WizardRumble::ReleaseSlugs(std::uintptr_t target, bool notify) {
    const auto found = slugs_.find(target);
    if (found == slugs_.end()) return;
    const auto state = found->second;
    slugs_.erase(found);
    CancelScene(wizard::Spell::EatSlugs, state.caster, state.target, notify, 0, state.car);
}

void WizardRumble::UpdateLocalEffects(ServerWrapper server, bool active) {
    if (!active) {
        slugs_.clear(); StopSounds();
        return;
    }
    const bool host = Authoritative(server);
    std::vector<std::uintptr_t> slugKeys;
    for (const auto& entry : slugs_) slugKeys.push_back(entry.first);
    for (const auto key : slugKeys) {
        const auto found = slugs_.find(key);
        if (found == slugs_.end()) continue;
        const auto state = found->second;
        auto player = FindPlayerByAddress(server, state.pri);
        auto car = player ? player.GetCar() : CarWrapper(0);
        if (!player || player.IsSpectator() || !Live(car) || car.memory_address != state.car || gameClock_ >= state.ends) {
            ReleaseSlugs(key, host);
        }
    }
    const float volume = std::clamp(Setting("wr_sfx_volume", 1), 0.0f, 1.0f);
    if (Setting("wr_sfx", 1) < 0.5f || volume <= 0) { StopSounds(); return; }
}

void WizardRumble::StopSummon(bool notify) {
    if (!summon_) return;
    const int caster = summon_->caster;
    summon_.reset();
    CancelScene(wizard::Spell::Accio, caster, -1, notify);
}

void WizardRumble::UpdateSummon(ServerWrapper server) {
    if (!summon_) return;
    const auto state = *summon_;
    auto player = FindPlayerByAddress(server, state.pri);
    auto car = player ? player.GetCar() : CarWrapper(0);
    auto ball = server.GetBall();
    if (!player || player.IsSpectator() || !Live(car) || car.memory_address != state.car ||
        !ball || ball.memory_address != state.ball || ball.GetbFrozen() || gameClock_ >= state.ends) {
        StopSummon(); return;
    }
    const double dt = gameClock_ - state.lastUpdate;
    if (dt < 1.0 / 240.0) return;
    const float touch = ball.GetLastTouchTime();
    if (std::isfinite(touch) && touch > state.lastTouch && gameClock_ - state.began > 0.15) {
        // Any new car touch hands control back immediately.
        StopSummon(); return;
    }
    float radius = ball.GetRadius();
    if (!std::isfinite(radius) || radius < 10 || radius > 1000) radius = 93;
    const auto target = wizard::AccioTarget(Core(car.GetLocation()), Core(RotatorToVector(car.GetRotation())), radius);
    const auto targetVelocity = Core(car.GetVelocity());
    const auto guidance = wizard::GuideAccio(Core(ball.GetLocation()), Core(ball.GetVelocity()), target,
        targetVelocity, dt, state.approach);
    if (!guidance) { StopSummon(); return; }
    auto velocity = guidance->velocity;
    const float gravity = ball.GetGravityZ();
    if (std::isfinite(gravity) && std::abs(gravity) < 10000)
        velocity.z -= gravity * static_cast<float>(std::min(dt, 0.1));
    const float speed = velocity.Length();
    if (speed > wizard::AccioMaxSpeed) velocity = velocity * (wizard::AccioMaxSpeed / speed);
    ball.SetVelocity(Engine(velocity));
    if ((target - Core(ball.GetLocation())).LengthSquared() < 200 * 200) {
        const auto angular = Core(ball.GetAngularVelocity());
        if (angular.IsFinite()) ball.SetAngularVelocity(Engine(angular * std::exp(-5.0f * static_cast<float>(std::min(dt, 0.1)))), false);
    }
    ball.ForceNetUpdate2();
    summon_->lastUpdate = gameClock_;
    summon_->settled = guidance->arrived ? state.settled + std::min(dt, 0.1) : 0;
    if (summon_->settled >= wizard::AccioSettleSeconds) {
        // Match a moving summoner instead of leaving a fast ball to hit its car.
        ball.SetVelocity(Engine(targetVelocity));
        ball.SetAngularVelocity(Vector{0, 0, 0}, false);
        ball.ForceNetUpdate2();
        if (Setting("wr_debug", 0) > 0.5f)
            cvarManager->log("Accio arrived: elapsed=" + std::to_string(gameClock_ - state.began) +
                " error=" + std::to_string((target - Core(ball.GetLocation())).Length()));
        StopSummon();
    }
}

void WizardRumble::CancelCharge(std::uintptr_t caster, bool notify) {
    const auto found = charges_.find(caster);
    if (found == charges_.end()) return;
    const auto charge = found->second;
    charges_.erase(found);
    if (charge.spell == wizard::Spell::Protego) {
        CancelScene(charge.spell, charge.casterId, -1, false, charge.casterCar);
        if (notify && transport_) {
            wizard::Message cancel; cancel.kind = wizard::MessageKind::ProtegoChargeCancel;
            cancel.casterId = charge.casterId;
            transport_->Send(wizard::SerializeMessage(cancel));
        }
        return;
    }
    CancelScene(charge.spell, charge.casterId, charge.targetId, notify, charge.casterCar, charge.targetCar);
}

void WizardRumble::UpdateCharges(ServerWrapper server) {
    // A demo callback can cancel other charges, so iterate stable keys.
    std::vector<std::uintptr_t> keys;
    for (const auto& entry : charges_) keys.push_back(entry.first);
    for (const auto key : keys) {
        const auto found = charges_.find(key);
        if (found == charges_.end()) continue;
        const auto charge = found->second;
        const bool ballCharge = wizard::IsBallTargetSpell(charge.spell);
        auto caster = FindPlayerByAddress(server, charge.casterPri);
        auto target = FindPlayerByAddress(server, charge.targetPri);
        auto car = caster ? caster.GetCar() : CarWrapper(0);
        auto victim = target ? target.GetCar() : CarWrapper(0);
        auto ball = ballCharge ? server.GetBall() : BallWrapper(0);
        const bool valid = caster && !caster.IsSpectator() && Live(car) && car.memory_address == charge.casterCar &&
            deck_.ReadyFor(charge.casterId) == charge.spell && (ballCharge ?
                (ball && ball.memory_address == charge.targetBall &&
                    wizard::InSpellRange(Core(car.GetLocation()), Core(ball.GetLocation()), SpellRange(server))) :
                (target && !target.IsSpectator() && Live(victim) && victim.memory_address == charge.targetCar &&
                    caster.GetTeamNum() != target.GetTeamNum() &&
                    wizard::InSpellRange(Core(car.GetLocation()), Core(victim.GetLocation()), SpellRange(server))));
        if (!valid) { CancelCharge(key); continue; }
        auto view = chargeViews_.find(charge.casterId);
        if (view != chargeViews_.end()) view->second.destination = ballCharge ? Core(ball.GetLocation()) : Core(victim.GetLocation());
        if (gameClock_ < charge.ends) continue;
        // Keep the exact selected PRI/car. A respawn or a different nearby enemy
        // cannot inherit a spell while its wind-up is in progress.
        charges_.erase(key);
        wizard::Message effect; effect.kind = wizard::MessageKind::Effect;
        effect.spell = charge.spell; effect.casterId = charge.casterId;
        effect.source = Core(car.GetLocation()) + wizard::Vec3{0, 0, 40};
        if (!ApplySpell(server, caster, effect.spell, effect, false, charge.targetPri)) {
            // Reuse charge cancellation semantics; a failed new ward must
            // leave its caster's older active wall intact on every PC.
            charges_.emplace(key, charge); CancelCharge(key);
            continue;
        }
        deck_.Consume(charge.casterId, gameClock_);
        ApplyScene(effect, charge.casterCar, charge.targetCar); // Keep the locked actor through impact/demo.
        if (transport_) transport_->Send(wizard::SerializeMessage(effect));
        Feedback(caster, wizard::Feedback::CastOk);
        SendState(server, caster);
    }
}

void WizardRumble::HoldPhysics(CarWrapper car, const Lift& lift) {
    if (lift.spell == wizard::Spell::Leviosa) {
        // Never teleport a rising car or override its collision response. The
        // native solver meets flat/curved roofs before UpdateEffects releases.
        if (LeviosaRoofContact(car)) return;
        car.SetCarRotation(lift.rotation);
        car.SetVelocity(Vector{0, 0, wizard::LeviosaRiseSpeed});
        car.SetAngularVelocity(Vector{0, 0, 0}, false);
        car.ForceNetUpdate2();
        return;
    }
    // These SDK setters update the actual rigid body; this is the same path
    // used by BakkesMod's RewindPlugin when restoring a car's pose.
    const auto desired = wizard::SuspensionPosition(Core(lift.start), lift.rise, gameClock_ - lift.began);
    car.SetLocation(Engine(desired));
    car.SetCarRotation(lift.rotation);
    car.SetVelocity(Vector{0, 0, 0});
    car.SetAngularVelocity(Vector{0, 0, 0}, false);
    car.ForceNetUpdate2();
}

bool WizardRumble::LeviosaRoofContact(CarWrapper car) {
    if (!Live(car)) return false;
    const auto contact = car.GetWorldContact();
    auto extent = Core(car.GetLocalCollisionExtent());
    auto offset = Core(car.GetLocalCollisionOffset());
    float reach = 130;
    if (extent.IsFinite() && offset.IsFinite() && extent.x > 0 && extent.y > 0 && extent.z > 0 &&
        extent.LengthSquared() <= 500 * 500 && offset.LengthSquared() <= 500 * 500)
        reach = std::clamp(extent.Length() + offset.Length() + 25, 60.0f, 300.0f);
    return wizard::IsLeviosaRoofContact(Core(car.GetCurrentRBLocation()), contact.bHasContact != 0,
        Core(contact.Location), Core(contact.Normal), reach);
}

void WizardRumble::VehicleInput(CarWrapper car, void* parameters) {
    if (goalReplay_) return;
    Tick();
    auto server = CurrentServer();
    const bool host = Authoritative(server);
    if (!parameters || !Live(car) || !Supported(server) || !Active(server) ||
        (host ? Setting("wr_enabled", 1) < 0.5f : !hostEnabled_)) return;
    auto player = car.GetPRI();
    if (!player || player.IsSpectator()) return;
    auto* input = static_cast<ControllerInput*>(parameters);
    const auto held = lifts_.find(player.memory_address);
    if (host && held != lifts_.end() && held->second.pri == player.memory_address && held->second.car == car.memory_address &&
        gameClock_ < held->second.ends) {
        *input = ControllerInput{};
        return;
    }
    // Assignment is idempotent on client prediction and host simulation.
    // The victim's own Obliviate cannot invert the caster's mirrored controls.
    if (ApplyImperioInput(server, car, *input)) return;
    // Inversion stays host-only; applying it on both paths would undo it.
    if (!host) return;
    const auto confused = confusions_.find(player.memory_address);
    if (confused != confusions_.end() && confused->second.pri == player.memory_address &&
        confused->second.car == car.memory_address && gameClock_ < confused->second.ends)
        wizard::InvertDirectionalControls(*input);
}

void WizardRumble::ReleaseHold(ServerWrapper server, std::uintptr_t pri, const char* reason) {
    auto found = lifts_.find(pri);
    if (found == lifts_.end()) return;
    auto player = FindPlayerByAddress(server, found->second.pri);
    auto car = player ? player.GetCar() : CarWrapper(0);
    if (player && Live(car) && player.memory_address == found->second.pri && car.memory_address == found->second.car) {
        // No frozen flag is owned by this build. Stop writing the hold state
        // and wake the body with its release velocity so gravity resumes.
        if (found->second.spell == wizard::Spell::Leviosa) {
            const bool roofRelease = std::string(reason) == "roof contact";
            if (liftProbe_ && liftProbe_->pri == pri) {
                liftProbe_->roofReached = roofRelease;
                liftProbe_->releasedAt = gameClock_;
                liftProbe_->peakZ = std::max(liftProbe_->peakZ, car.GetCurrentRBLocation().Z);
                liftProbe_->checkAt = std::min(liftProbe_->checkAt, gameClock_ + 0.35);
            }
            // Peel even an inverted car's wheels away from the ceiling using
            // native velocity, then let ordinary gravity/control resume.
            car.SetVelocity(Vector{0, 0, roofRelease ? -wizard::LeviosaRoofDropSpeed : 0});
        } else car.SetVelocity(Vector{found->second.velocity.X * 0.3f, found->second.velocity.Y * 0.3f, 0});
        car.SetAngularVelocity(Vector{0, 0, 0}, false);
        car.ForceNetUpdate2();
    }
    cvarManager->log("Hold released: id=" + std::to_string(found->second.id) + " elapsed=" +
        std::to_string(gameClock_ - found->second.began) + " reason=" + reason);
    CancelScene(found->second.spell, found->second.caster, found->second.id, true, 0, found->second.car);
    lifts_.erase(found);
}

void WizardRumble::ReleaseConfusion(std::uintptr_t pri, bool notify) {
    const auto found = confusions_.find(pri);
    if (found == confusions_.end()) return;
    CancelScene(wizard::Spell::Obliviate, found->second.caster, found->second.targetId, notify, 0, found->second.car);
    confusions_.erase(found);
}

void WizardRumble::BeforeDemolish(CarWrapper car) {
    if (goalReplay_) return;
    auto server = CurrentServer();
    if (!Authoritative(server) || !car) return;
    auto player = car.GetPRI();
    if (!player) return;
    if (summon_ && summon_->pri == player.memory_address) StopSummon();
    const auto ghost = patronums_.find(player.GetPlayerID());
    if (ghost != patronums_.end() && ghost->second.pri == player.memory_address && ghost->second.impact < 0)
        CancelScene(wizard::Spell::ExpectoPatronum, player.GetPlayerID(), -1, true, car.memory_address);
    std::vector<std::uintptr_t> interrupted;
    for (const auto& entry : charges_) {
        if (entry.second.casterPri == player.memory_address || entry.second.targetPri == player.memory_address)
            interrupted.push_back(entry.first);
    }
    for (const auto address : interrupted) CancelCharge(address);
    std::vector<std::uintptr_t> controlled;
    for (const auto& entry : imperios_)
        if (entry.second.pri == player.memory_address || entry.second.casterPri == player.memory_address)
            controlled.push_back(entry.first);
    for (const auto address : controlled) ReleaseImperio(address);
    ReleaseHold(server, player.memory_address, "demolition");
    ReleaseConfusion(player.memory_address);
    ReleaseSlugs(player.memory_address);
}

void WizardRumble::RequestCast() {
    if (goalReplay_) return;
    if (Elapsed(lastLocalCast_) < 0.2) return;
    lastLocalCast_ = Clock::now();
    auto server = CurrentServer();
    if (!Supported(server)) { ShowFeedback(wizard::Feedback::Unavailable); return; }
    auto player = LocalPlayer();
    if (!player || player.IsSpectator() || !Live(player.GetCar())) return;
    if (chargeViews_.count(player.GetPlayerID())) { ShowFeedback(wizard::Feedback::Charging); return; }
    if (Authoritative(server)) { Tick(); Cast(server, player); return; }
    wizard::Message cast; cast.kind = wizard::MessageKind::Cast;
    if (!transport_ || !transport_->Send(wizard::SerializeMessage(cast))) ShowFeedback(wizard::Feedback::Unavailable);
}

void WizardRumble::Cast(ServerWrapper server, PriWrapper caster) {
    if (!Authoritative(server) || !caster || caster.IsSpectator() || !Live(caster.GetCar())) return;
    if (Setting("wr_enabled", 1) < 0.5f || !Active(server)) { Feedback(caster, wizard::Feedback::Disabled); return; }
    const int id = caster.GetPlayerID();
    if (charges_.count(caster.memory_address)) { Feedback(caster, wizard::Feedback::Charging); return; }
    const auto spell = deck_.ReadyFor(id);
    if (spell == wizard::Spell::None) { Feedback(caster, wizard::Feedback::NotReady); return; }
    if (spell == wizard::Spell::Accio && summon_ && summon_->caster == id) {
        if (summon_->ends > gameClock_) { Feedback(caster, wizard::Feedback::Shielded); return; }
        StopSummon(); // Retire an old X before the new charge's G.
    }
    if (spell == wizard::Spell::BombardaMaxima && ricochet_ && ricochet_->caster == id) {
        if (ricochet_->ends > gameClock_) { Feedback(caster, wizard::Feedback::Shielded); return; }
        StopRicochet(server); // Old X precedes the next G and cannot cancel it.
    }
    if (spell == wizard::Spell::ExpectoPatronum) {
        const auto prior = patronums_.find(id);
        // An old flight's X cancellation must never erase a newer summon
        // wind-up. Keep the held spell until this caster's flight has ended.
        if (prior != patronums_.end() && prior->second.impact < 0 && prior->second.ends > gameClock_) {
            Feedback(caster, wizard::Feedback::Shielded); return;
        }
        if (prior != patronums_.end() && prior->second.ends <= gameClock_) {
            // Cast can also be called after a flight deadline but before its
            // expiry tick. Clear that old phase before starting the new G, so
            // a later expiry cannot stop the new voice or target indicator.
            if (prior->second.impact < 0)
                CancelScene(wizard::Spell::ExpectoPatronum, id, -1, true, prior->second.car);
            else patronums_.erase(prior);
        }
    }
    wizard::Message effect; effect.kind = wizard::MessageKind::Effect;
    effect.spell = spell; effect.casterId = id;
    effect.source = Core(caster.GetCar().GetLocation()) + wizard::Vec3{0, 0, 40};
    if (wizard::IsChargedSpell(spell)) {
        if (wizard::IsBallTargetSpell(spell)) {
            auto ball = server.GetBall();
            auto car = caster.GetCar();
            if (!ball || !wizard::InSpellRange(Core(car.GetLocation()), Core(ball.GetLocation()), SpellRange(server))) {
                Feedback(caster, wizard::Feedback::NoTarget); return;
            }
            charges_.insert_or_assign(caster.memory_address, ChargedCast{spell, id, -1,
                caster.memory_address, car.memory_address, 0, 0,
                gameClock_, gameClock_ + wizard::SpellChargeSeconds(spell), ball.memory_address});
            effect.kind = wizard::MessageKind::Charge; effect.targetId = -1;
            effect.destination = Core(ball.GetLocation()); effect.durationMillis = Milliseconds(wizard::SpellChargeSeconds(spell));
            ApplyScene(effect, car.memory_address);
            if (transport_) transport_->Send(wizard::SerializeMessage(effect));
            Feedback(caster, wizard::Feedback::Charging);
            return;
        }
        auto target = Target(server, caster);
        auto victim = target ? target.GetCar() : CarWrapper(0);
        if (!target || !Live(victim)) { Feedback(caster, wizard::Feedback::NoTarget); return; }
        if (spell == wizard::Spell::Leviosa && victim.GetbFrozen()) { Feedback(caster, wizard::Feedback::Shielded); return; }
        if (spell == wizard::Spell::Leviosa || spell == wizard::Spell::PetrificusTotalus) {
            const auto held = lifts_.find(target.memory_address);
            // Do not stack our own bind or let its expiry cancel a newer
            // charge on the same caster/target pair. The held spell is kept.
            if (held != lifts_.end() && held->second.spell == spell &&
                held->second.caster == id && held->second.ends > gameClock_) {
                Feedback(caster, wizard::Feedback::Shielded); return;
            }
        }
        if (spell == wizard::Spell::EatSlugs) {
            const auto active = slugs_.find(target.memory_address);
            // Old same-pair debuff expiry must not cancel a newer wind-up.
            if (active != slugs_.end() && active->second.caster == id && active->second.ends > gameClock_) {
                Feedback(caster, wizard::Feedback::Shielded); return;
            }
            if (active != slugs_.end() && active->second.ends <= gameClock_)
                ReleaseSlugs(target.memory_address); // Expired X must precede the new G.
        }
        if (spell == wizard::Spell::Obliviate) {
            const auto active = confusions_.find(target.memory_address);
            if (active != confusions_.end() && active->second.caster == id && active->second.ends > gameClock_) {
                Feedback(caster, wizard::Feedback::Shielded); return;
            }
            if (active != confusions_.end() && active->second.ends <= gameClock_)
                ReleaseConfusion(target.memory_address);
        }
        if (spell == wizard::Spell::Imperio) {
            const auto active = imperios_.find(target.memory_address);
            if (active != imperios_.end() && active->second.caster == id && active->second.ends > gameClock_) {
                Feedback(caster, wizard::Feedback::Shielded); return;
            }
            if (active != imperios_.end() && active->second.ends <= gameClock_)
                ReleaseImperio(target.memory_address);
        }
        charges_.insert_or_assign(caster.memory_address, ChargedCast{spell, id, target.GetPlayerID(),
            caster.memory_address, caster.GetCar().memory_address, target.memory_address, victim.memory_address,
            gameClock_, gameClock_ + wizard::SpellChargeSeconds(spell)});
        effect.kind = wizard::MessageKind::Charge;
        effect.targetId = target.GetPlayerID(); effect.destination = Core(victim.GetLocation());
        effect.durationMillis = Milliseconds(wizard::SpellChargeSeconds(spell));
        ApplyScene(effect, caster.GetCar().memory_address, victim.memory_address);
        if (transport_) transport_->Send(wizard::SerializeMessage(effect));
        Feedback(caster, wizard::Feedback::Charging);
        return;
    }
    // Select once: gameplay and art must never independently acquire neighbors.
    auto target = wizard::IsPlayerTargetSpell(spell) ? Target(server, caster) : PriWrapper(0);
    auto victim = target ? target.GetCar() : CarWrapper(0);
    if (wizard::IsPlayerTargetSpell(spell) && !Live(victim)) { Feedback(caster, wizard::Feedback::NoTarget); return; }
    if (!ApplySpell(server, caster, spell, effect, false, target ? target.memory_address : 0)) return;
    deck_.Consume(id, gameClock_);
    ApplyScene(effect, caster.GetCar().memory_address, victim ? victim.memory_address : 0);
    if (transport_) transport_->Send(wizard::SerializeMessage(effect));
    Feedback(caster, wizard::Feedback::CastOk);
    SendState(server, caster);
}

bool WizardRumble::ApplySpell(ServerWrapper server, PriWrapper caster, wizard::Spell spell, wizard::Message& effect,
    bool selfTest, std::uintptr_t testTarget) {
    if (goalReplay_) return false;
    if (selfTest && ((!gameWrapper->IsInFreeplay() && !testTarget) || !Authoritative(server) ||
        (spell != wizard::Spell::Leviosa && spell != wizard::Spell::PetrificusTotalus &&
            spell != wizard::Spell::Obliviate && spell != wizard::Spell::EatSlugs &&
            (spell != wizard::Spell::Imperio || !testTarget)))) return false;
    auto car = caster.GetCar();
    if (!Live(car)) return false;
    const int id = caster.GetPlayerID();
    int& targetId = effect.targetId;
    if (wizard::IsPlayerTargetSpell(spell)) {
        auto target = testTarget ? FindPlayerByAddress(server, testTarget) : selfTest ? caster : Target(server, caster);
        targetId = target ? target.GetPlayerID() : -1;
        auto victim = target ? target.GetCar() : CarWrapper(0);
        if (!target || !Live(victim)) { Feedback(caster, wizard::Feedback::NoTarget); return false; }
        effect.destination = Core(victim.GetLocation());
        if (spell == wizard::Spell::AvadaKedavra) {
            effect.durationMillis = 850;
            ReleaseHold(server, target.memory_address, "demolition spell");
            victim.NotifyNewAttacker(caster);
            victim.Demolish();
            lifts_.erase(target.memory_address);
        } else if (spell == wizard::Spell::Leviosa || spell == wizard::Spell::PetrificusTotalus) {
            auto existing = lifts_.find(target.memory_address);
            if (victim.GetbFrozen()) { Feedback(caster, wizard::Feedback::Shielded); return false; }
            const Vector start = victim.GetLocation();
            const Vector velocity = existing != lifts_.end() ? existing->second.velocity : victim.GetVelocity();
            if (existing != lifts_.end())
                CancelScene(existing->second.spell, existing->second.caster, targetId, true, 0, existing->second.car);
            const double duration = spell == wizard::Spell::Leviosa ? wizard::LeviosaSafetySeconds :
                wizard::PetrificusTotalusDurationSeconds;
            effect.durationMillis = Milliseconds(duration);
            lifts_.insert_or_assign(target.memory_address, Lift{targetId, target.memory_address, victim.memory_address,
                start, velocity, victim.GetRotation(), gameClock_, gameClock_ + duration,
                spell == wizard::Spell::Leviosa ? wizard::LeviosaRiseSpeed : 0, spell, id});
            cvarManager->log("Hold started: spell=" + std::string(wizard::GetSpellInfo(spell).identifier) +
                " target=" + std::to_string(targetId) + " bot=" + std::to_string(target.GetbBot() != 0) +
                " pri=" + std::to_string(target.memory_address) + " car=" + std::to_string(victim.memory_address) +
                " duration=" + std::to_string(duration) + " startZ=" + std::to_string(start.Z));
            HoldPhysics(victim, lifts_.at(target.memory_address));
        } else if (spell == wizard::Spell::EatSlugs) {
            ReleaseSlugs(target.memory_address);
            const double total = wizard::SlugFlightSeconds + wizard::EatSlugsDurationSeconds;
            effect.durationMillis = Milliseconds(total);
            slugs_.insert_or_assign(target.memory_address, SlugState{id, targetId, target.memory_address,
                victim.memory_address, gameClock_ + wizard::SlugFlightSeconds, gameClock_ + total});
        } else if (spell == wizard::Spell::Obliviate) {
            ReleaseConfusion(target.memory_address);
            const double duration = wizard::ObliviateDurationSeconds;
            effect.durationMillis = Milliseconds(duration);
            confusions_.insert_or_assign(target.memory_address, Confusion{target.memory_address, victim.memory_address,
                gameClock_, gameClock_ + duration, id, targetId});
        } else if (spell == wizard::Spell::Imperio) {
            const auto controlled = imperios_.find(caster.memory_address);
            if (controlled != imperios_.end() && controlled->second.ends > gameClock_) {
                Feedback(caster, wizard::Feedback::Shielded); return false;
            }
            ReleaseImperio(target.memory_address);
            effect.durationMillis = Milliseconds(wizard::ImperioDurationSeconds);
            imperios_.insert_or_assign(target.memory_address, Imperio{id, targetId,
                caster.memory_address, car.memory_address, target.memory_address, victim.memory_address,
                gameClock_, gameClock_ + wizard::ImperioDurationSeconds});
        } else {
            ReleaseHold(server, target.memory_address, "knockback spell");
            if (victim.GetbFrozen()) { Feedback(caster, wizard::Feedback::Shielded); return false; }
            if (spell == wizard::Spell::Stupefy) {
                effect.durationMillis = 900;
                auto direction = (Core(victim.GetLocation()) - Core(car.GetLocation())).Normalized();
                victim.SetVelocity(Engine(direction * 1900 + wizard::Vec3{0, 0, 650}));
            } else return false;
        }
        return true;
    }
    auto ball = server.GetBall();
    if (!wizard::IsBallTargetSpell(spell) || !ball ||
        !wizard::InSpellRange(Core(car.GetLocation()), Core(ball.GetLocation()), SpellRange(server))) {
        Feedback(caster, wizard::Feedback::NoTarget); return false;
    }
    effect.destination = Core(ball.GetLocation());
    if (spell == wizard::Spell::ExpectoPatronum) {
        if (ball.GetbFrozen()) { Feedback(caster, wizard::Feedback::Shielded); return false; }
        const auto goal = OpponentGoal(server, caster.GetTeamNum());
        const auto spawn = wizard::PatronumSpawn(Core(car.GetLocation()), Core(RotatorToVector(car.GetRotation())));
        const auto direction = goal ? wizard::PatronumShotVelocity(Core(ball.GetLocation()), *goal) : std::nullopt;
        if (!spawn || !goal || !direction || (patronums_.size() >= 16 && !patronums_.count(id))) {
            Feedback(caster, wizard::Feedback::NoTarget); return false;
        }
        if (patronums_.count(id)) CancelScene(spell, id, -1);
        const auto forward = (Core(ball.GetLocation()) - *spawn).Normalized();
        patronums_.insert_or_assign(id, PatronumState{id, caster.memory_address, car.memory_address,
            ball.memory_address, caster.GetTeamNum(), *spawn, forward, Core(ball.GetLocation()), *goal,
            gameClock_, gameClock_ + wizard::PatronumTimeoutSeconds, gameClock_});
        effect.source = *spawn; effect.normal = direction->Normalized();
        effect.durationMillis = Milliseconds(wizard::PatronumTimeoutSeconds);
        return true;
    }
    if (spell == wizard::Spell::Protego) {
        auto wall = wizard::PlanBarrier(Core(ball.GetLocation()), Core(ball.GetVelocity()), Core(RotatorToVector(car.GetRotation())));
        if (!wall) { Feedback(caster, wizard::Feedback::NoTarget); return false; }
        // E replaces this caster's previous wall. A broad cancellation here
        // would incorrectly stop the new charge's still-playing voice.
        walls_.insert_or_assign(id, WallState{*wall, id, gameClock_, gameClock_ + 3, -1});
        effect.destination = wall->center; effect.normal = wall->normal; effect.durationMillis = 3000;
        previousBall_ = Core(ball.GetLocation()); previousBallActor_ = ball.memory_address;
        return true;
    }
    if (spell != wizard::Spell::Accio && spell != wizard::Spell::BombardaMaxima) return false;
    if (ball.GetbFrozen()) { Feedback(caster, wizard::Feedback::Shielded); return false; }
    if (spell == wizard::Spell::Accio) {
        StopRicochet(server);
        StopSummon();
        float radius = ball.GetRadius();
        if (!std::isfinite(radius) || radius < 10 || radius > 1000) radius = 93;
        const auto target = wizard::AccioTarget(Core(car.GetLocation()), Core(RotatorToVector(car.GetRotation())), radius);
        const double approach = wizard::AccioApproachSeconds((target - Core(ball.GetLocation())).Length());
        summon_ = Summon{id, caster.memory_address, car.memory_address, ball.memory_address,
            gameClock_, gameClock_ + wizard::AccioDurationSeconds, gameClock_ - 1.0 / 120.0, approach, 0, ball.GetLastTouchTime()};
        effect.durationMillis = Milliseconds(wizard::AccioDurationSeconds);
        UpdateSummon(server);
        return true;
    }
    StopSummon();
    return StartRicochet(server, caster, effect);
}

void WizardRumble::SendState(ServerWrapper server, PriWrapper player) {
    if (!player) return;
    wizard::Message message; message.kind = wizard::MessageKind::State;
    message.playerId = player.GetPlayerID();
    message.enabled = Setting("wr_enabled", 1) > 0.5f && Active(server);
    message.spell = message.enabled ? deck_.ReadyFor(message.playerId) : wizard::Spell::None;
    message.cooldownDeciseconds = std::clamp(static_cast<int>(std::ceil(deck_.CooldownFor(message.playerId) * 10)), 1, 600);
    message.spellRange = SpellRange(server);
    message.remainingDeciseconds = message.spell == wizard::Spell::None && message.enabled ?
        std::clamp(static_cast<int>(std::ceil(deck_.Remaining(message.playerId, gameClock_) * 10)), 0, 600) : 0;
    sentSpells_[message.playerId] = message.spell;
    auto local = LocalPlayer();
    if (local && local.memory_address == player.memory_address) {
        localSpell_ = message.spell; localRemaining_ = message.remainingDeciseconds / 10.0;
        localCooldown_ = message.cooldownDeciseconds / 10.0; hostRange_ = message.spellRange; stateClock_ = gameClock_;
        hostEnabled_ = message.enabled; synchronized_ = true; stateReceived_ = Clock::now();
    } else if (peers_.count(message.playerId) && transport_) transport_->Send(wizard::SerializeMessage(message));
}

void WizardRumble::Feedback(PriWrapper player, wizard::Feedback code) {
    if (!player) return;
    auto local = LocalPlayer();
    if (local && local.memory_address == player.memory_address) { ShowFeedback(code); return; }
    wizard::Message message; message.kind = wizard::MessageKind::Feedback;
    message.playerId = player.GetPlayerID(); message.feedback = code;
    if (transport_) transport_->Send(wizard::SerializeMessage(message));
}

void WizardRumble::ShowFeedback(wizard::Feedback code) {
    switch (code) {
    case wizard::Feedback::CastOk: feedback_ = "Spell cast!"; break;
    case wizard::Feedback::NoTarget: feedback_ = "No valid target in range. Your spell is kept."; break;
    case wizard::Feedback::Shielded: feedback_ = "Target protected or already affected. Your spell is kept."; break;
    case wizard::Feedback::NotReady: feedback_ = "Your next spell is still charging."; break;
    case wizard::Feedback::Disabled: feedback_ = "Waiting for the host / active round."; break;
    case wizard::Feedback::Charging: feedback_ = "Spell charging..."; break;
    default: feedback_ = "Use freeplay or a LAN match with NetcodePlugin."; break;
    }
    feedbackUntil_ = Clock::now() + std::chrono::seconds(2);
}

void WizardRumble::Receive(const std::string& payload, PriWrapper sender) {
    if (receiving_ || goalReplay_) return;
    const auto parsed = wizard::ParseMessage(payload);
    if (!parsed || !sender) return;
    auto server = CurrentServer();
    if (!Supported(server)) return;
    const auto& message = *parsed;
    auto local = LocalPlayer();
    if (Authoritative(server)) {
        // Clients provide intent only. Spell, target and every effect are chosen here.
        if (message.kind != wizard::MessageKind::Cast && message.kind != wizard::MessageKind::Hello) return;
        if (sender.IsSpectator() || (local && sender.memory_address == local.memory_address)) return;
        const int id = sender.GetPlayerID();
        if (id < 0 || id > wizard::MaxPlayerId) return;
        const double realtime = std::chrono::duration<double>(Clock::now().time_since_epoch()).count();
        auto recent = lastRequest_.find(id);
        if (recent != lastRequest_.end() && realtime - recent->second < 0.15) return;
        lastRequest_[id] = realtime;
        receiving_ = true;
        Tick();
        peers_.insert(id);
        if (message.kind == wizard::MessageKind::Cast) Cast(server, sender);
        else { SendState(server, sender); SendSceneSnapshot(server, sender); }
        receiving_ = false;
        return;
    }
    auto owner = server.GetGameOwner();
    if (!owner || owner.memory_address != sender.memory_address || !local) return;
    if (message.recipientId != -1 && message.recipientId != local.GetPlayerID()) return;
    if (message.kind == wizard::MessageKind::Clear) {
        if (message.recipientId < 0) RecordScene(server, true);
        if (message.recipientId < 0) StopRecordedSounds(server);
        visuals_.ClearEffects(message.recipientId >= 0); walls_.clear(); patronums_.clear(); confusions_.clear(); chargeViews_.clear();
        // Targeted clears begin periodic scene snapshots. A refresh must not
        // cut off an already playing voice; broadcast clears end the round.
        if (message.recipientId < 0) { imperios_.clear(); slugs_.clear(); StopSounds(); }
        return;
    }
    if (message.kind == wizard::MessageKind::Cancel) {
        CancelScene(message.spell, message.casterId, message.targetId, false);
        if (message.spell == wizard::Spell::Protego) walls_.erase(message.casterId);
        if (message.spell == wizard::Spell::Obliviate) {
            for (auto it = confusions_.begin(); it != confusions_.end();) {
                if (it->second.targetId == message.targetId && it->second.caster == message.casterId)
                    it = confusions_.erase(it);
                else ++it;
            }
        }
        return;
    }
    if (message.kind == wizard::MessageKind::ProtegoChargeCancel) {
        CancelScene(wizard::Spell::Protego, message.casterId, -1, false);
        return;
    }
    if (message.kind == wizard::MessageKind::Impact) {
        auto wall = walls_.find(message.casterId);
        if (wall != walls_.end()) wall->second.impact = gameClock_;
        return;
    }
    if (message.kind == wizard::MessageKind::PatronumImpact) { PatronumImpact(message); return; }
    if (message.kind == wizard::MessageKind::Effect || message.kind == wizard::MessageKind::EffectSnapshot ||
        message.kind == wizard::MessageKind::Charge || message.kind == wizard::MessageKind::ChargeSnapshot) {
        ApplyScene(message); return;
    }
    if (message.playerId != local.GetPlayerID()) return;
    if (message.kind == wizard::MessageKind::State) {
        localSpell_ = message.spell; localRemaining_ = message.remainingDeciseconds / 10.0;
        localCooldown_ = message.cooldownDeciseconds / 10.0; hostRange_ = message.spellRange; stateClock_ = gameClock_;
        hostEnabled_ = message.enabled; synchronized_ = true; stateReceived_ = Clock::now();
    } else if (message.kind == wizard::MessageKind::Feedback) ShowFeedback(message.feedback);
}

void WizardRumble::ApplyScene(const wizard::Message& message, std::uintptr_t casterCar, std::uintptr_t targetCar) {
    const double duration = message.durationMillis / 1000.0;
    const bool charging = message.kind == wizard::MessageKind::Charge || message.kind == wizard::MessageKind::ChargeSnapshot;
    auto server = CurrentServer();
    const auto previous = chargeViews_.find(message.casterId);
    const bool sameCharge = previous != chargeViews_.end() && previous->second.spell == message.spell &&
        previous->second.targetId == message.targetId;
    if (sameCharge) {
        if (!casterCar) casterCar = previous->second.casterCar;
        if (!targetCar) targetCar = previous->second.targetCar;
    }
    if (!casterCar) {
        auto caster = SceneTarget(server, message.casterId, message.source);
        auto body = caster ? caster.GetCar() : CarWrapper(0);
        if (Live(body)) casterCar = body.memory_address;
    }
    if (!targetCar && message.targetId >= 0) {
        auto target = SceneTarget(server, message.targetId, message.destination);
        auto body = target ? target.GetCar() : CarWrapper(0);
        if (Live(body)) targetCar = body.memory_address;
    }
    if (charging) {
        const auto prior = chargeViews_.find(message.casterId);
        const bool alreadyCharging = prior != chargeViews_.end() && prior->second.spell == message.spell && prior->second.ends > gameClock_;
        chargeViews_.insert_or_assign(message.casterId, ChargeView{message.spell, message.targetId, message.destination,
            gameClock_ + duration, casterCar, targetCar});
        if (!alreadyCharging && message.kind == wizard::MessageKind::Charge) {
            const auto cue = message.spell == wizard::Spell::Leviosa ? wizard::SpellCue::LeviosaCast :
                message.spell == wizard::Spell::EatSlugs ? wizard::SpellCue::EatSlugs :
                message.spell == wizard::Spell::PetrificusTotalus ? wizard::SpellCue::PetrificusCast :
                message.spell == wizard::Spell::Protego ? wizard::SpellCue::ProtegoCast :
                message.spell == wizard::Spell::BombardaMaxima ? wizard::SpellCue::BombardaCast :
                message.spell == wizard::Spell::ExpectoPatronum ? wizard::SpellCue::PatronumCast :
                message.spell == wizard::Spell::Obliviate ? wizard::SpellCue::ObliviateCast :
                message.spell == wizard::Spell::Accio ? wizard::SpellCue::AccioCast :
                message.spell == wizard::Spell::Imperio ? wizard::SpellCue::ImperioCast :
                message.spell == wizard::Spell::Stupefy ? wizard::SpellCue::Stupefy : wizard::SpellCue::Avada;
            if (PlaySound(cue, casterCar, targetCar)) {
                if (message.spell == wizard::Spell::Leviosa) {
                    leviosaVoiceCaster_ = message.casterId; leviosaVoiceTarget_ = message.targetId;
                    leviosaVoiceCasterCar_ = casterCar; leviosaVoiceTargetCar_ = targetCar;
                }
                else if (message.spell == wizard::Spell::EatSlugs) {
                    slugsVoiceCaster_ = message.casterId; slugsVoiceTarget_ = message.targetId;
                    slugsVoiceCasterCar_ = casterCar; slugsVoiceTargetCar_ = targetCar;
                }
                else if (message.spell == wizard::Spell::Stupefy) {
                    stupefyVoiceCaster_ = message.casterId; stupefyVoiceTarget_ = message.targetId;
                    stupefyVoiceCasterCar_ = casterCar; stupefyVoiceTargetCar_ = targetCar;
                }
                else if (message.spell == wizard::Spell::PetrificusTotalus) {
                    petrificusVoiceCaster_ = message.casterId; petrificusVoiceTarget_ = message.targetId;
                    petrificusVoiceCasterCar_ = casterCar; petrificusVoiceTargetCar_ = targetCar;
                }
                else if (message.spell == wizard::Spell::Protego) {
                    protegoVoiceCaster_ = message.casterId; protegoVoiceCasterCar_ = casterCar;
                }
                else if (message.spell == wizard::Spell::BombardaMaxima) {
                    bombardaVoiceCaster_ = message.casterId; bombardaVoiceCasterCar_ = casterCar;
                }
                else if (message.spell == wizard::Spell::ExpectoPatronum) {
                    patronumVoiceCaster_ = message.casterId; patronumVoiceCasterCar_ = casterCar;
                }
                else if (message.spell == wizard::Spell::Obliviate) {
                    obliviateVoiceCaster_ = message.casterId; obliviateVoiceTarget_ = message.targetId;
                    obliviateVoiceCasterCar_ = casterCar; obliviateVoiceTargetCar_ = targetCar;
                }
                else if (message.spell == wizard::Spell::Accio) {
                    accioVoiceCaster_ = message.casterId; accioVoiceCasterCar_ = casterCar;
                }
                else if (message.spell == wizard::Spell::Imperio) {
                    imperioVoiceCaster_ = message.casterId; imperioVoiceTarget_ = message.targetId;
                    imperioVoiceCasterCar_ = casterCar; imperioVoiceTargetCar_ = targetCar;
                }
                else { audioCaster_ = message.casterId; audioCasterCar_ = casterCar; }
            }
        }
    } else if (wizard::IsChargedSpell(message.spell)) {
        chargeViews_.erase(message.casterId);
    }
    if (message.kind == wizard::MessageKind::Effect && message.spell == wizard::Spell::Leviosa) {
        if (PlaySound(wizard::SpellCue::LeviosaFloat, casterCar, targetCar)) {
            leviosaFloatCasterCar_ = casterCar; leviosaFloatTargetCar_ = targetCar;
        }
    }
    // Patronum speech belongs to G's three-second wind-up. E launches the
    // Octane without replaying it; flight snapshots and V impacts stay silent.
    if (!charging && message.spell == wizard::Spell::ExpectoPatronum && !Authoritative(server)) {
        auto ball = server.GetBall();
        if (ball && duration > 0 && duration <= wizard::PatronumTimeoutSeconds &&
            message.source.IsFinite() && (patronums_.size() < 16 || patronums_.count(message.casterId))) {
            const double began = message.kind == wizard::MessageKind::EffectSnapshot ?
                gameClock_ - (wizard::PatronumTimeoutSeconds - duration) : gameClock_;
            patronums_.insert_or_assign(message.casterId, PatronumState{message.casterId, 0, casterCar,
                ball.memory_address, -1, message.source, (Core(ball.GetLocation()) - message.source).Normalized(),
                Core(ball.GetLocation()), {}, began, gameClock_ + duration, gameClock_});
        }
    }
    if (!charging && message.spell == wizard::Spell::EatSlugs) {
        if (!Authoritative(server)) {
            auto target = FindPlayerByCar(server, targetCar);
            auto car = target ? target.GetCar() : CarWrapper(0);
            if (target && Live(car)) {
                slugs_.insert_or_assign(target.memory_address, SlugState{message.casterId, message.targetId,
                    target.memory_address, car.memory_address,
                    gameClock_ + std::max(0.0, duration-wizard::EatSlugsDurationSeconds), gameClock_ + duration});
            }
        }
    }
    if (!charging && message.spell == wizard::Spell::Obliviate && !Authoritative(server)) {
        auto target = FindPlayerByCar(server, targetCar);
        auto car = target ? target.GetCar() : CarWrapper(0);
        if (target && Live(car)) confusions_.insert_or_assign(target.memory_address,
            Confusion{target.memory_address, car.memory_address, gameClock_, gameClock_ + duration,
                message.casterId, message.targetId});
    }
    if (!charging && message.spell == wizard::Spell::Imperio && !Authoritative(server) &&
        duration > 0 && duration <= wizard::ImperioDurationSeconds) {
        auto caster = FindPlayerByCar(server, casterCar);
        auto target = FindPlayerByCar(server, targetCar);
        auto source = caster ? caster.GetCar() : CarWrapper(0);
        auto victim = target ? target.GetCar() : CarWrapper(0);
        if (caster && target && Live(source) && Live(victim) && caster.GetTeamNum()!=target.GetTeamNum())
            imperios_.insert_or_assign(target.memory_address, Imperio{message.casterId, message.targetId,
                caster.memory_address, source.memory_address, target.memory_address, victim.memory_address,
                gameClock_, gameClock_+duration});
    }
    if (!charging && message.spell == wizard::Spell::Protego) {
        if (!Authoritative(server)) {
            auto geometry = wizard::PlanBarrier(message.destination, message.normal, message.normal, 0);
            if (!geometry) return;
            // The host has already placed/floor-adjusted the wall.
            geometry->center = message.destination;
            const double began = message.kind == wizard::MessageKind::EffectSnapshot && duration > 0 && duration <= 3 ?
                gameClock_ - (3 - duration) : gameClock_;
            walls_.insert_or_assign(message.casterId, WallState{*geometry, message.casterId,
                began, gameClock_ + duration, -1});
        }
    }
    visuals_.StartEffect(SpellVisuals::Effect{message.spell, message.casterId, message.targetId,
        message.source, message.destination, gameClock_, duration,
        message.kind == wizard::MessageKind::EffectSnapshot || message.kind == wizard::MessageKind::ChargeSnapshot,
        charging, casterCar, targetCar});
    RecordScene(server, true);
}

void WizardRumble::CancelScene(wizard::Spell spell, int caster, int target, bool notify,
    std::uintptr_t casterCar, std::uintptr_t targetCar) {
    visuals_.StopEffect(spell, caster, target, gameClock_, true, casterCar, targetCar);
    if (spell == wizard::Spell::ExpectoPatronum) patronums_.erase(caster);
    auto server = CurrentServer();
    if (wizard::IsChargedSpell(spell)) {
        const auto cue = spell == wizard::Spell::Leviosa ? wizard::SpellCue::LeviosaCast :
            spell == wizard::Spell::EatSlugs ? wizard::SpellCue::EatSlugs :
            spell == wizard::Spell::PetrificusTotalus ? wizard::SpellCue::PetrificusCast :
            spell == wizard::Spell::Protego ? wizard::SpellCue::ProtegoCast :
            spell == wizard::Spell::BombardaMaxima ? wizard::SpellCue::BombardaCast :
            spell == wizard::Spell::ExpectoPatronum ? wizard::SpellCue::PatronumCast :
            spell == wizard::Spell::Obliviate ? wizard::SpellCue::ObliviateCast :
            spell == wizard::Spell::Accio ? wizard::SpellCue::AccioCast :
            spell == wizard::Spell::Imperio ? wizard::SpellCue::ImperioCast :
            spell == wizard::Spell::Stupefy ? wizard::SpellCue::Stupefy : wizard::SpellCue::Avada;
        // A completed bind can expire while another caster charges at that
        // same victim. Numeric ownership must not truncate the newer voice.
        const int owner = spell == wizard::Spell::Leviosa ? leviosaVoiceCaster_ :
            spell == wizard::Spell::EatSlugs ? slugsVoiceCaster_ :
            spell == wizard::Spell::PetrificusTotalus ? petrificusVoiceCaster_ :
            spell == wizard::Spell::Protego ? protegoVoiceCaster_ :
            spell == wizard::Spell::BombardaMaxima ? bombardaVoiceCaster_ :
            spell == wizard::Spell::ExpectoPatronum ? patronumVoiceCaster_ :
            spell == wizard::Spell::Obliviate ? obliviateVoiceCaster_ :
            spell == wizard::Spell::Accio ? accioVoiceCaster_ :
            spell == wizard::Spell::Imperio ? imperioVoiceCaster_ :
            spell == wizard::Spell::Stupefy ? stupefyVoiceCaster_ : audioCaster_;
        const auto ownerCar = spell == wizard::Spell::Leviosa ? leviosaVoiceCasterCar_ :
            spell == wizard::Spell::EatSlugs ? slugsVoiceCasterCar_ :
            spell == wizard::Spell::PetrificusTotalus ? petrificusVoiceCasterCar_ :
            spell == wizard::Spell::Protego ? protegoVoiceCasterCar_ :
            spell == wizard::Spell::BombardaMaxima ? bombardaVoiceCasterCar_ :
            spell == wizard::Spell::ExpectoPatronum ? patronumVoiceCasterCar_ :
            spell == wizard::Spell::Obliviate ? obliviateVoiceCasterCar_ :
            spell == wizard::Spell::Accio ? accioVoiceCasterCar_ :
            spell == wizard::Spell::Imperio ? imperioVoiceCasterCar_ :
            spell == wizard::Spell::Stupefy ? stupefyVoiceCasterCar_ : audioCasterCar_;
        if (casterCar || owner == caster)
            StopRecordedSounds(server, cue, casterCar ? casterCar : ownerCar, targetCar);
    }
    if (spell == wizard::Spell::Leviosa) {
        StopRecordedSounds(server, wizard::SpellCue::LeviosaFloat, casterCar, targetCar);
        if ((!casterCar || leviosaFloatCasterCar_ == casterCar) &&
            (!targetCar || leviosaFloatTargetCar_ == targetCar)) {
            leviosaFloat_.Stop(); leviosaFloatCasterCar_ = leviosaFloatTargetCar_ = 0;
        }
    }
    if (wizard::IsChargedSpell(spell)) {
        const auto found = chargeViews_.find(caster);
        if (found != chargeViews_.end() && found->second.spell == spell &&
            (target < 0 || found->second.targetId == target) &&
            (!casterCar || found->second.casterCar == casterCar) &&
            (!targetCar || found->second.targetCar == targetCar)) chargeViews_.erase(found);
        if (spell == wizard::Spell::AvadaKedavra && audioCaster_ == caster &&
            (!casterCar || audioCasterCar_ == casterCar)) {
            audio_.Stop(); audioCaster_ = -1; audioCasterCar_ = 0;
        }
        if (spell == wizard::Spell::Accio && accioVoiceCaster_ == caster &&
            (!casterCar || accioVoiceCasterCar_ == casterCar)) {
            accioVoice_.Stop(); accioVoiceCaster_ = -1; accioVoiceCasterCar_ = 0;
        }
        if (spell == wizard::Spell::Imperio && imperioVoiceCaster_ == caster &&
            (target < 0 || imperioVoiceTarget_ == target) &&
            (!casterCar || imperioVoiceCasterCar_ == casterCar) &&
            (!targetCar || imperioVoiceTargetCar_ == targetCar)) {
            imperioVoice_.Stop(); imperioVoiceCaster_ = imperioVoiceTarget_ = -1;
            imperioVoiceCasterCar_ = imperioVoiceTargetCar_ = 0;
        }
        if (spell == wizard::Spell::Leviosa && leviosaVoiceCaster_ == caster &&
            (target < 0 || leviosaVoiceTarget_ == target) &&
            (!casterCar || leviosaVoiceCasterCar_ == casterCar) &&
            (!targetCar || leviosaVoiceTargetCar_ == targetCar)) {
            leviosaVoice_.Stop(); leviosaVoiceCaster_ = -1; leviosaVoiceTarget_ = -1;
            leviosaVoiceCasterCar_ = leviosaVoiceTargetCar_ = 0;
        }
        if (spell == wizard::Spell::EatSlugs && slugsVoiceCaster_ == caster &&
            (target < 0 || slugsVoiceTarget_ == target) &&
            (!casterCar || slugsVoiceCasterCar_ == casterCar) &&
            (!targetCar || slugsVoiceTargetCar_ == targetCar)) {
            slugsVoice_.Stop(); slugsVoiceCaster_ = -1; slugsVoiceTarget_ = -1;
            slugsVoiceCasterCar_ = slugsVoiceTargetCar_ = 0;
        }
        if (spell == wizard::Spell::Stupefy && stupefyVoiceCaster_ == caster &&
            (target < 0 || stupefyVoiceTarget_ == target) &&
            (!casterCar || stupefyVoiceCasterCar_ == casterCar) &&
            (!targetCar || stupefyVoiceTargetCar_ == targetCar)) {
            stupefyVoice_.Stop(); stupefyVoiceCaster_ = stupefyVoiceTarget_ = -1;
            stupefyVoiceCasterCar_ = stupefyVoiceTargetCar_ = 0;
        }
        if (spell == wizard::Spell::PetrificusTotalus && petrificusVoiceCaster_ == caster &&
            (target < 0 || petrificusVoiceTarget_ == target) &&
            (!casterCar || petrificusVoiceCasterCar_ == casterCar) &&
            (!targetCar || petrificusVoiceTargetCar_ == targetCar)) {
            petrificusVoice_.Stop(); petrificusVoiceCaster_ = petrificusVoiceTarget_ = -1;
            petrificusVoiceCasterCar_ = petrificusVoiceTargetCar_ = 0;
        }
        if (spell == wizard::Spell::Protego && protegoVoiceCaster_ == caster &&
            (!casterCar || protegoVoiceCasterCar_ == casterCar)) {
            protegoVoice_.Stop(); protegoVoiceCaster_ = -1; protegoVoiceCasterCar_ = 0;
        }
        if (spell == wizard::Spell::BombardaMaxima && bombardaVoiceCaster_ == caster &&
            (!casterCar || bombardaVoiceCasterCar_ == casterCar)) {
            bombardaVoice_.Stop(); bombardaVoiceCaster_ = -1; bombardaVoiceCasterCar_ = 0;
        }
        if (spell == wizard::Spell::ExpectoPatronum && patronumVoiceCaster_ == caster &&
            (!casterCar || patronumVoiceCasterCar_ == casterCar)) {
            patronumVoice_.Stop(); patronumVoiceCaster_ = -1; patronumVoiceCasterCar_ = 0;
        }
        if (spell == wizard::Spell::Obliviate && obliviateVoiceCaster_ == caster &&
            (target < 0 || obliviateVoiceTarget_ == target) &&
            (!casterCar || obliviateVoiceCasterCar_ == casterCar) &&
            (!targetCar || obliviateVoiceTargetCar_ == targetCar)) {
            obliviateVoice_.Stop(); obliviateVoiceCaster_ = obliviateVoiceTarget_ = -1;
            obliviateVoiceCasterCar_ = obliviateVoiceTargetCar_ = 0;
        }
    }
    if (spell == wizard::Spell::EatSlugs && !Authoritative(CurrentServer())) {
        for (auto it = slugs_.begin(); it != slugs_.end();) {
            if ((caster < 0 || it->second.caster == caster) && (target < 0 || it->second.target == target) &&
                (!targetCar || it->second.car == targetCar))
                it = slugs_.erase(it);
            else ++it;
        }
    }
    if (spell == wizard::Spell::Imperio && !Authoritative(server)) {
        for (auto it=imperios_.begin(); it!=imperios_.end();) {
            if ((caster<0 || it->second.caster==caster) && (target<0 || it->second.target==target) &&
                (!casterCar || it->second.casterCar==casterCar) && (!targetCar || it->second.car==targetCar))
                it=imperios_.erase(it);
            else ++it;
        }
    }
    if (!notify || !transport_) return;
    wizard::Message message; message.kind = wizard::MessageKind::Cancel;
    message.spell = spell; message.casterId = caster; message.targetId = target;
    transport_->Send(wizard::SerializeMessage(message));
}

void WizardRumble::SendSceneSnapshot(ServerWrapper server, PriWrapper recipient) {
    if (!transport_ || !recipient || !Authoritative(server)) return;
    const int id = recipient.GetPlayerID();
    wizard::Message clear; clear.kind = wizard::MessageKind::Clear; clear.recipientId = id;
    transport_->Send(wizard::SerializeMessage(clear));
    const auto snapshot = [&](wizard::Spell spell, int casterId, int targetId,
        wizard::Vec3 destination, wizard::Vec3 normal, double remaining) {
        if (remaining <= 0) return;
        wizard::Message message; message.kind = wizard::MessageKind::EffectSnapshot;
        message.recipientId = id; message.spell = spell; message.casterId = casterId;
        message.targetId = targetId; message.destination = destination; message.normal = normal;
        auto caster = FindPlayer(server, casterId);
        auto car = caster ? caster.GetCar() : CarWrapper(0);
        message.source = car ? Core(car.GetLocation()) + wizard::Vec3{0, 0, 40} : destination;
        message.durationMillis = Milliseconds(remaining);
        transport_->Send(wizard::SerializeMessage(message));
    };
    for (const auto& entry : lifts_) {
        auto player = FindPlayerByAddress(server, entry.first);
        auto car = player ? player.GetCar() : CarWrapper(0);
        if (car && car.memory_address == entry.second.car && player.memory_address == entry.second.pri)
            snapshot(entry.second.spell, entry.second.caster, entry.second.id, Core(car.GetLocation()),
                {1, 0, 0}, entry.second.ends - gameClock_);
    }
    for (const auto& entry : confusions_) {
        auto player = FindPlayerByAddress(server, entry.first);
        auto car = player ? player.GetCar() : CarWrapper(0);
        if (player && Live(car) && player.memory_address == entry.second.pri && car.memory_address == entry.second.car)
            snapshot(wizard::Spell::Obliviate, entry.second.caster, entry.second.targetId, Core(car.GetLocation()),
                {1, 0, 0}, entry.second.ends - gameClock_);
    }
    for (const auto& entry : imperios_) {
        const auto& state=entry.second;
        auto caster=FindPlayerByAddress(server,state.casterPri);
        auto target=FindPlayerByAddress(server,state.pri);
        auto source=caster?caster.GetCar():CarWrapper(0);
        auto victim=target?target.GetCar():CarWrapper(0);
        if (!Live(source)||!Live(victim)||source.memory_address!=state.casterCar||
            victim.memory_address!=state.car||state.ends<=gameClock_) continue;
        wizard::Message message;message.kind=wizard::MessageKind::EffectSnapshot;
        message.spell=wizard::Spell::Imperio;message.casterId=state.caster;message.targetId=state.target;
        message.recipientId=id;message.source=Core(source.GetLocation())+wizard::Vec3{0,0,40};
        message.destination=Core(victim.GetLocation());message.durationMillis=Milliseconds(state.ends-gameClock_);
        transport_->Send(wizard::SerializeMessage(message));
    }
    if (ricochet_ && ricochet_->ends>gameClock_) {
        auto ball=server.GetBall();
        if(ball && ball.memory_address==ricochet_->ball)
            snapshot(wizard::Spell::BombardaMaxima,ricochet_->caster,-1,Core(ball.GetLocation()),
                {1,0,0},ricochet_->ends-gameClock_);
    }
    for (const auto& entry : walls_)
        snapshot(wizard::Spell::Protego, entry.first, -1, entry.second.geometry.center,
            entry.second.geometry.normal, entry.second.ends - gameClock_);
    for (const auto& entry : slugs_) {
        auto player = FindPlayerByAddress(server, entry.first);
        auto car = player ? player.GetCar() : CarWrapper(0);
        if (Live(car) && car.memory_address == entry.second.car)
            snapshot(wizard::Spell::EatSlugs, entry.second.caster, entry.second.target, Core(car.GetLocation()),
                {1, 0, 0}, entry.second.ends - gameClock_);
    }
    if (summon_) {
        auto ball = server.GetBall();
        if (ball && ball.memory_address == summon_->ball)
            snapshot(wizard::Spell::Accio, summon_->caster, -1, Core(ball.GetLocation()), {1, 0, 0}, summon_->ends - gameClock_);
    }
    for (const auto& entry : patronums_) {
        const auto& ghost = entry.second;
        if (ghost.impact >= 0 || ghost.ends <= gameClock_) continue;
        wizard::Message message; message.kind = wizard::MessageKind::EffectSnapshot;
        message.spell = wizard::Spell::ExpectoPatronum; message.casterId = entry.first;
        message.recipientId = id; message.source = ghost.position; message.destination = ghost.previousBall;
        message.normal = (ghost.goal - ghost.previousBall).Normalized();
        message.durationMillis = Milliseconds(ghost.ends - gameClock_);
        transport_->Send(wizard::SerializeMessage(message));
    }
    for (const auto& entry : charges_) {
        const auto& charge = entry.second;
        const bool ballCharge = wizard::IsBallTargetSpell(charge.spell);
        auto caster = FindPlayerByAddress(server, charge.casterPri);
        auto target = FindPlayerByAddress(server, charge.targetPri);
        auto car = caster ? caster.GetCar() : CarWrapper(0);
        auto victim = target ? target.GetCar() : CarWrapper(0);
        auto ball = ballCharge ? server.GetBall() : BallWrapper(0);
        if (!Live(car) || charge.ends <= gameClock_ || (ballCharge ?
            (!ball || ball.memory_address != charge.targetBall) : !Live(victim))) continue;
        wizard::Message message; message.kind = wizard::MessageKind::ChargeSnapshot;
        message.spell = charge.spell; message.recipientId = id;
        message.casterId = charge.casterId; message.targetId = charge.targetId;
        message.source = Core(car.GetLocation()) + wizard::Vec3{0, 0, 40};
        message.destination = ballCharge ? Core(ball.GetLocation()) : Core(victim.GetLocation());
        message.durationMillis = std::min(Milliseconds(wizard::SpellChargeSeconds(charge.spell)),
            Milliseconds(charge.ends - gameClock_));
        transport_->Send(wizard::SerializeMessage(message));
    }
}

void WizardRumble::Practice(const std::vector<std::string>& arguments) {
    auto server = CurrentServer();
    if (!gameWrapper->IsInFreeplay() || !Authoritative(server)) {
        cvarManager->log("wr_practice is available only in local Freeplay.");
        return;
    }
    Tick();
    auto player = LocalPlayer();
    if (!player || !Live(player.GetCar())) return;
    if (arguments.size() != 2) {
        cvarManager->log("Usage: wr_practice accio | avada_kedavra | leviosa | bombarda_maxima | protego | stupefy | petrificus_totalus | obliviate | eat_slugs | expecto_patronum | imperio");
        return;
    }
    for (const auto spell : wizard::ActiveSpells) {
        if (CanonicalSpell(arguments[1]) == wizard::GetSpellInfo(spell).identifier) {
            if (deck_.Grant(player.GetPlayerID(), spell, gameClock_)) {
                SendState(server, player);
                feedback_ = std::string("Practice: ") + wizard::GetSpellInfo(spell).name;
                feedbackUntil_ = Clock::now() + std::chrono::seconds(2);
                cvarManager->log(feedback_ + ". Press R / wr_cast.");
            }
            return;
        }
    }
    cvarManager->log("Unknown or removed spell. Run wr_practice without arguments for the eleven spell identifiers.");
}

void WizardRumble::SelfTest(const std::vector<std::string>& arguments) {
    auto server = CurrentServer();
    if (!gameWrapper->IsInFreeplay() || !Authoritative(server)) {
        cvarManager->log("wr_selftest is available only in local Freeplay.");
        return;
    }
    Tick();
    auto player = LocalPlayer();
    auto car = player ? player.GetCar() : CarWrapper(0);
    if (!player || player.IsSpectator() || !Live(car) || !Active(server) || Setting("wr_enabled", 1) < 0.5f) return;
    wizard::Spell spell = wizard::Spell::None;
    if (arguments.size() == 2) {
        for (const auto allowed : {wizard::Spell::Leviosa, wizard::Spell::PetrificusTotalus, wizard::Spell::Obliviate, wizard::Spell::EatSlugs})
            if (CanonicalSpell(arguments[1]) == wizard::GetSpellInfo(allowed).identifier) spell = allowed;
    }
    if (spell == wizard::Spell::None) {
        cvarManager->log("Usage: wr_selftest leviosa | petrificus_totalus | obliviate | eat_slugs (local car, Freeplay only)");
        return;
    }
    const float startZ = car.GetCurrentRBState().Location.Z;
    liftProbe_.reset();
    wizard::Message effect; effect.kind = wizard::MessageKind::Effect;
    effect.spell = spell; effect.casterId = player.GetPlayerID();
    effect.source = Core(car.GetLocation()) + wizard::Vec3{0, 0, 40};
    if (!ApplySpell(server, player, spell, effect, true)) return;
    ApplyScene(effect);
    if (spell == wizard::Spell::Leviosa || spell == wizard::Spell::PetrificusTotalus) {
        const auto& hold = lifts_.at(player.memory_address);
        const float expected = wizard::SuspensionPosition(Core(hold.start), hold.rise, 1).z - hold.start.Z;
        LiftProbe probe{car.memory_address, player.GetPlayerID(), startZ, startZ, expected, hold.ends + 0.4};
        probe.spell = spell; probe.anchor = Core(hold.start); probe.ends = hold.ends; probe.pri = player.memory_address;
        liftProbe_ = probe;
    }
    cvarManager->log(std::string("Self-test: ") + wizard::GetSpellInfo(spell).name +
        " applied to your car for " + std::to_string(effect.durationMillis / 1000.0) + " seconds.");
}

void WizardRumble::CheckLiftProbe(ServerWrapper server) {
    if (!liftProbe_) return;
    auto player = liftProbe_->pri ? FindPlayerByAddress(server, liftProbe_->pri) : FindPlayer(server, liftProbe_->id);
    auto car = player ? player.GetCar() : CarWrapper(0);
    if (!Live(car) || car.memory_address != liftProbe_->car) { liftProbe_.reset(); return; }
    const auto state = car.GetCurrentRBState();
    const auto held = lifts_.find(liftProbe_->pri);
    const bool observed = held != lifts_.end() && held->second.observedAt >= 0 &&
        gameClock_ - held->second.observedAt < 0.03;
    const float measuredZ = observed ? held->second.observedPosition.z : car.GetCurrentRBLocation().Z;
    liftProbe_->peakZ = std::max(liftProbe_->peakZ, measuredZ);
    if (liftProbe_->spell == wizard::Spell::PetrificusTotalus && gameClock_ >= liftProbe_->ends - 0.15 &&
        gameClock_ < liftProbe_->ends && observed &&
        (held->second.observedPosition - liftProbe_->anchor).Length() < 25 && held->second.observedSpeed < 100)
        liftProbe_->heldNearEnd = true;
    if (gameClock_ < liftProbe_->checkAt) return;
    const float rise = liftProbe_->peakZ - liftProbe_->startZ;
    const bool released = !lifts_.count(liftProbe_->pri) && !car.GetbFrozen();
    const bool falling = state.Location.Z < liftProbe_->peakZ - 10 || state.LinearVelocity.Z < -10;
    if (liftProbe_->spell == wizard::Spell::PetrificusTotalus) {
        cvarManager->log(std::string("Petrificus Totalus live probe: ") +
            (liftProbe_->heldNearEnd && released ? "PASS" : "CHECK") +
            " held_near_2s=" + std::to_string(liftProbe_->heldNearEnd) + " released=" + std::to_string(released) +
            ". Check is from this local test run only.");
    } else {
        cvarManager->log(std::string("Leviosa live probe: ") +
            (liftProbe_->roofReached && released && falling ? "PASS" : "CHECK") +
            " roof_contact=" + std::to_string(liftProbe_->roofReached) +
            " rise=" + std::to_string(rise) + " released=" + std::to_string(released) +
            " falling=" + std::to_string(falling) + ". Check is from this local test run only.");
    }
    liftProbe_.reset();
}

void WizardRumble::BotTest(const std::vector<std::string>& arguments) {
    auto server = CurrentServer();
    auto local = LocalPlayer();
    auto localCar = local ? local.GetCar() : CarWrapper(0);
    if (!Authoritative(server) || gameWrapper->IsInFreeplay() || !local || local.IsSpectator() ||
        !Live(localCar) || !Active(server) || Setting("wr_enabled", 1) < 0.5f) {
        cvarManager->log("wr_bot_test requires an active, enabled LAN match hosted by you with bots only.");
        return;
    }
    wizard::Spell spell = wizard::Spell::None;
    if (arguments.size() == 2) {
        if (arguments[1] == "leviosa") spell = wizard::Spell::Leviosa;
        if (CanonicalSpell(arguments[1]) == "petrificus_totalus") spell = wizard::Spell::PetrificusTotalus;
        if (arguments[1] == "imperio") spell = wizard::Spell::Imperio;
    }
    if (spell == wizard::Spell::None) {
        cvarManager->log("Usage: wr_bot_test leviosa | petrificus_totalus | imperio");
        return;
    }
    PriWrapper target(0);
    auto players = server.GetPRIs();
    if (players.IsNull()) return;
    for (int i = 0; i < players.Count(); ++i) {
        auto player = players.Get(i);
        if (!player || player.IsSpectator() || player.memory_address == local.memory_address) continue;
        if (!player.GetbBot()) { cvarManager->log("wr_bot_test refuses matches containing another human."); return; }
        if (!target && player.GetTeamNum() != local.GetTeamNum() && Live(player.GetCar())) target = player;
    }
    if (!target) { cvarManager->log("wr_bot_test needs an opposing bot on the field."); return; }
    Tick();
    Status();
    cvarManager->getCvar("wr_debug").setValue(1);
    auto car = target.GetCar();
    const float startZ = car.GetCurrentRBLocation().Z;
    wizard::Message effect; effect.kind = wizard::MessageKind::Effect;
    effect.spell = spell; effect.casterId = local.GetPlayerID(); effect.source = Core(localCar.GetLocation());
    if (!ApplySpell(server, local, spell, effect, true, target.memory_address)) return;
    ApplyScene(effect, localCar.memory_address, car.memory_address);
    if (spell == wizard::Spell::Imperio) {
        cvarManager->log("Imperio bot test started on " + target.GetPlayerName().ToString() +
            ". It mirrors your controls for 3.5 seconds.");
        return;
    }
    const auto& hold = lifts_.at(target.memory_address);
    const float expected = wizard::SuspensionPosition(Core(hold.start), hold.rise, 1).z - hold.start.Z;
    LiftProbe probe{car.memory_address, target.GetPlayerID(), startZ, startZ, expected, hold.ends + 0.4};
    probe.spell = spell; probe.anchor = Core(hold.start); probe.ends = hold.ends; probe.pri = target.memory_address;
    liftProbe_ = probe;
    cvarManager->log("LAN bot probe started on " + target.GetPlayerName().ToString() +
        ". Logs will record physical pose and the exact release reason.");
}

void WizardRumble::Draw(CanvasWrapper canvas) {
    if (goalReplay_) { DrawGoalReplay(canvas); return; }
    auto server = CurrentServer();
    if (!Supported(server)) return;
    auto player = LocalPlayer();
    if (!player || player.IsSpectator()) return;
    const bool host = Authoritative(server);
    SpellVisuals::Frame frame;
    frame.localPlayerId = player.GetPlayerID();
    const auto localSlugs = slugs_.find(player.memory_address);
    auto localBody = player.GetCar();
    frame.localPlayerKey = Live(localBody) ? localBody.memory_address : 0;
    frame.localSlugged = localSlugs != slugs_.end() && gameClock_ >= localSlugs->second.began &&
        localSlugs->second.ends > gameClock_ &&
        Live(localBody) && localSlugs->second.car == localBody.memory_address;
    frame.spell = host ? deck_.ReadyFor(player.GetPlayerID()) : localSpell_;
    frame.cooldown = host ? deck_.CooldownFor(player.GetPlayerID()) : localCooldown_;
    frame.remaining = host ? deck_.Remaining(player.GetPlayerID(), gameClock_) :
        std::max(0.0, localRemaining_ - (gameClock_ - stateClock_));
    frame.time = gameClock_;
    frame.hudScale = Setting("wr_hud_scale", 1);
    frame.showHud = Setting("wr_hud", 1) > 0.5f;
    frame.showEffects = Setting("wr_vfx", 1) > 0.5f;
    frame.isHost = host;
    frame.enabled = (host ? Setting("wr_enabled", 1) > 0.5f : hostEnabled_) && Active(server);
    frame.synchronized = host || synchronized_;
    frame.transportReady = host || (transport_ && transport_->Available());
    const auto chargeView = chargeViews_.find(player.GetPlayerID());
    if (frame.enabled && chargeView != chargeViews_.end() && gameClock_ < chargeView->second.ends + 0.3) {
        frame.spell = chargeView->second.spell;
        frame.spellCharging = true;
        frame.avadaCharging = frame.spell == wizard::Spell::AvadaKedavra;
        frame.chargeRemaining = std::max(0.0, chargeView->second.ends - gameClock_);
    }
    if (Clock::now() < feedbackUntil_) frame.feedback = feedback_;
    const auto confused = confusions_.find(player.memory_address);
    if (frame.enabled && confused != confusions_.end() && gameClock_ < confused->second.ends)
        frame.feedback = "Obliviate: controls reversed (" +
            std::to_string(static_cast<int>(std::ceil(confused->second.ends - gameClock_))) + "s)";
    auto localCar = player.GetCar();
    const bool canTarget = frame.enabled && frame.synchronized && frame.transportReady &&
        Live(localCar) && wizard::IsCastableSpell(frame.spell);
    if (canTarget && wizard::IsPlayerTargetSpell(frame.spell)) {
        auto target = frame.spellCharging ? FindPlayerByCar(server, chargeView->second.targetCar) : Target(server, player);
        const auto pending = charges_.find(player.memory_address);
        if (frame.spellCharging && host && pending != charges_.end())
            target = FindPlayerByAddress(server, pending->second.targetPri);
        auto car = target ? target.GetCar() : CarWrapper(0);
        if (Live(car) && wizard::InSpellRange(Core(localCar.GetLocation()), Core(car.GetLocation()), SpellRange(server))) {
            auto offset = Core(car.GetLocalCollisionOffset());
            if (!offset.IsFinite() || offset.LengthSquared() >= 500 * 500) offset = {0, 0, 18};
            frame.target = Core(car.GetLocation()) +
                Core(RotateVectorWithQuat(Engine(offset), RotatorToQuat(car.GetRotation())));
        }
    }
    PopulateWorldFrame(server, frame);
    if (frame.ballValid && canTarget && wizard::IsBallTargetSpell(frame.spell) &&
        wizard::InSpellRange(Core(localCar.GetLocation()), frame.ball, SpellRange(server)))
        frame.target = frame.ball;
    visuals_.Draw(canvas, *gameWrapper, frame, &glow_);
}

void WizardRumble::PopulateWorldFrame(ServerWrapper server, SpellVisuals::Frame& frame) {
    auto ball = server.GetBall();
    frame.ballValid = static_cast<bool>(ball);
    frame.ballKey = ball ? ball.memory_address : 0;
    if (ball) {
        frame.ball = Core(ball.GetLocation());
        const float radius = ball.GetRadius();
        frame.ballRadius = std::isfinite(radius) && radius >= 10 && radius <= 1000 ? radius : 93;
    }
    for (auto current : Players(server)) {
        auto car = current.GetCar();
        if (!Live(car)) continue;
        SpellVisuals::Entity entity;
        entity.key = car.memory_address;
        entity.id = current.GetPlayerID(); entity.position = Core(car.GetLocation());
        const auto orientation = RotatorToQuat(car.GetRotation());
        entity.forward = Core(RotateVectorWithQuat(Vector{1, 0, 0}, orientation)).Normalized();
        entity.right = Core(RotateVectorWithQuat(Vector{0, 1, 0}, orientation)).Normalized();
        entity.up = Core(RotateVectorWithQuat(Vector{0, 0, 1}, orientation)).Normalized();
        auto extent = Core(car.GetLocalCollisionExtent());
        if (extent.IsFinite() && extent.x >= 10 && extent.x <= 500 &&
            extent.y >= 10 && extent.y <= 500 && extent.z >= 5 && extent.z <= 300)
            entity.halfExtent = extent;
        const auto offset = Core(car.GetLocalCollisionOffset());
        if (offset.IsFinite() && offset.LengthSquared() < 500 * 500) entity.localOffset = offset;
        frame.cars.push_back(entity);
    }
    for (const auto& entry : walls_) {
        const auto& wall = entry.second;
        if (wall.ends > gameClock_) frame.walls.push_back({static_cast<std::uint32_t>(entry.first),
            wall.geometry.center, wall.geometry.normal, wall.geometry.radius,
            wall.began, wall.ends, wall.impact});
    }
    for (const auto& entry : patronums_) {
        const auto& ghost = entry.second;
        if (ghost.ends > gameClock_) frame.patronums.push_back({static_cast<std::uint32_t>(entry.first),
            ghost.position, ghost.forward, ghost.began, ghost.ends, ghost.impact, ghost.contact});
    }
}

void WizardRumble::RecordScene(ServerWrapper server, bool force) {
    if (goalReplay_ || !server || server.memory_address != session_ || gameWrapper->IsInFreeplay()) return;
    auto director = server.GetReplayDirector();
    auto replay = director ? director.GetReplay() : ReplaySoccarWrapper(0);
    if (!replay) return;
    const double timeline = replay.GetCurrentTime();
    if (!std::isfinite(timeline) || timeline < 0) return;
    // Copy only cosmetics/poses. No saved wrapper is ever dereferenced in playback.
    SpellVisuals::Frame frame;
    frame.time = gameClock_; frame.enabled = true; frame.showHud = false;
    frame.replayClockRunning = Active(server);
    PopulateWorldFrame(server, frame);
    replayHistory_.Record(timeline, ReplayScene{visuals_, std::move(frame)}, force);
}

void WizardRumble::BeginGoalReplay(ServerWrapper server) {
    // session_ was established only for a supported live match. The replay
    // transition may already have changed IsInReplay(), so do not gate on it.
    if (!server || server.memory_address != session_) return;
    goalReplay_ = true;
    // Restore native ball tuning without truncating the recorded voice's
    // original interval; the replay flag also prevents new live capture here.
    StopRicochet(server, false);
    StopSounds();
    replayAudio_.ResetPlayback();
    lastTick_ = Clock::now();
    if (Setting("wr_debug", 0) > 0.5f)
        cvarManager->log("Goal replay began: visual samples=" + std::to_string(replayHistory_.Size()));
}

void WizardRumble::EndGoalReplay() {
    replayDebugAt_ = -1;
    StopSounds(); replayAudio_.ResetPlayback();
    auto server = gameWrapper->IsInOnlineGame() ? gameWrapper->GetOnlineGame() : gameWrapper->GetGameEventAsServer();
    StopRicochet(server, false);
    if (server && server.memory_address == session_ && Authoritative(server)) {
        ClearEffects(server); deck_.Clear(); sentSpells_.clear();
    } else {
        visuals_.ClearEffects(); chargeViews_.clear(); confusions_.clear(); imperios_.clear(); slugs_.clear(); walls_.clear(); patronums_.clear(); StopSounds();
        lifts_.clear(); charges_.clear(); summon_.reset(); liftProbe_.reset();
        previousBall_.reset(); previousBallActor_ = 0; lastHoldUpdate_ = -1;
        deck_.Clear(); sentSpells_.clear();
    }
    goalReplay_ = false;
    lastTick_ = Clock::now(); // Replay duration never advances cooldowns/holds.
    wasActive_ = false;
}

void WizardRumble::DrawGoalReplay(CanvasWrapper canvas) {
    auto server = gameWrapper->IsInOnlineGame() ? gameWrapper->GetOnlineGame() : gameWrapper->GetGameEventAsServer();
    if (!server || server.memory_address != session_) { EndGoalReplay(); return; }
    auto director = server.GetReplayDirector();
    auto replay = director ? director.GetReplay() : ReplaySoccarWrapper(0);
    if (!replay) { ReplaySounds(std::numeric_limits<double>::quiet_NaN()); return; }
    const double timeline = replay.GetCurrentTime();
    ReplaySounds(timeline);
    if (Setting("wr_vfx", 1) < 0.5f) return;
    const auto span = replayHistory_.AtSpan(timeline);
    if (Setting("wr_debug", 0) > 0.5f && (replayDebugAt_ < 0 || std::abs(timeline - replayDebugAt_) > 1)) {
        cvarManager->log("Goal replay time=" + std::to_string(timeline) + " samples=" +
            std::to_string(replayHistory_.Size()) + " matched=" + std::to_string(span.before != nullptr));
        replayDebugAt_ = timeline;
    }
    if (!span.before) return;
    // Discrete spell events come only from the earlier snapshot. Interpolate
    // copied poses and the visual clock; never resurrect a future cast.
    auto scene = span.before->value;
    scene.frame = wizard::SmoothReplayFrame(scene.frame,
        span.after ? &span.after->value.frame : nullptr, span.blend,
        timeline - span.before->timeline);
    scene.frame.showHud = false; scene.frame.showEffects = true;
    scene.frame.localSlugged = false; scene.frame.localPlayerId = -1; scene.frame.localPlayerKey = 0;
    scene.frame.target.reset(); scene.frame.feedback.clear();
    scene.visuals.Draw(canvas, *gameWrapper, scene.frame, &glow_);
}

void WizardRumble::Status() {
    auto server = CurrentServer();
    cvarManager->log("Wizard Rumble 0.9.2 | supported=" + std::to_string(Supported(server)) +
        " host=" + std::to_string(server && Authoritative(server)) +
        " transport=" + std::to_string(transport_ && transport_->Available()) +
        " synced=" + std::to_string(synchronized_) + " peers=" + std::to_string(peers_.size()) +
        " range=" + std::to_string(SpellRange(server)) +
        " range_revision=" + std::to_string(Setting("wr_range_revision", 0)) +
        " patronums=" + std::to_string(patronums_.size()) +
        " walls=" + std::to_string(walls_.size()) + " holds=" + std::to_string(lifts_.size()) +
        " obliviate=" + std::to_string(confusions_.size()) + " charges=" + std::to_string(charges_.size()) +
        " accio=" + std::to_string(summon_.has_value()) + " slugs=" + std::to_string(slugs_.size()) +
        " audio=" + std::to_string(audio_.Ready()) + " leviosa_voice=" + std::to_string(leviosaVoice_.Ready()) +
        " slugs_voice=" + std::to_string(slugsVoice_.Ready()) +
        " leviosa_float=" + std::to_string(leviosaFloat_.Ready()) + " stupefy_voice=" + std::to_string(stupefyVoice_.Ready()) +
        " petrificus_voice=" + std::to_string(petrificusVoice_.Ready()) +
        " protego_voice=" + std::to_string(protegoVoice_.Ready()) +
        " bombarda_voice=" + std::to_string(bombardaVoice_.Ready()) +
        " patronum_voice=" + std::to_string(patronumVoice_.Ready()) +
        " obliviate_voice=" + std::to_string(obliviateVoice_.Ready()) +
        " accio_voice=" + std::to_string(accioVoice_.Ready()) + " imperio_voice=" + std::to_string(imperioVoice_.Ready()) +
        " imperio=" + std::to_string(imperios_.size()) + " ricochet=" + std::to_string(ricochet_.has_value()) +
        " glow_textures=" + std::to_string(glow_.Ready()) +
        " sfx_volume=" + std::to_string(Setting("wr_sfx_volume", 1)));
    cvarManager->log("goal replay=" + std::to_string(goalReplay_) +
        " recorded visual samples=" + std::to_string(replayHistory_.Size()) +
        " recorded audio events=" + std::to_string(replayAudio_.EventCount()));
    if (!audio_.Ready()) cvarManager->log("Wizard Rumble audio: " + audio_.Error());
    if (!glow_.Ready()) cvarManager->log("Wizard Rumble glow textures: " + glow_.Error());
    if (server) {
        auto owner = server.GetGameOwner();
        auto local = LocalPlayer();
        cvarManager->log("local player=" + std::to_string(local ? local.GetPlayerID() : -1) +
            " game owner=" + std::to_string(owner ? owner.GetPlayerID() : -1) +
            " active=" + std::to_string(Active(server)) + " enabled=" + std::to_string(Setting("wr_enabled", 1)));
        auto roster = server.GetPRIs();
        for (int i = 0; !roster.IsNull() && i < roster.Count(); ++i) {
            auto player = roster.Get(i);
            if (!player) continue;
            auto car = player.GetCar();
            cvarManager->log("Roster id=" + std::to_string(player.GetPlayerID()) +
                " bot=" + std::to_string(player.GetbBot() != 0) + " pri=" + std::to_string(player.memory_address) +
                " car=" + std::to_string(car ? car.memory_address : 0) + " name=" + player.GetPlayerName().ToString());
        }
    }
}
