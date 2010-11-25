// -*- mode: c++ -*-
//
//  Copyright(C) 2010 Taro Watanabe <taro.watanabe@nict.go.jp>
//

#ifndef __CICADA__EVAL__WLCS__HPP__
#define __CICADA__EVAL__WLCS__HPP__ 1

// weighted longest common subsequence, wlcs

#include <cicada/eval/f.hpp>

namespace cicada
{
  namespace eval
  {
    class WLCSScorer;

    class WLCS : public F
    {
      friend class WLCSScorer;
    };

    class WLCSScorerImpl;
    
    class WLCSScorer : public Scorer
    {
    public:
      typedef double count_type;
      
    private:
      typedef WLCSScorerImpl impl_type;
      typedef std::vector<impl_type*, std::allocator<impl_type*> >  impl_set_type;
  
    public:
      WLCSScorer(const double& __alpha) : impl(), alpha(__alpha) { }
      WLCSScorer(const WLCSScorer& x);
      ~WLCSScorer();
      WLCSScorer& operator=(const WLCSScorer& x);

    public:      
      bool error_metric() const { return false; }
      
      scorer_ptr_type clone() const { return scorer_ptr_type(new WLCSScorer(*this)); }
      
      void clear();
      
      void insert(const sentence_type& sentence);
      score_ptr_type score(const sentence_type& __sentence) const;
      
    private:
      impl_set_type impl;
      double alpha;
    };
    
  };
};


#endif
