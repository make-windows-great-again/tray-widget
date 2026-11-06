#pragma once
#include <algorithm>
#include <cmath>

namespace TrayFlyoutLayout
{
struct Box {double x,y,width,height;};
struct Layout {Box surface,host;double insetX,insetY;};

// All arguments use physical pixels; spacing is scaled once from DIP.
// Center while space allows, then clamp and shrink against the work area.
inline Layout Fit(Box work,double centerX,double anchorTop,double requestedWidth,
    double requestedHeight,double scale,double topInsetDips=12)
{
    const double gap=std::min(12*scale,std::max(0.0,(std::min(work.width,work.height)-1)/4)),shadow=24*scale;
    const double availableWidth=std::max(1.0,work.width-2*gap);
    const double availableBottom=std::clamp(anchorTop,work.y+2*gap+1,work.y+work.height);
    const double availableHeight=std::max(1.0,availableBottom-work.y-gap-std::min(topInsetDips*scale,gap));
    const double width=std::clamp(requestedWidth,1.0,availableWidth);
    const double height=std::clamp(requestedHeight,1.0,availableHeight);
    const double left=std::clamp(centerX-width/2,work.x+gap,work.x+work.width-gap-width);
    const double top=availableBottom-gap-height;
    Box surface{left,top,width,height};
    const double hostLeft=std::max(work.x,left-shadow);
    const double hostTop=std::max(work.y,top-shadow);
    const double hostRight=std::min(work.x+work.width,left+width+shadow);
    const double hostBottom=std::min(availableBottom,top+height+shadow);
    return {surface,{hostLeft,hostTop,hostRight-hostLeft,hostBottom-hostTop},left-hostLeft,top-hostTop};
}
}
