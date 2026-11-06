param([string]$App=(Join-Path $PSScriptRoot '..\..\..\YataNativeFramework.exe'),[int]$Seconds=20,[string]$Label='profile-v12')
$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue'
Add-Type -TypeDefinition @'
using System;using System.Runtime.InteropServices;
public static class BackendProfile {
 [StructLayout(LayoutKind.Sequential)]struct Time{public uint low,high;public ulong Value{get{return ((ulong)high<<32)|low;}}}
 [DllImport("kernel32.dll")]static extern IntPtr OpenThread(uint rights,bool inherit,uint tid);
 [DllImport("kernel32.dll")]static extern bool GetThreadTimes(IntPtr thread,out Time created,out Time exited,out Time kernel,out Time user);
 [DllImport("kernel32.dll")]static extern bool CloseHandle(IntPtr value);
 [DllImport("user32.dll",CharSet=CharSet.Unicode)]public static extern uint RegisterWindowMessage(string name);
 [DllImport("user32.dll")]public static extern bool PostMessage(IntPtr w,uint m,IntPtr a,IntPtr b);
 [DllImport("user32.dll")]static extern int GetSystemMetrics(int key);
 [DllImport("user32.dll")]static extern void mouse_event(uint f,uint x,uint y,uint d,UIntPtr extra);
 public static void MoveAway(){mouse_event(0x8001,(uint)(1500*65535.0/(GetSystemMetrics(0)-1)),(uint)(750*65535.0/(GetSystemMetrics(1)-1)),0,UIntPtr.Zero);}
 public static double ThreadCpu(uint tid){IntPtr h=OpenThread(0x800,false,tid);if(h==IntPtr.Zero)return -1;try{Time c,e,k,u;if(!GetThreadTimes(h,out c,out e,out k,out u))return -1;return (k.Value+u.Value)/10000.0;}finally{CloseHandle(h);}}
}
'@
$App=[IO.Path]::GetFullPath($App)
function Snapshot {
 $start=[Diagnostics.ProcessStartInfo]::new($App,'--profile');$start.UseShellExecute=$false;$start.CreateNoWindow=$true;$start.RedirectStandardOutput=$true
 $process=[Diagnostics.Process]::Start($start);$text=$process.StandardOutput.ReadToEnd();$process.WaitForExit();$code=$process.ExitCode;$process.Dispose()
 if($code -ne 0){throw 'Profile endpoint unavailable'};return ($text|ConvertFrom-Json)
}
$map=[IO.MemoryMappedFiles.MemoryMappedFile]::OpenExisting('Local\YataNativeFramework.v13');$view=$map.CreateViewAccessor(0,0,[IO.MemoryMappedFiles.MemoryMappedFileAccess]::Read)
try{
 if($view.ReadInt32(20) -le 0){throw 'Native backend must be applied before profiling'}
 $explorerId=$view.ReadInt32(32);$thread=$view.ReadInt32(24);$applicationId=$view.ReadInt32(4)
 [BackendProfile]::MoveAway();Start-Sleep -Milliseconds 700
 $before=Snapshot;$explorer=Get-Process -Id $explorerId;$cpuBefore=$explorer.TotalProcessorTime.TotalMilliseconds;$handlesBefore=$explorer.HandleCount;$memoryBefore=$explorer.PrivateMemorySize64
 $threadBefore=[BackendProfile]::ThreadCpu($thread);$application=Get-Process -Id $applicationId;$appBefore=$application.TotalProcessorTime.TotalMilliseconds
 $clock=[Diagnostics.Stopwatch]::StartNew();Start-Sleep -Seconds $Seconds;$elapsed=$clock.Elapsed.TotalMilliseconds
 $after=Snapshot;$explorer=Get-Process -Id $explorerId;$application=Get-Process -Id $applicationId
 $idle=[pscustomobject]@{ElapsedMs=$elapsed;BackendRefreshes=$after.refreshes-$before.refreshes;Notifications=$after.notifications-$before.notifications;TooltipOpens=$after.tooltipOpens-$before.tooltipOpens;PollingTimers=$after.pollingTimers;TooltipTimer=$after.tooltipTimer;ExplorerCpuMs=$explorer.TotalProcessorTime.TotalMilliseconds-$cpuBefore;ExplorerUiThreadCpuMs=[BackendProfile]::ThreadCpu($thread)-$threadBefore;ApplicationCpuMs=$application.TotalProcessorTime.TotalMilliseconds-$appBefore;HandlesBefore=$handlesBefore;HandlesAfter=$explorer.HandleCount;PrivateBytesBefore=$memoryBefore;PrivateBytesAfter=$explorer.PrivateMemorySize64}
 if($after.pollingTimers -ne 0 -or $after.tooltipTimer -ne 0){throw 'Unexpected timer in the idle backend'}
 $before=Snapshot;$window=[IntPtr]::new($view.ReadInt64(104));$session=[IntPtr]::new($view.ReadInt64(72));$message=[BackendProfile]::RegisterWindowMessage('YataTray.State.v12')
 for($i=0;$i -lt 1000;$i++){if(![BackendProfile]::PostMessage($window,$message,$session,[IntPtr]::Zero)){throw 'Notification burst was rejected'}}
 Start-Sleep -Milliseconds 500;$after=Snapshot
 $burst=[pscustomobject]@{Sent=1000;Received=$after.notifications-$before.notifications;Updates=$after.refreshes-$before.refreshes;WorkMicroseconds=if($after.qpcFrequency){($after.updateTicks-$before.updateTicks)*1000000.0/$after.qpcFrequency}else{0};MaxUpdateMicrosecondsSinceAttach=if($after.qpcFrequency){$after.maxUpdateTicks*1000000.0/$after.qpcFrequency}else{0}}
 if($burst.Received -ne 1000 -or $burst.Updates -ge 1000){throw 'Notification coalescing did not reduce work'}
 $os=Get-ItemProperty -LiteralPath 'HKLM:\SOFTWARE\Microsoft\Windows NT\CurrentVersion'
 [pscustomobject]@{WindowsBuild=$os.CurrentBuild;UBR=$os.UBR;ExplorerPid=$explorerId;UiThread=$thread;Idle=$idle;Burst=$burst;Note='Explorer CPU includes unrelated shell work; backend counters attribute only this module.'}|ConvertTo-Json -Depth 7|Set-Content -LiteralPath "$PSScriptRoot\$Label.json" -Encoding UTF8
 'PASS no periodic timers; idle and burst measurements recorded'|Set-Content -LiteralPath "$PSScriptRoot\$Label-results.txt"
}finally{$view.Dispose();$map.Dispose()}
