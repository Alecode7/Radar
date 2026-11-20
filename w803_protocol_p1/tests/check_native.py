from pathlib import Path
import sys,ctypes as C,json
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT.parent/'w803_iq_ingest/tools'))
from native_api import IQ
from inspect_capture import extract
from native_api import Native
class Status(C.Structure):
 _fields_=[(name,C.c_uint32) for name in 'epoch count accepted duplicates bad frame_gaps n_gaps resets versions bed0 paused state source_ms frame n generation received_ms age_ms have_source bed'.split()]
d=C.CDLL(str(ROOT/'native-output/rxstate.dll'))
d.rxl_size.restype=d.rxl_status_size.restype=C.c_size_t
assert d.rxl_status_size()==C.sizeof(Status)==80
state=C.create_string_buffer(d.rxl_size())
d.rxl_init.argtypes=[C.c_void_p];d.rxl_event.argtypes=[C.c_void_p,C.c_uint]
d.rxl_push.argtypes=[C.c_void_p,C.POINTER(IQ),C.c_uint32]
d.rxl_line.argtypes=[C.c_void_p,C.c_void_p,C.c_size_t,C.c_uint,C.c_uint32]
d.rxl_status.argtypes=[C.c_void_p,C.c_uint32,C.POINTER(Status)]
d.rxl_snapshot.argtypes=[C.c_void_p,C.POINTER(IQ),C.c_size_t,C.POINTER(C.c_uint32)];d.rxl_snapshot.restype=C.c_size_t
checks=[]
def status(now=1000):
 s=Status();d.rxl_status(state,now,C.byref(s));return s
def sample(n=1,ms=10000,frame=1,gen=8):
 return IQ(frame=frame,source_ms=ms,generation=gen,sequence=n,bin=13,flags=3,breath=22,sample_rate=5)
def push(s,local=1000):return d.rxl_push(state,C.byref(s),local)
def line(s,kind):
 b=s.encode('ascii');return d.rxl_line(state,b,len(b),kind,1000)
d.rxl_init(state);assert status().state==0;checks.append('empty_waits')
p=sample();assert push(p)==1 and status().count==1;assert push(p)==0 and status().duplicates==1;checks.append('duplicate_not_added')
assert status(4000).state==4 and status(4000).count==1;checks.append('local_silence_diagnostic_keeps_history')
assert line('SLEEP P=0 A=0 BED=0 BV=0 BR=0',4)==0 and status().count==0;assert push(sample(2,10200,2))==0;checks.append('bed_exit_blocks_old_generation')
assert push(sample(1,10400,3,9))==1 and status().count==1;checks.append('new_generation_can_start')
d.rxl_event(state,3);assert push(sample(2,10600,4,9))==0 and status().state==3;d.rxl_event(state,2);assert push(sample(3,10800,5,9))==1;checks.append('manual_stop_start_isolation')
assert push(sample(5,11200,7,9))==1 and status().n_gaps==1 and status().count==1;checks.append('missing_emitted_sequence_clears_history')
assert push(sample(6,11600,9,9))==1 and status().count==2 and status().frame_gaps>=1;checks.append('sampling_frame_gap_is_not_emitted_sequence_loss')
e=status().epoch;assert push(sample(1,100,1,9))==1 and status().epoch>e and status().count==1;checks.append('source_clock_frame_sequence_rollback_isolated')
d.rxl_init(state);assert push(sample(1,0xffffff00,1),0xffffff00)==1;assert push(sample(2,100,2),100)==1 and status(300).age_ms==200 and status(300).count==2;checks.append('source_and_local_uint32_time_wrap')
assert line('SLEEP BED=0 BED=1',4)==-1 and status().count==0;checks.append('ambiguous_sleep_does_not_keep_valid_history')
d.rxl_init(state)
for i in range(1,501):assert push(sample(i,i*200,i))==1
snapshot=(IQ*416)();epoch=C.c_uint32();assert d.rxl_snapshot(state,snapshot,416,C.byref(epoch))==416
assert snapshot[0].sequence==85 and snapshot[-1].sequence==500
assert d.rxl_snapshot(state,snapshot,415,C.byref(epoch))==0;checks.append('bounded_ordered_snapshot_and_capacity')
real=ROOT.parent/'w803_gui_project/logs/COM9_2026_11_18.15.29.41.175.txt'
d.rxl_init(state);before_stop=None
import re
for text in real.read_text(encoding='utf-8-sig').splitlines():
 if 'W803_LINK: REQUEST START' in text:d.rxl_event(state,2)
 if 'W803_LINK: REQUEST STOP' in text:before_stop=status();d.rxl_event(state,3)
 m=re.search(r'RX SEQ=\d+ KIND=(\d+) LEN=\d+ HASH=[0-9a-f]+ \|(.*)',text)
 if m:
  payload=m[2].encode('ascii');d.rxl_line(state,payload,len(payload),int(m[1]),1000)
assert before_stop and before_stop.accepted==1072 and before_stop.count==416 and before_stop.bad==0 and before_stop.n_gaps==0 and before_stop.frame_gaps==13
assert status().count==0 and status().state==3;checks.append('real_current_capture_1072_with_source_gaps_and_stop')
data,container=extract(real);rows,bad=Native().decode_stream(data);assert len(rows)==1072 and not bad
d.rxl_init(state)
for row in rows:assert push(row)==1
assert d.rxl_snapshot(state,snapshot,416,C.byref(epoch))==416
assert all(bytes(a)==bytes(b) for a,b in zip(snapshot,rows[-416:]));checks.append('snapshot_preserves_all_source_fields_and_iq')
fixture=ROOT.parent/'w803_iq_ingest/fixtures/radar_v26_capture.txt';data,container=extract(fixture);rows,bad=Native().decode_stream(data);assert len(rows)==1812 and not bad
d.rxl_init(state)
for row in rows:push(row)
assert status().accepted==1812 and status().bad==0;checks.append('historical_1812_frame_capture_supported')
d.rxl_init(state);p=sample();assert push(p)==1;p.iq[0]=123
assert push(p)==-1 and status().count==0;checks.append('contradictory_duplicate_cannot_reuse_history')
d.rxl_init(state);assert push(sample(1,10000,0xffffffff))==1;e=status().epoch
assert push(sample(2,10200,0))==1 and status().epoch>e and status().count==1;checks.append('frame_wrap_starts_new_epoch')
d.rxl_event(state,1);assert status().count==0 and status().versions==1;checks.append('version_banner_invalidates_old_snapshot')
d.rxl_init(state);assert push(sample())==1;e=status().epoch
d.rxl_event(state,4);assert status().count==0 and status().epoch>e;checks.append('io_fault_invalidates_history')
report=dict(passed=len(checks),checks=checks,state_bytes=d.rxl_size(),status_bytes=d.rxl_status_size(),heart_compute=False,serial_opened=False,flashed=False)
(ROOT/'native-verification.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8');print(json.dumps(report,ensure_ascii=False),flush=True)
