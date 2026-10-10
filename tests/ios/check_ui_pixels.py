"""Validate a screenshot of IOS_UI_GPU_TEST=1, including mask interior/exterior."""
import sys
from PIL import Image

def check(path):
    image=Image.open(path).convert('RGB')
    w,h=image.size
    cases=[('opaque',0,.5,.5,(0,255,0)),
           ('alpha',1,.5,.5,(128,0,128)),
           ('blend-disable',2,.5,.5,(255,0,0)),
           ('depth-disable',3,.5,.5,(0,255,0)),
           ('mask-center',4,.5,.5,(0,255,0)),
           ('mask-corner',4,.1,.1,(0,0,255)),
           ('clear-disabled',5,.5,.5,(0,255,0)),
           ('scissor-disable',6,.75,.5,(255,255,0))]
    failures=[]
    for name,cell,x,y,expected in cases:
        px=int(w*(cell+x)/7); py=int(h*y)
        actual=image.getpixel((px,py))
        passed=max(abs(a-b) for a,b in zip(actual,expected))<=12
        print(f'{"PASS" if passed else "FAIL"}: {name}: actual={actual} expected={expected} pixel={px},{py}')
        if not passed: failures.append(name)
    return not failures
if __name__=='__main__':
    sys.exit(0 if check(sys.argv[1]) else 1)
