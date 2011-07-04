//
//  Copyright(C) 2009-2011 Taro Watanabe <taro.watanabe@nict.go.jp>
//

#ifndef __CICADA_LEXICON_HMM_IMPL__HPP__
#define __CICADA_LEXICON_HMM_IMPL__HPP__ 1

#include <numeric>

#include "cicada_lexicon_impl.hpp"

#include "utils/vector2_aligned.hpp"
#include "utils/vector3_aligned.hpp"
#include "utils/mathop.hpp"
#include "utils/aligned_allocator.hpp"

#include "kuhn_munkres.hpp"
#include "itg_alignment.hpp"

struct LearnHMM : public LearnBase
{
  LearnHMM(const LearnBase& __base)
    : LearnBase(__base) {}
  

  struct HMMData
  {
    typedef utils::vector2_aligned<prob_type, utils::aligned_allocator<prob_type> > forward_type;
    typedef utils::vector2_aligned<prob_type, utils::aligned_allocator<prob_type> > backward_type;
    
    typedef utils::vector2_aligned<prob_type, utils::aligned_allocator<prob_type> > emission_type;
    typedef utils::vector3_aligned<prob_type, utils::aligned_allocator<prob_type> > transition_type;
    
    typedef std::vector<prob_type, utils::aligned_allocator<prob_type> > scale_type;
    
    typedef utils::vector2_aligned<prob_type, utils::aligned_allocator<prob_type> > posterior_type;
    
    forward_type    forward;
    backward_type   backward;
    
    emission_type   emission;
    transition_type transition;
    
    scale_type scale;
    
    posterior_type posterior;
    
    sentence_type source;
    sentence_type target;
    sentence_type source_class;
    sentence_type target_class;
        
    void prepare(const sentence_type& __source,
		 const sentence_type& __target,
		 const ttable_type& ttable,
		 const atable_type& atable,
		 const classes_type& classes_source,
		 const classes_type& classes_target)
    {
      const size_type source_size = __source.size();
      const size_type target_size = __target.size();
      
      const double prob_null  = p0;
      const double prob_align = 1.0 - p0;

      source.clear();
      target.clear();
      
      source.reserve((source_size + 2) * 2);
      target.reserve((target_size + 2) * 2);
      source.resize((source_size + 2) * 2, vocab_type::NONE);
      target.resize((target_size + 2) * 2, vocab_type::NONE);
      
      source[0] = vocab_type::BOS;
      target[0] = vocab_type::BOS;
      source[(source_size + 2) - 1] = vocab_type::EOS;
      target[(target_size + 2) - 1] = vocab_type::EOS;
      
      std::copy(__source.begin(), __source.end(), source.begin() + 1);
      std::copy(__target.begin(), __target.end(), target.begin() + 1);
      
      source_class.reserve(source_size + 2);
      target_class.reserve(target_size + 2);
      source_class.resize(source_size + 2);
      target_class.resize(target_size + 2);
      
      source_class[0] = vocab_type::BOS;
      target_class[0] = vocab_type::BOS;
      source_class[(source_size + 2) - 1] = vocab_type::EOS;
      target_class[(target_size + 2) - 1] = vocab_type::EOS;
      
      sentence_type::iterator csiter = source_class.begin() + 1;
      for (sentence_type::const_iterator siter = __source.begin(); siter != __source.end(); ++ siter, ++ csiter)
	*csiter = classes_source[*siter];
      
      sentence_type::iterator ctiter = target_class.begin() + 1;
      for (sentence_type::const_iterator titer = __target.begin(); titer != __target.end(); ++ titer, ++ ctiter)
	*ctiter = classes_target[*titer];
      
      emission.clear();
      transition.clear();
      
      emission.reserve(target_size + 2, (source_size + 2) * 2);
      transition.reserve(target_size + 2, (source_size + 2) * 2, (source_size + 2) * 2);
      
      emission.resize(target_size + 2, (source_size + 2) * 2, 0.0);
      transition.resize(target_size + 2, (source_size + 2) * 2, (source_size + 2) * 2, 0.0);
      
      // compute emission table...
      emission(0, 0) = 1.0;
      emission(target_size + 2 - 1, source_size + 2 - 1) = 1.0;
      for (int trg = 1; trg <= static_cast<int>(target_size); ++ trg) {
	
	// translation into non-NULL word
	prob_type* eiter = &(*emission.begin(trg)) + 1;
	for (size_type src = 1; src <= source_size; ++ src, ++ eiter)
	  (*eiter) = ttable(source[src], target[trg]);
	
	prob_type* eiter_first = &(*emission.begin(trg)) + source_size + 2;
	prob_type* eiter_last  = eiter_first + source_size + 2 - 1; // -1 to exclude EOS
	
	std::fill(eiter_first, eiter_last, ttable(vocab_type::NONE, target[trg]));
      }

      
      // compute transition table...
      // we start from 1, since there exists no previously aligned word before BOS...
      for (int trg = 1; trg < target_size + 2; ++ trg) {
	// alignment into non-null
	// start from 1 to exlude transition into <s>
	for (int next = 1; next < source_size + 2; ++ next) {
	  prob_type* titer1 = &(*transition.begin(trg, next)); // from word
	  prob_type* titer2 = titer1 + (source_size + 2);      // from NONE
	  
	  // - 1 to exclude previous </s>...
	  for (int prev = 0; prev < source_size + 2 - 1; ++ prev, ++ titer1, ++ titer2) {
	    const prob_type prob = prob_align * atable(source_class[prev], target_class[trg],
						       source_size, target_size,
						       prev - 1, next - 1);
	    
	    *titer1 = prob;
	    *titer2 = prob;
	  }
	}
	
	// null transition
	// we will exclude EOS
	for (int next = 0; next < (source_size + 2) - 1; ++ next) {
	  const int next_none = next + (source_size + 2);
	  const int prev1_none = next;
	  const int prev2_none = next + (source_size + 2);
	  
	  transition(trg, next_none, prev1_none) = prob_null;
	  transition(trg, next_none, prev2_none) = prob_null;
	}
      }
    }
    
