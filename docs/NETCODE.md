# LAN transport

Version **0.9.2** adds Accio's **500 ms** ball-target charge and Imperio's **1,000 ms** locked-opponent charge. Both use the existing G/J charge messages and host revalidation, with E releasing gameplay only after the deadline. Accio retains its three-second maximum guidance after release; Imperio retains its 3.5-second control mirror after release. G plays each new local voice once; J/P/E do not repeat it. Canceling an invalid charge retains the spell and stops its owned audio. An older same-pair effect is retired before a new charge so its cancellation cannot remove the new charge. Every PC must use the **0.9.2** DLL and all **twelve WAVs**, including `accio.wav` and `imperio.wav`, plus both glow PNGs. Earlier DLLs do not recognize the new charge kinds for these spell IDs.

Since 0.6.1, local effects and Obliviate state bind to the actual selected car/PRI. Charged effects retain this binding through completion rather than reacquiring the nearest same-numbered bot every draw. Local cancellation also scopes voices and visuals to that car. The W2 wire format is unchanged; cancellation packets still identify actors numerically, so simultaneous same-ID bot effects on a remote client can be cosmetically canceled together. Host bot cleanup is exact; remote bot/latency behavior needs a two-PC playtest.

Each PC records its own bounded cosmetic history for immediate goal replays. Playback reads that PC's replay timeline and never transmits new cast intent or reapplies gameplay. Since 0.6.2, fresh G/E events also record local sound cues for the goal replay. Sounds join at their replay offset without restarting each frame, honor local sound/volume settings independently of world VFX, and stop when the replay ends or the world changes. This history is not part of exported Rocket League replay files.

Version 0.9.0 recovers original cosmetic age from remaining P lifetime for Accio, Leviosa and Petrificus, alongside the existing sustained-status normalization. Repeated same-caster Bombarda P replaces its glow rather than stacking it. Client Protego and Patronum P also recover the wall/ghost birth time while keeping their remaining expiry and simulation update clock unchanged. Refreshes therefore preserve the current appearance phase without replaying a cast or sound; no W2 fields or spell physics change.

Version 0.9.0 interpolates matching cosmetic poses and the animation clock between adjacent 30 Hz samples separated by no more than 0.1 seconds. Spell events and other discrete state still come only from the earlier snapshot, preventing future casts, statuses or spawns from appearing early. Cars match unique actual actor keys, the ball matches its original actor and Patronum matches the same flight/impact phase. Rotation uses shortest-path quaternion interpolation; respawns, teleports and missing intervals do not blend. At the captured tail, poses hold and clock advancement is bounded to a running captured clock. Seeking or pausing replay does not mutate history or run host gameplay. Native alignment with the game's replay camera still needs playtesting.

WizardRumble uses the installed **NetcodePlugin** (BakkesPlugins ID **166**).
Install that dependency on every player's PC using BakkesMod's Plugin Manager.
Every participant must set Rocket League chat to **Allow All** and leave the
other participants unmuted: the dependency transports invisible chat messages.
The mod only sends or accepts transport packets in a LAN playlist, outside
replays. A normal Rocket League online private match uses a remote server and
does not give this mod authority to change gameplay.

The adapter is independently implemented against the cvar protocol documented
by the public NetcodeManager sources. It does not copy or compile NetcodeManager.
The protocol-facing channel name must match the BakkesMod plugin class name:
`WizardRumble`.

| Interface | Data |
| --- | --- |
| Write `NETCODE_Message_Out` | `[WizardRumble]payload` |
| Observe `NETCODE_Message_In` | `[WizardRumble][decimal sender PRI address]payload` |

NetcodePlugin forwards a client's messages to the host and other clients. The
application must distinguish requests from authoritative state. Gameplay
decisions, held spells, cooldowns and target validity belong to the host.
The 128-character chat limit includes transport framing; this channel permits
110 bytes of printable ASCII payload per message. Messages are kept compact.

Version 0.2.2 extends `W2 S` state messages with an optional host spell-range
field, bounded to 500–10,000 units. A legacy state without that field is accepted
with the historical range of 4,500 units. The host also resends state when its range
setting changes, so client crosshair previews use the host's current value.
Every PC should run 0.9.2 with all twelve bundled WAV files for the current charge behavior, audio,
spell visuals, range, pool and controls. The crosshair is
a local range/aim preview; the host still selects and validates the target when
it receives a cast request.

Version 0.3.0 added Confundo at spell ID 13. In 0.9.0 it becomes Obliviate, retaining the same ID and five-second directional inversion; Imperio is added at ID 16.
The current pool has eleven spells, including Imperio at ID 16: Eat Slugs keeps ID 14, Expecto Patronum is ID 15, the renamed Petrificus Totalus keeps ID 9, and Bombarda Maxima replaces Depulso at ID 4. IDs 5 (Glacius), 7, 10 (Descendo), 11 (Expelliarmus) and 12
remain reserved and invalid in held-state, effect, snapshot and cancellation
messages. Do not mix old and current DLLs: a shared W2 prefix does not imply
that an older release supports the same spells.

