"""Local protocol panel. UART opens only after the user clicks Connect."""
from pathlib import Path
from datetime import datetime
import json,queue,struct,threading,time,tkinter as tk
from tkinter import ttk,messagebox
import serial
from serial.tools import list_ports
from rsp_protocol import SerialClient,IDS,RULES,config_decode,config_encode,control_decode,event_decode
ROOT=Path(__file__).resolve().parents[1]
LABELS={'report_ms':'结果上报周期 ms','heartbeat_ms':'保活周期 ms','enabled':'8类事件启用位图（0—255）',
 'br_low':'呼吸下限 ×10','br_high':'呼吸上限 ×10','hr_low':'心率下限 ×10','hr_high':'心率上限 ×10',
 'confirm_ms':'异常确认时间 ms','recover_ms':'恢复确认时间 ms','missing_br_ms':'无可靠呼吸持续时间 ms',
 'missing_hr_ms':'无可靠心率持续时间 ms','long_bed_ms':'连续在床上限 ms','away_ms':'离床未返时间 ms',
 'min_hr_quality':'异常判断最低心率质量（1—3）','reject_harmonic':'排除谐波冲突（1是/0否）',
 'mount_enabled':'安装补偿（1开/0关）','mount_height':'安装高度 cm（0—300）','mount_pitch':'俯角 °（0—80）'}
