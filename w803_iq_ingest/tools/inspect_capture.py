"""Decode an existing file to structured IQ/CSV; never opens a serial port."""
from pathlib import Path
import argparse,csv,hashlib,json,re,sys
from native_api import Native,as_dict,gui_receiver_bytes

def fnv(data):
    h=2166136261
    for value in data:h=((h^value)*16777619)&0xffffffff
    return h

def extract(path):
    path=Path(path);data=path.read_bytes();text=data.decode('utf-8-sig',errors='replace');issues=[];boots=0
    if 'W803_LINK:' in text:
        lines=[]
        for row in text.splitlines():
            if re.search(r'W803_LINK:\s+BOOT\b',row):boots+=1
            match=re.search(r'W803_LINK:\s+RX SEQ=(\d+) KIND=(\d+) LEN=(\d+) HASH=([0-9a-fA-F]+) \|(.*)$',row)
            if match:
                try:payload=match[5].encode('ascii')
                except UnicodeEncodeError:issues.append('Non-ASCII forwarded payload');continue
                if len(payload)!=int(match[3]) or fnv(payload)!=int(match[4],16):
                    issues.append('Forwarding length/hash mismatch');continue
                lines.append(payload)
        return b'\r\n'.join(lines)+b'\r\n',dict(format='W803 L1/Q1 envelopes',boot_count=boots,issues=issues)
    if 'CONTROL 控制收← ' in text:return gui_receiver_bytes(path),dict(format='Frozen GUI log',issues=[])
    return data,dict(format='Original radar text',issues=[])

def inspect(path):
    data,container=extract(path);rows,malformed=Native().decode_stream(data)
    return [as_dict(v) for v in rows],dict(container=container,decoded_samples=len(rows),malformed=malformed,
        state='已解出带雷达采样时间的IQ' if rows else '未解出HRIN采样；不以HRRAW或电脑接收时间替代源时间',
        source=str(Path(path).resolve()),source_sha256=hashlib.sha256(Path(path).read_bytes()).hexdigest(),
        live_hardware_connected=False,heart_accuracy_verified=False)

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('capture',type=Path);p.add_argument('--out',type=Path)
    args=p.parse_args();rows,report=inspect(args.capture)
    out=args.out or args.capture.parent;out.mkdir(parents=True,exist_ok=True);stem=args.capture.stem+'_IQ'
    with (out/(stem+'.csv')).open('w',encoding='utf-8-sig',newline='') as f:
        writer=csv.DictWriter(f,fieldnames=['frame','ms','generation','sequence','bin','flags','breath','I0','Q0','I1','Q1','I2','Q2','source_version','source_enqueued','sample_rate']);writer.writeheader()
        for row in rows:
            item=dict(row);iq=item.pop('iq');item.update(dict(zip(('I0','Q0','I1','Q1','I2','Q2'),iq)));writer.writerow(item)
    (out/(stem+'.json')).write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    print(report['state']);print('样本数：',len(rows));print('输出目录：',out)
    return 0
if __name__=='__main__':sys.exit(main())
