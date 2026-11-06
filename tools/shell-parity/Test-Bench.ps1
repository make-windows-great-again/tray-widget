param([switch]$Dark,[switch]$KeepOpen)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$out=Join-Path $root 'evidence\shell-parity-26100\cpp';[IO.Directory]::CreateDirectory($out)|Out-Null
Add-Type -AssemblyName System.Drawing,System.Windows.Forms
Add-Type @'
using System;using System.Runtime.InteropServices;
public static class BenchNative {
 [DllImport("user32.dll",CharSet=CharSet.Unicode)]public static extern IntPtr FindWindow(string c,string n);
 [DllImport("user32.dll")]public static extern bool PostMessage(IntPtr w,uint m,IntPtr p,IntPtr l);
 [DllImport("user32.dll")]public static extern IntPtr SendMessage(IntPtr w,uint m,IntPtr p,IntPtr l);
 [DllImport("user32.dll")]public static extern bool SetForegroundWindow(IntPtr w);
 [DllImport("user32.dll")]public static extern bool SetWindowPos(IntPtr w,IntPtr after,int x,int y,int width,int height,uint flags);
 [DllImport("user32.dll")]public static extern IntPtr SetThreadDpiAwarenessContext(IntPtr c);
}
'@
[void][BenchNative]::SetThreadDpiAwarenessContext([IntPtr]::new(-4))
$exe=Join-Path $root 'artifacts\shell-panels\Release\ShellBench.exe'
$arguments=if($Dark){'--dark'}else{'--light'}
$process=Start-Process -FilePath $exe -ArgumentList $arguments -WorkingDirectory $out -WindowStyle Hidden -PassThru
try{
 Start-Sleep -Milliseconds 1600
 $hostHandle=[BenchNative]::FindWindow('Yata.ShellBench.Host','ShellPanels test host');$backHandle=[BenchNative]::FindWindow('Yata.ShellBench.Background','C++ ShellPanels test bench')
 if($hostHandle-eq[IntPtr]::Zero){throw 'Test host did not initialize'}
 foreach($kind in 0..2){
  [void][BenchNative]::SetForegroundWindow($backHandle)
  [void][BenchNative]::PostMessage($hostHandle,0x8001,[IntPtr]::new($kind),[IntPtr]::Zero)
  Start-Sleep -Milliseconds 1200
  $phase=[BenchNative]::SendMessage($hostHandle,0x8004,[IntPtr]::Zero,[IntPtr]::Zero)
  Write-Output "kind=$kind phase=$phase"
  if($phase.ToInt64()-ne2){throw "Panel $kind did not reach Open"}
  $bitmap=[Drawing.Bitmap]::new(1120,1600);$g=[Drawing.Graphics]::FromImage($bitmap)
  try{$g.CopyFromScreen(1440,0,0,0,$bitmap.Size);$bitmap.Save((Join-Path $out ((@('lang','quick','date')[$kind])+'-'+$arguments.Substring(2)+'.png')))}finally{$g.Dispose();$bitmap.Dispose()}
  [void][BenchNative]::PostMessage($hostHandle,0x8003,[IntPtr]::Zero,[IntPtr]::Zero);Start-Sleep -Milliseconds 400
 }
}finally{
 if(!$KeepOpen-and!$process.HasExited){[void][BenchNative]::PostMessage($backHandle,0x10,[IntPtr]::Zero,[IntPtr]::Zero);[void]$process.WaitForExit(5000)}
}
