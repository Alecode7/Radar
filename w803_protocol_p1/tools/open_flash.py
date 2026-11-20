"""Add file selection to the vendor GUI; UART opens only after Start."""
from pathlib import Path
import importlib.util,json,re,sys
from tkinter import filedialog,messagebox,StringVar
from tkinter import ttk
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
from ide_build import sha,source_hashes,image_audit

def checked_image():
    r=json.loads((ROOT/'build-verification.json').read_text(encoding='utf-8'))
    if r['source_hashes']!=source_hashes():raise ValueError('Source changed: save files and click Build first.')
    image=Path(r['image'])
    if image.resolve()!=(ROOT/'build/w803_link.fls').resolve() or sha(image)!=r['sha256']:
        raise ValueError('Image changed: click Build again before flashing.')
    image_audit(image);return image,r

def selected_image(path):
    image=Path(path).resolve()
    if image.suffix.lower()!='.fls':
        raise ValueError('请选择W803完整烧录文件（.fls），不能使用雷达的.img或裸.bin文件。')
    try:image_audit(image)
    except (AssertionError,OSError) as exc:
        raise ValueError('无法读取完整FLS镜像，或文件格式/CRC检查未通过。') from exc
    if image==(ROOT/'build/w803_link.fls').resolve():
        _,report=checked_image()
        description='本工程固件：'+report['firmware_id']
    else:
        description='手动选择：'+image.name
    return image,sha(image),description+' · '+str(image.stat().st_size)+' 字节'

def checked_selection(path,digest):
    image,current,_=selected_image(path)
    if current!=digest:raise ValueError('所选固件在选择后发生变化，请重新选择文件。')
    return image

def make_ui():
    vendor=ROOT.parent/'w803_sdk/wm_iot_sdk/tools/wm/guiflash.py'
    spec=importlib.util.spec_from_file_location('w803_vendor_guiflash',vendor);module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
    ui=module.SerialUI()
    ui.mainwin.title('W803 图形烧录 - 官方SDK工具 / 文件选择入口')
    ui.mainwin.geometry('760x300');ui.mainwin.resizable(width=True,height=False)
    ui.mainwin.minsize(760,300)
    ui.combobox_band.set('115200');ui.combobox_port.set('');ui.check_button_value.set(0)
    ui.label1.configure(text='串口（port）：',font=('Microsoft YaHei UI',11))
    ui.label1.place_configure(x=12,y=118)
    ui.combobox_port.configure(state='readonly');ui.combobox_port.place_configure(x=150,y=115,width=330)
    ui.label2.configure(text='波特率：',font=('Microsoft YaHei UI',11))
    ui.label2.place_configure(x=12,y=163)
    ui.combobox_band.configure(state='readonly');ui.combobox_band.place_configure(x=150,y=160,width=330)
    ui.check_button.configure(text='erase（全片擦除）',font=('Microsoft YaHei UI',11))
    ui.check_button.place_configure(x=12,y=213)
    ui.button_OK.configure(text='start（开始烧录）',font=('Microsoft YaHei UI',12))
    ui.button_OK.place_configure(x=250,y=205)
    ui.progress_dl.pack_configure(fill='x')
    frame=ttk.Frame(ui.mainwin,padding=(12,8));frame.place(x=0,y=0,relwidth=1,height=110)
    frame.columnconfigure(0,weight=1)
    ttk.Label(frame,text='固件文件（W803完整 .fls 镜像）：').grid(row=0,column=0,columnspan=3,sticky='w',pady=(0,5))
    path_var=StringVar(master=ui.mainwin);status_var=StringVar(master=ui.mainwin)
    ttk.Entry(frame,textvariable=path_var,state='readonly').grid(row=1,column=0,sticky='ew',padx=(0,8))
    status=ttk.Label(frame,textvariable=status_var,wraplength=725)
    status.grid(row=2,column=0,columnspan=3,sticky='w',pady=(7,0))
    selection={}
    def select(path):
        image,digest,description=selected_image(path)
        selection.update(path=image,digest=digest)
        path_var.set(str(image));status_var.set(description)
    def use_project(silent=False):
        try:select(ROOT/'build/w803_link.fls')
        except Exception as exc:
            if not silent:messagebox.showerror('需要先编译',str(exc),parent=ui.mainwin)
            if not selection:status_var.set('请先编译本工程，或选择已有的W803完整.fls固件。')
    def browse():
        initial=selection.get('path',ROOT/'build/w803_link.fls')
        path=filedialog.askopenfilename(parent=ui.mainwin,title='选择W803固件',initialdir=str(initial.parent),
                                       filetypes=[('W803完整烧录镜像','*.fls')])
        if path:
            try:select(path)
            except Exception as exc:messagebox.showerror('固件不可用',str(exc),parent=ui.mainwin)
    browse_button=ttk.Button(frame,text='选择固件…',command=browse)
    browse_button.grid(row=1,column=1,padx=(0,8))
    project_button=ttk.Button(frame,text='使用本工程固件',command=use_project)
    project_button.grid(row=1,column=2)
    original=ui.button_OK_click
    def start():
        if not selection:messagebox.showerror('选择固件','请先选择要烧录的W803 .fls文件。',parent=ui.mainwin);return
        try:image=checked_selection(selection['path'],selection['digest'])
        except Exception as exc:messagebox.showerror('固件需要重新确认',str(exc),parent=ui.mainwin);return
        port=ui.combobox_port.get().strip().upper()
        if not re.fullmatch(r'COM[1-9][0-9]*',port):messagebox.showerror('选择串口','请选择接到W803的USB串口端口。',parent=ui.mainwin);return
        sys.argv=[str(vendor),str(image)]
        browse_button.configure(state='disabled');project_button.configure(state='disabled')
        original()
    ui.button_OK.configure(command=start)
    use_project(silent=True)
    ui.firmware_path=path_var
    return ui

def main():
    try:ui=make_ui()
    except AssertionError:
        messagebox.showerror('W803 serial port','No serial adapter found. Connect the USB serial adapter, then reopen Flash.');return 1
    ui.show();return 0
if __name__=='__main__':sys.exit(main())
