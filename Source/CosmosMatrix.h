#pragma once
#include <array>
#include <algorithm>
namespace cosmos {
constexpr int matrixCount=64,matrixSourceCount=155,matrixDestCount=12;
struct MatrixRoute {bool enabled=false;int source=0,destination=0,layer=0,polarity=0;float amount=0;};
inline bool validRoute(const MatrixRoute& r)noexcept{return r.enabled&&r.amount!=0&&r.source>0&&r.source<matrixSourceCount&&r.destination>0&&r.destination<=matrixDestCount&&r.layer>=0&&r.layer<=4;}
inline float matrixValue(float raw,const MatrixRoute& r)noexcept{return (r.polarity?raw*2-1:raw)*r.amount;}
}
