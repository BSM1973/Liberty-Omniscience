#pragma once
#if defined(__aarch64__)
 #include <arm_neon.h>
#elif defined(__SSE2__) || defined(_M_X64)
 #include <emmintrin.h>
#endif
#if defined(COSMOS_AVX2_DISPATCH) && defined(_M_X64)
 #include <intrin.h>
#endif
namespace cosmos {
#if defined(COSMOS_AVX2_DISPATCH) && defined(_M_X64)
void fxButterflyAVX2(float*,float*,const float*,int,bool) noexcept;
void fxWindowAddAVX2(float*,const float*,const float*,int) noexcept;
void advancePhasesAVX2(double*,const double*,int) noexcept;
void interpolateAVX2(float*,const float*,const float*,const float*,const float*,const float*,float,int) noexcept;
inline bool hasAVX2() noexcept {
    static const bool supported=[](){
        int regs[4]{};__cpuid(regs,0);if(regs[0]<7)return false;
        __cpuidex(regs,1,0);
        constexpr int required=(1<<27)|(1<<28);
        if((regs[2]&required)!=required)return false;
        if((_xgetbv(0)&6)!=6)return false;
        __cpuidex(regs,7,0);return (regs[1]&(1<<5))!=0;
    }();return supported;
}
#else
inline bool hasAVX2() noexcept{return false;}
#endif

inline const char* phaseBackend() noexcept {
#if defined(__aarch64__)
 return "NEON";
#elif defined(__SSE2__) || defined(_M_X64)
 return hasAVX2()?"AVX2":"SSE2";
#else
 return "scalar";
#endif
}
// mode: 0 scalar, 1 native SSE2/NEON, 2 auto AVX2 where supported.
inline const char* interpolationBackend(int mode)noexcept{
 if(mode==0)return "scalar";
#if defined(__aarch64__)
 return "NEON";
#elif defined(__SSE2__) || defined(_M_X64)
 return mode==2&&hasAVX2()?"AVX2":"SSE2";
#else
 return "scalar";
#endif
}
inline void interpolateAdd(float* out,const float* a,const float* aa,const float* b,const float* bb,const float* t,float morph,int n,int mode)noexcept{
#if defined(COSMOS_AVX2_DISPATCH) && defined(_M_X64)
 if(mode==2&&hasAVX2()){interpolateAVX2(out,a,aa,b,bb,t,morph,n);return;}
#endif
 int i=0;
#if defined(__aarch64__)
 if(mode!=0){
   auto m=vdupq_n_f32(morph);
   for(;i+3<n;i+=4){
     auto p=vld1q_f32(t+i),x=vld1q_f32(a+i),y=vld1q_f32(b+i);
     x=vaddq_f32(x,vmulq_f32(vsubq_f32(vld1q_f32(aa+i),x),p));
     y=vaddq_f32(y,vmulq_f32(vsubq_f32(vld1q_f32(bb+i),y),p));
     auto z=vaddq_f32(x,vmulq_f32(vsubq_f32(y,x),m));
     vst1q_f32(out+i,vaddq_f32(vld1q_f32(out+i),z));
   }
 }
#elif defined(__SSE2__) || defined(_M_X64)
 if(mode!=0){
   auto m=_mm_set1_ps(morph);
   for(;i+3<n;i+=4){
     auto p=_mm_loadu_ps(t+i),x=_mm_loadu_ps(a+i),y=_mm_loadu_ps(b+i);
     x=_mm_add_ps(x,_mm_mul_ps(_mm_sub_ps(_mm_loadu_ps(aa+i),x),p));
     y=_mm_add_ps(y,_mm_mul_ps(_mm_sub_ps(_mm_loadu_ps(bb+i),y),p));
     auto z=_mm_add_ps(x,_mm_mul_ps(_mm_sub_ps(y,x),m));
     _mm_storeu_ps(out+i,_mm_add_ps(_mm_loadu_ps(out+i),z));
   }
 }
#endif
 for(;i<n;++i){float x=a[i]+(aa[i]-a[i])*t[i],y=b[i]+(bb[i]-b[i])*t[i];out[i]+=x+(y-x)*morph;}
}
// Interleaved real/imaginary pairs. A and B are disjoint FFT half-blocks.
inline void fxButterfly(float* a,float* b,const float* w,int count,bool inverse,int mode)noexcept{
#if defined(COSMOS_AVX2_DISPATCH) && defined(_M_X64)
 if(mode==2&&count>=4&&hasAVX2()){fxButterflyAVX2(a,b,w,count,inverse);return;}
#endif
 int j=0;
#if defined(__aarch64__)
 if(mode){for(;j+3<count;j+=4){auto x=vld2q_f32(a+2*j),y=vld2q_f32(b+2*j),q=vld2q_f32(w+2*j);if(inverse)q.val[1]=vnegq_f32(q.val[1]);auto vr=vsubq_f32(vmulq_f32(y.val[0],q.val[0]),vmulq_f32(y.val[1],q.val[1])),vi=vaddq_f32(vmulq_f32(y.val[0],q.val[1]),vmulq_f32(y.val[1],q.val[0]));float32x4x2_t plus,minus;plus.val[0]=vaddq_f32(x.val[0],vr);plus.val[1]=vaddq_f32(x.val[1],vi);minus.val[0]=vsubq_f32(x.val[0],vr);minus.val[1]=vsubq_f32(x.val[1],vi);vst2q_f32(a+2*j,plus);vst2q_f32(b+2*j,minus);}}
#elif defined(__SSE2__) || defined(_M_X64)
 if(mode){const auto signs=_mm_setr_ps(-1,1,-1,1);for(;j+1<count;j+=2){auto x=_mm_loadu_ps(a+2*j),y=_mm_loadu_ps(b+2*j),q=_mm_loadu_ps(w+2*j);auto wr=_mm_shuffle_ps(q,q,_MM_SHUFFLE(2,2,0,0)),wi=_mm_shuffle_ps(q,q,_MM_SHUFFLE(3,3,1,1));if(inverse)wi=_mm_sub_ps(_mm_setzero_ps(),wi);auto v=_mm_add_ps(_mm_mul_ps(y,wr),_mm_mul_ps(_mm_mul_ps(_mm_shuffle_ps(y,y,_MM_SHUFFLE(2,3,0,1)),wi),signs));_mm_storeu_ps(a+2*j,_mm_add_ps(x,v));_mm_storeu_ps(b+2*j,_mm_sub_ps(x,v));}}
#endif
 for(;j<count;++j){int i=2*j;float wr=w[i],wi=inverse?-w[i+1]:w[i+1],vr=b[i]*wr-b[i+1]*wi,vi=b[i]*wi+b[i+1]*wr,ar=a[i],ai=a[i+1];a[i]=ar+vr;a[i+1]=ai+vi;b[i]=ar-vr;b[i+1]=ai-vi;}
}
inline void fxWindowAdd(float* out,const float* signal,const float* window,int n,int mode)noexcept{
#if defined(COSMOS_AVX2_DISPATCH) && defined(_M_X64)
 if(mode==2&&hasAVX2()){fxWindowAddAVX2(out,signal,window,n);return;}
#endif
 int i=0;
#if defined(__aarch64__)
 if(mode){const auto g=vdupq_n_f32(2.f/3.f);for(;i+3<n;i+=4){auto x=vmulq_f32(vld1q_f32(signal+i),vld1q_f32(window+i));vst1q_f32(out+i,vaddq_f32(vld1q_f32(out+i),vmulq_f32(x,g)));}}
#elif defined(__SSE2__) || defined(_M_X64)
 if(mode){const auto g=_mm_set1_ps(2.f/3.f);for(;i+3<n;i+=4){auto x=_mm_mul_ps(_mm_loadu_ps(signal+i),_mm_loadu_ps(window+i));_mm_storeu_ps(out+i,_mm_add_ps(_mm_loadu_ps(out+i),_mm_mul_ps(x,g)));}}
#endif
 for(;i<n;++i)out[i]+=signal[i]*window[i]*(2.f/3.f);
}
inline void advancePhases(double* phase,const double* delta,int n) noexcept {
#if defined(COSMOS_AVX2_DISPATCH) && defined(_M_X64)
 if(hasAVX2()){advancePhasesAVX2(phase,delta,n);return;}
#endif
 int i=0;
#if defined(__aarch64__)
 const auto one=vdupq_n_f64(1.);
 for(;i+1<n;i+=2){
   auto x=vaddq_f64(vld1q_f64(phase+i),vld1q_f64(delta+i));
   auto mask=vcgeq_f64(x,one);
   x=vbslq_f64(mask,vsubq_f64(x,one),x);vst1q_f64(phase+i,x);
 }
#elif defined(__SSE2__) || defined(_M_X64)
 const auto one=_mm_set1_pd(1.);
 for(;i+1<n;i+=2){
   auto x=_mm_add_pd(_mm_loadu_pd(phase+i),_mm_loadu_pd(delta+i));
   auto mask=_mm_cmpge_pd(x,one);
   x=_mm_sub_pd(x,_mm_and_pd(mask,one));_mm_storeu_pd(phase+i,x);
 }
#endif
 for(;i<n;++i){phase[i]+=delta[i];if(phase[i]>=1.)phase[i]-=1.;}
}
}
