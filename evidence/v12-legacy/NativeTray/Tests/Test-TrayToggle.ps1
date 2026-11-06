$ErrorActionPreference='Stop'
$ProgressPreference='SilentlyContinue'
Add-Type -AssemblyName UIAutomationClient,UIAutomationTypes
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class TrayToggleTest {
 [DllImport("user32.dll",CharSet=CharSet.Unicode)] public static extern IntPtr FindWindow(string cls,string name);
}
'@
$map=[IO.MemoryMappedFiles.MemoryMappedFile]::OpenExisting('Local\YataNativeFramework.v13');$view=$map.CreateViewAccessor()
$checks=[Collections.Generic.List[string]]::new()
function Assert([bool]$ok,[string]$message){if(!$ok){throw $message};$checks.Add("PASS $message")}
function Invoke-Tray {
 $bar=[Windows.Automation.AutomationElement]::FromHandle([TrayToggleTest]::FindWindow('Shell_TrayWnd',$null))
 $all=$bar.FindAll([Windows.Automation.TreeScope]::Descendants,[Windows.Automation.Condition]::TrueCondition)
 foreach($element in $all){
  if($element.Current.AutomationId -eq 'NotifyItemIcon' -and $element.Current.Name -like 'Yata Native Framework*'){
   $element.GetCurrentPattern([Windows.Automation.InvokePattern]::Pattern).Invoke();return
  }
 }
 throw 'Tray target was not found'
}
try {
 Assert ($view.ReadInt32(20) -eq 2) 'speed decoration is applied in live Explorer'
 Assert ($view.ReadInt32(56) -eq 0) 'panel begins closed'
 foreach($i in 1..10){
  Invoke-Tray;Start-Sleep -Milliseconds 450
  Assert ($view.ReadInt32(56) -eq 1) "native tray Invoke opens panel cycle $i"
  Invoke-Tray;Start-Sleep -Milliseconds 450
  Assert ($view.ReadInt32(56) -eq 0) "native tray Invoke closes panel cycle $i"
 }
 $checks.Add('ALL NATIVE TRAY TOGGLE CHECKS PASSED')
}catch{$checks.Add("FAIL $($_.Exception.Message)");throw}
finally{$checks | Set-Content -LiteralPath "$PSScriptRoot\tray-toggle-v6-results.txt" -Encoding UTF8;$view.Dispose();$map.Dispose()}
