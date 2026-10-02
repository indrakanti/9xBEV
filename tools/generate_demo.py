"""Generate artificial RGB fixtures without third-party Python dependencies."""
from pathlib import Path
root=Path(__file__).resolve().parents[1]/'demo'
root.mkdir(exist_ok=True)
for c,name in enumerate(['front','left','rear','right']):
 data=bytearray()
 for y in range(480):
  for x in range(640):
   line=x%40<2 or y%40<2
   data.extend((230,230,230) if line else ((x+c*41)%256,(y+c*71)%256,(x//2+y//2+c*37)%256))
 (root/(name+'.ppm')).write_bytes(b'P6\n640 480\n255\n'+data)
print(root)