Version 0.3.2 retained the wire format and eight-spell pool. The spell now named
Petrificus Totalus (then Arresto Momentum) keeps its two-second host-authored duration. Protego is a circular disk of radius
350 units, shared by current host collision and client visuals. All PCs must
update together; the W2 prefix alone does not identify barrier geometry.

Version 0.7.0 adds Expecto Patronum as ball-target spell ID 15 and renames spell ID 9 to Petrificus Totalus without changing its two-second hold. The host owns the Patronum flight and its single permitted ball impulse toward the opposing goal. In 0.9.0 the larger ghost chases at up to 1,700 units/second (between 0.7.3's 1,000 and 0.7.2's 2,500) for at most 2.5 seconds; swept contact permits one 2,400-unit/second shot, followed by about 0.45 seconds of visual fade. The host selects a goal by its opposing team ownership, rather than assuming a goal array index. The compiled Octane mesh and cosmetic flight on each PC do not gain physics authority. Native replication supplies the resulting ball motion; scene snapshots must join only the remaining flight, and goal replay drawing must never reapply its hit. The supplied GLB is converted at development time, not loaded or transferred over chat. Its derivative mesh attribution is included in [MODEL_CREDITS.md](MODEL_CREDITS.md).

Version 0.9.0 adds a three-second ball-target G/J charge for Patronum before E/P flight begins. G starts its three-second voice once; J snapshots join only the remaining charge and stay silent. The host retains and revalidates the original ball and caster before launch. No ghost flight or ball impulse occurs during G/J. Normal X with spell ID 15 and target -1 cancels a charge or pending flight; U remains Protego-only. The host therefore keeps the ready spell instead of starting another Patronum charge while that caster's previous flight is still active. Its completed 0.45-second fade can overlap a new charge because fade expiry sends no X. Patronum starts or joins flight through the E/P scene events. Its host-only impact uses the dedicated nine-token `W2 V caster x y z nx ny nz` packet: an integer contact position and a direction quantized at 1,000 units per component. The direction must be finite and near unit length; malformed coordinates, extra fields and invalid caster IDs are rejected. `V` supplies cosmetic impact state to clients and never authorizes a client-originated gameplay hit. `X` cancels a pending flight. All participants must update to 0.9.2; earlier W2 clients do not understand the ball-target Protego charge, U charge-only cancellation or all current spell timing/events.

Version 0.3.3 increases the default host range to 5,500 units. A one-time
check on the first supported-session tick migrates a saved old default of
exactly 4,500; custom nondefault ranges remain. A persisted hidden revision
prevents repeating the migration. Native config-load timing remains a live
check; pure protocol tests do not execute BakkesMod's cvar/config lifecycle.
Current state messages transmit the actual bounded host range, so clients use
the same value for their crosshair previews. Legacy seven-token states retain
their 4,500-unit meaning. The stronger casting animations use the existing
effect events and do not transfer physics authority or introduce audio data.

Version 0.4.0 introduced `W2 G` for a fresh Avada Kedavra charge and `W2 J` for a
recipient's remaining-charge snapshot. Both use the existing 16-token effect
layout: `W2`, kind, spell, caster, target, recipient, source vector, destination
vector, normal vector and duration. Version 0.5.0 extended these charge kinds
to Wingardium Leviosa. Version 0.6.0
also accepts Eat Slugs. Version 0.9.0 adds Obliviate at ID 13. Version 0.7.0 adds Stupefy, 0.7.1 adds Petrificus Totalus and 0.7.2 adds Protego and 0.7.3 adds Bombarda Maxima at ID 4. Version 0.9.0 adds Patronum's charge at ID 15. Version 0.9.2 adds Accio at ID 1 and Imperio at ID 16. Opponent charges require a real target ID; Accio, Protego, Bombarda and Patronum require target ID -1 for their original ball actor. They have a spell-specific
remaining duration: 1–3,000 milliseconds for Patronum, 1–2,000 for Avada/Leviosa/Bombarda Maxima, 1–1,000 for Protego/Imperio,
1–650 for Stupefy, 1–500 for Obliviate/Accio and 1–1,500 for Eat Slugs/Petrificus Totalus. Removed and invalid spells are rejected in G/J messages. Existing framing, bounded
coordinates, sender/recipient checks and the 110-byte payload limit still
apply. An unchanged W2 prefix does not make old DLLs compatible with these
new message kinds.