    void forward_backward(const sentence_type& __source,
			  const sentence_type& __target)
    {
      const size_type source_size = __source.size();
      const size_type target_size = __target.size();
      
      forward.clear();
      backward.clear();
      scale.clear();
      
      forward.reserve(target_size + 2, (source_size + 2) * 2);
      backward.reserve(target_size + 2, (source_size + 2) * 2);
      scale.reserve(target_size + 2);

      forward.resize(target_size + 2, (source_size + 2) * 2, 0.0);
      backward.resize(target_size + 2, (source_size + 2) * 2, 0.0);
      scale.resize(target_size + 2, 1.0);
      
      forward(0, 0) = 1.0;
      for (int trg = 1; trg < target_size + 2; ++ trg) {
	// +1 to exclude BOS
	prob_type*       niter = &(*forward.begin(trg)) + 1;
	const prob_type* eiter = &(*emission.begin(trg)) + 1;
	
	for (int next = 1; next < source_size + 2; ++ next, ++ niter, ++ eiter) {
	  const prob_type* piter = &(*forward.begin(trg - 1));
	  const prob_type* titer = &(*transition.begin(trg, next));
	  
	  const double factor = *eiter;
	  if (factor > 0.0) {
	    double accum[4] = {0.0, 0.0, 0.0, 0.0};
	    const int loop_size = (source_size + 2) * 2;
	    for (int prev = 0; prev < loop_size - 3; prev += 4) {
	      accum[0] += piter[prev + 0] * titer[prev + 0] * factor;
	      accum[1] += piter[prev + 1] * titer[prev + 1] * factor;
	      accum[2] += piter[prev + 2] * titer[prev + 2] * factor;
	      accum[3] += piter[prev + 3] * titer[prev + 3] * factor;
	    }
	    
	    switch (loop_size & 0x03) {
	    case 3: accum[4 - 3] += piter[loop_size - 3] * titer[loop_size - 3] * factor;
	    case 2: accum[4 - 2] += piter[loop_size - 2] * titer[loop_size - 2] * factor;
	    case 1: accum[4 - 1] += piter[loop_size - 1] * titer[loop_size - 1] * factor;
	    }
	    *niter += accum[0] + accum[1] + accum[2] + accum[3];
	  }
	}
	
	prob_type*       niter_none = &(*forward.begin(trg)) + (source_size + 2);
	const prob_type* eiter_none = &(*emission.begin(trg)) + (source_size + 2);
	const prob_type* piter_none1 = &(*forward.begin(trg - 1));
	const prob_type* piter_none2 = piter_none1 + (source_size + 2);
	
	for (int next = 0; next < source_size + 2 - 1; ++ next, ++ niter_none, ++ eiter_none, ++ piter_none1, ++ piter_none2) {
	  const int next_none = next + source_size + 2;
	  const int prev_none1 = next;
	  const int prev_none2 = next + source_size + 2;
	  
	  *niter_none += (*piter_none1) * (*eiter_none) * transition(trg, next_none, prev_none1);
	  *niter_none += (*piter_none2) * (*eiter_none) * transition(trg, next_none, prev_none2);
	}
	
	scale[trg] = std::accumulate(forward.begin(trg), forward.end(trg), 0.0);
	scale[trg] = (scale[trg] == 0.0 ? 1.0 : 1.0 / scale[trg]);
	if (scale[trg] != 1.0)
	  std::transform(forward.begin(trg), forward.end(trg), forward.begin(trg), std::bind2nd(std::multiplies<double>(), scale[trg]));
      }
      
      backward(target_size + 2 - 1, source_size + 2 - 1) = 1.0;
      
      for (int trg = target_size + 2 - 2; trg >= 0; -- trg) {
	const prob_type factor_scale = scale[trg];
	
	// +1 to exclude BOS
	const prob_type* niter = &(*backward.begin(trg + 1)) + 1;
	const prob_type* eiter = &(*emission.begin(trg + 1)) + 1;
	
	for (int next = 1; next < source_size + 2; ++ next, ++ niter, ++ eiter) {
	  prob_type*       piter = &(*backward.begin(trg));
	  const prob_type* titer = &(*transition.begin(trg + 1, next));
	  
	  const double factor = (*eiter) * (*niter) * factor_scale;
	  if (factor > 0.0) {
	    const int loop_size = (source_size + 2) * 2;
	    for (int prev = 0; prev < loop_size - 3; prev += 4) {
	      piter[prev + 0] += titer[prev + 0] * factor;
	      piter[prev + 1] += titer[prev + 1] * factor;
	      piter[prev + 2] += titer[prev + 2] * factor;
	      piter[prev + 3] += titer[prev + 3] * factor;
	    }
	    switch (loop_size & 0x03) {
	    case 3: piter[loop_size - 3] += titer[loop_size - 3] * factor;
	    case 2: piter[loop_size - 2] += titer[loop_size - 2] * factor;
	    case 1: piter[loop_size - 1] += titer[loop_size - 1] * factor;
	    }
	  }
	}
	
	const prob_type* niter_none = &(*backward.begin(trg + 1)) + (source_size + 2);
	const prob_type* eiter_none = &(*emission.begin(trg + 1)) + (source_size + 2);
	prob_type*       piter_none1 = &(*backward.begin(trg));
	prob_type*       piter_none2 = piter_none1 + (source_size + 2);
	
	for (int next = 0; next < source_size + 2 - 1; ++ next, ++ niter_none, ++ eiter_none, ++ piter_none1, ++ piter_none2) {
	  const int next_none = next + source_size + 2;
	  const int prev_none1 = next;
	  const int prev_none2 = next + source_size + 2;
	  
	  *piter_none1 += (*niter_none) * (*eiter_none) * transition(trg + 1, next_none, prev_none1) * factor_scale;
	  *piter_none2 += (*niter_none) * (*eiter_none) * transition(trg + 1, next_none, prev_none2) * factor_scale;
	}
      }
    }

