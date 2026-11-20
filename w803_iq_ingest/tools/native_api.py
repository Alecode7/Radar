"""Offline native bindings; importing this module never opens hardware."""
from pathlib import Path
import ctypes as C
ROOT=Path(__file__).resolve().parents[1]

class IQ(C.Structure):
    _fields_=[('frame',C.c_uint32),('source_ms',C.c_uint32),('generation',C.c_uint32),('sequence',C.c_uint32),
              ('iq',C.c_int16*6),('bin',C.c_uint16),('flags',C.c_uint8),('breath',C.c_uint8),
              ('source_version',C.c_uint8),('source_enqueued',C.c_uint8),('sample_rate',C.c_uint8),('reserved',C.c_uint8)]

class Native:
    def __init__(self):
        self.d=C.CDLL(str(ROOT/'native-output/radar_iq.dll'))
        self.d.radar_iq_size.restype=C.c_size_t
        assert self.d.radar_iq_size()==C.sizeof(IQ)==36
        self.d.radar_iq_parse.argtypes=[C.c_void_p,C.c_size_t,C.POINTER(IQ)]
        self.d.radar_iq_parse.restype=C.c_int
        self.d.link_stream_size.restype=C.c_size_t
        self.d.link_stream_init.argtypes=[C.c_void_p]
        self.callback_type=C.CFUNCTYPE(None,C.c_void_p,C.c_void_p,C.c_uint32,C.c_uint32)
        self.d.link_stream_feed.argtypes=[C.c_void_p,C.c_void_p,C.c_size_t,self.callback_type,C.c_void_p]
    def parse(self,line):
        if isinstance(line,str):line=line.encode('ascii')
        value=IQ();C.memset(C.byref(value),0xa5,C.sizeof(value))
        status=self.d.radar_iq_parse(line,len(line),C.byref(value))
        return status,value
    def decode_stream(self,data,chunk=511):
        stream=C.create_string_buffer(self.d.link_stream_size());self.d.link_stream_init(stream)
        values=[];malformed=[];failures=[]
        def receive(_context,pointer,size,kind):
            try:
                line=C.string_at(pointer,size);status,value=self.parse(line)
                if status==1:values.append(IQ.from_buffer_copy(value))
                elif status<0:malformed.append(dict(line=line.decode('ascii'),status=status))
            except Exception as exc:failures.append(exc)
        callback=self.callback_type(receive)
        for i in range(0,len(data),chunk):
            block=data[i:i+chunk];self.d.link_stream_feed(stream,block,len(block),callback,None)
        if failures:raise failures[0]
        return values,malformed

def gui_receiver_bytes(path):
    """Recover original radar lines from the frozen GUI's two log categories."""
    import re
    rows=[]
    for line in Path(path).read_text(encoding='utf-8-sig').splitlines():
        if 'CONTROL 控制收← ' in line:rows.append(line.split('CONTROL 控制收← ',1)[1])
        else:
            match=re.match(r'^\[[^\]]+\] HRRAW (HRRAW .*)$',line)
            if match:rows.append(match[1])
    return ('\r\n'.join(rows)+'\r\n').encode('utf-8')

def as_dict(v):
    return dict(frame=v.frame,ms=v.source_ms,generation=v.generation,sequence=v.sequence,bin=v.bin,
                flags=v.flags,breath=v.breath,iq=list(v.iq),source_version=v.source_version,
                source_enqueued=v.source_enqueued,sample_rate=v.sample_rate)
