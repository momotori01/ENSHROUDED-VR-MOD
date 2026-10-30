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
- Start "Enshrouded VR setting.exe" (with the game closed) and choose the play mode
  ("motion VR", "controller VR 【FPS】", "controller VR 【TPS】" or "Desktop").
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
- controller VR: "controller VR 【FPS】" and "controller VR 【TPS】" show the game in the
  headset and are played with an Xbox controller and the game's own controls. FPS aims
  with your head, TPS with the right stick. In the start menu: RT = switch FPS / TPS,
  D-pad left/right = view tilt for playing lying down (1 degree a press), LT = reset the
  view (where your head looks now is ahead, the tilt included), right stick click = pitch
  lock (TPS), left stick click = VR bug report.
- The picture size, DLSS / FSR 3 mode and sharpening for VR are in the same program.
- "View fit (VR)": Auto fits the picture to the headset's own field of view when the mod finds
  ALVR (with ALVR the view is cross-eyed otherwise). If the view is cross-eyed, choose "Fit".
- "Menu flicker fix (VR)": off by default. Turn it on if the character in the inventory
  flickers black: while a menu is open, the game's own view is not drawn (the character's
  black frames come from it).
- "Frame mode (VR)": "One eye a frame" draws the left and the right eye in turn, and the
  other eye shows the picture it had. It gives more frames a second; things that move
  can look a little doubled. It is a test - switch back to "Both eyes every frame" if it
  does not suit you.
- While VR is on, the mod sets the game's anti-aliasing to DLSS (DLAA stays DLAA; a card
  without DLSS, such as AMD, uses FSR 3) and turns HDR (VR stops with it) and vertical sync
  off. The DLSS / FSR 3 quality stays your choice. They come back the next time the game is
  started with VR off; nothing else is changed. "Recommended graphics (VR)" sets all graphics settings for
  VR on this PC (DLSS Quality, or FSR 3 on cards that are not NVIDIA's).
- Only what your lenses show is drawn: at every start the mod reads your headset's field
  of view and the area its lenses show, turns each eye's camera toward the middle of it
  and draws a picture that just covers it - about 15 to 25 % fewer pixels at the same
  sharpness. Nothing to set. To turn it off (for a test), add these two lines to
  EnshroudedVR\EnshroudedVR.ini:
    [Environment]
    EVR_CANTED_FOV=0

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
- A graphics card that runs the game in VR: NVIDIA cards use DLSS, AMD cards FSR 3.
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
(If VR was on last, start the game once with VR off before, so your anti-aliasing, HDR
and vertical sync settings come back.)

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
- ゲームを閉じた状態で「Enshrouded VR setting.exe」を開き、動作モード（「motion VR」
  「controller VR 【FPS】」「controller VR 【TPS】」「Desktop」）を選びます。
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
- コントローラー VR：「controller VR 【FPS】」と「controller VR 【TPS】」は、ヘッドセットに
  ゲームを映し、Xbox コントローラーとゲームの元の操作で遊びます。FPS は頭の向きで、TPS は
  右スティックで狙います。スタートメニューで RT＝FPS と TPS の切り替え、十字キー左右＝
  寝ながら遊ぶ用の視点の傾き（1 回 1°）、LT＝視点のリセット（今の頭の向きを正面に。傾きも
  合わせる）、右スティック押し込み＝上下の傾きの固定（TPS）、左スティック押し込み＝VR バグ報告。
- VR の絵の大きさ・DLSS / FSR 3 のモード・シャープさも同じプログラムで選べます。
- 「View fit (VR)」：Auto は、MOD が ALVR を見つけた時だけ絵をヘッドセットの視野に合わせます（ALVR
  ではそうしないと寄り目になります）。寄り目になったら「Fit」を選んでください。
- 「Menu flicker fix (VR)」：既定は切。持ち物のキャラクターが黒くちらつく時は入にしてください。
  メニューを開いている間、ゲーム自身の画面を描きません（キャラクターが黒くなるのはその画面の回です）。
- 「Frame mode (VR)」の「One eye a frame」は、左右の目を 1 コマずつ交互に描き、描かない
  方の目は前の絵を見せます。1 秒あたりのコマ数が増えます。動く物は少し二重に見える事が
  あります。試験中の機能です。合わなければ「Both eyes every frame」に戻してください。
- VR が「入」の間、MOD はゲームのアンチエイリアスを DLSS にし、FSR 3 も同じ倍率にします
  （DLSS の無い AMD などのカードは FSR 3 で描きます）。元の設定は、次に VR「切」で
  ゲームを起動した時に戻ります。
- レンズに映る所だけを描きます。起動のたびにヘッドセットの視野とレンズの見える範囲を
  読み、目のカメラをその中心の方へ向けて、ちょうど覆う大きさの絵を描きます（同じ細かさで
  画素が約 15〜25% 減ります）。設定は要りません。試しに切る時は、
  EnshroudedVR\EnshroudedVR.ini に次の 2 行を足します：
    [Environment]
    EVR_CANTED_FOV=0

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
- VR を動かせるグラフィックカード（NVIDIA は DLSS、AMD は FSR 3 を使います）。
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
