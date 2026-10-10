"""Build separately, then execute pixel regressions on an available simulator."""
import argparse
import json
import os
import re
from pathlib import Path
import shutil
import subprocess
import time
from check_ui_pixels import check

parser=argparse.ArgumentParser()
parser.add_argument('--device',required=True)
parser.add_argument('--app',required=True)
parser.add_argument('--output',required=True)
parser.add_argument('--soak-seconds',type=int,default=0)
args=parser.parse_args()
out=Path(args.output)
out.mkdir(parents=True,exist_ok=False)
bundle='com.c4rlox.simpsons.ios'
def sim(*args,**kwargs):
    return subprocess.run(['xcrun','simctl',*args],check=True,**kwargs)
sim('install',args.device,args.app)
env=dict(os.environ,SIMCTL_CHILD_IOS_UI_GPU_TEST='1')
passed=False
with (out/'console.log').open('w') as log:
    process=subprocess.Popen(['xcrun','simctl','launch','--terminate-running-process','--console',args.device,bundle],env=env,stdout=log,stderr=subprocess.STDOUT)
    try:
        container=sim('get_app_container',args.device,bundle,'data',capture_output=True,text=True).stdout.strip()
        health=Path(container)/'Documents/srr2_health.log'
        deadline=time.monotonic()+60
        while time.monotonic()<deadline:
            # The bootstrap splash can draw before UIKit presents the game
            # window. Wait for both submitted frames and a live game context.
            ready='IOS_UI_TEST scene-ready' in (out/'console.log').read_text()
            state=health.read_text() if health.exists() else ''
            if ready and 'event=frame-stats' in state and re.search(r'event=health .*context=\d+',state): break
            if process.poll() is not None: raise RuntimeError('App exited before the GPU scene was ready')
            time.sleep(.5)
        else: raise TimeoutError('GPU scene did not become ready within 60 seconds')
        results=[]
        for i in range(3):
            time.sleep(1)
            shot=out/f'gpu-{i}.png'
            sim('io',args.device,'screenshot',str(shot))
            results.append(check(shot))
        passed=all(results)
        if args.soak_seconds:
            deadline=time.monotonic()+args.soak_seconds
            while time.monotonic()<deadline:
                if process.poll() is not None: raise RuntimeError('App exited during memory soak')
                time.sleep(1)
        container=sim('get_app_container',args.device,bundle,'data',capture_output=True,text=True).stdout.strip()
        health=Path(container)/'Documents/srr2_health.log'
        if not health.exists(): raise RuntimeError('Persistent health log is missing')
        shutil.copy2(health,out/'health.log')
        if 'event=health' not in health.read_text(): raise RuntimeError('Memory sample missing from health log')
        (out/'result.json').write_text(json.dumps({'pixel_pass':passed,'captures':results,'health_log_present':True},indent=2))
    finally:
        subprocess.run(['xcrun','simctl','terminate',args.device,bundle],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
        try: process.wait(timeout=5)
        except subprocess.TimeoutExpired: process.terminate()
if not passed: raise SystemExit(1)
print('PASS: all eight pixel assertions in three captures; persistent memory log present')
