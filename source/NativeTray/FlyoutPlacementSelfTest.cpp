#include "FlyoutPlacement.h"
#include <cstdio>
#include <stdexcept>
using namespace TrayFlyoutLayout;
void Require(bool value,const char* message){if(!value)throw std::runtime_error(message);std::printf("PASS %s\n",message);}
bool Near(double a,double b){return std::abs(a-b)<0.001;}
int main(){try{
 auto centered=Fit({0,0,1920,1040},1400,1040,600,300,1);
 Require(Near(centered.surface.x+centered.surface.width/2,1400),"panel is centered while it fits");
 auto edge=Fit({0,0,1920,1040},1880,1040,600,300,1);
 auto wider=Fit({0,0,1920,1040},1880,1040,900,300,1);
 Require(Near(edge.surface.x+edge.surface.width,1908) && Near(wider.surface.x+900,1908) && Near(edge.surface.x-wider.surface.x,300),
  "growth at the right limit changes only the left edge");
 auto huge=Fit({0,0,1920,1040},1880,1040,5000,4000,1);
 Require(Near(huge.surface.width,1896) && Near(huge.surface.height,1016) && Near(huge.surface.x,12) && Near(huge.surface.y,12),
  "oversize content is reduced to the work area with native margins");
 auto other=Fit({-1920,-200,1920,1080},-30,880,543,300,1.5);
 Require(Near(other.surface.x+other.surface.width,-18) && other.host.x>=-1920 && other.host.x+other.host.width<=0,
  "negative monitor origins and 150 percent DPI retain the correct edge margin");
 auto twice=Fit({2560,0,2560,1440},5000,1440,10000,10000,2);
 Require(Near(twice.surface.x,2584) && Near(twice.surface.width,2512) && Near(twice.surface.y,24),"200 percent DPI scales screen margins once");
 for(double scale:{1.0,1.25,1.5,1.75,2.0})for(int anchor=0;anchor<=1920;anchor+=80)for(int width=20;width<=4000;width+=160){
  auto layout=Fit({0,0,1920,1040},anchor,1040,width*scale,800*scale,scale);
  const auto& s=layout.surface;const auto& h=layout.host;const double gap=12*scale;
  if(s.x<gap-0.001 || s.y<gap-0.001 || s.x+s.width>1920-gap+0.001 || s.y+s.height>1040-gap+0.001 ||
    h.x<0 || h.y<0 || h.x+h.width>1920.001 || h.y+h.height>1040.001 || layout.insetX<0 || layout.insetY<0)
   throw std::runtime_error("layout crossed a monitor or shadow boundary");
 }
 Require(true,"3125 combinations of anchors widths and DPI stay inside work area and shadow bounds");
 std::puts("All placement checks passed.");return 0;
}catch(std::exception const& e){std::printf("FAIL %s\n",e.what());return 1;}}
