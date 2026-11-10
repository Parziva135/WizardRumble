# Validation record — 0.9.2

Date: **10 November 2026** (Asia/Bangkok).

## Requested behavior

Accio now charges for **0.5 seconds**, then runs its existing controlled ball guidance for at most **three seconds**. Imperio now charges for **one second**, then mirrors the caster's controls on the original targeted opponent for **3.5 seconds**. Both use the existing host-authoritative charge path, preserving the caster/car and original target actor, rechecking range at release and retaining the held spell after an invalid charge. Old same-pair effects retire before the next charge to prevent their cancellation from erasing a new wind-up.

The supplied Accio and Imperio recordings play from charge acceptance, once per cast. Their full audio streams are retained without trimming, speed changes or fades. Successful release does not restart or truncate speech. Both have independent replay channels; snapshots stay silent and cancellation is scoped to the owned actors. All eleven spells now have charge behavior. R remains the cast key.

Patronus now has a filled, shaded model-derived body, separate windows, wheels and hubs, with reduced exterior mist. Its three-second charge, 1,700-unit/second flight, single 2,400-unit/second ball hit and model scale are unchanged. The previous release's soft additive Avada beam and glow PNGs are retained. The removed decorative charge circles, thin frozen-car outline, open circular Protego and bottom-left HUD remain.

## Automated gameplay and sound evidence

- `scripts/test.ps1`: **42,491 gameplay and protocol checks pass**, including all eleven charge configurations, target sentinels, bounded G/J durations, removed IDs, twelve overlapping replay channels, seeks, mute/cancel behavior and existing gameplay rules.
- `build/check_spells092_adapter.ps1`: **489 actual-source adapter assertions pass**. Native SDK I/O is replaced by recorded fake wrappers, while current production cast/update/cancel/input/scene/replay methods execute. The cases check no early ball write or mirrored input; exact 0.5/1-second release; three-second guidance and 3.5-second mirror after release; locked target despite changed aim; no inheritance by replacement ball/car; cancellation and retry without a goal; cooldown; G-only audio; silent J/P/E; replay offsets; and same-pair cleanup order. This is not a native match test.
- `build/check_audio092.ps1`: **35 production audio parser/replay checks pass** without opening an audio output device. Both new files decode successfully with their exact expected frame counts.
- Independent review of the audio mappings, twelve replay channels, ownership cleanup and packaging scripts found no blocking issue.

| New clip | Full duration | Format | Bytes | SHA256 |
| --- | --- | --- | --- | --- |
| `accio.wav` | 0.7666667 s | 48 kHz, mono, 16-bit PCM | 73,644 | `d9bcc85cf3ccdc87a20245bb1ab25019d0f22707418cb7bdb1b5d4685c1a034d` |
| `imperio.wav` | 1.7266667 s | 48 kHz, mono, 16-bit PCM | 165,804 | `07450669ccb191effbef8b7e4d0276621bf84c58fb4ad0e2aa471543807a1864` |

The new normalized WAVs measure approximately **−0.9 dBTP**, with **zero full-scale/clipped samples**. Both preserve the complete first audio stream, including audio beyond the gameplay charge deadline. `build/audio092-report.json` records extraction and analysis; the ten previously shipped WAVs are unchanged. The unused old `leviosa_loop.wav` is excluded from installation and both archives.

## Graphics, build and package evidence

- **55,921 production-renderer assertions pass**, using the current graphics source and real glow PNGs with fake native Canvas I/O. The crowded fixture remains inside the **2,200 world-primitive** limit, including **477 texture tiles**, and preserves **26 glow tiles** for a newly released red spell. Bounds, near-plane handling, repeated times, reverse seeks, snapshots, replay scene copies and missing-texture fallback are covered. These are correctness/work-budget checks, not GPU frame-rate measurements.
- **4,685 shell-geometry checks pass**, including finite bounds, normals, closed edges, winding and an exact deterministic rebake. The new source-profile shell contains **448 detailed / 304 coarse triangles**. The standard-library baker adds no runtime dependency. Header SHA256: `139063444e8dbc5e4e775592326c3562f2dea2cb0e2554567323f9a449ce245b`.
- `build/patronus092-comparison.png` and `build/patronus092-bright-dark.png` were regenerated and visually inspected after the final geometry correction. They use actual production faces and soft-light commands, without added bloom, on illustrative cameras/backgrounds. They are not Rocket League footage.
- The final full **MSVC x64 release build passes**, with only the installed SDK's existing unused `up_f` warning. Binary inspection confirms all **three plugin exports**, **144 SDK imports** and **zero unresolved SDK symbols**.

