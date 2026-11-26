#pragma once
#include "CosmosEngine.h"
#include <thread>
#include <mutex>
#include <condition_variable>
#include <array>
#include <vector>
#include <stdexcept>
// OFFLINE / BENCHMARK ONLY. Blocking rendezvous is not qualified for a DAW callback.
namespace cosmos {
class ParallelBench {
    static constexpr int maxWorkers=4;
    Engine& engine;
    int workers,capacity;
    std::array<int,voices> active{};int activeCount=0;
    bool skipInactive=false;
    int participating=1;
    std::array<std::vector<float>,voices> l,r;
    std::array<std::thread,maxWorkers-1> pool;
    std::mutex mutex;
    std::condition_variable start,done;
    unsigned generation=0;
    int remaining=0,n=0;
    bool stopping=false;
    const Patch* patch=nullptr;
    void runRange(int k) noexcept {
      for(int j=k;j<(skipInactive?activeCount:voices);j+=participating){
        int i=skipInactive?active[size_t(j)]:j;
        std::fill_n(l[i].data(),n,0.f);std::fill_n(r[i].data(),n,0.f);
        engine.renderRange(l[i].data(),r[i].data(),n,*patch,i,i+1);
      }
    }
    void loop(int k){
      unsigned seen=0;
      for(;;){
        {std::unique_lock lock(mutex);start.wait(lock,[&]{return stopping||generation!=seen;});if(stopping)return;seen=generation;if(k>=participating)continue;}
        runRange(k);
        {std::lock_guard lock(mutex);--remaining;}done.notify_one();
      }
    }
    void stop() noexcept{
      {std::lock_guard lock(mutex);stopping=true;}start.notify_all();
      for(auto& t:pool)if(t.joinable())t.join();
    }
public:
    ParallelBench(Engine& e,int count,int maxBlock,bool compact=false):engine(e),workers(std::clamp(count,1,maxWorkers)),capacity(maxBlock),skipInactive(compact){
      if(capacity<=0)throw std::invalid_argument("capacity");
      for(int i=0;i<voices;++i){l[i].resize(capacity);r[i].resize(capacity);}
      try{for(int k=1;k<workers;++k)pool[k-1]=std::thread([this,k]{loop(k);});}catch(...){stop();throw;}
    }
    ~ParallelBench(){stop();}
    ParallelBench(const ParallelBench&)=delete;
    ParallelBench& operator=(const ParallelBench&)=delete;
    void render(float* left,float* right,int count,const Patch& p){
      if(count<=0)return;
      if(count>capacity)throw std::invalid_argument("block exceeds capacity");
      {std::lock_guard lock(mutex);patch=&p;n=count;activeCount=0;
        if(skipInactive)for(int i=0;i<voices;++i)if(engine.voiceActive(i))active[size_t(activeCount++)]=i;
        if(skipInactive&&activeCount==0)return;
        if(skipInactive&&activeCount==1){engine.render(left,right,count,p);return;}
        participating=skipInactive?std::min(workers,activeCount):workers;
        remaining=participating-1;++generation;}
      start.notify_all();runRange(0);
      {std::unique_lock lock(mutex);done.wait(lock,[&]{return remaining==0;});}
      // Original voice order, even when workers finish in a different order.
      for(int k=0;k<(skipInactive?activeCount:voices);++k){int i=skipInactive?active[size_t(k)]:k;for(int j=0;j<count;++j){left[j]+=l[i][j];right[j]+=r[i][j];}}
    }
};
}
