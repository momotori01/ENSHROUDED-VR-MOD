#!/usr/bin/env python3
"""Walk the Mock character to places by its position: bounded left-stick commands steered by the live root camera log.

Open-loop stick scripts cover a distance that depends on the frame rate (0.09 to 0.15 m a frame), which is too coarse
to stand in front of one tree. This tool reads the character's position from the running session
(root_camera_sample of the live probe) and writes Mock navigation commands (the file format of mock_navigate.py,
each command expires by itself within 0.4 s). The session must be an opted-in Mock launch with the hand script kept
(-MockNavigate of run_home_equipment_trial.ps1): the hand and head scripts go on as written, only the left stick is
added while a command is active. Nothing of the player, the save or physics is written.

Positions are metres from the first root camera sample of the session: x to the right and z forward as the character
stands at the home spawn. The stick moves the character relative to the VR view, whose yaw is the XR anchor
(recorded_xr_anchor.orientation) and not the native camera: the native camera follows the walking direction by up to
40 degrees while the anchor stays put (game160843 walked 37 degrees off with the native yaw). The head script is
assumed to keep its first yaw during the walk.

   mock_walk_to.py <game folder> --points "6.3,0.3;6.3,9;6.41,26" --push "0,1,4"

--push "dx,dz,seconds": after the last point walk slowly in that direction until the character stops moving (it is
against something) or the seconds are over.
"""
import argparse, json, math, os, time
from pathlib import Path


def tail_sample(path, size=262144):
    """The newest root_camera_sample of a growing log with the newest XR anchor orientation, or None."""
    with open(path, 'rb') as f:
        f.seek(0, 2); end = f.tell(); f.seek(max(0, end - size)); data = f.read()
    sample = anchor = None
    for line in reversed(data.split(b'\n')):
        try:
            if sample is None and b'"event":"root_camera_sample"' in line:
                sample = json.loads(line)
            elif anchor is None and b'"event":"recorded_xr_anchor"' in line:
                anchor = json.loads(line)['orientation']
        except (ValueError, KeyError):
            continue
        if sample is not None and anchor is not None:
            break
    if sample is not None:
        sample['anchor_orientation'] = anchor or [0, 0, 0, 1]
    return sample


def first_sample(path):
    with open(path, 'rb') as f:
        for line in f:
            if b'"event":"root_camera_sample"' in line:
                try:
                    return json.loads(line)
                except ValueError:
                    return None
    return None


