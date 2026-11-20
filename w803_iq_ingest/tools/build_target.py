"""Compile C-Sky objects and verify sample layout; produces no flash image."""
from pathlib import Path
import hashlib,json,subprocess
from elftools.elf.elffile import ELFFile
ROOT=Path(__file__).resolve().parents[1]
COMPILER=Path('D:/C-Sky/CDKRepo/Toolchain/CKV2ElfMinilib/V3.10.32/R/bin/csky-elfabiv2-gcc.exe')
def main():
    out=ROOT/'target-output';out.mkdir(exist_ok=True)
    commands=[]
    for source,name in [('Drivers/radar_iq.c','radar_iq.o'),('tools/target_probe.c','target_probe.o')]:
        command=[str(COMPILER),'-c','-std=c11','-O2','-Wall','-Wextra','-Werror','-mcpu=ck804ef','-mhard-float',
                 '-I'+str(ROOT/'Drivers'),str(ROOT/source),'-o',str(out/name)]
        p=subprocess.run(command,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True)
        print(source,'exit',p.returncode,flush=True)
        if p.returncode:print(p.stdout);raise SystemExit(p.returncode)
        assert not p.stdout.strip(),p.stdout
        commands.append(command)
    with (out/'target_probe.o').open('rb') as f:
        elf=ELFFile(f);assert elf['e_machine']=='EM_CSKY' and elf.elfclass==32
        values={}
        for s in elf.get_section_by_name('.symtab').iter_symbols():
            if s.name.startswith('radar_iq_') and s['st_size']==4:
                section=elf.get_section(s['st_shndx']);offset=s['st_value']-section['sh_addr']
                values[s.name]=int.from_bytes(section.data()[offset:offset+4],'little')
    assert values==dict(radar_iq_target_bytes=36,radar_iq_time_offset=4,radar_iq_iq_offset=16,radar_iq_flags_offset=30),values
    inputs=['Drivers/radar_iq.c','Drivers/radar_iq.h','tools/target_probe.c']
    outputs=['target-output/radar_iq.o','target-output/target_probe.o']
    hashes=lambda names:{name:hashlib.sha256((ROOT/name).read_bytes()).hexdigest() for name in names}
    report=dict(scope='C-Sky CK804EF object compilation and ABI probe only; no linked firmware or board execution',commands=commands,
                sample_layout=values,input_hashes=hashes(inputs),artifact_hashes=hashes(outputs),serial_opened=False,flashed=False)
    (ROOT/'target-verification.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    print(json.dumps(values))
if __name__=='__main__':main()
