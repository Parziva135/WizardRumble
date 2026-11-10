# Wizard Rumble 0.9.2

A Harry Potter inspired spell mode for Rocket League on **Windows PC**. A compact Rumble-style spell slot shows your next-spell countdown, then reveals the spell you can cast. Each successful cast starts a new random-spell cooldown, normally 10 seconds.

**Players: open the [release download page](https://github.com/Parziva135/WizardRumble/releases/latest), download `WizardRumble-0.9.2-Windows.zip` under Assets, then follow [START_HERE.md](START_HERE.md).** It contains the compiled mod, sounds, glow textures and installer. For friends connecting through NordVPN, follow the [Meshnet host/join guide](docs/MESHNET.md). Everyone needs their own installed copy. Private-repository downloads require an accepted GitHub invitation and sign-in.

Version 0.9.2 adds the supplied **Accio and Imperio voices**, with **0.5-second Accio** and **one-second Imperio** charges. Expecto Patronum gains a fuller luminous Octane body with less surrounding mist; its flight speed and single-hit strength are unchanged. The soft additive glow pass, clean green Avada charge, open Protego circle and bottom-left HUD remain. See [VISUALS.md](docs/VISUALS.md) for rendering limits and [VALIDATION.md](docs/VALIDATION.md) for checked and pending results. [SHARING.md](docs/SHARING.md) explains private release access and distribution.

Goal replay effects use the last 30 seconds recorded by the plugin on that PC. The plugin must be loaded during the live play; these extra effects are not saved inside exported Rocket League replay files. Playback smooths matching recorded poses and animation timing between captured frames, while keeping each spell's start, impact and cancellation at its recorded event. It does not blend across a missing interval, replacement actor or different Patronum flight. Goal replay playback shows world effects and replays recorded cast/lift sounds without recasting gameplay spells or covering the replay camera with victim slugs. Audio respects `wr_sfx` and `wr_sfx_volume`, even with `wr_vfx 0`. Sounds trigger from recorded events, not every rendered frame; joining a clip partway through or seeking uses the corresponding offset instead of repeating its opening on every frame. Slow motion and backward timeline reads use the recorded visual state. Native replay timing and smoothness are pending an in-game playtest.

Use **Soccar with the Rumble mutator set to None**. This plugin supplies its own spell dispenser and HUD; it does not replace native Rumble assets or sounds. Multiplayer requires a **player-hosted LAN match**, including a LAN/VPN connection for remote friends. Ordinary online private matches use a remote Rocket League server and cannot apply this mode's gameplay changes.

## Update or install

Use the same **0.9.2 DLL, all twelve WAV files and both glow PNGs on every PC**. The runtime installation contains sixteen files: DLL, settings, twelve WAVs and two textures. Bombarda Maxima replaces Depulso at spell ID 4; Expecto Patronum uses spell ID 15, Petrificus Totalus keeps ID 9, and Eat Slugs keeps ID 14. Protego charges for one second against the original ball actor; Petrificus Totalus charges for 1.5 seconds, Stupefy for 650 milliseconds, Eat Slugs for 1.5 seconds, and Bombarda Maxima/Avada/Leviosa for two seconds. Expecto Patronum charges against the original ball for three seconds before its flight begins. Obliviate charges for 0.5 seconds; Accio charges for 0.5 seconds against the original ball; Imperio charges for one second against the locked opponent. Charge packets validate each spell's own duration limit. Obliviate replaces Confundo at spell ID 13; Imperio uses ID 16; removed spell IDs stay reserved and are rejected. W2 state includes the host's current spell range for casting previews. Legacy seven-token state messages without that field retain the historical **4,500-unit** interpretation, not the new default. The 0.1.0 protocol is incompatible.

1. In Rocket League's **F6** console, unload an older copy before replacing it:

   ```text
   plugin unload WizardRumble
   ```

   Closing Rocket League before updating also works.
2. Extract `WizardRumble-0.9.2.zip` (the full source package). For the ready-to-copy Windows ZIP, follow [START_HERE.md](START_HERE.md) instead. In the BakkesMod injector, choose **File → Open BakkesMod folder**.
3. Using **Windows Explorer**, copy these files into that live BakkesMod folder, creating the audio and texture subfolders if needed:

   | Release file | Destination inside BakkesMod |
   | --- | --- |
   | `dist/WizardRumble.dll` | `plugins/WizardRumble.dll` |
   | `plugins/settings/wizardrumble.set` | `plugins/settings/wizardrumble.set` |
   | `assets/audio/accio.wav` | `data/wizardrumble/audio/accio.wav` |
   | `assets/audio/imperio.wav` | `data/wizardrumble/audio/imperio.wav` |
   | `assets/audio/avada_kedavra.wav` | `data/wizardrumble/audio/avada_kedavra.wav` |
   | `assets/audio/leviosa_cast.wav` | `data/wizardrumble/audio/leviosa_cast.wav` |
   | `assets/audio/leviosa_float.wav` | `data/wizardrumble/audio/leviosa_float.wav` |
   | `assets/audio/eat_slugs.wav` | `data/wizardrumble/audio/eat_slugs.wav` |
   | `assets/audio/stupefy.wav` | `data/wizardrumble/audio/stupefy.wav` |
   | `assets/audio/petrificus_totalus.wav` | `data/wizardrumble/audio/petrificus_totalus.wav` |
   | `assets/audio/protego.wav` | `data/wizardrumble/audio/protego.wav` |
   | `assets/audio/expecto_patronum.wav` | `data/wizardrumble/audio/expecto_patronum.wav` |
   | `assets/audio/bombarda_maxima.wav` | `data/wizardrumble/audio/bombarda_maxima.wav` |
   | `assets/audio/obliviate.wav` | `data/wizardrumble/audio/obliviate.wav` |
   | `assets/textures/glow_soft.png` | `data/wizardrumble/textures/glow_soft.png` |
   | `assets/textures/glow_mist.png` | `data/wizardrumble/textures/glow_mist.png` |

   Verify all sixteen files are visible there. Installing or reinstalling BakkesMod alone does not install Wizard Rumble's DLL, clips or glow textures.
4. With BakkesMod working in the EAC-disabled game, run in F6:

   ```text
   plugin load WizardRumble
   ```

The usual BakkesMod folder is `%APPDATA%\bakkesmod\bakkesmod`. An optional installation script is included; run it yourself from PowerShell in the extracted `WizardRumble` folder:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\install.ps1
```

The script copies DLL, settings, twelve WAVs and two PNGs, verifies all sixteen copied files' SHA256 values, and preserves existing key binds and startup settings. The release's `SHA256SUMS.txt` records all sixteen runtime checksums, including settings. For a custom BakkesMod location, pass `-BakkesModPath "C:\your\bakkesmod"`. Always verify the files in the live folder through Explorer if the game says a file is missing. `wr_status` should report `glow_textures=1`; after correcting either PNG, run `wr_glow_reload` to load the pair again.

## Launch Rocket League for mods

Exit Rocket League, start [BakkesMod](https://bakkesmod.com/), and check its compatibility status. In **Epic Games Launcher → Library → Rocket League → three dots**, select **Launch without EAC**. Epic documents **Manage → Launch options → `-noeac`** as a fallback; remove the option when returning to normal online play. Steam offers an EAC-disabled launch choice. These are [Epic's supported launch instructions](https://www.epicgames.com/help/c-37599050/c-42869540/a16737879).

Wait for BakkesMod to report injection and confirm **F2 opens its menu** before loading Wizard Rumble. F6 opens the BakkesMod command console. If F2/F6 do nothing, resolve BakkesMod's loading or compatibility status first.

## Try the spells

Enter **Freeplay**, load the plugin, and wait for the spell slot to charge. **R** casts. The plugin assigns R while it is loaded and restores the previous BakkesMod R assignment on unload if R still points to `wr_cast`. An old Q assignment is removed only when it points exactly to `wr_cast`; unrelated Q assignments are preserved. Optional controller bind: `bind XboxTypeS_RightThumbstick "wr_cast"`; this changes right-stick click and is never installed automatically. `wr_cast` can also be entered directly in the console.

For quick testing, use `wr_cooldown 1` before starting a new cooldown. Restore `wr_cooldown 10` afterward. In solo Freeplay, `wr_practice accio` grants a chosen spell; substitute an identifier from the table below, such as `protego` or `bombarda_maxima`. This practice command is restricted to Freeplay. Use `wr_practice expecto_patronum` to try the new summon, or `wr_practice petrificus_totalus` for the renamed bind. The old `arresto_momentum` and `confundo` diagnostic spellings remain compatibility aliases for Petrificus Totalus and Obliviate. Ball spells and Protego work alone; ordinary opponent spells require another eligible car. You keep a spell when no valid target exists. Removed identifiers such as `glacius`, `descendo` and `expelliarmus` are rejected.

To check timed effects alone, `wr_selftest leviosa`, `wr_selftest petrificus_totalus` and `wr_selftest obliviate` apply the named effect directly to your own car in **local Freeplay only**. `wr_selftest eat_slugs` previews the victim's camera obstruction there. Close F6 after entering the command. The direct Leviosa, Petrificus and Eat Slugs diagnostics bypass normal targeting and charge. Normal R casting charges for two seconds for Leviosa, 1.5 seconds for Petrificus and 1.5 seconds for Eat Slugs. For the lift diagnostic, run `wr_selftest leviosa` and leave the controls idle: your car should keep rising at 700 units/second, release at roof contact and fall without a goal. An eight-second safety deadline releases it on maps without a detected roof. The console then prints `Leviosa live probe: PASS` or `CHECK` with measured rise/release/fall information. That result describes that local run; multiplayer still needs testing with a friend.

For the user's **LAN match with bots** setup, use `wr_bot_test leviosa` or `wr_bot_test petrificus_totalus`, or the matching buttons in **F2 → Plugins → Wizard Rumble**. These commands require an active, enabled LAN match hosted by the sole human player. They select an opposing bot directly and refuse matches containing another human. They enable `wr_debug 1` for periodic actual-position and release-reason logs. Close F6, watch the selected bot, then inspect the probe and `wr_status` roster output. Leviosa should keep rising until roof contact, then release and fall; Petrificus should keep its original pose for two seconds after its normal 1.5-second charge, then release. The direct bot hold diagnostic bypasses that charge. A printed test start is not a passing result.

Aim your **car's nose** toward an opponent for targeted spells. The host chooses the opposing car closest to your aim within a roughly 35-degree cone and **5,500 Unreal units by default**. Your camera direction does not choose the target. Allies, spectators, demolished cars and your own car are excluded. Ball spells require the ball to be within range.

On the first Freeplay or LAN session after upgrading, a one-time update changes a saved range equal to the old 4,500 default to 5,500. Existing custom nondefault ranges are preserved. The host still owns this setting and shares its current value with clients; crosshairs and actual casts use that host value. `wr_status` prints the effective range and migration revision for troubleshooting.

When a spell is ready, a spell-colored crosshair marks its current on-screen target within the host's range setting. Opponent spells mark the selected car; Accio, Bombarda Maxima, Protego and Expecto Patronum mark the ball. Avada has its own green reticle. For Avada, Leviosa, Stupefy, Petrificus, Obliviate or Eat Slugs, press R once to lock the opponent and start its charge; holding R is unnecessary. Avada/Leviosa take two seconds, Petrificus 1.5 seconds, Eat Slugs 1.5 seconds Stupefy 0.65 seconds and Obliviate 0.5 seconds. Imperio locks the selected opponent for a one-second charge; its mirrored controls begin only at successful release. Accio locks the original ball for a half-second charge before guidance begins. Bombarda Maxima locks the original ball for a two-second charge before blasting it away from the caster at release. Expecto Patronum locks the original ball for a three-second charge, playing the first three seconds of the supplied clip before its Octane launches. Protego locks the current ball for a one-second charge; it needs no opposing car. Its shield is placed only at successful release, using that ball's current movement. The HUD countdown and target indicator remain, while decorative world charge circles are removed. The host revalidates the original actor and range before applying the spell. Invalidation while active play continues keeps the unconsumed spell. Repeated R does not restart the charge; pause, goal and disabling the mode reset the dispenser.

Accio follows a point about 260 units in front of your car's horizontal nose direction, updating as you drive or turn. It eases the ball's velocity toward that point rather than teleporting it. Guidance lasts at most **three active seconds**. After staying close with little relative motion for 0.35 seconds, it can release earlier and normal ball physics resume so you can take control. A touch, Bombarda Maxima, Protego collision, replacement ball/car or session cleanup also stops guidance.

## Play with friends

Every player needs BakkesMod, the same Wizard Rumble 0.9.2 DLL, all twelve bundled WAVs, both glow PNGs installed locally, and [NetcodePlugin, ID 166](https://bakkesplugins.com/plugin/166). Install NetcodePlugin through **F2 → Plugins → Plugin Manager → Install by ID: 166**. Set chat to **Allow All**, and leave the other participants unmuted; the dependency transports invisible chat messages.

For remote friends, use the [NordVPN Meshnet setup](docs/MESHNET.md) to link each friend's PC to the host. Meshnet is free for friends with their own Nord accounts. The host creates a **LAN Soccar match with Rumble None** through [Rocket Plugin](https://bakkesplugins.com/plugin/26); friends use its IP-join menu with the host's Meshnet address. Wizard Rumble supplies spells after the match is connected. This specific mod still needs its native two-PC Meshnet playtest.

The host deals spells, validates casts, controls timed effects and applies ball/car physics. Other players receive the spell state and effect events. Solo Freeplay does not need NetcodePlugin. `wr_status` prints match, authority and synchronization diagnostics in F6.

## The 11 spells

Values below are defaults. Velocities and distances use Rocket League's Unreal units.

| Spell / practice identifier | Gameplay effect | Visual treatment |
| --- | --- | --- |
| Accio — `accio` | Charge for **0.5 seconds**, then guide the ball in front of the moving caster for at most **three seconds**, settle, then release for control. Speed is capped at 6,000 units/second and acceleration at 18,000 units/second². | Tapered cyan/silver ribbons, inward-moving comets and light crescents around the ball; no charging seal. |
| Avada Kedavra — `avada_kedavra` | Lock an opponent, charge for two seconds, then demolish that same valid target. Cooldown begins on success; active-play invalidation keeps the spell. | Clean car-only green charge glow; release layers a soft luminous green envelope and mist around a bright branching core, ending in a pale-hot green impact at the captured hit point and brief upper-view tint. |
| Wingardium Leviosa — `leviosa` | Charge for two seconds, then raise the target continuously at 700 units/second until roof contact, release it and let it fall. An eight-second safety deadline releases it if no roof is detected. | Open violet/silver updraft ribbons and feather-shaped glints follow the rising car; a narrower flowing ribbon delivers the cast. |
| Bombarda Maxima — `bombarda_maxima` | Lock the original ball, charge for **two seconds** with the full supplied recording, then launch it at **12,000 units/second** with an upward component. Valid native tuning boosts ricochets for **five seconds**; unavailable fields retain the engine's limit. Original owned tuning is restored on cleanup. | A hot amber lance, broken pressure fronts and two-tone ejecta lead into curved flame tongues and bright surface streaks on the ball. |
| Protego — `protego` | Charge for one second with the supplied voice, then place a fixed circular ball barrier of radius 350 units, 450 units ahead of the current ball travel, for 3 seconds. Cars pass through. | Fine mint/gold calligraphic runes, travelling rim light and impact arcs frame a geometrically clear center; the HUD glyph is circular too. |
| Stupefy — `stupefy` | Lock an opponent, charge for 0.65 seconds, then knock that same valid car away. | A crisp white-hot scarlet hit, smoothly evolving red lightning and directed pressure crescents replace the jumping bolt pattern. |
| Petrificus Totalus — `petrificus_totalus` | Charge for 1.5 seconds with the supplied voice, then hold that same valid opponent at its cast position and rotation for 2 seconds before release. | A smooth ice-blue lance and forked crystalline splinters lead into the thin light-blue car contour with moving frost highlights. |
| Obliviate — `obliviate` | Charge for **0.5 seconds**, then reverse the same valid target's directional controls for **5 seconds**, including W/S, A/D, controller driving, aerial and dodge axes. Jump, boost and handbrake retain their functions. | A bright golden spear, sharp contact light and drifting memory wisps give the yellow laser a clearer cast/impact/status sequence. |
| Eat Slugs — `eat_slugs` | Charge for **1.5 seconds**, launch at the same valid opponent, then obstruct its camera for **3.75 seconds** after the **0.35-second** projectile flight. Controls keep their normal functions. | Shaded dark-green globs have wet highlights and mucus tails; camera slugs have organic skin, eye stalks and slime traces as they slide away. |
| Expecto Patronum — `expecto_patronum` | Lock the original ball and charge for **three seconds**, then launch a luminous Octane that chases at up to **1,700 units/second** for **2.5 seconds** and strikes at most once at **2,400 units/second** toward the opposing goal. A fast/distant ball can escape. | A filled model-derived silver/blue car shell supports the soft luminous surface samples. Darker glazing and wheels keep the larger-looking body readable, with restrained haze and contact glow. |
| Imperio — `imperio` | Charge for **one second**, then make the targeted opponent mirror the caster's seven directional axes and jump/boost/handbrake controls for **3.5 seconds**. Copies control actions rather than position; the caster can still drive normally. A caster already under Imperio cannot start another Imperio link. | A restrained violet/gold control thread with directional light packets and small end marks identifies the exact caster/victim link. The supplied Imperio voice starts once at charge acceptance. |

Protego's existing wall keeps working while that caster charges another Protego. A canceled wind-up keeps the ready spell and the older wall; the dedicated charge-cancel event cannot remove that wall. Only a successful one-second release replaces it. Each caster has at most one physical barrier. The wall stays at its cast location rather than following the ball; near-zero ball velocity falls back to the caster's forward direction. Its host-side collision reflects normal ball velocity with 0.95 restitution, and it does not protect a car from hostile spells.

Leviosa and Petrificus Totalus identify their target by the actual current PRI/car pair, avoiding hold cancellation when bot roster IDs are reassigned. Petrificus uses SDK pose/velocity setters for its two-second hold. Leviosa uses native upward velocity rather than teleporting its position, allowing stadium collision to stop it; a current rigid-body contact above the car triggers release and gravity resumes. An eight-second active-time safety deadline handles open-roof or missing-contact cases. A match-tick hook runs alongside input and drawing callbacks. These implementation changes require live bot checks. Obliviate changes directional input on the authoritative simulation; victim-client prediction and steering feel still need the two-PC checks in the playtest guide.

Leviosa does not start another charge on a target still held by your own active Leviosa; your ready spell is kept. This prevents a short cooldown from stacking a new same-pair charge onto the old lift. Another opponent may still be targeted. Eat Slugs likewise keeps the ready spell instead of starting a same-caster/same-victim charge while your earlier Eat Slugs obstruction remains active.

The spell slot, glyphs and effect placement are procedural. Two original 128×128 PNG masks supply soft light and mist, loaded once through the SDK's Canvas image path. Their RGB and alpha fall to zero at every border, avoiding a rectangular backdrop even when additive blending does not use texture alpha. Patronum uses 176 luminous surface samples and a sparse set of contours derived from the supplied Octane GLB, compiled into the DLL; no separate model file is needed. Its gameplay size remains 50% larger than in 0.7.2. The raw GLB and its original textures are not distributed. See [model credit and license](docs/MODEL_CREDITS.md). These are projected Canvas sprites anchored to game positions, with no guaranteed native volumetric material, depth-buffer occlusion or HDR bloom. Some effects can remain visible through stadium scenery. Protego's ball collision is calculated separately on the host.

If either glow image is missing or cannot load, the mode uses its retained line/contour presentation. This fallback preserves a visible spell but will look thinner than the textured path. Run `wr_status` and inspect `glow_textures`; verify both PNGs in `data/wizardrumble/textures`, then use `wr_glow_reload`. Textures are not reloaded per frame, so reloading them may briefly stutter and should be done outside a cast.

Patronum is a temporary host-controlled spell, rather than another player or permanent AI car. The host detects its swept contact with the current ball and applies at most one impulse toward a goal belonging to the opposing team. It does not guarantee a score: a car, ward, goalkeeper or stadium collision can still change the shot. Resetting the ball, replacing the caster, leaving active play or reaching the flight timeout cancels any pending hit. A successful strike ends the chase and fades the ghost for about 0.45 seconds. If your previous Patronum is still chasing, another Patronum charge keeps the ready spell instead of starting; this prevents the old flight's cancellation from interrupting a newer charge. The completed ghost's short visual fade can overlap a new charge.

Casting visuals follow host gameplay events. Decorative world charging circles, caster ground seals and generic orbit rings are removed; functional HUD countdown/target marks, actual circular Protego walls and impact feedback remain. Avada's charge is a clean green body glow on the exact locked car, with no surrounding charge cloud. Its pose-fitted soft light does not replace the car's native emissive material or game lighting, and no demo occurs before the deadline. The impact stays at the captured hit point rather than following a respawn. World VFX use bounded projected geometry and soft sprites; Eat Slugs blobs and camera obstruction deliberately use filled organic shapes. Avada's brief atmosphere treatment is a camera-aware upper-view tint, not a map sky-shader change. The shared world drawing budget prioritizes physical wards, held-car feedback and recent casts; native blending, brightness, occlusion and performance require an in-game check.

The Avada, Leviosa, Eat Slugs, Stupefy, Petrificus, Protego, Bombarda, Patronum and Obliviate voices play locally for fresh accepted charges. Leviosa plays its replacement two-second cast voice once, then starts `leviosa_float.wav` once when the target begins rising. It does not loop or restart during the hold. Stupefy starts its supplied voice when the 0.65-second charge is accepted; the red impact and knockback happen only after successful completion. Petrificus starts its supplied 1.5-second voice on its charge, with no early body bind or second voice at impact. Protego starts its supplied voice once on its one-second charge; the natural clip may finish after the wall appears, without playing again at release. The floating clip is a three-second one-shot and may finish before a longer Leviosa ascent reaches the roof. Late-join snapshots do not replay any of these sounds. Bombarda's full 5.31-second recording starts at its two-second charge and continues naturally after release, with no second playback or cut at the two-second impact. The entire source audio stream is included without trimming or changing playback speed. Patronum uses exactly the supplied recording's first **0–3.000 seconds** at natural speed. Its voice starts once on the three-second charge; the Octane launches only after successful release, without replaying the voice at E. Silent snapshots do not restart speech or add another replay cue. Separate local buffers keep all twelve clips independent. Accio and Imperio play their complete supplied clips once at charge acceptance; release does not repeat the voice, and the recording can continue beyond the wind-up. These sounds do not change Windows or Rocket League master volume. `wr_sfx 0` disables spell sounds; `wr_sfx_volume` sets local volume from 0 to 1, default 1. `wr_audio_reload` reloads all twelve installed WAVs. Obliviate starts its supplied voice once on its half-second charge; its yellow laser and control reversal begin only on release, without a second voice. Optional local checks are `wr_audio_test` for Avada, `wr_audio_test leviosa`, `wr_audio_test leviosa_float`, `wr_audio_test eat_slugs`, `wr_audio_test stupefy`, `wr_audio_test petrificus_totalus`, `wr_audio_test protego`, `wr_audio_test expecto_patronum`, `wr_audio_test bombarda_maxima`, `wr_audio_test obliviate`, `wr_audio_test accio` and `wr_audio_test imperio`. The Eat Slugs clip may continue naturally through release and projectile flight rather than being cut or compressed to the wind-up length. An old `leviosa_loop.wav` left by a previous installation is unused and does not need to be copied into this release.

Eat Slugs launches only after its 1.5-second charge succeeds and consumes the spell at launch. Its 0.35-second flight precedes the full 3.75-active-second camera obstruction and slide-away phase, for about **5.60 seconds from R to clearance**. The exact victim PRI/car pair is retained throughout. A replacement/demolished or otherwise invalid in-flight target cancels the spent projectile rather than transferring it to another car. Hiding HUD or cosmetic world VFX cannot remove the victim obstruction. Snapshots join the remaining flight/obstruction phase instead of restarting it.

Imperio is host-authoritative for all affected cars; a Client also applies it only to its own local victim for prediction. It reads the caster car's native input, sanitizes analog values, then copies Throttle, Steer, Pitch, Yaw, Roll, DodgeForward and DodgeStrafe plus Handbrake, Jump, ActivateBoost, HoldingBoost and Jumped. No continuous input is sent over the spell chat transport. Holds take precedence over mirrored input, and mirrored input takes precedence over the victim's own Obliviate inversion. Actor replacement, team/spectator changes, expiry and round/session cleanup end the link. Native input replication, client prediction and the resulting feel need two-PC testing.

Bombarda applies its launch even if optional native speed/bounce metadata is unset or invalid. Valid fields receive temporary speed/bounce tuning for five seconds, including a bounce scale of at least 1.8; unavailable fields are skipped and the engine's existing speed limit can still apply. Stadium collision determines subsequent bounce directions. Cleanup restores only tuning actually applied and still owned on the same ball. Another caster's Bombarda replaces the old buff; competing Accio/Patronum guidance cancels it. Your own active ricochet blocks a new Bombarda charge while keeping the ready spell. Use `wr_debug 1` to record the original and applied native values in a `Bombarda launch` diagnostic. A successful release starts the next-spell cooldown without requiring a goal.

## Settings

Open **F2 → Plugins → Wizard Rumble**, or use F6 commands. Gameplay settings belong to the host; HUD and sound preferences are local.

| Command | Default | Purpose |
| --- | --- | --- |
| `wr_enabled` | `1` | Enable the mode; `0` clears temporary gameplay effects. |
| `wr_cooldown` | `10` | New cooldown duration, from 1 to 60 seconds. Existing deadlines keep their value. |
| `wr_range` | `5500` | Host spell range, from 500 to 10,000 units; custom nondefault values are preserved on update. |
| `wr_hud` | `1` | Show or hide the local spell HUD; does not hide an Eat Slugs obstruction. |
| `wr_hud_scale` | `1` | Local spell-slot scale, from 0.7 to 1.6. |
| `wr_vfx` | `1` | Show or hide cosmetic world spell animations; Eat Slugs still obstructs its victim. |
| `wr_glow_reload` | — | Reload `glow_soft.png` and `glow_mist.png` after correcting their installed files. `wr_status` reports `glow_textures`. |
| `wr_sfx` | `1` | Enable local spell sound effects. |
| `wr_sfx_volume` | `1` | Local spell sound volume, from 0 to 1; does not alter system or game volume. |
| `wr_debug` | `0` | Log timed hold positions and release reasons; enabled by LAN-bot tests. |

A goal, round transition, pause or disabling the mode clears temporary effects. A new active round begins with fresh charging. Spells can repeat.

## Troubleshooting

- **"Unable to find file" when loading:** verify `WizardRumble.dll` is visible in the live BakkesMod `plugins` folder in Windows Explorer. Copy it there again from the release if missing. Keep the `.dll` filename; do not put only the ZIP there.
- **F2/F6 do nothing:** BakkesMod has not been verified as loaded. Use the supported EAC-disabled launch choice and check the injector's exact status before troubleshooting the spell plugin.
- **Waiting for the host / missing effects on one PC:** run `wr_status` on both PCs. Confirm every player uses 0.9.2, both plugins are loaded, the match is LAN, chat allows all players and nobody is muted. Allow a few seconds for synchronization after joining.
- **Standard Rumble items also appear:** restart the LAN match as Soccar with Rumble set to None.
- **R does nothing:** test `wr_cast` in F6, then reload Wizard Rumble to reinstall its R assignment. The spell still needs to be ready and have a valid target.
- **No target:** aim the car's nose toward an opposing car within range.
- **Avada starts but does not demo:** the two-second charge must finish with the same live opposing car in range. Invalidation during active play keeps the spell, so aim and cast again. A round, pause or enable-state reset starts a fresh cooldown instead.
- **Missing spell audio:** verify all twelve WAV files in `data/wizardrumble/audio`, enable `wr_sfx 1`, set a local volume above zero and run `wr_audio_reload`. `wr_status` reports readiness and load errors. The optional `wr_audio_test`, `wr_audio_test leviosa`, `wr_audio_test leviosa_float`, `wr_audio_test eat_slugs`, `wr_audio_test stupefy`, `wr_audio_test petrificus_totalus`, `wr_audio_test protego`, `wr_audio_test expecto_patronum` and `wr_audio_test bombarda_maxima`, `wr_audio_test obliviate`, `wr_audio_test accio` and `wr_audio_test imperio` check the installed assets without casting.
- **Avada/Patronum still look like thin lines:** confirm `glow_textures=1` in `wr_status`. Install both PNGs in `data/wizardrumble/textures` and run `wr_glow_reload`; read the texture-load error if readiness stays zero. The same PNGs are required on every PC, including a replay viewer.
- **Leviosa starts but does not lift:** normal R casting has a two-second charge before continuous rise to roof contact. The car should drop when it reaches the roof, or by the eight-second safety deadline if no roof is detected. The same live opposing car must remain in range. Use the direct Freeplay or LAN-bot hold diagnostic separately from a full charge test.
- **Slugs appear on the wrong view or remain after release:** record caster/victim IDs, current cars and remaining duration on both PCs. Only the current victim may receive the overlay; cosmetics toggles do not remove it. Its final slide must clear by 3.75 active seconds.
- **A bot hold ends early or does not move:** use the LAN-bot test buttons and capture `wr_status`, the actual-position log lines and the release reason. Roster IDs alone do not identify the held car in this build.
- **Protego looked opaque in an older build:** this build does not draw a filled shield surface. Its open rings and runes remain see-through even if the Canvas backend ignores primitive alpha.
- **Reloading NetcodePlugin:** unload Wizard Rumble first, reload the dependency, then reload Wizard Rumble.
- **Updating a DLL in use:** unload Wizard Rumble or close Rocket League before replacing the file.

To stop the mod, enter `plugin unload WizardRumble`. The host cleans up its temporary gameplay effects before releasing state. The previous BakkesMod R assignment is restored only if R still points to `wr_cast`, so a later manual reassignment is preserved.

## Build and validation

Source needs an x64 MSVC C++17 toolchain and the BakkesMod SDK. Open an **x64 Developer PowerShell for Visual Studio 2022** with C++ Desktop tools, then run:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1 -RunTests
```

The scripts also recognize a local portable toolchain under `.tools/msvc`. The SDK defaults to `%APPDATA%\bakkesmod\bakkesmod\bakkesmodsdk`; pass `-SdkPath "C:\path\to\bakkesmodsdk"` to change it. The DLL is written to `dist/WizardRumble.dll`. The release includes the source but excludes Microsoft's compiler and the SDK.

The generated `include/PatronumMesh.h` is already included, so building or installing does not need the original GLB. To regenerate its bounded open edges from the supplied source, run `python scripts/bake-patronum-model.py "C:\path\to\octane_-_rocket_league_car.glb"` before compiling. Keep the [model attribution](docs/MODEL_CREDITS.md) with derived source and binary distributions.

Version 0.9.2's automated and live validation results are recorded separately in [VALIDATION.md](docs/VALIDATION.md). See [PLAYTEST.md](docs/PLAYTEST.md) for charged Protego and old-wall preservation, Avada glow/branches/impact, removed charge circles, slower Patronum flight, retained roof release, timing/audio and synchronization checks. A compiled DLL and passing rule tests alone do not prove live replication, native audio playback or the appearance of every effect.

Keep the [third-party notices](docs/THIRD_PARTY.md) with any shared package. For transport details and references, see [LAN networking notes](docs/NETCODE.md). This is an unofficial fan project. Bundled audio is extracted from the user's supplied recordings; spell graphics use procedural effects and the credited derived Octane mesh.
