"""Reject an ELF without the linked WM device registry (never opens hardware)."""
from pathlib import Path
import argparse,json,struct
from elftools.elf.elffile import ELFFile

REQUIRED={'rcc','dma','gpio','uart0','uart3','iflash','pmu','rng'}

def audit(path):
    with Path(path).open('rb') as handle:
        elf=ELFFile(handle)
        symbols={s.name:s for s in elf.get_section_by_name('.symtab').iter_symbols()}
        start=symbols['wm_dt_table_start']['st_value'];end=symbols['wm_dt_table_end']['st_value']
        if end<=start or (end-start)%12:
            raise ValueError('Device registry is empty/invalid: generated wm_dt_hw.c was not linked')
        def read(address,count):
            for section in elf.iter_sections():
                if section['sh_flags']&2 and section['sh_type']!='SHT_NOBITS' and section['sh_addr']<=address and address+count<=section['sh_addr']+section['sh_size']:
                    offset=address-section['sh_addr'];return section.data()[offset:offset+count]
            raise ValueError(f'Address outside initialized ELF sections: {address:#x}')
        def string(address):
            output=bytearray()
            for i in range(64):
                value=read(address+i,1)[0]
                if value==0:return output.decode('ascii')
                output.append(value)
            raise ValueError('Invalid device name')
        tables=[];names=set()
        for address in range(start,end,12):
            name_ptr,count,entries=struct.unpack('<III',read(address,12))
            if not 1<=count<=256:raise ValueError('Invalid device entry count')
            devices=[]
            for i in range(count):
                dev_name,hardware,ops=struct.unpack('<III',read(entries+12*i,12))
                name=string(dev_name);read(hardware,2);read(ops,4)
                names.add(name);devices.append(name)
            tables.append(dict(table_name=string(name_ptr),entries=count,devices=devices))
        if not REQUIRED<=names:raise ValueError('Missing startup devices: '+repr(REQUIRED-names))
        return dict(elf=str(Path(path).resolve()),registry_start=hex(start),registry_end=hex(end),
                    registry_bytes=end-start,devices=sorted(names),tables=tables)

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('elf',type=Path)
    args=parser.parse_args();print(json.dumps(audit(args.elf),ensure_ascii=False,indent=2))
