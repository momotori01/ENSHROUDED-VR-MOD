Enshrouded VR MOD
=================

日本語の説明は下にあります。

INSTALL
1. Install and run the Steam PC version of Enshrouded normally once, then close it.
2. In Steam: Manage -> Browse local files.
3. Put ALL contents of this package into the folder that contains enshrouded.exe.
   dinput8.dll and "Enshrouded VR setting.exe" must sit directly beside enshrouded.exe,
   and the EnshroudedVR folder beside them.
4. Start the game from Steam as usual.

The mod comes with VR OFF. Nothing else is needed for playing on the monitor.

VR ON / OFF
- Start "Enshrouded VR setting.exe" (with the game closed) and choose "VR" or "Normal".
  The choice is saved; the game uses it every time it starts. Use the program again
  only to change it.
- VR: connect the headset and start your PC VR runtime (SteamVR) first, then start the
  game from Steam and put the headset on. From the start the game's picture is on a
  screen in front of you, and the controllers work its menus:
    point at the screen and pull the trigger = click
    stick = move the selection, A / X = confirm, B / Y = back
  Choose or join a world. When you are in it the screen stays for a few more seconds
  (a small block moves along its lower edge), then VR begins. The very first time on a
  PC this takes about half a minute; after that about 10 to 20 seconds.
  If you turn away from the screen, it comes back in front of you after a moment.
- The picture size, DLSS mode and sharpening for VR are in the same program.
- "Frame mode (VR)": "One eye a frame" draws the left and the right eye in turn, and the
  other eye shows the picture it had. It gives more frames a second; things that move
  can look a little doubled. It is a test - switch back to "Both eyes every frame" if it
  does not suit you.
- While VR is on, the mod sets the game's anti-aliasing to DLSS. Your own setting comes
  back the next time the game is started with VR off.

UPDATE
- "Enshrouded VR setting.exe" shows the installed version and the newest release.
  "Update now" downloads the newest release from GitHub, checks it, and replaces the
  mod's files. Your settings (EnshroudedVR\EnshroudedVR.ini) stay. The replaced files
  are kept in EnshroudedVR\backup (the last 3 updates). Close the game first.

PLAYING ONLINE
- With the mod installed you see how other VR players move - with VR on or off.
- Players without the mod can play with you as usual. They do not see VR movements.
- The mod talks only to other copies of the mod (Steam friends in your session and
  the members of your lobby), directly through Steam.

REQUIREMENTS
- The game version this package was made for (see EnshroudedVR\version.txt). After a
  game update the mod switches itself off and the game runs without it, until a new
  package is out.
- An NVIDIA graphics card with DLSS for VR.
- The Microsoft Visual C++ x64 runtime (normally installed with Steam games):
  https://aka.ms/vs/17/release/vc_redist.x64.exe
- For VR: an OpenXR runtime (tested with SteamVR).

IF SOMETHING GOES WRONG
- "Enshrouded VR setting.exe" shows what the mod did at the last start of the game.
- "Make zip" in that program puts the mod's logs, its settings, the game's graphics
  settings and the game's log (enshrouded.log) into one zip beside the program. Send
  that zip with a short description of what happened.
- The logs are in EnshroudedVR\logs. The newest 8 starts are kept ([Log] Keep in
  EnshroudedVR\EnshroudedVR.ini).

REMOVE
Delete dinput8.dll, "Enshrouded VR setting.exe" and the EnshroudedVR folder.
(If VR was on last, start the game once with VR off before, so your anti-aliasing
setting comes back.)

Third-party parts: OpenXR loader (Apache-2.0), MinHook (BSD-2-Clause) - see
EnshroudedVR\licenses.


日本語
======

入れ方
1. Steam 版 Enshrouded を一度ふつうに起動してから終了します。
2. Steam で「管理 → ローカルファイルを閲覧」。
3. このパッケージの中身を全部、enshrouded.exe のあるフォルダへ置きます。
   dinput8.dll と「Enshrouded VR setting.exe」が enshrouded.exe のすぐ隣、
   EnshroudedVR フォルダもその隣に来るようにしてください。
