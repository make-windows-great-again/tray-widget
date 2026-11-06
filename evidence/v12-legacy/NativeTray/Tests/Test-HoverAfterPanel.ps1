$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue'
. "$PSScriptRoot\Audit-HoverFonts.ps1" -DefinitionsOnly
Add-Type -TypeDefinition @'
using System;using System.Runtime.InteropServices;
public static class HoverAfterPanel {
 [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
}
'@
$map=[IO.MemoryMappedFiles.MemoryMappedFile]::OpenExisting('Local\YataNativeFramework.v13');$view=$map.CreateViewAccessor()
try{
 $window=[IntPtr]::new($view.ReadInt64(8))
 if($view.ReadInt32(56)){
  $root=[Windows.Automation.AutomationElement]::FromHandle($window)
  $close=$root.FindFirst([Windows.Automation.TreeScope]::Descendants,[Windows.Automation.PropertyCondition]::new([Windows.Automation.AutomationElement]::AutomationIdProperty,'ClosePanel'))
  $close.GetCurrentPattern([Windows.Automation.InvokePattern]::Pattern).Invoke()
  Start-Sleep -Milliseconds 500
 }
 $physical=[HoverAudit]::NativeIconBounds()
 $r=[Windows.Rect]::new($physical.left,$physical.top,$physical.right-$physical.left,$physical.bottom-$physical.top)
 Move-Pointer ([int]$r.X-200) ([int]$r.Y-200);Start-Sleep -Milliseconds 900
 Move-Pointer ([int]($r.X+$r.Width/2)) ([int]$r.Bottom-3);Start-Sleep -Milliseconds 1100
 [uint32]$foregroundPid=0;[void][HoverAudit]::GetWindowThreadProcessId([HoverAfterPanel]::GetForegroundWindow(),[ref]$foregroundPid)
 $cursor=New-Object HoverAudit+Point;[void][HoverAudit]::GetPhysicalCursorPos([ref]$cursor)
 [pscustomobject]@{ForegroundPid=$foregroundPid;AppPid=$view.ReadInt32(4);Mode=$view.ReadInt32(16);Applied=$view.ReadInt32(20);Phase=$view.ReadInt32(60);Panel=$view.ReadInt32(56);CursorX=$cursor.x;CursorY=$cursor.y;Anchor=$r.ToString()} | ConvertTo-Json | Set-Content -LiteralPath "$PSScriptRoot\hover-after-panel-state.json"
 $Label='hover-after-panel-v11';Sample 'vpn' 'bottom'
 $records|ConvertTo-Json -Depth 8|Set-Content -LiteralPath "$PSScriptRoot\$Label.json" -Encoding UTF8
 Move-Pointer 1500 900
}finally{$view.Dispose();$map.Dispose()}
