from pathlib import Path
import ctypes as C,json,struct,sys
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tests'));sys.path.insert(0,str(ROOT/'Host'))
from test_protocol import Server,D
from rsp_protocol import IDS,decode,encode,config_decode,config_encode
s=Server();vectors=[]
def query(name,extra=b'',scoped=True):
    request,reply=s.request(name,extra,scoped,raw=True);r=decode(reply)
    assert encode(r.kind,r.id,r.sequence,r.body)==reply
    vectors.append(dict(name=name,request=request.hex(),response=reply.hex(),request_bytes=len(request),response_bytes=len(reply),
        result=struct.unpack_from('<H',r.body)[0],response_payload=r.body.hex()))
    return r.body[12 if scoped else 4:]
query('CAPS',scoped=False);query('IDENTITY',scoped=False);query('OPEN',struct.pack('<Q',s.boot))
cfg=config_decode(query('CONFIG_GET')[:64]);cfg.update(report_ms=500,hr_low=650,confirm_ms=1000,recover_ms=1000)
query('CONFIG_SET',config_encode(cfg));current=config_decode(query('CONFIG_GET')[:64])
query('CONFIG_SAVE',struct.pack('<I',current['revision']));query('LIMITS');query('INSTALL');query('STREAM',struct.pack('<I',3))
query('START');s.actual_ack(1);s.sleep();s.breath(1,20);s.heart(60);s.tick(0)
query('CONTROL');query('HEALTH');query('RESULT',scoped=False);query('LINK_RESULT');query('SNAPSHOT')
s.tick(2000);s.sleep();s.breath(2,20);s.heart(60);s.tick(2000)
query('ALERTS');query('EVENTS',struct.pack('<I',0));query('EVENT_ACK',struct.pack('<I',1));query('STATS')
pushes=[];out=C.create_string_buffer(256)
for _ in range(3):
    n=D.p1_push(s.state,out,256,s.now)
    if n:
        frame=out.raw[:n];r=decode(frame);pushes.append(dict(id=r.id,frame=frame.hex(),bytes=n,payload=r.body.hex()))
query('STOP');s.actual_ack(0,running=0);query('CONTROL');query('STATS')
query('CONFIG_DEFAULT',struct.pack('<I',s.config()['revision']))
query(0x0001,scoped=False);query('CONFIG_SET')
good=encode(1,IDS['IDENTITY'],123);bad=bytearray(good);bad[-1]^=1;assert s.handle(bytes(bad))==b''
report=dict(synthetic=True,scope='Actual C service in memory, independent Python frame verification. No hardware; do not copy demo token/boot into production.',
    boot_id=f'{s.boot:016x}',token=f'{s.token:016x}',request_response_vectors=vectors,unsolicited_vectors=pushes,
    invalid_crc=dict(request=bytes(bad).hex(),expected_response=None))
(ROOT/'protocol/golden_vectors.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
print('Golden vectors:',len(vectors),'paired,',len(pushes),'unsolicited, 1 bad CRC')
