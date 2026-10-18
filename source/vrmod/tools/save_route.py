#!/usr/bin/env python3
"""Keep a walk that the user made in a Mock session, for the mod's own tests (2026-10-14, user: "save this route for
self-tests"; then "record the mouse's turning too; I press F8 where the faults show, and the self-test turns round
there taking pictures").

  save_route.py <game folder under vrmod/output> <start save backup name> <route name> [--note "..."]
  save_route.py --check-only <route name>     (re)write replay.keys (from input.jsonl), check.keys, check-head.csv

Writes vrmod/test_env/routes/<route name>/:
  route.json      the start save (a folder of vrmod/output/save-backups), the game folder it came from, the note, the
                  spots, the time of a frame, the minutes a replay and a self-test take
  track.csv       XR frame, the anchor's world position (metres, as the game's Q32 / 2^32) and heading (degrees), every
                  15th frame (recorded_xr_anchor of the probe log; the probe logs at most -LogNameCap lines of it)
  pictures.csv    the frames where pictures of both eyes were taken (F8 or a request) and where the player stood then
  spots.csv       the frames where the user pressed F8 - the places where the faults show - and where the player stood
  input.jsonl     every keyboard and mouse record the game read, by XR frame (EVR_INPUT_RECORD=1, hooks/key_script.hpp)
  keys.jsonl      the keys the user pressed while the game was in front (record_keyboard_operations.py; older routes)
  head.csv        the Mock head script of the session (the head is still in it)
  replay.keys     the walk as a Mock input script (EVR_KEY_SCRIPT): every key and every mouse movement at its XR frame.
                  With the start save and head.csv it walks and turns as the user did; STOP ends the session where the
                  user's ended
  check.keys      the same walk for the self-test: at every spot the input stops (the keys held are let go), the head
                  turns round in steps of 30 degrees - once 5 degrees down, once 15 up, as tools/make_scenery_survey_
                  fixture.py - with one picture of both eyes in every direction, then the keys are pressed again and the
                  walk goes on (all later steps later by the time spent there)
  check-head.csv  the Mock head script for check.keys
tools/run_route_check.ps1 runs the self-test (or the plain replay), tools/check_route_run.py reads its pictures.
Reads only; copies, never moves or deletes anything of the game folder.
"""
import argparse, json, math, pathlib, shutil, statistics

root = pathlib.Path('S:/SteamLibrary/steamapps/common/Enshrouded')
FUNCTION_KEYS = range(0x70, 0x88)        # F1..F24: F8 took the pictures, the layer reads it by itself
# Keys a replay must not press: the Windows keys and Alt (the user leaving the game window: Windows key, Alt + Tab).
LEAVE_KEYS = {0x5b, 0x5c, 0x5d, 0x12, 0xa4, 0xa5}
DWELL, DIRECTIONS, PITCHES, SETTLE, SHOT = 60, 12, (-5., 15.), 30, 20
STARTUP_MINUTES = 2.0                    # from the launch to the first XR frame, and the end
STOP_AFTER = 300                         # frames from the last step to the end of a replay


def anchor_lines(game):
    probe = sorted(game.glob('view-resource-probe-*'))[-1] / 'view-resource-probe.jsonl'
    for line in open(probe, encoding='utf-8'):
        if '"recorded_xr_anchor"' in line: yield json.loads(line)


def heading(q):
    qx, qy, qz, qw = q
    return math.degrees(math.atan2(2 * (qw * qy + qx * qz), 1 - 2 * (qy * qy + qx * qx)))


def layer_events(game, names):
    xr = game / 'native-xr/openxr.jsonl'
    if not xr.exists(): return
    for line in open(xr, encoding='utf-8'):
        if any('"%s"' % n in line for n in names): yield json.loads(line)


def near(track, f):
    t = min(track, key=lambda t: abs(t[0] - f)); return (f,) + t[1:]


def csv_rows(rows):
    return 'frame,x,y,z,heading_deg\n' + ''.join('%d,%.3f,%.3f,%.3f,%.1f\n' % t for t in rows)


def replay_from_input(path):
    """input-record.jsonl -> script lines (frame, text) in the order read."""
    out = []
    for line in open(path, encoding='utf-8'):
        e = json.loads(line)
        if 'mouse' in e:
            dx, dy = e['mouse']
            if dx or dy: out.append((e['frame'], 'MOUSE %d %d' % (dx, dy)))
        elif 'vkey' in e and e['vkey'] not in FUNCTION_KEYS and e['vkey'] not in LEAVE_KEYS:
            out.append((e['frame'], 'RAWKEY %d %d %d' % (e['make'], e['flags'], e['vkey'])))
    return out


