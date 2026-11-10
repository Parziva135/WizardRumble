# Wizard Rumble 0.9.2 — play with friends

Windows PC only. Everyone needs the same **0.9.2** package. **R** casts your ready spell; aim your car toward an opponent for targeted spells.

Open the [private release download page](https://github.com/Parziva135/WizardRumble/releases/latest), expand **Assets** and download **WizardRumble-0.9.2-Windows.zip**. This is the ready-to-play package; you do not need a compiler. If the private repository or download shows 404, sign in with the GitHub account the owner invited and accept the invitation first.

## Install on every PC

1. Install [BakkesMod](https://bakkesmod.com/). Start Rocket League with its supported EAC-disabled launch choice and confirm F2 opens the BakkesMod menu. See [Epic's launch instructions](https://www.epicgames.com/help/c-37599050/c-42869540/a16737879).
2. In **F2 → Plugins → Plugin Manager**, use **Install by ID: 166** for [NetcodePlugin](https://bakkesplugins.com/plugin/166). Install [Rocket Plugin, ID 26](https://bakkesplugins.com/plugin/26) for hosting/joining by IP.
3. Close Rocket League before copying the mod. Extract **WizardRumble-0.9.2-Windows.zip**. In the BakkesMod injector choose **File → Open BakkesMod folder**. Copy the extracted `plugins` and `data` folders into that folder, merging folders and replacing this mod's files. Keep the folder structure.
4. Alternatively, open PowerShell in the extracted `WizardRumble` folder and run:

   ```powershell
   powershell -ExecutionPolicy Bypass -File .\scripts\install.ps1
   ```

   The installer verifies and copies **16 files**: DLL, settings, 12 WAVs and 2 PNGs. It preserves existing key binds and startup configuration. For a different BakkesMod folder, add `-BakkesModPath "C:\path\to\bakkesmod"`.
5. Start the game with BakkesMod, open **F6**, and enter:

   ```text
   plugin load WizardRumble
   wr_status
   ```

   Check that the version is **0.9.2**, audio has no load errors and `glow_textures=1`. The Octane model is included in the DLL; no GLB download is needed. After restarting the game, load WizardRumble again if it is not configured to load automatically.

The larger `WizardRumble-0.9.2.zip` includes source and build instructions. It also supports the same installer; its manual-copy paths are in its README.

## Join across countries

Wizard Rumble runs in a **player-hosted LAN Soccar match with Rumble set to None**. GitHub only distributes the files. A normal online private match uses a remote game server and does not give this plugin authority over its physics.

1. Set up **NordVPN Meshnet** on the host's PC and each friend's PC. Everyone signs in with their own Nord account; Meshnet is free, so friends do not need paid VPN subscriptions. The host links each friend using Meshnet invitations. Follow the [host-and-friend Meshnet steps](docs/MESHNET.md).
2. Leave Meshnet running. The host opens Rocket Plugin with **Home**, goes to **Multiplayer**, chooses a standard arena with **Soccar / Rumble None**, sets a lobby password and clicks **Host**. Friends open the same menu, enter the host's **Meshnet IP address** on the join side, use the same port/password and click **Join**. The usual Rocket Plugin port is **7777**. A standard arena needs no custom-map files.
3. Set Rocket League chat to **Allow All** on every PC and leave all participants unmuted. NetcodePlugin carries the spell messages through invisible chat.
4. In active play, run `wr_status` on every PC. The host should report `supported=1`, `host=1`, `active=1`; clients should report `supported=1`, `host=0`, `active=1` and receive their spell slot. Wait for the slot, then press R. If the match itself cannot be joined, fix the LAN/IP connection before troubleshooting spells.

Start with two people on opposite teams. Check that both see the same Avada target, both hear Accio/Imperio at charge start, and the target is affected only after its charge. Cross-country latency can affect timing and control feel; this release still needs a native two-PC test with your group.

Meshnet connects the computers; Rocket Plugin starts the match; Wizard Rumble supplies the spells. The host's PC must remain on and in the match. GitHub provides the download only. Connecting everyone to an ordinary VPN country/server does not link their PCs for this setup. Meshnet is the proposed connection method, based on its documented LAN capability; this specific mod has not yet had a two-PC Meshnet test. [NordVPN Meshnet](https://nordvpn.com/meshnet/), [Rocket Plugin hosting guide](https://bakkesplugins.com/plugin/26).

## Quick fixes

- **F2/F6 do nothing:** first check BakkesMod injection/compatibility.
- **Unable to find file:** verify `plugins/WizardRumble.dll` in the live BakkesMod folder; copying only the ZIP does not install it.
- **Waiting for host:** confirm LAN match, matching mod versions, NetcodePlugin on every PC, Allow All chat and no muted players.
- **Thin effects:** check both PNGs in `data/wizardrumble/textures`, then run `wr_glow_reload`.
- **Missing sounds:** check all 12 WAVs in `data/wizardrumble/audio`, then run `wr_sfx 1`, `wr_sfx_volume 1` and `wr_audio_reload`.
- **Unload:** `plugin unload WizardRumble`. Close the game before updating the DLL.

Accio charges **0.5 seconds**, then guides for at most three seconds. Imperio charges **1 second**, then mirrors controls for 3.5 seconds. Patronus has a fuller luminous car body; its existing three-second charge, speed and single-hit strength are unchanged.

Keep [model attribution](docs/MODEL_CREDITS.md) and [third-party notices](docs/THIRD_PARTY.md) with the files.
