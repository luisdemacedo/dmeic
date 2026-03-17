#include "partition.h"
#include "FormulaPB.h"
#include <cstdint>

namespace partition{
  void logPart(typename MyPartition::part_t& p){
    
  }
  void filter(typename MyPartition::part_t& p, map<Lit, uint64_t>& terms){
    auto iter = terms.begin();
    while(iter != terms.end()){
      if(!p.count(iter->first))
	terms.erase(iter++);
      else
	      ++iter;
    }
  }

MyPartition mix(vector<MyPartition>&& partitions){
  MyPartition mix{};
  random_device rd{};
  mt19937 rng{rd()};
  uniform_int_distribution<int> uni(0,partitions.size()-1);
  int rnd{};
  while(partitions.size()){
    uniform_int_distribution<int> uni(0,partitions.size()-1);
    rnd = uni(rng);    
    mix.push(partitions[rnd].pop());

    if(partitions[rnd].size()==0){
      partitions[rnd]=partitions[partitions.size()-1];
      partitions.pop_back();
    }
  }
  return mix;
}




  std::ostream& operator<<(std::ostream& os, MyPartition& mp){
    return os;
  }
  MyPartition Partitioner::generate(){
    MyPartition res{};

    while(lits.size()){
      auto chead = head();
      MyPartition::part_t prt{};
      for(auto& el :chead)
	prt.insert(el.second);
      res.push(prt);
    }
    return res;
  }


  vector<std::pair<uint64_t,Lit>> Partitioner::head(){
    auto it = lits.begin();
    auto end = lits.end();
    int w_n = 0, n = 0;
    vector<std::pair<uint64_t,Lit>> prt;

    while(it!=end){
      auto ubit = lits.upper_bound(it->first);
      while(it!=ubit){
	prt.push_back(*it);
	it = lits.erase(it);
	n++;
      }
      w_n++;
      if(n / w_n >= param)
	return prt;
    }
    return prt;
  }
  uint64_t Partitioner::threshold(const head_t& head){
    return mean(head);
  }
  uint64_t Partitioner::mode(const head_t& head){
    std::map<uint64_t, uint64_t> cmap{};
    for(const auto& el: head)
      cmap[el.first]++;

    uint64_t res = 0;
    uint64_t resn = 0;

    for(const auto& el: cmap)
      if(resn < el.second){
	resn = el.second;
	res = el.first;
      }
    return res;
  }
  uint64_t Partitioner::min(const head_t& head){
    return head.rbegin()->first;
  }

  uint64_t Partitioner::mean(const head_t& head){
    uint64_t total{};
    for(const auto& el: head)
      total += el.first;
    return total/head.size();
  }
  
  PBObjFunction  Partitioner::headUnitary() {
    int64_t shift = 0;
    auto chead = head();
    // if head is empty, return empty function.
    if(!chead.size())
      return PBObjFunction{};
    auto m = threshold(chead);
    //function to be filled
    vec<Lit> pblits;
    vec<uint64_t> coeffs;
    for(auto& el :chead){
      auto cur = el.first;
      int64_t remain = cur % m;
      auto proxy = (cur / m) * m;
      if(!proxy){
	proxy = m;
	remain = (cur - proxy);
      }
      coeffs.push(proxy / m);
      pblits.push(el.second);
      if(remain > 0)
	lits.insert({remain,el.second});
      else
	if(remain < 0)
	  {
	    lits.insert({-remain,~el.second});
	    shift += remain;//TODO
	  }
    }
    return PBObjFunction{pblits,coeffs,shift,(int64_t) m};
  }

  PBObjFunction Partitioner::tail(){
    vec<Lit> pblits;
    vec<uint64_t> coeffs;
    for(auto& el :lits){
      coeffs.push(el.first);
      pblits.push(el.second);
    }
    return PBObjFunction{pblits,coeffs,0};

  }

  void Partitioner::reset_terms(){
    lits = lits_copy;
  } 
  void Partitioner::bump_param(){
    param += quantum;
    std::cout<<"c new parameter: "<< param<<std::endl;
  }
}
