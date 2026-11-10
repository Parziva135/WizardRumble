# Sharing Wizard Rumble 0.9.2

Get the player download from [Parziva135/WizardRumble releases](https://github.com/Parziva135/WizardRumble/releases/latest). The repository is private, so each friend needs a GitHub account with an accepted repository invitation.

## Which file friends should download

| File | Purpose |
| --- | --- |
| `WizardRumble-0.9.2-Windows.zip` | Ready-to-play download: `plugins`/`data` folders, installer, quickstart, Meshnet instructions, credits and 16 runtime SHA256 checksums. No compiler needed. |
| `WizardRumble-0.9.2.zip` | Complete source/build package with the same compiled DLL, sounds, textures, tests and documentation. |

Download the **Windows ZIP** under the release's **Assets** section. The automatically generated GitHub source archives are intended for browsing/building source. Extract the download, open its `WizardRumble` folder and follow [START_HERE.md](../START_HERE.md).

Every PC needs its own installed copy; joining the host does not download the mod. The Octane geometry is included in the DLL. Raw recordings, the original GLB, SDK/compiler, local build experiments and downloads are excluded. Keep the [model attribution](MODEL_CREDITS.md) and [third-party notices](THIRD_PARTY.md) with shared copies.

## Give friends access

The repository owner can invite friends through **Settings → Collaborators** on [the repository](https://github.com/Parziva135/WizardRumble). Each friend accepts the invitation and signs into that GitHub account before opening the release link. A private link alone does not grant access. A 404 can mean the invitation is pending or the wrong account is signed in. [GitHub's access documentation](https://docs.github.com/en/repositories/creating-and-managing-repositories/access-to-repositories).

GitHub personal repositories have public/private visibility, without an unlisted setting. This release uses private sharing with invited accounts. [Repository visibility](https://docs.github.com/en/repositories/creating-and-managing-repositories/about-repositories).

## Play from Australia or another country

Use the [Meshnet host/join guide](MESHNET.md). NordVPN Meshnet links the host's PC with each friend's PC. Friends can use free Nord accounts. The host then starts the **LAN Soccar** match in **Rocket Plugin**, with **Rumble None**; friends join the host's **Meshnet IP address**.

Every player still needs BakkesMod, Wizard Rumble 0.9.2 and NetcodePlugin 166. Set chat to **Allow All** and leave the other players unmuted, as required by the [NetcodePlugin author](https://bakkesplugins.com/plugin/166). GitHub distributes the files, while the host PC runs the match.

A normal Rocket League online private match does not give this mod authority to change ball/car physics. Establish a connected player-hosted LAN lobby first. Native two-PC Meshnet replication and cross-country latency still need testing with the group; begin with one friend on the opposite team and check casting, a goal replay, respawning and reconnecting.

An existing working virtual LAN can also be used. [Rocket Plugin](https://bakkesplugins.com/plugin/26) documents virtual-network hosting and direct connections using port 7777. Those are alternative connection arrangements; the mod's installer does not configure a router or VPN.

## Release notes

Wizard Rumble 0.9.2 adds the supplied Accio and Imperio sounds. Accio charges for 0.5 seconds before its existing three-second guidance; Imperio charges for one second before 3.5 seconds of mirrored controls. Expecto Patronum has a fuller luminous Octane body with less mist. The download contains all 11 spells, 12 WAVs, soft glow textures, a Windows installer and installation/multiplayer instructions.