class Panel:
    def __init__(self,root):
        self.root=root;root.title('睡眠雷达协议联调 P1');root.geometry('1060x790')
        self.jobs=queue.Queue();self.messages=queue.Queue();self.quit=threading.Event();self.worker=None
        self.config=None;self.last_event=0;self.snapshot=None;self.snapshot_at=0
        self.status=tk.StringVar(value='未连接。协议接六针TX/RX；02口用于烧录和调试。')
        self.value=tk.StringVar(value='心率：—     呼吸：—     在床：未知')
        self.detail=tk.StringVar(value='');self.control=tk.StringVar(value='设备状态：未知');self.alarms=tk.StringVar(value='异常状态：未知')
        self.install=tk.StringVar(value='实际安装设置尚未读回。')
        top=ttk.Frame(root,padding=10);top.pack(fill='x')
        ttk.Label(top,text='协议串口').pack(side='left');self.port=ttk.Combobox(top,width=15,state='readonly');self.port.pack(side='left',padx=8)
        ttk.Button(top,text='刷新',command=self.refresh).pack(side='left')
        ttk.Button(top,text='连接',command=self.connect).pack(side='left',padx=8)
        ttk.Button(top,text='断开',command=self.disconnect).pack(side='left')
        ttk.Label(top,text='115200 · 8N1 · 独立二进制协议').pack(side='right')
        ttk.Label(root,textvariable=self.status,wraplength=1020,padding=(12,5)).pack(fill='x')
        actions=ttk.Frame(root,padding=10);actions.pack(fill='x')
        for title,job in [('开始监护','START'),('停止监护','STOP'),('读取状态','CONTROL'),('读取配置','CONFIG_GET'),('实际安装','INSTALL'),('读取统计','STATS'),('确认已读事件','ACK')]:
            ttk.Button(actions,text=title,command=lambda j=job:self.enqueue(j)).pack(side='left',padx=4)
        tabs=ttk.Notebook(root);tabs.pack(fill='both',expand=True,padx=10,pady=6)
        result=ttk.Frame(tabs,padding=14);tabs.add(result,text='测量与事件')
        ttk.Label(result,textvariable=self.value,font=('Microsoft YaHei UI',19)).pack(anchor='w',pady=8)
        ttk.Label(result,textvariable=self.detail,wraplength=980).pack(anchor='w',pady=6)
        ttk.Label(result,textvariable=self.control,wraplength=980).pack(anchor='w',pady=6)
        ttk.Label(result,textvariable=self.alarms,wraplength=980).pack(anchor='w',pady=6)
        ttk.Label(result,text='保持值继续显示原年龄；有数值不代表高质量。无可靠估计事件不等于呼吸暂停。').pack(anchor='w',pady=8)
        self.event_text=tk.Text(result,height=18,wrap='word');self.event_text.pack(fill='both',expand=True)
        config=ttk.Frame(tabs,padding=12);tabs.add(config,text='参数')
        self.entries={}
        for i,(key,label) in enumerate(LABELS.items()):
            col=(i//9)*2;row=i%9
            ttk.Label(config,text=label).grid(row=row,column=col,sticky='w',padx=8,pady=8)
            var=tk.StringVar();ttk.Entry(config,textvariable=var,width=16).grid(row=row,column=col+1,padx=12,pady=8);self.entries[key]=var
        options=ttk.Frame(config);options.grid(row=10,column=0,columnspan=4,sticky='w',pady=15)
        ttk.Button(options,text='应用到RAM',command=self.apply).pack(side='left',padx=4)
        ttk.Button(options,text='保存当前RAM配置至Flash',command=self.save).pack(side='left',padx=4)
        ttk.Button(options,text='恢复默认（仅RAM）',command=lambda:self.enqueue('CONFIG_DEFAULT')).pack(side='left',padx=4)
        ttk.Label(config,text='修改和保存要求已停止监护。安装参数在下次开始前由雷达应用并读回；未确认不会启动。\n当前呼吸算法范围12—24次/分，心率频谱范围48—120次/分；报警阈值不会扩大量程。\n位图bit0—7依次为：呼吸低/高、心率低/高、无呼吸估计、无心率估计、在床过久、离床未返。',wraplength=960).grid(row=11,column=0,columnspan=4,sticky='w',pady=10)
        ttk.Label(config,textvariable=self.install,wraplength=960).grid(row=12,column=0,columnspan=4,sticky='w',pady=6)
        log=ttk.Frame(tabs,padding=8);tabs.add(log,text='通信记录')
        self.log=tk.Text(log,wrap='word');self.log.pack(fill='both',expand=True)
        ttk.Label(log,text='每次连接自动保存原始TX/RX帧和解析结果到工程logs目录。').pack(anchor='w')
        self.refresh();root.protocol('WM_DELETE_WINDOW',self.close);root.after(100,self.update)
    def refresh(self):self.port['values']=[p.device for p in list_ports.comports()]
    def enqueue(self,job,data=None):
        if not self.worker or not self.worker.is_alive():messagebox.showinfo('未连接','请先选择协议串口并连接。');return
        if job=='STOP':self.snapshot=None
        self.jobs.put((job,data))
    def connect(self):
        if self.worker and self.worker.is_alive():return
        port=self.port.get()
        if not port:messagebox.showinfo('选择串口','请选择接在模块六针TX/RX上的串口。');return
        self.quit.clear();self.last_event=0
        while not self.jobs.empty():self.jobs.get_nowait()
        self.worker=threading.Thread(target=self.run,args=(port,),daemon=True);self.worker.start()
    def disconnect(self):self.quit.set()
    def close(self):self.quit.set();self.root.destroy()
    def edited(self):
        if self.config is None:raise ValueError('请先读取配置')
        values=dict(self.config)
        for key,var in self.entries.items():values[key]=int(var.get())
        return values
    def apply(self):
        try:data=config_encode(self.edited())
        except (ValueError,struct.error) as e:messagebox.showerror('参数错误',str(e));return
        self.enqueue('CONFIG_SET',data)
    def save(self):
        try:
            if self.edited()!=self.config:raise ValueError('请先应用到RAM，再保存已生效参数。')
        except ValueError as e:messagebox.showinfo('保存参数',str(e));return
        self.enqueue('CONFIG_SAVE')
    def run(self,port):
        connection=None;record=None
        try:
            connection=serial.Serial(port,115200,timeout=.05,write_timeout=.5)
            (ROOT/'logs').mkdir(exist_ok=True)
            path=ROOT/'logs'/('P1_'+port+'_'+datetime.now().strftime('%Y%m%d_%H%M%S_%f')+'.jsonl')
            record=path.open('w',encoding='utf-8')
            def write_log(direction,**values):
                record.write(json.dumps(dict(time=datetime.now().isoformat(timespec='milliseconds'),direction=direction,**values),ensure_ascii=False)+'\n');record.flush()
            class Port:
                def write(_,data):write_log('TX',hex=data.hex());return connection.write(data)
                def read(_,n):return connection.read(n)
            def frame(f):
                write_log('RX',kind=f.kind,id=f.id,sequence=f.sequence,body=f.body.hex())
            client=SerialClient(Port(),frame);boot=client.open_link()
            self.messages.put(('status',f'已建立会话，启动身份 {boot:016x}；日志 {path}'))
            self.jobs.put(('CONFIG_GET',None));last_poll=0;after=0
            while not self.quit.is_set():
                try:job,data=self.jobs.get_nowait()
                except queue.Empty:job=None;data=None
                if job:
                    extra=data or b''
                    if job in ('CONFIG_SAVE','CONFIG_DEFAULT'):
                        if self.config is None:self.messages.put(('log','请先读取配置。'));continue
                        extra=struct.pack('<I',self.config['revision'])
                    if job=='ACK':id=IDS['EVENT_ACK'];extra=struct.pack('<I',after)
                    else:id=IDS[job]
                    try:
                        response=client.exchange(id,extra)
                        write_log('RESULT',job=job,hex=response.hex());self.messages.put(('log',job+' OK '+response.hex()))
                        if job.startswith('CONFIG_'):self.messages.put(('config',config_decode(response[:64])))
                        if job in ('START','STOP','CONTROL'):self.messages.put(('control',control_decode(response)))
                        if job=='INSTALL':self.messages.put(('install',response))
                        if job=='STATS':
                            boot,monitor,start,stop,duration,bed,active,br_count,hr_count,br_mean,hr_mean,events,monitoring=struct.unpack('<QIIIIIIIIHHIB7x',response)
                            self.messages.put(('event',f'监护{monitor}统计：时长{duration/1000:.1f}秒，在床{bed/1000:.1f}秒；合格呼吸快照{br_count}，合格心率新值{hr_count}；事件转换{events}。'))
                    except Exception as e:
                        write_log('ERROR',job=job,error=str(e));self.messages.put(('log',job+'：'+str(e)))
                if time.monotonic()-last_poll>=2:
                    snapshot=client.snapshot();self.messages.put(('snapshot',snapshot))
                    control=control_decode(client.exchange(IDS['CONTROL']));self.messages.put(('control',control))
                    alerts=client.exchange(IDS['ALERTS']);_,_,enabled,active,unknown,pending,_,_,lost=struct.unpack('<QIIIIIIII',alerts)
                    self.messages.put(('alerts',(enabled,active,unknown,pending,lost)))
                    events=client.exchange(IDS['EVENTS'],struct.pack('<I',after))
                    boot_id,monitor,oldest,latest,overwrites,gap,count=struct.unpack_from('<QIIIIII',events)
                    if boot_id!=client.boot:raise RuntimeError('事件启动身份变化')
                    if gap:self.messages.put(('event',f'事件历史存在缺口；最早{oldest}，累计覆盖{overwrites}条。'))
                    for i in range(count):
                        event=event_decode(events[32+i*32:64+i*32]);after=event['sequence']
                        write_log('EVENT',**event)
                        transition={1:'触发',2:'恢复',3:'数据未知，保留既有状态',4:'结束（停止/条件失效）',5:'恢复观察'}.get(event['transition'],'未知')
                        self.messages.put(('event',f"#{after} {RULES[event['rule']]}：{transition}，原因={event['reason']}，值={event['value']}"))
                    last_poll=time.monotonic()
                time.sleep(.02)
        except Exception as e:self.messages.put(('status','连接结束：'+str(e)))
        finally:
            if connection:connection.close()
            if record:record.close()
            self.messages.put(('clear',None))
    def update(self):
        try:
            while True:
                kind,value=self.messages.get_nowait()
                if kind=='status':self.status.set(value)
                elif kind=='log':self.log.insert('end',value+'\n');self.log.see('end')
                elif kind=='event':self.event_text.insert('end',value+'\n');self.event_text.see('end')
                elif kind=='config':
                    self.config=value
                    for key,var in self.entries.items():var.set(str(value[key]))
                elif kind=='snapshot':self.snapshot=value;self.snapshot_at=time.monotonic()
                elif kind=='control':
                    state={0:'已停止',1:'运行中',2:'等待雷达执行回执',3:'执行失败/状态未确认'}.get(value['state'],'未知')
                    if value['state']==2 and value['action']==3:state='正在应用并读回安装设置'
                    self.control.set(f"设备状态：{state}；监护会话 {value['monitor_id']}；监护启用 {value['monitoring']}；故障位 0x{value['faults']:x}")
                elif kind=='alerts':
                    enabled,active,unknown,pending,lost=value
                    summary=[]
                    for i,name in enumerate(RULES):summary.append(name+'：'+('停用' if not enabled&(1<<i) else '异常保持/数据未知' if unknown&active&(1<<i) else '未知' if unknown&(1<<i) else '异常' if active&(1<<i) else '确认中' if pending&(1<<i) else '正常'))
                    self.alarms.set('；'.join(summary)+f'；历史覆盖 {lost}')
                elif kind=='install':
                    boot,known,enabled,height,pitch,flags,age,revision=struct.unpack('<QBBHHHII',value)
                    self.install.set('实际安装尚无确认。' if not known else f'雷达读回：补偿{enabled}，高度{height}cm，俯角{pitch}°；读回年龄{age/1000:.1f}秒；目标配置版本{revision}；'+('与目标不一致，开始前需应用。' if flags&1 else '与当前有效目标匹配。'))
                elif kind=='clear':self.snapshot=None;self.control.set('设备状态：未知（通信已断开）');self.alarms.set('异常状态：未知')
        except queue.Empty:pass
        r=self.snapshot
        if r is None or time.monotonic()-self.snapshot_at>=6:
            self.value.set('心率：—     呼吸：—     在床：未知');self.detail.set('暂无有效通信快照；不会用保活延续旧结果。')
        else:
            h='—' if r['heart'] is None else str(r['heart']);b='—' if r['breath'] is None else str(r['breath'])
            self.value.set(f"心率：{h}     呼吸：{b}     在床：{ {0:'未知',1:'否',2:'是'}.get(r['bed'],'未知') }")
            age=r['heart_age_ms'];age_text='未知' if age==0xffffffff else f"{(age+(time.monotonic()-self.snapshot_at)*1000)/1000:.1f}秒"
            state={0:'无值',1:'近期估计',2:'保持旧值'}.get(r['heart_state'],'未知')
            self.detail.set(f"心率状态：{state}；质量 Q={r['heart_quality']}；谐波冲突={bool(r['flags']&1)}；年龄={age_text}；测量序号={r['heart_sequence']}；设备故障位=0x{r['device_faults']:x}")
        self.root.after(100,self.update)
def main():
    root=tk.Tk();Panel(root);root.mainloop()
if __name__=='__main__':main()
