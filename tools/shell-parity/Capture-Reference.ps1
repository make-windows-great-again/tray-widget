param([string]$OutputDirectory=(Join-Path $PSScriptRoot '..\..\evidence\shell-parity-26100\reference'))
$ErrorActionPreference='Stop'
$OutputDirectory=[IO.Path]::GetFullPath($OutputDirectory);[IO.Directory]::CreateDirectory($OutputDirectory)|Out-Null
Add-Type -AssemblyName UIAutomationClient,UIAutomationTypes,System.Drawing,System.Windows.Forms
Add-Type @'
using System;using System.Text;using System.Collections.Generic;using System.Runtime.InteropServices;
public static class RefNative {
 [StructLayout(LayoutKind.Sequential)]public struct Rect{public int l,t,r,b;}
 public class Win{public long Handle;public string Class;public uint Pid;public Rect Bounds;}
 delegate bool CB(IntPtr w,IntPtr p);
 [DllImport("user32.dll")]public static extern IntPtr GetForegroundWindow();
 [DllImport("user32.dll")]static extern bool EnumWindows(CB c,IntPtr p);
 [DllImport("user32.dll")]static extern bool IsWindowVisible(IntPtr w);
 [DllImport("user32.dll",CharSet=CharSet.Unicode)]static extern int GetClassName(IntPtr w,StringBuilder b,int n);
 [DllImport("user32.dll")]static extern bool GetWindowRect(IntPtr w,out Rect r);
 [DllImport("user32.dll")]static extern uint GetWindowThreadProcessId(IntPtr w,out uint p);
 [DllImport("user32.dll")]public static extern IntPtr SetThreadDpiAwarenessContext(IntPtr c);
 [DllImport("user32.dll",SetLastError=true)]public static extern bool SetCursorPos(int x,int y);
 [DllImport("user32.dll",SetLastError=true)]public static extern bool SetPhysicalCursorPos(int x,int y);
 [StructLayout(LayoutKind.Sequential)]public struct Point{public int X,Y;}
 [DllImport("user32.dll")]static extern IntPtr WindowFromPoint(Point p);
 [DllImport("user32.dll")]static extern IntPtr GetAncestor(IntPtr w,uint flags);
 public static string RootAtPoint(int x,int y){var previous=SetThreadDpiAwarenessContext(new IntPtr(-4));try{var w=GetAncestor(WindowFromPoint(new Point{X=x,Y=y}),2);var name=new StringBuilder(128);GetClassName(w,name,128);return name.ToString();}finally{SetThreadDpiAwarenessContext(previous);}}
 public static bool IsTrayPoint(int x,int y){var name=RootAtPoint(x,y);return name=="Shell_TrayWnd"||name=="Shell_SecondaryTrayWnd"||name=="Shell_InputSwitchDismissOverlay";}
 [DllImport("user32.dll")]static extern void mouse_event(uint f,uint x,uint y,uint d,UIntPtr e);
 [DllImport("user32.dll")]static extern void keybd_event(byte k,byte s,uint f,UIntPtr e);
 public static void Click(int x,int y,bool right){var previous=SetThreadDpiAwarenessContext(new IntPtr(-4));try{if(!SetCursorPos(x,y))throw new Exception("Cannot position pointer in PMv2 context: "+Marshal.GetLastWin32Error());mouse_event(right?8u:2u,0,0,0,UIntPtr.Zero);System.Threading.Thread.Sleep(45);mouse_event(right?16u:4u,0,0,0,UIntPtr.Zero);}finally{SetThreadDpiAwarenessContext(previous);}}
 public static void Escape(){keybd_event(27,0,0,UIntPtr.Zero);keybd_event(27,0,2,UIntPtr.Zero);}
 public static Win[] Windows(){var previous=SetThreadDpiAwarenessContext(new IntPtr(-4));try{var a=new List<Win>();EnumWindows((w,p)=>{if(IsWindowVisible(w)){var b=new StringBuilder(256);GetClassName(w,b,256);Rect r;GetWindowRect(w,out r);uint id;GetWindowThreadProcessId(w,out id);if(r.r-r.l>5&&r.b-r.t>5)a.Add(new Win{Handle=w.ToInt64(),Class=b.ToString(),Pid=id,Bounds=r});}return true;},IntPtr.Zero);return a.ToArray();}finally{SetThreadDpiAwarenessContext(previous);}}
}
'@
[void][RefNative]::SetThreadDpiAwarenessContext([IntPtr]::new(-4))
$profile=Get-Content (Join-Path $PSScriptRoot '..\..\evidence\shell-parity-26100\taskbar.json')|ConvertFrom-Json
$back=[Windows.Forms.Form]::new();$back.FormBorderStyle='None';$back.StartPosition='Manual';$back.Bounds=[Drawing.Rectangle]::new(1450,100,1110,1428);$back.BackColor=[Drawing.Color]::FromArgb(96,96,96);$back.Show();$back.Refresh();[Windows.Forms.Application]::DoEvents()
try{
 Write-Output ('profileType='+$profile.GetType().FullName+' count='+$profile.Count); foreach($name in @('lang','quick','date')){
  $pattern=@{lang='Tray Input Indicator*';quick='Volume*';date='Clock*'}[$name]
  $button=$profile|Where-Object Name -like $pattern|Select-Object -First 1
  if(!$button){throw "Missing tray button $name"}
  foreach($right in @($false,$true)){
   $back.Activate();[Windows.Forms.Application]::DoEvents();Start-Sleep -Milliseconds 150
   $before=@([RefNative]::Windows()|ForEach-Object Handle)
   [RefNative]::Click([int]($button.X+$button.W/2),[int]($button.Y+$button.H/2),$right);Start-Sleep -Milliseconds 800
   Write-Output ('click '+$name+' x='+$button.X+' y='+$button.Y); $case=$name+$(if($right){'-context'}else{'-panel'})
   $snapshot=[Drawing.Bitmap]::new(1120,1600);$graphics=[Drawing.Graphics]::FromImage($snapshot);try{$graphics.CopyFromScreen(1440,0,0,0,$snapshot.Size);$snapshot.Save((Join-Path $OutputDirectory ($case+'-crop.png')))}finally{$graphics.Dispose();$snapshot.Dispose()}
   $focus=[RefNative]::GetForegroundWindow(); $rootFocus=[Windows.Automation.AutomationElement]::FromHandle($focus); $b=$rootFocus.Current.BoundingRectangle; Write-Output ('foreground='+$focus+' '+$rootFocus.Current.ClassName); $allWindows=@([pscustomobject]@{Handle=$focus.ToInt64();Class=$rootFocus.Current.ClassName;Pid=$rootFocus.Current.ProcessId;Bounds=[pscustomobject]@{l=[int]$b.Left;t=[int]$b.Top;r=[int]$b.Right;b=[int]$b.Bottom}}); $allWindows|ConvertTo-Json -Depth 4|Set-Content (Join-Path $OutputDirectory ($case+'-all.json'));  $windows=$allWindows
   Write-Output ('windows='+$windows.Count+' output='+$OutputDirectory); $windows|ConvertTo-Json -Depth 5|Set-Content (Join-Path $OutputDirectory "$case-windows.json") -Encoding UTF8
   foreach($w in $windows){
    $r=$w.Bounds;$bitmap=[Drawing.Bitmap]::new($r.r-$r.l,$r.b-$r.t);$g=[Drawing.Graphics]::FromImage($bitmap)
    try{$g.CopyFromScreen($r.l,$r.t,0,0,$bitmap.Size);$bitmap.Save((Join-Path $OutputDirectory "$case-$($w.Handle).png"))}finally{$g.Dispose();$bitmap.Dispose()}
    $desktopRows=foreach($de in [Windows.Automation.AutomationElement]::RootElement.FindAll([Windows.Automation.TreeScope]::Children,[Windows.Automation.Condition]::TrueCondition)){$dc=$de.Current;$db=$dc.BoundingRectangle;[pscustomobject]@{Name=$dc.Name;Class=$dc.ClassName;Handle=$dc.NativeWindowHandle;Pid=$dc.ProcessId;Bounds=$db.ToString()}};$desktopRows|ConvertTo-Json|Set-Content (Join-Path $OutputDirectory ($case+'-desktop.json')); $root=[Windows.Automation.AutomationElement]::FromHandle([IntPtr]::new($w.Handle));$rows=foreach($e in $root.FindAll([Windows.Automation.TreeScope]::Descendants,[Windows.Automation.Condition]::TrueCondition)){
     $c=$e.Current;$b=$c.BoundingRectangle;if(!$b.IsEmpty){[pscustomobject]@{Name=$c.Name;Id=$c.AutomationId;Class=$c.ClassName;Type=$c.ControlType.ProgrammaticName;X=$b.X;Y=$b.Y;W=$b.Width;H=$b.Height;Enabled=$c.IsEnabled}}
    };$rows|ConvertTo-Json -Depth 4|Set-Content (Join-Path $OutputDirectory "$case-$($w.Handle)-uia.json") -Encoding UTF8
   }
   [RefNative]::Escape();Start-Sleep -Milliseconds 400
  }
 }
}finally{$back.Close();$back.Dispose()}