    void estimate_posterior(const sentence_type& __source,
			    const sentence_type& __target)
    {
      const size_type source_size = __source.size();
      const size_type target_size = __target.size();
      
      posterior.clear();
      posterior.reserve(target_size + 1, source_size + 1);
      posterior.resize(target_size + 1, source_size + 1, 0.0);
      
      const prob_type sum = forward(target_size + 2 - 1, source_size + 2 - 1);
      
      for (int trg = 1; trg <= target_size; ++ trg) {
	const double factor = 1.0 / (scale[trg] * sum);
	
	// + 1 to skip BOS
	const prob_type* fiter = &(*forward.begin(trg)) + 1;
	const prob_type* biter = &(*backward.begin(trg)) + 1;
	prob_type* piter = &(*posterior.begin(trg)) + 1;
	for (int src = 1; src <= source_size; ++ src, ++ fiter, ++ biter, ++ piter) {
	  const prob_type count = (*fiter) * (*biter) * factor;
	  
	  if (std::isfinite(count) && count > 0.0)
	    (*piter) += count;
	}
	
	fiter = &(*forward.begin(trg)) + source_size + 2;
	biter = &(*backward.begin(trg)) + source_size + 2;
	prob_type count_none = 0.0;
	for (int src = 0; src < source_size + 2; ++ src, ++ fiter, ++ biter) {
	  const prob_type count = (*fiter) * (*biter) * factor;
	  
	  if (std::isfinite(count) && count > 0.0)
	    count_none += count;
	}
	posterior(trg, 0) = count_none;
      }
    }

    struct logminus
    {
      double operator()(const double& logsum, const double& prob) const
      {
	return logsum - utils::mathop::log(prob);
      }
    };
    
    double objective() const
    {
      return std::accumulate(scale.begin(), scale.end(), 0.0, logminus());
    }

    void accumulate(const sentence_type& __source,
		    const sentence_type& __target,
		    ttable_type& counts)
    {
      const size_type source_size = __source.size();
      const size_type target_size = __target.size();
      
      const prob_type sum = forward(target_size + 2 - 1, source_size + 2 - 1);
      
      // accumulate lexcion
      for (int trg = 1; trg <= target_size; ++ trg) {
	const double factor = 1.0 / (scale[trg] * sum);
      
	// + 1 to skip BOS
	const prob_type* fiter = &(*forward.begin(trg)) + 1;
	const prob_type* biter = &(*backward.begin(trg)) + 1;
	
	for (int src = 1; src < source_size + 2; ++ src, ++ fiter, ++ biter) {
	  const prob_type count = (*fiter) * (*biter) * factor;
	  
	  if (std::isfinite(count) && count > 0.0)
	    counts[source[src]][target[trg]] += count;
	}
      
	// null alignment...
	double count_none = 0.0;
	for (int src = 0; src < source_size + 2; ++ src, ++ fiter, ++ biter) {
	  const prob_type count = (*fiter) * (*biter) * factor;
	  if (std::isfinite(count) && count > 0.0)
	    count_none += count;
	}
      
	counts[vocab_type::NONE][target[trg]] += count_none;
      }
    }
    
