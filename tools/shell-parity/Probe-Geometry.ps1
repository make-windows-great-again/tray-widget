$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'));$out=Join-Path $root 'evidence\shell-parity-26100\geometry';[IO.Directory]::CreateDirectory($out)|Out-Null
Add-Type -AssemblyName UIAutomationClient,UIAutomationTypes,WindowsBase,System.Drawing
foreach($file in @('Test-Bench.ps1','Capture-Reference.ps1')){$source=[IO.File]::ReadAllText((Join-Path $PSScriptRoot $file));$begin=$source.IndexOf('using System;');$end=$source.IndexOf("'@",$begin);Add-Type $source.Substring($begin,$end-$begin)}
[void][BenchNative]::SetThreadDpiAwarenessContext([IntPtr]::new(-4))
$bench=Start-Process -FilePath (Join-Path $root 'artifacts\shell-panels\Release\ShellBench.exe') -WindowStyle Hidden -PassThru
function Save([string]$name,[int]$x,[int]$y){
 $element=[Windows.Automation.AutomationElement]::FromPoint([Windows.Point]::new($x,$y));$chain=[Collections.Generic.List[object]]::new();$last=$element
 for($i=0;$i-lt15-and$element;$i++){$chain.Add([pscustomobject]@{Name=$element.Current.Name;Class=$element.Current.ClassName;Pid=$element.Current.ProcessId});$parent=[Windows.Automation.TreeWalker]::RawViewWalker.GetParent($element);if(!$parent-or$parent.Current.ProcessId-eq0){break};$last=$element;$element=$parent}
 $chain|ConvertTo-Json|Set-Content (Join-Path $out ($name+'-parents.json'))
 $rows=foreach($child in $last.FindAll([Windows.Automation.TreeScope]::Descendants,[Windows.Automation.Condition]::TrueCondition)){$c=$child.Current;$b=$c.BoundingRectangle;[pscustomobject]@{Name=$c.Name;Id=$c.AutomationId;Class=$c.ClassName;Type=$c.ControlType.ProgrammaticName;X=$b.X;Y=$b.Y;W=$b.Width;H=$b.Height}}
 $rows|ConvertTo-Json -Depth 3|Set-Content (Join-Path $out ($name+'-uia.json'))
 $bitmap=[Drawing.Bitmap]::new(1120,1600);$g=[Drawing.Graphics]::FromImage($bitmap);try{$g.CopyFromScreen(1440,0,0,0,$bitmap.Size);$bitmap.Save((Join-Path $out ($name+'.png')))}finally{$g.Dispose();$bitmap.Dispose()}
}
try{
 Start-Sleep -Milliseconds 1800;$h=[BenchNative]::FindWindow('Yata.ShellBench.Host','ShellPanels test host');$b=[BenchNative]::FindWindow('Yata.ShellBench.Background','C++ ShellPanels test bench');[void][BenchNative]::SetForegroundWindow($b)
 [RefNative]::Click(2363,1564,$false);Start-Sleep -Milliseconds 900;Save 'native-quick' 2090 1000;[RefNative]::Escape();Start-Sleep -Milliseconds 400
 [void][BenchNative]::PostMessage($h,0x8001,[IntPtr]::new(1),[IntPtr]::Zero);Start-Sleep -Milliseconds 900;Save 'cpp-quick' 2090 1000;[RefNative]::Escape();Start-Sleep -Milliseconds 400
 foreach($part in @(@('network',2320,1400),@('battery',2408,1480))){[RefNative]::Click($part[1],1564,$true);Start-Sleep -Milliseconds 700;Save ('native-'+$part[0]+'-menu') 2400 $part[2];[RefNative]::Escape();Start-Sleep -Milliseconds 300}
}finally{[void][BenchNative]::PostMessage($b,0x10,[IntPtr]::Zero,[IntPtr]::Zero);if(!$bench.WaitForExit(5000)){$bench.Kill()}}
