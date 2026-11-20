"""Offline decoded-serial -> unchanged shared-double core replay."""
from pathlib import Path
import ctypes as C,json
from native_api import Native,gui_receiver_bytes,ROOT

class Result(C.Structure):
    _fields_=[('bin_share',C.c_double),('duration_s',C.c_double),('cluster_score',C.c_double),('candidate_bpm',C.c_double),
              ('accepted_ms',C.c_uint32),('bpm',C.c_int16),('target_bin',C.c_int16),('target_bins',C.c_int16*2),
              ('harmonic_bpm',C.c_int16),('reason',C.c_uint16),('support',C.c_uint8),('quality',C.c_uint8),
              ('target_count',C.c_uint8),('breath',C.c_uint8),('harmonic_conflict',C.c_uint8),('harmonic_order',C.c_uint8),
              ('fresh',C.c_uint8),('reserved',C.c_uint8)]

def verify_pipeline():
    d=C.CDLL(str(ROOT/'native-output/shared_core.dll'))
    d.hr_core_size.restype=d.hr_workspace_size.restype=d.hr_result_size.restype=C.c_size_t
    assert d.hr_result_size()==C.sizeof(Result)==56
    d.hr_core_init.argtypes=[C.c_void_p];d.hr_core_attach.argtypes=[C.c_void_p,C.c_void_p]
    d.hr_core_push.argtypes=[C.c_void_p,C.c_uint32,C.c_uint32,C.c_uint16,C.c_uint8,C.c_uint8,C.c_void_p]
    d.hr_core_maybe.argtypes=[C.c_void_p,C.c_uint8,C.c_uint32,C.POINTER(Result)]
    core=C.create_string_buffer(d.hr_core_size());workspace=C.create_string_buffer(d.hr_workspace_size())
    d.hr_core_init(core);d.hr_core_attach(core,workspace)
    rows,bad=Native().decode_stream(gui_receiver_bytes(ROOT/'fixtures/radar_v26_capture.txt'),511)
    assert len(rows)==1812 and not bad
    reference=json.loads((ROOT/'reference/core_reference_results.json').read_text(encoding='utf-8'))['replay_rows']
    generated=[];generation=None
    for item in rows:
        if generation!=item.generation:
            d.hr_core_init(core);d.hr_core_attach(core,workspace);generation=item.generation
        assert d.hr_core_push(core,item.source_ms,item.frame,item.bin,item.flags,item.sample_rate,item.iq)==0
        result=Result();status=d.hr_core_maybe(core,item.breath,item.source_ms,C.byref(result))
        if status:
            assert status==1
            generated.append(dict(ms=item.source_ms,result_bytes=bytes(result).hex(),
                                  bpm=result.bpm,reason=result.reason,fresh=result.fresh))
    assert len(generated)==len(reference)==73
    for actual,expected in zip(generated,reference):
        assert actual['ms']==expected['ms']
        assert actual['result_bytes']==expected['result_bytes'],actual['ms']
    return dict(serial_input_frames=len(rows),estimate_count=len(generated),full_result_bytes=56,bit_differences=0,
                source_time_and_iq_matched=True,core_bytes=d.hr_core_size(),workspace_bytes=d.hr_workspace_size(),
                scope='PC offline C serial decoder -> unchanged C heart core; same 5-second source-time cadence; no W803 runtime, queue or human-accuracy proof')
