param([string[]]$Tags=@('native-light','cpp-light','native-dark','cpp-dark'),[string]$OutputName='motion-measurements.csv')
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'));$out=Join-Path $root 'evidence\shell-parity-26100';$recordings=Join-Path $out 'recordings'
$results=[Collections.Generic.List[object]]::new()
foreach($tag in $Tags){
 $events=Get-Content (Join-Path $recordings ($tag+'-events.json')) -Raw|ConvertFrom-Json
 $vertical=Import-Csv (Join-Path $recordings ($tag+'-motion.csv'))|ForEach-Object{[pscustomobject]@{Time=[double]$_.seconds;Top=[int]$_.top_px;Tone=[double]$_.material_gray}}
 $horizontal=Import-Csv (Join-Path $recordings ($tag+'-horizontal.csv'))|ForEach-Object{[pscustomobject]@{Time=[double]$_.seconds;Top=[int]$_.left_px;Tone=[double]$_.material_gray}}
 foreach($scene in @('lang','quick','date')){
  $frames=if($scene-eq'date'){$horizontal}else{$vertical};$axis=if($scene-eq'date'){'X'}else{'Y'}
  $open=$events|Where-Object Event -eq ($scene+'-open');$t=[double]$open.Ms/1000
  $range=@($frames|Where-Object {$_.Time-ge$t-.5-and$_.Time-le$t+.75})
  $visible=@($range|Where-Object Top -ge 0);$stable=($visible|Group-Object Top|Sort-Object Count -Descending|Select-Object -First 1).Name -as[int]
  $begin=$visible|Select-Object -First 1;$end=$visible|Where-Object{$_.Top-le$stable+2}|Select-Object -First 1
  $gaps=for($i=1;$i-lt$range.Count;$i++){1000*($range[$i].Time-$range[$i-1].Time)}
  $duration=if($begin-and$end-and$end.Time-ge$begin.Time){[Math]::Round(1000*($end.Time-$begin.Time),2)}else{$null}
  $results.Add([pscustomobject]@{Tag=$tag;Scene=$scene;Axis=$axis;Action='open';Start=$begin.Time;End=$end.Time;ObservedMs=$duration;StableEdge=$stable;MaxWindowGapMs=[Math]::Round(($gaps|Measure-Object -Maximum).Maximum,2);ToneMin='';ToneMax=''})
  foreach($action in @('toggle-close','outside-close')){
   $event=$events|Where-Object Event -eq ($scene+'-'+$action);$t=[double]$event.Ms/1000
   $range=@($frames|Where-Object {$_.Time-ge$t-.5-and$_.Time-le$t+.5})
   $begin=$range|Where-Object {$_.Top-gt$stable+2}|Select-Object -First 1
   $end=$range|Where-Object {$_.Time-gt$begin.Time-and$_.Top-lt0}|Select-Object -First 1
   $limit=if($axis-eq'X'){1060}else{1450}
   $moving=@($range|Where-Object {$_.Time-ge$begin.Time-and$_.Time-lt$end.Time-and$_.Top-lt$limit})
   $gaps=for($i=1;$i-lt$range.Count;$i++){1000*($range[$i].Time-$range[$i-1].Time)}
   $tone=$moving|Measure-Object Tone -Minimum -Maximum
   $duration=if($begin-and$end-and$end.Time-gt$begin.Time){[Math]::Round(1000*($end.Time-$begin.Time),2)}else{$null}
   $results.Add([pscustomobject]@{Tag=$tag;Scene=$scene;Axis=$axis;Action=$action;Start=$begin.Time;End=$end.Time;ObservedMs=$duration;StableEdge=$stable;MaxWindowGapMs=[Math]::Round(($gaps|Measure-Object -Maximum).Maximum,2);ToneMin=$tone.Minimum;ToneMax=$tone.Maximum})
  }
 }
}
$results|Export-Csv (Join-Path $out $OutputName) -NoTypeInformation
$results|Format-Table -AutoSize