def replay_from_keyboard(path, game):
    """keyboard-operations.jsonl (record_keyboard_operations.py) -> script lines, its times turned into XR frames."""
    clock = sorted((e['us'], e['recorded_xr_frame']) for e in anchor_lines(game))
    def frame_at(us):
        if us <= clock[0][0]: return clock[0][1]
        for (u0, f0), (u1, f1) in zip(clock, clock[1:]):
            if u0 <= us <= u1: return f0 + round((us - u0) / max(1, u1 - u0) * (f1 - f0))
        return clock[-1][1]
    allowed = set('WASDQEFRZXC') | {'SPACE', 'SHIFT', 'SHIFT_LEFT', 'CTRL', 'CTRL_LEFT', 'ESC', 'ENTER', 'TAB', 'UP', 'DOWN', 'LEFT', 'RIGHT'}
    out = []
    for line in open(path, encoding='utf-8'):
        e = json.loads(line)
        if e.get('event') != 'key' or e.get('key') not in allowed: continue
        name = {'SHIFT_LEFT': 'SHIFT', 'CTRL_LEFT': 'CTRL'}.get(e['key'], e['key'])
        out.append((frame_at(e['us']), '%s %s' % (name, 'down' if e['down'] else 'up')))
    return out


def read_script(path):
    out = []
    for line in path.read_text(encoding='utf-8').splitlines():
        line = line.split('#')[0].strip()
        if not line: continue
        f, text = line.split(None, 1); out.append((int(f), text))
    return out


def key_state(text):
    """(identity, True when pressed / False when let go, the text that lets it go) or None (mouse, a tap)."""
    w = text.split()
    if w[0] == 'RAWKEY':
        make, flags, vkey = int(w[1]), int(w[2]), int(w[3])
        return ('raw', make, flags & 2), not (flags & 1), 'RAWKEY %d %d %d' % (make, flags | 1, vkey)
    if len(w) == 2 and w[1] in ('down', 'up'):
        return ('named', w[0]), w[1] == 'down', '%s up' % w[0]
    return None


def build_check(steps, spots, head_rows):
    """The self-test: the walk with a look round at every spot. -> (script lines, head rows, spot plans, end frame)."""
    held = {}; lines = []; shift = 0; plans = []; turns = []; queue = sorted(spots)
    def look_round(spot):
        nonlocal shift
        start = spot + shift + 1
        for ident, (press, release) in held.items(): lines.append((start, release))
        t = start + SETTLE; shots = []
        for pitch in PITCHES:
            for k in range(DIRECTIONS):
                turns.append((t, t + DWELL, 360. / DIRECTIONS * k, pitch))
                lines.append((t + SHOT, 'CAPTURE')); shots.append(dict(frame=t + SHOT, turn=360. / DIRECTIONS * k, pitch=pitch))
                t += DWELL
        for ident, (press, release) in held.items(): lines.append((t, press))
        plans.append(dict(spot=spot, check_frame=spot + shift, look_from=start, look_to=t, held=len(held), pictures=shots))
        shift += t - start
    for f, text in steps:
        while queue and queue[0] < f: look_round(queue.pop(0))
        lines.append((f + shift, text))
        s = key_state(text)
        if s:
            ident, down, release = s
            if down: held[ident] = (text, release)
            else: held.pop(ident, None)
    while queue: look_round(queue.pop(0))
    end = max([f for f, _ in lines] + [p['look_to'] for p in plans]) if lines or plans else 0
    base = head_rows[0]; rows = []
    for frame in range(1, end + 900):
        r = list(base); r[0] = str(frame)
        for a, b, turn, pitch in turns:
            if a <= frame < b:
                r[4] = '%g' % (float(base[4]) + turn); r[5] = '%g' % pitch; break
        rows.append(','.join(r))
    return sorted(lines, key=lambda t: t[0]), rows, plans, end


def head_of(path):
    text = path.read_text(encoding='utf-8').splitlines()
    return [l for l in text if l.startswith('#')][:1], [l.split(',') for l in text if l and not l.startswith('#')]