4. いつもどおり Steam からゲームを起動します。

入れた直後は VR は「切」です。モニターで遊ぶだけなら、他に何もいりません。

VR の入・切
- ゲームを閉じた状態で「Enshrouded VR setting.exe」を開き、「VR」か「Normal」を選びます。
  選んだ内容は保存され、ゲームを起動するたびに使われます。変えたい時だけ開いてください。
- VR：ヘッドセットをつなぎ、先に PC の VR ランタイム（SteamVR）を起動してから、Steam で
  ゲームを起動し、ヘッドセットをかぶります。最初からゲームの画面が目の前の板に出て、
  メニューはコントローラーで操作できます。
    板に向けてトリガー＝クリック
    スティック＝選択の移動、A / X＝決定、B / Y＝戻る
  ワールドを選ぶか参加します。ワールドに入ると、板のまま数秒（下の方を小さな四角が動き
  ます）、その後 VR が始まります。その PC で最初の 1 回だけ約 30 秒、2 回目からは約 10〜20
  秒です。
  板から顔を背けると、少しして板が目の前に戻ります。
- VR の絵の大きさ・DLSS のモード・シャープさも同じプログラムで選べます。
- 「Frame mode (VR)」の「One eye a frame」は、左右の目を 1 コマずつ交互に描き、描かない
  方の目は前の絵を見せます。1 秒あたりのコマ数が増えます。動く物は少し二重に見える事が
  あります。試験中の機能です。合わなければ「Both eyes every frame」に戻してください。
- VR が「入」の間、MOD はゲームのアンチエイリアスを DLSS にします。元の設定は、次に VR
  「切」でゲームを起動した時に戻ります。

アップデート
- 「Enshrouded VR setting.exe」に、入っている版と最新版が出ます。「Update now」を押すと、
  GitHub から最新版をダウンロードし、確かめてから MOD のファイルを入れ替えます。設定
  （EnshroudedVR\EnshroudedVR.ini）はそのまま残ります。入れ替えた古いファイルは
  EnshroudedVR\backup に残ります（最新 3 回分）。ゲームを閉じてから押してください。

オンライン
- MOD を入れていれば、VR の入・切に関わらず、他の VR プレイヤーの動きが見えます。
- MOD を入れていない人とも普通に遊べます。その人たちには VR の動きは見えません。
- MOD が通信する相手は、同じ MOD を入れた人（セッションにいる Steam フレンドと、自分の
  ロビーのメンバー）だけで、Steam を通した直接の通信です。

必要な物
- このパッケージが対応するゲームのバージョン（EnshroudedVR\version.txt）。ゲームが更新
  されると MOD は自分で止まり、新しいパッケージが出るまでゲームは MOD なしで動きます。
- VR には DLSS の使える NVIDIA のグラフィックカード。
- Microsoft Visual C++ x64 ランタイム（ふつうは Steam のゲームと一緒に入っています）：
  https://aka.ms/vs/17/release/vc_redist.x64.exe
- VR には OpenXR ランタイム（SteamVR で確認しています）。

うまく動かない時
- 「Enshrouded VR setting.exe」に、前回ゲームを起動した時に MOD が何をしたかが出ます。
- 同じプログラムの「Make zip」で、MOD のログ・MOD の設定・ゲームの画面設定・ゲームの
  ログ（enshrouded.log）が 1 つの zip になり、プログラムと同じ場所に出ます。その zip と、
  何が起きたかの短い説明を送ってください。
- ログは EnshroudedVR\logs にあります。新しい方から 8 回ぶんが残ります
  （EnshroudedVR\EnshroudedVR.ini の [Log] Keep）。

外し方
dinput8.dll、「Enshrouded VR setting.exe」、EnshroudedVR フォルダを消します。
（最後に VR「入」で遊んだ場合は、消す前に一度 VR「切」でゲームを起動すると、
アンチエイリアスの設定が元に戻ります。）
