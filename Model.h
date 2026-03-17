#ifndef MODEL_H
#define MODEL_H
#include "vector"
#include "core/SolverTypes.h"
#include <cstdint>
#include <map>

using NSPACE::vec;
using NSPACE::lbool;
namespace openwbo {
  class Model :public std::vector<lbool>{
    using std::vector<lbool>::vector;
  public:
    Model(size_t length):std::vector<lbool>(length){};
    Model(const lbool* m, size_t length):std::vector<lbool>(length){
      for(int i = 0, n = size(); i < n; i++)
	(*this)[i] = m[i];
    }
    Model(const vec<lbool>&, const std::map<uint64_t, uint64_t>);
    Model(const vec<lbool>& m): 
      Model{m, std::map<uint64_t, uint64_t>{{0,m.size()}}}{}

    void copyTo(vec<lbool>& vc) const{
      vc.clear();
      for(const auto el: *this)
	vc.push(el);
    }
    uint64_t size() const {return std::vector<lbool>::size();}
    void print(bool compact=true, uint64_t size=0){
      if(!size)
	size = this->size();
      else
	size = (size > this->size())? this->size() : size;
      if(compact)
	for (uint64_t i = 0; i < size; i++){
	  if ((*this)[i] == l_True) printf("1");
	  else printf("0");
	}
      else
	for (uint64_t i = 0; i < size; i++){
	  if ((*this)[i] == l_True) printf("%lu ",i+1);
	  else printf("%ld ",-((int64_t)i+1));
	}
    }

  private:
    std::map<uint64_t, uint64_t> _intervals{};
  };

}

#endif
