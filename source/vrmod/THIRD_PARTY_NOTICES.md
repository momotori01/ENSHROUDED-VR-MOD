# 参照・依存ライセンス

出典と固定commitは `../references/manifest.json` に記録しています。
この文書はライセンス本文の代替ではありません。配布時には該当本文・著作権表示も含めます。

- Khronos OpenXR-SDK: Apache-2.0等、`references/OpenXR-SDK/LICENSE` / `COPYING.adoc`。
  loader DLLとheadersを使用。
- Khronos OpenXR-SDK-Source: 各ソースの表示と `LICENSE` / `COPYING.adoc`。
  独立テストrendererがhello_xrの `vulkan_shaders/vert.spv` / `frag.spv` をincludeする。
  このshaderはゲームのshaderではない。
- Vulkan-Headers: 各headerの表示と `references/Vulkan-Headers/LICENSE.md`。
  `layer/feature_metadata.hpp` のfield一覧・offset定義はvendored `registry/vk.xml` から生成。
  元registryのCopyright 2015-2026 The Khronos Group Inc.、Apache-2.0 OR MIT表示を保持。
- Valve OpenVR: BSD-3-Clause、`references/openvr/LICENSE`。
  自作virtual HMD driverが公開driver interfaceを使用。制御utilityはopenvr_apiへリンク。
- Shroudtopia: MIT、`references/shroudtopia/LICENSE`。FirstPersonViewを参照したが既存DLLを導入していない。
- kfc-parser: GPL-3.0、`references/kfc-parser/LICENSE`。
  `tools/analyze_game.py` はmetadata layout研究に基づくためGPL-3.0-or-later表示で分離。
  kfc-parserの実装をMODのC++ランタイムへリンクしていない。
- pefile、Capstone、CMake、clang-format: `tools/python` 内各packageのライセンス表示を保持。
  依存バージョンは `tools/requirements.txt`。

Nexus配布物は未取得・未組込。転載許諾を取得したとは扱いません。
Enshrouded executable、ゲームasset、捕捉SPIR-V、memory dump、save dataは再配布しません。
公開する場合はreference由来ライセンス適合を個別に確認してから配布対象を選択します。

- MinHook / bundled HDE: BSD-2-Clause等、references/minhook/LICENSE.txtを保持。commit `8af6b4acae5a9388fd742b56fa79ece89d96f823`。in-process observerと独立fixtureで使用。
- Unicorn 2.1.4: CPU engineのGPL-2.0文書を `references/unicorn/COPYING` に保存。Python packageのBSD metadataだけをengine全体のライセンスと扱わない。オフライン解析用で、C++ MODへリンク・同梱していない。入手先: https://github.com/unicorn-engine/unicorn/tree/2.1.4 、https://pypi.org/project/unicorn/2.1.4/ 。
