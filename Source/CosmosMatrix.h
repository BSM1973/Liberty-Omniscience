#pragma once
#include <array>
#include <algorithm>
#include <cmath>
namespace cosmos {
constexpr int matrixCount=64,matrixSourceCount=163,matrixDestCount=20;
struct MatrixRoute {bool enabled=false;int source=0,destination=0,layer=0,polarity=0;float amount=0;int oscillator=0,curve=0;float smoothing=0;};
inline bool validRoute(const MatrixRoute& r)noexcept{return r.enabled&&r.amount!=0&&r.source>0&&r.source<matrixSourceCount&&r.destination>0&&r.destination<=matrixDestCount&&r.layer>=0&&r.layer<=4&&r.oscillator>=0&&r.oscillator<=5;}
inline float matrixValue(float raw,const MatrixRoute& r)noexcept{float v=r.polarity?raw*2-1:raw;float a=std::abs(v);if(r.curve==1)a*=a;else if(r.curve==2)a=std::sqrt(a);else if(r.curve==3)a=a>=.5f?1.f:0.f;return std::copysign(a,v)*r.amount;}
}
