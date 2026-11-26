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
