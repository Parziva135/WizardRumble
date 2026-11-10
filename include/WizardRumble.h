#pragma once

#include "SpellCore.h"
#include "SpellTransport.h"
#include "SpellVisuals.h"
#include "SpellGlow.h"
#include "SpellAudio.h"
#include "SpellReplay.h"
#include "SpellReplayAudio.h"
#include "bakkesmod/plugin/bakkesmodplugin.h"

#include <chrono>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <vector>

class WizardRumble final : public BakkesMod::Plugin::BakkesModPlugin {
public:
    void onLoad() override;
    void onUnload() override;

private:
    using Clock = std::chrono::steady_clock;
    struct Summon {
        int caster;
        std::uintptr_t pri;
        std::uintptr_t car;
        std::uintptr_t ball;
        double began;
        double ends;
        double lastUpdate;
        double approach;
        double settled = 0;
        float lastTouch = 0;
    };
    struct ChargedCast {
        wizard::Spell spell;
        int casterId;
        int targetId;
        std::uintptr_t casterPri;
        std::uintptr_t casterCar;
        std::uintptr_t targetPri;
        std::uintptr_t targetCar;
        double began;
        double ends;
        std::uintptr_t targetBall = 0;
    };
    struct ChargeView {
        wizard::Spell spell;
        int targetId;
        wizard::Vec3 destination;
        double ends;
        std::uintptr_t casterCar = 0;
        std::uintptr_t targetCar = 0;
    };
    struct SlugState {
        int caster;
        int target;
        std::uintptr_t pri;
        std::uintptr_t car;
        double began;
        double ends;
    };
    struct Lift {
        int id;
        std::uintptr_t pri;
        std::uintptr_t car;
        Vector start;
        Vector velocity;
        Rotator rotation;
        double began;
        double ends;
        float rise;
        wizard::Spell spell;
        int caster;
        double nextLog = 0;
        wizard::Vec3 observedPosition;
        float observedSpeed = 0;
        double observedAt = -1;
    };
    struct Confusion {
        std::uintptr_t pri;
        std::uintptr_t car;
        double began;
        double ends;
        int caster;
        int targetId = -1;
    };
    struct Imperio {
        int caster;
        int target;
        std::uintptr_t casterPri;
        std::uintptr_t casterCar;
        std::uintptr_t pri;
        std::uintptr_t car;
        double began;
        double ends;
    };
    struct Ricochet {
        int caster;
        std::uintptr_t casterCar;
        std::uintptr_t ball;
        float originalMax;
        float originalSpeedScale;
        float originalBounce;
        float appliedMax;
        float appliedSpeedScale;
        float appliedBounce;
        double began;
        double ends;
    };
    struct WallState {
        wizard::Barrier geometry;
        int caster;
        double began;
        double ends;
        double impact = -1;
    };
    struct PatronumState {
        int caster;
        std::uintptr_t pri;
        std::uintptr_t car;
        std::uintptr_t ball;
        int team;
        wizard::Vec3 position;
        wizard::Vec3 forward;
        wizard::Vec3 previousBall;
        wizard::Vec3 goal;
        double began;
        double ends;
        double lastUpdate;
        double impact = -1;
        wizard::Vec3 contact;
    };
    struct LiftProbe {
        std::uintptr_t car;
        int id;
        float startZ;
        float peakZ;
        float expectedRise;
        double checkAt;
        wizard::Spell spell = wizard::Spell::Leviosa;
        wizard::Vec3 anchor;
        double ends = 0;
        bool heldNearEnd = false;
        std::uintptr_t pri = 0;
        bool roofReached = false;
        double releasedAt = -1;
    };
    struct ReplayScene {
        SpellVisuals visuals;
        SpellVisuals::Frame frame;
    };
    ServerWrapper CurrentServer();
    bool Supported(ServerWrapper server);
    bool Authoritative(ServerWrapper server);
    bool Active(ServerWrapper server);
    PriWrapper LocalPlayer();
    PriWrapper FindPlayer(ServerWrapper server, int id);
    PriWrapper FindPlayerByAddress(ServerWrapper server, std::uintptr_t address);
    PriWrapper FindPlayerByCar(ServerWrapper server, std::uintptr_t address);
    PriWrapper SceneTarget(ServerWrapper server, int id, wizard::Vec3 position);
    std::vector<PriWrapper> Players(ServerWrapper server);
    PriWrapper Target(ServerWrapper server, PriWrapper caster);
    void Tick();
    void Heartbeat();
    void ResetSession();
    void ClearEffects(ServerWrapper server);
    void UpdateEffects(ServerWrapper server);
    void UpdatePatronums(ServerWrapper server);
    void PatronumImpact(const wizard::Message& message);
    std::optional<wizard::Vec3> OpponentGoal(ServerWrapper server, int team);
    void UpdateSummon(ServerWrapper server);
    void StopSummon(bool notify = true);
    void UpdateCharges(ServerWrapper server);
    void CancelCharge(std::uintptr_t caster, bool notify = true);
    void UpdateLocalEffects(ServerWrapper server, bool active);
    void StopSounds();
    SpellAudio& Sound(wizard::SpellCue cue);
    bool PlaySound(wizard::SpellCue cue, std::uintptr_t casterCar, std::uintptr_t targetCar);
    void StopRecordedSounds(ServerWrapper server, wizard::SpellCue cue = wizard::SpellCue::Count,
        std::uintptr_t casterCar = 0, std::uintptr_t targetCar = 0);
    void ReplaySounds(double timeline);
    void ReleaseSlugs(std::uintptr_t target, bool notify = true);
    void VehicleInput(CarWrapper car, void* parameters);
    void HoldPhysics(CarWrapper car, const Lift& lift);
    bool LeviosaRoofContact(CarWrapper car);
    void ReleaseHold(ServerWrapper server, std::uintptr_t pri, const char* reason = "interrupted");
    void ReleaseConfusion(std::uintptr_t pri, bool notify = true);
    void ReleaseImperio(std::uintptr_t target, bool notify = true);
    void UpdateImperios(ServerWrapper server);
    bool ApplyImperioInput(ServerWrapper server, CarWrapper car, ControllerInput& input);
    bool StartRicochet(ServerWrapper server, PriWrapper caster, wizard::Message& effect);
    void StopRicochet(ServerWrapper server, bool notify = true);
    void UpdateRicochet(ServerWrapper server);
    void BeforeDemolish(CarWrapper car);
    void RequestCast();
    void Cast(ServerWrapper server, PriWrapper caster);
    bool ApplySpell(ServerWrapper server, PriWrapper caster, wizard::Spell spell, wizard::Message& effect,
        bool selfTest = false, std::uintptr_t testTarget = 0);
    void Receive(const std::string& payload, PriWrapper sender);
    void SendState(ServerWrapper server, PriWrapper player);
    void Feedback(PriWrapper player, wizard::Feedback code);
    void ShowFeedback(wizard::Feedback code);
    void ApplyScene(const wizard::Message& message, std::uintptr_t casterCar = 0, std::uintptr_t targetCar = 0);
    void CancelScene(wizard::Spell spell, int caster, int target, bool notify = true,
        std::uintptr_t casterCar = 0, std::uintptr_t targetCar = 0);
    void SendSceneSnapshot(ServerWrapper server, PriWrapper recipient);
    void Practice(const std::vector<std::string>& arguments);
    void SelfTest(const std::vector<std::string>& arguments);
    void BotTest(const std::vector<std::string>& arguments);
    void CheckLiftProbe(ServerWrapper server);
    void Draw(CanvasWrapper canvas);
    void PopulateWorldFrame(ServerWrapper server, SpellVisuals::Frame& frame);
    void RecordScene(ServerWrapper server, bool force = false);
    void BeginGoalReplay(ServerWrapper server);
    void EndGoalReplay();
    void DrawGoalReplay(CanvasWrapper canvas);
    void Status();
    float Setting(const std::string& name, float fallback);
    float SpellRange(ServerWrapper server);

