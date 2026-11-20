"""R1 defaults, selected-mode lifetime, and replay against frozen ROTATE16.

Runs the inherited DIRECT fallback/overload suite first. No serial access.
The DLLs use the PC math library; this verifies packaging and state behavior,
not W803 timing or equivalence between DIRECT and ROTATE16.
"""
from pathlib import Path
import ctypes as C
import hashlib,json,runpy

ROOT=Path(__file__).resolve().parents[1]
api=runpy.run_path(str(ROOT/'tests/check_snapshot.py'))
d,t,e=api['d'],api['t'],api['e']
snapshot=api['snapshot']
stat,push,take,prepare,calculate,finish,report,sample=(api[n] for n in
    ('stat','push','take','prepare','calculate','finish','report','sample'))
Result=api['Result']
checks=[]
d.hr_core_spectrum_mode.argtypes=[C.c_void_p]
d.hr_core_spectrum_mode.restype=C.c_uint

def reset(mode=None):
    d.sh_init(t);d.sh_engine_init(e)
    assert stat().mode==1 and d.hr_core_spectrum_mode(e)==1
    if mode is not None:
        d.sh_set_mode(t,mode)

reset()
assert push(sample())==2 and take()==1 and snapshot.mode==1
prepare();assert d.hr_core_spectrum_mode(e)==1
code,result=calculate();assert code==1 and finish(result,code)==1
assert report()[1].mode==1
checks.append('boot_transport_snapshot_engine_workspace_and_publication_default_to_rotate16')

for mode in (1,0):
    for event in (1,2,3,4):
        reset(mode)
        assert push(sample())==2 and take()==1
        prepare();code,result=calculate();assert finish(result,code)==1
        d.sh_event(t,event)
        assert stat().mode==mode and report()[0]==0
        if event==3:
            assert push(sample(n=2,ms=10200,frame=2))==0
            d.sh_event(t,2)
        assert push(sample(n=2,ms=16000,frame=2))==2 and take()==1
        assert snapshot.mode==mode
        prepare();assert d.hr_core_spectrum_mode(e)==mode
        code,result=calculate();assert code==1 and finish(result,code)==1
        assert report()[1].mode==mode
        checks.append(f'mode_{mode}_preserved_after_event_{event}_old_result_isolated')

for mode in (1,0):
    reset(mode);assert push(sample())==2 and take()==1;prepare()
    code,result=calculate();assert finish(result,code)==1
    payload=b'SLEEP P=0 A=0 BED=0 BV=0 BR=0'
    assert d.sh_line(t,payload,len(payload),4,1000)==0
    assert stat().mode==mode and report()[0]==0
    assert push(sample(n=2,ms=10200,frame=2))==0
    assert push(sample(n=1,ms=16000,frame=3,gen=9))==2 and take()==1
    prepare();code,result=calculate();assert code==1 and finish(result,code)==1
    assert snapshot.mode==mode and d.hr_core_spectrum_mode(e)==mode
    checks.append(f'mode_{mode}_preserved_after_bed_exit_and_new_source_generation')

for mode in (1,0):
    reset(mode);assert push(sample())==2 and take()==1;prepare()
    code,result=calculate();assert code==1
    assert d.sh_set_mode(t,1-mode)==1
    assert finish(result,code)==0 and report()[0]==0
    assert stat().mode==1-mode
    assert push(sample(n=2,ms=16000,frame=2))==2 and take()==1
    prepare();code,result=calculate();assert code==1 and finish(result,code)==1
    assert snapshot.mode==1-mode and d.hr_core_spectrum_mode(e)==1-mode
    checks.append(f'switch_from_mode_{mode}_rejects_inflight_old_result_and_rebuilds_core')

reset(0)
reset()
assert stat().mode==1
checks.append('power_cycle_default_is_rotate16_after_explicit_direct_session')

reference_path=ROOT.parent/'w803_live_heart_c2/native-output/rotation.dll'
ref=C.CDLL(str(reference_path))
ref.hr_core_size.restype=ref.hr_workspace_size.restype=C.c_size_t
ref.hr_core_init.argtypes=[C.c_void_p]
ref.hr_core_attach.argtypes=[C.c_void_p,C.c_void_p]
ref.hr_core_push.argtypes=[C.c_void_p,C.c_uint32,C.c_uint32,C.c_uint16,C.c_uint8,C.c_uint8,C.c_void_p]
ref.hr_core_maybe.argtypes=[C.c_void_p,C.c_uint8,C.c_uint32,C.POINTER(Result)]
ref.hr_core_spectrum_mode.argtypes=[C.c_void_p]
ref.hr_core_spectrum_mode.restype=C.c_uint
replays={}
for label,values in api['all_rows'].items():
    reset()
    core=C.create_string_buffer(ref.hr_core_size())
    workspace=C.create_string_buffer(ref.hr_workspace_size())
    ref.hr_core_init(core);ref.hr_core_attach(core,workspace)
    assert ref.hr_core_spectrum_mode(core)==1
    results=[];accepted=0
    for row in values:
        assert ref.hr_core_push(core,row.source_ms,row.frame,row.bin,row.flags,row.sample_rate,row.iq)==0
        expected=Result();ready=ref.hr_core_maybe(core,row.breath,row.source_ms,C.byref(expected))
        local=200000+row.source_ms-values[0].source_ms
        assert push(row,local) in (1,2)
        have=take()
        assert bool(ready)==bool(have),(label,row.sequence)
        if not have:continue
        assert snapshot.mode==1
        prepare();code,result=calculate();assert code==1 and finish(result,code,local,local)==1
        assert bytes(result)==bytes(expected),(label,row.sequence,bytes(result).hex(),bytes(expected).hex())
        results.append(bytes(result))
        accepted+=int(result.fresh!=0)
    status=stat(local)
    assert status.mode==1 and status.lost==0 and status.skipped==0 and status.math_resets==1
    replays[label]=dict(samples=len(values),estimates=len(results),new_accepts=accepted,
        result_bytes=56,bit_differences_from_frozen_rotate16=0,
        result_sha256=hashlib.sha256(b''.join(results)).hexdigest())
    checks.append(label+'_release_default_matches_frozen_c2_rotation_56_bytes')

report_data=dict(firmware='W803_PROTOCOL_P1_20261120',passed=len(checks),checks=checks,
    default='ROTATE16',reanchor=16,precision='double',replays=replays,
    frozen_rotation_dll=dict(path=str(reference_path),sha256=hashlib.sha256(reference_path.read_bytes()).hexdigest()),
    direct_fallback_suite='snapshot-verification.json',receive_lifecycle_suite='native-verification.json',
    scope='PC native build and recorded-input regression. No board timing or accuracy validation.',
    serial_opened=False,flashed=False)
(ROOT/'release-verification.json').write_text(json.dumps(report_data,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps(report_data,ensure_ascii=False,indent=2))
