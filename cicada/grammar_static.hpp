// -*- mode: c++ -*-
//
//  Copyright(C) 2010-2011 Taro Watanabe <taro.watanabe@nict.go.jp>
//

#ifndef __CICADA__GRAMMAR_STATIC__HPP__
#define __CICADA__GRAMMAR_STATIC__HPP__ 1

// static storage grammar...

#include <string>

#include <cicada/transducer.hpp>

#include <boost/filesystem/path.hpp>

namespace cicada
{
  
  struct GrammarStaticImpl;

  class GrammarStatic : public Transducer
  {
  private:
    typedef GrammarStaticImpl impl_type;
    
    typedef boost::filesystem::path path_type;
    
  public:
    // static grammar, use to encode rule-table generated by nicttm, moses, joshua etc.
    // 
    // [x] ||| [x,1] ... ||| [x,1] ... ||| score1 score2
    //
    // The parameter must be of form:
    // parameter = file-name:[key=value delimited by ',']*
    // where:
    // file-name: file name for the grammar, "-" for stdin
    // key,value: key, value pair... valid pairs are:
    //
    //    max-span = 15 : maximum non-terminals span
    // 
    //    feature0 = feature-name0
    //    feature1 = feature-name1
    //
    // if not supplied we will use rule-table-0, rule-table-1 etc.
    

    GrammarStatic(const std::string& parameter);
    ~GrammarStatic();

    GrammarStatic(const GrammarStatic& x);
    GrammarStatic& operator=(const GrammarStatic& x);

  private:
    GrammarStatic() {}
  public:
    // virtual members
    transducer_ptr_type clone() const;
    bool valid_span(int first, int last, int distance) const;
    id_type root() const;
    id_type next(const id_type& node, const symbol_type& symbol) const;
    bool has_next(const id_type& node) const;
    const rule_pair_set_type& rules(const id_type& node) const;

    // grammar_static specific members
    void quantize();
    void write(const path_type& path) const;
    
  private:
    impl_type* pimpl;
  };
  
};

#endif
