"""Record inputs/artifacts immediately after a successful native build."""
from pathlib import Path
import hashlib,json
ROOT=Path(__file__).resolve().parents[1]
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
inputs=[p for folder in ('Drivers','reference') for p in (ROOT/folder).rglob('*') if p.suffix in ('.c','.h')]
outputs=[ROOT/'native-output/radar_iq.dll',ROOT/'native-output/shared_core.dll']
report=dict(input_hashes={p.relative_to(ROOT).as_posix():sha(p) for p in sorted(inputs)},
            artifact_hashes={p.relative_to(ROOT).as_posix():sha(p) for p in outputs},
            scope='MSVC native DLLs; core built with /fp:strict; not MCU firmware')
(ROOT/'native-verification.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
