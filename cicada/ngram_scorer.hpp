// -*- mode: c++ -*-
//
//  Copyright(C) 2013 Taro Watanabe <taro.watanabe@nict.go.jp>
//

//
// ngram scorer inspired by kenlm's left.hh which is described in
//
// @InProceedings{heafield:2011:WMT,
//  author    = {Heafield, Kenneth},
//  title     = {KenLM: Faster and Smaller Language Model Queries},
//  booktitle = {Proceedings of the Sixth Workshop on Statistical Machine Translation},
//  month     = {July},
//  year      = {2011},
//  address   = {Edinburgh, Scotland},
//  publisher = {Association for Computational Linguistics},
//  pages     = {187--197},
//  url       = {http://www.aclweb.org/anthology/W11-2123}
// }
//
// and
//
// @InProceedings{heafield-koehn-lavie:2012:EMNLP-CoNLL,
//   author    = {Heafield, Kenneth  and  Koehn, Philipp  and  Lavie, Alon},
//   title     = {Language Model Rest Costs and Space-Efficient Storage},
//   booktitle = {Proceedings of the 2012 Joint Conference on Empirical Methods in Natural Language Processing and Computational Natural Language Learning},
//   month     = {July},
//   year      = {2012},
//   address   = {Jeju Island, Korea},
//   publisher = {Association for Computational Linguistics},
//   pages     = {1169--1178},
//   url       = {http://www.aclweb.org/anthology/D12-1107}
// }

#ifndef __EXPGRAM__NGRAM_SCORER__HPP__
#define __EXPGRAM__NGRAM_SCORER__HPP__ 1

#include <vector>

#include <cicada/symbol.hpp>
#include <cicada/vocab.hpp>

#include <cicada/ngram.hpp>
#include <cicada/ngram_state.hpp>
#include <cicada/ngram_state_chart.hpp>

namespace cicada
{

  struct NGramScorer
  {
    typedef size_t             size_type;
    typedef ptrdiff_t          difference_type;
    
    typedef Symbol             symbol_type;
    typedef Symbol             word_type;
    typedef Vocab              vocab_type;
    typedef float              logprob_type;

    typedef NGram           ngram_type;
    typedef NGramStateChart ngram_state_type;

    typedef std::vector<char, std::allocator<char> > buffer_type;

    struct score_type
    {
      score_type() : prob_(0), bound_(0) {}
      score_type(const double& prob) : prob_(prob), bound_(0) {}
      score_type(const double& prob, const double& bound) : prob_(prob), bound_(bound) {}

      void clear()
      {
	prob_  = 0;
	bound_ = 0;
      }
      
      score_type& operator+=(const score_type& x)
      {
	prob_  += x.prob_;
	bound_ += x.bound_;
	return *this;
      }
      
      score_type& operator-=(const score_type& x)
      {
	prob_  -= x.prob_;
	bound_ -= x.bound_;
	return *this;
      }

      double prob_;
      double bound_;
    };

    NGramScorer()
      : ngram_state_(), ngram_(), state_(0), score_(), complete_(false)
    { }

    NGramScorer(const ngram_type& ngram)
      : ngram_state_(ngram.index.order()), ngram_(&ngram), state_(0), score_(), complete_(false)
    {
      buffer1_.reserve(ngram_state_.suffix_.buffer_size());
      buffer2_.reserve(ngram_state_.suffix_.buffer_size());
      
      buffer1_.resize(ngram_state_.suffix_.buffer_size());
      buffer2_.resize(ngram_state_.suffix_.buffer_size());      
    }
    
    NGramScorer(const ngram_type& ngram, void* state)
      : ngram_state_(ngram.index.order()), ngram_(&ngram), state_(state), score_(), complete_(false)
    {
      ngram_state_.size_prefix(state_) = 0;
      ngram_state_.size_suffix(state_) = 0;
      ngram_state_.complete(state_) = false;
      
      buffer1_.reserve(ngram_state_.suffix_.buffer_size());
      buffer2_.reserve(ngram_state_.suffix_.buffer_size());
      
      buffer1_.resize(ngram_state_.suffix_.buffer_size());
      buffer2_.resize(ngram_state_.suffix_.buffer_size());
    }

