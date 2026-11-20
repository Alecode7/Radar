from pathlib import Path
import json,struct,unittest
from test_protocol import Server,IDS,decode,encode,config_decode,config_encode
from rsp_protocol import SerialClient,ConnectionChanged
ROOT=Path(__file__).resolve().parents[1]
class MemoryPort:
    def __init__(self):self.server=Server();self.rx=bytearray();self.writes=[];self.short=False;self.corrupt=False
    def write(self,data):
        self.writes.append(data)
        if self.short:return len(data)-1
        reply=self.server.handle(data)
        if self.corrupt:
            bad=bytearray(reply);bad[-1]^=1;self.rx.extend(bad);self.corrupt=False
        self.rx.extend(reply);return len(data)
    def read(self,n):
        result=bytes(self.rx[:n]);del self.rx[:n];return result
class PythonClientTests(unittest.TestCase):
    def test_gui_client_handshake_control_config_and_snapshot(self):
        port=MemoryPort();client=SerialClient(port);self.assertEqual(client.open_link(),port.server.boot)
        cfg=config_decode(client.exchange(IDS['CONFIG_GET'])[:64]);cfg['report_ms']=500
        updated=client.exchange(IDS['CONFIG_SET'],config_encode(cfg));self.assertEqual(config_decode(updated[:64])['report_ms'],500)
        client.exchange(IDS['START']);port.server.actual_ack(1);port.server.sleep();port.server.heart(70)
        self.assertEqual(client.snapshot()['heart'],70)
    def test_python_parser_recovers_corruption_in_actual_exchange(self):
        port=MemoryPort();client=SerialClient(port);client.open_link();port.corrupt=True
        self.assertEqual(client.snapshot()['heart'],None);self.assertGreater(client.parser.errors,0)
    def test_partial_transmit_does_not_silently_retry_write(self):
        port=MemoryPort();client=SerialClient(port);client.open_link();before=len(port.writes);port.short=True
        with self.assertRaises(ConnectionChanged):client.exchange(IDS['START'])
        self.assertEqual(len(port.writes),before+1)
    def test_reboot_heartbeat_invalidates_session(self):
        port=MemoryPort();client=SerialClient(port);client.open_link()
        port.rx.extend(encode(4,IDS['HEARTBEAT'],1,struct.pack('<QQQII4B',0x9999111122223333,0,0,0,0,0,0,0,0)))
        with self.assertRaises(ConnectionChanged):client.snapshot()
        self.assertEqual(client.token,0)
if __name__=='__main__':
    result=unittest.TextTestRunner(verbosity=2).run(unittest.defaultTestLoader.loadTestsFromTestCase(PythonClientTests))
    (ROOT/'python-client-verification.json').write_text(json.dumps(dict(tests=result.testsRun,passed=result.wasSuccessful(),
        failures=len(result.failures),errors=len(result.errors),scope='Actual GUI Python client and parser to actual C protocol service through an in-memory serial substitute.'),indent=2),encoding='utf-8')
    raise SystemExit(0 if result.wasSuccessful() else 1)
