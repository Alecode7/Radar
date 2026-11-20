from pathlib import Path
import ctypes as C,json,re,struct,sys,unittest
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'Host'))
from rsp_protocol import IDS,Parser,encode,decode,result_decode,control_decode,config_encode,config_decode
R=C.CDLL(str(ROOT/'native-output/runtime.dll'))
R.rt_feed.argtypes=[C.c_void_p,C.c_size_t];R.rt_output.argtypes=[C.c_void_p];R.rt_output.restype=C.c_size_t
R.rt_line.argtypes=[C.c_char_p,C.c_uint];R.rt_source.restype=C.c_char_p
R.rt_heart.argtypes=[C.c_int,C.c_uint]
class RuntimeTests(unittest.TestCase):
    def setUp(self):
        self.assertEqual(R.rt_reset(),0);self.sequence=0;self.token=0x4455112233445566
    def tearDown(self):self.assertEqual(R.rt_lock_errors(),0)
    def output(self):
        out=C.create_string_buffer(16384);n=R.rt_output(out);return Parser().feed(out.raw[:n],0)
    def query(self,id,extra=b'',scoped=True):
        self.sequence+=1;req=encode(1,IDS[id],self.sequence,(struct.pack('<Q',self.token) if scoped else b'')+extra)
        R.rt_feed(req,len(req));R.rt_step()
        frames=[f for f in self.output() if f.kind==2 and f.id==IDS[id] and f.sequence==self.sequence]
        self.assertEqual(len(frames),1)
        code,detail=struct.unpack_from('<HH',frames[0].body)
        return code,frames[0].body[12 if scoped else 4:]
    def source_command(self,action):
        match=re.search(rb'mmwc pctl ([0-9a-f]{16}) (\d)',R.rt_source());self.assertIsNotNone(match)
        self.assertEqual(int(match[2]),action);R.rt_clear_source();return match[1]
    def acknowledge(self,op,action,running,mount=0,height=0,pitch=0):
        R.rt_line(b'PCTL V=1 OP='+op+f' ACTION={action} RET=0 RUN={running} MOUNT={mount} HEIGHT={height} PITCH={pitch} CLOCK_OK=1'.encode(),0);R.rt_step();self.output()
    def ready(self):
        R.rt_line(b'FW_VERIFY_VERSION=UART_RADAR_PROTOCOL_20261120_V29',1);R.rt_step();self.output()
        op=self.source_command(2);self.acknowledge(op,2,0)
        code,identity=self.query('IDENTITY',scoped=False);self.assertEqual(code,0)
        boot=struct.unpack_from('<Q',identity)[0];self.assertEqual(self.query('OPEN',struct.pack('<Q',boot))[0],0)
    def start(self):
        self.ready();code,control=self.query('START');self.assertEqual(code,0);self.assertEqual(control_decode(control)['state'],2)
        op=self.source_command(1);self.acknowledge(op,1,1);self.assertEqual(R.rt_paused(),0)
    def sleep(self,bed=1):R.rt_line(f'SLEEP P=1 BED={bed} A=0 BV=1 BR=20 SS=2'.encode(),4)
    def snapshot(self):
        code,b=self.query('SNAPSHOT');self.assertEqual(code,0);return result_decode(b[16:])
    def test_binary_port_has_only_valid_frames(self):
        self.assertEqual(self.query('CAPS',scoped=False)[0],0);self.assertEqual(R.rt_source(),b'')
    def test_v28_is_rejected_for_control(self):
        R.rt_line(b'FW_VERIFY_VERSION=UART_RADAR_IQ_20261119_V28',1);R.rt_step();self.output()
        code,identity=self.query('IDENTITY',scoped=False);boot=struct.unpack_from('<Q',identity)[0]
        self.query('OPEN',struct.pack('<Q',boot));self.assertEqual(self.query('START')[0],5)
    def test_real_text_ack_start_and_measurement_bridge(self):
        self.start();self.sleep();R.rt_line(b'BRC V=1 SEQ=3 RATE=20 AGE_MS=0',0);R.rt_heart(70,0)
        r=self.snapshot();self.assertEqual((r['heart'],r['breath'],r['bed']),(70,20,2))
        self.assertEqual((r['heart_sequence'],r['breath_sequence']),(1,3))
    def test_old_epoch_completion_cannot_overwrite_current_value(self):
        self.start();self.sleep();R.rt_heart(70,0);self.assertEqual(self.snapshot()['heart'],70)
        R.rt_heart(90,1);self.assertEqual(self.snapshot()['heart'],70)
    def test_actual_bed_exit_invalidates_and_stop_pauses_core(self):
        self.start();self.sleep();R.rt_heart(70,0);self.sleep(bed=0)
        r=self.snapshot();self.assertIsNone(r['heart']);self.assertEqual(r['bed'],1)
        self.assertEqual(self.query('STOP')[0],0);self.assertTrue(R.rt_paused())
        op=self.source_command(0);self.acknowledge(op,0,0)
    def test_partial_uart_write_is_completed_without_text(self):
        R.rt_limit(3);self.assertEqual(self.query('CAPS',scoped=False)[0],0)
    def test_failed_uart_write_and_advanced_callback_clock(self):
        R.rt_limit(0);req=encode(1,IDS['CAPS'],1);R.rt_feed(req,len(req));R.rt_step();self.assertEqual(self.output(),[])
        R.rt_limit(256);self.sequence=1;code,b=self.query('IDENTITY',scoped=False)
        self.assertEqual(code,0);boot,uptime,features,faults=struct.unpack_from('<QQII',b)
        self.assertLess(uptime,10000);self.assertTrue(faults&16);self.assertNotEqual(boot,0)
    def test_installation_preflight_is_sent_with_exact_parameters(self):
        self.ready();code,b=self.query('CONFIG_GET');self.assertEqual(code,0);config=config_decode(b[:64])
        config.update(mount_enabled=1,mount_height=100,mount_pitch=20)
        self.assertEqual(self.query('CONFIG_SET',config_encode(config))[0],0)
        self.assertEqual(self.query('START')[0],0)
        self.assertIn(b'3 1 100 20',R.rt_source());op=self.source_command(3)
        self.acknowledge(op,3,0,1,100,20);op=self.source_command(1)
        self.acknowledge(op,1,1,1,100,20);self.assertEqual(R.rt_paused(),0)
    def test_source_clock_fault_clears_monitoring(self):
        self.start();self.sleep();R.rt_heart(70,0);self.assertEqual(self.snapshot()['heart'],70)
        R.rt_line(b'HRSTREAM V=1 CLOCK_OK=0 TXDROP=0 FORMAT_DROP=0',0)
        self.assertIsNone(self.snapshot()['heart']);control=control_decode(self.query('CONTROL')[1])
        self.assertFalse(control['monitoring']);self.assertTrue(control['faults']&1)
    def test_controller_restart_discovers_already_running_radar(self):
        R.rt_time(5000);R.rt_step();self.output();op=self.source_command(2)
        self.acknowledge(op,2,1)
        code,identity=self.query('IDENTITY',scoped=False);boot=struct.unpack_from('<Q',identity)[0]
        self.query('OPEN',struct.pack('<Q',boot))
        code,b=self.query('CONTROL');self.assertEqual(code,0);self.assertEqual(control_decode(b)['actual_running'],1)
        self.assertEqual(control_decode(b)['monitoring'],0)
        self.assertEqual(self.query('START')[0],0);self.source_command(1)

if __name__=='__main__':
    result=unittest.TextTestRunner(verbosity=2).run(unittest.defaultTestLoader.loadTestsFromTestCase(RuntimeTests))
    (ROOT/'runtime-verification.json').write_text(json.dumps(dict(tests=result.testsRun,passed=result.wasSuccessful(),
        failures=len(result.failures),errors=len(result.errors),scope='Actual production runtime + snapshot intake + protocol + BSP with memory UART/RTOS mocks; lock-order checks, no real timing/ports.'),indent=2),encoding='utf-8')
    raise SystemExit(0 if result.wasSuccessful() else 1)
