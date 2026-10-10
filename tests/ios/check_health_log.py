"""Verify persistent samples and detect rapid growth within one stable context."""
import argparse
import re
from pathlib import Path

parser=argparse.ArgumentParser()
parser.add_argument('log')
parser.add_argument('--context',type=int)
parser.add_argument('--minimum-seconds',type=int,default=30)
parser.add_argument('--max-growth-mib',type=float,default=64)
args=parser.parse_args()
samples=[]
for line in Path(args.log).read_text().splitlines():
    if 'event=health ' not in line: continue
    fields=dict(re.findall(r'(\w+)=(-?\d+)',line))
    sample={key:int(value) for key,value in fields.items()}
    if sample.get('vm_status')!=0: raise SystemExit('FAIL: memory sampling failed')
    if sample.get('footprint_bytes',0)<=0: raise SystemExit('FAIL: footprint missing')
    samples.append(sample)
if len(samples)<2: raise SystemExit('FAIL: fewer than two health samples')
# Analyse the last contiguous context, avoiding asset-loading transitions.
end=len(samples)
if args.context is not None:
    while end and samples[end-1]['context']!=args.context: end-=1
    if not end: raise SystemExit('FAIL: requested context missing')
last_context=samples[end-1]['context']
start=end-1
while start>0 and samples[start-1]['context']==last_context: start-=1
stable=samples[start:end]
# Ignore the first 15 seconds of the selected state while assets settle.
settled=[s for s in stable if s['epoch']-stable[0]['epoch']>=15]
if len(settled)<2: raise SystemExit('FAIL: no settled memory interval')
elapsed=settled[-1]['epoch']-settled[0]['epoch']
if elapsed<args.minimum_seconds: raise SystemExit(f'FAIL: interval too short: {elapsed}s')
growth=(settled[-1]['footprint_bytes']-settled[0]['footprint_bytes'])/1024**2
peak=max(s['footprint_bytes'] for s in settled)/1024**2
print(f'context={last_context} samples={len(settled)} elapsed={elapsed}s growth={growth:.2f}MiB peak={peak:.2f}MiB')
if growth>args.max_growth_mib: raise SystemExit('FAIL: rapid memory growth')
print('PASS: successful periodic memory samples and bounded growth in the measured interval')
