"""Reject unrecognized GI presets before changing settings or launching the game.
The names/values below are from the reviewed local RenderGiSetting table.
This is a GI preset check, not validation of all game settings.
"""
import argparse,hashlib,json
from pathlib import Path

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('settings',type=Path);a=p.parse_args()
 data=json.loads(a.settings.read_text(encoding='utf-8-sig'));value=data['graphics']['render']['dynamicGi']
 allowed={'Performance':0,'Balance':1,'High':2,'Ultra':3}
 exe=Path(__file__).resolve().parents[2]/'enshrouded.exe'
 sha=hashlib.file_digest(exe.open('rb'),'sha256').hexdigest() if hasattr(hashlib,'file_digest') else hashlib.sha256(exe.read_bytes()).hexdigest()
 # June af2f5a12 (enum table 0x16116d0); 1.0 build 25988036 82e5020a (keen::pc::RenderGiSetting at 0x1b34cf0, the same four values; build 26082781 d64966e2 at 0x1b37040,
 # read 2026-10-17 with reflection_enum_by_name.py)
 reviewed={'af2f5a1227911d8aa06b3908d6bd0211838211cae14ea91099cb57d0df990781':'0x16116d0','82e5020a8736348cd0374f004b2f5e5ef456b5bd27aa5420df34c7dd241cfeeb':'0x1b34cf0','d64966e23b1a458ee6f147bcbceac0b577fa7e39a212f371287d8cc119817882':'0x1b37040'}
 if sha not in reviewed:raise ValueError('Executable changed; recheck RenderGiSetting table before using this validator')
 ok=value in allowed
 print(json.dumps(dict(status='PASS' if ok else 'FAIL',setting='dynamicGi',value=value,allowed=list(allowed),exe_sha256=sha,enum_table_rva=reviewed[sha],scope='GI preset only; no settings or process writes')))
 return int(not ok)
if __name__=='__main__':raise SystemExit(main())
