param([string]$Label='hover-font-audit',[switch]$OnlyVpn,[switch]$NoMenus,[switch]$DefinitionsOnly)
$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue'
trap {($_|Out-String)+$_.ScriptStackTrace|Set-Content -LiteralPath "$PSScriptRoot\hover-font-error.txt" -Encoding UTF8;exit 1}
Add-Type -AssemblyName UIAutomationClient,UIAutomationTypes,System.Drawing
Add-Type -TypeDefinition @'
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
public static class HoverAudit {
 [StructLayout(LayoutKind.Sequential)]struct Identifier{public uint size;public IntPtr window;public uint id;public Guid guid;}
 [DllImport("shell32.dll")]static extern int Shell_NotifyIconGetRect(ref Identifier item,out Rect rect);
 public static Rect NativeIconBounds(){var item=new Identifier{size=(uint)Marshal.SizeOf(typeof(Identifier)),guid=new Guid("ce31fe29-112b-4c67-a3b0-dc2459420796")};Rect rect;Marshal.ThrowExceptionForHR(Shell_NotifyIconGetRect(ref item,out rect));return rect;}
 [StructLayout(LayoutKind.Sequential)] public struct Point {public int x,y;}
 [StructLayout(LayoutKind.Sequential)] public struct Rect {public int left,top,right,bottom;}
 [StructLayout(LayoutKind.Sequential,CharSet=CharSet.Unicode)] public class Font {
  public int height,width,escapement,orientation,weight;public byte italic,underline,strikeOut,charSet,outPrecision,clipPrecision,quality,pitch;
  [MarshalAs(UnmanagedType.ByValTStr,SizeConst=32)]public string face;
 }
 public delegate bool Callback(IntPtr w,IntPtr p);
 [DllImport("user32.dll")] static extern bool EnumDesktopWindows(IntPtr desktop,Callback callback,IntPtr value);
 [DllImport("user32.dll")] static extern bool IsWindowVisible(IntPtr w);
 [DllImport("user32.dll",CharSet=CharSet.Unicode)] static extern int GetClassName(IntPtr w,System.Text.StringBuilder s,int length);
 [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr w,out uint p);
 [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr w,out Rect r);
 [DllImport("user32.dll")] public static extern uint GetDpiForWindow(IntPtr w);
 [DllImport("user32.dll")] public static extern IntPtr SetThreadDpiAwarenessContext(IntPtr c);
 [DllImport("user32.dll")] public static extern int GetSystemMetrics(int n);
 [DllImport("user32.dll",CharSet=CharSet.Unicode)] public static extern IntPtr FindWindow(string c,string t);
 [DllImport("user32.dll")] public static extern bool GetPhysicalCursorPos(out Point p);
 [DllImport("user32.dll")] public static extern void mouse_event(uint f,uint x,uint y,uint d,UIntPtr e);
 [DllImport("user32.dll")] public static extern void keybd_event(byte k,byte s,uint f,UIntPtr e);
 [DllImport("user32.dll")] static extern IntPtr SendMessage(IntPtr w,uint m,IntPtr a,IntPtr b);
 [DllImport("gdi32.dll",CharSet=CharSet.Unicode)] static extern int GetObject(IntPtr h,int size,[Out]Font font);
 public static string Class(IntPtr w){var s=new System.Text.StringBuilder(256);GetClassName(w,s,256);return s.ToString();}
 public static IntPtr[] Popups(){var list=new List<IntPtr>();EnumDesktopWindows(IntPtr.Zero,(w,p)=>{string c=Class(w);if(IsWindowVisible(w)&&(c.Contains("tooltips")||c.Contains("Popup")||c=="Windows.UI.Core.CoreWindow"))list.Add(w);return true;},IntPtr.Zero);return list.ToArray();}
 public static Font GetFont(IntPtr w){var h=SendMessage(w,0x31,IntPtr.Zero,IntPtr.Zero);if(h==IntPtr.Zero)return null;var f=new Font();return GetObject(h,Marshal.SizeOf(f),f)>0?f:null;}
}
'@
[void][HoverAudit]::SetThreadDpiAwarenessContext([IntPtr]::new(-4))
$barWindow=[HoverAudit]::FindWindow('Shell_TrayWnd',$null);$bar=[Windows.Automation.AutomationElement]::FromHandle($barWindow)
$all=$bar.FindAll([Windows.Automation.TreeScope]::Descendants,[Windows.Automation.Condition]::TrueCondition)
$targets=@{}
foreach($e in $all){$c=$e.Current
 if($c.Name -like 'Yata Native Framework*' -and $c.AutomationId -eq 'NotifyItemIcon'){$targets.vpn=$e}
 if($c.ClassName -like 'SystemTray.*' -and $c.Name -like 'Network *'){$targets.quick=$e}
 if($c.ClassName -like 'SystemTray.*' -and $c.Name -like 'Clock *'){$targets.date=$e}
}
$records=[Collections.Generic.List[object]]::new()
function Move-Pointer([int]$x,[int]$y){$mx=[uint32]($x*65535/([HoverAudit]::GetSystemMetrics(0)-1));$my=[uint32]($y*65535/([HoverAudit]::GetSystemMetrics(1)-1));[HoverAudit]::mouse_event(0x8001,$mx,$my,0,[UIntPtr]::Zero)}
function Sample([string]$target,[string]$phase){
 foreach($w in [HoverAudit]::Popups()){
  $r=New-Object HoverAudit+Rect;[void][HoverAudit]::GetWindowRect($w,[ref]$r)
  if($r.right-$r.left -lt 10 -or $r.bottom-$r.top -lt 10){continue}
  $font=[HoverAudit]::GetFont($w);[uint32]$owner=0;[void][HoverAudit]::GetWindowThreadProcessId($w,[ref]$owner)
  $texts=@()
  try{$root=[Windows.Automation.AutomationElement]::FromHandle($w);foreach($e in $root.FindAll([Windows.Automation.TreeScope]::Descendants,[Windows.Automation.Condition]::TrueCondition)){
   $pattern=$null
   if($e.TryGetCurrentPattern([Windows.Automation.TextPattern]::Pattern,[ref]$pattern)){$texts += [pscustomobject]@{Name=$e.Current.Name;Font=$pattern.DocumentRange.GetAttributeValue([Windows.Automation.TextPattern]::FontNameAttribute);SizeRaw=$pattern.DocumentRange.GetAttributeValue([Windows.Automation.TextPattern]::FontSizeAttribute);Bounds=$e.Current.BoundingRectangle.ToString()}}
  }}catch{}
  $records.Add([pscustomobject]@{Target=$target;Phase=$phase;Class=[HoverAudit]::Class($w);Pid=$owner;X=$r.left;Y=$r.top;Width=$r.right-$r.left;Height=$r.bottom-$r.top;Dpi=[HoverAudit]::GetDpiForWindow($w);GdiFont=$font;Text=$texts})
 }
 $b=New-Object Drawing.Bitmap(1100,550);$g=[Drawing.Graphics]::FromImage($b)
 try{$g.CopyFromScreen([HoverAudit]::GetSystemMetrics(0)-1100,[HoverAudit]::GetSystemMetrics(1)-550,0,0,$b.Size);$b.Save("$PSScriptRoot\$Label-$target-$phase.png",[Drawing.Imaging.ImageFormat]::Png)}finally{$g.Dispose();$b.Dispose()}
}
if($DefinitionsOnly){return}
$names=if($OnlyVpn){@('vpn')}else{@('vpn','quick','date')}
foreach($name in $names){
 if(!$targets[$name]){continue};$r=$targets[$name].Current.BoundingRectangle
 if($name -eq 'vpn'){$physical=[HoverAudit]::NativeIconBounds();$r=[Windows.Rect]::new($physical.left,$physical.top,$physical.right-$physical.left,$physical.bottom-$physical.top)}
 foreach($position in 'bottom','top'){
  Move-Pointer ([int]$r.X-200) ([int]$r.Y-250);Start-Sleep -Milliseconds 900
  $y=if($position -eq 'top'){$r.Y+3}else{$r.Bottom-3}
  Move-Pointer ([int]($r.X+$r.Width/2)) ([int]$y);Start-Sleep -Milliseconds 1100
  Sample $name $position
 }
 if($NoMenus){continue}
 [HoverAudit]::mouse_event(8,0,0,0,[UIntPtr]::Zero);Start-Sleep -Milliseconds 30;[HoverAudit]::mouse_event(16,0,0,0,[UIntPtr]::Zero);Start-Sleep -Milliseconds 400
 Sample $name 'menu'
 [HoverAudit]::keybd_event(27,0,0,[UIntPtr]::Zero);[HoverAudit]::keybd_event(27,0,2,[UIntPtr]::Zero);Start-Sleep -Milliseconds 100
}
$records|ConvertTo-Json -Depth 8|Set-Content -LiteralPath "$PSScriptRoot\$Label.json" -Encoding UTF8