    void assign(const ngram_type& ngram)
    {
      // assign ngram language model
      ngram_ = &ngram;
      
      // assign state representation
      ngram_state_ = ngram_state_type(ngram.index.order());
      
      buffer1_.reserve(ngram_state_.suffix_.buffer_size());
      buffer2_.reserve(ngram_state_.suffix_.buffer_size());
      
      buffer1_.resize(ngram_state_.suffix_.buffer_size());
      buffer2_.resize(ngram_state_.suffix_.buffer_size());      
    }
    
    void assign(void* state)
    {
      state_    = state;
      score_.clear();
      complete_ = false;
      
      ngram_state_.size_prefix(state_) = 0;
      ngram_state_.size_suffix(state_) = 0;
      ngram_state_.complete(state_) = false;
    }

    size_type buffer_size() const
    {
      return ngram_state_.buffer_size();
    }
    
    void initial_bos()
    {
      const word_type::id_type bos_id = ngram_->index.vocab()[vocab_type::BOS];
      
      ngram_->lookup_context(&bos_id, (&bos_id) + 1, ngram_state_.suffix(state_));
      complete_ = true;
    }
    
    void initial_bos(const void* buffer)
    {
      // copy suffix state...
      ngram_state_.suffix_.copy(ngram_state_.suffix(buffer), ngram_state_.suffix(state_));
      complete_ = true;
    }
    
    template <typename Word_>
    void terminal(const Word_& word)
    {
      void* state_prev = &(*buffer1_.begin());
      
      ngram_state_.suffix_.copy(ngram_state_.suffix(state_), state_prev);
      
      const ngram_type::result_type result = ngram_->ngram_score(state_prev, word, ngram_state_.suffix(state_));
      
      if (complete_ || result.complete) {
	score_.prob_ += result.prob;
	
	complete_ = true;
      } else {
	score_.bound_ += result.bound;
	
	ngram_state_.state(state_)[ngram_state_.size_prefix(state_)] = result.state;
	++ ngram_state_.size_prefix(state_);
	
	// if not incremental, this is a complete state!
	if (ngram_state_.size_suffix(state_) != ngram_state_.suffix_.size(state_prev) + 1)
	  complete_ = true;
      }
    }

    // special handling for left-most non-terminal
    void initial_non_terminal(const void* antecedent)
    {
      ngram_state_.copy(antecedent, state_);
      
      complete_ = ngram_state_.complete(antecedent);
    }
    
