$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue'
. "$PSScriptRoot\Audit-HoverFonts.ps1" -DefinitionsOnly
$map=[IO.MemoryMappedFiles.MemoryMappedFile]::OpenExisting('Local\YataNativeFramework.v13');$view=$map.CreateViewAccessor()
$hostTool='C:\Users\Yata\AppData\Local\Temp\YataVpnNativeTray\YataNativeFramework.exe'
$barBounds=$bar.Current.BoundingRectangle;$explorerId=$bar.Current.ProcessId
$originalMode=$view.ReadInt32(16)
$checks=[Collections.Generic.List[string]]::new();$samples=[Collections.Generic.List[object]]::new()
function Assert([bool]$ok,[string]$message){if(!$ok){throw $message};$checks.Add("PASS $message")}
function Find-Tooltip{
 foreach($w in [HoverAudit]::Popups()){
  [uint32]$process=0;[void][HoverAudit]::GetWindowThreadProcessId($w,[ref]$process)
  if($process -ne $explorerId -or [HoverAudit]::Class($w) -ne 'Xaml_WindowedPopupClass'){continue}
  $root=[Windows.Automation.AutomationElement]::FromHandle($w)
  $all=$root.FindAll([Windows.Automation.TreeScope]::Descendants,[Windows.Automation.Condition]::TrueCondition)
  foreach($e in $all){
   if($e.Current.AutomationId -eq 'YataTrayTooltip' -or $e.Current.Name -like 'Yata Native Framework*'){
    $r=New-Object HoverAudit+Rect;[void][HoverAudit]::GetWindowRect($w,[ref]$r)
    return [pscustomobject]@{Window=$w;Left=$r.left;Top=$r.top;Right=$r.right;Bottom=$r.bottom}
   }
  }
 }
 return $null
}
try{
 foreach($mode in 1,2){
  & $hostTool --mode @('off','vpn','speed')[$mode] | Out-Null
  Start-Sleep -Milliseconds 500
  $elements=$bar.FindAll([Windows.Automation.TreeScope]::Descendants,[Windows.Automation.Condition]::TrueCondition)
  $target=$elements | Where-Object {$_.Current.AutomationId -eq 'NotifyItemIcon' -and $_.Current.Name -like 'Yata Native Framework*'} | Select-Object -First 1
  $physical=[HoverAudit]::NativeIconBounds()
  $r=[Windows.Rect]::new($physical.left,$physical.top,$physical.right-$physical.left,$physical.bottom-$physical.top)
  $width=$view.ReadDouble(40);$first=$null
  foreach($row in 'bottom','top'){foreach($column in 'left','center','right'){
   Move-Pointer ([int]$r.X-220) ([int]$r.Y-250);Start-Sleep -Milliseconds 850
   $x=switch($column){'left'{$r.X+3};'center'{$r.X+$r.Width/2};'right'{$r.Right-3}}
   $y=if($row -eq 'top'){$r.Y+3}else{$r.Bottom-3}
   Move-Pointer ([int]$x) ([int]$y);Start-Sleep -Milliseconds 950
   $cursor=New-Object HoverAudit+Point;[void][HoverAudit]::GetPhysicalCursorPos([ref]$cursor)
   if([Math]::Abs($cursor.x-$x) -gt 3 -or [Math]::Abs($cursor.y-$y) -gt 3){
    Move-Pointer ([int]$r.X-220) ([int]$r.Y-250);Start-Sleep -Milliseconds 850
    Move-Pointer ([int]$x) ([int]$y);Start-Sleep -Milliseconds 950
    [void][HoverAudit]::GetPhysicalCursorPos([ref]$cursor)
   }
   Assert ([Math]::Abs($cursor.x-$x) -le 3 -and [Math]::Abs($cursor.y-$y) -le 3) "cursor stays in the sampled $row-$column point (mode $mode): wanted=$x,$y actual=$($cursor.x),$($cursor.y)"
   $tip=Find-Tooltip
   Assert ($null -ne $tip) "tooltip is visible from $row-$column (mode $mode)"
   Assert ($tip.Bottom -le $barBounds.Y) "tooltip stays wholly above the taskbar from $row-$column (mode $mode)"
   if($first){Assert ([Math]::Abs($tip.Left-$first.Left) -le 1 -and [Math]::Abs($tip.Top-$first.Top) -le 1) "tooltip position is pointer independent at $row-$column (mode $mode)"}else{$first=$tip}
   Assert ($view.ReadInt32(20) -eq $mode -and [Math]::Abs($view.ReadDouble(40)-$width) -lt 0.1) "tray width remains stable at $row-$column (mode $mode)"
   $samples.Add([pscustomobject]@{Mode=$mode;Point="$row-$column";CursorX=$cursor.x;CursorY=$cursor.y;Left=$tip.Left;Top=$tip.Top;Right=$tip.Right;Bottom=$tip.Bottom;WidthDIP=$width})
  }}
 }
 Move-Pointer 1500 900;Start-Sleep -Milliseconds 350
 Assert ($null -eq (Find-Tooltip)) 'tooltip dismisses after pointer leaves the tray'
 $checks.Add('ALL HOVER POSITION AND STABLE WIDTH CHECKS PASSED')
}catch{$checks.Add("FAIL $($_.Exception.Message)");$checks.Add($_.ScriptStackTrace);throw}
finally{
 & $hostTool --mode @('off','vpn','speed')[$originalMode] | Out-Null
 $checks|Set-Content -LiteralPath "$PSScriptRoot\hover-matrix-v12-results.txt" -Encoding UTF8
 $samples|ConvertTo-Json|Set-Content -LiteralPath "$PSScriptRoot\hover-matrix-v12-samples.json" -Encoding UTF8
 $view.Dispose();$map.Dispose()
}
