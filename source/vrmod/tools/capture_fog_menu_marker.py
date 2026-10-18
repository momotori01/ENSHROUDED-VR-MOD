"""Read-only menu marker detection, bounded submitted-eye captures, then ordinary managed stop."""
import argparse, json, time
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('launch',type=Path);p.add_argument('--timeout',type=int,default=450)
a=p.parse_args();end=time.monotonic()+a.timeout
assert a.launch.is_dir()
output=a.launch/'fog-menu-recording.json'
assert not output.exists()
streams={};normal_seen=False;opened=None;marker=None;started=None;probe=None;ready=False
requests=[];receipts=[];completed=[];single=[];events=[];closed=False;next_single=0
burst_times=[1,8,18,30]
def log_event(kind,**fields):
 row=dict(kind=kind,wall_unix=time.time(),**fields);events.append(row);print(json.dumps(row),flush=True)
def write_report(status):
 output.write_text(json.dumps(dict(status=status,marker=marker,requests=requests,receipts=receipts,completed=completed,single_requests=single,events=events,limitation='Captures wait for GPU completion. Menu timing identifies user location, not image quality.'),indent=2)+'\n',encoding='utf-8')
while time.monotonic()<end:
 candidates=sorted(a.launch.glob('view-resource-probe-*/world-session.txt'))
 if candidates:probe=candidates[-1].parent
 paths=[a.launch/'native-xr/openxr.jsonl']
 if probe:paths.append(probe/'view-resource-probe.jsonl')
 for path in paths:
  if not path.exists():continue
  offset,fragment=streams.get(str(path),(0,b''))
  with path.open('rb') as f:f.seek(offset);data=fragment+f.read();offset=f.tell()
  lines=data.split(b'\n');streams[str(path)]=(offset,lines.pop())
  for line in lines:
   try:e=json.loads(line)
   except (ValueError,UnicodeError):continue
   kind=e.get('event')
   if kind=='native_game_pair_submitted':ready=True
   if kind=='native_xr_closed':closed=True
   if kind=='manual_xr_burst':receipts.append(e)
   if kind=='native_burst_capture_complete' and e.get('index')==32:completed.append(e);log_event('burst_complete',frame=e['frame'],count=len(completed))
   if kind=='root_camera_ui_hold_begin':log_event('camera_hold_begin',sample=e.get('sample'),serial=e.get('serial'))
   if kind=='root_camera_ui_hold_end':log_event('camera_hold_end',milliseconds=e.get('milliseconds'),serial=e.get('serial'))
   if kind=='input_ui_native_sample' and ready and e.get('readable') and e.get('stable_read') and e.get('world')!='0x0':
    normal=e.get('hud_screen')==1 and e.get('auxiliary_state')==0
    stamp=e.get('us',0)
    if normal:
     normal_seen=True
     if opened is not None:
      elapsed=(stamp-opened['us'])/1e6
      log_event('menu_closed',duration_seconds=elapsed)
      if elapsed>=3.5 and marker is None:
       marker=dict(open=opened,close=e,duration_seconds=elapsed);started=time.monotonic();log_event('fog_marker_accepted',duration_seconds=elapsed)
      opened=None
    elif normal_seen and opened is None:
     opened=e;log_event('menu_opened',screen=e.get('hud_screen'))
 if closed:write_report('CLOSED');break
 if started is not None:
  elapsed=time.monotonic()-started
  burst=a.launch/'xr-burst.request';shot=a.launch/'xr-capture.request'
  if burst_times and elapsed>=burst_times[0] and len(completed)==len(requests) and not burst.exists() and not shot.exists():
   due=burst_times.pop(0);burst.write_text('32 4',encoding='ascii');requests.append(dict(seconds=elapsed,scheduled=due));log_event('burst_requested',seconds=elapsed)
  elif elapsed<60 and elapsed>=next_single and len(completed)==len(requests) and not burst.exists() and not shot.exists():
   shot.touch();single.append(dict(seconds=elapsed));next_single=elapsed+1.5
  if elapsed>=70 and len(completed)==4:
   (probe/'stop.request').write_text('fog recording complete\n',encoding='ascii');log_event('ordinary_stop_requested');write_report('CAPTURED_STOP_REQUESTED');break
 write_report('RECORDING' if started else 'WAITING_FOR_MENU')
 time.sleep(.15)
else:write_report('TIMEOUT')