    void accumulate(const sentence_type& __source,
		    const sentence_type& __target,
		    atable_type& counts)
    {
      typedef std::vector<atable_type::mapped_type*, std::allocator<atable_type::mapped_type*> > mapped_type;

      const size_type source_size = __source.size();
      const size_type target_size = __target.size();
      
      const double sum = forward(target_size + 2 - 1, source_size + 2 - 1);
      const double factor = 1.0 / sum;

      mapped_type mapped((source_size + 2) - 1);

      for (int trg = 1; trg < target_size + 2; ++ trg) {
	
	for (int prev = 0; prev < (source_size + 2) - 1; ++ prev)
	  mapped[prev] = &(counts[std::make_pair(source_class[prev], target_class[trg])]);
	
	const prob_type* biter = &(*backward.begin(trg)) + 1;
	const prob_type* eiter = &(*emission.begin(trg)) + 1;
	
	// + 1, we will exclude <s>, since we will never aligned to <s>
	for (int next = 1; next < source_size + 2; ++ next, ++ biter, ++ eiter) {
	  const prob_type factor_backward = (*biter) * (*eiter);

	  if (factor_backward > 0.0) {
	    const prob_type* fiter_word = &(*forward.begin(trg - 1));
	    const prob_type* titer_word = &(*transition.begin(trg, next));
	    const prob_type* fiter_none = &(*forward.begin(trg - 1)) + (source_size + 2);
	    const prob_type* titer_none = &(*transition.begin(trg, next)) + (source_size + 2);
	    
	    // - 1 to exlude EOS
	    for (int prev = 0; prev < (source_size + 2) - 1; ++ prev, ++ fiter_word, ++ titer_word, ++ fiter_none, ++ titer_none) {
	      const double count_word = (*fiter_word) * factor_backward * (*titer_word) * factor;
	      const double count_none = (*fiter_none) * factor_backward * (*titer_none) * factor;
	      
	      mapped[prev]->operator[](next - prev) += ((count_word > 0.0 && std::isfinite(count_word) ? count_word : 0.0)
							+ (count_none > 0.0 && std::isfinite(count_none) ? count_none : 0.0));
	    }
	  }
	}
      }
    }
  };

  typedef HMMData hmm_data_type;
  
  
  void learn(const sentence_type& source,
	     const sentence_type& target,
	     const ttable_type& ttable,
	     const atable_type& atable,
	     const classes_type& classes_source,
	     const classes_type& classes_target,
	     ttable_type& counts_ttable,
	     atable_type& counts_atable,
	     aligned_type& aligned,
	     double& objective)
  {
    const size_type source_size = source.size();
    const size_type target_size = target.size();

    hmm.prepare(source, target, ttable, atable, classes_source, classes_target);
    
    hmm.forward_backward(source, target);
    
    objective += hmm.objective() / target_size;
    
    hmm.accumulate(source, target, counts_ttable);
    
    hmm.accumulate(source, target, counts_atable);
  }
  
  void operator()(const sentence_type& source, const sentence_type& target)
  {
    learn(source,
	  target,
	  ttable_source_target,
	  atable_source_target,
	  classes_source,
	  classes_target,
	  ttable_counts_source_target,
	  atable_counts_source_target,
	  aligned_source_target,
	  objective_source_target);
    learn(target,
	  source,
	  ttable_target_source,
	  atable_target_source,
	  classes_target,
	  classes_source,
	  ttable_counts_target_source,
	  atable_counts_target_source,
	  aligned_target_source,
	  objective_target_source);
  }

  hmm_data_type hmm;
};

struct LearnHMMPosterior : public LearnBase
{
  LearnHMMPosterior(const LearnBase& __base)
    : LearnBase(__base) {}

  typedef LearnHMM::hmm_data_type hmm_data_type;
  
  typedef std::vector<prob_type, std::allocator<prob_type> > phi_set_type;
  
  void learn(const sentence_type& source,
	     const sentence_type& target,
	     const ttable_type& ttable,
	     const atable_type& atable,
	     const classes_type& classes_source,
	     const classes_type& classes_target,
	     ttable_type& counts_ttable,
	     atable_type& counts_atable,
	     aligned_type& aligned,
	     double& objective)
  {
    const size_type source_size = source.size();
    const size_type target_size = target.size();
    
    hmm.prepare(source, target, ttable, atable, classes_source, classes_target);
    
    hmm.forward_backward(source, target);
    
    objective += hmm.objective() / target_size;
    
    phi.clear();
    phi.resize(source_size + 1, 0.0);
    
    exp_phi_old.clear();
    exp_phi_old.resize(source_size + 1, 1.0);
    
    for (int iter = 0; iter < 5; ++ iter) {
      hmm.estimate_posterior(source, target);
      
      exp_phi.clear();
      exp_phi.resize(source_size + 1, 1.0);
      
      size_type count_zero = 0;
      for (int src = 1; src <= source_size; ++ src) {
	double sum_posterior = 0.0;
	for (int trg = 1; trg <= target_size; ++ trg)
	  sum_posterior += hmm.posterior(trg, src);
	
	phi[src] += 1.0 - sum_posterior;
	if (phi[src] > 0.0)
	  phi[src] = 0.0;
	
	count_zero += (phi[src] == 0.0);
	exp_phi[src] = utils::mathop::exp(phi[src]);
      }
      
      if (count_zero == source_size) break;
      
      // rescale emission table...
      for (int trg = 1; trg <= target_size; ++ trg) {
	// translation into non-NULL word
	prob_type* eiter = &(*hmm.emission.begin(trg)) + 1;
	for (int src = 1; src <= source_size; ++ src, ++ eiter)
	  (*eiter) *= exp_phi[src] / exp_phi_old[src];
      }
      // swap...
      exp_phi_old.swap(exp_phi);
      
      hmm.forward_backward(source, target);
    }
    
    hmm.accumulate(source, target, counts_ttable);
    
    hmm.accumulate(source, target, counts_atable);
  }

