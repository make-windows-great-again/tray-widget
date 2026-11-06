param([string[]]$Names=@('native-light-interactions','cpp-light-interactions-v15'),[string]$OutputName='date-v15-card-motion.csv')
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$out=Join-Path $root 'evidence\shell-parity-26100';$recordings=Join-Path $out 'recordings'
$ff=Join-Path $root 'artifacts\tools\ffmpeg-9.0.2-essentials_build\bin\ffmpeg.exe'
if(!('CardFrames'-as[type])){Add-Type @'
using System;using System.IO;using System.Collections.Generic;
public static class CardFrames {
 public static string[] Read(string path,bool dark,string[] times){
  byte[] bytes=File.ReadAllBytes(path);const int stride=1528*2;
  if(bytes.Length/stride!=times.Length)throw new Exception("Frame/timestamp count differs");
  var rows=new List<string>();rows.Add("frame,seconds,calendar_top_px");
  for(int f=0;f<times.Length;f++){
   int start=-1,edge=-1;
   for(int y=700;y<1450;y++){
    int value=bytes[f*stride+y*2];bool gap=dark?value>75:value<150;
    if(gap){if(start<0)start=y;}
    else {if(start>=0&&y-start>=12&&y-start<=40){edge=y;break;}start=-1;}
   }
   rows.Add(f+","+times[f]+","+edge);
  }return rows.ToArray();
 }
}
'@}
$rows=foreach($name in $Names){
 $raw=Join-Path $recordings ($name+'-cards.gray')
 & $ff -hide_banner -loglevel error -y -i (Join-Path $recordings ($name+'.mp4')) -vf 'crop=2:1528:1006:0,format=gray' -fps_mode passthrough -enc_time_base 1:1000000 -f rawvideo $raw
 if($LASTEXITCODE){throw 'Frame extraction failed'}
 $metadata=Get-Content (Join-Path $recordings ($name+'-frames.json')) -Raw|ConvertFrom-Json
 $times=[string[]]@($metadata.frames|ForEach-Object best_effort_timestamp_time)
 $trace=Join-Path $recordings ($name+'-cards.csv')
 [CardFrames]::Read($raw,$name.Contains('dark'),$times)|Set-Content $trace
 $frames=Import-Csv $trace|ForEach-Object{[pscustomobject]@{Time=[double]$_.seconds;Top=[int]$_.calendar_top_px}}
 $events=Get-Content (Join-Path $recordings ($name+'-events.json')) -Raw|ConvertFrom-Json
 foreach($action in @('Collapse','Expand')){
  $event=$events|Where-Object Event -like ('press-'+$action+'*')|Select-Object -First 1
  if(!$event){throw "Missing $action event"}
  $time=$event.Ms/1000
  $framesInRange=@($frames|Where-Object {$_.Time-ge$time-.5-and$_.Time-le$time+.85-and$_.Top-ge0})
  $edges=$framesInRange|Measure-Object Top -Minimum -Maximum
  $initial=if($action-eq'Collapse'){$edges.Minimum}else{$edges.Maximum}
  $final=if($action-eq'Collapse'){$edges.Maximum}else{$edges.Minimum}
  $begin=$framesInRange|Where-Object {[Math]::Abs($_.Top-$initial)-gt2}|Select-Object -First 1
  $end=$framesInRange|Where-Object {$_.Time-ge$begin.Time-and[Math]::Abs($_.Top-$final)-le2}|Select-Object -First 1
  if(!$begin-or!$end-or[Math]::Abs($final-$initial)-lt500){throw "Incomplete transition: $name $action"}
  $moving=@($framesInRange|Where-Object {$_.Time-ge$begin.Time-and$_.Time-le$end.Time})
  $gaps=for($i=1;$i-lt$moving.Count;$i++){1000*($moving[$i].Time-$moving[$i-1].Time)}
  [pscustomobject]@{Name=$name;Action=$action;InitialTopPx=$initial;FinalTopPx=$final;ObservedMs=[Math]::Round(1000*($end.Time-$begin.Time),2);MovingFrames=$moving.Count;MaxMovingCaptureGapMs=[Math]::Round(($gaps|Measure-Object -Maximum).Maximum,2)}
 }
}
$rows|Export-Csv (Join-Path $out $OutputName) -NoTypeInformation
$rows|Format-Table -AutoSize
