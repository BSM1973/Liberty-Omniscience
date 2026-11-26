#include <immintrin.h>
namespace cosmos {
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
}