Final DLL: **604,672 bytes**, built **10 November 2026, 16:31:52**, SHA256 `d17d4ab2d9731cfc4c6477606341dfb22e83dae84341225a2f615fad0008e74b`. The final graphics source and shell header were stable before this build. Native appearance still needs the playtest below.

## Installation and sharing

The runtime comprises **16 files**: the x64 DLL, settings, twelve WAVs and two PNGs. The model geometry is compiled into the DLL. `scripts/release-files.ps1` supplies one manifest for installation and both ZIP layouts. The installer verifies every packaged runtime checksum before copying and verifies every copied file afterward.

`build/check_release092_install.ps1` passes against the final DLL in both source and player layouts: **all sixteen file hashes match**, unrelated plugins/startup configuration are preserved, a tampered Accio WAV is rejected before any installed file changes, and a player layout missing its checksum manifest is rejected. `build/installer092-report.json` records the results. Four release PowerShell scripts pass syntax checks.

The release layouts contain **60 entries** in `WizardRumble-0.9.2.zip` and **25 entries** in `WizardRumble-0.9.2-Windows.zip`, each including sixteen runtime checksums and the updated Meshnet instructions. `scripts/verify-package.ps1` checks exact entry allowlists, byte equality with current source, runtime hashes, required attribution and exclusion of recordings/GLB/SDK/compiler/build/downloads/cache files. The native-copy stage at `build/native-install092` has exactly sixteen runtime files, all matching their sources. The documentation refresh for GitHub does not change the DLL or runtime assets.

**Native installation completed** with Rocket League closed. Windows Explorer copied the stage's two folders into the live BakkesMod root. The observed dialog identified **22 items**, the correct source/destination and **12 existing filenames**; replacement finished without an error/lock dialog. Native DLL Properties confirms **604,672 bytes**, modified **10 November 2026, 16:31:52**, in the live `bakkesmod/bakkesmod/plugins` folder. Both new voice files and the other runtime clips were visible under `data/wizardrumble/audio`, and both glow PNGs were visible under `data/wizardrumble/textures`. The unused pre-existing Leviosa loop was preserved. Stage hashes and package hashes are verified; no native-file hash or in-game load/playback pass is claimed. Start the game, load `WizardRumble` and check `wr_status` for version 0.9.2 and `glow_textures=1`.

The user has authorized publication to the private `Parziva135/WizardRumble` GitHub repository. The Windows ZIP is intended for friends; the complete source ZIP includes the same runtime and development files. This validation record covers the build and installation, while the live GitHub release records publication. No collaborator invitation or router/VPN change is part of this upload. See [START_HERE.md](../START_HERE.md), [MESHNET.md](MESHNET.md) and [SHARING.md](SHARING.md).

## Live validation limits

No live Rocket League appearance, GPU blending/gamma, frame-rate, audible playback or two-PC/cross-country gameplay pass is claimed for this update. Off-game illustrations composite actual production Canvas commands and the real PNGs; they are not game footage. Canvas magic does not gain depth-correct world lighting or a native volumetric material by using glow textures.

The mod requires a **player-hosted LAN Soccar session**, with NetcodePlugin 166 on every PC and a working LAN/IP connection for remote friends. A normal online private server does not grant this plugin gameplay authority. Goal replay history stays local and is not embedded in exported replay files. The prior numeric-bot-ID ambiguity in remote cosmetic cancellation remains. Follow [PLAYTEST.md](PLAYTEST.md), beginning with the host's usual LAN match with bots, then two people on opposing teams before a full group session.
