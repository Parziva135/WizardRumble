"""Bake open hologram feature edges from the user-supplied glTF 2.0 Octane.

No executable/model payload is loaded by the game. Only bounded numeric arrays
are emitted. Usage: python scripts/bake-patronum-model.py source.glb [header]

Source: Octane - Rocket League Car by Jako, CC BY 4.0.
https://sketchfab.com/3d-models/octane-rocket-league-car-9910f0a5d158425bbc7deb60c7a81f69
"""
from pathlib import Path
import collections
import hashlib
import json
import math
import struct
import sys


def multiply(a, b):
    return [sum(a[k * 4 + row] * b[col * 4 + k] for k in range(4))
            for col in range(4) for row in range(4)]


def transform(m, p):
    return tuple(sum(m[col * 4 + row] * p[col] for col in range(3)) + m[12 + row]
                 for row in range(3))


def sub(a, b): return tuple(x - y for x, y in zip(a, b))
def dot(a, b): return sum(x * y for x, y in zip(a, b))
def length(v): return math.sqrt(dot(v, v))
def normal(a, b, c):
    u, v = sub(b, a), sub(c, a)
    n = (u[1]*v[2]-u[2]*v[1], u[2]*v[0]-u[0]*v[2], u[0]*v[1]-u[1]*v[0])
    size = length(n)
    return tuple(x / size for x in n) if size > 1e-10 else None


def simplify(points, tolerance):
    if len(points) < 3: return points
    start, end = points[0], points[-1]
    axis = sub(end, start)
    denominator = dot(axis, axis)
    furthest, at = 0, 0
    for i, point in enumerate(points[1:-1], 1):
        ratio = max(0., min(1., dot(sub(point, start), axis) / denominator)) if denominator > 1e-15 else 0
        nearest = tuple(a + ratio*b for a, b in zip(start, axis))
        distance = length(sub(point, nearest))
        if distance > furthest: furthest, at = distance, i
    if furthest <= tolerance: return [start, end]
    return simplify(points[:at+1], tolerance)[:-1] + simplify(points[at:], tolerance)


def chains_for(edges):
    neighbors = collections.defaultdict(set)
    remaining = set(edges)
    for a, b in edges:
        neighbors[a].add(b); neighbors[b].add(a)
    chains = []
    def walk(a, b):
        chain = [a, b]
        remaining.discard(tuple(sorted((a, b))))
        while len(neighbors[b]) == 2:
            c = next(x for x in neighbors[b] if x != a)
            edge = tuple(sorted((b, c)))
            if edge not in remaining: break
            chain.append(c); remaining.remove(edge); a, b = b, c
        return chain
    for a in sorted(neighbors):
        if len(neighbors[a]) == 2: continue
        for b in sorted(neighbors[a]):
            if tuple(sorted((a, b))) in remaining: chains.append(walk(a, b))
    while remaining:
        a, b = min(remaining)
        chains.append(walk(a, b))
    return chains


