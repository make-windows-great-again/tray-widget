param([string]$Mapping='Local\YataNativeFramework.v13',[string]$Label='material-live-v10',[string[]]$Methods=@('toggle'))
$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue'
Add-Type -AssemblyName UIAutomationClient,UIAutomationTypes
Add-Type -TypeDefinition @'
using System;using System.Collections.Generic;using System.Diagnostics;using System.Runtime.InteropServices;
public static class MaterialCapture {
 [StructLayout(LayoutKind.Sequential)]public struct Rect{public int left,top,right,bottom;}
 [StructLayout(LayoutKind.Sequential)]struct Header{public uint size;public int width,height;public ushort planes,bits;public uint compression,sizeImage;public int x,y;public uint used,important;}
 [StructLayout(LayoutKind.Sequential)]struct Info{public Header header;public uint color;}
 public class Frame{public double Ms;public int Top;public double R,G,B;}
 [DllImport("user32.dll",CharSet=CharSet.Unicode)]public static extern IntPtr CreateWindowEx(uint ex,string c,string n,int style,int x,int y,int w,int h,IntPtr p,IntPtr menu,IntPtr instance,IntPtr data);
 [DllImport("user32.dll")]public static extern bool DestroyWindow(IntPtr w);
 [DllImport("user32.dll")]public static extern bool SetForegroundWindow(IntPtr w);
 [StructLayout(LayoutKind.Sequential)]struct Point{public int x,y;}
 [DllImport("user32.dll")]static extern bool GetPhysicalCursorPos(out Point p);
 [DllImport("user32.dll")]static extern bool SetCursorPos(int x,int y);
 [DllImport("user32.dll")]static extern void mouse_event(uint flags,uint x,uint y,uint data,UIntPtr extra);
 [DllImport("user32.dll")]static extern int GetSystemMetrics(int value);
 [DllImport("user32.dll")]static extern IntPtr WindowFromPoint(Point point);
 static void Move(int x,int y){mouse_event(0x8001,(uint)(x*65535.0/(GetSystemMetrics(0)-1)),(uint)(y*65535.0/(GetSystemMetrics(1)-1)),0,UIntPtr.Zero);}
 [DllImport("user32.dll")]public static extern bool SetProcessDpiAwarenessContext(IntPtr c);
 [DllImport("user32.dll",CharSet=CharSet.Unicode)]public static extern IntPtr FindWindow(string c,string n);
 [DllImport("user32.dll")]public static extern bool GetWindowRect(IntPtr w,out Rect r);
 [DllImport("user32.dll")]public static extern uint GetDpiForWindow(IntPtr w);
 [DllImport("user32.dll",CharSet=CharSet.Unicode)]public static extern uint RegisterWindowMessage(string n);
 [DllImport("user32.dll")]public static extern bool PostMessage(IntPtr w,uint m,IntPtr a,IntPtr b);
 [DllImport("user32.dll")]static extern IntPtr GetDC(IntPtr w);
 [DllImport("user32.dll")]static extern int ReleaseDC(IntPtr w,IntPtr dc);
 [DllImport("gdi32.dll")]static extern IntPtr CreateCompatibleDC(IntPtr dc);
 [DllImport("gdi32.dll")]static extern bool DeleteDC(IntPtr dc);
 [DllImport("gdi32.dll")]static extern bool DeleteObject(IntPtr o);
 [DllImport("gdi32.dll")]static extern IntPtr SelectObject(IntPtr dc,IntPtr o);
 [DllImport("gdi32.dll",SetLastError=true)]static extern IntPtr CreateDIBSection(IntPtr dc,ref Info info,uint usage,out IntPtr data,IntPtr section,uint offset);
 [DllImport("gdi32.dll")]static extern bool BitBlt(IntPtr to,int x,int y,int w,int h,IntPtr from,int fx,int fy,uint op);
 [DllImport("gdi32.dll")]static extern bool GdiFlush();
 public static Frame[] Run(IntPtr owner,int left,int top,int bottom,IntPtr focus){
  int width=6,height=bottom-top;var info=new Info();info.header.size=40;info.header.width=width;info.header.height=-height;info.header.planes=1;info.header.bits=32;
  var screen=GetDC(IntPtr.Zero);var memory=CreateCompatibleDC(screen);IntPtr data;
  var bitmap=CreateDIBSection(memory,ref info,0,out data,IntPtr.Zero,0);if(bitmap==IntPtr.Zero)throw new System.ComponentModel.Win32Exception(Marshal.GetLastWin32Error(),"DIB "+width+"x"+height+" header "+Marshal.SizeOf(typeof(Header)));var old=SelectObject(memory,bitmap);
  var buffer=new byte[width*height*4];var records=new List<Frame>();var clock=Stopwatch.StartNew();
  try{
   // First frame captures the open material; then trigger closing.
   for(int frame=0;frame<24;++frame){
    BitBlt(memory,0,0,width,height,screen,left+6,top,0x00CC0020);GdiFlush();Marshal.Copy(data,buffer,0,buffer.Length);
    int first=-1;for(int y=0;y<height-40;++y){bool visible=true;foreach(int dy in new[]{0,8,16})if(buffer[((y+dy)*width+2)*4+2]<160)visible=false;if(visible){first=y;break;}}
    if(first>=0){double r=0,g=0,b=0;int count=0;for(int y=first+24;y<first+40;y++)for(int x=0;x<width;x++){int i=(y*width+x)*4;r+=buffer[i+2];g+=buffer[i+1];b+=buffer[i];count++;}
     records.Add(new Frame{Ms=clock.Elapsed.TotalMilliseconds,Top=top+first,R=r/count,G=g/count,B=b/count});}
    if(frame==0){if(focus!=IntPtr.Zero){
      Rect r;GetWindowRect(focus,out r);Point previous;GetPhysicalCursorPos(out previous);Move((r.left+r.right)/2,(r.top+r.bottom)/2);
      System.Threading.Thread.Sleep(30);Point actual;GetPhysicalCursorPos(out actual);
      if(actual.x<r.left || actual.x>=r.right || actual.y<r.top || actual.y>=r.bottom || WindowFromPoint(actual)!=focus)throw new InvalidOperationException("Pointer did not reach the owned focus-test window; no click sent");
      mouse_event(2,0,0,0,UIntPtr.Zero);mouse_event(4,0,0,0,UIntPtr.Zero);Move(previous.x,previous.y);
     }else PostMessage(owner,RegisterWindowMessage("YataVpn.Tray.Notify.v4"),IntPtr.Zero,new IntPtr(0x400));}
    System.Threading.Thread.Sleep(10);
   }
  }finally{SelectObject(memory,old);DeleteObject(bitmap);DeleteDC(memory);ReleaseDC(IntPtr.Zero,screen);}
  return records.ToArray();
 }
}
'@
[void][MaterialCapture]::SetProcessDpiAwarenessContext([IntPtr]::new(-4))
$map=[IO.MemoryMappedFiles.MemoryMappedFile]::OpenExisting($Mapping);$view=$map.CreateViewAccessor();$owner=[IntPtr]::new($view.ReadInt64(8))
$root=[Windows.Automation.AutomationElement]::FromHandle($owner)
$filter=[Windows.Automation.PropertyCondition]::new([Windows.Automation.AutomationElement]::AutomationIdProperty,'VpnPanelSurface')
$background=[IntPtr]::Zero;$other=[IntPtr]::Zero;$output=@();$checks=@()
try{
 if($view.ReadInt32(56)){
  $close=$root.FindFirst([Windows.Automation.TreeScope]::Descendants,[Windows.Automation.PropertyCondition]::new([Windows.Automation.AutomationElement]::AutomationIdProperty,'ClosePanel'))
  $close.GetCurrentPattern([Windows.Automation.InvokePattern]::Pattern).Invoke();Start-Sleep -Milliseconds 400
 }
 $background=[MaterialCapture]::CreateWindowEx(0x08000088,'STATIC','Material test background',0x90000004,1750,850,810,678,[IntPtr]::Zero,[IntPtr]::Zero,[IntPtr]::Zero,[IntPtr]::Zero)
 $other=[MaterialCapture]::CreateWindowEx(0x88,'STATIC','Material focus target',0x90000100,100,100,40,40,[IntPtr]::Zero,[IntPtr]::Zero,[IntPtr]::Zero,[IntPtr]::Zero)
 foreach($method in $Methods){
  [void][MaterialCapture]::PostMessage($owner,[MaterialCapture]::RegisterWindowMessage('YataVpn.Tray.Show.v4'),[IntPtr]::Zero,[IntPtr]::Zero);Start-Sleep -Milliseconds 700
  if($view.ReadInt32(60) -ne 2){throw 'Panel did not remain open before capture; focus or user input interrupted the setup'}
  $root=[Windows.Automation.AutomationElement]::FromHandle($owner)
  $surface=$root.FindFirst([Windows.Automation.TreeScope]::Descendants,$filter).Current.BoundingRectangle
  if($null -eq $surface){
   $bounds=New-Object MaterialCapture+Rect;[void][MaterialCapture]::GetWindowRect($owner,[ref]$bounds)
   $barBounds=New-Object MaterialCapture+Rect;[void][MaterialCapture]::GetWindowRect([MaterialCapture]::FindWindow('Shell_TrayWnd',$null),[ref]$barBounds)
   $scale=[MaterialCapture]::GetDpiForWindow($owner)/96.0
   $surface=[pscustomobject]@{X=$bounds.left+24*$scale;Y=$bounds.top+24*$scale;Bottom=$barBounds.top-12*$scale}
  }
  "phase=$($view.ReadInt32(60)) surface=$surface" | Add-Content -LiteralPath "$PSScriptRoot\$Label-geometry.txt"
  $focus=if($method -eq 'deactivate'){$other}else{[IntPtr]::Zero}
  $frames=[MaterialCapture]::Run($owner,[int]$surface.X,[int]$surface.Y,[Math]::Min(1528,[int]$surface.Bottom),$focus)
  $output += [pscustomobject]@{Method=$method;Frames=$frames}
  foreach($channel in 'R','G','B'){
   $range=$frames | Measure-Object -Property $channel -Minimum -Maximum
   if($frames.Count -lt 6 -or $range.Maximum-$range.Minimum -gt 2){throw "Unstable material during $method ($channel): $($range.Minimum)..$($range.Maximum), $($frames.Count) samples"}
  }
  Start-Sleep -Milliseconds 250
  if($view.ReadInt32(60) -ne 0){throw "Closing did not complete during $method"}
  $checks += "PASS $method keeps background RGB stable across $($frames.Count) visible frames and completes closing"
 }
 $checks+='ALL LIVE MATERIAL CHECKS PASSED'
}catch{$checks+="FAIL $($_.Exception.Message)";throw}
finally{
 if($other -ne [IntPtr]::Zero){[void][MaterialCapture]::DestroyWindow($other)}
 if($background -ne [IntPtr]::Zero){[void][MaterialCapture]::DestroyWindow($background)}
 $output | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath "$PSScriptRoot\$Label-samples.json" -Encoding UTF8
 $checks | Set-Content -LiteralPath "$PSScriptRoot\$Label-results.txt" -Encoding UTF8
 $view.Dispose();$map.Dispose()
}
