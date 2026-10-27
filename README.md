# Enshrouded VR - 6DoF Motion Controls

Fight by swinging your sword, cast spells by swinging your wand, draw and loose your bow, and fish with the rod in your hand. Made with VHVR (ValheimVR) as the model.

## Features
- Native stereo rendering
- Multiplayer: other players' full-body IK is shown - including the fish they just caught
- Four play modes: motion VR (full VR with motion controllers), controller VR in first person (FPS) or third person (TPS) - also lying down -, and desktop. VR and desktop players can play together
- Combat by motion controls: melee, wand, bow, throwing
- Rising slashes, downward slashes, and the movement of skill attacks follow your swing.
  Example: when you attack in the air, swing down for a plunging attack, swing up for a rising slash.
- Fishing by motion controls: cast with a swing, fight the fish by pulling the rod against it, flick the rod to hook and reel in, feel the fish in the controller, and hold up your catch
- Enemy health bars and the lock-on arrow above the enemies, not on the HUD
- Throwing with your left hand as the guide line
- Changing weapons with holsters on your body
- Sneaking with room scale: crouch for real
- A UI that fades out by itself 5 seconds after the last change
- Draws only what your lenses show: at every start the mod reads your headset's field of view and the area its lenses show, and fits each eye's picture to it - about 15-25 % fewer pixels at the same sharpness, from the very first start
- An update notice in the game when a new version is out
- And much more.

