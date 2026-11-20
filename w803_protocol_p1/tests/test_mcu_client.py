from pathlib import Path
import ctypes as C,json,struct,unittest
from test_protocol import Server,D,IDS,decode,encode,control_decode,result_decode
ROOT=Path(__file__).resolve().parents[1]
D.mcu_rsp_size.restype=C.c_size_t
Token=C.CFUNCTYPE(C.c_uint64,C.c_void_p)
D.mcu_rsp_init.argtypes=[C.c_void_p,Token,C.c_void_p]
D.mcu_rsp_poll.argtypes=[C.c_void_p,C.c_uint32,C.c_void_p,C.c_size_t];D.mcu_rsp_poll.restype=C.c_size_t
D.mcu_rsp_feed.argtypes=[C.c_void_p,C.c_void_p,C.c_size_t,C.c_uint32]
D.mcu_rsp_begin.argtypes=[C.c_void_p,C.c_uint16,C.c_void_p,C.c_size_t,C.c_uint32,C.c_void_p,C.c_size_t];D.mcu_rsp_begin.restype=C.c_size_t
D.mcu_rsp_result.argtypes=[C.c_void_p,C.c_uint32,C.c_void_p,C.c_size_t]
D.mcu_rsp_reply.argtypes=[C.c_void_p,C.POINTER(C.c_uint16),C.POINTER(C.c_uint16),C.c_void_p,C.c_size_t]
D.mcu_rsp_disconnect.argtypes=[C.c_void_p,C.c_uint32]
D.client_phase.argtypes=D.client_pending.argtypes=[C.c_void_p]
class Pair:
    def __init__(self,token=0x4455667788991122,now=0):
        self.server=Server(now=now);self.client=C.create_string_buffer(D.mcu_rsp_size());self.now=now;self.next_token=token
        @Token
        def fresh(_):
            result=self.next_token;self.next_token+=1;return result
        self.fresh=fresh;D.mcu_rsp_init(self.client,fresh,None);self.sent=[]
    def poll(self,now=None):
        if now is not None:self.now=now&0xffffffff
        out=C.create_string_buffer(256);n=D.mcu_rsp_poll(self.client,self.now,out,256)
        return out.raw[:n]
    def feed(self,reply,now=None,fragment=False):
        if now is not None:self.now=now&0xffffffff
        if fragment:
            for byte in reply:D.mcu_rsp_feed(self.client,bytes([byte]),1,self.now)
        else:D.mcu_rsp_feed(self.client,reply,len(reply),self.now)
    def roundtrip(self,frame):
        self.sent.append(decode(frame));self.server.now=self.now;reply=self.server.handle(frame);self.feed(reply,fragment=True);return reply
    def open(self):
        for _ in range(3):self.roundtrip(self.poll())
        assert D.client_phase(self.client)==3
    def begin(self,id,extra=b''):
        out=C.create_string_buffer(256);n=D.mcu_rsp_begin(self.client,IDS.get(id,id),extra,len(extra),self.now,out,256);return out.raw[:n]
    def result(self,now=None):
        out=C.create_string_buffer(72);n=D.mcu_rsp_result(self.client,self.now if now is None else now,out,72);return out.raw[:n]

