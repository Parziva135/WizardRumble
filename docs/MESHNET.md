# Host and join with NordVPN Meshnet

Use this after every player has followed [START_HERE.md](../START_HERE.md) and installed Wizard Rumble **0.9.2**, BakkesMod, Rocket Plugin **26** and NetcodePlugin **166**.

The host's Rocket League session runs the match. **Meshnet connects the PCs**, including friends in another country; **Rocket Plugin creates and joins the lobby**. GitHub supplies the files. Keep the host PC, game and Meshnet running throughout play.

## 1. Everyone turns on Meshnet

Each player installs the [NordVPN Windows app](https://nordvpn.com/meshnet/), signs in with their own Nord account and enables Meshnet. Friends can use free accounts; a paid VPN subscription is not required. In Nord's Windows guide, the feature is named **Devices in Meshnet** in the sidebar. Newer app layouts may group it under Products. [Nord's Windows setup](https://meshnet.nordvpn.com/getting-started/how-to-start-using-meshnet/using-meshnet-on-windows).

## 2. The host links each friend

On the host's PC, open **Devices in Meshnet → Link devices → Link external device**. Enter the email address of the friend's Nord account and send an invitation. The friend opens their received Meshnet invitations, responds and accepts. Repeat for every friend's playing PC. Nord's [Windows linking guide](https://meshnet.nordvpn.com/features/linking-devices-in-meshnet/adding-meshnet-devices-on-windows) shows the current invitation screens.

For each friend, the host must allow Meshnet's **remote access** permission so that friend's PC can connect to the host PC. Check the linked devices on both ends if connectivity fails. This permission concerns incoming connections over Meshnet. The direct-to-host arrangement here does not use internet traffic routing through the host or access to other devices on the host's home LAN. [Permission explanation](https://meshnet.nordvpn.com/features/explaining-permissions/remote-access-permissions).

## 3. The host shares the right address

In Meshnet, find the host PC's **Meshnet IP address** under its device details and share it with the invited players. Use the numerical Meshnet IP in Rocket Plugin. Keep it alongside the lobby password. It is the host PC's address within Meshnet, not the IP of a country VPN server. Nord's [Windows guide](https://meshnet.nordvpn.com/getting-started/how-to-start-using-meshnet/using-meshnet-on-windows) shows where the device address appears.

## 4. The host creates the game

Start Rocket League with BakkesMod working. Press **Home** to open Rocket Plugin, select **Multiplayer** and use the hosting side of the menu. Choose a standard arena and **Soccar**, set **Rumble to None**, choose a lobby password and click **Host**. Keep the host and joining players' port settings consistent; Rocket Plugin documents **7777** for local hosting. [Rocket Plugin instructions](https://bakkesplugins.com/plugin/26).

## 5. Friends join

Each friend leaves Meshnet running, starts Rocket League with BakkesMod, presses **Home** and opens Rocket Plugin's **Multiplayer** tab. On the join side, enter the host's Meshnet IP and the matching port/password, then click **Join**. First confirm that normal driving and ball movement work in the same lobby.

## 6. Everyone loads the spells

Set Rocket League chat to **Allow All** and leave the other players unmuted. NetcodePlugin needs this for its invisible spell messages. [NetcodePlugin requirements](https://bakkesplugins.com/plugin/166).

On every PC, open **F6** and run:

```text
plugin load WizardRumble
wr_status
```

During active play, the host should report `supported=1`, `host=1`, `active=1`. Friends should report `supported=1`, `host=0`, `active=1`, and get a spell slot. Check version **0.9.2** and `glow_textures=1`. Wait for a spell, aim and press **R**.

## If joining fails

Check Meshnet is enabled on both PCs, the invitation was accepted on the actual playing PC, the host permits that peer's remote access, and the address is the host's Meshnet IP. Check Rocket Plugin's port/password and that the host is still in the lobby. If players can drive together but spells say "waiting for host," check matching versions, NetcodePlugin, chat and mute settings.

This connection method follows Meshnet's documented private-network capability and Rocket Plugin's IP-join feature. **A native two-PC/cross-country Wizard Rumble test is still required.** Start with one friend on the opposing team; test a charged cast, goal replay, respawn and reconnect before a full group session.
