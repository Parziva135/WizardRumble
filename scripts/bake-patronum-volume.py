"""Bake a bounded luminous surface cloud from the user's supplied Octane GLB.

Numeric geometry only. The game does not parse the original GLB or textures.
Matches scripts/bake-patronum-model.py's X-forward/Y-right/Z-up transform.
"""
from pathlib import Path
import argparse
import bisect
import hashlib
import json
import math
import struct
import sys

ROOT = Path(__file__).resolve().parent.parent

DEFAULT_OUTPUT = ROOT / 'include/PatronumVolume.h'


def sub(a,b): return tuple(x-y for x,y in zip(a,b))
def add(a,b): return tuple(x+y for x,y in zip(a,b))
def scale(a,t): return tuple(x*t for x in a)
def dot(a,b): return sum(x*y for x,y in zip(a,b))
def cross(a,b): return (a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0])
def length(a): return math.sqrt(dot(a,a))
def unit(a): return scale(a,1/length(a)) if length(a)>1e-12 else (0.,0.,1.)
def multiply(a,b): return [sum(a[k*4+r]*b[c*4+k] for k in range(4)) for c in range(4) for r in range(4)]
def transform(m,p): return tuple(sum(m[c*4+r]*p[c] for c in range(3))+m[12+r] for r in range(3))
def normal_transform(m,n):
    # Inverse transpose via column cofactors; retains outward source normals
    # through node rotation/nonuniform scale, independently of triangle winding.
    if any(not math.isfinite(x) for x in (*m,*n)): raise ValueError('Nonfinite source transform/normal')
    a,b,c=[tuple(m[j*4+r] for r in range(3)) for j in range(3)]
    determinant=dot(a,cross(b,c))
    if abs(determinant)<1e-12: raise ValueError('Singular node transform')
    result=scale(add(add(scale(cross(b,c),n[0]),scale(cross(c,a),n[1])),scale(cross(a,b),n[2])),1/determinant)
    return unit(result)