def bake(source, target):
    blob = source.read_bytes()
    magic, version, total = struct.unpack_from('<III', blob)
    if magic != 0x46546c67 or version != 2 or total != len(blob): raise ValueError('Expected glTF 2.0 GLB')
    chunks, offset = {}, 12
    while offset + 8 <= len(blob):
        size, kind = struct.unpack_from('<II', blob, offset)
        if size > len(blob) - offset - 8: raise ValueError('Truncated GLB chunk')
        chunks[kind] = blob[offset+8:offset+8+size]; offset += 8 + size
    document = json.loads(chunks[0x4e4f534a])
    data = chunks[0x004e4942]
    def read(index):
        a = document['accessors'][index]
        if 'sparse' in a: raise ValueError('Sparse accessors are unsupported')
        v = document['bufferViews'][a['bufferView']]
        shape = {'SCALAR':1, 'VEC2':2, 'VEC3':3, 'VEC4':4}[a['type']]
        fmt = {5121:'B', 5123:'H', 5125:'I', 5126:'f'}[a['componentType']]
        width = struct.calcsize('<'+fmt*shape)
        stride = v.get('byteStride', width)
        begin = v.get('byteOffset', 0) + a.get('byteOffset', 0)
        if begin + max(0,a['count']-1)*stride + width > len(data): raise ValueError('Accessor exceeds buffer')
        return [struct.unpack_from('<'+fmt*shape, data, begin+i*stride) for i in range(a['count'])]
    identity = [1.,0.,0.,0.,0.,1.,0.,0.,0.,0.,1.,0.,0.,0.,0.,1.]
    meshes = []
    def visit(index, parent):
        node = document['nodes'][index]
        if any(x in node for x in ('translation','rotation','scale')):
            raise ValueError('This source uses explicit node matrices; unexpected TRS requires conversion')
        matrix = multiply(parent, node.get('matrix', identity))
        if 'mesh' in node:
            for primitive in document['meshes'][node['mesh']]['primitives']:
                if primitive.get('mode',4) != 4 or 'extensions' in primitive: raise ValueError('Expected plain triangles')
                positions = [transform(matrix, p) for p in read(primitive['attributes']['POSITION'])]
                indices = [p[0] for p in read(primitive['indices'])]
                meshes.append((node.get('name','mesh'),positions,indices))
        for child in node.get('children',[]): visit(child,matrix)
    for node in document['scenes'][document.get('scene',0)]['nodes']: visit(node,identity)
    all_points = [p for _, ps, _ in meshes for p in ps]
    if not all_points or any(not math.isfinite(x) for p in all_points for x in p): raise ValueError('Invalid model coordinates')
    low = [min(p[k] for p in all_points) for k in range(3)]
    high = [max(p[k] for p in all_points) for k in range(3)]
    # This glTF is Y-up, X-forward. Bake X-forward/Y-right/Z-up at unit half-length.
    half_length = (high[0]-low[0])/2
    midpoint = [(a+b)/2 for a,b in zip(low,high)]
    def canonical(p): return ((p[0]-midpoint[0])/half_length,(p[2]-midpoint[2])/half_length,(p[1]-low[1])/half_length)
    selected = []
    report = []
    body_triangles = []
    for name, points, indices in meshes:
        points = [canonical(p) for p in points]
        if name.startswith('Octane_'):
            body_triangles.extend(tuple(points[j] for j in indices[i:i+3]) for i in range(0,len(indices),3))
        unique, welded = [], {}
        remap = []
        for point in points:
            key = tuple(round(x*10000) for x in point)
            if key not in welded: welded[key]=len(unique); unique.append(point)
            remap.append(welded[key])
        adjacency = collections.defaultdict(list)
        for i in range(0,len(indices),3):
            face = [remap[x] for x in indices[i:i+3]]
            if len(set(face)) != 3: continue
            n = normal(*(unique[x] for x in face))
            if n is None: continue
            for j in range(3): adjacency[tuple(sorted((face[j],face[(j+1)%3])))].append(n)
        features = []
        for edge, normals in adjacency.items():
            if length(sub(unique[edge[0]],unique[edge[1]])) < .001: continue
            if len(normals) == 1 or any(dot(a,b) < .79 for i,a in enumerate(normals) for b in normals[i+1:]):
                features.append(edge)
        loops = []
        for chain in chains_for(features):
            path = [unique[i] for i in chain]
            if path[0] == path[-1] and len(path)>4:
                middle = len(path)//2
                simplified = simplify(path[:middle+1],.013)[:-1] + simplify(path[middle:],.013)
            else: simplified = simplify(path,.009)
            segments = list(zip(simplified,simplified[1:]))
            score = sum(length(sub(a,b)) for a,b in segments)
            if score > .035: loops.append((score,segments))
        # Whole chains remain intact: wheel rims never become random dotted arcs.
        budget = 20 if 'Chassis' in name else 30 if 'Paint' in name else 45 if 'Body' in name else 26 if 'Window' in name else 8 if 'Rim' in name else 24
        chosen = []
        for score, segments in sorted(loops,reverse=True):
            if len(chosen)+len(segments)<=budget: chosen.extend(segments)
        rank = 0 if 'Tread' in name else 1 if 'Window' in name else 2 if 'Body' in name else 3 if 'Paint' in name else 4
        selected.extend((rank, name, a,b) for a,b in chosen)
        report.append({'name':name,'source_vertices':len(points),'feature_edges':len(features),'baked_edges':len(chosen)})
    # Real triangle/plane intersections give coherent body contours rather than
    # selecting disconnected details from the original highly detailed chassis.
    # These are open holographic scan lines, not a replacement box silhouette.
    for axis, level in ((1,-.33),(1,0),(1,.33),(2,.29),(2,.59),(2,.78)):
        vertices, keys, cuts = [], {}, set()
        def vertex(point):
            key = tuple(round(x*10000) for x in point)
            if key not in keys: keys[key]=len(vertices); vertices.append(point)
            return keys[key]
        for triangle in body_triangles:
            intersections = []
            for i in range(3):
                a,b=triangle[i],triangle[(i+1)%3]
                da,db=a[axis]-level,b[axis]-level
                if da*db>0 or abs(da-db)<1e-12: continue
                t=da/(da-db)
                if -1e-6<=t<=1+1e-6:
                    point=tuple(a[k]+(b[k]-a[k])*t for k in range(3))
                    if not any(length(sub(point,p))<1e-7 for p in intersections): intersections.append(point)
            if len(intersections)==2:
                a,b=(vertex(p) for p in intersections)
                if a!=b: cuts.add(tuple(sorted((a,b))))
        paths=[]
        for chain in chains_for(cuts):
            points=[vertices[i] for i in chain]
            if points[0]==points[-1] and len(points)>4:
                middle=len(points)//2
                points=simplify(points[:middle+1],.012)[:-1]+simplify(points[middle:],.012)
            else: points=simplify(points,.012)
            segments=list(zip(points,points[1:])); score=sum(length(sub(a,b)) for a,b in segments)
            if score>.12: paths.append((score,segments))
        count=0
        for score,segments in sorted(paths,reverse=True):
            if count+len(segments)>24: continue
            selected.extend((1,'Body profile '+str(axis)+' '+str(level),a,b) for a,b in segments)
            count+=len(segments)
        report.append({'profile_axis':axis,'profile_level':level,'baked_edges':count})
    selected.sort(key=lambda item:(item[0],item[1]))
    global_vertices, lookup, edges = [], {}, []
    for _,name,a,b in selected:
        result=[]
        for point in (a,b):
            key=tuple(round(x*10000) for x in point)
            if key not in lookup: lookup[key]=len(global_vertices); global_vertices.append(point)
            result.append(lookup[key])
        key=tuple(sorted(result))
        if key not in edges: edges.append(key)
    if not edges or len(edges)>380 or len(global_vertices)>65535: raise ValueError('Baked mesh exceeds bounded renderer limits')
    header = ['#pragma once','','// Derived from Octane - Rocket League Car by Jako, CC BY 4.0.',
      '// https://sketchfab.com/3d-models/octane-rocket-league-car-9910f0a5d158425bbc7deb60c7a81f69',
      '// Baked open feature edges; X forward, Y right, Z up. Unit half-length.',
      '// Source SHA256: '+hashlib.sha256(blob).hexdigest(),'#include <array>','#include <cstdint>','',
      'namespace wizard::patronum_mesh {','struct Vertex { float x, y, z; };','struct Edge { std::uint16_t a, b; };',
      'inline constexpr std::array<Vertex, '+str(len(global_vertices))+'> Vertices{{']
    number=lambda x: format(x,'.6f')+'f'
    header += ['    {'+', '.join(number(x) for x in point)+'},' for point in global_vertices]
    header += ['}};','inline constexpr std::array<Edge, '+str(len(edges))+'> Edges{{']
    header += ['    {'+str(a)+', '+str(b)+'},' for a,b in edges]
    header += ['}};','} // namespace wizard::patronum_mesh','']
    target.parent.mkdir(parents=True,exist_ok=True); target.write_text('\n'.join(header),encoding='utf-8')
    print(json.dumps({'source_sha256':hashlib.sha256(blob).hexdigest(),'bounds':{'low':low,'high':high},'vertices':len(global_vertices),'edges':len(edges),'meshes':report},indent=2))


if __name__ == '__main__':
    bake(Path(sys.argv[1]),Path(sys.argv[2]) if len(sys.argv)>2 else Path(__file__).resolve().parent.parent/'include/PatronumMesh.h')