  void operator()(const sentence_type& source, const sentence_type& target)
  {
    learn(source,
	  target,
	  ttable_source_target,
	  atable_source_target,
	  classes_source,
	  classes_target,
	  ttable_counts_source_target,
	  atable_counts_source_target,
	  aligned_source_target,
	  objective_source_target);
    learn(target,
	  source,
	  ttable_target_source,
	  atable_target_source,
	  classes_target,
	  classes_source,
	  ttable_counts_target_source,
	  atable_counts_target_source,
	  aligned_target_source,
	  objective_target_source);
  }

  hmm_data_type hmm;
  
  phi_set_type phi;
  phi_set_type exp_phi;
  phi_set_type exp_phi_old;
};

struct LearnHMMSymmetric : public LearnBase
{
  LearnHMMSymmetric(const LearnBase& __base)
    : LearnBase(__base) {}

  typedef LearnHMM::hmm_data_type hmm_data_type;
  
  void operator()(const sentence_type& source, const sentence_type& target)
  {
    const size_type source_size = source.size();
    const size_type target_size = target.size();
    
    hmm_source_target.prepare(source, target, ttable_source_target, atable_source_target, classes_source, classes_target);
    hmm_target_source.prepare(target, source, ttable_target_source, atable_target_source, classes_target, classes_source);
    
    hmm_source_target.forward_backward(source, target);
    hmm_target_source.forward_backward(target, source);
    
    objective_source_target += hmm_source_target.objective() / target_size;
    objective_target_source += hmm_target_source.objective() / source_size;
    
    // accumulate lexicon
    hmm_source_target.estimate_posterior(source, target);
    hmm_target_source.estimate_posterior(target, source);
    
    // combine!
    for (int src = 0; src <= source_size; ++ src)
      for (int trg = 0; trg <= target_size; ++ trg) {
	double count = (trg == 0 ? 1.0 : hmm_source_target.posterior(trg, src)) * (src == 0 ? 1.0 : hmm_target_source.posterior(src, trg));
	if (src && trg)
	  count = utils::mathop::sqrt(count);
	
	const word_type word_source = (src == 0 ? vocab_type::NONE : source[src - 1]);
	const word_type word_target = (trg == 0 ? vocab_type::NONE : target[trg - 1]);
	
	if (trg != 0)
	  ttable_counts_source_target[word_source][word_target] += count;
	
	if (src != 0)
	  ttable_counts_target_source[word_target][word_source] += count;
      }
    
    hmm_source_target.accumulate(source, target, atable_counts_source_target);
    hmm_target_source.accumulate(target, source, atable_counts_target_source);
  }
  
  hmm_data_type hmm_source_target;
  hmm_data_type hmm_target_source;
};

struct LearnHMMSymmetricPosterior : public LearnBase
{
  LearnHMMSymmetricPosterior(const LearnBase& __base)
    : LearnBase(__base) {}
  
  typedef LearnHMM::hmm_data_type hmm_data_type;
  typedef utils::vector2_aligned<double, utils::aligned_allocator<double> > phi_set_type;

  void operator()(const sentence_type& source, const sentence_type& target)
  {
    const size_type source_size = source.size();
    const size_type target_size = target.size();
    
    hmm_source_target.prepare(source, target, ttable_source_target, atable_source_target, classes_source, classes_target);
    hmm_target_source.prepare(target, source, ttable_target_source, atable_target_source, classes_target, classes_source);
    
    hmm_source_target.forward_backward(source, target);
    hmm_target_source.forward_backward(target, source);
    
    objective_source_target += hmm_source_target.objective() / target_size;
    objective_target_source += hmm_target_source.objective() / source_size;
    
    phi.clear();
    phi.resize(target_size + 1, source_size + 1, 0.0);
    
    exp_phi.clear();
    exp_phi.resize(target_size + 1, source_size + 1, 1.0);
    
    exp_phi_old.clear();
    exp_phi_old.resize(target_size + 1, source_size + 1, 1.0);
    
    for (int iter = 0; iter < 5; ++ iter) {
      hmm_source_target.estimate_posterior(source, target);
      hmm_target_source.estimate_posterior(target, source);

      bool updated = false;
      
      // update phi...
      for (int src = 1; src <= source_size; ++ src)
	for (int trg = 1; trg <= target_size; ++ trg) {
	  const double epsi = hmm_source_target.posterior(trg, src) - hmm_target_source.posterior(src, trg);
	  const double update = - epsi;
	  
	  phi(trg, src) += update;
	  
	  updated |= (phi(trg, src) != 0.0);
	  exp_phi(trg, src) = utils::mathop::exp(phi(trg, src));
	}
      
      if (! updated) break;
      
      // rescale emission table...
      for (int trg = 1; trg <= target_size; ++ trg) {
	prob_type* eiter = &(*hmm_source_target.emission.begin(trg)) + 1;
	for (int src = 1; src <= source_size; ++ src, ++ eiter)
	  (*eiter) *= exp_phi(trg, src) / exp_phi_old(trg, src);
      }
      
      for (int src = 1; src <= source_size; ++ src) {
	prob_type* eiter = &(*hmm_target_source.emission.begin(src)) + 1;
	for (int trg = 1; trg <= target_size; ++ trg, ++ eiter)
	  (*eiter) *= exp_phi_old(trg, src) / exp_phi(trg, src);
      }
      
      // swap...
      exp_phi_old.swap(exp_phi);
      
      // forward-backward...
      hmm_source_target.forward_backward(source, target);
      hmm_target_source.forward_backward(target, source);
    }
    
    hmm_source_target.accumulate(source, target, ttable_counts_source_target);
    hmm_target_source.accumulate(target, source, ttable_counts_target_source);
    
    hmm_source_target.accumulate(source, target, atable_counts_source_target);
    hmm_target_source.accumulate(target, source, atable_counts_target_source);
  }

