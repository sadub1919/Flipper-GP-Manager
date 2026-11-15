"""Explicit --update accepts new baselines; normal test runs never rewrite them."""
import hashlib,json,sys
from pathlib import Path
folder=Path(sys.argv[1]);root=Path(__file__).resolve().parent
actual={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(folder.glob('*.pbm'))}
baseline=root/'screenshots.sha256.json'
if '--update' in sys.argv:
    baseline.write_text(json.dumps(actual,indent=2)+'\n')
else:
    expected=json.loads(baseline.read_text())
    changed=[name for name in expected.keys()|actual.keys() if expected.get(name)!=actual.get(name)]
    if changed:raise SystemExit('Screenshot changes require review: '+', '.join(sorted(changed)[:20]))
    print(f'PASS: {len(actual)} screenshot baselines match')
