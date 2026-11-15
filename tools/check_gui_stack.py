#!/usr/bin/env python3
"""Check optimized ARM render frames. Run after ufbt regenerates compile_commands.
These bounds leave room for the firmware's GUI/Canvas callers; they are not a
measurement of the complete runtime stack or a replacement for hardware testing.
"""
import json
from pathlib import Path
import shlex
import subprocess
import tempfile

root=Path(__file__).resolve().parent.parent
entries=json.loads((root/'.vscode/compile_commands.json').read_text())
entry=next(e for e in entries if Path(e['file']).name=='flipper_gp_manager.c')
with tempfile.TemporaryDirectory(prefix='gp-gui-stack-') as folder:
    target=Path(folder)/'gui.o'
    args=shlex.split(entry['command'])
    args[args.index('-o')+1]=str(target)
    args.append('-fstack-usage')
    subprocess.run(args,cwd=entry.get('directory',str(root)),check=True)
    frames={}
    for line in target.with_suffix('.su').read_text().splitlines():
        name,size,kind=line.split('\t')
        name=name.rsplit(':',1)[-1].split('.')[0]
        if kind!='static':raise SystemExit(f'Unbounded stack frame: {name}: {kind}')
        frames[name]=int(size)
    for name,budget in [('draw_cb',160),('draw_circuit',384),('circuit_segment_length',32)]:
        if name not in frames:raise SystemExit(f'Missing ARM stack measurement for {name}')
        if frames[name]>budget:raise SystemExit(f'GUI stack budget exceeded: {name}: {frames[name]} > {budget}')
    chain=sum(frames[name] for name in ('draw_cb','draw_circuit','circuit_segment_length'))
    if chain>512:raise SystemExit(f'GUI render chain budget exceeded: {chain} > 512')
    print(f'PASS ARM GUI stack: draw_cb={frames["draw_cb"]}, draw_circuit={frames["draw_circuit"]}, segment={frames["circuit_segment_length"]}; measured app render frames={chain} bytes (budget 512; firmware callers excluded)')

# Larger season archives must never become automatic variables in save paths.
with tempfile.TemporaryDirectory(prefix='gp-save-stack-') as folder:
    for filename in ('gp_save.c','weekend.c'):
        entry=next(e for e in entries if Path(e['file']).name==filename)
        target=Path(folder)/(filename+'.o')
        args=shlex.split(entry['command']);args[args.index('-o')+1]=str(target);args.append('-fstack-usage')
        subprocess.run(args,cwd=entry.get('directory',str(root)),check=True)
        maximum=0
        for line in target.with_suffix('.su').read_text().splitlines():
            name,size,kind=line.split('\t');maximum=max(maximum,int(size))
            if kind!='static' or int(size)>192:raise SystemExit(f'Storage/championship stack budget exceeded: {line}')
        print(f'PASS ARM {filename}: largest app frame {maximum} bytes (budget 192, callees excluded)')
