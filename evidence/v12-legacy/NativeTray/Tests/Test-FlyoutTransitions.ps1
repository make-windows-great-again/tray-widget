param([string]$Label='transitions-v7')
$ErrorActionPreference='Stop'
$ProgressPreference='SilentlyContinue'
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class FlyoutTransitionsTest {
 [DllImport("user32.dll",CharSet=CharSet.Unicode)] public static extern uint RegisterWindowMessage(string name);
 [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr window,uint message,IntPtr w,IntPtr l);
}
'@
$map=[IO.MemoryMappedFiles.MemoryMappedFile]::OpenExisting('Local\YataNativeFramework.v13');$view=$map.CreateViewAccessor()
$window=[IntPtr]::new($view.ReadInt64(8));$toggle=[FlyoutTransitionsTest]::RegisterWindowMessage('YataVpn.Tray.Notify.v4')
$checks=[Collections.Generic.List[string]]::new();$samples=[Collections.Generic.List[object]]::new()
function Assert([bool]$ok,[string]$text){if(!$ok){throw $text};$checks.Add("PASS $text")}
function Toggle{[void][FlyoutTransitionsTest]::PostMessage($window,$toggle,[IntPtr]::Zero,[IntPtr]::new(0x400))}
function Phase{return $view.ReadInt32(60)}
function Wait-Phase([int]$wanted){
 $limit=[Diagnostics.Stopwatch]::StartNew()
 while((Phase) -ne $wanted){if($limit.ElapsedMilliseconds -gt 1500){throw "Phase $wanted was not reached; current=$(Phase)"};Start-Sleep -Milliseconds 2}
}
try{
 Assert ((Phase) -eq 0) 'panel begins closed'
 foreach($cycle in 1..3){
  Toggle;Wait-Phase 1
  $clock=[Diagnostics.Stopwatch]::StartNew()
  while($clock.ElapsedMilliseconds -lt 150){
   Toggle
   $samples.Add([pscustomobject]@{Cycle=$cycle;Step='opening';Ms=$clock.Elapsed.TotalMilliseconds;Phase=(Phase);Visible=$view.ReadInt32(56)})
   Assert ((Phase) -eq 1) "opening is not interrupted at $([int]$clock.ElapsedMilliseconds) ms (cycle $cycle)"
   Start-Sleep -Milliseconds 12
  }
  Wait-Phase 2
  Assert ($clock.ElapsedMilliseconds -ge 190) "opening finishes through composition completion (cycle $cycle)"
  Assert ($view.ReadInt32(56) -eq 1) "panel remains open after the opening click burst (cycle $cycle)"
  Toggle;Wait-Phase 3
  $clock.Restart()
  while($clock.ElapsedMilliseconds -lt 100){
   Toggle
   $samples.Add([pscustomobject]@{Cycle=$cycle;Step='closing';Ms=$clock.Elapsed.TotalMilliseconds;Phase=(Phase);Visible=$view.ReadInt32(56)})
   Assert ((Phase) -eq 3 -and $view.ReadInt32(56) -eq 1) "closing keeps selected state and ignores toggles at $([int]$clock.ElapsedMilliseconds) ms (cycle $cycle)"
   Start-Sleep -Milliseconds 12
  }
  Wait-Phase 0
  Assert ($clock.ElapsedMilliseconds -ge 120) "closing finishes before a new toggle is allowed (cycle $cycle)"
  Assert ($view.ReadInt32(56) -eq 0) "selected state clears after complete closing (cycle $cycle)"
 }
 $checks.Add('ALL TRANSITION AND SPAM CHECKS PASSED')
}catch{$checks.Add("FAIL $($_.Exception.Message)");throw}
finally{
 $checks | Set-Content -LiteralPath "$PSScriptRoot\$Label-results.txt" -Encoding UTF8
 $samples | ConvertTo-Json | Set-Content -LiteralPath "$PSScriptRoot\$Label-samples.json" -Encoding UTF8
 $view.Dispose();$map.Dispose()
}
