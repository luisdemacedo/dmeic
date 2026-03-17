#ifndef ROOTLITSINT_H
#define ROOTLITSINT_H

#include "core/SolverTypes.h"
#include <cstdint>
#include <iterator>
#include <memory>
#include <utility>
#include "RootLitsTypes.h"
#include "RootLitsIterator.h"
namespace rootLits{
  // Maps values to corresponding order variables.
  class RootLitsInt {
  public: 
    // virtual Iterator begin(uint64_t i) = 0;
    // virtual Iterator end(uint64_t i) = 0;
    map<uint64_t, bool> eval(const Model& m){
      map<uint64_t, bool> res;  
      for(const auto& el: *this)
	res.insert({el.first, m[Glucose::var(el.second)] == l_True? true: false });
      return res;
    }



    virtual Iterator begin() = 0;
    virtual Iterator end() = 0;
    virtual CIterator cbegin() const = 0;
    virtual CIterator cend() const = 0;
    // given value v, returns variable x such that x => f < v;
    virtual Iterator at_key(uint64_t) = 0;
    // given index i, return corresponding variable;
    virtual value_t at(uint64_t) = 0;
    virtual value_t operator[](uint64_t) = 0;
    virtual uint64_t size() const = 0;
    virtual void insert(uint64_t n, value_t) = 0;
    virtual void push(value_t) = 0;
    virtual void clear() = 0;
  };

  class  RootLitsAdaptor: RootLitsInt{
    Iterator begin(uint64_t i);
    Iterator end(uint64_t i);
    Iterator begin() override;
    Iterator end() override;
    Iterator at_key(uint64_t) override;
    value_t at(uint64_t) override;
    value_t operator[](uint64_t) override;
  };

}

#endif
