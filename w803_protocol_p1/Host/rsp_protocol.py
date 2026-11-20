"""RSP P1 independent Python wire reference. Importing never opens a port."""
from dataclasses import dataclass
import struct,time,secrets

MAX_FRAME=256
IDS=dict(CAPS=0x0100,RESULT=0x0101,IDENTITY=0x0102,OPEN=0x0110,LINK_RESULT=0x0111,HEALTH=0x0112,
         START=0x0120,STOP=0x0121,CONTROL=0x0122,SNAPSHOT=0x0123,STREAM=0x0124,
         CONFIG_GET=0x0130,CONFIG_SET=0x0131,CONFIG_SAVE=0x0132,CONFIG_DEFAULT=0x0133,LIMITS=0x0134,INSTALL=0x0135,
         ALERTS=0x0140,EVENTS=0x0141,EVENT_ACK=0x0142,STATS=0x0150,
         RESULT_PUSH=0x8101,HEARTBEAT=0x8200,EVENT_PUSH=0x8201)
ERRORS={0:'OK',1:'UNSUPPORTED',2:'BAD_LENGTH',3:'BAD_ARGUMENT',4:'BUSY',5:'NOT_READY',6:'CONFLICT',7:'STORAGE',8:'INTERNAL',9:'SESSION_REQUIRED',10:'OLD_SEQUENCE',12:'BOOT_CHANGED'}
RULES=['呼吸偏低','呼吸偏高','心率偏低','心率偏高','持续无可靠呼吸估计','持续无可靠心率估计','连续在床过久','离床后未返回']
def crc(data):
    c=0xffff
    for b in data:
        c^=b<<8
        for _ in range(8):c=((c<<1)^0x1021)&0xffff if c&0x8000 else (c<<1)&0xffff
    return c
@dataclass(frozen=True)
class Frame:
    kind:int
    id:int
    sequence:int
    body:bytes
def encode(kind,id,sequence,body=b''):
    if not 0<sequence<2**32 or kind not in (1,2,3,4) or len(body)>240:raise ValueError('invalid frame')
    head=b'RS'+struct.pack('<BBBBHIH',1,1,kind,0,id,sequence,len(body))
    return head+body+struct.pack('<H',crc(head[2:]+body))
def decode(data):
    if len(data)<16 or len(data)>256 or data[:2]!=b'RS':raise ValueError('bad frame length/magic')
    major,minor,kind,reserved,id,sequence,n=struct.unpack_from('<BBBBHIH',data,2)
    if (major,minor)!=(1,1) or reserved or kind not in (1,2,3,4) or not sequence or n!=len(data)-16:raise ValueError('bad header')
    if crc(data[2:-2])!=struct.unpack_from('<H',data,len(data)-2)[0]:raise ValueError('bad CRC')
    return Frame(kind,id,sequence,data[14:-2])
class Parser:
    def __init__(self):self.data=bytearray();self.last_ms=0;self.errors=0
    def feed(self,data,now_ms=None):
        now=int(time.monotonic()*1000) if now_ms is None else now_ms
        result=[]
        if self.data and ((now-self.last_ms)&0xffffffff)>200:
            self.errors+=1;self.data.pop(0)
            # Scan any complete valid frame hidden behind the expired prefix.
            result.extend(self._drain(expire=True))
        for b in data:
            if len(self.data)>=256:self.data.pop(0);self.errors+=1
            self.data.append(b);self.last_ms=now;result.extend(self._drain())
        return result
    def _drain(self,expire=False):
        out=[]
        while self.data:
            if self.data[0]!=0x52 or len(self.data)>1 and self.data[1]!=0x53:self.data.pop(0);continue
            if len(self.data)<14:
                if expire:self.data.pop(0);continue
                break
            n=struct.unpack_from('<H',self.data,12)[0]+16
            if self.data[2:4]!=b'\x01\x01' or self.data[5] or n>256 or not struct.unpack_from('<I',self.data,8)[0]:
                self.data.pop(0);self.errors+=1;continue
            if len(self.data)<n:
                if expire:self.data.pop(0);continue
                break
            try:out.append(decode(bytes(self.data[:n])));del self.data[:n]
            except ValueError:self.errors+=1;self.data.pop(0)
        return out
CONFIG_FIELDS={
    'revision':(4,'I'),'report_ms':(8,'H'),'heartbeat_ms':(10,'H'),'enabled':(12,'H'),
    'br_low':(16,'H'),'br_high':(18,'H'),'hr_low':(20,'H'),'hr_high':(22,'H'),
    'confirm_ms':(24,'I'),'recover_ms':(28,'I'),'missing_br_ms':(32,'I'),'missing_hr_ms':(36,'I'),
    'long_bed_ms':(40,'I'),'away_ms':(44,'I'),'min_hr_quality':(48,'B'),'reject_harmonic':(49,'B'),
    'mount_enabled':(50,'B'),'mount_height':(52,'H'),'mount_pitch':(54,'H')}
def config_decode(data):
    if len(data)!=64 or struct.unpack_from('<HH',data)!=(1,64):raise ValueError('bad config')
    return {key:struct.unpack_from('<'+fmt,data,offset)[0] for key,(offset,fmt) in CONFIG_FIELDS.items()}
def config_encode(values):
    out=bytearray(64);struct.pack_into('<HH',out,0,1,64)
    for key,(offset,fmt) in CONFIG_FIELDS.items():struct.pack_into('<'+fmt,out,offset,values[key])
    return bytes(out)
