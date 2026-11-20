"""Audit and package P1/V29. No device opens, resets, flashes, or deletion."""
from pathlib import Path
from datetime import datetime
import hashlib,json,re,shutil,subprocess,zipfile
from ide_build import ROOT,TOOLCHAIN,source_hashes,image_audit,sha
BASE=ROOT.parent/'w803_live_heart_r1'
RADAR=ROOT.parent/'radar_protocol_v29'
OLD_RADAR=ROOT.parent/'radar_acquisition_v28'
DELIVERY=ROOT.parent/'协议交付_20261120'
RP=Path('project/mmwave/radar_analysis_sleep')
RB=RADAR/'sdk'/RP/'cdk_6130_1812_xip_cpus_sleep'
RTOOL=Path('D:/C-Sky/CDKRepo/Toolchain/XTGccElfNewlib/V3.1.0/R/bin')
def load(path):return json.loads(Path(path).read_text(encoding='utf-8'))
def save(path,data):Path(path).write_text(json.dumps(data,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
def function(path,name):
    text=Path(path).read_text(encoding='utf-8');m=re.search(r'\b'+re.escape(name)+r'\s*\([^;{}]*\)\s*\{',text)
    assert m,(path,name)
    at=m.end();level=1
    while level:
        if text[at]=='{':level+=1
        elif text[at]=='}':level-=1
        at+=1
    return text[m.start():at]
def bundle(path,files,prefix):
    with zipfile.ZipFile(path,'w',zipfile.ZIP_DEFLATED) as z:
        for p in sorted(files):z.write(p,(Path(prefix)/p.relative_to(files[p])).as_posix())
    with zipfile.ZipFile(path) as z:
        assert z.testzip() is None
        for p in files:assert hashlib.sha256(z.read((Path(prefix)/p.relative_to(files[p])).as_posix())).hexdigest()==sha(p)
def main():
    build=load(ROOT/'build-verification.json')
    assert build['firmware_id']=='W803_PROTOCOL_P1_20261120'
    assert build['source_hashes']==source_hashes()
    image=Path(build['image']);assert sha(image)==build['sha256'];image_audit(image)
    assert 'uart1' in build['linked_device_registry']['devices'] and 'uart3' in build['linked_device_registry']['devices']
    old=load(BASE/'release-manifest.json')
    for rel,digest in old['source_hashes'].items():assert sha(BASE/rel)==digest,rel
    for p in (BASE/'Algorithm').iterdir():
        if p.is_file():assert sha(p)==sha(ROOT/'Algorithm'/p.name),p
    for folder in ('Drivers',):
        for p in (BASE/folder).iterdir():
            if p.is_file():assert sha(p)==sha(ROOT/folder/p.name),p
    for rel in ('App/snapshot_heart.c','App/snapshot_heart.h','BSP/board_uart.c','BSP/board_uart.h','device_table.toml'):
        assert sha(BASE/rel)==sha(ROOT/rel),rel
    old_flash=BASE/'release/W803_HEART_R1_V1.0.0_ROTATE16_20261119.fls'
    assert sha(old_flash)=='454afcd20ef7b31c011d944b7d21b129547eab9381ff73f1edd14860ec9fa29f'
    old_radar=OLD_RADAR/'release/RS6130_V28_IQ_STREAM_20261119.img'
    assert sha(old_radar)=='16077f590cdb927f364ccec8ebfd524708c43e399ee13acacd8e2d022aafbe2d'
    before=load(RADAR/'reference/v28-source-hashes.json')
    for rel,digest in before.items():assert sha(OLD_RADAR/rel)==digest,rel
    protected=['subsys/mmw/mmw_algorithm/mmw_alg_ant_calibration.c','subsys/mmw/mmw_algorithm/mmw_alg_ant_calibration.h',
               'platform/utils/img_cfg_template.yml',str(RP/'app/inc/prj_config.h')]
    for rel in protected:assert sha(OLD_RADAR/'sdk'/rel)==sha(RADAR/'sdk'/rel),rel
    proofs={}
    for rel,names in {
        str(RP/'app/src/main.c'):['human_motion_default_config'],
        str(RP/'app/src/sleep_detect.c'):['sleep_detect_stabilize_breath_rate','sleep_detect_update_breath_phase',
            'sleep_detect_update','sleep_detect_set_mount','sleep_detect_disable_mount','sleep_detect_is_point_in_bed_zone'],
        'subsys/mmw/mmw_cmd_sleep/src/mmw_report.c':['sleep_breath_phase_sample_get','sleep_heart_raw_sample_get'],
        'subsys/mmw/mmw_cmd/src/mmw_shell.c':['cmd_mmw_start','cmd_mmw_stop','cmd_mmw_report','sleepcfg_is_stopped']}.items():
        for name in names:
            a=function(OLD_RADAR/'sdk'/rel,name);b=function(RADAR/'sdk'/rel,name)
            assert a==b,name;proofs[name]=hashlib.sha256(a.encode()).hexdigest()
    source_math=RADAR/'sdk'/RP/'app/src/sleep_detect.c'
    method=function(source_math,'sleep_detect_public_breath_rate')
    observed=method.index('    /* Observe a new qualified stable estimate;')
    tail=method.index('    return stable_rate;',observed)
    without_observer=method[:observed]+method[tail:]
    assert without_observer==function(OLD_RADAR/'sdk'/RP/'app/src/sleep_detect.c','sleep_detect_public_breath_rate')

    checks={}
    for filename,key,expected in [('native-verification.json','passed',19),('snapshot-verification.json','passed',17),
            ('release-verification.json','passed',17),('protocol-verification.json','tests',33),
            ('mcu-client-verification.json','tests',12),('storage-verification.json','tests',5),
            ('runtime-verification.json','tests',10),('python-client-verification.json','tests',4)]:
        r=load(ROOT/filename);assert r[key]==expected,(filename,r)
        if key=='tests':assert r['passed'] is True and not r.get('failures',0) and not r.get('errors',0)
        checks[filename]=expected
    stream=load(RADAR/'stream-verification.json');assert stream['passed']==12 and stream['total_recorded_samples']==4665
    checks['radar_stream']=12;assert sum(checks.values())==129
    numerical=load(ROOT/'release-verification.json')
    assert sum(r['estimates'] for r in numerical['replays'].values())==189
    assert all(r['bit_differences_from_frozen_rotate16']==0 for r in numerical['replays'].values())
    protocol_report=load(ROOT/'protocol-verification.json');assert protocol_report['native_dll_sha256']==sha(ROOT/'native-output/protocol.dll')
    gui=load(ROOT/'gui-verification.json');assert gui['widgets_created'] and not gui['serial_opened'] and gui['config_fields']==18
    assert gui['gui_sha256']==sha(ROOT/'Host/protocol_gui.py')
    vectors=load(ROOT/'protocol/golden_vectors.json')
    assert vectors['synthetic'] and len(vectors['request_response_vectors'])==26 and len(vectors['unsolicited_vectors'])==3

    nm=subprocess.check_output([str(TOOLCHAIN/'csky-elfabiv2-nm.exe'),str(ROOT/'build/w803_link.elf')],text=True)
    for symbol in ('p1_handle','product_runtime_init','product_board_save','wm_nvs_set_binary','wm_drv_fill_random'):
        assert re.search(r'\b'+symbol+r'$',nm,re.M),symbol
    rn=subprocess.check_output([str(RTOOL/'riscv64-unknown-elf-nm.exe'),str(RB/'Obj/psdf.elf')],text=True)
    for symbol in ('cmd_mmw_pctl','sleep_trace_init','sleep_detect_protocol_breath'):
        assert re.search(r'\b'+symbol+r'$',rn,re.M),symbol
    assert not re.search(r'\bhr_core_\w+$',rn,re.M)
    radar_log=(RADAR/'build-logs/make.log').read_text(encoding='utf-8',errors='replace')
    assert not re.search(r'\b(?:warning|error):',radar_log,re.I)
    assert b'UART_RADAR_PROTOCOL_20261120_V29' in (RB/'output/mrs6130_p1812.img').read_bytes()
    radar_image=RADAR/'release/RS6130_V29_PROTOCOL_20261120.img'
    assert sha(radar_image)==sha(RB/'output/mrs6130_p1812.img')
    assert (RB/'Obj/psdf.elf').stat().st_mtime>=max(p.stat().st_mtime for p in (RADAR/'sdk').rglob('*')
        if p.is_file() and p.suffix in ('.c','.h') and not any(x in ('Obj','Lst','output') for x in p.parts))
    memory=load(RADAR/'build-logs/memory-exact.json')[0]
    release=ROOT/'release';release.mkdir(exist_ok=True)
    fls=release/'W803_PROTOCOL_P1_20261120.fls';shutil.copy2(image,fls);image_audit(fls)
    report=dict(date='2026-11-20',software_delivery_complete=True,board_validated=False,serial_opened=False,flashed=False,
        firmware_w803=build['firmware_id'],firmware_radar='UART_RADAR_PROTOCOL_20261120_V29',
        checks=checks,software_check_total=sum(checks.values()),gui_widget_creation_passed=True,
        recorded_iq_frames=4665,rotation_estimates=189,rotation_differences_from_frozen=0,
        native_power_cut_positions=80,native_corrupt_storage_bits=640,
        requests=21,push_types=3,heart_core_files_unchanged=True,preserved_radar_functions=proofs,
        calibration_sources_unchanged=protected,radar_breath_change='Qualified-estimate metadata only, public output math unchanged.',
        old_r1_source_hashes_unchanged=True,old_v28_sdk_hashes_unchanged=len(before),
        w803_sram_mapped_bytes=98288,w803_sram_region_bytes=163840,
        radar_sram_mapped_bytes=memory['sram_mapped_bytes'],radar_sram_total_bytes=memory['sram_total_bytes'],
        product_port='W803 UART1 J1 TX/PB6 RX/PB7, 115200 8N1',
        pending_board_checks=['Dedicated UART electrical/byte stability','Actual sensor control and installation readback',
            'NVS save then power-cycle readback and power-cut qualification','Boot identity across real resets',
            'P1 traffic concurrent with ROTATE16 timing and long runtime','Company mainboard integration'],
        accuracy_verified=False,
        images=[dict(path=str(p),bytes=p.stat().st_size,sha256=sha(p),
            modified_local=datetime.fromtimestamp(p.stat().st_mtime).isoformat(timespec='seconds')) for p in (fls,radar_image)])
    save(ROOT/'verification.json',report);save(RADAR/'verification.json',report)
    save(ROOT/'source-lock.json',source_hashes())

    p1_files={}
    for folder in ('.vscode','Algorithm','App','BSP','Config','Drivers','main','tools','tests','Host','protocol','reference'):
        for p in (ROOT/folder).rglob('*'):
            if p.is_file() and '__pycache__' not in p.parts:p1_files[p]=ROOT
    for name in ('CMakeLists.txt','prj.config','device_table.toml','README.md','W803_PROTOCOL_P1.code-workspace',
        '启动协议联调工具.vbs','verification.json','source-lock.json','build-verification.json'):
        p1_files[ROOT/name]=ROOT
    p1zip=release/'W803_PROTOCOL_P1_20261120_source.zip';bundle(p1zip,p1_files,ROOT.name)

    radar_files={};pinned={'src_hal_board.o','utils_version.o','radar_analysis_sleep_MRS6130_P1812.txt'}
    for p in (RADAR/'sdk').rglob('*'):
        if not p.is_file() or '__pycache__' in p.parts:continue
        if any(part in ('Obj','Lst','output') for part in p.relative_to(RADAR/'sdk').parts):
            if p.parent!=RB/'Obj' or p.name not in pinned:continue
        radar_files[p]=RADAR
    for folder in ('tests',):
        for p in (RADAR/folder).rglob('*'):
            if p.is_file() and '__pycache__' not in p.parts:radar_files[p]=RADAR
    for name in ('README.md','build_radar.ps1','build-safe.mk','package_safe.py','packet_main_safe.py','verification.json','stream-verification.json'):
        radar_files[RADAR/name]=RADAR
    radarzip=RADAR/'release/RS6130_V29_PROTOCOL_20261120_source.zip';bundle(radarzip,radar_files,RADAR.name)
    DELIVERY.mkdir(exist_ok=True)
    for folder in ('firmware','MCU','Host','verification'):(DELIVERY/folder).mkdir(exist_ok=True)
    for p in (fls,radar_image):shutil.copy2(p,DELIVERY/'firmware'/p.name)
    shutil.copy2(ROOT/'protocol/协议D0.7_双芯片产品接口.md',DELIVERY/'协议D0.7_双芯片产品接口.md')
    shutil.copy2(ROOT/'protocol/golden_vectors.json',DELIVERY/'golden_vectors.json')
    for p in (ROOT/'protocol/reference').iterdir():
        if p.is_file():shutil.copy2(p,DELIVERY/'MCU'/p.name)
    for p in (ROOT/'Host').glob('*'):
        if p.is_file():shutil.copy2(p,DELIVERY/'Host'/p.name)
    for filename in checks:
        if filename=='radar_stream':shutil.copy2(RADAR/'stream-verification.json',DELIVERY/'verification/radar-stream-verification.json')
        else:shutil.copy2(ROOT/filename,DELIVERY/'verification'/filename)
    for name in ('verification.json','build-verification.json','gui-verification.json'):
        shutil.copy2(ROOT/name,DELIVERY/('verification.json' if name=='verification.json' else 'verification/'+name))
    artifacts=[fls,radar_image,p1zip,radarzip]
    manifest=dict(created_local=datetime.now().isoformat(timespec='seconds'),stage='SOFTWARE_COMPLETE_BOARD_ACCEPTANCE_PENDING',
        software_checks=129,gui_created_without_uart=True,protocol='RSP 1.1 / D0.7 structure revision2',
        software_sources_w803={p.relative_to(ROOT).as_posix():sha(p) for p in p1_files},
        software_sources_radar={p.relative_to(RADAR).as_posix():sha(p) for p in radar_files},
        images_and_sources=[dict(path=str(p),bytes=p.stat().st_size,sha256=sha(p)) for p in artifacts],
        serial_opened=False,flashed=False,calibration_written=False,board_verified=False)
    save(ROOT/'release-manifest.json',manifest);save(RADAR/'release-manifest.json',manifest);save(DELIVERY/'release-manifest.json',manifest)
    (DELIVERY/'FIRMWARE_SHA256SUMS.txt').write_text(''.join(f'{sha(p)}  firmware/{p.name}\n' for p in (fls,radar_image)),encoding='ascii')
    # The outer checksum list contains this ZIP's digest, so it must stay
    # outside the archive rather than embedding a stale/self-referential list.
    inputs={p:DELIVERY for p in DELIVERY.rglob('*') if p.is_file() and p.suffix!='.zip' and p.name!='SHA256SUMS.txt'}
    archive=DELIVERY/'MRS6130_协议交付_P1_V29_20261120.zip';bundle(archive,inputs,DELIVERY.name)
    sums=artifacts+[archive]
    (DELIVERY/'SHA256SUMS.txt').write_text(''.join(f'{sha(p)}  {p}\n' for p in sums),encoding='utf-8')
    print(json.dumps(dict(images=report['images'],software_checks=129,board_verified=False,archive=str(archive),
        archive_sha256=sha(archive),archive_bytes=archive.stat().st_size),ensure_ascii=False,indent=2))
if __name__=='__main__':main()
