"""Static, local-only comparison. Never loads the downloaded mod DLL."""
import hashlib
import json
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).parent / 'python'))
import pefile

root = Path(__file__).resolve().parents[2]
folder = root / 'references/first-person-review-20260913'
pattern = bytes.fromhex('F3 0F 11 9F C4 06 00 00')
payload = bytes.fromhex('C7 87 C4 06 00 00 00 00 00 00 C7 87 C8 06 00 00 00 00 80 BF')


def offsets(data, needle):
    result = []
    at = data.find(needle)
    while at >= 0:
        result.append(at)
        at = data.find(needle, at + 1)
    return result


files = {}
for name in ('Shroudtopia-FirstPersonView.cpp', 'Shroudtopia-LICENSE', 'first_person_view-0.0.3.dll'):
    data = (folder / name).read_bytes()
    files[name] = dict(size=len(data), sha256=hashlib.sha256(data).hexdigest())
game = (root / 'enshrouded.exe').read_bytes()
pe = pefile.PE(data=game)
sections = []
for section in pe.sections:
    if section.Characteristics & 0x20000000:
        sections.append(dict(name=section.Name.rstrip(b'\0').decode(),
                             matches=[hex(section.VirtualAddress + x) for x in offsets(section.get_data(), pattern)]))
mod_data = (folder / 'first_person_view-0.0.3.dll').read_bytes()
mod = pefile.PE(data=mod_data)
result = dict(
    retrieved='2026-09-13', source_commit=json.loads((folder / 'upstream-main.json').read_text())['sha'],
    files=files, game_sha256=hashlib.sha256(game).hexdigest(),
    game_executable_section_matches=sections,
    source_pattern=pattern.hex(' '), binary_pattern_file_offsets=[hex(x) for x in offsets(mod_data, pattern)],
    binary_override_payload_file_offsets=[hex(x) for x in offsets(mod_data, payload)],
    binary_exports=[x.name.decode() for x in mod.DIRECTORY_ENTRY_EXPORT.symbols if x.name],
    binary_import_dlls=[x.dll.decode() for x in mod.DIRECTORY_ENTRY_IMPORT],
    limitation='Static byte/PE inspection, not source-to-binary equivalence or runtime validation. No game or mod execution.')
(folder / 'static-comparison.json').write_text(json.dumps(result, indent=2), encoding='utf8')
print(json.dumps(result, indent=2))
