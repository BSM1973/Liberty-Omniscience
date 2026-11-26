#pragma once
#if defined(__aarch64__)
 #include <arm_neon.h>
#elif defined(__SSE2__) || defined(_M_X64)
 #include <emmintrin.h>
#endif
namespace cosmos {
inline const char* phaseBackend() noexcept {
#if defined(__aarch64__)
 return "NEON";
#elif defined(__SSE2__) || defined(_M_X64)
 return "SSE2";
#else
 return "scalar";
#endif
}
inline void advancePhases(double* phase,const double* delta,int n) noexcept {
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
