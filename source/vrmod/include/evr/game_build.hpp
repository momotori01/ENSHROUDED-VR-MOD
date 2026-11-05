// The build of enshrouded.exe the mod's fixed addresses belong to (2026-10-16: the 1.0 update moved every address;
// until then the three numbers were written into ten files). The loader, the layer and the hooks compare the loaded
// image's PE header with these and stand aside when it is another build ("Off: this version of the game is not the one
// the mod was made for"). A new build: tools/match_functions.py maps the hooks' addresses, then these change.
#pragma once
#include <cstdint>
namespace evr::gameBuild {
// Enshrouded, Steam build 26430625 (2026-11-05 00:19, a small update after build 26320373); SHA-256 of enshrouded.exe
// 5d9ef2f140b8ed49e30dbee655841b89d08baec20f799a1817e171a18a850c77 (a copy in vrmod/output/exe-backups/5d9ef2f1-20261105-build26430625/;
// the addresses carried from build 26320373 by tools/shift_rematch.py).
constexpr uint32_t timeDateStamp=1793791052u;   // IMAGE_FILE_HEADER.TimeDateStamp
constexpr uint32_t sizeOfImage=44298240u;       // IMAGE_OPTIONAL_HEADER64.SizeOfImage (0x2a3f000)
constexpr const char* sha256Prefix="5d9ef2f140b8";
// Build 26320373 (2026-11-02 19:44, SHA-256 0403981fd2e8...): TimeDateStamp 1793351710, SizeOfImage 44302336. Its executable
// is kept in vrmod/output/exe-backups/0403981f-20261102-build26320373/.
// Build 26082781 (2026-10-21 19:21, SHA-256 d64966e23b1a...): TimeDateStamp 1792507660, SizeOfImage 44163072. Its executable
// is kept in vrmod/output/exe-backups/d64966e2-20261021-build26082781/.
// Build 25988036 (2026-10-16 23:56, a hotfix after 1.0.0.1, SHA-256 82e5020a8736...): TimeDateStamp 1792148057, SizeOfImage
// 44146688. Its executable is kept in vrmod/output/exe-backups/82e5020a-20261017-build25988036/.
// Hotfix 1.0.0.1 (Steam build 25966634, 2026-10-16 09:33, SHA-256 4d6d43ea4a91...): TimeDateStamp 1792083981, SizeOfImage
// 44142592. Its executable is kept in vrmod/output/exe-backups/4d6d43ea-20261016-hotfix-1.0.0.1/.
// 1.0 (Steam build 25936191, 2026-10-16 morning, SHA-256 b331a6d037c5...): TimeDateStamp 1791978644, SizeOfImage 44142592.
// Its executable was replaced by Steam before a copy was kept; its function features are in
// vrmod/output/hook-rematch-20261016-hotfix/cache-b331a6d0.pkl.
// The build before it (June 30 2026, Steam build 23966345, SHA-256 af2f5a1227911d8a...): TimeDateStamp 1782724296,
// SizeOfImage 47869952. Its executable is kept in vrmod/output/exe-backups/af2f5a12-20260630/.
}