## Download
Download **EnshroudedVR-x.y.z.zip** under **Assets** on the [Releases](https://github.com/momotori01/ENSHROUDED-VR-MOD/releases) page (not the source code archives).

## Bug reports
1. Hold the left controller at the left side of your head and press the left stick. The start menu opens with the red **"! VR bug report !"** button.
2. Press the button and close the menu. The mod now records 30 seconds of detailed logs and pictures of both eyes (the last seconds before you pressed it are kept too). Keep playing - show the problem if you can.
   With controller VR: open the start menu and press the left stick, then close the menu.
3. A zip appears in the game folder (the folder with enshrouded.exe): **VR_bug_report_<time>.zip** (with the game in Japanese: バグ報告_<time>.zip).

If you would like to help fix bugs, please send that zip with a short description of what happened (on Discord or as a GitHub issue). The logs contain no personal information: names, the PC's name, IP addresses and Steam IDs are replaced in the zip. I fix what I can.
If the game crashes while it records, the zip is still made. A report that was cut short is finished by **Make zip** in "Enshrouded VR setting.exe".

## Updating
When a newer version is out, **"MOD Update"** blinks slowly in the game's main menu (above "Play") and in the start menu next to the VR buttons.
Press **Update now** in "Enshrouded VR setting.exe" (with the game closed). The mod is updated to the latest version automatically: it downloads the newest release, checks it, and replaces the mod's files. Your settings are kept, and the replaced files are saved in EnshroudedVR\backup.

## Enshrouded VR setting.exe
The program next to enshrouded.exe holds the settings that cannot be changed in the game. Start the game from Steam as usual; it uses these settings every time. They can be changed only while the game is closed and take effect at the next start.

| Setting | What it does |
|---|---|
| Update | Shows the installed and the newest version. **Update now** installs the newest release. |
| Play mode | **motion VR**: full VR. **controller VR 【FPS】** / **controller VR 【TPS】**: the headset shows the game, played with an Xbox controller (see [controller VR](#controller-vr)). **Desktop**: play on your monitor without VR. Other VR players' movements show in all of them. |
| Pitch lock (controller VR, TPS) | On: the view stays level; the camera's up and down is left out. |
| View tilt (controller VR) | 0-90°: tilts the view down, for playing lying down. At 90°, lying on your back, the ceiling is ahead. |
| Picture size (VR) | Low 0.7 (faster) / Normal 0.8 / High 1.0 (sharper, slower) |
| Sharpening (VR) | Off / Soft 0.2 / Strong 0.5 |
| View fit (VR) | How each eye's picture is fitted to the headset's view. **Auto** (the default): fitted only when the mod finds ALVR (with ALVR the view is cross-eyed without it). **Normal**: as always. **Fit (if cross-eyed)**: always fitted - choose it if the view is cross-eyed. |
| Menu flicker fix (VR) | Off by default. Turn it on only if the character in the inventory flickers black: while a menu is open the game's own view is drawn every frame (menus get a little heavier). |
| Frame mode (VR) | Both eyes every frame (normal), or one eye a frame (a test for more FPS) |
| Problem report | **Make zip**: the mod's logs, its settings and the game's log in one zip next to the program, to send with a description. It also finishes a VR bug report that a crash cut short. |
| Put settings back | All settings back to how they came. |

It also shows what the mod did at the last start of the game.
A VR start changes three of the game's graphics settings, the ones that are always right for VR: anti-aliasing to DLSS (DLAA stays DLAA; cards without DLSS use FSR 3), HDR off (VR stops with HDR on) and vertical sync off (the headset sets the frame timing). The DLSS / FSR 3 quality (Quality, Performance and so on) stays your choice in the game. They come back the next time the game is started without VR. The mod changes nothing else. **Recommended graphics (VR)** in this program sets all of the game's graphics settings to the ones recommended for VR on this PC: DLSS Quality (FSR 3 Quality on cards that are not NVIDIA's), indirect lighting and the rest. After that, change anything you like in the game; the mod keeps it.
Real crouching for sneaking is switched on or off, and its depth set, in the game itself: the **VR settings** button next to the bug report button in the start menu.

## controller VR
Choose **controller VR 【FPS】** or **controller VR 【TPS】** in "Enshrouded VR setting.exe". The headset shows the game; you play with an Xbox controller and the game's own controls.
- **FPS**: first person. You aim where your head looks. The head bob stays out, as in motion VR.
- **TPS**: the game's own third-person camera. You aim with the right stick; the HUD and the crosshair stay in front of the game camera.
- The locked enemy's health bar and lock-on arrow stand above the enemy, as in motion VR.

In the start menu, a small panel lists what the controller changes there:
- **RT**: switch between FPS and TPS
- **D-pad left/right**: view tilt, 1° a press (hold to keep going)
- **LT**: reset the view - where your head looks now becomes ahead, the tilt included (lie down, look where you want "ahead" to be, press LT)
- **Right stick click**: pitch lock on/off (TPS)
- **Left stick click**: VR bug report (it records 30 seconds after you close the menu)

The changes are kept for the next start.

## Combat
### Melee
- **Attack:** swing your weapon. The hit lands where your blade really hits - you have to land it yourself. The hit area is slightly larger than the blade, and quick wrist flicks count along the path of the blade's tip.
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
  - In solo play and as the host, a flick a little before or after the enemy's parry moment (0.2 s) still counts.

### Wand
- Swing the wand to cast its normal attack. A normal, deliberate swing is enough; the cooldown is the vanilla one.
- The trigger casts as well.

### Staff
- Hold the trigger to charge; let go to cast. Nothing is cast while you hold it, so you can switch spells while charging.
- The left trigger charges too (it does not cast); the right trigger's release casts.

### Special abilities
When the special ability gauge is full:
- **Melee weapons:** swing while holding the right grip (no trigger needed).
- **Wand:** hold the right grip and pull the trigger, or swing while holding the right grip.
- **Staff:** hold the right grip and pull the trigger - while you hold it, it is the game's own RT.
- **Bow:** take the string with the right grip instead of the trigger, draw and let go: a strong shot.

### Lock-on
- The game's own lock-on works as usual. The enemy's health bar and the lock-on arrow are shown above the enemy instead of on the HUD - for the locked enemy, the lock-on candidate and enemies that are aware of you. Moving stays headset-based while locked on.

### Bow
- Bring your right hand to the string, hold the right trigger and pull back. Let go of the trigger to shoot.
- While you pull, the bow turns to the line between your hands - aim along the arrow.

### Throwing
1. Holding a throwable, hold the right trigger to get ready.
2. Hold your left hand out in front of you: a guide line shows where it will go, following your left hand.
3. Swing your right hand and let go of the trigger to throw along the guide. Without the guide it flies the way your hand swings.
- The throwable shows in your right hand, and it leaves the moment you let go.

### Weapons and tools
- **Holsters:** left shoulder + left grip = bow, right shoulder + right grip = your current weapon, left hip + left grip = shield. A short vibration tells you it switched.
- **Bow from the shoulder:** if your equipped bow is not on the action bar, the mod puts it in your left hand. The game only takes it out while you draw the string, so you walk at full speed, and drawing in the air still uses the bow's air skills. Put it away at the left shoulder again to get your weapon back.
- **Quick slots:** hold right B, point your hand at the slot you want and let go.
- **Tools** (pickaxe, axe, ...): swing them like weapons. You aim with your right hand's direction.

### Building
- **Enter / leave building mode:** left trigger with the building hammer in your hand.
- **Build:** right trigger.
- **Choose what to build:** hold left B (or right B), choose with the right stick, let go to take it.
- **Hold the right grip** - it is the gamepad's RT - for the combined inputs:

| Right grip held + | Does |
| --- | --- |
| Right stick left / right | Turn the piece by 90 degrees |
| Right stick up / down | Move the piece nearer / farther |
| Left stick left / right | Turn the piece a little |
| Left stick up / down | Raise / lower the piece |
| Right B | Undo |
| Right A | Material catalog |
| Left A | Snapping on / off |
| Left grip | Take the material you are looking at |

- While the right grip is held, the right stick does not turn you. Tilting and scaling a piece (the gamepad's RT + right stick) are not available in VR.

### Food, potions and other items
- Choosing a consumable on the quick slots puts it in your right hand (a short vibration) instead of using it at once. Bring it to your mouth or pull the right trigger to use it.
- Each item is held the way it would be in life: bottles, cups and bombs in the closed hand, plates, bowls and handfuls (berries, nuts, popcorn, mushrooms) on the open palm, a whole fish by its tail.
- Other players with the mod see the item in your hand.

### Fishing
- **Cast:** hold the trigger, swing the rod and let go of the trigger during the swing. A faster swing casts farther (how long you hold the trigger does not matter).
- **Reel in:** hold the trigger.
- **Hook, reel in, land:** instead of the trigger you can **flick the rod upward** (nearly straight up). The flick is ignored while you fight the fish.
- **Fight:** when the fish pulls to one side, lean the rod tip to the other side (the same as A / D on keyboard). The controller pulses while you are tiring the fish.
- **Landing:** when the fish is tired, click (the trigger or a flick). If you are still holding the trigger then, the mod clicks for you - keep holding to land it.
- **The catch:** when the fish is landed, you hold it in your left hand the way the game holds it. Other players with the mod see it in your hand too - show it off.

## Other controls
### Movement
- Left stick: move (forward is where you look)
- Right stick left/right: turn
- Right stick up: sprint / sneak
- Right stick down: dodge
- Right A: jump
- Right B: short press to interact (pick up, open, examine)
- Left trigger: interact, for as long as you hold it (pick up, open, revive) - pull it all the way; a resting finger does nothing
- **Revive a fallen player:** go to them and hold the left trigger (or hold right B within 3 m of them).
- **Sneak:** crouch in real life. Switch it on or off and set how deep you must crouch in the in-game "VR settings".

### HUD
- The HUD fades out after 5 seconds without changes and comes back when your health, stamina and the like change. The held item's name and button hints at the bottom fade with it.
- The crosshair and gauges appear where your right hand points.
- Enemy health bars and the lock-on arrow stand above the enemies.

### Menus
- The title screen and the menus appear on a panel in front of you. Point at it with the controller and pull the trigger to click.
- With the pointer on a menu, right A is the game's A on what is selected (pull the trigger to select something first): craft, unlock a skill, fast travel from a picked map location, and so on. In the backpack, right A works on the slot you point at.
- Hold a hand against your temple until the controller buzzes briefly, then press: the right stick becomes the D-pad, pressing the right stick opens the inventory, pressing the left stick opens the start menu. A stick already pressed before the buzz does not count.
- **Building:** with the building hammer, hold left B to choose what to build (choose with the right stick, let go to confirm).
- **Left trigger with tools:** with the building hammer the left trigger enters building mode; with the summoning staff it enters summoning.
- **Camera:** every game camera stays first person in VR (talking to NPCs, altars and the like).

### More
- **Reset the view:** look at both palms in front of your face for 3 seconds (the headset's own reset works too).
- **Bug report:** left controller at the left side of your head + left stick, then the red button (see [Bug reports](#bug-reports)).
- **VR or desktop:** with the game closed, open "Enshrouded VR setting.exe" and choose the play mode. Players with the mod see VR players' movements in both modes; players without the mod can still play with you.
- **Update:** "Update now" in "Enshrouded VR setting.exe" (see [Updating](#updating)).

## Installation
1. Start the Steam version of Enshrouded normally once, then quit.
2. In Steam: Manage -> Browse local files.
3. Put everything in the zip into the folder that contains enshrouded.exe.
4. With the game closed, open "Enshrouded VR setting.exe" and choose a VR play mode (VR is off right after installing).
5. Start SteamVR first, then start the game from Steam.

For details, see EnshroudedVR\README.txt in the package.

## Requirements
- The game version the package is made for (EnshroudedVR\version.txt). When the game is updated, the mod switches itself off and the game runs without it until a new package is out.
- A graphics card that runs the game well in VR: NVIDIA cards use DLSS; AMD cards use FSR 3 at the same scale
- An OpenXR runtime (tested with SteamVR)

## Known issues
Most of the parallax and visual differences between the eyes are fixed. Two visual issues remain:
- Water reflections can look different between the left and right eye.
- A puddle in the shade of trees can look brighter in the right eye.

---

This mod is based on ValheimVR. I have played that mod for 300 hours - I love it.
This mod was made with AI. I cannot read code.
I made it for my friends and myself. There will be no long-term support.
Anyone is free to edit it - if updates have stopped, please fix the bugs yourself.
