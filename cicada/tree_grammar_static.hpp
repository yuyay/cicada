// -*- mode: c++ -*-

#ifndef __CICADA__TREE_GRAMMAR_STATIC__HPP__
#define __CICADA__TREE_GRAMMAR_STATIC__HPP__ 1

// static storage grammar...

#include <string>

#include <cicada/tree_transducer.hpp>

#include <boost/filesystem/path.hpp>

namespace cicada
{
  
  class TreeGrammarStaticImpl;

  class TreeGrammarStatic : public TreeTransducer
  {
  private:
    typedef TreeGrammarStaticImpl impl_type;
    
    typedef boost::filesystem::path path_type;
    
  public:
    // static grammar, use to encode rule-table generated by nicttm (probably...)
    // 
    //  ([x] ([x] (a)) (b) ([x,1]) (c)) ||| ([x] (A) ([x,1]) (B)) ||| score1 score2
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
    
    TreeGrammarStatic(const std::string& parameter);
    ~TreeGrammarStatic();
    
    TreeGrammarStatic(const TreeGrammarStatic& x);
    TreeGrammarStatic& operator=(const TreeGrammarStatic& x);

  private:
    TreeGrammarStatic() {}
    
  public:
    // virtual members
    transducer_ptr_type clone() const;
    id_type root() const;
    id_type next(const id_type& node, const symbol_type& symbol) const;
    id_type next_epsilon(const id_type& node) const;
    id_type next_comma(const id_type& node) const;
    id_type next_delimitter(const id_type& node) const;
    bool has_next(const id_type& node) const;
    const rule_pair_set_type& rules(const id_type& node) const;

    // grammar_static specific members
    void quantize();
    void write(const path_type& path);
    
  private:
    impl_type* pimpl;
  };
  
};

#endif
