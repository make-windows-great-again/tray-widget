$ErrorActionPreference='Stop'
Add-Type -AssemblyName UIAutomationClient,UIAutomationTypes,WindowsBase,System.Drawing
if(!('DpiDesktop'-as[type])){Add-Type @'
using System;using System.Runtime.InteropServices;
public static class DpiDesktop {
 [StructLayout(LayoutKind.Sequential)]public struct Rect{public int Left,Top,Right,Bottom;}
 [StructLayout(LayoutKind.Sequential)]public struct MonitorInfo{public int Size;public Rect Monitor,Work;public uint Flags;}
 [DllImport("user32.dll",CharSet=CharSet.Unicode)]public static extern IntPtr FindWindow(string type,string name);
 [DllImport("user32.dll")]public static extern uint GetDpiForWindow(IntPtr window);
 [DllImport("user32.dll")]public static extern bool ShowWindow(IntPtr window,int command);
 [DllImport("user32.dll")]public static extern bool SetForegroundWindow(IntPtr window);
 [DllImport("user32.dll")]public static extern bool SetWindowPos(IntPtr window,IntPtr after,int x,int y,int width,int height,uint flags);
 [DllImport("user32.dll")]public static extern IntPtr GetForegroundWindow();
 [DllImport("user32.dll")]public static extern bool GetWindowRect(IntPtr window,out Rect rect);
 [DllImport("user32.dll")]public static extern IntPtr SetThreadDpiAwarenessContext(IntPtr context);
 [DllImport("user32.dll")]static extern IntPtr MonitorFromWindow(IntPtr window,uint flags);
 [DllImport("user32.dll")]static extern bool GetMonitorInfo(IntPtr monitor,ref MonitorInfo info);
 [DllImport("user32.dll",CharSet=CharSet.Unicode)]public static extern uint RegisterWindowMessage(string name);
 [DllImport("user32.dll")]public static extern IntPtr SendMessage(IntPtr window,uint message,IntPtr wp,IntPtr lp);
 [DllImport("user32.dll")]public static extern bool PostMessage(IntPtr window,uint message,IntPtr wp,IntPtr lp);
 public static IntPtr Tray(){return FindWindow("Shell_TrayWnd",null);}
 public static void Position(IntPtr w,IntPtr after,int x,int y,int width,int height,uint flags){var old=SetThreadDpiAwarenessContext(new IntPtr(-4));try{if(!SetWindowPos(w,after,x,y,width,height,flags))throw new Exception("SetWindowPos failed");}finally{SetThreadDpiAwarenessContext(old);}}
 public static MonitorInfo Display(){var old=SetThreadDpiAwarenessContext(new IntPtr(-4));try{var info=new MonitorInfo();info.Size=Marshal.SizeOf(info);if(!GetMonitorInfo(MonitorFromWindow(Tray(),2),ref info))throw new Exception("Monitor unavailable");return info;}finally{SetThreadDpiAwarenessContext(old);}}
}
'@}
function Get-SettingsWindow {
 $condition=[Windows.Automation.PropertyCondition]::new([Windows.Automation.AutomationElement]::NameProperty,'Settings')
 $window=[Windows.Automation.AutomationElement]::RootElement.FindFirst([Windows.Automation.TreeScope]::Children,$condition)
 if(!$window){throw 'Display Settings window unavailable; open ms-settings:display first'}
 return $window
}
function Get-ScaleControl {
 $window=Get-SettingsWindow
 [void][DpiDesktop]::ShowWindow([IntPtr]$window.Current.NativeWindowHandle,9)
 [void][DpiDesktop]::SetForegroundWindow([IntPtr]$window.Current.NativeWindowHandle)
 $condition=[Windows.Automation.PropertyCondition]::new([Windows.Automation.AutomationElement]::AutomationIdProperty,'SystemSettings_Display_Scaling_ItemSizeOverride_ComboBox')
 $until=[DateTime]::UtcNow.AddSeconds(4)
 do{$control=$window.FindFirst([Windows.Automation.TreeScope]::Descendants,$condition);if($control){return $control};Start-Sleep -Milliseconds 100}while([DateTime]::UtcNow-lt$until)
 throw 'Standard display Scale control unavailable; no custom scaling will be used'
}
function Get-DisplayScale {
 $control=Get-ScaleControl
 $selected=$control.GetCurrentPattern([Windows.Automation.SelectionPattern]::Pattern).Current.GetSelection()
 if($selected.Count-ne1-or$selected[0].Current.Name-notmatch'^(\d+)%'){throw 'Cannot read selected scale'}
 return [int]$Matches[1]
}
function Set-DisplayScale([ValidateSet(100,125,150,175,200,225,250,300)][int]$Percent) {
 $control=Get-ScaleControl
 $selected=$control.GetCurrentPattern([Windows.Automation.SelectionPattern]::Pattern).Current.GetSelection()
 if($selected.Count-eq1-and$selected[0].Current.Name-match('^'+$Percent+'%')){return}
 $control.GetCurrentPattern([Windows.Automation.ExpandCollapsePattern]::Pattern).Expand()
 $until=[DateTime]::UtcNow.AddSeconds(3)
 do{
  $items=$control.FindAll([Windows.Automation.TreeScope]::Descendants,[Windows.Automation.PropertyCondition]::new([Windows.Automation.AutomationElement]::ControlTypeProperty,[Windows.Automation.ControlType]::ListItem))
  $item=$items|Where-Object {$_.Current.Name-match('^'+$Percent+'%(\s|$)')}|Select-Object -First 1
  if(!$item){Start-Sleep -Milliseconds 80}
 }while(!$item-and[DateTime]::UtcNow-lt$until)
 if(!$item){throw "Scale $Percent is not a standard option on this display"}
 $item.GetCurrentPattern([Windows.Automation.SelectionItemPattern]::Pattern).Select()
 $until=[DateTime]::UtcNow.AddSeconds(8)
 do{Start-Sleep -Milliseconds 100;$dpi=[DpiDesktop]::GetDpiForWindow([DpiDesktop]::Tray())}while($dpi-ne[uint32]($Percent*96/100)-and[DateTime]::UtcNow-lt$until)
 if($dpi-ne[uint32]($Percent*96/100)){throw "Scale change did not reach expected DPI: $dpi"}
 Start-Sleep -Milliseconds 900
}
