[CmdletBinding()]
param([string]$OutputDirectory=(Join-Path $PSScriptRoot '..\..\evidence\visual-audit-20261105'),[ValidateSet('network','volume','battery','clock','framework')][string[]]$Target=@('network'),[switch]$Hover,[switch]$MenuOnly)
$ErrorActionPreference='Stop';$OutputDirectory=[IO.Path]::GetFullPath($OutputDirectory)
[IO.Directory]::CreateDirectory($OutputDirectory)|Out-Null
Add-Type -AssemblyName UIAutomationClient,UIAutomationTypes,System.Drawing
Add-Type -TypeDefinition @'
using System;using System.Text;using System.Collections.Generic;using System.Runtime.InteropServices;
public static class AuditReference {
 [StructLayout(LayoutKind.Sequential)]public struct Rect{public int left,top,right,bottom;}
 [StructLayout(LayoutKind.Sequential)]public struct Point{public int x,y;}
 public class Window{public long Handle;public string Class;public uint Dpi;public Rect Bounds;}
 delegate bool Callback(IntPtr w,IntPtr p);
 [DllImport("user32.dll",CharSet=CharSet.Unicode)]public static extern IntPtr FindWindow(string c,string n);
 [DllImport("user32.dll")]static extern bool EnumWindows(Callback c,IntPtr p);
 [DllImport("user32.dll")]static extern bool IsWindowVisible(IntPtr w);
 [DllImport("user32.dll",CharSet=CharSet.Unicode)]static extern int GetClassName(IntPtr w,StringBuilder b,int n);
 [DllImport("user32.dll")]public static extern uint GetWindowThreadProcessId(IntPtr w,out uint p);
 [DllImport("user32.dll")]static extern uint GetDpiForWindow(IntPtr w);
 [DllImport("user32.dll")]static extern bool GetWindowRect(IntPtr w,out Rect r);
 [DllImport("user32.dll")]public static extern IntPtr SetThreadDpiAwarenessContext(IntPtr c);
 [DllImport("user32.dll")]public static extern bool GetPhysicalCursorPos(out Point p);
 [DllImport("user32.dll")]public static extern bool SetCursorPos(int x,int y);
 [DllImport("user32.dll")]static extern void mouse_event(uint f,uint x,uint y,uint data,UIntPtr extra);
 [DllImport("user32.dll")]static extern void keybd_event(byte k,byte s,uint f,UIntPtr extra);
 public static void RightClick(int x,int y){SetCursorPos(x,y);mouse_event(8,0,0,0,UIntPtr.Zero);mouse_event(16,0,0,0,UIntPtr.Zero);}
 public static void Escape(){keybd_event(27,0,0,UIntPtr.Zero);keybd_event(27,0,2,UIntPtr.Zero);}
 public static Window[] Popups(uint pid){var result=new List<Window>();EnumWindows((w,p)=>{uint id;GetWindowThreadProcessId(w,out id);var b=new StringBuilder(256);GetClassName(w,b,256);string c=b.ToString();if(id==pid&&IsWindowVisible(w)&&(c.Contains("Popup")||c=="#32768")){Rect r;GetWindowRect(w,out r);if(r.right-r.left>2&&r.bottom-r.top>2)result.Add(new Window{Handle=w.ToInt64(),Class=c,Dpi=GetDpiForWindow(w),Bounds=r});}return true;},IntPtr.Zero);return result.ToArray();}
}
'@
$dpi=[AuditReference]::SetThreadDpiAwarenessContext([IntPtr]::new(-4));$cursor=New-Object AuditReference+Point;[void][AuditReference]::GetPhysicalCursorPos([ref]$cursor)
$barWindow=[AuditReference]::FindWindow('Shell_TrayWnd',$null);[uint32]$explorerId=0;[void][AuditReference]::GetWindowThreadProcessId($barWindow,[ref]$explorerId)
$bar=[Windows.Automation.AutomationElement]::FromHandle($barWindow)
$targets=@{}
if($Target.Count-eq1 -and $Target[0]-eq'framework'){
 $elements=@($bar.FindFirst([Windows.Automation.TreeScope]::Descendants,[Windows.Automation.PropertyCondition]::new([Windows.Automation.AutomationElement]::NameProperty,"Yata Native Framework`nWindows controls")))
}else{
 $classes=[Windows.Automation.Condition[]]@('SystemTray.NormalButton','SystemTray.AccentButton','SystemTray.OmniButtonCenter','SystemTray.OmniButton'|ForEach-Object{[Windows.Automation.PropertyCondition]::new([Windows.Automation.AutomationElement]::ClassNameProperty,$_)})
 $elements=$bar.FindAll([Windows.Automation.TreeScope]::Descendants,[Windows.Automation.OrCondition]::new($classes))
}
foreach($e in $elements){
 if(!$e){continue}
 $c=$e.Current;if($c.ClassName-like 'SystemTray.*' -and $c.Name-like 'Network*'){$targets.network=$e}
 if($c.ClassName-like 'SystemTray.*' -and $c.Name-like 'Clock*'){$targets.clock=$e}
 if($c.ClassName-like 'SystemTray.*' -and $c.Name-like 'Volume*'){$targets.volume=$e}
 if($c.ClassName-like 'SystemTray.*' -and $c.Name-like 'Power*'){$targets.battery=$e}
 if($c.ClassName-like 'SystemTray.*' -and $c.Name-like 'Yata Native Framework*'){$targets.framework=$e}
}
$records=[Collections.Generic.List[object]]::new()
try{
 foreach($targetName in $Target){
  if(!$targets[$targetName]){continue};$r=$targets[$targetName].Current.BoundingRectangle
  [AuditReference]::RightClick([int]($r.X+$r.Width/2),[int]($r.Y+$r.Height/2));Start-Sleep -Milliseconds 500
  $popupOwner=if($targetName-eq'framework'){@(Get-Process YataNativeFramework -ErrorAction Stop)[0].Id}else{$explorerId}
  $popups=[AuditReference]::Popups([uint32]$popupOwner)
  foreach($popup in $popups){
   $root=[Windows.Automation.AutomationElement]::FromHandle([IntPtr]::new($popup.Handle));$rows=@()
   # The Win32 PopupHost may expose a transient sibling after the XAML popup.
   # Measure the semantic menu subtree, excluding that host's provider bridge.
   $menu=$root.FindFirst([Windows.Automation.TreeScope]::Descendants,[Windows.Automation.PropertyCondition]::new([Windows.Automation.AutomationElement]::ControlTypeProperty,[Windows.Automation.ControlType]::Menu))
   if(!$menu){continue}
   if($Hover){
    $first=$menu.FindFirst([Windows.Automation.TreeScope]::Descendants,[Windows.Automation.PropertyCondition]::new([Windows.Automation.AutomationElement]::ControlTypeProperty,[Windows.Automation.ControlType]::MenuItem))
    if($first){$r=$first.Current.BoundingRectangle;[void][AuditReference]::SetCursorPos([int]($r.X+$r.Width/2),[int]($r.Y+$r.Height/2));Start-Sleep -Milliseconds 180}
   }
   foreach($element in @($menu)+@($menu.FindAll([Windows.Automation.TreeScope]::Descendants,[Windows.Automation.Condition]::TrueCondition))){
    $c=$element.Current;$pattern=$null;$font=$null;$size=$null
    if($element.TryGetCurrentPattern([Windows.Automation.TextPattern]::Pattern,[ref]$pattern)){try{$font=$pattern.DocumentRange.GetAttributeValue([Windows.Automation.TextPattern]::FontNameAttribute);$size=$pattern.DocumentRange.GetAttributeValue([Windows.Automation.TextPattern]::FontSizeAttribute)}catch{}}
    $r=$c.BoundingRectangle;if($r.IsEmpty){continue};$rows+=[pscustomobject]@{Name=$c.Name;Type=$c.ControlType.ProgrammaticName;X=$r.X;Y=$r.Y;W=$r.Width;H=$r.Height;Font=$font;FontSizeRaw=$size}
   }
   $r=$popup.Bounds;$left=$r.left;$top=$r.top;$width=$r.right-$r.left;$height=$r.bottom-$r.top
   if($MenuOnly){$bounds=$menu.Current.BoundingRectangle;$left=[int][Math]::Round($bounds.X);$top=[int][Math]::Round($bounds.Y);$width=[int][Math]::Round($bounds.Width);$height=[int][Math]::Round($bounds.Height)}
   $bitmap=[Drawing.Bitmap]::new($width,$height);$graphics=[Drawing.Graphics]::FromImage($bitmap)
   $filename="reference-$targetName-$($popup.Handle).png"
   try{$graphics.CopyFromScreen($left,$top,0,0,$bitmap.Size);$bitmap.Save((Join-Path $OutputDirectory $filename),[Drawing.Imaging.ImageFormat]::Png)}finally{$graphics.Dispose();$bitmap.Dispose()}
   $records.Add([pscustomobject]@{Target=$targetName;Hovered=[bool]$Hover;Window=$popup;Screenshot=$filename;Elements=$rows})
  }
  if($popups.Count){[AuditReference]::Escape();Start-Sleep -Milliseconds 150}
 }
}finally{
 $records|ConvertTo-Json -Depth 9|Set-Content (Join-Path $OutputDirectory 'native-reference.json') -Encoding UTF8
 [void][AuditReference]::SetCursorPos($cursor.x,$cursor.y);[void][AuditReference]::SetThreadDpiAwarenessContext($dpi)
}
'Native reference capture complete'
