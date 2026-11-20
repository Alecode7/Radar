from pathlib import Path
import ctypes as C,json,unittest
from test_protocol import D,Server,config_encode,config_decode
ROOT=Path(__file__).resolve().parents[1]
D.board_load.argtypes=[C.c_void_p];D.board_save.argtypes=[C.c_void_p]
D.board_open.argtypes=[C.POINTER(C.c_uint64)]
D.board_record.argtypes=[C.c_uint,C.c_void_p];D.board_record.restype=C.c_size_t
D.board_inject.argtypes=[C.c_uint,C.c_void_p,C.c_size_t]
class StorageTests(unittest.TestCase):
    def setUp(self):
        D.board_reset();self.out=C.create_string_buffer(64);self.assertEqual(D.board_load(self.out),0)
        s=Server();s.open();self.cfg=s.config()
    def test_save_load_and_namespace(self):
        self.assertEqual(D.board_save(config_encode(self.cfg)),1);self.assertEqual(D.board_load(self.out),1)
        self.assertEqual(config_decode(self.out.raw),self.cfg);self.assertEqual(D.board_writes(),1)
    def test_all_interrupted_writes_recover_previous_slot(self):
        old=config_encode(self.cfg);newcfg=dict(self.cfg,revision=2,report_ms=500);new=config_encode(newcfg)
        for cut in range(80):
            D.board_reset();D.board_load(self.out);self.assertEqual(D.board_save(old),1)
            D.board_failure(cut);self.assertEqual(D.board_save(new),0)
            self.assertEqual(D.board_load(self.out),1);self.assertEqual(config_decode(self.out.raw),self.cfg)
    def test_every_record_bit_corruption_falls_back(self):
        self.assertEqual(D.board_save(config_encode(self.cfg)),1)
        self.assertEqual(D.board_save(config_encode(dict(self.cfg,revision=2,report_ms=500))),1)
        record=C.create_string_buffer(80);self.assertEqual(D.board_record(0,record),80);new=record.raw
        for bit in range(640):
            damaged=bytearray(new);damaged[bit//8]^=1<<(bit%8);D.board_inject(0,bytes(damaged),80)
            self.assertEqual(D.board_load(self.out),1);self.assertEqual(config_decode(self.out.raw),self.cfg,bit)
        D.board_inject(0,new,80);D.board_load(self.out);self.assertEqual(config_decode(self.out.raw)['revision'],2)
    def test_both_invalid_report_fault_without_erasing(self):
        D.board_inject(0,b'X'*80,80);D.board_inject(1,b'Y'*80,80)
        self.assertEqual(D.board_load(self.out),-2);self.assertEqual(D.board_writes(),0)
    def test_boot_entropy_and_uart_fail_closed(self):
        boot=C.c_uint64()
        self.assertEqual(D.board_open(C.byref(boot)),0);self.assertNotEqual(boot.value,0)
        for mode in (1,2):D.board_mode(mode);self.assertEqual(D.board_open(C.byref(boot)),-2);self.assertEqual(boot.value,0)
        D.board_mode(3);self.assertEqual(D.board_open(C.byref(boot)),-1)
if __name__=='__main__':
    result=unittest.TextTestRunner(verbosity=2).run(unittest.defaultTestLoader.loadTestsFromTestCase(StorageTests))
    (ROOT/'storage-verification.json').write_text(json.dumps(dict(tests=result.testsRun,passed=result.wasSuccessful(),
        interrupted_write_positions=80,corrupted_record_bits=640,scope='Actual W803 BSP journal logic with mocked NVS/RNG/UART; not real Flash power-cut or entropy qualification.'),indent=2),encoding='utf-8')
    raise SystemExit(0 if result.wasSuccessful() else 1)
