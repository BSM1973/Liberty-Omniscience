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
