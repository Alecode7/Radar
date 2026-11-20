"""Lifecycle and timing-model tests for isolated sampling and confirmation."""
from pathlib import Path
import ctypes as C,json,sys
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT.parent/'w803_iq_ingest/tools'))
from native_api import IQ,Native
from inspect_capture import extract
from core_pipeline import Result

class Ticket(C.Structure):_fields_=[('sample',IQ),('epoch',C.c_uint32),('received_ms',C.c_uint32)]
class Snapshot(C.Structure):_fields_=[('ticket',Ticket),('count',C.c_uint32),('mode',C.c_uint32),('skipped_before',C.c_uint32),('samples',IQ*416)]
class Prepare(C.Structure):_fields_=[(n,C.c_uint32) for n in ('pushed','retired','lost','reset')]
class Report(C.Structure):
    _fields_=[('raw',Result)]+[(n,C.c_uint32) for n in 'epoch generation frame n source_ms received_ms completed_ms cost_ms lag_ms age_ms on_time current sequence mode skipped'.split()]
class Status(C.Structure):
    _fields_=[(n,C.c_uint32) for n in 'epoch pending requests taken skipped received pushed retired lost math_resets attempts completed canceled budget over_cycle last_cost last_lag produced rx_state source_ms source_n source_gen mode snapshot_failures pending_canceled'.split()]

d=C.CDLL(str(ROOT/'native-output/rxstate.dll'))
for name in ('transport','engine','snapshot','report','status'):getattr(d,'sh_'+name+'_size').restype=C.c_size_t
assert d.sh_snapshot_size()==C.sizeof(Snapshot)
assert d.sh_report_size()==C.sizeof(Report) and d.sh_status_size()==C.sizeof(Status)
t=C.create_string_buffer(d.sh_transport_size());e=C.create_string_buffer(d.sh_engine_size());snapshot=Snapshot()
Callback=C.CFUNCTYPE(C.c_int,C.c_void_p,C.c_uint)
d.sh_init.argtypes=[C.c_void_p];d.sh_engine_init.argtypes=[C.c_void_p]
d.sh_event.argtypes=[C.c_void_p,C.c_uint];d.sh_set_mode.argtypes=[C.c_void_p,C.c_uint]
d.sh_enqueue.argtypes=[C.c_void_p,C.POINTER(IQ),C.c_uint32]
d.sh_line.argtypes=[C.c_void_p,C.c_void_p,C.c_size_t,C.c_uint,C.c_uint32]
d.sh_take.argtypes=[C.c_void_p,C.POINTER(Snapshot)]
d.sh_prepare.argtypes=[C.c_void_p,C.POINTER(Snapshot),C.POINTER(Prepare)]
d.sh_account_prepare.argtypes=[C.c_void_p,C.POINTER(Prepare)]
d.sh_compute.argtypes=[C.c_void_p,C.POINTER(Snapshot),C.POINTER(Result),Callback,C.c_void_p]
d.sh_finish.argtypes=[C.c_void_p,C.POINTER(Snapshot),C.POINTER(Result),C.c_int,C.c_uint32,C.c_uint32,C.c_uint]
d.sh_report.argtypes=[C.c_void_p,C.c_uint32,C.POINTER(Report)]
d.sh_status.argtypes=[C.c_void_p,C.c_uint32,C.POINTER(Status)]
checks=[];replays={};simulations={}
@Callback
def run(_context,_phase):return 0
def reset():
    # This inherited suite deliberately exercises the explicit DIRECT fallback.
    d.sh_init(t);d.sh_engine_init(e);d.sh_set_mode(t,0)
def stat(now=1000):
    s=Status();d.sh_status(t,now,C.byref(s));return s
def report(now=1000):
    r=Report();return d.sh_report(t,now,C.byref(r)),r
def push(row,now=1000):return d.sh_enqueue(t,C.byref(row),now)
def take():return d.sh_take(t,C.byref(snapshot))
def prepare():
    info=Prepare();assert d.sh_prepare(e,C.byref(snapshot),C.byref(info))==0
    d.sh_account_prepare(t,C.byref(info));return info
def calculate(callback=run):
    result=Result();code=d.sh_compute(e,C.byref(snapshot),C.byref(result),callback,None);return code,result
def finish(result,code,start=1000,end=1000,cancel=0):
    return d.sh_finish(t,C.byref(snapshot),C.byref(result),code,start,end,cancel)
def sample(n=1,ms=10000,frame=1,gen=8):return IQ(frame=frame,source_ms=ms,generation=gen,sequence=n,bin=13,flags=3,breath=22,sample_rate=5)
def rows(path):
    raw,container=extract(path);values,bad=Native().decode_stream(raw);assert not bad and not container['issues'];return values

