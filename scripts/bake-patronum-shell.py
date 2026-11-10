"""Bake two bounded solid Octane face shells from the supplied glTF 2.0 model.

Only numeric geometry is compiled into the plugin. Source cross-section profiles and wheel
bounds create a clean, closed stylised hull; the original feature mesh supplies
smaller details. Only Python standard library and the adjacent volume baker
are needed to reproduce this file.
"""
from pathlib import Path
import argparse
import hashlib
import importlib.util
import json
import math

ROOT = Path(__file__).resolve().parent.parent
spec = importlib.util.spec_from_file_location('patronum_volume_baker', Path(__file__).with_name('bake-patronum-volume.py'))
volume = importlib.util.module_from_spec(spec)
spec.loader.exec_module(volume)


def cross_section(triangles, x):
    points = []
    for triangle in triangles:
        for a, b in zip(triangle[:3], (triangle[1], triangle[2], triangle[0])):
            if (a[0] - x) * (b[0] - x) <= 0 and abs(a[0] - b[0]) > 1e-9:
                t = (x - a[0]) / (b[0] - a[0])
                points.append(tuple(a[k] + t * (b[k] - a[k]) for k in range(3)))
    return points


def shell(groups, detailed):
    vertices, faces = [], []
    def face(a, b, c, part, outward):
        normal = volume.cross(volume.sub(vertices[b], vertices[a]), volume.sub(vertices[c], vertices[a]))
        if volume.length(normal) < 1e-7:
            return
        normal = volume.unit(normal)
        if volume.dot(normal, outward) < 0:
            b, c = c, b
            normal = volume.scale(normal, -1)
        faces.append(((a, b, c), normal, part))
    sections = (-.84, -.66, -.49, -.30, -.13, .08, .28, .48, .67, .84, .98) if detailed else (-.84, -.49, -.13, .28, .48, .84, .98)
    for x in sections:
        points = cross_section(groups['Body'] + groups['Chassis'], x)
        ys, zs = sorted(abs(p[1]) for p in points), sorted(p[2] for p in points)
        width = min(.40, ys[int((len(ys) - 1) * .97)])
        top = min(.75 if x > -.55 else .54, zs[int((len(zs) - 1) * .97)])
        bottom = max(.14, min(.23, zs[int((len(zs) - 1) * .08)]))
        shoulder = min(top - .06, .46)
        roof = width * (.62 if top > .55 else .83)
        vertices.extend((x, y, z) for y, z in ((-width, bottom + .04), (-width, shoulder), (-roof, top),
            (roof, top), (width, shoulder), (width, bottom + .04), (width * .75, bottom), (-width * .75, bottom)))
    for i in range(len(sections) - 1):
        for j in range(8):
            a, b, c, d = i * 8 + j, i * 8 + (j + 1) % 8, (i + 1) * 8 + (j + 1) % 8, (i + 1) * 8 + j
            center = tuple(sum(vertices[k][axis] for k in (a, b, c, d)) / 4 for axis in range(3))
            normal = volume.unit(volume.cross(volume.sub(vertices[b], vertices[a]), volume.sub(vertices[d], vertices[a])))
            # Dark glazing is part of the shell, so it cannot float outside a
            # simplified body or be hidden by a mismatched exterior surface.
            glass = (j in (1, 3) and -.49 < center[0] < .48 and center[2] > .48) or (j == 2 and .28 < center[0] < .48)
            part = 1 if glass else 0
            outward = volume.scale(volume.cross(volume.sub(vertices[b], vertices[a]), volume.sub(vertices[d], vertices[a])), -1)
            face(a, b, c, part, outward)
            face(a, c, d, part, outward)
    for start, direction in ((0, -1.), ((len(sections) - 1) * 8, 1.)):
        for j in range(1, 7):
            face(start, start + j, start + j + 1, 0, (direction, 0., 0.))
    # The Octane's raised rear wing is distinct from its closed body volume.
    # Its source profile is a separate part rather than lofting the spoiler
    # down into the trunk and making the entire rear body unnaturally tall.
    def box(low, high):
        start = len(vertices)
        vertices.extend((x, y, z) for x, y, z in ((low[0], low[1], low[2]), (high[0], low[1], low[2]),
            (high[0], high[1], low[2]), (low[0], high[1], low[2]), (low[0], low[1], high[2]),
            (high[0], low[1], high[2]), (high[0], high[1], high[2]), (low[0], high[1], high[2])))
        for a, b, c, d, outward in ((0, 1, 2, 3, (0., 0., -1.)), (4, 5, 6, 7, (0., 0., 1.)),
            (0, 1, 5, 4, (0., -1., 0.)), (3, 2, 6, 7, (0., 1., 0.)),
            (0, 3, 7, 4, (-1., 0., 0.)), (1, 2, 6, 5, (1., 0., 0.))):
            face(start + a, start + b, start + c, 0, outward)
            face(start + a, start + c, start + d, 0, outward)
    rear = [p for triangle in groups['Body'] for p in triangle[:3] if p[0] < -.88]
    wing_y = max(abs(p[1]) for p in rear)
    wing_z = sum(p[2] for p in rear) / len(rear)
    box((-.98, -wing_y, wing_z - .017), (-.76, wing_y, wing_z + .017))
    for side in (-1., 1.):
        box((-.80, side * .26 - .015, .51), (-.77, side * .26 + .015, wing_z))
    segments = 12 if detailed else 8
    for group in ('WheelFR', 'WheelFL', 'WheelBR', 'WheelBL'):
        ps = [p for triangle in groups[group] for p in triangle[:3]]
        low = [min(p[k] for p in ps) for k in range(3)]
        high = [max(p[k] for p in ps) for k in range(3)]
        cx, cz = (low[0] + high[0]) / 2, (low[2] + high[2]) / 2
        radius = (high[2] - low[2]) / 2
        start = len(vertices)
        for y in (low[1], high[1]):
            vertices.extend((cx + radius * math.cos(j * math.tau / segments), y,
                cz + radius * math.sin(j * math.tau / segments)) for j in range(segments))
        for j in range(segments):
            a, b, c, d = start + j, start + (j + 1) % segments, start + segments + (j + 1) % segments, start + segments + j
            center = tuple(sum(vertices[k][axis] for k in (a, b, c, d)) / 4 for axis in range(3))
            outward = (center[0] - cx, 0., center[2] - cz)
            face(a, b, c, 2, outward); face(a, c, d, 2, outward)
        for end, direction in ((0, -1.), (segments, 1.)):
            center = len(vertices); vertices.append((cx, low[1] if end == 0 else high[1], cz))
            for j in range(segments):
                face(center, start + end + j, start + end + (j + 1) % segments, 2, (0., direction, 0.))
        # A separate, small silver hub keeps the filled tire from reading as
        # a shapeless dark disk. The original feature mesh adds tread detail.
        outside = low[1] - .001 if (low[1] + high[1]) < 0 else high[1] + .001
        start = len(vertices); vertices.append((cx, outside, cz))
        vertices.extend((cx + radius * .42 * math.cos(j * math.tau / segments), outside,
            cz + radius * .42 * math.sin(j * math.tau / segments)) for j in range(segments))
        for j in range(segments):
            face(start, start + 1 + j, start + 1 + (j + 1) % segments, 3, (0., -1. if outside < 0 else 1., 0.))
    return vertices, faces