    wizard::SpellDeck deck_{std::random_device{}()};
    std::unique_ptr<SpellTransport> transport_;
    std::map<int, std::uintptr_t> identities_;
    std::map<int, wizard::Spell> sentSpells_;
    std::map<int, double> lastRequest_;
    std::map<std::uintptr_t, Lift> lifts_;
    std::map<std::uintptr_t, Confusion> confusions_;
    std::map<std::uintptr_t, Imperio> imperios_;
    std::optional<Ricochet> ricochet_;
    std::optional<LiftProbe> liftProbe_;
    std::map<int, WallState> walls_;
    std::map<int, PatronumState> patronums_;
    std::optional<wizard::Vec3> previousBall_;
    std::uintptr_t previousBallActor_ = 0;
    std::set<int> peers_;
    SpellGlow glow_;
    SpellVisuals visuals_;
    wizard::ReplayHistory<ReplayScene> replayHistory_;
    wizard::ReplayAudioTrack replayAudio_;
    bool goalReplay_ = false;
    double replayDebugAt_ = -1;
    SpellAudio audio_;
    int audioCaster_ = -1;
    std::uintptr_t audioCasterCar_ = 0;
    SpellAudio leviosaVoice_;
    SpellAudio leviosaFloat_;
    std::uintptr_t leviosaFloatCasterCar_ = 0;
    std::uintptr_t leviosaFloatTargetCar_ = 0;
    SpellAudio stupefyVoice_;
    int stupefyVoiceCaster_ = -1;
    int stupefyVoiceTarget_ = -1;
    std::uintptr_t stupefyVoiceCasterCar_ = 0;
    std::uintptr_t stupefyVoiceTargetCar_ = 0;
    SpellAudio petrificusVoice_;
    int petrificusVoiceCaster_ = -1;
    int petrificusVoiceTarget_ = -1;
    std::uintptr_t petrificusVoiceCasterCar_ = 0;
    std::uintptr_t petrificusVoiceTargetCar_ = 0;
    SpellAudio protegoVoice_;
    int protegoVoiceCaster_ = -1;
    std::uintptr_t protegoVoiceCasterCar_ = 0;
    SpellAudio bombardaVoice_;
    int bombardaVoiceCaster_ = -1;
    std::uintptr_t bombardaVoiceCasterCar_ = 0;
    SpellAudio patronumVoice_;
    int patronumVoiceCaster_ = -1;
    std::uintptr_t patronumVoiceCasterCar_ = 0;
    SpellAudio obliviateVoice_;
    int obliviateVoiceCaster_ = -1;
    int obliviateVoiceTarget_ = -1;
    std::uintptr_t obliviateVoiceCasterCar_ = 0;
    std::uintptr_t obliviateVoiceTargetCar_ = 0;
    SpellAudio accioVoice_;
    int accioVoiceCaster_ = -1;
    std::uintptr_t accioVoiceCasterCar_ = 0;
    SpellAudio imperioVoice_;
    int imperioVoiceCaster_ = -1;
    int imperioVoiceTarget_ = -1;
    std::uintptr_t imperioVoiceCasterCar_ = 0;
    std::uintptr_t imperioVoiceTargetCar_ = 0;
    SpellAudio slugsVoice_;
    int slugsVoiceCaster_ = -1;
    int slugsVoiceTarget_ = -1;
    std::uintptr_t slugsVoiceCasterCar_ = 0;
    std::uintptr_t slugsVoiceTargetCar_ = 0;
    int leviosaVoiceCaster_ = -1;
    int leviosaVoiceTarget_ = -1;
    std::uintptr_t leviosaVoiceCasterCar_ = 0;
    std::uintptr_t leviosaVoiceTargetCar_ = 0;
    std::map<std::uintptr_t, SlugState> slugs_;
    std::optional<Summon> summon_;
    std::map<std::uintptr_t, ChargedCast> charges_;
    std::map<int, ChargeView> chargeViews_;
    std::uintptr_t session_ = 0;
    double gameClock_ = 0;
    double lastHoldUpdate_ = -1;
    bool wasActive_ = false;
    bool wasEnabled_ = false;
    std::string previousRBind_;
    bool receiving_ = false;
    bool ticking_ = false;
    wizard::Spell localSpell_ = wizard::Spell::None;
    double localRemaining_ = 0;
    double localCooldown_ = 10;
    bool rangeDefaultsChecked_ = false;
    float hostRange_ = wizard::DefaultSpellRange;
    float sentRange_ = -1;
    double stateClock_ = 0;
    bool hostEnabled_ = false;
    bool synchronized_ = false;
    Clock::time_point stateReceived_{};
    Clock::time_point lastTick_{};
    Clock::time_point lastHeartbeat_{};
    Clock::time_point lastHello_{};
    Clock::time_point lastLocalCast_{};
    Clock::time_point feedbackUntil_{};
    std::string feedback_;
};