  hmm_data_type hmm_source_target;
  hmm_data_type hmm_target_source;
  
  phi_set_type phi;
  phi_set_type exp_phi;
  phi_set_type exp_phi_old;
};

struct ViterbiHMM : public ViterbiBase
{
  ViterbiHMM(const ttable_type& __ttable_source_target,
	     const ttable_type& __ttable_target_source,
	     const atable_type& __atable_source_target,
	     const atable_type& __atable_target_source,
	     const classes_type& __classes_source,
	     const classes_type& __classes_target)
    : ViterbiBase(__ttable_source_target, __ttable_target_source,
		  __atable_source_target, __atable_target_source,
		  __classes_source, __classes_target) {}
  
  void viterbi(const sentence_type& source,
	       const sentence_type& target,
	       const ttable_type& ttable,
	       const atable_type& atable,
	       const classes_type& classes_source,
	       const classes_type& classes_target,
	       alignment_type& alignment)
  {
    const size_type source_size = source.size();
    const size_type target_size = target.size();
    
    const double prob_null  = p0;
    const double prob_align = 1.0 - p0;
    
    forward_max.clear();
    forward_sum.clear();
    backptr.clear();
    scale.clear();
    
    forward_max.clear();
    forward_sum.clear();
    backptr.clear();
    scale.clear();
    
    forward_max.resize(target_size + 2, (source_size + 2) * 2, 0.0);
    forward_sum.resize(target_size + 2, (source_size + 2) * 2, 0.0);
    backptr.resize(target_size + 2, (source_size + 2) * 2, -1);
    scale.resize(target_size + 2, 1.0);
    
    source_class.reserve(source_size + 2);
    target_class.reserve(target_size + 2);
    source_class.resize(source_size + 2);
    target_class.resize(target_size + 2);
    
    source_class[0] = vocab_type::BOS;
    target_class[0] = vocab_type::BOS;
    source_class[(source_size + 2) - 1] = vocab_type::EOS;
    target_class[(target_size + 2) - 1] = vocab_type::EOS;

    sentence_type::iterator csiter = source_class.begin() + 1;
    for (sentence_type::const_iterator siter = source.begin(); siter != source.end(); ++ siter, ++ csiter)
      *csiter = classes_source[*siter];
    
    sentence_type::iterator ctiter = target_class.begin() + 1;
    for (sentence_type::const_iterator titer = target.begin(); titer != target.end(); ++ titer, ++ ctiter)
      *ctiter = classes_target[*titer];
    
    forward_max(0, 0) = 1.0;
    forward_sum(0, 0) = 1.0;
    
    double scale_accumulated = 0.0;
    for (int trg = 1; trg < target_size + 2; ++ trg) {
      const word_type target_word = (trg == target_size + 1 ? vocab_type::EOS : target[trg - 1]);
      const double emission_none = ttable(vocab_type::NONE, target_word);
      
      // + 1 to exclude BOS
      prob_type* niter_sum = &(*forward_sum.begin(trg)) + 1;
      prob_type* niter_max = &(*forward_max.begin(trg)) + 1;
      index_type* biter_max = &(*backptr.begin(trg)) + 1;
      
      for (int next = 1; next < source_size + 2; ++ next, ++ niter_sum, ++ niter_max, ++ biter_max) {
	const word_type source_word = (next == source_size + 1 ? vocab_type::EOS : source[next - 1]);
	const double emission = ttable(source_word, target_word);
	
	if (emission > 0.0) {
	  const prob_type* piter_sum = &(*forward_sum.begin(trg - 1));
	  const prob_type* piter_max = &(*forward_max.begin(trg - 1));
	  
	  for (int prev = 0; prev < (source_size + 2) - 1; ++ prev) {
	    const prob_type transition_word = prob_align * atable(source_class[prev], target_class[trg],
								  source_size, target_size,
								  prev - 1, next - 1);
	    
	    *niter_sum += emission * transition_word * piter_sum[prev];
	    *niter_sum += emission * transition_word * piter_sum[prev + source_size + 2];
	    
	    const double prob1 = emission * transition_word * piter_max[prev];
	    const double prob2 = emission * transition_word * piter_max[prev + source_size + 2];
	    
	    if (prob1 > *niter_max) {
	      *niter_max = prob1;
	      *biter_max = prev;
	    }
	    if (prob2 > *niter_max) {
	      *niter_max = prob2;
	      *biter_max = prev + source_size + 2;
	    }
	  }
	}
      }
      
      prob_type* niter_sum_none = &(*forward_sum.begin(trg)) + source_size + 2;
      prob_type* niter_max_none = &(*forward_max.begin(trg)) + source_size + 2;
      index_type* biter_max_none = &(*backptr.begin(trg)) + source_size + 2;
      
      for (int next = 0; next < (source_size + 2) - 1; ++ next, ++ niter_sum_none, ++ niter_max_none, ++ biter_max_none) {
	// alignment into none...
	const int next_none = next + source_size + 2;
	const int prev_none1 = next;
	const int prev_none2 = next + source_size + 2;
	
	const prob_type transition_none = prob_null;
	
	*niter_sum_none += forward_sum(trg - 1, prev_none1) * emission_none * transition_none;
	*niter_sum_none += forward_sum(trg - 1, prev_none2) * emission_none * transition_none;
	
	const double prob1 = forward_max(trg - 1, prev_none1) * emission_none * transition_none;
	const double prob2 = forward_max(trg - 1, prev_none2) * emission_none * transition_none;
	
	if (prob1 > *niter_max_none) {
	  *niter_max_none = prob1;
	  *biter_max_none = prev_none1;
	}
	
	if (prob2 > *niter_max_none) {
	  *niter_max_none = prob2;
	  *biter_max_none = prev_none2;
	}
      }

      scale[trg] = std::accumulate(forward_sum.begin(trg), forward_sum.end(trg), 0.0);
      scale[trg] = (scale[trg] == 0.0 ? 1.0 : 1.0 / scale[trg]);
      
      if (scale[trg] != 1.0) {
	std::transform(forward_sum.begin(trg), forward_sum.end(trg), forward_sum.begin(trg), std::bind2nd(std::multiplies<double>(), scale[trg]));
	std::transform(forward_max.begin(trg), forward_max.end(trg), forward_max.begin(trg), std::bind2nd(std::multiplies<double>(), scale[trg]));
      }
    }
    
    // traverse-back...
    alignment.clear();
    index_type position = backptr(target_size + 2 - 1 , source_size + 2 - 1);
    for (int trg = target_size; trg >= 1; -- trg) {
      if (position < source_size + 2)
	alignment.push_back(std::make_pair(position - 1, trg - 1));
      position = backptr(trg, position);
    }
  }
  
