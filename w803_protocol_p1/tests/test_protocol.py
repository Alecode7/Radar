"""Independent Python/C request, lifecycle, control, config and alert tests."""
from pathlib import Path
import ctypes as C,hashlib,json,struct,sys,unittest
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'Host'))
from rsp_protocol import IDS,encode,decode,crc,config_decode,config_encode,result_decode,control_decode,event_decode,Parser
D=C.CDLL(str(ROOT/'native-output/protocol.dll'))
D.p1_size.restype=C.c_size_t
D.p1_crc.argtypes=[C.c_void_p,C.c_size_t];D.p1_crc.restype=C.c_uint16
D.p1_init.argtypes=[C.c_void_p,C.c_uint64,C.c_uint32,C.c_void_p,C.c_void_p]
D.p1_handle.argtypes=[C.c_void_p,C.c_void_p,C.c_size_t,C.c_void_p,C.c_size_t,C.c_uint32];D.p1_handle.restype=C.c_size_t
D.p1_source.argtypes=[C.c_void_p,C.c_int,C.c_uint32]
D.p1_install_observation.argtypes=[C.c_void_p,C.c_uint,C.c_uint,C.c_uint,C.c_uint32]
D.p1_input_epoch.argtypes=[C.c_void_p,C.c_uint32,C.c_uint32,C.c_uint]
D.p1_tick.argtypes=[C.c_void_p,C.c_uint32]
D.p1_sleep.argtypes=[C.c_void_p]+[C.c_uint]*5+[C.c_uint32]
D.p1_breath.argtypes=[C.c_void_p,C.c_uint32,C.c_uint16,C.c_uint32,C.c_uint32]
D.p1_heart.argtypes=[C.c_void_p,C.c_int,C.c_uint,C.c_uint,C.c_uint,C.c_uint32,C.c_uint32,C.c_uint32]
D.p1_begin_control.argtypes=[C.c_void_p,C.c_uint,C.c_uint32]
D.p1_control_due.argtypes=[C.c_void_p,C.c_uint32,C.POINTER(C.c_uint64),C.POINTER(C.c_uint)]
D.p1_control_ack.argtypes=[C.c_void_p,C.c_uint64,C.c_uint,C.c_int,C.c_uint,C.c_uint32]
D.p1_load_config.argtypes=[C.c_void_p,C.c_void_p,C.c_size_t]
D.p1_push.argtypes=[C.c_void_p,C.c_void_p,C.c_size_t,C.c_uint32];D.p1_push.restype=C.c_size_t
D.p1_parser_init.argtypes=[C.c_void_p]
Callback=C.CFUNCTYPE(None,C.c_void_p,C.c_size_t,C.c_void_p)
Store=C.CFUNCTYPE(C.c_int,C.c_void_p,C.c_size_t,C.c_void_p)
D.p1_parser_feed.argtypes=[C.c_void_p,C.c_void_p,C.c_size_t,C.c_uint32,Callback,C.c_void_p]
D.p1_parser_poll.argtypes=[C.c_void_p,C.c_uint32,Callback,C.c_void_p]
D.ctl_apply.argtypes=[C.c_uint64,C.c_uint,C.POINTER(C.c_uint)]
class Server:
    def __init__(self,boot=0x123456789abcdef0,now=0):
        self.state=C.create_string_buffer(D.p1_size());self.boot=boot;self.now=now;self.seq=0;self.token=0x4b33553311224488
        self.saved=[];self.store_ok=True
        @Store
        def store(p,n,_):
            self.saved.append(C.string_at(p,n));return int(self.store_ok)
        self.store=store;D.p1_init(self.state,boot,now,C.cast(store,C.c_void_p),None)
        D.p1_source(self.state,1,now)
        D.p1_install_observation(self.state,0,0,0,now)
    def handle(self,frame,cap=256):
        out=C.create_string_buffer(256);n=D.p1_handle(self.state,frame,len(frame),out,cap,self.now);return out.raw[:n]
    def request(self,id,extra=b'',scoped=True,sequence=None,raw=False):
        if sequence is None:self.seq+=1;sequence=self.seq
        req=encode(1,IDS.get(id,id),sequence,(struct.pack('<Q',self.token) if scoped else b'')+extra)
        reply=self.handle(req)
        if raw:return req,reply
        f=decode(reply);assert f.sequence==sequence and f.id==IDS.get(id,id) and f.kind==2
        code,detail=struct.unpack_from('<HH',f.body)
        if scoped:assert struct.unpack_from('<Q',f.body,4)[0]==self.token
        return code,f.body[12 if scoped else 4:]
    def open(self,write=True):return self.request('OPEN',struct.pack('<Q',self.boot) if write else b'')
    def config(self):
        code,data=self.request('CONFIG_GET');assert code==0;return config_decode(data[:64])
    def set_config(self,**kwargs):
        cfg=self.config();cfg.update(kwargs);code,data=self.request('CONFIG_SET',config_encode(cfg));return code,data
    def actual_ack(self,action=None,ret=0,running=None):
        op=C.c_uint64();a=C.c_uint();assert D.p1_control_due(self.state,self.now,C.byref(op),C.byref(a))==1
        if action is not None:assert a.value==action
        D.p1_control_ack(self.state,op.value,a.value,ret,a.value==1 if running is None else running,self.now)
        return op.value
    def start(self):
        assert self.request('START')[0]==0;self.actual_ack(1)
    def sleep(self,bed=1,motion=0,bv=1):D.p1_sleep(self.state,1,bed,motion,2,bv,self.now)
    def breath(self,seq,rate,age=0):D.p1_breath(self.state,seq,rate,age,self.now)
    def heart(self,bpm=70,q=3,hcon=0,fresh=1,age=0):D.p1_heart(self.state,bpm,q,hcon,fresh,age,4,self.now)
    def tick(self,now):self.now=now&0xffffffff;D.p1_tick(self.state,self.now)
    def alerts(self):
        code,b=self.request('ALERTS');assert code==0
        return dict(zip(('boot','monitor','enabled','active','unknown','pending','last','ack','overwrites'),struct.unpack('<QIIIIIIII',b)))
    def snapshot(self):
        code,b=self.request('SNAPSHOT');assert code==0 and struct.unpack_from('<Q',b)[0]==self.boot;return result_decode(b[16:])