def result_decode(data):
    if len(data)!=56:raise ValueError('bad result')
    uptime,generation,presence,bed,motion,sleep,br,hr,bs,hs,hk,hq,ba,ha,bseq,hseq,ins,outs,flags,faults=struct.unpack('<QIBBBBHHBBBBIIIIIIII',data)
    return dict(uptime_ms=uptime,generation=generation,presence=presence,bed=bed,motion=motion,sleep_state=sleep,
        breath=None if br==65535 else br/10,heart=None if hr==65535 else hr/10,
        breath_state=bs,heart_state=hs,heart_kind=hk,heart_quality=hq,
        breath_age_ms=ba,heart_age_ms=ha,breath_sequence=bseq,heart_sequence=hseq,
        in_bed_s=ins,out_bed_s=outs,flags=flags,faults=faults)
def control_decode(data):
    if len(data)!=32:raise ValueError('bad control')
    boot,monitor,operation,state,actual,monitoring,action,error,faults=struct.unpack('<QIQBBBBII',data)
    return dict(boot_id=boot,monitor_id=monitor,operation=operation,state=state,actual_running=actual,monitoring=monitoring,action=action,error=error,faults=faults)
def event_decode(data):
    if len(data)!=32:raise ValueError('bad event')
    seq,uptime,monitor,rule,transition,reason,value,threshold,measurement,reserved=struct.unpack('<IQIBBHHHII',data)
    return dict(sequence=seq,uptime_ms=uptime,monitor_id=monitor,rule=rule,transition=transition,reason=reason,
        value=value,threshold=threshold,measurement_sequence=measurement,reserved=reserved)
class ProtocolError(RuntimeError):
    def __init__(self,code,detail=0):self.code=code;self.detail=detail;super().__init__(f'{ERRORS.get(code,code)} detail={detail}')
class ConnectionChanged(RuntimeError):pass
class SerialClient:
    """Single worker owns this client and its serial-like object.

    open_link is read-only. start/stop/configuration are explicit requests.
    A timeout never automatically replays a write with a new sequence/session.
    """
    def __init__(self,port,on_frame=None):
        self.port=port;self.parser=Parser();self.sequence=0;self.token=0;self.boot=0;self.on_frame=on_frame
    def exchange(self,id,extra=b'',scoped=True):
        if scoped and not self.token:raise ConnectionChanged('通信会话未建立')
        self.sequence+=1
        if self.sequence>=2**32:raise ConnectionChanged('序号用尽，请重新连接')
        request=encode(1,id,self.sequence,(struct.pack('<Q',self.token) if scoped else b'')+extra)
        start=time.monotonic();last=-1.0;attempts=0
        while time.monotonic()-start<2.4:
            now=time.monotonic()
            if attempts<3 and (attempts==0 or now-last>=.8):
                written=self.port.write(request)
                if written!=len(request):raise ConnectionChanged('串口未完整发送请求')
                last=now;attempts+=1
            for frame in self.parser.feed(self.port.read(512)):
                if self.on_frame:self.on_frame(frame)
                if frame.id==IDS['HEARTBEAT'] and len(frame.body)==36:
                    boot=struct.unpack_from('<Q',frame.body)[0]
                    if self.boot and boot!=self.boot:
                        self.token=0;raise ConnectionChanged('设备已重启，请重新连接')
                if frame.kind!=2 or frame.id!=id or frame.sequence!=self.sequence:continue
                if len(frame.body)<(12 if scoped else 4):continue
                if scoped and struct.unpack_from('<Q',frame.body,4)[0]!=self.token:continue
                code,detail=struct.unpack_from('<HH',frame.body)
                if code:raise ProtocolError(code,detail)
                self.last_response_delay_ms=int((time.monotonic()-start)*1000)
                return frame.body[12 if scoped else 4:]
        raise TimeoutError('请求超时：操作结果可能未知，请读取状态确认；未自动重新执行')
    def open_link(self):
        self.exchange(IDS['CAPS'],scoped=False)
        identity=self.exchange(IDS['IDENTITY'],scoped=False)
        if len(identity)!=28:raise ValueError('设备身份长度错误')
        self.boot=struct.unpack_from('<Q',identity)[0]
        if not self.boot:raise ConnectionChanged('设备启动身份不可用')
        self.token=secrets.randbits(64) or 1
        data=self.exchange(IDS['OPEN'],struct.pack('<Q',self.boot))
        if len(data)!=24 or struct.unpack_from('<Q',data,16)[0]!=self.boot:raise ConnectionChanged('启动身份绑定失败')
        return self.boot
    def snapshot(self):
        data=self.exchange(IDS['SNAPSHOT'])
        if len(data)!=72 or struct.unpack_from('<Q',data)[0]!=self.boot:raise ConnectionChanged('快照启动身份不匹配')
        result=result_decode(data[16:]);result['monitor_id'],result['device_faults']=struct.unpack_from('<II',data,8)
        # Include the whole transaction delay as a conservative age bound,
        # including a possible cached reply following retransmission.
        for key,state in [('heart_age_ms','heart_state'),('breath_age_ms','breath_state')]:
            if result[key]!=0xffffffff:
                result[key]=min(0xfffffffe,result[key]+self.last_response_delay_ms)
                if result[state] and result[key]>10000:result[state]=2
        return result
