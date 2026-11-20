"""Verify V28 and freeze release evidence. No hardware and no file deletion."""
from pathlib import Path
from datetime import datetime
import hashlib,json,re,shutil,subprocess,zipfile
ROOT=Path(__file__).resolve().parent
BASE=Path('D:/REA/workspace/heart-chip-adapt-v27-20261113')
SDK=ROOT/'sdk'
PROJECT=SDK/'project/mmwave/radar_analysis_sleep'
BUILD=PROJECT/'cdk_6130_1812_xip_cpus_sleep'
TOOLCHAIN=Path('D:/C-Sky/CDKRepo/Toolchain/XTGccElfNewlib/V3.1.0/R/bin')
NAME='RS6130_V28_IQ_STREAM_20261119'
def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def load(path):return json.loads(Path(path).read_text(encoding='utf-8'))
def save(path,data):Path(path).write_text(json.dumps(data,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')

before=load(ROOT/'baseline-v27-hashes.json')
for rel,digest in before.items():assert sha(BASE/rel)==digest,rel
source_extensions={'.c','.h','.ld','.yml','.yaml','.sym','.a'}
changes=[];preserved=0
for rel,digest in before.items():
    p=Path(rel)
    if any(part in ('Obj','Lst','output') for part in p.parts):continue
    if p.suffix not in source_extensions and p.name not in ('project.cdkproj','after_build.sh','radar_analysis_sleep_MRS6130_P1812.mk'):continue
    if sha(ROOT/rel)!=digest:changes.append(rel)
    else:preserved+=1
expected={
 'sdk/project/mmwave/radar_analysis_sleep/app/src/main.c',
 'sdk/project/mmwave/radar_analysis_sleep/app/src/v6_heart_device.c',
 'sdk/project/mmwave/radar_analysis_sleep/app/inc/v6_heart_device.h',
 'sdk/project/mmwave/radar_analysis_sleep/cdk_6130_1812_xip_cpus_sleep/project.cdkproj',
 'sdk/project/mmwave/radar_analysis_sleep/cdk_6130_1812_xip_cpus_sleep/after_build.sh',
 'sdk/project/mmwave/radar_analysis_sleep/cdk_6130_1812_xip_cpus_sleep/radar_analysis_sleep_MRS6130_P1812.mk'}
assert set(changes)==expected,changes
for filename in ('src_hal_board.o','utils_version.o'):
    assert sha(BUILD/'Obj'/filename)==sha(BASE/'sdk/project/mmwave/radar_analysis_sleep/cdk_6130_1812_xip_cpus_sleep/Obj'/filename)
assert (ROOT/'source/v6_heart_device.c').read_bytes()==(PROJECT/'app/src/v6_heart_device.c').read_bytes()
image=ROOT/'release'/(NAME+'.img')
assert image.read_bytes()==(BUILD/'output/mrs6130_p1812.img').read_bytes()
assert b'UART_RADAR_IQ_20261119_V28' in image.read_bytes()
assert b'MODE=IQ_STREAM HEART_COMPUTE=0 OWNER=W803' in image.read_bytes()
assert b'HRIN V=27 ' in image.read_bytes() and b'ENQ=0' in image.read_bytes()
assert b'HREST V=27' not in image.read_bytes() and b'HRDEV VER=1' not in image.read_bytes()
rollback=ROOT/'reference/RS6130_V27_rollback.img'
assert sha(rollback)=='0c4f7db2ff9d991350873e05b35437e4cef8e761e92846657986f8669fd09364'

symbols=subprocess.check_output([str(TOOLCHAIN/'riscv64-unknown-elf-nm.exe'),'-S',str(BUILD/'Obj/psdf.elf')],text=True)
adapter_symbols=subprocess.check_output([str(TOOLCHAIN/'riscv64-unknown-elf-nm.exe'),'-u',str(BUILD/'Obj/src_v6_heart_device.o')],text=True)
assert not re.search(r'\bhr_core_\w+\s*$',symbols,re.M)
assert not re.search(r'\b(?:xTaskCreate|xQueue\w+|pvPortMalloc|vPortFree|hr_core_\w+)\s*$',adapter_symbols,re.M)
(ROOT/'build-logs/adapter-undefined-symbols.txt').write_text(adapter_symbols,encoding='ascii')
log=(ROOT/'build-logs/make.log').read_text(encoding='utf-8',errors='replace')
assert not re.search(r'\b(?:warning|error):|undefined reference',log,re.I)
stream=load(ROOT/'stream-verification.json')
assert stream['passed']==12 and stream['total_recorded_samples']==4665
assert (ROOT/'stream-verification.json').stat().st_mtime>=(ROOT/'native-output/stream.dll').stat().st_mtime
memory=load(ROOT/'build-logs/memory-exact.json')[0]
assert memory['sram_mapped_bytes']==260352 and memory['sram_total_bytes']==301056
source_files=[p for p in SDK.rglob('*') if p.is_file() and p.suffix in source_extensions
              and not any(part in ('Obj','Lst','output') for part in p.relative_to(SDK).parts)]
assert (BUILD/'Obj/psdf.elf').stat().st_mtime>=max(p.stat().st_mtime for p in source_files)
report=dict(created_local=datetime.now().isoformat(timespec='seconds'),marker='UART_RADAR_IQ_20261119_V28',
    target_build_passed=True,source_baseline='frozen V27 SDK and published image',
    source_edits=changes,preserved_source_or_library_files=preserved,protected_v27_sdk_files=len(before),
    unchanged_pinned_objects=['src_hal_board.o','utils_version.o'],
    heart_core_linked=False,adapter_creates_queue=False,adapter_creates_worker=False,adapter_allocates_workspace=False,
    wire_schema='HRIN V=27',local_queue_flag='ENQ=0',compatible_w803='R1 V1.0.0 parser unchanged',
    tests=12,recorded_payload_roundtrips=4665,payload_field_differences=0,
    sram_mapped_bytes=memory['sram_mapped_bytes'],sram_total_bytes=memory['sram_total_bytes'],
    v27_sram_mapped_bytes=294784,sram_mapping_reduction_bytes=294784-memory['sram_mapped_bytes'],
    image=dict(path=str(image),bytes=image.stat().st_size,sha256=sha(image),
        last_write_local=datetime.fromtimestamp(image.stat().st_mtime).isoformat(timespec='seconds')),
    target_realtime_verified=False,human_accuracy_verified=False,serial_opened=False,flashed=False,calibration_written=False)
save(ROOT/'verification.json',report)

release=ROOT/'release'
debug=release/'debug';debug.mkdir(exist_ok=True)
for src,dst in ((BUILD/'Obj/psdf.elf',debug/(NAME+'.elf')),(BUILD/'Lst/psdf.map',debug/(NAME+'.map'))):shutil.copy2(src,dst)
evidence=release/'verification';evidence.mkdir(exist_ok=True)
for rel in ('verification.json','stream-verification.json','build-logs/memory-exact.json','build-logs/adapter-undefined-symbols.txt'):
    shutil.copy2(ROOT/rel,evidence/Path(rel).name)
shutil.copy2(ROOT/'README.md',release/'使用说明.md')

# The source bundle contains the SDK and two locked board/version objects.
# Generated old kernels/ELFs are excluded, never deleted from the workspace.
pinned={'src_hal_board.o','utils_version.o','radar_analysis_sleep_MRS6130_P1812.txt'}
bundle=[]
for p in SDK.rglob('*'):
    if not p.is_file() or '__pycache__' in p.parts:continue
    if any(part in ('Obj','Lst','output') for part in p.relative_to(SDK).parts):
        if p.parent!=BUILD/'Obj' or p.name not in pinned:continue
    bundle.append(p)
for folder in ('tests','source'):
    bundle.extend(p for p in (ROOT/folder).rglob('*') if p.is_file() and '__pycache__' not in p.parts)
bundle.extend(ROOT/n for n in ('README.md','build_radar.ps1','build-safe.mk','package_safe.py','packet_main_safe.py','verify_release.py',
    'baseline-v27-hashes.json','verification.json','stream-verification.json'))
source_archive=release/(NAME+'_source.zip')
with zipfile.ZipFile(source_archive,'w',zipfile.ZIP_DEFLATED) as archive:
    for p in sorted(bundle):archive.write(p,Path(ROOT.name)/p.relative_to(ROOT))
with zipfile.ZipFile(source_archive) as archive:
    assert archive.testzip() is None
    for p in bundle:assert hashlib.sha256(archive.read((Path(ROOT.name)/p.relative_to(ROOT)).as_posix())).hexdigest()==sha(p)
artifacts=[image,source_archive]+sorted(debug.iterdir())
manifest=dict(marker=report['marker'],flashed=False,board_verified=False,paired_w803='W803_HEART_R1_V1.0.0_ROTATE16_20261119',
    radar_wire_schema=27,local_heart_compute=False,
    artifacts=[dict(path=str(p.relative_to(ROOT)),bytes=p.stat().st_size,sha256=sha(p)) for p in artifacts],
    source_hashes={p.relative_to(ROOT).as_posix():sha(p) for p in sorted(bundle)},
    rollback=dict(path=str(rollback),bytes=rollback.stat().st_size,sha256=sha(rollback)),verification='verification.json')
save(ROOT/'release-manifest.json',manifest);save(release/'release-manifest.json',manifest)
(release/'SHA256SUMS.txt').write_text(''.join(f'{sha(p)}  {p.relative_to(release).as_posix()}\n' for p in artifacts),encoding='ascii')
print(json.dumps(report,ensure_ascii=False,indent=2))
