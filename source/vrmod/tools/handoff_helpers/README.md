# 補助スクリプト（2026-10-09 Claude が一時フォルダで使っていたものを引き継ぎ用に複製）

製品コードではなく、試験の起動・取得・比較を楽にするための小道具です。Git Bash（`bash`）と
`S:/stable/Python31011/python.exe`（Python 3.10、標準ライブラリだけ）で動かします。
シェルスクリプトは run の出力テキスト（`run_with_sound.ps1 -Out` のファイル）に出る `== game-YYYYMMDD-HHMMSS` を見て
対象の起動フォルダ（`vrmod/output/game-*`）を探します。

| ファイル | 使い方 | 内容 |
|---|---|---|
| `run_with_sound.ps1` | `pwsh -File run_with_sound.ps1 -Script ./vrmod/tools/run_play_session.ps1 -ScriptArgs "<引数>" -Out <出力txt>` | ルート（`S:\SteamLibrary\steamapps\common\Enshrouded`）で試験を実行し、全出力をファイルへ。ゲーム終了後に終了音（`notify_user_sound.ps1 -Kind GameClosed`）。起動音はユーザー指示（10-09「ゲーム起動時の音は不要」）で鳴らさない。 |
| `ab_run.sh <tag> <追加の引数>` | 例 `bash ab_run.sh base` / `bash ab_run.sh noparticles -NoVfxParticles` | 草むらへ前進する台本（`world-live-input-grass-walk-hands.csv`）で2分のMock試験 → 最初のペアから30秒後に連続10フレーム取得 → 点滅の指標。出力は `$SP`（既定 `handoff_helpers/out/`）。 |
| `burst_at.sh <出力txt> <秒> [枚数]` | 試験と並行して起動 | 最初のペアから指定秒後に `<launch>/xr-burst.request` を置き、XR画像を連続取得（1セッション1回・最大32）。 |
| `frame_caps.sh <出力txt> f1 f2 ...` | 〃 | 手の台本のフレーム番号に達したら `xr-capture.request`（単発取得）。 |
| `periodic_caps.sh <出力txt> <開始秒> <間隔秒> <枚数>` | 〃 | 一定間隔で単発取得。 |
| `desk_caps.sh <出力txt> <秒> <枚数>` | 〃 | デスクトップ（ミラー）の表示を `capture.request` で取得。 |
| `toggles.py <launch> [heat.png]` | 試験後 | 連続フレームで「A→B→A」と戻る画素の割合（点滅）を左右の目ごとに出す。風の揺れはなめらかなので数えない。右目の発生位置をPNGに。 |
| `flicker.py <launch>` | 試験後 | 近いフレーム間の平均差。 |
| `lr_luma.py <launch>` | 試験後 | 取得フレームごとの左右の目の平均輝度（全体・下半分）。 |
| `grid_lr.py <out.png> <launch> k1 k2 ...` | 試験後 | k番目の取得フレームを左右並べて1/5で縦に積む（目視比較用）。 |

取得画像は `<launch>/native-xr/frame-<N>-eye-<0|1>.ppm`（XRへ送った左右の目そのもの）。
目安：静止した場面の点滅の画素は 0.01% 前後、頭を振ると 0.2% 程度（細い枝）。旧ミラーの不具合のような破損は桁が違う。


## 追加の景観比較（2026-10-09）
- `../capture_scenery_bursts.py <launch> --frames 850,1150,1450,1750`：明示した最大4回×32組を順に要求し、受理と全枚数完了を検査。既存の1欄requestは従来どおり1回。4回撮影では`32 4`を使用する。終了が早く必要frameへ到達しなければFAIL。
- `../analyze_scenery_bursts.py <launch>`：NumPy/Pillow環境で実行。カメラ回転を補正した隣接差分と両眼の一覧画像を作成。既定は4回×32組が必須。差分数値は視差/風/露出/手も含み、画質PASSを意味しない。
- 同一条件比較は`run_play_session.ps1 -SaveReference <保全済み基準> -SceneryReplay <描画基準> -Pose <6DoF台本>`。現全セーブを保全してから復元し、起動時SHAを照合。起動後の時刻/風は固定していない。
- `-SymmetricFov`、`-DetailBindings`、`-ShadowProjection`は現在opt-inの別々の修正。最新の実機確認状況はdocs/STATUS.mdとcheckpointを参照。

## はしご状の影の試験（2026-10-09 21:58）
- `../prepare_shadow_receiver_shader.py <保存された日光shadow SPIRV> <出力dir>`: SHA `e0d6de57a4db980fcdcfd7ba61640f0db21be193ef57ffde91b43dd6efca2427` のユーザーの保存shaderだけから局所生成する。ゲームshader本体の配布はしない。補正部分は `vrmod/shaders/shadow_receiver_helpers.glsl`。
- 通常の試験生成物は `vrmod/output/shadow-receiver-live-20261009`。`run_play_session.ps1` に `-ShadowReceiverPlane ./vrmod/output/shadow-receiver-live-20261009` を追加する。`-ShadowProjection` と併用するが別の問題の修正。shader生成物/SHAは `launch.json` と `shadow-receiver-check.json` に記録する。
- `--reset-history` と `--control` は保存RenderDocだけの比較用。launcherはこれらのmanifestを拒否する。通常版はゲームの影historyを保持する。
- `../check_shadow_receiver.py <launch>` は読み込まれたshaderの同一性だけの監査。画質合格ではない。生成shaderは日光visibilityの全Viewに適用（私有眼のみの切り替えではない）、既定は無効。
- 単純にbiasを増す案は影の輪郭をずらすので不採用。現在の補正は受け側の幾何面を深度から復元し、各sample位置へ比較深度を合わせる。手動PCFのgather位置をtexel中点へ明示して、GPUの丸めと小数部分の不一致を防ぐ。
- 検証: std140再compile controlは元の履歴なし計算と全画素一致。手動PCF control差は最大2/255、14画素だけ差>1。通常Mock game215351で4方向×32両眼撮影、同一カメラの床の縞が改善。実機は未検証。

