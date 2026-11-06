[CmdletBinding()]
param([string]$OutputDirectory=(Join-Path $PSScriptRoot '..\..\evidence\visual-audit-20261105'),[ValidateSet('Buttons','Selection','Menus','Navigation','All')][string]$Mode='Buttons')
$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue'
$OutputDirectory=[IO.Path]::GetFullPath($OutputDirectory);[IO.Directory]::CreateDirectory($OutputDirectory)|Out-Null
Add-Type -AssemblyName UIAutomationClient,UIAutomationTypes,System.Drawing
Add-Type -TypeDefinition @'
using System;using System.Text;using System.Collections.Generic;using System.Runtime.InteropServices;
public static class VisualAuditNative {
 [StructLayout(LayoutKind.Sequential)]public struct Rect{public int left,top,right,bottom;}
 [StructLayout(LayoutKind.Sequential)]public struct Point{public int x,y;}
 [StructLayout(LayoutKind.Sequential,CharSet=CharSet.Unicode)]public struct MonitorInfo{public uint size;public Rect monitor,work;public uint flags;[MarshalAs(UnmanagedType.ByValTStr,SizeConst=32)]public string device;}
 public class Window{public long Handle;public uint Pid,Dpi;public string Class;public Rect Bounds;}
 public class Monitor{public string Device;public Rect Bounds,Work;public uint Dpi;public bool Primary;}
 delegate bool WindowCallback(IntPtr w,IntPtr p);delegate bool MonitorCallback(IntPtr m,IntPtr dc,IntPtr r,IntPtr p);
 [DllImport("user32.dll")]static extern bool EnumWindows(WindowCallback c,IntPtr p);
 [DllImport("user32.dll")]static extern bool IsWindowVisible(IntPtr w);
 [DllImport("user32.dll",CharSet=CharSet.Unicode)]static extern int GetClassName(IntPtr w,StringBuilder b,int n);
 [DllImport("user32.dll")]public static extern bool GetWindowRect(IntPtr w,out Rect r);
 [DllImport("user32.dll")]public static extern uint GetWindowThreadProcessId(IntPtr w,out uint p);
 [DllImport("user32.dll")]public static extern uint GetDpiForWindow(IntPtr w);
 [DllImport("user32.dll")]public static extern IntPtr SetThreadDpiAwarenessContext(IntPtr c);
 [DllImport("user32.dll")]public static extern bool SetForegroundWindow(IntPtr w);
 [DllImport("user32.dll")]public static extern bool SetCursorPos(int x,int y);
 [DllImport("user32.dll")]public static extern bool GetPhysicalCursorPos(out Point p);
 [DllImport("user32.dll")]static extern void mouse_event(uint f,uint x,uint y,uint data,UIntPtr extra);
 [DllImport("user32.dll")]static extern void keybd_event(byte key,byte scan,uint flags,UIntPtr extra);
 [DllImport("user32.dll",CharSet=CharSet.Unicode)]public static extern IntPtr FindWindow(string c,string n);
 [DllImport("user32.dll")]static extern bool EnumDisplayMonitors(IntPtr dc,IntPtr clip,MonitorCallback c,IntPtr p);
 [DllImport("user32.dll",CharSet=CharSet.Unicode)]static extern bool GetMonitorInfo(IntPtr m,ref MonitorInfo info);
 [DllImport("shcore.dll")]static extern int GetDpiForMonitor(IntPtr m,int type,out uint x,out uint y);
 public static void Click(int x,int y,bool right){SetCursorPos(x,y);mouse_event(right?8u:2u,0,0,0,UIntPtr.Zero);mouse_event(right?16u:4u,0,0,0,UIntPtr.Zero);}
 public static void Escape(){keybd_event(27,0,0,UIntPtr.Zero);keybd_event(27,0,2,UIntPtr.Zero);}
 public static Window[] Windows(uint pid){var rows=new List<Window>();EnumWindows((w,p)=>{uint id;GetWindowThreadProcessId(w,out id);if(id==pid&&IsWindowVisible(w)){var b=new StringBuilder(256);GetClassName(w,b,256);Rect r;GetWindowRect(w,out r);if(r.right-r.left>2&&r.bottom-r.top>2)rows.Add(new Window{Handle=w.ToInt64(),Pid=id,Dpi=GetDpiForWindow(w),Class=b.ToString(),Bounds=r});}return true;},IntPtr.Zero);return rows.ToArray();}
 public static Monitor[] Monitors(){var rows=new List<Monitor>();EnumDisplayMonitors(IntPtr.Zero,IntPtr.Zero,(m,dc,r,p)=>{var i=new MonitorInfo();i.size=(uint)Marshal.SizeOf(i);GetMonitorInfo(m,ref i);uint x,y;GetDpiForMonitor(m,0,out x,out y);rows.Add(new Monitor{Device=i.device,Bounds=i.monitor,Work=i.work,Dpi=x,Primary=(i.flags&1)!=0});return true;},IntPtr.Zero);return rows.ToArray();}
}
'@
$oldDpi=[VisualAuditNative]::SetThreadDpiAwarenessContext([IntPtr]::new(-4))
$cursor=New-Object VisualAuditNative+Point;[void][VisualAuditNative]::GetPhysicalCursorPos([ref]$cursor)
$mapping=[IO.MemoryMappedFiles.MemoryMappedFile]::OpenExisting('Local\YataNativeFramework.v13');$view=$mapping.CreateViewAccessor(0,0,[IO.MemoryMappedFiles.MemoryMappedFileAccess]::Read)
$applicationId=$view.ReadInt32(4);$window=[IntPtr]::new($view.ReadInt64(8));$originalPanel=$view.ReadInt32(56)
$app=Join-Path $PSScriptRoot '..\..\bin\Release\YataNativeFramework.exe'
$records=[Collections.Generic.List[object]]::new()
function Show-Panel([switch]$KeepContent) {
 $arguments=if($KeepContent){'--show'}else{'--controls --show'}
 $info=[Diagnostics.ProcessStartInfo]::new([IO.Path]::GetFullPath($app),$arguments);$info.UseShellExecute=$false;$info.CreateNoWindow=$true
 $p=[Diagnostics.Process]::Start($info);try{if(!$p.WaitForExit(5000)){throw 'Show command timed out'}}finally{$p.Dispose()}
 Start-Sleep -Milliseconds 650;[void][VisualAuditNative]::SetForegroundWindow($window)
 $script:root=[Windows.Automation.AutomationElement]::FromHandle($window)
}
function Find-Element([string]$name,[string]$id=''){
 $condition=if($id){[Windows.Automation.PropertyCondition]::new([Windows.Automation.AutomationElement]::AutomationIdProperty,$id)}else{[Windows.Automation.PropertyCondition]::new([Windows.Automation.AutomationElement]::NameProperty,$name)}
 $element=$root.FindFirst([Windows.Automation.TreeScope]::Descendants,$condition)
 if(!$element){throw "Element not found: $name $id"};return $element
}
function Select-Page([string]$name){
 $e=Find-Element $name;$r=$e.Current.BoundingRectangle
 [VisualAuditNative]::Click([int]($r.X+$r.Width/2),[int]($r.Y+$r.Height/2),$false)
 Start-Sleep -Milliseconds 350
}
function Capture([string]$label){
 foreach($w in [VisualAuditNative]::Windows([uint32]$applicationId)){
  $rows=@();try{
   $element=[Windows.Automation.AutomationElement]::FromHandle([IntPtr]::new($w.Handle))
   foreach($e in $element.FindAll([Windows.Automation.TreeScope]::Descendants,[Windows.Automation.Condition]::TrueCondition)){
    $c=$e.Current;$font=$null;$size=$null;$pattern=$null
    if($e.TryGetCurrentPattern([Windows.Automation.TextPattern]::Pattern,[ref]$pattern)){
     try{$font=$pattern.DocumentRange.GetAttributeValue([Windows.Automation.TextPattern]::FontNameAttribute);$size=$pattern.DocumentRange.GetAttributeValue([Windows.Automation.TextPattern]::FontSizeAttribute)}catch{}
    }
    $r=$c.BoundingRectangle
    $rows+=[pscustomobject]@{Name=$c.Name;Id=$c.AutomationId;Class=$c.ClassName;Type=$c.ControlType.ProgrammaticName;Offscreen=$c.IsOffscreen;X=$r.X;Y=$r.Y;Width=$r.Width;Height=$r.Height;Font=$font;FontSizeRaw=$size}
   }
  }catch{$rows+=@{Error=$_.Exception.Message}}
  $r=$w.Bounds;$width=$r.right-$r.left;$height=$r.bottom-$r.top
  $bitmap=[Drawing.Bitmap]::new($width,$height);$graphics=[Drawing.Graphics]::FromImage($bitmap)
  $filename="$label-$($w.Handle).png"
  try{$graphics.CopyFromScreen($r.left,$r.top,0,0,$bitmap.Size);$bitmap.Save((Join-Path $OutputDirectory $filename),[Drawing.Imaging.ImageFormat]::Png)}finally{$graphics.Dispose();$bitmap.Dispose()}
  $records.Add([pscustomobject]@{Label=$label;Window=$w;Screenshot=$filename;Elements=$rows})
 }
}
try{
 [VisualAuditNative]::Monitors()|ConvertTo-Json -Depth 5|Set-Content (Join-Path $OutputDirectory 'monitors.json') -Encoding UTF8
 Show-Panel;Capture 'live-buttons'
 if($Mode-in 'Navigation','All'){
  Select-Page 'Навигация';Capture 'live-navigation-initial'
  for($cycle=1;$cycle-le3;$cycle++){
   $toggle=Find-Element '' 'TogglePaneButton';$r=$toggle.Current.BoundingRectangle
   [VisualAuditNative]::Click([int]($r.X+$r.Width/2),[int]($r.Y+$r.Height/2),$false);Start-Sleep -Milliseconds 400;Capture "live-navigation-open-$cycle"
   $r=$toggle.Current.BoundingRectangle;[VisualAuditNative]::Click([int]($r.X+$r.Width/2),[int]($r.Y+$r.Height/2),$false);Start-Sleep -Milliseconds 400;Capture "live-navigation-closed-$cycle"
  }
  $toggle=Find-Element '' 'TogglePaneButton';$r=$toggle.Current.BoundingRectangle
  [VisualAuditNative]::Click([int]($r.X+$r.Width/2),[int]($r.Y+$r.Height/2),$false);Start-Sleep -Milliseconds 400
  for($cycle=1;$cycle-le2;$cycle++){
   (Find-Element '' 'ClosePanel').GetCurrentPattern([Windows.Automation.InvokePattern]::Pattern).Invoke();Start-Sleep -Milliseconds 350
   Show-Panel -KeepContent;Capture "live-navigation-host-$cycle"
  }
 }
 if($Mode-eq 'All'){Select-Page 'Ввод';Capture 'live-input'}
 if($Mode-in 'Selection','All'){
 Select-Page 'Выбор';Capture 'live-selection'
 $combo=Find-Element '' 'ComboInput';$expand=$combo.GetCurrentPattern([Windows.Automation.ExpandCollapsePattern]::Pattern);$expand.Expand();Start-Sleep -Milliseconds 350;Capture 'live-combo';$expand.Collapse()
 }
 if($Mode-in 'Menus','All'){
 Select-Page 'Меню';Capture 'live-menu-page'
 $button=Find-Element 'Всплывающее меню';$button.GetCurrentPattern([Windows.Automation.InvokePattern]::Pattern).Invoke();Start-Sleep -Milliseconds 350;Capture 'live-gallery-menu'
 [VisualAuditNative]::Escape();Start-Sleep -Milliseconds 200
 }
 'Live captures complete'
}finally{
 $records|ConvertTo-Json -Depth 10|Set-Content (Join-Path $OutputDirectory "live-uia-$Mode.json") -Encoding UTF8
 [void][VisualAuditNative]::SetCursorPos($cursor.x,$cursor.y)
 if(!$originalPanel){[VisualAuditNative]::Escape()}
 $view.Dispose();$mapping.Dispose();[void][VisualAuditNative]::SetThreadDpiAwarenessContext($oldDpi)
}
