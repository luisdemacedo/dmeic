#ifndef WAITING_LIST_H
#define WAITING_LIST_H
#include "../MOCO.h"
#include "../Pareto.h"
#include <queue>
#include <set>
#include <stdexcept>
#include <vector>
namespace waiting_list {
  using namespace openwbo;
  enum class Types {
    Stack = 0,
    Queue = 1,
    PriorityQueue = 2,
    List = 3,
  };
  
  template <bool lower=true, bool ascend=!lower> struct YPoint_priority: YPoint{
    int count = 0;
    YPoint_priority(const YPoint& yp, int note): YPoint{yp}, count{note}{}
    friend bool operator<(const YPoint_priority& a,const YPoint_priority& b){
      if(a.count < b.count)
	return false;
      if(a.count > b.count)
	return true;
      // implies first elements have low hv score  
      int64_t a_score = 0;
      int64_t b_score = 0;
      if constexpr (lower){
	a_score = pareto::hv_shift(pareto::min, a);
	b_score =  pareto::hv_shift(pareto::min, b);
      }
      else
	{
	  a_score = pareto::hv_shift(a, pareto::max);
	  b_score =  pareto::hv_shift(b, pareto::max);
	}
      if constexpr (!ascend)
	return a_score < b_score;
      else
	return a_score > b_score;
    }      
  };

  class WaitingListI{
  public:
    virtual YPoint pop() = 0;
    virtual void insert(const YPoint& yp, bool force=false, int note=0) = 0;
    virtual void unpop(int note=0) {insert(popped, true, note);}
    virtual void reset() {}
    virtual int size() = 0;
    virtual void report() {};
  protected:
    int max_size = 0;
    YPoint popped{};
  };
  class Queue: public WaitingListI{
  public:
    Queue(): queue(){};
    YPoint pop() override {
      popped = queue.front();
      queue.pop();
      return popped;
    }
    int size() override{return queue.size();}
    void insert(const YPoint& yp, bool force=false, int note=0) override {
      auto a = set.insert(yp);
      if(a.second || force){
	queue.push(yp);
	auto sz = size();
	if(sz > max_size)
	  max_size = sz;
      }
    }
    void report() override {
      std::vector<YPoint> v;
      v.reserve(size());
      std::cout<<"c waiting list statistics"<<endl;
      std::cout<<"c size: "<<size()<<endl;
      std::cout<<"c max_size: "<<max_size<<endl;
      while(size())
	v.push_back(pop());
      reverse(v.begin(), v.end());
      for(const auto& el: v){
	std::cout<<"c "<<el<<endl;
	insert(el);
      }
    };
  private:
    std::queue<YPoint> queue;
    std::set<YPoint> set;
  };
  class Stack: public WaitingListI{
  public:
    Stack(): stack{}{};
    YPoint pop() override {
      popped = *stack.rbegin();
      stack.pop_back();
      return popped;
    }
    int size() override {return stack.size();}
    void insert (const YPoint& yp, bool force=false, int note=0) override {
      auto a = set.insert(yp);
      if(a.second || force){
	stack.push_back(yp);
	auto sz = size();
	if(sz > max_size)
	  max_size = sz;
      }
    }
    void report() override {
      std::vector<YPoint> v;
      v.reserve(size());
      std::cout<<"c waiting list statistics"<< endl;
      std::cout<<"c size:"<<size()<<endl;
      std::cout<<"c max_size: "<<max_size<<endl;
      while(size())
	v.push_back(pop());
      reverse(v.begin(), v.end());
      for(const auto& el: v){
	std::cout<<"c "<<el<<endl;
	insert(el);
      }
    }
  private:
    std::vector<YPoint> stack;
    std::set<YPoint> set;
  };
  // polarity false: large hv first. polarity true: small hv first
  template <bool lower=false, bool ascend=false> class PriorityQueue: public WaitingListI{
    using  element_t = YPoint_priority<lower, ascend>;

  public:
    PriorityQueue(): queue{}{};
    YPoint pop() override {
      auto x = queue.top();
      popped_count = x.count;
      popped = x;
      queue.pop();
      return popped;
    }
    int size() override {return queue.size();}

    void insert(const YPoint& yp, bool force=false, int note=0) override {
      auto a = set.insert(yp);
      if(a.second || force){
	queue.push({yp, note});
	auto sz = size();
	if(sz > max_size)
	  max_size = sz;
      }
    }
    void reset() override{
      queue = std::priority_queue<element_t, std::vector<element_t>>{};
    }
    void report() override {
      std::vector<YPoint> v;
      v.reserve(size());
      std::cout<<"c waiting list statistics"<< endl;
      std::cout<<"c size:"<<size()<<endl;
      std::cout<<"c max_size: "<<max_size<<endl;
      while(size())
	v.push_back(pop());
      reverse(v.begin(), v.end());
      for(const auto& el: v){
	std::cout<<"c "<<el<<endl;
	insert(el);
      }
    }
    virtual void unpop(int note=0) override {insert(popped, true, note + popped_count);}
      
  private:
    std::priority_queue<element_t, std::vector<element_t>> queue{};
    int popped_count = 0;
    std::set<YPoint> set;
  };
  template <bool lower=false, bool ascend=false>  class List: public WaitingListI{
    using  element_t = YPoint_priority<lower, ascend>;

  public:
    List(): list{}{};
    YPoint pop() override {
      if(!list.size())
	throw std::runtime_error("empty list can't be popped");
      auto x = *--list.end();
      popped_count = x.count;
      popped = x;
      list.pop_back();
      auto max = std::max_element(list.begin(), list.end());
	if(max != list.end() && list.size()){
	  auto last = --list.end();
	  if(max != last)
	    std::iter_swap(max, last);
	}
      return popped;
    }
    int size() override {return list.size();}
    void insert (const YPoint& yp, bool force=false, int note=0) override {
      auto a = set.insert(yp);
      if(a.second || force){
	list.push_back({yp, note});
	if(list.size() == 1)
	  return;
	auto best = ++list.rbegin();
	// place largest element in sufix list at the end
	if(*list.rbegin() < *best)
	  std::iter_swap(best, list.rbegin());
	auto sz = size();
	if(sz > max_size)
	  max_size = sz;
      }
    }
    void report() override {
      std::vector<YPoint> v;
      v.reserve(size());
      std::cout<<"c waiting list statistics"<< endl;
      std::cout<<"c size:"<<size()<<endl;
      std::cout<<"c max_size: "<<max_size<<endl;
      while(size())
	v.push_back(pop());
      reverse(v.begin(), v.end());
      for(const auto& el: v){
	std::cout<<"c "<<el<<endl;
	insert(el);
      }
    }
    virtual void unpop(int note=0) override {insert(popped, true, note + popped_count);}
    void reset() override{
      decltype(list) list1;
      for(const auto& el: list)
	if(set.count(el))
	  list1.push_back(el);
      list=std::move(list1);
}
  private:
    std::vector<element_t> list;
    std::set<YPoint> set;
    int popped_count = 0;
 };

  unique_ptr<WaitingListI> construct(int wl_type, bool lower, bool ascend);

}
#endif