class Session:
    def __init__(self, launch):
        self.launch = launch
        record = json.loads((launch / 'launch.json').read_text(encoding='utf-8-sig'))
        if record.get('runtime') != 'mock' or not record.get('mock_navigation') or not record.get('mock_navigation_hand_input'):
            raise SystemExit('requires a Mock launch with -MockNavigate (hand script kept)')
        self.file = launch / 'mock-navigation.txt'
        if not self.file.is_file() or self.file.is_symlink():
            raise SystemExit('navigation file missing or linked')
        old = self.file.read_text(encoding='ascii').split()
        self.sequence = int(old[1]) if len(old) == 10 and old[0] == 'EVR_MOCK_NAV_1' else 0
        self.log = None; self.origin = None; self.commands = 0

    def closing(self):
        return (self.launch / 'ui-close-request.json').exists() or (self.launch / 'process-exit.json').exists()

    def wait_live(self, timeout):
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            if self.closing():
                raise SystemExit('session is closing')
            probes = sorted(p for p in self.launch.glob('view-resource-probe-*') if p.is_dir())
            if len(probes) >= 2 and (probes[-1] / 'view-resource-probe.jsonl').exists():
                first = first_sample(probes[-1] / 'view-resource-probe.jsonl')
                if first:
                    self.log = probes[-1] / 'view-resource-probe.jsonl'; self.origin = first['stable_q32']; return
            time.sleep(1)
        raise SystemExit('no live probe with root camera samples')

    def state(self):
        s = tail_sample(self.log)
        if not s:
            return None
        x, y, z, w = s['anchor_orientation']
        fx, fz = 2 * (x * z + w * y), 1 - 2 * (x * x + y * y)
        n = math.hypot(fx, fz) or 1.0
        return dict(frame=s['frame'], x=(s['stable_q32'][0] - self.origin[0]) / 2 ** 32, y=(s['stable_q32'][1] - self.origin[1]) / 2 ** 32,
                    z=(s['stable_q32'][2] - self.origin[2]) / 2 ** 32, forward=(fx / n, fz / n))

    def stick(self, state, dx, dz, magnitude, hold_ms=400):
        """Holds the left stick so that the character moves along the world direction (dx, dz)."""
        n = math.hypot(dx, dz)
        if n < 1e-6:
            return
        dx, dz = dx / n, dz / n; fx, fz = state['forward']
        left_y, left_x = dx * fx + dz * fz, dx * fz - dz * fx
        try:   # another walk of the same session may have written since: commands must keep counting up
            old = self.file.read_text(encoding='ascii').split()
            if len(old) == 10 and old[0] == 'EVR_MOCK_NAV_1':
                self.sequence = max(self.sequence, int(old[1]))
        except (OSError, ValueError):
            pass
        self.sequence += 1; issued = time.time_ns() // 1000000
        line = f'EVR_MOCK_NAV_1 {self.sequence} {issued} {hold_ms} 0.000000 0.000000 {left_x * magnitude:.6f} {left_y * magnitude:.6f} 0.000000 0'
        tmp = self.launch / f'mock-navigation-{issued}.tmp'
        with tmp.open('x', encoding='ascii', newline='') as f:
            f.write(line + '\n')
        deadline = time.monotonic() + .25
        while True:   # the runtime may briefly hold a read handle without delete sharing
            try:
                os.replace(tmp, self.file); self.commands += 1; return
            except PermissionError:
                if time.monotonic() >= deadline:
                    tmp.unlink(missing_ok=True); return
                time.sleep(.005)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('launch', type=Path)
    p.add_argument('--points', required=True, help='"x,z;x,z;..." metres from the session start')
    p.add_argument('--push', default='', help='"dx,dz,seconds": then walk slowly that way until blocked')
    p.add_argument('--after-frame', type=int, default=500, help='do not start before this Mock frame (the quick wheel and the height calibration come first)')
    p.add_argument('--tolerance', type=float, default=.35, help='a point counts as reached within this distance (the last point: half of it)')
    p.add_argument('--timeout', type=float, default=240)
    p.add_argument('--report', default='mock-walk.json', help='name of the report in the game folder (a second walk of the same session needs another name)')
    a = p.parse_args()
    points = [tuple(float(v) for v in pair.split(',')) for pair in a.points.split(';')]
    push = tuple(float(v) for v in a.push.split(',')) if a.push else None
    session = Session(a.launch.resolve()); session.wait_live(a.timeout)
    report = dict(points=points, push=push, reached=[], scope='Mock left stick only, steered by the logged position; no transform of the player, the save or physics is written')
    deadline = time.monotonic() + a.timeout
    while True:
        s = session.state()
        if s and s['frame'] >= a.after_frame:
            break
        if session.closing() or time.monotonic() > deadline:
            raise SystemExit('session ended before the walk started')
        time.sleep(.2)
    report['start'] = s
    for index, (tx, tz) in enumerate(points):
        tolerance = a.tolerance / 2 if index == len(points) - 1 else a.tolerance
        best, best_time = None, time.monotonic()
        while True:
            if session.closing() or time.monotonic() > deadline:
                report['aborted'] = 'session closing or timeout'; break
            s = session.state()
            if not s:
                time.sleep(.05); continue
            dx, dz = tx - s['x'], tz - s['z']; distance = math.hypot(dx, dz)
            if distance <= tolerance:
                report['reached'].append(dict(point=(tx, tz), at=(round(s['x'], 3), round(s['z'], 3)), y=round(s['y'], 3), frame=s['frame'], distance=round(distance, 3))); break
            if best is None or distance < best - .15:
                best, best_time = distance, time.monotonic()
            elif time.monotonic() - best_time > 4:
                report['reached'].append(dict(point=(tx, tz), at=(round(s['x'], 3), round(s['z'], 3)), y=round(s['y'], 3), frame=s['frame'], distance=round(distance, 3), stuck=True)); break
            session.stick(s, dx, dz, 1.0 if distance > 2.5 else max(.45, distance / 2.5))
            time.sleep(.08)
        if 'aborted' in report:
            break
    if push and 'aborted' not in report:
        started = time.monotonic(); history = []
        while time.monotonic() - started < push[2] and not session.closing():
            s = session.state()
            if s:
                history.append((time.monotonic(), s['x'], s['z']))
                old = [h for h in history if history[-1][0] - h[0] >= .8]
                if old and time.monotonic() - started > 1.2 and math.hypot(s['x'] - old[-1][1], s['z'] - old[-1][2]) < .05:
                    report['blocked'] = dict(at=(round(s['x'], 3), round(s['z'], 3)), y=round(s['y'], 3), frame=s['frame'], seconds=round(time.monotonic() - started, 2)); break
                session.stick(s, push[0], push[1], .6)
            time.sleep(.08)
        if 'blocked' not in report:
            s = session.state(); report['not_blocked'] = dict(at=(round(s['x'], 3), round(s['z'], 3)), frame=s['frame']) if s else None
    time.sleep(.6)
    s = session.state(); report['end'] = dict(x=round(s['x'], 3), y=round(s['y'], 3), z=round(s['z'], 3), frame=s['frame'], forward=[round(v, 4) for v in s['forward']]) if s else None
    report['commands'] = session.commands
    (session.launch / a.report).write_text(json.dumps(report, indent=1) + '\n', encoding='utf-8')
    print(json.dumps(report))


if __name__ == '__main__':
    main()