def write_check(out, info):
    steps = read_script(out / 'replay.keys')
    spots = [int(l.split(',')[0]) for l in (out / 'spots.csv').read_text(encoding='utf-8').splitlines()[1:] if l]
    if not any(t == 'STOP' for _, t in steps):
        # The replay ends the session a few seconds after the last key, mouse movement or spot (hooks/key_script.hpp:
        # STOP -> <probe>/stop.request) - not where the user's ended, often long after.
        stop = max([f for f, _ in steps] + spots) + STOP_AFTER
        with open(out / 'replay.keys', 'a', encoding='utf-8') as f: f.write('%d STOP\n' % stop)
        steps.append((stop, 'STOP'))
        info['replay_minutes'] = math.ceil(stop * ((info.get('frame_ms') or 30.) + 3.) / 60000. + STARTUP_MINUTES + 1)
    comment, head = head_of(out / 'head.csv')
    lines, rows, plans, end = build_check(steps, spots, head)
    (out / 'check.keys').write_text('# Self-test of route %s (tools/save_route.py): the walk of replay.keys, at every spot (F8) a look round with %d pictures.\n'
                                    % (out.name, len(PITCHES) * DIRECTIONS) + ''.join('%d %s\n' % t for t in lines), encoding='utf-8')
    (out / 'check-head.csv').write_text('\n'.join(['# Self-test head of route %s: still, turning round at the spots (tools/save_route.py).' % out.name] + rows) + '\n',
                                        encoding='utf-8', newline='\n')
    ms = info.get('frame_ms') or 30.
    info['check'] = dict(keys='check.keys', head='check-head.csv', end_frame=end, spots=plans, pictures=sum(len(p['pictures']) for p in plans),
                         minutes=math.ceil(end * (ms + 3.) / 60000. + STARTUP_MINUTES + 1))
    (out / 'route.json').write_text(json.dumps(info, ensure_ascii=False, indent=1) + '\n', encoding='utf-8')
    print('check', end, 'frames', info['check']['pictures'], 'pictures', info['check']['minutes'], 'minutes')


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('game', nargs='?'); p.add_argument('save', nargs='?'); p.add_argument('name', nargs='?')
    p.add_argument('--note', default=''); p.add_argument('--check-only')
    a = p.parse_args()
    if a.check_only:
        out = root / 'vrmod/test_env/routes' / a.check_only; info = json.loads((out / 'route.json').read_text(encoding='utf-8'))
        game = root / 'vrmod/output' / info['game']
        if not (out / 'head.csv').exists(): shutil.copyfile(game / 'mock-pose.csv', out / 'head.csv')
        if (out / 'input.jsonl').exists():   # the replay again from the record (the keys left out may have changed)
            head = "# The user's input of %s by XR frame (tools/save_route.py); start from save %s with head.csv.\n" % (info['game'], info['save'])
            (out / 'replay.keys').write_text(head + ''.join('%d %s\n' % t for t in replay_from_input(out / 'input.jsonl')), encoding='utf-8')
        if not (out / 'spots.csv').exists():
            track = [tuple(float(v) for v in l.split(',')) for l in (out / 'track.csv').read_text(encoding='utf-8').splitlines()[1:]]
            spots = [near(track, e['frame']) for e in layer_events(game, ['manual_key_capture'])]
            (out / 'spots.csv').write_text(csv_rows(spots), encoding='utf-8'); info['spots'] = [s[0] for s in spots]
        write_check(out, info); return
    if not (a.game and a.save and a.name): p.error('game, save and name')
    game = root / 'vrmod/output' / a.game; save = root / 'vrmod/output/save-backups' / a.save
    if not (save / 'backup.json').exists(): raise SystemExit('no save backup ' + a.save)
    out = root / 'vrmod/test_env/routes' / a.name
    if out.exists(): raise SystemExit('route exists already: ' + str(out))
    anchors = list(anchor_lines(game))
    track = [(e['recorded_xr_frame'],) + tuple(v / 4294967296.0 for v in e['camera_q32']) + (heading(e['orientation']),) for e in anchors if e['recorded_xr_frame'] % 15 == 0]
    if not track: raise SystemExit('no anchor track in ' + a.game)
    steps_us = [(b['us'] - a_['us']) / 1000. / max(1, b['recorded_xr_frame'] - a_['recorded_xr_frame']) for a_, b in zip(anchors, anchors[1:]) if b['recorded_xr_frame'] > a_['recorded_xr_frame']]
    frame_ms = round(statistics.median(steps_us), 2) if steps_us else 30.
    pictures = [near(track, e['frame']) for e in layer_events(game, ['native_fixture_capture_complete', 'native_burst_capture_complete'])]
    spots = [near(track, e['frame']) for e in layer_events(game, ['manual_key_capture'])]
    if (game / 'input-record.jsonl').exists(): script = replay_from_input(game / 'input-record.jsonl')
    elif (game / 'keyboard-operations.jsonl').exists(): script = replay_from_keyboard(game / 'keyboard-operations.jsonl', game)
    else: raise SystemExit('no record of the input in ' + a.game)
    out.mkdir(parents=True)
    (out / 'track.csv').write_text(csv_rows(track), encoding='utf-8')
    (out / 'pictures.csv').write_text(csv_rows(pictures), encoding='utf-8')
    (out / 'spots.csv').write_text(csv_rows(spots), encoding='utf-8')
    for source, target in (('input-record.jsonl', 'input.jsonl'), ('keyboard-operations.jsonl', 'keys.jsonl'), ('mock-pose.csv', 'head.csv')):
        if (game / source).exists(): shutil.copyfile(game / source, out / target)
    head = "# The user's input of %s by XR frame (tools/save_route.py); start from save %s with head.csv.\n" % (a.game, a.save)
    (out / 'replay.keys').write_text(head + ''.join('%d %s\n' % t for t in script), encoding='utf-8')
    last = max([script[-1][0] if script else 0, track[-1][0]])
    info = dict(save=a.save, game=a.game, note=a.note, frames=[track[0][0], track[-1][0]], frame_ms=frame_ms, pictures=len(pictures),
                spots=[s[0] for s in spots], mouse=(game / 'input-record.jsonl').exists(), key_script='replay.keys', head='head.csv',
                replay_minutes=math.ceil(last * frame_ms / 60000. + STARTUP_MINUTES))
    print(out, 'track', len(track), 'pictures', len(pictures), 'spots', len(spots), 'steps', len(script), 'frame_ms', frame_ms)
    write_check(out, info)


if __name__ == '__main__':
    main()
