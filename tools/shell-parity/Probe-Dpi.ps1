param([int[]]$Scales=@(150),[string[]]$Scenes=@('date'),[string[]]$Targets=@('native','cpp'),[string]$Label='baseline',[string]$BinaryPath,[switch]$Collapsed,[switch]$Contexts,[ValidateSet('light','dark','custom')][string]$Theme='light')
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$out=Join-Path $root ('evidence\shell-parity-26100\dpi-v16\'+$Label)
[IO.Directory]::CreateDirectory($out)|Out-Null
. (Join-Path $PSScriptRoot 'DpiHarness.ps1')
[void][DpiDesktop]::SetThreadDpiAwarenessContext([IntPtr]::new(-4))
if(!('RefNative'-as[type])){$source=[IO.File]::ReadAllText((Join-Path $PSScriptRoot 'Capture-Reference.ps1'));$begin=$source.IndexOf('using System;');$end=$source.IndexOf("'@",$begin);Add-Type $source.Substring($begin,$end-$begin)}
if(!$BinaryPath){$BinaryPath=Join-Path $root 'artifacts\dpi-trace-v16\ShellBench.exe'}
if([DpiDesktop]::FindWindow('Yata.ShellBench.Host','ShellPanels test host')-ne[IntPtr]::Zero){throw 'A bench is already running'}
$initialScale=Get-DisplayScale
$bench=$null;$records=[Collections.Generic.List[object]]::new()
function Prepare {
 $display=[DpiDesktop]::Display()
 [DpiDesktop]::Position($back,[IntPtr]::new(-1),$display.Monitor.Right-1120,$display.Work.Top,1120,$display.Work.Bottom-$display.Work.Top,0x40)
 [RefNative]::Click($display.Monitor.Right-960,200,$false)
 [DpiDesktop]::Position($back,[IntPtr]::new(-2),0,0,0,0,0x13)
 Start-Sleep -Milliseconds 200
 if([DpiDesktop]::GetForegroundWindow()-ne$back){
  [pscustomobject]@{Foreground=[DpiDesktop]::GetForegroundWindow().ToInt64();Expected=$back.ToInt64();Dpi=[DpiDesktop]::GetDpiForWindow($back);PointRoot=[RefNative]::RootAtPoint($display.Monitor.Right-960,200);Windows=@([RefNative]::Windows())}|ConvertTo-Json -Depth 6|Set-Content (Join-Path $out 'foreground-failure.json')
  throw 'Neutral background does not own foreground'
 }
}
function TrayButton([string]$scene) {
 $pattern=@{date='Clock*';quick='Volume*';lang='Tray Input Indicator*'}[$scene]
 $tray=[Windows.Automation.AutomationElement]::FromHandle([DpiDesktop]::Tray())
 $button=$tray.FindAll([Windows.Automation.TreeScope]::Descendants,[Windows.Automation.Condition]::TrueCondition)|Where-Object {$_.Current.Name-like$pattern-and!$_.Current.IsOffscreen}|Select-Object -First 1
 if(!$button){throw "Tray target missing: $scene"}
 return $button.Current.BoundingRectangle
}
function NativeRoot([string]$scene) {
 if($scene-eq'lang'){
  # The input switcher uses a full-desktop nonactivating TextInputHost surface.
  # Do not traverse unrelated CoreWindow providers to guess its UIA root.
  return $null
 }
 $until=[DateTime]::UtcNow.AddSeconds(2)
 do{$handle=[DpiDesktop]::GetForegroundWindow();if($handle-ne[IntPtr]::Zero-and$handle-ne$back){break};Start-Sleep -Milliseconds 100}while([DateTime]::UtcNow-lt$until)
 if($handle-eq$back-or$handle-eq$hostWindow){throw 'Native panel did not take foreground'}
 return [Windows.Automation.AutomationElement]::FromHandle($handle)
}
function Save([string]$tag,$window,[bool]$cpp,$popupBounds=$null) {
 $elements=if($window){$window.FindAll([Windows.Automation.TreeScope]::Descendants,[Windows.Automation.Condition]::TrueCondition)}else{@()}
 $rows=foreach($element in $elements){
  $item=$element.Current;$bounds=$item.BoundingRectangle
  $font=$null;$size=$null;$weight=$null
  if($item.ControlType-eq[Windows.Automation.ControlType]::Text){try{$range=$element.GetCurrentPattern([Windows.Automation.TextPattern]::Pattern).DocumentRange;$value=$range.GetAttributeValue([Windows.Automation.TextPattern]::FontNameAttribute);if($value-is[string]){$font=$value};$value=$range.GetAttributeValue([Windows.Automation.TextPattern]::FontSizeAttribute);if($value-is[double]){$size=$value};$value=$range.GetAttributeValue([Windows.Automation.TextPattern]::FontWeightAttribute);if($value-is[int]){$weight=$value}}catch{}}
  [pscustomobject]@{Name=$item.Name;Id=$item.AutomationId;Type=$item.ControlType.ProgrammaticName;Class=$item.ClassName;X=$bounds.X;Y=$bounds.Y;W=$bounds.Width;H=$bounds.Height;Offscreen=$item.IsOffscreen;Font=$font;FontSize=$size;FontWeight=$weight}
 }
 $rows|ConvertTo-Json -Depth 4|Set-Content (Join-Path $out ($tag+'-uia.json'))
 $display=[DpiDesktop]::Display()
 $bitmap=[Drawing.Bitmap]::new(1120,$display.Monitor.Bottom-$display.Monitor.Top);$graphics=[Drawing.Graphics]::FromImage($bitmap)
 try{$graphics.CopyFromScreen($display.Monitor.Right-1120,$display.Monitor.Top,0,0,$bitmap.Size);$pixel=$bitmap.GetPixel(50,100);if([Math]::Abs($pixel.R-96)-gt2){throw 'Background obscured; snapshot rejected'};$bitmap.Save((Join-Path $out ($tag+'.png')))}finally{$graphics.Dispose();$bitmap.Dispose()}
 if($cpp){[void][DpiDesktop]::PostMessage($hostWindow,[DpiDesktop]::RegisterWindowMessage('Yata.ShellPanels.TraceLayout.v1'),[IntPtr]::Zero,[IntPtr]::Zero);Start-Sleep -Milliseconds 100;$trace=Join-Path $out 'layout-trace.csv';if(Test-Path $trace){Copy-Item $trace (Join-Path $out ($tag+'-layout.csv')) -Force}}
 $records.Add([pscustomobject]@{Tag=$tag;PopupPhysicalBounds=$popupBounds;Scale=[DpiDesktop]::GetDpiForWindow([DpiDesktop]::Tray())/96.0;HostDpi=[DpiDesktop]::GetDpiForWindow($hostWindow);Work=$display.Work;WindowClass=$(if($window){$window.Current.ClassName}else{'capture only'});WindowBounds=$(if($window){$window.Current.BoundingRectangle.ToString()}else{''})})
}
function CalendarClick($window,[string]$verb) {
 $button=$window.FindAll([Windows.Automation.TreeScope]::Descendants,[Windows.Automation.PropertyCondition]::new([Windows.Automation.AutomationElement]::ControlTypeProperty,[Windows.Automation.ControlType]::Button))|Where-Object {$_.Current.Name-like($verb+'*calendar')-and!$_.Current.IsOffscreen}|Select-Object -First 1
 if(!$button){throw "Calendar action unavailable: $verb"}
 $rect=$button.Current.BoundingRectangle
 [RefNative]::Click([int]($rect.X+$rect.Width/2),[int]($rect.Y+$rect.Height/2),$false)
 Start-Sleep -Milliseconds 500
}
try {
 $bench=Start-Process -FilePath $BinaryPath -ArgumentList ('--'+$Theme) -WorkingDirectory $out -WindowStyle Hidden -PassThru
 Start-Sleep -Milliseconds 1500
 $hostWindow=[DpiDesktop]::FindWindow('Yata.ShellBench.Host','ShellPanels test host');$back=[DpiDesktop]::FindWindow('Yata.ShellBench.Background','C++ ShellPanels test bench')
 if($hostWindow-eq[IntPtr]::Zero){throw 'Bench initialization failed'}
 foreach($percent in $Scales){
  Set-DisplayScale $percent
  foreach($scene in $Scenes){foreach($target in $Targets){
   $cpp=$target-eq'cpp';$kind=@{lang=0;quick=1;date=2}[$scene]
   Prepare
   $bounds=TrayButton $scene;$x=[int]($bounds.X+$bounds.Width/2);$y=[int]($bounds.Y+$bounds.Height/2)
   [void][DpiDesktop]::SendMessage($hostWindow,0x800b,[IntPtr]::new($x),[IntPtr]::new($y))
   if($cpp){[void][DpiDesktop]::PostMessage($hostWindow,0x8001,[IntPtr]::new($kind),[IntPtr]::Zero)}else{if(![RefNative]::IsTrayPoint($x,$y)){[pscustomobject]@{X=$x;Y=$y;Bounds=$bounds.ToString();Root=[RefNative]::RootAtPoint($x,$y);Work=[DpiDesktop]::Display().Work}|ConvertTo-Json|Set-Content (Join-Path $out 'tray-failure.json');throw 'Taskbar input target obscured'};[RefNative]::Click($x,$y,$false)}
   Start-Sleep -Milliseconds 750
   $window=if($cpp){[Windows.Automation.AutomationElement]::FromHandle($hostWindow)}else{NativeRoot $scene}
   if($cpp-and[DpiDesktop]::SendMessage($hostWindow,0x8004,[IntPtr]::Zero,[IntPtr]::Zero).ToInt64()-ne2){throw 'C++ panel failed to reach Open'}
   $tag="$percent-$target-$scene"
   Save $tag $window $cpp
   if($cpp-and$scene-eq'quick'){
    $page=$window.FindFirst([Windows.Automation.TreeScope]::Descendants,[Windows.Automation.PropertyCondition]::new([Windows.Automation.AutomationElement]::NameProperty,'More quick settings'))
    if($page-and!$page.Current.IsOffscreen){
     $page.GetCurrentPattern([Windows.Automation.InvokePattern]::Pattern).Invoke();Start-Sleep -Milliseconds 450
     Save ($tag+'-page2') $window $cpp
     $first=$window.FindFirst([Windows.Automation.TreeScope]::Descendants,[Windows.Automation.PropertyCondition]::new([Windows.Automation.AutomationElement]::NameProperty,'First quick settings page'))
     $first.GetCurrentPattern([Windows.Automation.InvokePattern]::Pattern).Invoke();Start-Sleep -Milliseconds 350
    }
   }
   if($scene-eq'date'-and$Collapsed){CalendarClick $window 'Collapse';Save ($tag+'-collapsed') $window $cpp;CalendarClick $window 'Expand'}
   Prepare
   if($Contexts-and$scene-ne'lang'){
    if($cpp){[void][DpiDesktop]::PostMessage($hostWindow,0x8002,[IntPtr]::new($kind),[IntPtr]::Zero)}else{if(![RefNative]::IsTrayPoint($x,$y)){throw 'Taskbar context target obscured'};[RefNative]::Click($x,$y,$true)}
    Start-Sleep -Milliseconds 500
    $popup=@([RefNative]::Windows()|Where-Object {$_.Class-eq'Xaml_WindowedPopupClass'}|Sort-Object {$_.Bounds.r-$_.Bounds.l}|Select-Object -First 1)
    if(!$popup){throw 'Context popup not found'}
    Save ($tag+'-context') ([Windows.Automation.AutomationElement]::FromHandle([IntPtr]::new($popup[0].Handle))) $false $popup[0].Bounds
    Prepare
   }
  }}
 }
}finally{
 try{Set-DisplayScale $initialScale}finally{
  if($bench-and!$bench.HasExited){[void][DpiDesktop]::PostMessage($back,0x10,[IntPtr]::Zero,[IntPtr]::Zero);if(!$bench.WaitForExit(5000)){$bench.Kill()}}
  $records|ConvertTo-Json -Depth 5|Set-Content (Join-Path $out 'captures.json')
  [pscustomobject]@{OriginalScale=$initialScale;FinalDpi=[DpiDesktop]::GetDpiForWindow([DpiDesktop]::Tray());BenchSHA256=(Get-FileHash $BinaryPath).Hash;Date=(Get-Date).ToString('o')}|ConvertTo-Json|Set-Content (Join-Path $out 'manifest.json')
 }
}
$records|Select-Object Tag,Scale,HostDpi,WindowClass|Format-Table -AutoSize
