"""Verify frozen sources, offline tests, target ABI and replay; never uses UART."""
from pathlib import Path
import hashlib,json,sys,unittest,zipfile
from core_pipeline import verify_pipeline
ROOT=Path(__file__).resolve().parents[1]
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def main():
    manifest=json.loads((ROOT/'source-manifest.json').read_text(encoding='utf-8'))
    for record in manifest['frozen_copies']:
        assert sha(record['source'])==record['sha256'],record['source']
        assert sha(ROOT/record['copy'])==record['sha256'],record['copy']
    for record in manifest['protected_files']:assert sha(record['path'])==record['sha256'],record['path']
    target=json.loads((ROOT/'target-verification.json').read_text(encoding='utf-8'))
    assert target['sample_layout']['radar_iq_target_bytes']==36
    native=json.loads((ROOT/'native-verification.json').read_text(encoding='utf-8'))
    for build in (target,native):
        for key in ('input_hashes','artifact_hashes'):
            for name,digest in build[key].items():assert sha(ROOT/name)==digest,name
    result=unittest.TextTestRunner(verbosity=2).run(unittest.defaultTestLoader.discover(str(ROOT/'tests')))
    if not result.wasSuccessful():return 1
    pipeline=verify_pipeline()
    inputs=[p for folder in ('Drivers','tools','tests') for p in (ROOT/folder).rglob('*') if p.is_file() and '__pycache__' not in p.parts]
    report=dict(date='2026-11-18',scope='Offline IQ parser and unchanged heart-core integration; target .o compilation only',
                tests_passed=result.testsRun,pipeline=pipeline,target_sample_layout=target['sample_layout'],
                frozen_copies_verified=len(manifest['frozen_copies']),protected_files_verified=len(manifest['protected_files']),
                source_hashes={p.relative_to(ROOT).as_posix():sha(p) for p in sorted(inputs)},
                board_firmware_changed=False,linked_firmware_generated=False,live_iq_verified=False,
                target_numeric_equivalence_verified=False,accuracy_verified=False,serial_opened=False,flashed=False,
                build_inputs_and_artifacts_verified=True)
    (ROOT/'verification.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    destination=ROOT/'deliverable';destination.mkdir(exist_ok=True)
    files=[p for p in sorted(ROOT.rglob('*')) if p.is_file() and not {'__pycache__','deliverable'}&set(p.relative_to(ROOT).parts)]
    output=destination/'W803_IQ字段解析与离线验证_20261118.zip'
    with zipfile.ZipFile(output,'w',zipfile.ZIP_DEFLATED) as z:
        for p in files:z.write(p,'w803_iq_ingest/'+p.relative_to(ROOT).as_posix())
    with zipfile.ZipFile(output) as z:assert z.testzip() is None
    print(json.dumps(dict(tests_passed=result.testsRun,pipeline=pipeline,package=str(output)),ensure_ascii=False,indent=2))
    return 0
if __name__=='__main__':sys.exit(main())