dataset=[('current1781',ROOT.parent/'w803_gui_project/logs/COM9_2026_11_18.17.13.02.848.txt',ROOT.parent/'w803_live_heart_c1/board-tests/20261118_171302/C1本轮电脑对照.json'),
         ('earlier1072',ROOT.parent/'w803_gui_project/logs/COM9_2026_11_18.15.29.41.175.txt',ROOT.parent/'w803_gui_project/analysis/20261118_152941/offline-heart-replay.json'),
         ('historical1812',ROOT.parent/'w803_iq_ingest/fixtures/radar_v26_capture.txt',ROOT.parent/'w803_iq_ingest/reference/core_reference_results.json')]
all_rows={}
for label,path,reference in dataset:
    reset();values=all_rows[label]=rows(path);expected=json.loads(reference.read_text(encoding='utf-8'))
    expected=expected['uninterrupted']['results'] if label=='current1781' else expected['results'] if label=='earlier1072' else expected['replay_rows']
    generated=[]
    for row in values:
        local=200000+row.source_ms-values[0].source_ms
        assert push(row,local) in (1,2)
        if not take():continue
        prepare();code,result=calculate();assert code==1
        assert finish(result,code,local,local)==1
        generated.append(dict(ms=snapshot.ticket.sample.source_ms,result_bytes=bytes(result).hex()))
    assert len(generated)==len(expected)
    assert all(a['ms']==b['ms'] and a['result_bytes']==b['result_bytes'] for a,b in zip(generated,expected)),label
    s=stat(local);assert s.mode==0 and s.pending==0 and s.skipped==0 and s.lost==0
    assert s.math_resets==1,(label,s.math_resets)
    replays[label]=dict(samples=len(values),estimates=len(generated),full_result_bytes=56,bit_differences=0,math_resets=s.math_resets)
    checks.append(label+'_nominal_direct_matches_original')

reset();push(sample());assert take()==1;before=bytes(snapshot)
for n in range(2,80):push(sample(n,10000+(n-1)*200,n))
assert bytes(snapshot)==before;prepare();code,result=calculate();assert code==1
checks.append('snapshot_not_changed_by_concurrent_receiver_updates')

reset()
for n in range(1,501):push(sample(n,n*200,n))
s=stat();assert s.epoch==2 and s.pending==1 and s.received==500 and s.skipped==s.requests-1
assert take()==1 and snapshot.ticket.sample.sequence<=500
checks.append('ring_overwrite_and_request_coalescing_do_not_reset_source_epoch')

reset();push(sample());assert take();prepare()
@Callback
def stop(_context,_phase):d.sh_event(t,3);return 1
code,result=calculate(stop);assert code==-2
assert finish(result,code,cancel=1)==0 and report()[0]==0
checks.append('stop_during_math_prevents_publication')

reset();push(sample());assert take();prepare();code,result=calculate()
d.sh_event(t,1);assert finish(result,code)==0 and report()[0]==0
checks.append('source_restart_before_commit_rejects_old_result')

reset();push(sample());assert take();prepare()
@Callback
def cancel(_context,_phase):return 1
code,result=calculate(cancel);assert code==-2
assert finish(result,code,cancel=2)==0 and stat().budget==1 and report()[0]==0
checks.append('budget_abort_is_explicit_discontinuity')

reset();push(sample());assert take();prepare()
r=Result(bpm=65,accepted_ms=snapshot.ticket.sample.source_ms-10000,fresh=0)
finish(r,1,1000,1200)
assert report(1300)[1].age_ms==10300 and report(1400)[1].age_ms==10400
checks.append('reading_or_republishing_held_value_does_not_renew_accepted_age')
assert report(5000)[1].current==0
checks.append('transport_silence_marks_published_value_noncurrent')

reset();d.sh_event(t,3);assert d.sh_set_mode(t,1)==1 and stat().mode==1 and stat().rx_state==3
assert push(sample())==0 and d.sh_set_mode(t,2)==-1
d.sh_event(t,2);assert push(sample())==2 and take();prepare();code,result=calculate();assert code==1
checks.append('explicit_mode_change_does_not_start_stopped_radar_or_reuse_old_history')

# A/B on exactly the same kernel state; temporary workspace mode is restored.
base=C.CDLL(str(ROOT.parent/'w803_live_heart_c1/native-output/rxstate.dll'))
base.hr_core_size.restype=C.c_size_t
d.hr_core_size.restype=C.c_size_t;d.hr_workspace_size.restype=C.c_size_t
d.hr_core_set_spectrum_mode.argtypes=[C.c_void_p,C.c_uint]
d.hr_core_estimate.argtypes=[C.c_void_p,C.c_uint8,C.c_uint32,C.POINTER(Result),C.c_void_p,C.c_void_p]
reset()
for row in all_rows['current1781'][:330]:
    push(row)
    if take():prepare();code,result=calculate();finish(result,code)
