#!/usr/bin/env python3
"""Put a managed save snapshot back through Steam's own storage interface (2026-10-18).

Why: a direct (Mock) start of the game reads its saves through Steam (ISteamRemoteStorage). Steam remembers each file as
the game last wrote it; a file copied back behind Steam's back (restore_scenery_backup.ps1) then has another size, the
read fails ("[savedata2] Steam file 'world1.ksd' read error (read:X size:Y)") and the game quietly loads the newest
backup revision - the end of the previous Mock run. (A start from Steam syncs first and does not have the problem.)
The user, 2026-10-18 11:30: "you are checking shadows with a save at a time and place clearly unsuited for it".
Here the snapshot's save files (world<n>.ksd, character<n>.ksd, global.ksd - not user_settings.json, the game's own
settings) that differ from the
current ones are written with ISteamRemoteStorage::FileWrite - as the game itself writes them - so Steam's record and
the file agree and the next direct start loads that state. The game's own backup\\ and migrated\\ folders are left alone.
Run with the game closed and Steam running; back the current saves up first (backup_game_saves.ps1).
  python -I vrmod/tools/steam_save_write.py vrmod/output/save-backups/<snapshot> [--dry-run]
--settings also takes the snapshot's user_settings.json, --only NAME limits it to the files named (2026-10-20: an old-style
restore - run_play_session.ps1 -SaveReference through restore_scenery_backup.ps1 - had copied a 10-17 user_settings.json
behind Steam's back, with the session mode Public instead of the player's Friends; it was put back from the backup made
just before: --settings --only user_settings.json).
"""
import argparse, ctypes, hashlib, json, os, pathlib, re, subprocess, sys

APP = '1203620'


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('snapshot', type=pathlib.Path)
    p.add_argument('--dry-run', action='store_true')
    p.add_argument('--settings', action='store_true', help='also user_settings.json')
    p.add_argument('--only', action='append', default=[], help='only this file (repeatable)')
    a = p.parse_args()
    game = pathlib.Path(__file__).resolve().parents[2]
    snap = a.snapshot.resolve()
    if not str(snap).lower().startswith(str((game / 'vrmod/output/save-backups').resolve()).lower()):
        raise SystemExit('the snapshot must be a managed save backup')
    meta = json.loads((snap / 'backup.json').read_text(encoding='utf-8-sig'))
    manifest = json.loads((snap / 'manifest.json').read_text(encoding='utf-8-sig'))
    if not meta.get('verified'):
        raise SystemExit('the snapshot is not verified')
    remote = pathlib.Path(meta['source'])
    if subprocess.run(['tasklist', '/FI', 'IMAGENAME eq enshrouded.exe'], capture_output=True, text=True).stdout.lower().count('enshrouded.exe'):
        raise SystemExit('close the game first')
    files = []
    for e in manifest:
        rel = e['relative']
        if re.search(r'[\\/]', rel) or not (re.fullmatch(r'[A-Za-z0-9_-]+\.ksd', rel) or (a.settings and rel == 'user_settings.json')):
            continue
        if a.only and rel not in a.only:
            continue
        data = (snap / 'remote' / rel).read_bytes()
        if hashlib.sha256(data).hexdigest().upper() != e['sha256'].upper():
            raise SystemExit(f'{rel}: the snapshot does not match its manifest')
        current = (remote / rel).read_bytes() if (remote / rel).exists() else None
        files.append((rel, data, current))
    changed = [(r, d) for r, d, c in files if c != d]
    print(f'snapshot {snap.name}: {len(files)} flat files, {len(changed)} differ:', ', '.join(f'{r} ({len(d)} bytes)' for r, d in changed))
    if a.dry_run or not changed:
        return
    # The current saves first, every time (2026-10-18 12:44: a write right after the user's own play would have replaced
    # it with nothing kept but the game's own backup\ revisions).
    shell = next((p for p in (pathlib.Path(os.environ.get('LOCALAPPDATA', '')).parent.parent / '.cache/codex-runtimes/codex-primary-runtime/dependencies/native/powershell/pwsh.exe',
                              pathlib.Path(os.environ.get('SystemRoot', 'C:/Windows')) / 'System32/WindowsPowerShell/v1.0/powershell.exe') if p.exists()), None)
    if not shell:
        raise SystemExit('no PowerShell for the save backup')
    kept = subprocess.run([str(shell), '-NoProfile', '-ExecutionPolicy', 'Bypass', '-Command',
                           f"& '{game / 'vrmod/tools/backup_game_saves.ps1'}' -Source '{remote}' -PassThru | ConvertTo-Json -Compress"],
                          capture_output=True, text=True, cwd=str(game))
    try:
        kept_record = json.loads(kept.stdout.strip().splitlines()[-1])
    except Exception:
        raise SystemExit(f'the current saves could not be backed up: {kept.stdout[-300:]} {kept.stderr[-300:]}')
    if not kept_record.get('verified'):
        raise SystemExit('the current saves backup did not verify')
    print(f'current saves kept at {kept_record["path"]}')
    os.environ['SteamAppId'] = APP; os.environ['SteamGameId'] = APP
    os.add_dll_directory(str(game))
    api = ctypes.WinDLL(str(game / 'steam_api64.dll'))
    api.SteamAPI_InitFlat.restype = ctypes.c_int   # ESteamAPIInitResult: 0 OK (the game's steam_api64.dll has no plain SteamAPI_Init export)
    api.SteamAPI_InitFlat.argtypes = [ctypes.c_char_p]
    api.SteamAPI_SteamRemoteStorage_v016.restype = ctypes.c_void_p
    api.SteamAPI_ISteamRemoteStorage_FileWrite.restype = ctypes.c_bool
    api.SteamAPI_ISteamRemoteStorage_FileWrite.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_void_p, ctypes.c_int32]
    api.SteamAPI_ISteamRemoteStorage_GetFileSize.restype = ctypes.c_int32
    api.SteamAPI_ISteamRemoteStorage_GetFileSize.argtypes = [ctypes.c_void_p, ctypes.c_char_p]
    message = ctypes.create_string_buffer(1024)
    if (result := api.SteamAPI_InitFlat(message)) != 0:
        raise SystemExit(f'SteamAPI_InitFlat failed ({result}: {message.value.decode(errors="replace")}; is Steam running?)')
    try:
        storage = api.SteamAPI_SteamRemoteStorage_v016()
        if not storage:
            raise SystemExit('no remote storage interface')
        for rel, data in changed:
            before = api.SteamAPI_ISteamRemoteStorage_GetFileSize(storage, rel.encode())
            buf = ctypes.create_string_buffer(data, len(data))
            ok = api.SteamAPI_ISteamRemoteStorage_FileWrite(storage, rel.encode(), buf, len(data))
            after = api.SteamAPI_ISteamRemoteStorage_GetFileSize(storage, rel.encode())
            disk = (remote / rel).read_bytes()
            print(f'{rel}: steam size {before} -> {after}, written {ok}, file on disk matches {disk == data}')
            if not ok or after != len(data) or disk != data:
                raise SystemExit(f'{rel}: not written')
    finally:
        api.SteamAPI_Shutdown()
    print('PASS: the snapshot is the current save, as Steam knows it')


if __name__ == '__main__':
    main()
