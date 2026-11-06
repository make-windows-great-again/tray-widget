[CmdletBinding()]
param([string]$OutputDirectory=(Join-Path $PSScriptRoot '..\..\evidence\exit-material-20261105\live'))
$ErrorActionPreference='Stop'
[IO.Directory]::CreateDirectory([IO.Path]::GetFullPath($OutputDirectory))|Out-Null
Add-Type -AssemblyName UIAutomationClient,UIAutomationTypes
Add-Type -TypeDefinition @'
using System;using System.Runtime.InteropServices;
public static class ExitMaterialNative {
 [StructLayout(LayoutKind.Sequential)]public struct Point{public int x,y;}
 [DllImport("user32.dll")]public static extern bool GetPhysicalCursorPos(out Point p);
 [DllImport("user32.dll")]public static extern bool SetCursorPos(int x,int y);
 [DllImport("user32.dll")]public static extern bool SetForegroundWindow(IntPtr w);
 [DllImport("user32.dll")]public static extern IntPtr GetForegroundWindow();
 [DllImport("user32.dll")]public static extern uint GetWindowThreadProcessId(IntPtr w,out uint pid);
 [DllImport("user32.dll")]public static extern IntPtr SetThreadDpiAwarenessContext(IntPtr c);
 [DllImport("user32.dll",CharSet=CharSet.Unicode)]public static extern IntPtr FindWindow(string c,string t);
 [DllImport("user32.dll")]static extern void mouse_event(uint flags,uint x,uint y,uint data,UIntPtr extra);
 [DllImport("user32.dll")]static extern void keybd_event(byte key,byte scan,uint flags,UIntPtr extra);
 public static void Click(int x,int y){SetCursorPos(x,y);mouse_event(2,0,0,0,UIntPtr.Zero);mouse_event(4,0,0,0,UIntPtr.Zero);}
 public static void Escape(){keybd_event(27,0,0,UIntPtr.Zero);keybd_event(27,0,2,UIntPtr.Zero);}
}
'@
$oldDpi=[ExitMaterialNative]::SetThreadDpiAwarenessContext([IntPtr]::new(-4))
$cursor=New-Object ExitMaterialNative+Point;[void][ExitMaterialNative]::GetPhysicalCursorPos([ref]$cursor)
$mapping=[IO.MemoryMappedFiles.MemoryMappedFile]::OpenExisting('Local\YataNativeFramework.v13')
$view=$mapping.CreateViewAccessor(0,0,[IO.MemoryMappedFiles.MemoryMappedFileAccess]::Read)
$appId=$view.ReadInt32(4);$window=[IntPtr]::new($view.ReadInt64(8))
$app=(Get-Process -Id $appId).Path
$results=[Collections.Generic.List[object]]::new()
function Require-Tray {
 if($view.ReadInt32(20)-ne2-or$view.ReadInt32(28)-ne0){throw 'A healthy attached native tray in lines mode is required'}
}
function Wait-Phase([int]$expected,[string]$label){
 $watch=[Diagnostics.Stopwatch]::StartNew()
 do{
  $phase=$view.ReadInt32(60)
  if($phase-eq$expected){$results.Add([pscustomobject]@{Case=$label;Phase=$phase;ObservedAfterMs=$watch.Elapsed.TotalMilliseconds});return}
  Start-Sleep -Milliseconds 5
 }while($watch.ElapsedMilliseconds-lt2000)
 throw "$label : expected phase $expected, got $phase"
}
function Element([string]$id){
 $root=[Windows.Automation.AutomationElement]::FromHandle($window)
 $condition=[Windows.Automation.PropertyCondition]::new([Windows.Automation.AutomationElement]::AutomationIdProperty,$id)
 $element=$root.FindFirst([Windows.Automation.TreeScope]::Descendants,$condition)
 if(!$element){throw "Element missing: $id"};return $element
}
function Open-Panel {
 $info=[Diagnostics.ProcessStartInfo]::new($app,'--show');$info.UseShellExecute=$false;$info.CreateNoWindow=$true
 $p=[Diagnostics.Process]::Start($info);try{if(!$p.WaitForExit(5000)){throw 'Show timed out'}}finally{$p.Dispose()}
 Wait-Phase 2 'panel-open'
 Start-Sleep -Milliseconds 150
 $r=(Element 'ClosePanel').Current.BoundingRectangle
 [ExitMaterialNative]::Click([int]($r.X-120),[int]($r.Y+$r.Height/2));Start-Sleep -Milliseconds 50
 [uint32]$foregroundId=0
 [void][ExitMaterialNative]::GetWindowThreadProcessId([ExitMaterialNative]::GetForegroundWindow(),[ref]$foregroundId)
 if($foregroundId-ne$appId){throw 'Panel did not receive foreground activation'}
}
function Click-Tray {
 Require-Tray
 $offset=$view.ReadInt32(64)-16
 $left=$view.ReadInt32($offset);$top=$view.ReadInt32($offset+4);$right=$view.ReadInt32($offset+8);$bottom=$view.ReadInt32($offset+12)
 if($right-le$left-or$bottom-le$top){throw 'Native tray bounds are unavailable'}
 [ExitMaterialNative]::Click([int](($left+$right)/2),[int](($top+$bottom)/2))
}
try{
 Require-Tray
 Open-Panel
 $r=(Element 'ClosePanel').Current.BoundingRectangle
 [ExitMaterialNative]::Click([int]($r.X+$r.Width/2),[int]($r.Y+$r.Height/2))
 Wait-Phase 3 'close-button-animated';Wait-Phase 0 'close-button-complete'
 Open-Panel;[ExitMaterialNative]::Escape();Wait-Phase 3 'escape-animated';Wait-Phase 0 'escape-complete'
 Open-Panel
 if(![ExitMaterialNative]::SetForegroundWindow([ExitMaterialNative]::FindWindow('Shell_TrayWnd',$null))){throw 'Could not activate Explorer for deactivation check'}
 Wait-Phase 0 'external-deactivation-complete'
 Open-Panel;Click-Tray;Wait-Phase 0 'tray-toggle-closes'
 Start-Sleep -Milliseconds 350
 if($view.ReadInt32(60)-ne0){throw 'Tray release reopened the dismissed panel'}
 Click-Tray;Wait-Phase 2 'next-tray-toggle-opens'
 $results|ConvertTo-Json|Set-Content -LiteralPath (Join-Path $OutputDirectory 'checks.json') -Encoding UTF8
 Write-Output "Live checks passed: $($results.Count) observations"
}finally{
 $results|ConvertTo-Json|Set-Content -LiteralPath (Join-Path $OutputDirectory 'observations.json') -Encoding UTF8
 $view.Dispose();$mapping.Dispose();[void][ExitMaterialNative]::SetCursorPos($cursor.x,$cursor.y)
 [void][ExitMaterialNative]::SetThreadDpiAwarenessContext($oldDpi)
}