class McuClientTests(unittest.TestCase):
    def test_handshake_and_read_only_automatic_flow(self):
        p=Pair();p.open();p.roundtrip(p.poll())
        self.assertEqual([f.id for f in p.sent],[IDS['IDENTITY'],IDS['CAPS'],IDS['OPEN'],IDS['SNAPSHOT']])
        self.assertEqual(result_decode(p.result()[16:])['heart'],None)
    def test_start_waits_for_actual_ack_and_snapshot(self):
        p=Pair();p.open();reply=p.roundtrip(p.begin('START'))
        self.assertEqual(control_decode(decode(reply).body[12:])['monitoring'],0)
        p.server.actual_ack(1);p.server.sleep();p.server.heart(70)
        p.roundtrip(p.poll());self.assertEqual(result_decode(p.result()[16:])['heart'],70)
    def test_retries_are_identical_and_write_not_reexecuted(self):
        p=Pair();p.open();original=p.begin('START');reply=p.server.handle(original)
        self.assertTrue(reply)
        retry=p.poll(800);self.assertEqual(retry,original);self.assertEqual(p.server.handle(retry),reply)
        p.feed(reply);self.assertEqual(D.client_pending(p.client),0)
        op=p.server.actual_ack(1);self.assertEqual(op&0xffffffff,1)
    def test_late_reply_past_hard_deadline_is_rejected(self):
        p=Pair();p.open();frame=p.begin('START');reply=p.server.handle(frame)
        p.feed(reply,2400);self.assertEqual(D.client_phase(p.client),4);self.assertEqual(p.result(),b'')
    def test_wrong_sequence_token_and_corruption_do_not_complete(self):
        p=Pair();p.open();frame=p.begin('CONTROL');reply=p.server.handle(frame);f=decode(reply)
        wrong=encode(2,f.id,f.sequence+1,f.body);p.feed(wrong);self.assertEqual(D.client_pending(p.client),1)
        body=bytearray(f.body);body[4]^=1;p.feed(encode(2,f.id,f.sequence,body));self.assertEqual(D.client_pending(p.client),1)
        corrupt=bytearray(reply);corrupt[-1]^=1;p.feed(bytes(corrupt));self.assertEqual(D.client_pending(p.client),1)
        p.feed(reply,1);self.assertEqual(D.client_pending(p.client),0)
    def test_age_advances_and_heartbeat_cannot_renew_result(self):
        p=Pair();p.open();p.roundtrip(p.begin('START'));p.server.actual_ack(1);p.server.sleep();p.server.heart(70)
        reply=p.roundtrip(p.poll());self.assertEqual(result_decode(p.result(3000)[16:])['heart_age_ms'],3000)
        heartbeat=encode(4,IDS['HEARTBEAT'],1,struct.pack('<QQQII4B',p.server.boot,6000,0,1,0,1,1,0,0))
        p.feed(heartbeat,6000);self.assertEqual(p.result(6000),b'')
        p.feed(reply,6000);self.assertEqual(p.result(6000),b'')
    def test_reboot_heartbeat_clears_result_and_new_token_used(self):
        p=Pair();p.open();p.roundtrip(p.poll());old_token=p.next_token
        heartbeat=encode(4,IDS['HEARTBEAT'],1,struct.pack('<QQQII4B',0x9999111122223333,0,0,0,0,0,0,0,0))
        p.feed(heartbeat,100);self.assertEqual(D.client_phase(p.client),4);self.assertEqual(p.result(),b'')
        self.assertEqual(p.poll(1099),b'')
        p.roundtrip(p.poll(1100));p.roundtrip(p.poll());p.roundtrip(p.poll())
        self.assertEqual(p.next_token,old_token+1)
    def test_zero_token_blocks(self):
        p=Pair(token=0)
        for _ in range(2):p.roundtrip(p.poll())
        self.assertEqual(p.poll(),b'');self.assertEqual(D.client_phase(p.client),5)
    def test_poll_retry_and_deadline_wrap(self):
        p=Pair(now=0xffffff00);frame=p.poll();retry=p.poll((0xffffff00+800)&0xffffffff)
        self.assertEqual(frame,retry);self.assertEqual(p.poll((0xffffff00+2400)&0xffffffff),b'')
        self.assertEqual(D.client_phase(p.client),4)
    def test_reboot_keepalive_during_handshake_does_not_livelock(self):
        p=Pair();p.open();p.roundtrip(p.poll())
        p.server=Server(boot=0x9999111122223333)
        heartbeat=encode(4,IDS['HEARTBEAT'],1,struct.pack('<QQQII4B',p.server.boot,0,0,0,0,0,0,0,0))
        p.feed(heartbeat,100);first=p.poll(1100)
        p.feed(heartbeat,1100);self.assertEqual(D.client_pending(p.client),1)
        p.roundtrip(first);p.roundtrip(p.poll());p.roundtrip(p.poll());self.assertEqual(D.client_phase(p.client),3)
    def test_stop_immediately_invalidates_local_result(self):
        p=Pair();p.open();p.roundtrip(p.begin('START'));p.server.actual_ack(1);p.server.sleep();p.server.heart(70)
        p.roundtrip(p.poll());self.assertTrue(p.result())
        self.assertTrue(p.begin('STOP'));self.assertEqual(p.result(),b'')
    def test_cached_snapshot_transaction_delay_is_added_to_age(self):
        p=Pair();p.open();p.roundtrip(p.begin('START'));p.server.actual_ack(1);p.server.sleep();p.server.heart(70)
        request=p.poll();reply=p.server.handle(request);self.assertTrue(reply)
        self.assertEqual(p.poll(800),request);p.feed(reply,1000)
        self.assertEqual(result_decode(p.result()[16:])['heart_age_ms'],1000)

if __name__=='__main__':
    result=unittest.TextTestRunner(verbosity=2).run(unittest.defaultTestLoader.loadTestsFromTestCase(McuClientTests))
    (ROOT/'mcu-client-verification.json').write_text(json.dumps(dict(tests=result.testsRun,passed=result.wasSuccessful(),
        failures=len(result.failures),errors=len(result.errors),scope='Actual independent MCU C reference to actual P1 C server, byte-fragmented roundtrips; no UART.'),indent=2),encoding='utf-8')
    raise SystemExit(0 if result.wasSuccessful() else 1)