  void operator()(const sentence_type& source,
		  const sentence_type& target,
		  const span_set_type& span_source,
		  const span_set_type& span_target,
		  alignment_type& alignment_source_target,
		  alignment_type& alignment_target_source)
  {
    operator()(source, target, alignment_source_target, alignment_target_source);
  }
  
  void operator()(const sentence_type& source,
		  const sentence_type& target,
		  alignment_type& alignment_source_target,
		  alignment_type& alignment_target_source)
  {
    viterbi(source, target, ttable_source_target, atable_source_target, classes_source, classes_target, alignment_source_target);
    viterbi(target, source, ttable_target_source, atable_target_source, classes_target, classes_source, alignment_target_source);
  }

  typedef utils::vector2_aligned<prob_type, std::allocator<prob_type> > forward_type;
  typedef std::vector<prob_type, std::allocator<prob_type> >    scale_type;
  
  typedef utils::vector2_aligned<index_type, std::allocator<index_type> > backptr_type;
  
  forward_type  forward_max;
  forward_type  forward_sum;
  backptr_type  backptr;
  scale_type    scale;

  sentence_type source_class;
  sentence_type target_class;
};

struct ITGHMM : public ViterbiBase
{
  typedef utils::vector2<double, std::allocator<double> > matrix_type;
  
  ITGHMM(const ttable_type& __ttable_source_target,
	 const ttable_type& __ttable_target_source,
	 const atable_type& __atable_source_target,
	 const atable_type& __atable_target_source,
	 const classes_type& __classes_source,
	 const classes_type& __classes_target)
    : ViterbiBase(__ttable_source_target, __ttable_target_source,
		  __atable_source_target, __atable_target_source,
		  __classes_source, __classes_target) {}

  typedef LearnHMM::hmm_data_type hmm_data_type;
  
  class insert_align
  {
    alignment_type& alignment_source_target;
    alignment_type& alignment_target_source;
    
  public:
    insert_align(alignment_type& __alignment_source_target,
		 alignment_type& __alignment_target_source)
      : alignment_source_target(__alignment_source_target),
	alignment_target_source(__alignment_target_source) {}
    
    template <typename Edge>
    insert_align& operator=(const Edge& edge)
    {	
      alignment_source_target.push_back(edge);
      alignment_target_source.push_back(std::make_pair(edge.second, edge.first));
      
      return *this;
    }
    
    insert_align& operator*() { return *this; }
    insert_align& operator++() { return *this; }
    insert_align operator++(int) { return *this; }
  };

  const span_set_type __span_source;
  const span_set_type __span_target;
  
