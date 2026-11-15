import importlib.util
from pathlib import Path
import math
p=Path(__file__).parent.parent/'tools/generate_tracks.py'
spec=importlib.util.spec_from_file_location('trackgen',p)
g=importlib.util.module_from_spec(spec);spec.loader.exec_module(g)
features=[]
for i,(name,code) in enumerate(g.TRACKS):
  lon=30+i*.1
  features.append({'type':'Feature','properties':{'id':code,'Location':name},
    'geometry':{'type':'LineString','coordinates':[[lon,10],[lon+.01,10],[lon+.01,10.01],[lon,10.01],[lon,10]]}})
source,found=g.generate({'type':'FeatureCollection','features':features})
assert len(found)==23 and source.count('static const uint8_t t')==23
for name,code,points in found:
  assert points[0]==points[-1]
  assert len(points)>=5
  assert all(0<=x<=100 and 0<=y<=100 for x,y in points)
try:
  g.generate({'features':features[:-1]})
  raise AssertionError('Missing circuits should fail the build')
except ValueError:
  pass
print('PASS: generator processes all 23 circuit IDs and rejects missing real tracks')
