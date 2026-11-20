"""Exercise actual V28 adapter with a mocked hardware clock and the R1 parser."""
from pathlib import Path
import ctypes as C
import hashlib,json,sys
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT.parent/'w803_iq_ingest/tools'))
from native_api import Native,IQ
from inspect_capture import extract
d=C.CDLL(str(ROOT/'native-output/stream.dll'))
d.test_clock.argtypes=[C.c_uint32,C.c_int]
d.test_status.argtypes=[C.c_bool,C.c_uint8]
d.test_submit.argtypes=[C.c_uint32,C.c_uint16,C.c_bool,C.c_void_p]
d.test_line.restype=C.c_char_p
d.test_count.restype=C.c_uint32
d.radar_iq_parse.argtypes=[C.c_void_p,C.c_size_t,C.POINTER(IQ)]
checks=[];iq=(C.c_int16*6)(-32768,32767,0,-1,1234,-2345)
def parsed():
    line=d.test_line();out=IQ()
    assert d.radar_iq_parse(line,len(line),C.byref(out))==1,line
    return out
def submit(frame=1,bin=13,active=False):d.test_submit(frame,bin,active,iq)
d.test_clock(10000,1);assert d.test_init()==-4
d.test_start();submit();assert d.test_count()==0
checks.append('clock_init_failure_does_not_enable_stream')
d.test_clock(10000,0);assert d.test_init()==0 and d.test_init()==0
submit();assert d.test_count()==0
checks.append('initialization_waits_for_explicit_start')
d.test_start();d.test_status(True,22);submit()
r=parsed();assert (r.source_ms,r.frame,r.bin,r.flags,r.breath,r.source_enqueued,r.source_version)==(10000,1,13,3,22,0,27)
assert list(r.iq)==list(iq);generation=r.generation;sequence=r.sequence
checks.append('real_source_timestamp_iq_extremes_flags_and_br_decode_in_unchanged_r1_parser')
d.test_clock(10200,0);d.test_status(True,0);submit(2,14,True)
r=parsed();assert (r.flags,r.breath,r.sequence)==(7,22,sequence+1)
checks.append('motion_bit_and_positive_breath_hold_match_v27_contract')
d.test_status(False,0);d.test_status(True,0);d.test_clock(10400,0);submit(3)
r=parsed();assert r.generation>generation and r.breath==0
checks.append('bed_exit_changes_generation_and_clears_breath_context')
before=d.test_count();d.test_stop();submit(4);assert d.test_count()==before
d.test_start();d.test_clock(10600,0);submit(4);r2=parsed()
assert r2.generation>r.generation and r2.sequence==r.sequence+1
checks.append('stop_blocks_input_start_isolates_history_without_resetting_lifetime_counter')
before=d.test_count();d.test_clock(10800,1);submit(5);assert d.test_count()==before
d.test_clock(10800,0);d.test_submit(5,13,False,None);assert d.test_count()==before
checks.append('runtime_clock_fault_and_null_iq_do_not_emit_fake_samples')
d.test_clock(0xfffffff0,0);submit(6);assert parsed().source_ms==0xfffffff0
d.test_clock(184,0);submit(7);assert parsed().source_ms==184
checks.append('source_uint32_wrap_is_preserved_without_replacing_it_with_receive_time')
d.test_clock(20000,0);d.test_emit();line=d.test_line()
assert b'HEART_COMPUTE=0' in line and b'OWNER=W803' in line and b'LOCAL_QUEUE=0' in line
before=d.test_count();d.test_clock(20001,0);d.test_emit();assert d.test_count()==before
checks.append('stream_diagnostic_is_rate_limited_and_never_claims_local_heart_result')

datasets=[('current1781',ROOT.parent/'w803_gui_project/logs/COM9_2026_11_18.17.13.02.848.txt'),
          ('earlier1072',ROOT.parent/'w803_gui_project/logs/COM9_2026_11_18.15.29.41.175.txt'),
          ('historical1812',ROOT.parent/'w803_iq_ingest/fixtures/radar_v26_capture.txt')]
replays={}
for label,path in datasets:
    raw,container=extract(path);rows,bad=Native().decode_stream(raw)
    assert not bad and not container['issues']
    last_gen=None;last_n=None
    d.test_start()
    for row in rows:
        if row.generation!=last_gen:d.test_reset();last_gen=row.generation
        d.test_clock(row.source_ms,0);d.test_status(True,row.breath)
        before=d.test_count();d.test_submit(row.frame,row.bin,bool(row.flags&4),row.iq)
        assert d.test_count()==before+1
        emitted=parsed()
        assert (emitted.source_ms,emitted.frame,emitted.bin,emitted.flags,emitted.breath)==(row.source_ms,row.frame,row.bin,row.flags,row.breath)
        assert list(emitted.iq)==list(row.iq) and emitted.source_enqueued==0
        if last_n is not None:assert emitted.sequence==last_n+1
        last_n=emitted.sequence
    replays[label]=dict(source=str(path),sha256=hashlib.sha256(path.read_bytes()).hexdigest(),samples=len(rows),
        r1_parse_failures=0,payload_field_differences=0,local_queue_created=False)
    checks.append(label+'_all_original_payloads_survive_adapter_to_r1_parser')
report=dict(passed=len(checks),checks=checks,replays=replays,total_recorded_samples=sum(r['samples'] for r in replays.values()),
    scope='Actual V28 adapter and unchanged R1 parser, PC-host mocked clock; not hardware timing or ADC equivalence.',
    serial_opened=False,flashed=False)
(ROOT/'stream-verification.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps(report,ensure_ascii=False,indent=2))
