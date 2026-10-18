# Enshrouded VR - 6DoF Motion Controls

Fight by swinging your sword, draw and loose your bow, and cast your fishing rod with a swing. Made with VHVR (ValheimVR) as the model.

## Features
- Native stereo rendering
- Multiplayer: other players' full-body IK is shown
- Switch between VR mode and desktop mode. VR and desktop players can play together
- Combat by motion controls
- Rising slashes, downward slashes, and the movement of skill attacks follow your swing.
  Example: when you attack in the air, swing down for a plunging attack, swing up for a rising slash.
- Throwing with your left hand as the guide line
- Changing weapons with holsters on your body
- Sneaking with room scale: crouch for real
- A UI that fades out by itself 5 seconds after the last change
- And much more.

## Download
Download **EnshroudedVR-x.y.z.zip** under **Assets** on the [Releases](https://github.com/momotori01/ENSHROUDED-VR-MOD/releases) page (not the source code archives).

## Bug reports
1. Hold the left controller at the left side of your head and press the left stick. The start menu opens with the red **"! VR bug report !"** button.
2. Press the button and close the menu. The mod now records 30 seconds of detailed logs and pictures of both eyes (the last seconds before you pressed it are kept too). Keep playing - show the problem if you can.
3. A zip appears in the game folder (the folder with enshrouded.exe): **VR_bug_report_<time>.zip** (with the game in Japanese: バグ報告_<time>.zip).

If you would like to help fix bugs, please send that zip with a short description of what happened (on Discord or as a GitHub issue). The logs contain no personal information: names, the PC's name, IP addresses and Steam IDs are replaced in the zip. I fix what I can.
If the game crashes while it records, the zip is still made. A report that was cut short is finished by **Make zip** in "Enshrouded VR setting.exe".

## Updating
Press **Update now** in "Enshrouded VR setting.exe" (with the game closed). The mod is updated to the latest version automatically: it downloads the newest release, checks it, and replaces the mod's files. Your settings are kept, and the replaced files are saved in EnshroudedVR\backup.

## Enshrouded VR setting.exe
The program next to enshrouded.exe holds the settings that cannot be changed in the game. Start the game from Steam as usual; it uses these settings every time. They can be changed only while the game is closed and take effect at the next start.

| Setting | What it does |
|---|---|
| Update | Shows the installed and the newest version. **Update now** installs the newest release. |
| Play mode | **VR**: play with the headset. **Normal**: play on your monitor without VR. Other VR players' movements show in both. |
| Picture size (VR) | Low 0.7 (faster) / Normal 0.8 / High 1.0 (sharper, slower) |
| DLSS (VR) | Quality / Balanced / Performance / DLAA (sharpest, slower) |
| Sharpening (VR) | Off / Soft 0.2 / Strong 0.5 |
| Frame mode (VR) | Both eyes every frame (normal), or one eye a frame (a test for more FPS) |
| Problem report | **Make zip**: the mod's logs, its settings and the game's log in one zip next to the program, to send with a description. It also finishes a VR bug report that a crash cut short. |
| Put settings back | All settings back to how they came. |

It also shows what the mod did at the last start of the game.
Real crouching for sneaking is switched on or off, and its depth set, in the game itself: the **VR settings** button next to the bug report button in the start menu.

## Combat
### Melee
- **Attack:** swing your weapon. The hit lands where your blade really hits - you have to land it yourself.
- **Heavy attack:** swing while holding the right trigger.
- **In the air:** swing down for a plunging attack, swing up for a rising slash.
- **Skill attacks:** they move you as far as in vanilla; you still land the hit with your own weapon.
- Cooldowns are taken from the vanilla animations, so the mod does not make you overpowered. While an attack cools down, the controller vibrates and the next attack waits.

### Guard and parry
- **Shield:** hold the left grip to guard. The guard faces the way the shield's face points.
- **Two-handed weapons:** hold the handle with the left grip (both hands on the weapon). While you hold it, you guard.
  Swing with your right hand to attack - the attack comes first.
- **Parry:**
  - press the left grip just as the attack comes (the moment you raise the guard is a parry), or
  - while guarding, flick your left hand quickly (with a two-handed weapon: the left hand alone; both hands swung together is an attack).
  - A flick parry lowers your guard for a moment, so time it to the enemy's blow. The next flick parry is ready after 1 second.

### Bow
- Bring your right hand to the string, hold the right trigger and pull back. Let go of the trigger to shoot.
- While you pull, the bow turns to the line between your hands - aim along the arrow.

### Throwing
1. Holding a throwable, hold the right trigger to get ready.
2. Hold your left hand out in front of you: a guide line shows where it will go, following your left hand.
3. Swing your right hand and let go of the trigger to throw along the guide. Without the guide it flies the way your hand swings.

### Weapons and tools
- **Holsters:** left shoulder + left grip = bow, right shoulder + right grip = your current weapon, left hip + left grip = shield. A short vibration tells you it switched.
- **Quick slots:** hold right B, point your hand at the slot you want and let go.
- **Tools** (pickaxe, axe, ...): swing them like weapons. You aim with your right hand's direction.
- **Fishing rod:** swing it and let go of the trigger during the swing to cast. A stronger swing casts farther.

## Other controls
### Movement
- Left stick: move (forward is where you look)
- Right stick left/right: turn
- Right stick up: sprint / sneak
- Right stick down: dodge
- Right A: jump
- Right B: short press to interact (pick up, open, examine)
- **Sneak:** crouch in real life. Switch it on or off and set how deep you must crouch in the in-game "VR settings".

### HUD
- The HUD fades out after 5 seconds without changes and comes back when your health, stamina and the like change.
- The crosshair and gauges appear where your right hand points.

### Menus
- The title screen and the menus appear on a panel in front of you. Point at it with the controller and pull the trigger to click.
- Hold a hand against your temple: the right stick becomes the D-pad, pressing the right stick opens the menu, pressing the left stick goes back.
- **Building:** with the building hammer, hold left B to choose what to build (choose with the right stick, let go to confirm).

### More
- **Reset the view:** look at both palms in front of your face for 3 seconds (the headset's own reset works too).
- **Bug report:** left controller at the left side of your head + left stick, then the red button (see [Bug reports](#bug-reports)).
- **VR or desktop:** with the game closed, open "Enshrouded VR setting.exe" and choose "VR" or "Normal". Players with the mod see VR players' movements in both modes; players without the mod can still play with you.
- **Update:** "Update now" in "Enshrouded VR setting.exe" (see [Updating](#updating)).

## Installation
1. Start the Steam version of Enshrouded normally once, then quit.
2. In Steam: Manage -> Browse local files.
3. Put everything in the zip into the folder that contains enshrouded.exe.
4. With the game closed, open "Enshrouded VR setting.exe" and choose "VR" (VR is off right after installing).
5. Start SteamVR first, then start the game from Steam.

For details, see EnshroudedVR\README.txt in the package.

## Requirements
- The game version the package is made for (EnshroudedVR\version.txt). When the game is updated, the mod switches itself off and the game runs without it until a new package is out.
- An NVIDIA graphics card that supports DLSS
- An OpenXR runtime (tested with SteamVR)

## Known issues
- Now and then some objects do not line up between the eyes (wrong parallax).

---

This mod draws heavily on ValheimVR.
This mod was made with AI. I cannot read code.
I made it for my friends and myself. There will be no long-term support.
Anyone is free to edit it - if updates have stopped, please fix the bugs yourself.