Version 0.7.2 treats Protego as a one-second ball charge. The host captures the original ball identity and caster PRI/car; it rechecks both and range before placing the wall from that ball's current movement. It never redirects a pending cast to a replacement ball. An older active wall continues while a new charge runs. `W2 U caster` is the compact three-token charge-only cancellation: it stops Protego's pending visuals/voice without removing that older wall. `X` remains wall cancellation, while a successful E replaces the previous wall. Clients accept U only from the validated host, as with E/J/P/X. Routine scene snapshots must restore remaining charge without replaying speech. The natural Protego clip can continue after one-second release, without starting again at E.

Bombarda Maxima keeps ID 4 and its two-second ball-target G/J charge, binding the original ball actor and caster pair. The host revalidates the ball, caster and range before the stronger 0.9.0 blast and temporary ricochet effect. A canceled Bombarda charge uses ordinary `W2 X` for spell 4 with target -1; `W2 U caster` remains specifically Protego charge cancellation so an older ward is preserved. No Bombarda hit occurs before successful E, and a replacement ball never inherits it.

Decorative world charge circles are removed in 0.7.2. Functional HUD countdowns and targeting marks remain. Avada's G glow follows the exact locked car; its stringy branched lightning and green impact use E and the captured endpoint. Cosmetic glow, impact and replay drawing never add demo authority or another physics application.

For opponent charges, the host locks the current caster and target PRI/car pairs at charge start.
After the spell-specific 0.5-, 0.65-, one-, 1.5- or two-second interval it revalidates that same
live opposing car and range, without choosing another target. A successful
demo, Leviosa ascent, Stupefy knockback, Petrificus bind, Obliviate reversal or Eat Slugs release consumes the charged spell and begins
cooldown. Leviosa begins rising only after its two-second charge, at 700 units/second through native velocity and stadium collision. A current nearby rigid-body roof contact above the car releases it and lets gravity resume; no fixed stadium roof height is assumed. An eight-second active-time safety limit releases it on open-roof or missing-contact maps. Petrificus binds for two seconds only after its 1.5-second charge. Invalid actors, interrupted play or cancellation cannot authorize a
replacement target. `W2 X` removes a canceled opponent charge and its local feedback;
scene clearing removes remaining charge state as well.

Caster/target invalidation during active play preserves the unconsumed spell.
Pause, goal and disable use the existing whole-dispenser reset and begin fresh
cooldowns. They do not preserve the held spell across a session transition.

Fresh accepted `G` events start Avada, Leviosa, Eat Slugs, Stupefy, Petrificus, Protego, Bombarda, Patronum, Obliviate, Accio and Imperio voices. A fresh
successful Leviosa `E` starts the supplied floating effect once at lift onset;
Stupefy G/J carries at most 650 milliseconds; its fresh G starts the voice, and only a successful E releases the red flash and knockback. Petrificus G/J carries at most 1,500 milliseconds; G starts its new voice once, and E begins the two-second body bind without playing the voice again.
`J` and `P` snapshots remain silent and do not add replay sound cues. Audio is
not transmitted over chat: every participant installs the same twelve local files
under `data/wizardrumble/audio`: `avada_kedavra.wav`, `leviosa_cast.wav`,
`leviosa_float.wav`, `eat_slugs.wav`, `stupefy.wav`, `petrificus_totalus.wav`, `protego.wav`, `expecto_patronum.wav`, `bombarda_maxima.wav`, `obliviate.wav`, `accio.wav` and `imperio.wav`. The replacement Leviosa
voice lasts two seconds; its separate floating clip is a three-second one-shot, not a loop, and can finish before a long ascent ends. Petrificus's supplied voice is a 1.5-second clip. Bombarda's full 5.3106667-second recording starts at G and continues naturally beyond its two-second charge; E does not restart or truncate it. The converted file includes the source audio from beginning to EOF at natural speed. Patronum's three-second voice is exactly source time 0–3.000 seconds, starting at G with no initial offset or tempo change. Successful E launches the Octane only after the three-second charge and never replays speech. J/P snapshots and V impact remain silent. Replay audio records each new cue once and honors offset joins and cleanup.
Local `wr_sfx` and
`wr_sfx_volume` preferences do not affect host timing, target validation or
another player's volume. The cached PCM player never changes Windows or game
master volume.

Routine recipient-targeted scene refreshes can rebuild persistent visuals
without stopping an already playing charge clip. Broadcast round/session
clears stop voice playback. Accio's persistent tether uses a remaining-effect snapshot
without replaying its opening cast.

Accio guidance remains host-authoritative. The host updates a bounded velocity
command toward the moving caster's front point for at most three active seconds,
then returns the ball to ordinary physics. A short settled period can release it
earlier. Touches, Bombarda Maxima, ward collisions,
actor replacement and session cleanup end guidance. Clients receive native
ball replication and cosmetic effect events, not a second ball controller.
These parser and authority rules require native LAN tests; they do not prove
remote visual timing, audio-device playback or ball control feel.

