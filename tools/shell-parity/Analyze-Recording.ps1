param([Parameter(Mandatory=$true)][string]$Name)
$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'));$out=Join-Path $root 'evidence\shell-parity-26100\recordings';$ff=Join-Path $root 'artifacts\tools\ffmpeg-9.0.2-essentials_build\bin\ffmpeg.exe'
$raw=Join-Path $out ($Name+'-column.gray')
& $ff -hide_banner -loglevel error -y -i (Join-Path $out ($Name+'.mp4')) -vf 'crop=2:1528:1006:0,format=gray' -fps_mode passthrough -enc_time_base 1:1000000 -f rawvideo $raw
if($LASTEXITCODE){throw 'Frame extraction failed'}
$metadata=Get-Content (Join-Path $out ($Name+'-frames.json')) -Raw|ConvertFrom-Json
if(!('FrameMetrics'-as[type])){Add-Type @'
using System;using System.IO;using System.Collections.Generic;
public static class FrameMetrics {
 public static string[] Read(string path,bool dark,string[] timestamps,int length){
  byte[] data=File.ReadAllBytes(path);int stride=length*2,count=data.Length/stride;if(count!=timestamps.Length)throw new Exception("Timestamp/frame count differs");
  var rows=new List<string>();rows.Add("frame,seconds,top_px,bottom_px,material_gray");
  for(int f=0;f<count;f++){int start=-1,top=-1,bottom=-1;int offset=f*stride;
   for(int y=0;y<length;y++){int value=data[offset+2*y];bool surface=dark?value<70:value>160;
    if(surface){if(start<0)start=y;}else{if(start>=0&&y-start>12){if(top<0)top=start;bottom=y-1;}start=-1;}}
   if(start>=0&&length-start>12){if(top<0)top=start;bottom=length-1;}
   double tone=0;if(top>=0){int first=Math.Min(top+20,length-1),last=Math.Min(first+20,length);for(int y=first;y<last;y++)tone+=data[offset+2*y];tone/=last-first;}
   rows.Add(f+","+timestamps[f]+","+top+","+bottom+","+tone.ToString("F3",System.Globalization.CultureInfo.InvariantCulture));
  }return rows.ToArray();
 }
}
'@
}
$timestamps=[string[]]@($metadata.frames|ForEach-Object best_effort_timestamp_time)
[FrameMetrics]::Read($raw,($Name.Contains('dark')-or$Name.Contains('custom')),$timestamps,1528)|Set-Content (Join-Path $out ($Name+'-motion.csv'))
$row=Join-Path $out ($Name+'-row.gray')
& $ff -hide_banner -loglevel error -y -i (Join-Path $out ($Name+'.mp4')) -vf 'crop=1120:2:0:900,transpose=1,format=gray' -fps_mode passthrough -enc_time_base 1:1000000 -f rawvideo $row
if($LASTEXITCODE){throw 'Horizontal frame extraction failed'}
[FrameMetrics]::Read($row,($Name.Contains('dark')-or$Name.Contains('custom')),$timestamps,1120)|ForEach-Object{$_.Replace('top_px,bottom_px','left_px,right_px')}|Set-Content (Join-Path $out ($Name+'-horizontal.csv'))
$times=@($timestamps|ForEach-Object{[double]::Parse($_,[Globalization.CultureInfo]::InvariantCulture)})
$intervals=for($i=1;$i-lt$times.Count;$i++){1000*($times[$i]-$times[$i-1])};$sorted=@($intervals|Sort-Object)
[pscustomobject]@{Name=$Name;Frames=$times.Count;Duration=$times[-1];MeanIntervalMs=($intervals|Measure-Object -Average).Average;MedianIntervalMs=$sorted[[int]($sorted.Count*.5)];P95IntervalMs=$sorted[[int]($sorted.Count*.95)];MaxIntervalMs=$sorted[-1]}|ConvertTo-Json|Set-Content (Join-Path $out ($Name+'-metrics.json'))
