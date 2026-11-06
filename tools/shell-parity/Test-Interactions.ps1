param([string]$BinaryPath)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$out=Join-Path $root 'evidence\shell-parity-26100'
Add-Type -AssemblyName UIAutomationClient,UIAutomationTypes,System.Drawing,System.Windows.Forms
foreach($file in @('Test-Bench.ps1','Capture-Reference.ps1')){
 $type=if($file-eq'Test-Bench.ps1'){'BenchNative'}else{'RefNative'}
 if(!($type-as[type])){$source=[IO.File]::ReadAllText((Join-Path $PSScriptRoot $file));$begin=$source.IndexOf('using System;');$end=$source.IndexOf("'@",$begin);Add-Type $source.Substring($begin,$end-$begin)}
}
[void][BenchNative]::SetThreadDpiAwarenessContext([IntPtr]::new(-4))
if(!$BinaryPath){$BinaryPath=Join-Path $root 'artifacts\shell-panels\Release\ShellBench.exe'}
$bench=Start-Process -FilePath $BinaryPath -WorkingDirectory $out -WindowStyle Hidden -PassThru
$results=[Collections.Generic.List[string]]::new()
function Query([int]$field){[BenchNative]::SendMessage($hostHandle,0x8006,[IntPtr]::new($field),[IntPtr]::Zero).ToInt64()}
function Check([bool]$ok,[string]$name){if(!$ok){throw "FAIL $name"};$results.Add("PASS $name")}
function Find([string]$name){$window=[Windows.Automation.AutomationElement]::FromHandle($hostHandle);$condition=[Windows.Automation.PropertyCondition]::new([Windows.Automation.AutomationElement]::NameProperty,$name);$window.FindFirst([Windows.Automation.TreeScope]::Descendants,$condition)}
try{
 Start-Sleep -Milliseconds 1600
 $hostHandle=[BenchNative]::FindWindow('Yata.ShellBench.Host','ShellPanels test host');$backHandle=[BenchNative]::FindWindow('Yata.ShellBench.Background','C++ ShellPanels test bench')
 Check ($hostHandle-ne[IntPtr]::Zero) 'independent test host initialized'
 [void][BenchNative]::SetWindowPos($backHandle,[IntPtr]::new(-1),0,0,0,0,0x53)
 [RefNative]::Click(1600,700,$false)
 [void][BenchNative]::SetWindowPos($backHandle,[IntPtr]::new(-2),0,0,0,0,0x13)
 Check ([RefNative]::GetForegroundWindow()-eq$backHandle) 'test background owns the input target'
 [void][BenchNative]::PostMessage($hostHandle,0x8001,[IntPtr]::new(1),[IntPtr]::Zero);Start-Sleep -Milliseconds 700
 $volume=Find 'Volume';Check ($null-ne$volume) 'volume exposes a UI Automation element'
 $range=$volume.GetCurrentPattern([Windows.Automation.RangeValuePattern]::Pattern)
 Check ($range.Current.Value-eq27) 'initial volume from model reaches the native slider'
 [void][BenchNative]::SendMessage($hostHandle,0x8007,[IntPtr]::new(72),[IntPtr]::Zero);Start-Sleep -Milliseconds 120
 Check ($range.Current.Value-eq72) 'live model update reaches the visible slider'
 Check ((Query 0)-eq0) 'live model update produces no feedback command'
 $range.SetValue(43);Start-Sleep -Milliseconds 120
 Check ((Query 1)-eq43-and(Query 0)-eq1) 'UIA slider input reaches the application exactly once'
 $tile=Find 'Wi-Fi';Check ($null-ne$tile) 'network fixture tile exposes a UI Automation element'
 $rect=$tile.Current.BoundingRectangle
 $results.Add("TRACE tile=$($tile.Current.ControlType.ProgrammaticName) bounds=$rect foreground=$([RefNative]::GetForegroundWindow()) phase=$([BenchNative]::SendMessage($hostHandle,0x8004,[IntPtr]::Zero,[IntPtr]::Zero))")
 $at=[Windows.Automation.AutomationElement]::FromPoint([Windows.Point]::new($rect.X+$rect.Width/2,$rect.Y+$rect.Height/2));$results.Add("TRACE hit=$($at.Current.Name) process=$($at.Current.ProcessId) own=$($bench.Id)")
 Check ($at.Current.ProcessId-eq$bench.Id) 'physical input target belongs to the test consumer'
 $bitmap=[Drawing.Bitmap]::new(1120,1600);$graphics=[Drawing.Graphics]::FromImage($bitmap);try{$graphics.CopyFromScreen(1440,0,0,0,$bitmap.Size);$bitmap.Save((Join-Path $out 'interaction-click.png'))}finally{$graphics.Dispose();$bitmap.Dispose()}
 [RefNative]::Click([int]($rect.X+$rect.Width/2),[int]($rect.Y+$rect.Height/2),$false);Start-Sleep -Milliseconds 150
 $results.Add("TRACE after click wifi=$(Query 2) actions=$(Query 0)")
 Check ((Query 2)-eq0-and(Query 0)-eq2) 'physical click updates only the fixture model exactly once'
 [RefNative]::Escape();Start-Sleep -Milliseconds 400
 Check ([BenchNative]::SendMessage($hostHandle,0x8004,[IntPtr]::Zero,[IntPtr]::Zero).ToInt64()-eq0) 'Escape closes the panel'
 foreach($part in 1..4){
  # Dedicated message accepts network and battery; normal message accepts volume/date.
  $message=if($part-eq1-or$part-eq3){0x8005}else{0x8002};$value=if($message-eq0x8005){$part}elseif($part-eq2){1}else{2}
  [void][BenchNative]::PostMessage($hostHandle,$message,[IntPtr]::new($value),[IntPtr]::Zero);Start-Sleep -Milliseconds 400
  Check ((Query 3)-eq1) "context menu $part opens"
  [RefNative]::Escape();Start-Sleep -Milliseconds 400
  Check ((Query 3)-eq0) "context menu $part closes with Escape"
 }
 [void][BenchNative]::PostMessage($hostHandle,0x8001,[IntPtr]::new(2),[IntPtr]::Zero);Start-Sleep -Milliseconds 700
 $window=[Windows.Automation.AutomationElement]::FromHandle($hostHandle)
 $calendar=$window.FindFirst([Windows.Automation.TreeScope]::Descendants,[Windows.Automation.PropertyCondition]::new([Windows.Automation.AutomationElement]::AutomationIdProperty,'CalendarControl'))
 Check ($null-ne$calendar) 'Date exposes the native CalendarView'
 $pattern=$null
 Check ($calendar.TryGetCurrentPattern([Windows.Automation.TablePattern]::Pattern,[ref]$pattern)) 'Calendar retains the native Table pattern'
 Check ($calendar.TryGetCurrentPattern([Windows.Automation.SelectionPattern]::Pattern,[ref]$pattern)) 'Calendar retains the native Selection pattern'
 $day=$calendar.FindAll([Windows.Automation.TreeScope]::Descendants,[Windows.Automation.PropertyCondition]::new([Windows.Automation.AutomationElement]::ControlTypeProperty,[Windows.Automation.ControlType]::DataItem))|Where-Object {!$_.Current.IsOffscreen}|Select-Object -First 1
 Check ($null-ne$day) 'native calendar exposes realized date items'
 $before=Query 0
 $day.GetCurrentPattern([Windows.Automation.SelectionItemPattern]::Pattern).Select();Start-Sleep -Milliseconds 120
 Check ((Query 0)-eq$before+1) 'native date selection emits exactly one callback'
 $collapse=Find 'Collapse calendar';$collapse.GetCurrentPattern([Windows.Automation.InvokePattern]::Pattern).Invoke();Start-Sleep -Milliseconds 450
 Check ($null-ne(Find 'Expand calendar')) 'calendar collapses with the native date control installed'
 [RefNative]::Escape();Start-Sleep -Milliseconds 400
 Check ([BenchNative]::SendMessage($hostHandle,0x8004,[IntPtr]::Zero,[IntPtr]::Zero).ToInt64()-eq0) 'Escape closes Date after native calendar interaction'
}catch{$results.Add($_.ToString());throw}finally{
 $results|Set-Content -LiteralPath (Join-Path $out 'interactions.log')
 $results|Write-Output
 if(!$bench.HasExited){[void][BenchNative]::PostMessage($backHandle,0x10,[IntPtr]::Zero,[IntPtr]::Zero);if(!$bench.WaitForExit(5000)){$bench.Kill()}}
}