def parse(source):
    blob=source.read_bytes()
    magic,version,size=struct.unpack_from('<III',blob)
    if (magic,version,size)!=(0x46546c67,2,len(blob)): raise ValueError('Expected complete glTF 2.0 GLB')
    chunks={};offset=12
    while offset<len(blob):
        size,kind=struct.unpack_from('<II',blob,offset)
        if offset+8+size>len(blob): raise ValueError('Truncated chunk')
        chunks[kind]=blob[offset+8:offset+8+size];offset+=8+size
    document=json.loads(chunks[0x4e4f534a]);data=chunks[0x004e4942]
    def read(index):
        a=document['accessors'][index];view=document['bufferViews'][a['bufferView']]
        if 'sparse' in a: raise ValueError('Unsupported sparse accessor')
        arity={'SCALAR':1,'VEC2':2,'VEC3':3,'VEC4':4}[a['type']]
        fmt={5121:'B',5123:'H',5125:'I',5126:'f'}[a['componentType']]
        width=struct.calcsize('<'+fmt*arity);stride=view.get('byteStride',width)
        begin=view.get('byteOffset',0)+a.get('byteOffset',0)
        if begin+max(0,a['count']-1)*stride+width>len(data): raise ValueError('Accessor outside buffer')
        return [struct.unpack_from('<'+fmt*arity,data,begin+i*stride) for i in range(a['count'])]
    meshes=[];identity=[1.,0,0,0,0,1.,0,0,0,0,1.,0,0,0,0,1.]
    def visit(index,parent):
        node=document['nodes'][index]
        if any(k in node for k in ('translation','rotation','scale')): raise ValueError('Unexpected source TRS')
        m=multiply(parent,node.get('matrix',identity))
        if 'mesh' in node:
            for primitive in document['meshes'][node['mesh']]['primitives']:
                if primitive.get('mode',4)!=4 or 'extensions' in primitive: raise ValueError('Expected ordinary triangles')
                ps=[transform(m,p) for p in read(primitive['attributes']['POSITION'])]
                ns=[normal_transform(m,n) for n in read(primitive['attributes']['NORMAL'])]
                indices=[v[0] for v in read(primitive['indices'])]
                meshes.append((node.get('name','mesh'),ps,ns,indices))
        for child in node.get('children',[]): visit(child,m)
    for index in document['scenes'][document.get('scene',0)]['nodes']: visit(index,identity)
    all_points=[p for _,ps,_,_ in meshes for p in ps]
    if not all_points or any(not math.isfinite(x) for p in all_points for x in p):
        raise ValueError('Invalid source model coordinates')
    low=[min(p[k] for p in all_points) for k in range(3)];high=[max(p[k] for p in all_points) for k in range(3)]
    half=(high[0]-low[0])/2;mid=[(a+b)/2 for a,b in zip(low,high)]
    if half<1e-8: raise ValueError('Model has no finite forward extent')
    def canonical(p): return ((p[0]-mid[0])/half,(p[2]-mid[2])/half,(p[1]-low[1])/half)
    groups={name:[] for name in ('Chassis','Paint','Body','Glass','WheelFR','WheelFL','WheelBR','WheelBL')}
    for name,ps,ns,indices in meshes:
        if 'Window' in name: group='Glass'
        elif 'Dieci - FR' in name: group='WheelFR'
        elif 'Dieci - FL' in name: group='WheelFL'
        elif 'Dieci - BR' in name: group='WheelBR'
        elif 'Dieci - BL' in name: group='WheelBL'
        elif 'Chassis' in name: group='Chassis'
        elif 'Paint' in name: group='Paint'
        else: group='Body'
        ps=[canonical(p) for p in ps];ns=[(n[0],n[2],n[1]) for n in ns]
        for i in range(0,len(indices),3):
            ids=indices[i:i+3];a,b,c=[ps[j] for j in ids]
            n=cross(sub(b,a),sub(c,a));area=length(n)/2
            if area<1e-9: continue
            ref=unit(tuple(sum(ns[j][k] for j in ids) for k in range(3)))
            face=unit(n)
            if dot(face,ref)<0: face=scale(face,-1)
            groups[group].append((a,b,c,face,area))
    return blob,low,high,groups

def candidates(triangles,count=2048):
    areas=[];total=0
    for t in triangles: total+=t[4];areas.append(total)
    result=[]
    # Low-discrepancy area-weighted candidates. No random seed or runtime RNG.
    for index in range(count):
        select=(index+.5)/count*total
        a,b,c,n,_=triangles[min(len(triangles)-1,bisect.bisect_left(areas,select))]
        u=math.sqrt(((index+.5)*.7548776662466927)%1)
        v=((index+.5)*.5698402909980532)%1
        p=add(add(scale(a,1-u),scale(b,u*(1-v))),scale(c,u*v))
        result.append((p,n))
    return result,total

def select_samples(triangles,count):
    cloud,area=candidates(triangles)
    center=tuple(sum(p[k] for p,_ in cloud)/len(cloud) for k in range(3))
    first=max(range(len(cloud)),key=lambda i:dot(sub(cloud[i][0],center),sub(cloud[i][0],center)))
    chosen=[];distance=[float('inf')]*len(cloud);at=first
    for _ in range(count):
        chosen.append(cloud[at])
        p=cloud[at][0]
        for index,(q,_) in enumerate(cloud):
            distance[index]=min(distance[index],dot(sub(p,q),sub(p,q)))
        at=max(range(len(cloud)),key=lambda i:distance[i])
    # Footprints overlap at low opacity to reconstruct a coherent luminous
    # surface; shader uses normal/part to keep windows and wheel form legible.
    result=[]
    for i,(p,n) in enumerate(chosen):
        nearest=min(length(sub(p,q)) for j,(q,_) in enumerate(chosen) if j!=i)
        radius=max(.075,min(.205,nearest*.78))
        result.append((p,n,radius))
    return result,area

