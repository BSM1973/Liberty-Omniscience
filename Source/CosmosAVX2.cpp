#include <immintrin.h>
namespace cosmos {
void fxButterflyAVX2(float* a,float* b,const float* w,int count,bool inverse)noexcept{
 int j=0;for(;j+3<count;j+=4){auto x=_mm256_loadu_ps(a+2*j),y=_mm256_loadu_ps(b+2*j),q=_mm256_loadu_ps(w+2*j);auto wr=_mm256_moveldup_ps(q),wi=_mm256_movehdup_ps(q);if(inverse)wi=_mm256_sub_ps(_mm256_setzero_ps(),wi);auto v=_mm256_addsub_ps(_mm256_mul_ps(y,wr),_mm256_mul_ps(_mm256_permute_ps(y,0xb1),wi));_mm256_storeu_ps(a+2*j,_mm256_add_ps(x,v));_mm256_storeu_ps(b+2*j,_mm256_sub_ps(x,v));}
 for(;j<count;++j){int i=2*j;float wr=w[i],wi=inverse?-w[i+1]:w[i+1],vr=b[i]*wr-b[i+1]*wi,vi=b[i]*wi+b[i+1]*wr,ar=a[i],ai=a[i+1];a[i]=ar+vr;a[i+1]=ai+vi;b[i]=ar-vr;b[i+1]=ai-vi;}
 _mm256_zeroupper();
}
void fxWindowAddAVX2(float* out,const float* signal,const float* window,int n)noexcept{
 const auto g=_mm256_set1_ps(2.f/3.f);int i=0;
 for(;i+7<n;i+=8){auto x=_mm256_mul_ps(_mm256_loadu_ps(signal+i),_mm256_loadu_ps(window+i));_mm256_storeu_ps(out+i,_mm256_add_ps(_mm256_loadu_ps(out+i),_mm256_mul_ps(x,g)));}
 for(;i<n;++i)out[i]+=signal[i]*window[i]*(2.f/3.f);
 _mm256_zeroupper();
}
void advancePhasesAVX2(double* phase,const double* delta,int n) noexcept {
 const auto one=_mm256_set1_pd(1.);
 int i=0;
 for(;i+3<n;i+=4){
   auto x=_mm256_add_pd(_mm256_loadu_pd(phase+i),_mm256_loadu_pd(delta+i));
   auto mask=_mm256_cmp_pd(x,one,_CMP_GE_OQ);
   x=_mm256_sub_pd(x,_mm256_and_pd(mask,one));_mm256_storeu_pd(phase+i,x);
 }
 for(;i<n;++i){phase[i]+=delta[i];if(phase[i]>=1.)phase[i]-=1.;}
 _mm256_zeroupper();
}
void interpolateAVX2(float* out,const float* a,const float* aa,const float* b,const float* bb,const float* t,float morph,int n)noexcept{
 auto m=_mm256_set1_ps(morph);int i=0;
 for(;i+7<n;i+=8){
   auto p=_mm256_loadu_ps(t+i),x=_mm256_loadu_ps(a+i),y=_mm256_loadu_ps(b+i);
   x=_mm256_add_ps(x,_mm256_mul_ps(_mm256_sub_ps(_mm256_loadu_ps(aa+i),x),p));
   y=_mm256_add_ps(y,_mm256_mul_ps(_mm256_sub_ps(_mm256_loadu_ps(bb+i),y),p));
   auto z=_mm256_add_ps(x,_mm256_mul_ps(_mm256_sub_ps(y,x),m));
   _mm256_storeu_ps(out+i,_mm256_add_ps(_mm256_loadu_ps(out+i),z));
 }
 for(;i<n;++i){float x=a[i]+(aa[i]-a[i])*t[i],y=b[i]+(bb[i]-b[i])*t[i];out[i]+=x+(y-x)*morph;}
 _mm256_zeroupper();
}
}