Eat Slugs uses a host-authorized 1.5-second G/J charge. Its voice starts on fresh G; there is no world volley or victim overlay during charge. Successful E launches the blobs and consumes the spell. Its E/P duration covers both phases: **4.10 seconds total**, comprising 0.35 seconds of flight followed by 3.75 seconds of obstruction. P carries only the remaining combined lifetime, allowing each receiver to join the correct phase without restarting speech or flight. Local victim state stores its start at arrival and ends after the full obstruction. The exact victim PRI/car stays bound; invalidation during flight cancels the spent projectile with X. The overlay remains gameplay state when cosmetic VFX/HUD are hidden. Replacement actors, demolition, spectator and round/session cleanup remove the owned effect. A caster cannot start another same-pair charge while earlier pending or active Slugs remain. Native two-PC alignment still needs playtesting.

Host holds/ascent retain the current PRI/car actor pair instead of resolving a
numeric roster ID for each correction. Leviosa no longer uses position teleports or the old lift-height/seconds settings; those persisted settings are ignored. Petrificus retains pose writes for its two-second bind. This supports the LAN-bot investigation
without changing wire sender validation. It does not establish a passed live
hold test; capture the bot-test pose and release diagnostics first.

Obliviate is a host-authorized half-second charge followed by five seconds of reversed directional input. Its accepted G starts the supplied voice once; the yellow release beam and inversion occur only on successful E. J/P remain silent. Its input handling and restoration
must be checked on the victim Client as well as Host, including prediction,
late snapshots, cancellation and replacement cars. Passing protocol tests
does not establish those live results.

Imperio is spell ID 16, an immediate host-authorized E/P effect with a 3.5-second lifetime. The host records the actual caster/victim PRI-car pairs and owns all authoritative control copying; a client applies mirrored input only to its own current victim for prediction. The source is the caster car's native GetInput, normalized/sanitized across seven axes and five flags (Handbrake, Jump, ActivateBoost, HoldingBoost, Jumped). There is no chat input-stream packet or client-selected control data. A source already under Imperio cannot cast another link, preventing feedback cycles. Holds win over mirrored input, then mirrored input wins over the victim's own Obliviate inversion. X/remaining snapshots and actor/round cleanup retain normal ownership rules. Actual source-input replication and prediction need two-PC validation.

Bombarda's charged E applies its 12,000-unit initial velocity command even when optional native speed/bounce fields are zero/default or invalid. Valid tuning is raised for five seconds; unsupported fields are skipped, leaving their native limits in place. The original ball identity and applied tuning are stored; owned expiry/cancel/round cleanup restores only changed settings without overwriting newer unrelated tuning. Collision remains native stadium physics. Another Bombarda replaces the prior buff, while competing guidance cancels it. The same caster keeps a ready Bombarda instead of charging while their older buff is active, avoiding an old X stopping a newer voice. A successful release consumes the spell and starts the ordinary cooldown. No client becomes a second ball controller.

`SpellTransport::Start` may load an already installed NetcodePlugin DLL. It
never installs or downloads it. Call `Tick` on the game thread to detect when
the dependency becomes ready. Call `Stop` before destroying the plugin; it
removes this plugin's cvar callback. No asynchronous timers or background
threads retain the adapter. If NetcodePlugin is deliberately unloaded and
reloaded during play, unload and reload WizardRumble afterward.

Incoming frames have a bounded length, exact channel match and checked decimal
address parsing. The adapter compares the sender address with current match
PRIs and passes an existing matching wrapper to the application. It never
dereferences a packet-supplied address directly. This prevents accidental
invalid-pointer construction but is **not cryptographic authentication**.
The publicly available manager documents sender handling; NetcodePlugin's
underlying forwarding implementation was not available for a source audit.
The adapter trusts NetcodePlugin's sender metadata. Use this in games with
friends, with host-side validation of every gameplay request. A participant
with arbitrary native plugins already has substantial control of their own
client. Do not treat this protocol as an adversarial networking boundary.

## Primary references checked on 27 October 2026

- [NetcodePlugin installation, chat requirements and transport description](https://bakkesplugins.com/plugin/166)
- [NetcodeManager public interface and wire-format implementation](https://github.com/CinderBlocc/NetcodeManager)
- [BakkesMod multiplayer plugin documentation](https://bakkesplugins.com/wiki/bakkesmod-sdk/useful-info/multiplayer-plugins)
- [BakkesMod SDK CVarWrapper callback API](https://github.com/bakkesmodorg/BakkesModSDK/blob/master/include/bakkesmod/wrappers/CVarWrapper.h)
- [BakkesMod SDK ServerWrapper authority API](https://github.com/bakkesmodorg/BakkesModSDK/blob/master/include/bakkesmod/wrappers/GameEvent/ServerWrapper.h)
