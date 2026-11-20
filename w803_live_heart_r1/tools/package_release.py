"""Freeze a verified local R1 build. No UART, reset, or flashing actions."""
from pathlib import Path
from datetime import datetime
import hashlib,json,re,shutil,subprocess,zipfile
from ide_build import ROOT,TOOLCHAIN,image_audit,sha,source_hashes

NAME='W803_HEART_R1_V1.0.0_ROTATE16_20261119'

def read(name):return json.loads((ROOT/name).read_text(encoding='utf-8'))
def write(path,data):path.write_text(json.dumps(data,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')

def main():
    build=read('build-verification.json')
    assert build['firmware_id']==NAME and build['source_hashes']==source_hashes()
    image=Path(build['image'])
    assert sha(image)==build['sha256']
    image_audit(image)
    core=read('reference/frozen-core.json')['unchanged_algorithm_files']
    for filename,digest in core.items():
        assert sha(ROOT/'Algorithm'/filename)==digest
        assert sha(ROOT.parent/'w803_live_heart_c4/Algorithm'/filename)==digest
    # Pin and transport contracts must stay those of the tested C4 build.
    for folder in ('BSP','Drivers'):
        for path in (ROOT/folder).iterdir():
            if path.is_file():assert path.read_bytes()==(ROOT.parent/'w803_live_heart_c4'/folder/path.name).read_bytes()
    assert (ROOT/'device_table.toml').read_bytes()==(ROOT.parent/'w803_live_heart_c4/device_table.toml').read_bytes()
    assert 'uart3' in build['linked_device_registry']['devices']

    native=read('native-verification.json')
    fallback=read('snapshot-verification.json')
    rotation=read('release-verification.json')
    assert (native['passed'],fallback['passed'],rotation['passed'])==(19,17,17)
    assert rotation['default']=='ROTATE16'
    assert sum(v['estimates'] for v in rotation['replays'].values())==189
    assert sum(v['samples'] for v in rotation['replays'].values())==4665
    for replay in rotation['replays'].values():assert replay['bit_differences_from_frozen_rotate16']==0
    dll=ROOT/'native-output/rxstate.dll'
    assert dll.stat().st_mtime>=max((ROOT/n).stat().st_mtime for n in source_hashes() if n.endswith(('.c','.h')))
    for report in ('native-verification.json','snapshot-verification.json','release-verification.json'):
        assert (ROOT/report).stat().st_mtime>=dll.stat().st_mtime

    commands=read('build/compile_commands.json')
    app=[c for c in commands if '/components/main/' in c['output'].replace('\\','/')]
    assert len(app)==7,len(app)
    for c in app:
        assert '-DHR_DEFAULT_FREQ_ROTATION=1' in c['command']
        assert '-ffp-contract=off' in c['command'] and '-fno-fast-math' in c['command']
        assert '-mcpu=ck804ef' in c['command'] and '-mhard-float' in c['command']
        assert not re.search(r'auto_compare|shared_sincos',c['file'])
    symbols=subprocess.run([str(TOOLCHAIN/'csky-elfabiv2-nm.exe'),str(ROOT/'build/w803_link.elf')],
        check=True,capture_output=True,text=True).stdout
    assert not re.search(r'\b(?:ac_\w+|shared_sincos\w*)\s*$',symbols,re.M)
    assert 'DEFAULT_MODE=ROTATE16'.encode() in image.read_bytes()
    assert 'AUTO_COMPARE=0'.encode() in image.read_bytes()

    release=ROOT/'release';release.mkdir(exist_ok=True)
    firmware=release/(NAME+'.fls')
    shutil.copy2(image,firmware);assert sha(firmware)==build['sha256'];image_audit(firmware)
    debug=release/'debug';debug.mkdir(exist_ok=True)
    for suffix in ('elf','map'):shutil.copy2(ROOT/f'build/w803_link.{suffix}',debug/f'{NAME}.{suffix}')
    evidence=release/'verification';evidence.mkdir(exist_ok=True)
    for filename in ('build-verification.json','native-verification.json','snapshot-verification.json','release-verification.json'):
        shutil.copy2(ROOT/filename,evidence/filename)
    shutil.copy2(ROOT/'README.md',release/'使用说明.md')

    project_sources=[]
    for folder in ('.vscode','Algorithm','App','BSP','Config','Drivers','main','tools','tests','reference'):
        project_sources.extend(p for p in (ROOT/folder).rglob('*') if p.is_file() and '__pycache__' not in p.parts)
    project_sources.extend(ROOT/n for n in ('CMakeLists.txt','prj.config','device_table.toml','README.md','W803_HEART_R1.code-workspace'))
    source_zip=release/(NAME+'_source.zip')
    with zipfile.ZipFile(source_zip,'w',zipfile.ZIP_DEFLATED) as archive:
        for path in sorted(project_sources):archive.write(path,Path(ROOT.name)/path.relative_to(ROOT))
    with zipfile.ZipFile(source_zip) as archive:
        assert archive.testzip() is None
        for path in project_sources:
            archived=archive.read((Path(ROOT.name)/path.relative_to(ROOT)).as_posix())
            assert hashlib.sha256(archived).hexdigest()==sha(path)
    artifacts=[firmware,source_zip]+sorted(p for p in debug.iterdir() if p.is_file())
    manifest=dict(version='1.0.0',project=str(ROOT),firmware_id=NAME,
        packaged_local_time=datetime.now().isoformat(timespec='seconds'),default_mode='ROTATE16',reanchor=16,
        precision='double',automatic_comparison=False,shared_reduction_enabled=False,
        direct_runtime_fallback=True,radar_firmware='existing V27 retained',serial_opened=False,flashed=False,
        engineering_checks=53,recorded_replay_samples_per_mode=4665,recorded_replay_estimates_per_mode=189,
        rotation_scope='Matches frozen ROTATE16 PC DLL; not numerical equivalence to DIRECT.',
        board_scope='No R1 board run yet. C4 had one 300-sample shadow ROTATE16 timing of 2132 ms.',
        pending=['R1 single-mode continuous board timing and recovery','Synchronous-reference accuracy'],
        application_compile_units=len(app),linked_automatic_comparison_symbols=False,
        source_hashes=source_hashes(),
        package_source_hashes={p.relative_to(ROOT).as_posix():sha(p) for p in sorted(project_sources)},
        artifacts=[dict(file=str(p.relative_to(ROOT)),bytes=p.stat().st_size,sha256=sha(p)) for p in artifacts],
        regression_reports=['native-verification.json','snapshot-verification.json','release-verification.json'],
        build_report='build-verification.json')
    write(ROOT/'release-manifest.json',manifest)
    write(release/'release-manifest.json',manifest)
    (release/'SHA256SUMS.txt').write_text(''.join(f"{sha(p)}  {p.relative_to(release).as_posix()}\n" for p in artifacts),encoding='utf-8')
    print(json.dumps(dict(firmware=str(firmware),bytes=firmware.stat().st_size,sha256=sha(firmware),
        source_package=str(source_zip),engineering_checks=53,flashed=False),ensure_ascii=False,indent=2))
    return 0

if __name__=='__main__':raise SystemExit(main())