class ProtocolTests(unittest.TestCase):
    def setUp(self):self.s=Server();self.s.open()
    def test_crc_and_wire_reference(self):
        self.assertEqual(crc(b'123456789'),0x29b1);self.assertEqual(D.p1_crc(b'123456789',9),0x29b1)
        request=encode(1,IDS['CAPS'],1);reply=self.s.handle(request)
        f=decode(reply);self.assertEqual((f.kind,f.id,f.sequence,len(f.body)),(2,IDS['CAPS'],1,68))
        self.assertEqual(struct.unpack_from('<H',f.body,6)[0],2)
    def test_legacy_read_queries_and_open(self):
        s=Server();self.assertEqual(s.request('CAPS',scoped=False)[0],0)
        self.assertEqual(s.request('RESULT',scoped=False)[0],5)
        self.assertEqual(s.open(False)[0],0);self.assertEqual(s.request('HEALTH')[0],0)
        self.assertEqual(s.request('LINK_RESULT')[0],5);self.assertEqual(s.request('START')[0],12)
    def test_identity_and_boot_binding(self):
        s=Server();self.assertEqual(s.request('IDENTITY',scoped=False)[0],0)
        self.assertEqual(s.request('OPEN',struct.pack('<Q',42))[0],12)
        self.assertEqual(s.open()[0],0)
        zero=Server(boot=0);self.assertEqual(zero.open()[0],12)
    def test_old_session_and_sequence_do_not_execute(self):
        s=self.s;req,reply=s.request('CONFIG_GET',raw=True)
        self.assertEqual(s.handle(req),reply)
        s.request('HEALTH');self.assertEqual(struct.unpack_from('<H',decode(s.handle(req)).body)[0],10)
        old=s.token;s.token+=1;self.assertEqual(s.request('START')[0],9);s.token=old
    def test_same_sequence_different_payload_is_conflict(self):
        s=self.s;req,_=s.request('CONFIG_GET',raw=True)
        wrong=encode(1,IDS['START'],s.seq,struct.pack('<Q',s.token))
        self.assertEqual(struct.unpack_from('<H',decode(s.handle(wrong)).body)[0],6)
        self.assertEqual(s.request('CONTROL')[1][20],0)
    def test_all_single_bit_corruption_and_recovery(self):
        valid=encode(1,IDS['IDENTITY'],123)
        parser=C.create_string_buffer(300);observed=[]
        @Callback
        def got(p,n,_):observed.append(C.string_at(p,n))
        for bit in range(len(valid)*8):
            corrupt=bytearray(valid);corrupt[bit//8]^=1<<(bit%8)
            self.assertEqual(self.s.handle(bytes(corrupt)),b'')
            D.p1_parser_init(parser);observed.clear()
            stream=bytes(corrupt)+valid
            D.p1_parser_feed(parser,stream,len(stream),0,got,None)
            D.p1_parser_poll(parser,201,got,None)
            self.assertIn(valid,observed,bit)
    def test_every_split_and_concatenated_frames(self):
        frames=[encode(1,IDS['IDENTITY'],123),encode(1,IDS['CAPS'],124)];stream=b''.join(frames)
        parser=C.create_string_buffer(300);observed=[]
        @Callback
        def got(p,n,_):observed.append(C.string_at(p,n))
        for split in range(len(stream)+1):
            D.p1_parser_init(parser);observed.clear()
            D.p1_parser_feed(parser,stream[:split],split,0,got,None)
            D.p1_parser_feed(parser,stream[split:],len(stream)-split,1,got,None)
            self.assertEqual(observed,frames)
        py=Parser();self.assertEqual([f.sequence for f in py.feed(b'noise'+stream,0)],[123,124])
    def test_incomplete_timeout_uint32_wrap(self):
        p=C.create_string_buffer(300);observed=[]
        @Callback
        def got(data,n,_):observed.append(C.string_at(data,n))
        D.p1_parser_init(p);part=b'RS\x01\x01\x01\x00'
        D.p1_parser_feed(p,part,len(part),0xffffff00,got,None)
        D.p1_parser_poll(p,0x100,got,None)
        good=encode(1,IDS['IDENTITY'],222);D.p1_parser_feed(p,good,len(good),0x101,got,None)
        self.assertEqual(observed,[good])
    def test_response_capacity_and_unknown_command(self):
        req=encode(1,IDS['CAPS'],1);self.assertEqual(self.s.handle(req,255),b'')
        self.assertEqual(self.s.request(0x7777)[0],1)
        self.assertEqual(self.s.request('START',b'X')[0],2)
    def test_start_pending_is_not_successful_measurement(self):
        s=self.s;code,b=s.request('START');self.assertEqual(code,0);self.assertEqual(control_decode(b)['state'],2)
        self.assertEqual(s.snapshot()['bed'],0)
        op=s.actual_ack(1);s.sleep();s.heart();self.assertEqual(s.snapshot()['heart'],70)
        self.assertEqual(s.request('START')[1][20],1)
        self.assertGreater(op,0)
    def test_timeout_and_late_ack(self):
        s=self.s;s.request('START');op=C.c_uint64();action=C.c_uint()
        self.assertEqual(D.p1_control_due(s.state,0,C.byref(op),C.byref(action)),1)
        s.tick(3000);D.p1_control_ack(s.state,op.value,1,0,1,3001)
        b=s.request('CONTROL')[1];self.assertEqual(control_decode(b)['state'],3)
        self.assertEqual(control_decode(b)['monitoring'],0)
    def test_stop_supersedes_start_and_rejects_old_ack(self):
        s=self.s;s.request('START');op=C.c_uint64();action=C.c_uint();D.p1_control_due(s.state,0,C.byref(op),C.byref(action))
        s.request('STOP');D.p1_control_ack(s.state,op.value,1,0,1,0)
        self.assertEqual(control_decode(s.request('CONTROL')[1])['state'],2)
        s.actual_ack(0,running=0);self.assertEqual(control_decode(s.request('CONTROL')[1])['state'],0)
    def test_radar_transaction_replay_and_out_of_order(self):
        D.ctl_reset();actual=C.c_uint();prefix=0xab12345600000000
        self.assertEqual(D.ctl_apply(prefix|1,1,C.byref(actual)),-101)
        self.assertEqual(D.ctl_apply(prefix|1,2,C.byref(actual)),0)
        for _ in range(20):self.assertEqual(D.ctl_apply(prefix|2,1,C.byref(actual)),0)
        self.assertEqual(D.ctl_starts(),1)
        self.assertEqual(D.ctl_apply(prefix|2,0,C.byref(actual)),-102)
        self.assertEqual(D.ctl_apply(prefix|3,0,C.byref(actual)),0)
        self.assertEqual(D.ctl_stops(),1)
        self.assertEqual(D.ctl_apply(prefix|2,1,C.byref(actual)),-103)
    def test_radar_failure_propagates(self):
        D.ctl_reset();actual=C.c_uint();prefix=0xab12345600000000
        D.ctl_apply(prefix|1,2,C.byref(actual));D.ctl_failure(-7)
        self.assertEqual(D.ctl_apply(prefix|2,1,C.byref(actual)),-7);self.assertEqual(actual.value,0)
        s=self.s;s.request('START');s.actual_ack(1,ret=-7,running=0)
        self.assertEqual(control_decode(s.request('CONTROL')[1])['state'],3)
    def test_configuration_atomic_validation_and_revision(self):
        s=self.s;before=s.config();code,_=s.set_config(report_ms=1);self.assertEqual(code,3);self.assertEqual(s.config(),before)
        code,b=s.set_config(report_ms=500);self.assertEqual(code,0);cfg=config_decode(b[:64]);self.assertEqual(cfg['revision'],before['revision']+1)
        self.assertEqual(s.request('CONFIG_SET',config_encode(before))[0],6)
        s.start();self.assertEqual(s.set_config(report_ms=1000)[0],4)
    def test_save_idempotence_failure_and_readback(self):
        s=self.s;s.set_config(report_ms=500);rev=s.config()['revision']
        req,reply=s.request('CONFIG_SAVE',struct.pack('<I',rev),raw=True)
        self.assertEqual(len(s.saved),1)
        for _ in range(5):self.assertEqual(s.handle(req),reply)
        self.assertEqual(len(s.saved),1)
        s.request('CONFIG_SAVE',struct.pack('<I',rev));self.assertEqual(len(s.saved),1)
        s.set_config(report_ms=1000);s.store_ok=False
        self.assertEqual(s.request('CONFIG_SAVE',struct.pack('<I',s.config()['revision']))[0],7)
        recovered=Server();self.assertEqual(D.p1_load_config(recovered.state,s.saved[0],64),1)
        recovered.open();self.assertEqual(recovered.config()['report_ms'],500)
    def test_defaults_ram_only_and_bad_reserved(self):
        s=self.s;s.set_config(report_ms=500);rev=s.config()['revision'];s.request('CONFIG_DEFAULT',struct.pack('<I',rev))
        self.assertEqual(s.config()['report_ms'],2000);self.assertEqual(s.saved,[])
        cfg=bytearray(config_encode(s.config()));cfg[63]=1;self.assertEqual(s.request('CONFIG_SET',bytes(cfg))[0],3)
    def test_heart_hold_does_not_create_measurement_or_alarm_evidence(self):
        s=self.s;s.start();s.sleep();s.heart(35)
        first=s.snapshot();self.assertEqual(first['heart_sequence'],1)
        for now in (3000,6000,9000):s.tick(now);s.sleep();s.heart(35,fresh=0,age=now)
        self.assertEqual(s.snapshot()['heart_sequence'],1)
        self.assertEqual(s.alerts()['active']&4,0)
        s.tick(11000);s.sleep();s.heart(35,fresh=0,age=11000)
        self.assertEqual(s.alerts()['unknown']&4,4)
    def test_threshold_activation_and_recovery_need_new_measurements(self):
        s=self.s;s.set_config(confirm_ms=1000,recover_ms=1000);s.start();s.sleep();s.heart(35);s.tick(0)
        s.tick(1000);s.sleep();s.heart(35);s.tick(1000);self.assertEqual(s.alerts()['active']&4,4)
        s.heart(70);s.tick(1000);s.tick(2000);s.sleep();s.heart(70);s.tick(2000)
        self.assertEqual(s.alerts()['active']&4,0)
        events=s.request('EVENTS',struct.pack('<I',0))[1];count=struct.unpack_from('<I',events,28)[0]
        parsed=[event_decode(events[32+i*32:64+i*32]) for i in range(count)]
        self.assertEqual([x['transition'] for x in parsed if x['rule']==2],[1,2])
    def test_low_quality_and_harmonic_are_not_physiologic_evidence(self):
        s=self.s;s.set_config(confirm_ms=1000);s.start()
        for now in (0,2000,4000):s.tick(now);s.sleep();s.heart(35,q=1,hcon=1);s.tick(now)
        self.assertEqual(s.alerts()['active']&12,0);self.assertEqual(s.alerts()['unknown']&12,12)
        self.assertEqual(s.snapshot()['heart'],35)
    def test_all_eight_rules_trigger(self):
        for index in range(8):
            with self.subTest(rule=index):
                s=Server();s.open();s.set_config(confirm_ms=1000,recover_ms=1000,missing_br_ms=10000,missing_hr_ms=10000,long_bed_ms=60000,away_ms=10000);s.start()
                target=62000 if index==6 else 12000 if index>=4 else 2000
                for now in range(0,target+1,1000):
                    s.tick(now);s.sleep(bed=0 if index==7 and now>=1000 else 1)
                    if index<4:
                        if index in (0,1):s.breath(now+1,5 if index==0 else 40)
                        else:s.heart(30 if index==2 else 150)
                    s.tick(now)
                self.assertTrue(s.alerts()['active']&(1<<index),s.alerts())
    def test_source_loss_unknown_and_stop_ends_latches(self):
        s=self.s;s.set_config(confirm_ms=1000);s.start();s.sleep();s.heart(35);s.tick(0)
        s.tick(1000);s.sleep();s.heart(35);s.tick(1000);self.assertTrue(s.alerts()['active']&4)
        s.tick(8000);self.assertTrue(s.alerts()['unknown']&4);self.assertEqual(s.snapshot()['heart'],None)
        s.request('STOP');self.assertEqual(s.alerts()['active'],0);s.actual_ack(0,running=0)
    def test_events_overwrite_gap_and_ack_bounds(self):
        s=self.s;s.set_config(confirm_ms=1000,recover_ms=1000);s.start()
        for cycle in range(36):
            base=cycle*4000
            for delta,bpm in ((0,35),(1000,35),(2000,70),(3000,70)):
                s.tick(base+delta);s.sleep();s.heart(bpm);s.tick(base+delta)
        code,data=s.request('EVENTS',struct.pack('<I',0));self.assertEqual(code,0)
        boot,monitor,oldest,last,overwrites,gap,count=struct.unpack_from('<QIIIIII',data)
        self.assertEqual(boot,s.boot);self.assertGreater(oldest,1);self.assertTrue(gap);self.assertEqual(count,5)
        self.assertEqual(s.request('EVENT_ACK',struct.pack('<I',last+1))[0],3)
        self.assertEqual(s.request('EVENT_ACK',struct.pack('<I',last))[0],0)
        self.assertGreater(overwrites,0)
    def test_reset_and_stale_have_no_fresh_result(self):
        s=self.s;s.start();s.sleep();s.heart();self.assertEqual(s.snapshot()['heart'],70)
        D.p1_source(s.state,1,100);s.now=100;self.assertEqual(s.snapshot()['heart'],None)
        old=encode(1,IDS['SNAPSHOT'],s.seq+1,struct.pack('<Q',s.token))
        restarted=Server(boot=0xaaaaaaaa55555555)
        self.assertEqual(struct.unpack_from('<H',decode(restarted.handle(old)).body)[0],9)
    def test_counter_age_and_deadline_across_uint32_wrap(self):
        s=Server(now=0xffffff00);s.open();s.start();s.sleep();s.heart(age=0)
        s.tick(0x100);s.sleep();self.assertEqual(s.snapshot()['heart_age_ms'],512)
        self.assertEqual(s.snapshot()['heart_sequence'],1)
    def test_heartbeat_runs_while_stopped_and_does_not_renew_result(self):
        s=self.s;out=C.create_string_buffer(256);s.tick(1000)
        n=D.p1_push(s.state,out,256,s.now);frame=decode(out.raw[:n]);self.assertEqual(frame.id,IDS['HEARTBEAT'])
        self.assertEqual(struct.unpack_from('<Q',frame.body)[0],s.boot);self.assertEqual(s.snapshot()['heart'],None)
    def test_input_epoch_clears_values_but_bed_exit_context_survives(self):
        s=self.s;s.set_config(away_ms=10000);s.start();s.sleep();s.heart()
        D.p1_input_epoch(s.state,5,0,0);self.assertIsNone(s.snapshot()['heart'])
        s.sleep();s.tick(1000);s.sleep(bed=0);D.p1_input_epoch(s.state,6,s.now,1)
        for now in range(2000,12001,1000):s.tick(now);s.sleep(bed=0)
        self.assertTrue(s.alerts()['active']&128)
    def test_old_breath_metadata_and_repeated_reads_do_not_renew(self):
        s=self.s;s.tick(1000);s.start();s.sleep();s.breath(1,20,500)
        self.assertIsNone(s.snapshot()['breath'])
        s.breath(2,20,0);self.assertEqual(s.snapshot()['breath_age_ms'],0)
        s.tick(1500);s.sleep();s.breath(2,20,0)
        self.assertEqual(s.snapshot()['breath_age_ms'],500)
    def test_limits_are_separate_from_alarm_configuration(self):
        code,b=self.s.request('LIMITS');self.assertEqual(code,0)
        self.assertEqual(struct.unpack('<QHHHHHHI',b),(self.s.boot,120,240,480,1200,200,5000,3))
    def test_control_fault_requires_a_new_execution_ack(self):
        s=self.s;s.start();self.assertEqual(D.p1_begin_control(s.state,2,0),0)
        s.actual_ack(2,ret=-7,running=1)
        self.assertEqual(control_decode(s.request('START')[1])['state'],2)
    def test_installation_is_confirmed_before_start(self):
        s=self.s;self.assertEqual(s.set_config(mount_enabled=1,mount_height=100,mount_pitch=20)[0],0)
        code,b=s.request('START');self.assertEqual(code,0);self.assertEqual(control_decode(b)['action'],3)
        D.p1_install_observation(s.state,1,100,20,s.now);first=s.actual_ack(3,running=0)
        self.assertEqual(control_decode(s.request('CONTROL')[1])['monitoring'],0)
        second=s.actual_ack(1);self.assertGreater(second,first)
        self.assertEqual(control_decode(s.request('CONTROL')[1])['monitoring'],1)
    def test_wrong_installation_readback_blocks_rf_start(self):
        s=self.s;s.set_config(mount_enabled=1,mount_height=100,mount_pitch=20);s.request('START')
        D.p1_install_observation(s.state,1,101,20,s.now);s.actual_ack(3,running=0)
        control=control_decode(s.request('CONTROL')[1]);self.assertEqual((control['state'],control['monitoring']),(3,0))
    def test_installation_limits_and_persistence(self):
        s=self.s;self.assertEqual(s.set_config(mount_enabled=1,mount_height=301,mount_pitch=20)[0],3)
        self.assertEqual(s.set_config(mount_enabled=1,mount_height=100,mount_pitch=81)[0],3)
        self.assertEqual(s.set_config(mount_enabled=1,mount_height=100,mount_pitch=20)[0],0)
        s.request('CONFIG_SAVE',struct.pack('<I',s.config()['revision']))
        restored=Server();self.assertEqual(D.p1_load_config(restored.state,s.saved[0],64),1);restored.open()
        self.assertEqual((restored.config()['mount_enabled'],restored.config()['mount_height'],restored.config()['mount_pitch']),(1,100,20))

if __name__=='__main__':
    suite=unittest.defaultTestLoader.loadTestsFromTestCase(ProtocolTests)
    result=unittest.TextTestRunner(verbosity=2).run(suite)
    report=dict(tests=result.testsRun,failures=len(result.failures),errors=len(result.errors),passed=result.wasSuccessful(),
        native_dll_sha256=hashlib.sha256((ROOT/'native-output/protocol.dll').read_bytes()).hexdigest(),
        scope='PC actual C server and radar-control engine against independent Python wire/client reference; no hardware access.',serial_opened=False)
    (ROOT/'protocol-verification.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    raise SystemExit(0 if result.wasSuccessful() else 1)