row=all_rows['current1781'][350];newrows=all_rows['current1781'][330:351]
for row in newrows:push(row)
assert take();prepare()
# Engine begins with HrCore, so the prefix is a copyable, private core object.
frozen_core=C.string_at(e,d.hr_core_size());clone=C.create_string_buffer(frozen_core,len(frozen_core))
assert d.hr_core_set_spectrum_mode(clone,0)==0
expected=Result();assert d.hr_core_estimate(clone,snapshot.ticket.sample.breath,snapshot.ticket.sample.source_ms,C.byref(expected),None,None)==1
clone=C.create_string_buffer(frozen_core,len(frozen_core))
assert d.hr_core_set_spectrum_mode(clone,1)==0
temporary=Result();assert d.hr_core_estimate(clone,snapshot.ticket.sample.breath,snapshot.ticket.sample.source_ms,C.byref(temporary),None,None)==1
assert C.string_at(e,d.hr_core_size())==frozen_core
code,after=calculate();assert code==1
assert bytes(after)==bytes(expected)
checks.append('comparison_copy_does_not_mutate_live_confirmation_state')

reset();push(sample());assert take();prepare();r=Result(bpm=65,accepted_ms=10000,fresh=1);finish(r,1)
for n in range(2,801):push(sample(n,10000+(n-1)*100,n))
assert take();info=prepare()
assert info.lost>0 and stat().lost==info.lost and report()[0]==0
checks.append('missing_required_recent_history_explicitly_resets_math_and_invalidates_publication')

reset();push(sample());assert stat().pending==1
d.sh_event(t,3);assert stat().pending_canceled==1 and stat().skipped==0
checks.append('stop_discard_is_separate_from_overload_request_coalescing')

def slow(label,values,cost):
    reset();clock=0;position=0;selected=[];arrivals=[x.source_ms-values[0].source_ms for x in values]
    # The independent original core follows only the explicitly selected updates.
    ref=base;ref.hr_workspace_size.restype=C.c_size_t
    ref.hr_core_init.argtypes=[C.c_void_p];ref.hr_core_attach.argtypes=[C.c_void_p,C.c_void_p]
    ref.hr_core_push.argtypes=[C.c_void_p,C.c_uint32,C.c_uint32,C.c_uint16,C.c_uint8,C.c_uint8,C.c_void_p]
    ref.hr_core_maybe.argtypes=[C.c_void_p,C.c_uint8,C.c_uint32,C.POINTER(Result)]
    core=C.create_string_buffer(ref.hr_core_size());workspace=C.create_string_buffer(ref.hr_workspace_size());ref.hr_core_init(core);ref.hr_core_attach(core,workspace);fed=0
    while position<len(values) or stat(clock).pending:
        if not stat(clock).pending:
            clock=max(clock,arrivals[position]);push(values[position],arrivals[position]);position+=1
        if not take():continue
        info=prepare();code,result=calculate();assert code==1
        endn=snapshot.ticket.sample.sequence
        while fed<len(values) and values[fed].sequence<=endn:
            r=values[fed];assert ref.hr_core_push(core,r.source_ms,r.frame,r.bin,r.flags,5,r.iq)==0;fed+=1
        expected=Result();assert ref.hr_core_maybe(core,snapshot.ticket.sample.breath,snapshot.ticket.sample.source_ms,C.byref(expected))==1
        assert bytes(result)==bytes(expected),(label,endn)
        end=clock+cost
        while position<len(values) and arrivals[position]<=end:push(values[position],arrivals[position]);position+=1
        finish(result,code,clock,end);selected.append(endn);clock=end
    s=stat(clock)
    assert s.skipped>0 and s.pending==0 and s.epoch==2 and s.lost==0 and s.math_resets==1
    simulations[label]=dict(injected_cost_ms=cost,received=s.received,requested=s.requests,computed=s.completed,skipped=s.skipped,
        retired_old_inputs=s.retired,required_history_losses=s.lost,math_resets=s.math_resets,max_pending=1,
        scope='Injected delay model; selected update schedule differs from complete original 5-second schedule')
slow('six_second_worker',all_rows['current1781'],6000)
checks.append('slow_math_keeps_sampling_and_confirmation_state_without_fifo_reset')
slow('ninety_second_worker',all_rows['historical1812'],90000)
checks.append('unprocessed_inputs_older_than_75_seconds_can_retire_without_resetting_confirmation')

reset();push(sample(ms=0xffffff00),0xffffff00);assert take();prepare()
r=Result(bpm=65,accepted_ms=snapshot.ticket.sample.source_ms,fresh=1)
finish(r,1,0xffffff00,100);assert report(200)[1].age_ms==456
checks.append('local_age_wrap_preserves_cross_clock_subtraction_contract')

summary=dict(passed=len(checks),checks=checks,replays=replays,simulations=simulations,
    sizes=dict(transport=d.sh_transport_size(),engine=d.sh_engine_size(),snapshot=C.sizeof(Snapshot),report=C.sizeof(Report),status=C.sizeof(Status)),
    test_mode='explicit_DIRECT_fallback',release_default='ROTATE16',board_execution_verified=False)
(ROOT/'snapshot-verification.json').write_text(json.dumps(summary,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps(summary,ensure_ascii=False,indent=2))
