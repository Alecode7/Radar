"""WM-IDE build entry. Uses SDK build, then audits this project's image."""
from pathlib import Path
from datetime import datetime
import binascii,hashlib,json,os,re,struct,subprocess,sys
from audit_device_table import audit
ROOT=Path(__file__).resolve().parents[1]
SDK=ROOT.parent/'w803_sdk/wm_iot_sdk'
VS=Path('C:/Program Files/Microsoft Visual Studio/18/Community/Common7/IDE/CommonExtensions/Microsoft/CMake')
VENV=ROOT.parent/'tools/w803_env/Scripts'
TOOLCHAIN=Path('D:/C-Sky/CDKRepo/Toolchain/CKV2ElfMinilib/V3.10.32/R/bin')
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def source_hashes():
    files=[p for folder in ('main','BSP','Drivers','Config','App','Algorithm') for p in (ROOT/folder).rglob('*') if p.is_file()]
    files += [ROOT/n for n in ('CMakeLists.txt','prj.config','device_table.toml')]
    return {p.relative_to(ROOT).as_posix():sha(p) for p in sorted(files)}
def image_audit(path):
    data=Path(path).read_bytes();offset=0;types=[]
    while offset<len(data):
        h=data[offset:offset+64];assert len(h)==64
        magic,kind,compression,run,length,address,update,crc=struct.unpack_from('<IHH5I',h)
        assert magic==0xa0ffff9f and compression==0 and length%4==0
        assert (binascii.crc32(h[:60])^0xffffffff)==struct.unpack_from('<I',h,60)[0]
        payload=data[offset+64:offset+64+length]
        assert len(payload)==length and (binascii.crc32(payload)^0xffffffff)==crc
        types.append(kind);offset+=64+length
    assert offset==len(data) and types==[0,1,2]
def main():
    logs=ROOT/'build-logs';logs.mkdir(exist_ok=True);log=logs/('ide_'+datetime.now().strftime('%Y%m%d_%H%M%S')+'.log')
    env=os.environ.copy();env['WM_IOT_SDK_PATH']=str(SDK);env['PYTHONUTF8']='1';env['PYTHONIOENCODING']='utf-8'
    env['PATH']=os.pathsep.join(map(str,(VENV,VS/'CMake/bin',VS/'Ninja',TOOLCHAIN)))+os.pathsep+env['PATH']
    before=source_hashes()
    command=[str(VENV/'python.exe'),'-B',str(SDK/'tools/wm/wm.py'),'build']
    with log.open('w',encoding='utf-8') as f:
        process=subprocess.Popen(command,cwd=ROOT,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,encoding='utf-8',errors='replace')
        for line in process.stdout:print(line,end='',flush=True);f.write(line)
        code=process.wait()
    if code:return code
    assert before==source_hashes(),'Source changed during compilation; build again after saving'
    assert not re.search(r'\b(?:warning|error):',log.read_text(encoding='utf-8'),re.I)
    image=ROOT/'build/w803_link.fls';image_audit(image)
    registry=audit(ROOT/'build/w803_link.elf');assert 'uart3' in registry['devices']
    header=(ROOT/'Config/board_config.h').read_text(encoding='utf-8')
    firmware_id=re.search(r'#define\s+LINK_FIRMWARE_ID\s+"([^"]+)"',header)[1]
    assert firmware_id.encode() in image.read_bytes()
    report=dict(firmware_id=firmware_id,image=str(image),sha256=sha(image),bytes=image.stat().st_size,
                source_hashes=before,linked_device_registry=registry,build_log=str(log),serial_opened=False,flashed=False)
    (ROOT/'build-verification.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    print('\nBUILD VERIFIED:',firmware_id,'\nIMAGE:',image,'\nSHA256:',report['sha256'],flush=True)
    return 0
if __name__=='__main__':sys.exit(main())
