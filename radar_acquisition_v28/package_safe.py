"""Vendor image packaging with no file deletion, including intermediates."""
import os
import shutil
import sys
import json
from datetime import datetime
from pathlib import Path

root = Path(__file__).resolve().parent
sdk = Path(os.environ['SDK_ROOT_DIR'])
build = Path(os.environ['CURRENT_PRJ_PATH'])
preserved = []
archive = root / 'packaging-preserved' / datetime.now().strftime('%Y%m%d_%H%M%S_%f')


def keep_file(path, *args, **kwargs):
    item = Path(path).resolve()
    allowed = (build / 'Obj').resolve()
    if not item.is_relative_to(allowed):
        raise RuntimeError(f'Unexpected removal outside reviewed intermediates: {item}')
    if not item.is_file():
        raise FileNotFoundError(item)
    archive.mkdir(parents=True, exist_ok=True)
    destination = archive / f'{len(preserved):03d}_{item.name}'
    # Move this one explicit file to an archive, preserving its contents.
    shutil.move(str(item), str(destination))
    preserved.append({'source': str(item), 'preserved': str(destination)})


def reject_directory_delete(*args, **kwargs):
    raise RuntimeError('Directory deletion is disabled for this build')


os.remove = keep_file
os.unlink = keep_file
shutil.rmtree = reject_directory_delete
sys.path.insert(0, str(sdk / 'tools/scripts/image_packet'))
import packet_main_safe as packet_module
from packet_main_safe import Packet

# Record exact bytes before the vendor logger rounds them to two decimals.
original_arrange = packet_module.arrange_img
memory_evidence = []
def record_arrange(images, config):
    used = original_arrange(images, config)
    memory_evidence.append(dict(sram_mapped_bytes=used,
        sram_total_bytes=int(str(config['sram-max-size']).lower().replace('k',''))*1024,
        mappings=[dict(name=str(i.get('name','')),remap=i['remap-address']) for i in images]))
    return used
packet_module.arrange_img = record_arrange

Packet.do_run(str(sdk), str(build / 'Obj'), str(sdk / 'platform/utils/img_cfg_template.yml'))
(root / 'build-logs/memory-exact.json').write_text(json.dumps(memory_evidence,indent=2),encoding='utf-8')
(root / 'build-logs/packaging-preserved-last.json').write_text(json.dumps(preserved, indent=2), encoding='utf-8')
print('NO_DELETE_PACKAGING_OK preserved_intermediate_operations=' + str(len(preserved)))
