$ErrorActionPreference='Stop'
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$out=Join-Path $root 'evidence\shell-parity-26100\dpi-v16'
$rows=[Collections.Generic.List[object]]::new()
foreach($folder in 'accepted-light','accepted-dark'){
 $captures=Get-Content (Join-Path $out "$folder\captures.json") -Raw|ConvertFrom-Json
 foreach($capture in $captures){
  $tag=$capture.Tag;$trace=Import-Csv (Join-Path $out "$folder\$tag-layout.csv")
  $layout=$trace|Select-Object -First 1;$scale=$capture.Scale
  $scene=if($tag -match '-date'){'date'}elseif($tag -match '-quick'){'quick'}else{'lang'}
  $expectedWidth=if($scene-eq'date'){336}else{360}
  $widthOk=[Math]::Abs([double]$layout.w-$expectedWidth)-le.01
  $dpiOk=[Math]::Abs([double]$layout.scale-$scale)-le.001
  $boundsOk=[double]$layout.screen_x-ge$capture.Work.Left-1-and([double]$layout.screen_x+[double]$layout.w*$scale)-le$capture.Work.Right+1-and[double]$layout.screen_y-ge$capture.Work.Top-1-and([double]$layout.screen_y+[double]$layout.h*$scale)-le$capture.Work.Bottom+1
  $weekdayCount=$null;$nativeHeight=$null;$cppHeight=$null;$delta=$null
  if($scene-eq'date'-and$tag-notmatch'collapsed'){
   $weekdayCount=@($trace|Where-Object {$_.name-match'^WeekDay[1-7]$'-and[double]$_.w-gt1}).Count
   $body=$trace|Where-Object {$_.name-eq'DateBody'};$cppHeight=[Math]::Round([double]$body.h*$scale,2)
   $percent=[int]($scale*100);$reference=if($percent-eq225){'native225-retry'}else{'baseline-matrix-core'}
   $native=Get-Content (Join-Path $out "$reference\$percent-native-date-uia.json") -Raw|ConvertFrom-Json
   $nativeHeight=($native|Where-Object {$_.Id-eq'CalendarControlScrollViewer'}).H
   $delta=[Math]::Round($cppHeight-$nativeHeight,2)
  }
  $passed=$widthOk-and$dpiOk-and$boundsOk-and($null-eq$weekdayCount-or$weekdayCount-eq7)
  $rows.Add([pscustomobject]@{Theme=$folder.Substring(9);Tag=$tag;ScalePercent=[int]($scale*100);DpiPass=$dpiOk;WidthPass=$widthOk;WorkAreaPass=$boundsOk;WeekdayLabels=$weekdayCount;CalendarNativeHeightPx=$nativeHeight;CalendarCppHeightPx=$cppHeight;CalendarDeltaPx=$delta;FunctionalPass=$passed;ExactVisualParity=$false})
 }
}
$rows|Export-Csv (Join-Path $out 'acceptance.csv') -NoTypeInformation -Encoding utf8
if(@($rows|Where-Object {!$_.FunctionalPass}).Count){$rows|Where-Object {!$_.FunctionalPass}|Format-Table;throw 'DPI acceptance failed'}
$rows|Format-Table Theme,Tag,WeekdayLabels,CalendarDeltaPx,FunctionalPass -AutoSize