## 左右差の追加監査（2026-10-09）
- `../analyze_coincident_eyes.py <launch>`: NumPy/Pillowで実行。`world-scenery-coincident-eyes-1963.csv`と4回×32組の撮影を使い、同じ光学系・ほぼ同じ位置の左右画像を比較。`geometry_status`のみPASS/失敗判定し、画質は手で確認する。実機用fixtureではない。体力の外側着色は意図的な左右差なので、必要に応じて診断時のみ`-StatusTint 0`で切り分ける。

## GIの黒い塊の診断（2026-10-10）
- `prepare_gi_probe_shader.py <保存されたnative GI shader> <新規出力dir> [--control]`: SHA限定で局所生成。native本体を配布しない。全候補prune時だけ相対重みを保持する補助は`vrmod/shaders/gi_probe_fallback.glsl`。
- `-GiProbeFallback ./vrmod/output/gi-probe-fallback-20261010`: 明示試験用。履歴を保持する、GI共有とは別のshader補正。既定は無効。実機の確認状況はSTATUS/road_shadow notesを参照。
- `check_gi_probe.py <launch>`はload/source/生成SHAの監査で、画質合格ではない。
- RenderDoc `debug-gi-pixel capture out event:x:y`: atomic queue indexは再生ごとに変わるので、指定pixelへの診断置換を使う。native33400bytes/FNV限定、traceの実pixelを照合。診断中の画像自体を画質証拠にしない。
- `compute-pair-compare capture out event1 shader1 previews event2 shader2`: 2つのcompute shaderの一時置換→before/after/restored。元captureは変更しない。`previews`は最大4textureindex。R32_UINTは量子化前の解釈を失わないためrawも保存する。RGB9E5として読むときはsampled viewを必ず照合。

## Mockの画像を見ながら短距離移動（2026-10-10）
- `run_play_session.ps1 -MockNavigate -LiveInput -Hands ./vrmod/test_env/world-scenery-neutral-hands.csv ...` を明示して起動する。RealVrでは拒否。固定台本の手poseは保持し、全ボタン/スティックはnavigation入力で置換される。実機や既定Mockの挙動は変更しない。
- `python vrmod/tools/mock_navigate.py <launch> --yaw 180 --pitch -20 --left-y 1 --hold-ms 1500`：通常controllerの左スティックを1.5秒だけ前へ。yaw/pitchは絶対Mock HMD角（度）、省略時は前回角を保持。頭位置/光学系は元fixtureのまま。
- `--left-x -1..1 --left-y -1..1 --right-x -1..1 --jump`、holdは0..2000ms。移動中でも更新を止めれば期限でneutral。連番/送信時刻を検査して古い指示で延長しない。手の装備/消耗品ボタンは操作しない。
- `python vrmod/tools/capture_mock_navigation.py <launch> <label>`（Pillow環境）：次のsubmitted両眼を単発取得、無補正PNGも保存。GPU待ちを伴うため通常非同期画質の証拠とは区別する。
- `<launch>/mock-navigation-sent.jsonl`は全指示、`mock.jsonl`のmock_navigation_command/inputは受理frameと解除。`launch.json`のmock_navigation=true/runtime=mockを照合する。
- 昼基準でも探索に時間が掛かれば夜になる。今起動のworld IDは `-ExpectedWorld 3bd85c7d` で確かめる。初期向き0は北の庭、草の道は頭180で南側出口・長い階段を下る。絶対座標の証拠はfog_motion_20261010/notes.md参照。

## ホーム鮮明さ・残像の切り分け（2026-10-10）
- align_head_replay.py <実測Mock CSV> <固定視点CSV> <新CSV>: 頭動作を固定視点へ剛体変換し、最後を保持。camera/セーブ監査は別に実行する。
- compare_home_sharpness.py <新出力dir> <launch0> <launch1>: 同fixture SHA/同セーブ/全camera/通常終了が前提。等倍4箇所/両眼と32連続の変化量。時刻/風/露出は固定されないので画質PASSを自動判定しない。
- request_dlss_snapshot.py <launch> --frame 1000 [--count 4]: -MockNavigate -EyeImageSweep -DlssObserveが前提。navigation指示を送らなければpose CSVの頭姿勢を保持。実際のNGXハンドルを同起動inventoryへ対応付け、8画像を選ぶ。V1は1frame、V2は明示2..4連続。1起動1request、各16image/256MiBが上限。診断は重いので2分でframe2400へ届かなかった失敗あり、早い要求を使う。
- inspect_dlss_snapshot.py <launch> [--frame N]: rawを保ち共通linear→sRGB表示で色前後を見る。R32_SFLOAT/D32両方の深度、SNORM motionを対応。表示変換は実ゲームの色処理と同一ではない。
- inspect_dlss_motion_geometry.py <launch> [--frame N]: 実測poseから静止世界のMVを計算し比較。草のアニメーションは仮定に含まない。
- inspect_dlss_raw_reprojection.py <launch>: 固定ホームの4raw連続で草/床の前画像対応を比べる。ROI、弓の混入、jitter候補、遅い診断間隔の制約を報告。残像なしの自動証明には使わない。
- 試験中のhooks/layer/runtime/include編集禁止。ゲームとRenderDoc replayを重ねない。チェック結果と画像品質を分け、失敗/未達もdocs/analysis/home_clarity_20261010/notes.mdへ残す。
