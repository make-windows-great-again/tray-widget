$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue'
. "$PSScriptRoot\Audit-HoverFonts.ps1" -DefinitionsOnly
Add-Type -TypeDefinition @'
using System;using System.Runtime.InteropServices;
public static class PaletteAudit {
 [StructLayout(LayoutKind.Sequential)]public struct Rect{public int left,top,right,bottom;}
 [StructLayout(LayoutKind.Sequential)]struct Identifier{public uint size;public IntPtr window;public uint id;public Guid guid;}
 [DllImport("shell32.dll")]static extern int Shell_NotifyIconGetRect(ref Identifier item,out Rect rect);
 public static Rect IconBounds(){var item=new Identifier{size=(uint)Marshal.SizeOf(typeof(Identifier)),guid=new Guid("ce31fe29-112b-4c67-a3b0-dc2459420796")};Rect rect;Marshal.ThrowExceptionForHR(Shell_NotifyIconGetRect(ref item,out rect));return rect;}
 [DllImport("user32.dll",CharSet=CharSet.Unicode)]public static extern IntPtr CreateWindowEx(uint ex,string c,string n,int style,int x,int y,int w,int h,IntPtr p,IntPtr menu,IntPtr instance,IntPtr data);
 [DllImport("user32.dll")]public static extern bool DestroyWindow(IntPtr w);
}
'@
$background=[PaletteAudit]::CreateWindowEx(0x08000088,'STATIC','Tooltip palette comparison background',0x90000004,1750,800,810,728,[IntPtr]::Zero,[IntPtr]::Zero,[IntPtr]::Zero,[IntPtr]::Zero)
$samples=@();$checks=@()
try{
 foreach($name in 'vpn','quick','date'){
  $target=$targets[$name];if(!$target){throw "Missing tray target: $name"};$r=$target.Current.BoundingRectangle
  if($name -eq 'vpn'){$native=[PaletteAudit]::IconBounds();$r=[Windows.Rect]::new($native.left,$native.top,$native.right-$native.left,$native.bottom-$native.top)}
  Move-Pointer 1500 750;Start-Sleep -Milliseconds 900
  Move-Pointer ([int]($r.X+$r.Width/2)) ([int]$r.Bottom-3);Start-Sleep -Milliseconds 1200
  $cursor=New-Object HoverAudit+Point;[void][HoverAudit]::GetPhysicalCursorPos([ref]$cursor)
  if([Math]::Abs($cursor.x-($r.X+$r.Width/2)) -gt 3 -or [Math]::Abs($cursor.y-($r.Bottom-3)) -gt 3){throw "Pointer moved away from sampled $name target; capture cancelled"}
  $found=@([HoverAudit]::Popups() | Where-Object {[HoverAudit]::Class($_) -eq 'Xaml_WindowedPopupClass'})
  if($found.Count -ne 1){throw "Expected one tooltip for $name; found $($found.Count)"}
  if($name -eq 'vpn'){
   $popupRoot=[Windows.Automation.AutomationElement]::FromHandle($found[0])
   [pscustomobject]@{TargetName=$target.Current.Name;TargetBounds=$r.ToString();CursorX=$cursor.x;CursorY=$cursor.y;PopupName=$popupRoot.Current.Name;PopupId=$popupRoot.Current.AutomationId} | ConvertTo-Json | Set-Content -LiteralPath "$PSScriptRoot\palette-v12-owner.json"
   $own=$popupRoot.FindFirst([Windows.Automation.TreeScope]::Subtree,[Windows.Automation.PropertyCondition]::new([Windows.Automation.AutomationElement]::AutomationIdProperty,'YataTrayTooltip'))
   if(!$own){throw 'The visible tooltip belongs to another tray button; capture cancelled'}
  }
  $bounds=New-Object HoverAudit+Rect;[void][HoverAudit]::GetWindowRect($found[0],[ref]$bounds)
  $width=[Math]::Min($bounds.right,2560)-$bounds.left;$height=$bounds.bottom-$bounds.top
  $bitmap=New-Object Drawing.Bitmap($width,$height);$graphics=[Drawing.Graphics]::FromImage($bitmap)
  try{
   $graphics.CopyFromScreen($bounds.left,$bounds.top,0,0,$bitmap.Size)
   $red=0.0;$green=0.0;$blue=0.0;$count=0
   for($y=25;$y -lt $height-25;$y++){for($x=12;$x -lt 16;$x++){
    $pixel=$bitmap.GetPixel($x,$y);$red+=$pixel.R;$green+=$pixel.G;$blue+=$pixel.B;$count++
   }}
   $samples += [pscustomobject]@{Target=$name;Left=$bounds.left;Top=$bounds.top;Right=$bounds.right;Bottom=$bounds.bottom;R=$red/$count;G=$green/$count;B=$blue/$count;Pixels=$count}
   $bitmap.Save("$PSScriptRoot\palette-v12-$name.png",[Drawing.Imaging.ImageFormat]::Png)
  }finally{$graphics.Dispose();$bitmap.Dispose()}
 }
 foreach($native in $samples | Where-Object {$_.Target -ne 'vpn'}){
  foreach($channel in 'R','G','B'){
   $delta=[Math]::Abs($samples[0].$channel-$native.$channel)
   if($delta -gt 2){throw "Tooltip palette differs from $($native.Target): $channel delta=$delta"}
  }
  $checks += "PASS tooltip background matches $($native.Target) within 2 RGB levels over the same background"
 }
 $checks+='ALL TOOLTIP PALETTE CHECKS PASSED'
}catch{$checks+="FAIL $($_.Exception.Message)";throw}
finally{
 Move-Pointer 1500 750
 [void][PaletteAudit]::DestroyWindow($background)
 $samples | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath "$PSScriptRoot\palette-v12-samples.json" -Encoding UTF8
 $checks | Set-Content -LiteralPath "$PSScriptRoot\palette-v12-results.txt" -Encoding UTF8
}