def bake(source, output, report=None):
    blob, _, _, groups = volume.parse(source)
    meshes = []
    statistics = {}
    for name, detailed in (('Detailed', True), ('Coarse', False)):
        vertices, faces = shell(groups, detailed)
        meshes.append((name, vertices, faces))
        statistics[name] = {'vertices': len(vertices), 'faces': len(faces),
            'parts': {part: sum(p == index for _, _, p in faces) for index, part in enumerate(('Body', 'Glass', 'Wheel', 'Hub'))}}
    number = lambda value: f'{value:.6f}f'
    text = ['#pragma once', '', '// Derived from Octane - Rocket League Car by Jako, CC BY 4.0.',
        '// https://sketchfab.com/3d-models/octane-rocket-league-car-9910f0a5d158425bbc7deb60c7a81f69',
        '// Source-profile loft body and source-bounded wheel shells. X forward, Y right, Z up; unit half-length.',
        '// Source SHA256: ' + hashlib.sha256(blob).hexdigest(), '#include <array>', '#include <cstdint>', '',
        'namespace wizard::patronum_shell {', 'enum class Part : std::uint8_t { Body, Glass, Wheel, Hub };',
        'struct Vertex { float x, y, z; };', 'struct Face { std::uint16_t a, b, c; float nx, ny, nz; Part part; };']
    for name, vertices, faces in meshes:
        assert len(vertices) < 65536
        assert all(math.isfinite(value) for point in vertices for value in point)
        assert all(abs(volume.length(normal) - 1) < 1e-6 for _, normal, _ in faces)
        text += [f'inline constexpr std::array<Vertex, {len(vertices)}> {name}Vertices{{{{']
        text.extend('    {' + ', '.join(map(number, point)) + '},' for point in vertices)
        text += ['}};', f'inline constexpr std::array<Face, {len(faces)}> {name}Faces{{{{']
        for indices, normal, part in faces:
            text.append('    {' + ', '.join(map(str, indices)) + ', ' + ', '.join(map(number, normal)) + ', Part::' + ('Body', 'Glass', 'Wheel', 'Hub')[part] + '},')
        text += ['}};']
    text += ['} // namespace wizard::patronum_shell', '']
    output.write_text('\n'.join(text), encoding='utf-8')
    result = {'source_sha256': hashlib.sha256(blob).hexdigest(), 'header_sha256': hashlib.sha256(output.read_bytes()).hexdigest(),
        'meshes': statistics, 'method': 'source cross-section loft and source-bounded cylinders', 'all_finite': True, 'normals_unit': True}
    if report:
        report.write_text(json.dumps(result, indent=2), encoding='utf-8')
    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--model', type=Path, required=True)
    parser.add_argument('--output', type=Path, default=ROOT / 'include/PatronumShell.h')
    parser.add_argument('--report', type=Path)
    options = parser.parse_args()
    bake(options.model, options.output, options.report)
