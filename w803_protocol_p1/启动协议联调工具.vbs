Set sh = CreateObject("WScript.Shell")
Set fs = CreateObject("Scripting.FileSystemObject")
root = fs.GetParentFolderName(WScript.ScriptFullName)
sh.Run Chr(34) & "D:\MRS6130\tools\w803_env\Scripts\pythonw.exe" & Chr(34) & " -B " & Chr(34) & root & "\Host\protocol_gui.py" & Chr(34), 0, False
