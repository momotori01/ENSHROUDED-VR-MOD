"""Local geometry coverage diagnostic for RenderDoc post-VS float4 triangle lists.

This plots exported geometry, not a replacement renderer or a VR eye image.
Raster coverage is approximate (Pillow polygon rules); no depth/material emulation.
"""
import argparse
import json
import math
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent / "python"))
from PIL import Image, ImageDraw


def clip(poly):
    # Vulkan homogeneous clip volume: -w <= x,y <= w, 0 <= z <= w.
    planes = (lambda p: p[3] + p[0], lambda p: p[3] - p[0],
              lambda p: p[3] + p[1], lambda p: p[3] - p[1],
              lambda p: p[2], lambda p: p[3] - p[2])
    for plane in planes:
        result = []
        for a, b in zip(poly, poly[1:] + poly[:1]):
            da, db = plane(a), plane(b)
            if da >= 0:
                result.append(a)
            if (da >= 0) != (db >= 0):
                t = da / (da - db)
                result.append(tuple(x + t * (y - x) for x, y in zip(a, b)))
        poly = result
        if not poly:
            break
    return poly


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("directory", type=Path)
    parser.add_argument("--event", type=int, required=True)
    parser.add_argument("--width", type=int, default=640)
    parser.add_argument("--height", type=int, default=720)
    args = parser.parse_args()
    if not 1 <= args.width <= 8192 or not 1 <= args.height <= 8192:
        raise ValueError("invalid diagnostic extent")
    entries = [json.loads(x) for x in (args.directory / "event-meshes.jsonl").read_text().splitlines()]
    mesh = next(x for x in entries if x["event"] == args.event)
    if (mesh["topology"], mesh["component_count"], mesh["component_bytes"], mesh["component_type"]) != (5, 4, 4, 1):
        raise ValueError("only float4 position triangle lists supported")
    raw = (args.directory / f"event_{args.event}_vertices.bin").read_bytes()
    vertices = [struct.unpack_from("<4f", raw, n) for n in range(0, len(raw), mesh["vertex_stride"])]
    if not all(math.isfinite(x) for v in vertices for x in v):
        raise ValueError("nonfinite clip position")
    if mesh["index_stride"]:
        fmt = {1: "B", 2: "H", 4: "I"}[mesh["index_stride"]]
        indices = [v[0] + mesh["base_vertex"] for v in struct.iter_unpack("<" + fmt, (args.directory / f"event_{args.event}_indices.bin").read_bytes())]
    else:
        indices = list(range(mesh["indices"]))
    if len(indices) % 3 or min(indices) < 0 or max(indices) >= len(vertices):
        raise ValueError("invalid triangle indices")
    mask = Image.new("L", (args.width, args.height))
    wire = Image.new("RGB", mask.size, (12, 12, 16))
    draw, lines = ImageDraw.Draw(mask), ImageDraw.Draw(wire)
    visible = 0
    for n in range(0, len(indices), 3):
        poly = clip([vertices[i] for i in indices[n:n + 3]])
        if len(poly) < 3 or any(p[3] <= 0 for p in poly):
            continue
        points = [((p[0] / p[3] + 1) * args.width / 2, (p[1] / p[3] + 1) * args.height / 2) for p in poly]
        draw.polygon(points, fill=255)
        lines.line(points + points[:1], fill=(70, 170, 230), width=1)
        visible += 1
    mask.save(args.directory / "postvs-coverage.png")
    wire.save(args.directory / "postvs-wireframe.png")
    report = {"event": args.event, "triangles": len(indices) // 3,
              "visible_triangles": visible, "vertices": len(vertices),
              "approximate_uncovered_fraction": mask.histogram()[0] / (args.width * args.height),
              "top_center_covered": bool(mask.getpixel((args.width // 2, 40))),
              "sky_sample_covered": bool(mask.getpixel((args.width // 2, 220))),
              "clip_component_ranges": [[min(v[i] for v in vertices), max(v[i] for v in vertices)] for i in range(4)],
              "limitations": "Approximate positive-height viewport coverage; no depth, culling, scissor or fragment discard."}
    (args.directory / "postvs-coverage.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report))


if __name__ == "__main__":
    main()