def bake(source, target, report_path=None):
    blob,low,high,groups=parse(source)
    budgets={'Chassis':12,'Paint':28,'Body':56,'Glass':24,'WheelFR':14,'WheelFL':14,'WheelBR':14,'WheelBL':14}
    parts={'Chassis':0,'Paint':0,'Body':0,'Glass':1,'WheelFR':2,'WheelFL':3,'WheelBR':4,'WheelBL':5}
    selected={};stats={}
    for name,count in budgets.items():
        selected[name],area=select_samples(groups[name],count)
        stats[name]={'triangles':len(groups[name]),'area':area,'samples':count}
    samples=[]
    # Interleave semantic features for deterministic low-detail prefixes. Even
    # a small allowance sees all wheels, glass and the three body components.
    for ordinal in range(max(budgets.values())):
        for name in budgets:
            if ordinal<len(selected[name]): samples.append((*selected[name][ordinal],parts[name]))
    assert len(samples)==176
    for p,n,radius,part in samples:
        assert all(math.isfinite(x) for x in (*p,*n,radius))
        assert abs(length(n)-1)<1e-6 and .075<=radius<=.205
        assert -1.00001<=p[0]<=1.00001 and -.6<=p[1]<=.6 and 0<=p[2]<1
    assert set(s[-1] for s in samples[:16])==set(range(6))
    number=lambda n:f'{n:.6f}f'
    output=['#pragma once','','// Derived from Octane - Rocket League Car by Jako, CC BY 4.0.',
      '// https://sketchfab.com/3d-models/octane-rocket-league-car-9910f0a5d158425bbc7deb60c7a81f69',
      '// Area-distributed luminous surface samples. X forward, Y right, Z up.',
      '// Matches PatronumMesh.h: unit half-length; multiply position/radius by75.',
      '// Source SHA256: '+hashlib.sha256(blob).hexdigest(),
      '#include <array>','#include <cstdint>','','namespace wizard::patronum_volume {',
      'enum class Part : std::uint8_t { Body, Glass, WheelFR, WheelFL, WheelBR, WheelBL };',
      'struct Sample { float x, y, z, nx, ny, nz, radius; Part part; };',
      'inline constexpr std::array<Sample, 176> Samples{{']
    part_names=['Body','Glass','WheelFR','WheelFL','WheelBR','WheelBL']
    for p,n,radius,part in samples:
        output.append('    {'+', '.join(number(x) for x in (*p,*n,radius))+', Part::'+part_names[part]+'},')
    output+=['}};','} // namespace wizard::patronum_volume','']
    target.parent.mkdir(parents=True,exist_ok=True)
    target.write_text('\n'.join(output),encoding='utf-8')
    report={'source_sha256':hashlib.sha256(blob).hexdigest(),'samples':len(samples),'bounds':{'low':low,'high':high},
      'canonical_bounds':{'low':[min(s[0][k] for s in samples) for k in range(3)],'high':[max(s[0][k] for s in samples) for k in range(3)]},
      'parts':stats,'radius_range':[min(s[2] for s in samples),max(s[2] for s in samples)],
      'header_sha256':hashlib.sha256(target.read_bytes()).hexdigest(),'all_finite':True,'normals_unit':True,
      'lod16_has_body_glass_and_four_wheels':True}
    if report_path is not None:
        report_path.parent.mkdir(parents=True,exist_ok=True)
        report_path.write_text(json.dumps(report,indent=2),encoding='utf-8')
    print(json.dumps(report,indent=2))

if __name__=='__main__':
    arguments=argparse.ArgumentParser(description=__doc__)
    arguments.add_argument('--model',type=Path,required=True,help='Supplied glTF2.0 Octane GLB')
    arguments.add_argument('--output',type=Path,default=DEFAULT_OUTPUT,help='Generated numeric sample header')
    arguments.add_argument('--report',type=Path,help='Optional generation statistics JSON')
    options=arguments.parse_args()
    bake(options.model,options.output,options.report)