    void non_terminal(const void* antecedent)
    {
      // antecedent has no prefix for re-scoring...
      if (ngram_state_.size_prefix(antecedent) == 0) {
	
	// if this antecedent is complete, we will copy suffix from antecedent to state_.
	// then, all the backoffs in the current suffix are accumulated.
	if (ngram_state_.complete(antecedent)) {
	  // score all the backoff
	  const logprob_type* biter     = ngram_state_.backoff(state_);
	  const logprob_type* biter_end = biter + ngram_state_.size_suffix(state_);
	  for (/**/; biter < biter_end; ++ biter)
	    score_.prob_ += *biter;
	  
	  // copy suffix state
	  ngram_state_.copy_suffix(antecedent, state_);
	  
	  // this is a complete scoring
	  complete_ = true;
	}
	
	return;
      }
      
      // we have no context as a suffix
      if (ngram_state_.size_suffix(state_) == 0) {
	// copy suffix state from antecedent
	ngram_state_.copy_suffix(antecedent, state_);
	
	if (complete_) {
	  // since we are a complete state, all the bound scoring in the antecedent's prefix should
	  // be upgraded to probability scoring, and complete it.
	  
	  ngram_->ngram_update_score(ngram_state_.state(antecedent),
				     ngram_state_.state(antecedent) + ngram_state_.size_prefix(antecedent),
				     1,
				     score_.prob_,
				     score_.bound_);
	} else if (ngram_state_.size_prefix(state_) == 0) {
	  // if prefix is empty, we aill also copy from antecedent
	  ngram_state_.copy_prefix(antecedent, state_);
	  
	  complete_ = ngram_state_.complete(antecedent);
	} else
	  complete_ = true;
	
	return;
      }
      
      // temporary storage...
      void* suffix_curr = &(*buffer1_.begin());
      void* suffix_next = &(*buffer2_.begin());
      
      const ngram_type::state_type* states = ngram_state_.state(antecedent);
      const size_type               states_length = ngram_state_.size_prefix(antecedent);
      
      for (size_type order = 1; order <= states_length; ++ order) {
	const ngram_type::result_type result = ngram_->ngram_partial_score(order == 1 ? ngram_state_.suffix(state_) : suffix_curr,
									   states[order - 1],
									   order,
									   suffix_next);
	
	if (complete_ || result.complete) {
	  score_.prob_  += result.prob;
	  score_.bound_ -= result.prev;
	  
	  complete_ = true;
	} else {
	  score_.bound_ += result.bound;
	  score_.bound_ -= result.prev;
	  
	  ngram_state_.state(state_)[ngram_state_.size_prefix(state_)] = result.state;
	  ++ ngram_state_.size_prefix(state_);
	}
	
	// do swap!
	std::swap(suffix_curr, suffix_next);
	
	// the original suffix length and the current suffix length is different... an indication of completion
	if (ngram_state_.suffix_.size(suffix_curr) != ngram_state_.size_suffix(state_)) {
	  complete_ = true;
	  
	  // we have finished all the scoring... and we do not have to score for the rest of the states
	  if (ngram_state_.suffix_.size(suffix_curr) == 0) {
	    ngram_state_.copy_suffix(antecedent, state_);
	    
	    // adjust rest-cost from antecedent.states + order, antecedent.states + length!

	    ngram_->ngram_update_score(ngram_state_.state(antecedent) + order,
				       ngram_state_.state(antecedent) + ngram_state_.size_prefix(antecedent),
				       order + 1,
				       score_.prob_,
				       score_.bound_);
	    
	    return;
	  }
	}
      }
      
      // antecedent is a complete state
      if (ngram_state_.complete(antecedent)) {
	// score all the backoff..
	const logprob_type* biter     = ngram_state_.suffix_.backoff(suffix_curr);
	const logprob_type* biter_end = biter + ngram_state_.suffix_.size(suffix_curr);
	for (/**/; biter < biter_end; ++ biter)
	  score_.prob_ += *biter;
	
	// copy suffix state
	ngram_state_.copy_suffix(antecedent, state_);
	
	// this is a complete scoring
	complete_ = true;
	
	return;
      }
      
      // minimum suffix is already computed in the antecedent, and this is already minimum wrt prefix which
      // is already combined with the previous suffix...
      if (ngram_state_.size_suffix(antecedent) < ngram_state_.size_prefix(antecedent)) {
	ngram_state_.copy_suffix(antecedent, state_);
	return;
      }
      
      // append states...
      ngram_state_.suffix_.append(ngram_state_.suffix(antecedent), suffix_curr, ngram_state_.suffix(state_));
    }
    
    score_type complete()
    {
      // if the prefix is already reached the ngram order - 1, then, it is also complete
      ngram_state_.complete(state_) = complete_ || (ngram_state_.size_prefix(state_) == ngram_state_.suffix_.order_ - 1);
      
      // fill the state
      ngram_state_.fill(state_);
      
      return score_;
    }
    
    ngram_state_type  ngram_state_;
    
    const ngram_type* ngram_;
    void*             state_;
    score_type        score_;
    bool              complete_;
    
    buffer_type buffer1_;
    buffer_type buffer2_;
  };
};

#endif