  void operator()(const sentence_type& source,
		  const sentence_type& target,
		  alignment_type& alignment_source_target,
		  alignment_type& alignment_target_source)
  {
    operator()(source, target, __span_source, __span_target, alignment_source_target, alignment_target_source);
  }
  
  
  void operator()(const sentence_type& source,
		  const sentence_type& target,
		  const span_set_type& span_source,
		  const span_set_type& span_target,
		  alignment_type& alignment_source_target,
		  alignment_type& alignment_target_source)
  {
    const size_type source_size = source.size();
    const size_type target_size = target.size();
    
    hmm_source_target.prepare(source, target, ttable_source_target, atable_source_target, classes_source, classes_target);
    hmm_target_source.prepare(target, source, ttable_target_source, atable_target_source, classes_target, classes_source);
    
    hmm_source_target.forward_backward(source, target);
    hmm_target_source.forward_backward(target, source);

    hmm_source_target.estimate_posterior(source, target);
    hmm_target_source.estimate_posterior(target, source);
    
    costs.clear();
    costs.resize(source_size + 1, target_size + 1, boost::numeric::bounds<double>::lowest());
    
    for (size_type src = 1; src <= source_size; ++ src)
      for (size_type trg = 1; trg <= target_size; ++ trg)
	costs(src, trg) = 0.5 * (utils::mathop::log(hmm_source_target.posterior(trg, src)) 
				 + utils::mathop::log(hmm_target_source.posterior(src, trg)));
    
    for (size_type trg = 1; trg <= target_size; ++ trg)
      costs(0, trg) = utils::mathop::log(hmm_source_target.posterior(trg, 0));
    
    for (size_type src = 1; src <= source_size; ++ src)
      costs(src, 0) = utils::mathop::log(hmm_target_source.posterior(src, 0));
    
    alignment_source_target.clear();
    alignment_target_source.clear();
    
    if (span_source.empty() && span_target.empty())
      aligner(costs, insert_align(alignment_source_target, alignment_target_source));
    else
      aligner(costs, span_source, span_target, insert_align(alignment_source_target, alignment_target_source));
    
    std::sort(alignment_source_target.begin(), alignment_source_target.end());
    std::sort(alignment_target_source.begin(), alignment_target_source.end());
  }
  
  hmm_data_type hmm_source_target;
  hmm_data_type hmm_target_source;
  
  matrix_type costs;
  
  detail::ITGAlignment aligner;
};

struct MaxMatchHMM : public ViterbiBase
{
  typedef utils::vector2<double, std::allocator<double> > matrix_type;
  
  MaxMatchHMM(const ttable_type& __ttable_source_target,
	      const ttable_type& __ttable_target_source,
	      const atable_type& __atable_source_target,
	      const atable_type& __atable_target_source,
	      const classes_type& __classes_source,
	      const classes_type& __classes_target)
    : ViterbiBase(__ttable_source_target, __ttable_target_source,
		  __atable_source_target, __atable_target_source,
		  __classes_source, __classes_target) {}

  typedef LearnHMM::hmm_data_type hmm_data_type;
  
  class insert_align
  {
    int source_size;
    int target_size;
    
    alignment_type& alignment_source_target;
    alignment_type& alignment_target_source;
    
  public:
    insert_align(const int& _source_size,
		 const int& _target_size,
		 alignment_type& __alignment_source_target,
		 alignment_type& __alignment_target_source)
      : source_size(_source_size), target_size(_target_size),
	alignment_source_target(__alignment_source_target),
	alignment_target_source(__alignment_target_source) {}
    
    template <typename Edge>
    insert_align& operator=(const Edge& edge)
    {	
      if (edge.first < source_size && edge.second < target_size) {
	alignment_source_target.push_back(edge);
	alignment_target_source.push_back(std::make_pair(edge.second, edge.first));
      }
      
      return *this;
    }
    
    insert_align& operator*() { return *this; }
    insert_align& operator++() { return *this; }
    insert_align operator++(int) { return *this; }
  };
  
  void operator()(const sentence_type& source,
		  const sentence_type& target,
		  const span_set_type& span_source,
		  const span_set_type& span_target,
		  alignment_type& alignment_source_target,
		  alignment_type& alignment_target_source)
  {
    operator()(source, target, alignment_source_target, alignment_target_source);
  }
  
  void operator()(const sentence_type& source,
		  const sentence_type& target,
		  alignment_type& alignment_source_target,
		  alignment_type& alignment_target_source)
  {
    const size_type source_size = source.size();
    const size_type target_size = target.size();
    
    hmm_source_target.prepare(source, target, ttable_source_target, atable_source_target, classes_source, classes_target);
    hmm_target_source.prepare(target, source, ttable_target_source, atable_target_source, classes_target, classes_source);
    
    hmm_source_target.forward_backward(source, target);
    hmm_target_source.forward_backward(target, source);
    
    hmm_source_target.estimate_posterior(source, target);
    hmm_target_source.estimate_posterior(target, source);
    
    costs.clear();
    costs.resize(source_size + target_size, target_size + source_size, 0.0);
    
    for (size_type src = 0; src != source_size; ++ src)
      for (size_type trg = 0; trg != target_size; ++ trg) {
	costs(src, trg) = 0.5 * (utils::mathop::log(hmm_source_target.posterior(trg + 1, src + 1))
				 + utils::mathop::log(hmm_target_source.posterior(src + 1, trg + 1)));
	
	costs(src, trg + source_size) = utils::mathop::log(hmm_target_source.posterior(src + 1, 0));
	costs(src + target_size, trg) = utils::mathop::log(hmm_source_target.posterior(trg + 1, 0));
      }
    
    alignment_source_target.clear();
    alignment_target_source.clear();
    
    kuhn_munkres_assignment(costs, insert_align(source_size, target_size, alignment_source_target, alignment_target_source));
    
    std::sort(alignment_source_target.begin(), alignment_source_target.end());
    std::sort(alignment_target_source.begin(), alignment_target_source.end());
  }
  
  matrix_type costs;

  hmm_data_type hmm_source_target;
  hmm_data_type hmm_target_source;
};

#endif
