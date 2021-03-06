//
//  Copyright(C) 2009-2013 Taro Watanabe <taro.watanabe@nict.go.jp>
//

#ifndef __CICADA_ALIGNMENT_HMM_IMPL__HPP__
#define __CICADA_ALIGNMENT_HMM_IMPL__HPP__ 1

#include <numeric>
#include <set>

#include "cicada_alignment_impl.hpp"

#include "utils/vector2_aligned.hpp"
#include "utils/vector3_aligned.hpp"
#include "utils/mathop.hpp"
#include "utils/aligned_allocator.hpp"

#include "kuhn_munkres.hpp"
#include "itg_alignment.hpp"
#include "dependency_hybrid.hpp"
#include "dependency_degree2.hpp"
#include "dependency_mst.hpp"

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

    typedef std::set<int, std::less<int>, std::allocator<int> > point_set_type;
    typedef std::vector<point_set_type, std::allocator<point_set_type> > point_map_type;
    
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

    point_map_type points;

    void shrink()
    {
      forward.clear();
      backward.clear();
      
      emission.clear();
      transition.clear();
      scale.clear();
      
      posterior.clear();
      
      source.clear();
      target.clear();
      
      source_class.clear();
      target_class.clear();
      
      points.clear();
      
      forward_type(forward).swap(forward);
      backward_type(backward).swap(backward);
      
      emission_type(emission).swap(emission);
      transition_type(transition).swap(transition);
      scale_type(scale).swap(scale);
      
      posterior_type(posterior).swap(posterior);
      
      sentence_type(source).swap(source);
      sentence_type(target).swap(target);
      sentence_type(source_class).swap(source_class);
      sentence_type(target_class).swap(target_class);
      
      point_map_type(points).swap(points);
    }
    
    void prepare(const sentence_type& __source,
		 const sentence_type& __target,
		 const alignment_type& alignment,
		 const bool inverse,
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
      source.resize((source_size + 2) * 2, vocab_type::EPSILON);
      target.resize((target_size + 2) * 2, vocab_type::EPSILON);
      
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

      points.clear();
      points.reserve(target_size);
      points.resize(target_size);

      if (inverse) {
	alignment_type::const_iterator aiter_end = alignment.end();
	for (alignment_type::const_iterator aiter = alignment.begin(); aiter != aiter_end; ++ aiter)
	  points[aiter->source].insert(aiter->target);
      } else {
	alignment_type::const_iterator aiter_end = alignment.end();
	for (alignment_type::const_iterator aiter = alignment.begin(); aiter != aiter_end; ++ aiter)
	  points[aiter->target].insert(aiter->source);
      }
      
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
	if (! points[trg - 1].empty()) {
	  point_set_type::const_iterator piter_end = points[trg - 1].end();
	  for (point_set_type::const_iterator piter = points[trg - 1].begin(); piter != piter_end; ++ piter)
	    emission(trg, *piter + 1) = ttable(source[*piter + 1], target[trg]);
	} else {
	  // translation into non-NULL word
	  prob_type* eiter = &(*emission.begin(trg)) + 1;
	  for (size_type src = 1; src <= source_size; ++ src, ++ eiter)
	    (*eiter) = ttable(source[src], target[trg]);
	
	  // NULL
	  prob_type* eiter_first = &(*emission.begin(trg)) + source_size + 2;
	  prob_type* eiter_last  = eiter_first + source_size + 2 - 1; // -1 to exclude EOS
	  
	  std::fill(eiter_first, eiter_last, ttable(vocab_type::EPSILON, target[trg]));
	}
      }

      // compute transition table...
      // we start from 1, since there exists no previously aligned word before BOS...
      for (size_type trg = 1; trg < target_size + 2; ++ trg) {
	
	// alignment into non-null
	// start from 1 to exlude transition into <s>
	for (size_type next = 1; next != source_size + 2; ++ next) {
	  prob_type* titer1 = &(*transition.begin(trg, next)); // from word
	  prob_type* titer2 = titer1 + (source_size + 2);      // from EPSILON
	  
	  // - 1 to exclude previous </s>...
	  for (size_type prev = 0; prev != (source_size + 2) - 1; ++ prev, ++ titer1, ++ titer2) {
	    const prob_type prob = prob_align * atable(source_class[prev], target_class[trg],
						       source_size, target_size,
						       prev - 1, next - 1);
	    
	    *titer1 = prob;
	    *titer2 = prob;
	  }
	}
	
	// null transition
	// we will exclude EOS
	for (size_type next = 0; next != (source_size + 2) - 1; ++ next) {
	  const size_type next_none = next + (source_size + 2);
	  const size_type prev1_none = next;
	  const size_type prev2_none = next + (source_size + 2);
	  
	  transition(trg, next_none, prev1_none) = prob_null;
	  transition(trg, next_none, prev2_none) = prob_null;
	}
      }
    }

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
      source.resize((source_size + 2) * 2, vocab_type::EPSILON);
      target.resize((target_size + 2) * 2, vocab_type::EPSILON);
      
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
	
	std::fill(eiter_first, eiter_last, ttable(vocab_type::EPSILON, target[trg]));
      }

      
      // compute transition table...
      // we start from 1, since there exists no previously aligned word before BOS...
      for (size_type trg = 1; trg != target_size + 2; ++ trg) {
	
	// alignment into non-null
	// start from 1 to exlude transition into <s>
	for (size_type next = 1; next != source_size + 2; ++ next) {
	  prob_type* titer1 = &(*transition.begin(trg, next)); // from word
	  prob_type* titer2 = titer1 + (source_size + 2);      // from EPSILON
	  
	  // - 1 to exclude previous </s>...
	  for (size_type prev = 0; prev != (source_size + 2) - 1; ++ prev, ++ titer1, ++ titer2) {
	    const prob_type prob = prob_align * atable(source_class[prev], target_class[trg],
						       source_size, target_size,
						       prev - 1, next - 1);
	    
	    *titer1 = prob;
	    *titer2 = prob;
	  }
	}
	
	// null transition
	// we will exclude EOS
	for (size_type next = 0; next != (source_size + 2) - 1; ++ next) {
	  const size_type next_none = next + (source_size + 2);
	  const size_type prev1_none = next;
	  const size_type prev2_none = next + (source_size + 2);
	  
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
      for (size_type trg = 1; trg != target_size + 2; ++ trg) {
	// +1 to exclude BOS
	prob_type*       niter = &(*forward.begin(trg)) + 1;
	const prob_type* eiter = &(*emission.begin(trg)) + 1;
	
	for (size_type next = 1; next != source_size + 2; ++ next, ++ niter, ++ eiter) {
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
	    
#if 0
	    for (int prev = 0; prev < (source_size + 2) * 2; ++ prev, ++ piter, ++ titer)
	      *niter += (*piter) * (*titer) * factor;
#endif
	  }
	}
	
	prob_type*       niter_none = &(*forward.begin(trg)) + (source_size + 2);
	const prob_type* eiter_none = &(*emission.begin(trg)) + (source_size + 2);
	const prob_type* piter_none1 = &(*forward.begin(trg - 1));
	const prob_type* piter_none2 = piter_none1 + (source_size + 2);
	
	for (size_type next = 0; next != (source_size + 2) - 1; ++ next, ++ niter_none, ++ eiter_none, ++ piter_none1, ++ piter_none2) {
	  const size_type next_none = next + source_size + 2;
	  const size_type prev_none1 = next;
	  const size_type prev_none2 = next + source_size + 2;
	  
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
	
	for (size_type next = 1; next != source_size + 2; ++ next, ++ niter, ++ eiter) {
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
	    
#if 0
	    for (int prev = 0; prev < (source_size + 2) * 2; ++ prev, ++ piter, ++ titer)
	      *piter += (*titer) * factor;
#endif
	  }
	}
	
	const prob_type* niter_none = &(*backward.begin(trg + 1)) + (source_size + 2);
	const prob_type* eiter_none = &(*emission.begin(trg + 1)) + (source_size + 2);
	prob_type*       piter_none1 = &(*backward.begin(trg));
	prob_type*       piter_none2 = piter_none1 + (source_size + 2);

	for (size_type next = 0; next != (source_size + 2) - 1; ++ next, ++ niter_none, ++ eiter_none, ++ piter_none1, ++ piter_none2) {
	  const size_type next_none = next + source_size + 2;
	  const size_type prev_none1 = next;
	  const size_type prev_none2 = next + source_size + 2;
	  
	  *piter_none1 += (*niter_none) * (*eiter_none) * transition(trg + 1, next_none, prev_none1) * factor_scale;
	  *piter_none2 += (*niter_none) * (*eiter_none) * transition(trg + 1, next_none, prev_none2) * factor_scale;
	}
      }
    }

    void estimate_posterior(const sentence_type& __source,
			    const sentence_type& __target)
    {
      const int source_size = __source.size();
      const int target_size = __target.size();
      
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
	
#if 1
	for (int src = 0; src < source_size - 3; src += 4) {
	  piter[src + 0] += fiter[src + 0] * biter[src + 0] * factor;
	  piter[src + 1] += fiter[src + 1] * biter[src + 1] * factor;
	  piter[src + 2] += fiter[src + 2] * biter[src + 2] * factor;
	  piter[src + 3] += fiter[src + 3] * biter[src + 3] * factor;
	}
	switch (source_size & 0x03) {
	case 3: piter[source_size - 3] += fiter[source_size - 3] * biter[source_size - 3] * factor;
	case 2: piter[source_size - 2] += fiter[source_size - 2] * biter[source_size - 2] * factor;
	case 1: piter[source_size - 1] += fiter[source_size - 1] * biter[source_size - 1] * factor;
	}
#endif	
#if 0
	for (int src = 1; src <= source_size; ++ src, ++ fiter, ++ biter, ++ piter)
	  (*piter) += (*fiter) * (*biter) * factor;
#endif
	
	fiter = &(*forward.begin(trg))  + source_size + 2;
	biter = &(*backward.begin(trg)) + source_size + 2;
	
#if 1
	double count_none[4] = {0.0, 0.0, 0.0, 0.0};
	const int loop_size = source_size + 2;
	for (int src = 0; src < loop_size - 3; src += 4) {
	  count_none[0] += fiter[src + 0] * biter[src + 0] * factor;
	  count_none[1] += fiter[src + 1] * biter[src + 1] * factor;
	  count_none[2] += fiter[src + 2] * biter[src + 2] * factor;
	  count_none[3] += fiter[src + 3] * biter[src + 3] * factor;
	}
	switch (loop_size & 0x03) {
	case 3: count_none[4 - 3] += fiter[loop_size - 3] * biter[loop_size - 3] * factor;
	case 2: count_none[4 - 2] += fiter[loop_size - 2] * biter[loop_size - 2] * factor;
	case 1: count_none[4 - 1] += fiter[loop_size - 1] * biter[loop_size - 1] * factor;
	}
	
	posterior(trg, 0) = count_none[0] + count_none[1] + count_none[2] + count_none[3];
#endif
#if 0
	double count_none = 0.0;
	for (int src = 0; src < source_size + 2; ++ src, ++ fiter, ++ biter)
	  count_none += (*fiter) * (*biter) * factor;
	
      	posterior(trg, 0) = count_none;
#endif
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
      const int source_size = __source.size();
      const int target_size = __target.size();
      
      const prob_type sum = forward(target_size + 2 - 1, source_size + 2 - 1);
      
      {
	// allocate enough buffer size...
	
	for (int src = 1; src <= source_size; ++ src) {
	  ttable_type::count_map_type& mapped = counts[source[src]];
	  mapped.rehash(mapped.size() + target_size);
	}
	
	ttable_type::count_map_type& mapped = counts[vocab_type::EPSILON];
	mapped.rehash(mapped.size() + target_size);
      }
      
      // accumulate lexcion
      for (int trg = 1; trg <= target_size; ++ trg) {
	const double factor = 1.0 / (scale[trg] * sum);
      
	// + 1 to skip BOS
	const prob_type* fiter = &(*forward.begin(trg)) + 1;
	const prob_type* biter = &(*backward.begin(trg)) + 1;
	
	for (int src = 1; src <= source_size; ++ src, ++ fiter, ++ biter)
	  counts[source[src]][target[trg]] += (*fiter) * (*biter) * factor;
	
	// null alignment...
	fiter = &(*forward.begin(trg)) + source_size + 2;
	biter = &(*backward.begin(trg)) + source_size + 2;
	
#if 1
	double count_none[4] = {0.0, 0.0, 0.0, 0.0};
	const int loop_size = source_size + 2;
	for (int src = 0; src < loop_size - 3; src += 4) {
	  count_none[0] += fiter[src + 0] * biter[src + 0] * factor;
	  count_none[1] += fiter[src + 1] * biter[src + 1] * factor;
	  count_none[2] += fiter[src + 2] * biter[src + 2] * factor;
	  count_none[3] += fiter[src + 3] * biter[src + 3] * factor;
	}
	switch (loop_size & 0x03) {
	case 3: count_none[4 - 3] += fiter[loop_size - 3] * biter[loop_size - 3] * factor;
	case 2: count_none[4 - 2] += fiter[loop_size - 2] * biter[loop_size - 2] * factor;
	case 1: count_none[4 - 1] += fiter[loop_size - 1] * biter[loop_size - 1] * factor;
	}
	
	counts[vocab_type::EPSILON][target[trg]] += count_none[0] + count_none[1] + count_none[2] + count_none[3];
#endif
#if 0
	double count_none = 0.0;
	for (int src = 0; src < source_size + 2; ++ src, ++ fiter, ++ biter)
	  count_none += (*fiter) * (*biter) * factor;
	
	counts[vocab_type::EPSILON][target[trg]] += count_none;
#endif
      }
    }
    
    void accumulate(const sentence_type& __source,
		    const sentence_type& __target,
		    atable_counts_type& counts)
    {
      typedef std::vector<atable_counts_type::mapped_type*, std::allocator<atable_counts_type::mapped_type*> > mapped_type;

      const size_type source_size = __source.size();
      const size_type target_size = __target.size();
      
      const double sum = forward(target_size + 2 - 1, source_size + 2 - 1);
      const double factor = 1.0 / sum;

      mapped_type mapped((source_size + 2) - 1);

      for (size_type trg = 1; trg != target_size + 2; ++ trg) {
	
	for (size_type prev = 0; prev != (source_size + 2) - 1; ++ prev) {
	  mapped[prev] = &(counts[std::make_pair(source_class[prev], target_class[trg])]);
	  
	  // next - prev
	  // minimum of next: 1
	  // maximum of next: source_size + 2 - 1
	  mapped[prev]->reserve(1 - prev, source_size + 2 - prev - 1);
	}
	
	const prob_type* biter = &(*backward.begin(trg)) + 1;
	const prob_type* eiter = &(*emission.begin(trg)) + 1;
	
	// + 1, we will exclude <s>, since we will never aligned to <s>
	for (size_type next = 1; next != source_size + 2; ++ next, ++ biter, ++ eiter) {
	  const prob_type factor_backward = (*biter) * (*eiter);

	  if (factor_backward > 0.0) {
	    const prob_type* fiter_word = &(*forward.begin(trg - 1));
	    const prob_type* titer_word = &(*transition.begin(trg, next));
	    const prob_type* fiter_none = &(*forward.begin(trg - 1)) + (source_size + 2);
	    const prob_type* titer_none = &(*transition.begin(trg, next)) + (source_size + 2);
	    
	    // - 1 to exlude EOS
	    for (size_type prev = 0; prev != (source_size + 2) - 1; ++ prev, ++ fiter_word, ++ titer_word, ++ fiter_none, ++ titer_none) {
	      const double count_word = (*fiter_word) * factor_backward * (*titer_word) * factor;
	      const double count_none = (*fiter_none) * factor_backward * (*titer_none) * factor;
	      
	      mapped[prev]->operator[](next - prev) += count_word + count_none;
	    }
	  }
	}
      }
    }
  };

  typedef HMMData hmm_data_type;
  
  void learn(const sentence_type& source,
	     const sentence_type& target,
	     const alignment_type& alignment,
	     const bool inverse,
	     const ttable_type& ttable,
	     const atable_type& atable,
	     const classes_type& classes_source,
	     const classes_type& classes_target,
	     ttable_type& counts_ttable,
	     atable_counts_type& counts_atable,
	     aligned_type& aligned,
	     double& objective)
  {
    const size_type source_size = source.size();
    const size_type target_size = target.size();

    hmm.prepare(source, target, alignment, inverse, ttable, atable, classes_source, classes_target);
    
    hmm.forward_backward(source, target);
    
    objective += hmm.objective() / target_size;
    
    hmm.accumulate(source, target, counts_ttable);
    
    hmm.accumulate(source, target, counts_atable);
  }
  
  void learn(const sentence_type& source,
	     const sentence_type& target,
	     const ttable_type& ttable,
	     const atable_type& atable,
	     const classes_type& classes_source,
	     const classes_type& classes_target,
	     ttable_type& counts_ttable,
	     atable_counts_type& counts_atable,
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

  void operator()(const sentence_type& source, const sentence_type& target, const alignment_type& alignment)
  {
    learn(source,
	  target,
	  alignment,
	  false,
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
	  alignment,
	  true,
	  ttable_target_source,
	  atable_target_source,
	  classes_target,
	  classes_source,
	  ttable_counts_target_source,
	  atable_counts_target_source,
	  aligned_target_source,
	  objective_target_source);
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

  void shrink()
  {
    hmm.shrink();

    LearnBase::shrink();
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
	     const alignment_type& alignment,
	     const bool inverse,
	     const ttable_type& ttable,
	     const atable_type& atable,
	     const classes_type& classes_source,
	     const classes_type& classes_target,
	     ttable_type& counts_ttable,
	     atable_counts_type& counts_atable,
	     aligned_type& aligned,
	     double& objective)
  {
    const size_type source_size = source.size();
    const size_type target_size = target.size();
    
    hmm.prepare(source, target, alignment, inverse, ttable, atable, classes_source, classes_target);
    
    hmm.forward_backward(source, target);
    
    objective += hmm.objective() / target_size;
    
    phi.clear();
    exp_phi_old.clear();
    
    phi.reserve(source_size + 1);
    exp_phi_old.reserve(source_size + 1);
    
    phi.resize(source_size + 1, 0.0);
    exp_phi_old.resize(source_size + 1, 1.0);
    
    for (int iter = 0; iter < 5; ++ iter) {
      hmm.estimate_posterior(source, target);
      
      exp_phi.clear();
      exp_phi.reserve(source_size + 1);
      exp_phi.resize(source_size + 1, 1.0);
      
      size_type count_zero = 0;
      for (size_type src = 1; src <= source_size; ++ src) {
	double sum_posterior = 0.0;
	for (size_type trg = 1; trg <= target_size; ++ trg)
	  sum_posterior += hmm.posterior(trg, src);
	
	phi[src] += 1.0 - sum_posterior;
	if (phi[src] > 0.0)
	  phi[src] = 0.0;
	
	count_zero += (phi[src] == 0.0);
	exp_phi[src] = utils::mathop::exp(phi[src]);
      }
      
      if (count_zero == source_size) break;
      
      // rescale emission table...
      for (size_type trg = 1; trg <= target_size; ++ trg) {
	// translation into non-NULL word
	prob_type* eiter = &(*hmm.emission.begin(trg)) + 1;
	for (size_type src = 1; src <= source_size; ++ src, ++ eiter)
	  (*eiter) *= exp_phi[src] / exp_phi_old[src];
      }
      // swap...
      exp_phi_old.swap(exp_phi);
      
      hmm.forward_backward(source, target);
    }
    
    hmm.accumulate(source, target, counts_ttable);
    
    hmm.accumulate(source, target, counts_atable);    
  }
  
  void learn(const sentence_type& source,
	     const sentence_type& target,
	     const ttable_type& ttable,
	     const atable_type& atable,
	     const classes_type& classes_source,
	     const classes_type& classes_target,
	     ttable_type& counts_ttable,
	     atable_counts_type& counts_atable,
	     aligned_type& aligned,
	     double& objective)
  {
    const size_type source_size = source.size();
    const size_type target_size = target.size();
    
    hmm.prepare(source, target, ttable, atable, classes_source, classes_target);
    
    hmm.forward_backward(source, target);
    
    objective += hmm.objective() / target_size;
    
    phi.clear();
    exp_phi_old.clear();
    
    phi.reserve(source_size + 1);
    exp_phi_old.reserve(source_size + 1);
    
    phi.resize(source_size + 1, 0.0);
    exp_phi_old.resize(source_size + 1, 1.0);
    
    for (int iter = 0; iter < 5; ++ iter) {
      hmm.estimate_posterior(source, target);
      
      exp_phi.clear();
      exp_phi.reserve(source_size + 1);
      exp_phi.resize(source_size + 1, 1.0);
      
      size_type count_zero = 0;
      for (size_type src = 1; src <= source_size; ++ src) {
	double sum_posterior = 0.0;
	for (size_type trg = 1; trg <= target_size; ++ trg)
	  sum_posterior += hmm.posterior(trg, src);
	
	phi[src] += 1.0 - sum_posterior;
	if (phi[src] > 0.0)
	  phi[src] = 0.0;
	
	count_zero += (phi[src] == 0.0);
	exp_phi[src] = utils::mathop::exp(phi[src]);
      }
      
      if (count_zero == source_size) break;
      
      // rescale emission table...
      for (size_type trg = 1; trg <= target_size; ++ trg) {
	// translation into non-NULL word
	prob_type* eiter = &(*hmm.emission.begin(trg)) + 1;
	for (size_type src = 1; src <= source_size; ++ src, ++ eiter)
	  (*eiter) *= exp_phi[src] / exp_phi_old[src];
      }
      // swap...
      exp_phi_old.swap(exp_phi);
      
      hmm.forward_backward(source, target);
    }
    
    hmm.accumulate(source, target, counts_ttable);
    
    hmm.accumulate(source, target, counts_atable);
  }

  void operator()(const sentence_type& source, const sentence_type& target, const alignment_type& alignment)
  {
    learn(source,
	  target,
	  alignment,
	  false,
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
	  alignment,
	  true,
	  ttable_target_source,
	  atable_target_source,
	  classes_target,
	  classes_source,
	  ttable_counts_target_source,
	  atable_counts_target_source,
	  aligned_target_source,
	  objective_target_source);
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

  void shrink()
  {
    phi.clear();
    exp_phi.clear();
    exp_phi_old.clear();
    
    phi_set_type(phi).swap(phi);
    phi_set_type(exp_phi).swap(exp_phi);
    phi_set_type(exp_phi_old).swap(exp_phi_old);
    
    hmm.shrink();
    
    LearnBase::shrink();
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

  void operator()(const sentence_type& source, const sentence_type& target, const alignment_type& alignment)
  {
    const size_type source_size = source.size();
    const size_type target_size = target.size();
    
    hmm_source_target.prepare(source, target, alignment, false, ttable_source_target, atable_source_target, classes_source, classes_target);
    hmm_target_source.prepare(target, source, alignment, true,  ttable_target_source, atable_target_source, classes_target, classes_source);
    
    hmm_source_target.forward_backward(source, target);
    hmm_target_source.forward_backward(target, source);
    
    objective_source_target += hmm_source_target.objective() / target_size;
    objective_target_source += hmm_target_source.objective() / source_size;
    
    // accumulate lexicon
    hmm_source_target.estimate_posterior(source, target);
    hmm_target_source.estimate_posterior(target, source);
    
    // combine!
    for (size_type src = 0; src <= source_size; ++ src)
      for (size_type trg = 0; trg <= target_size; ++ trg) {
	double count = (! trg ? 1.0 : hmm_source_target.posterior(trg, src)) * (! src ? 1.0 : hmm_target_source.posterior(src, trg));
	if (src && trg)
	  count = utils::mathop::sqrt(count);
	
	const word_type word_source = (! src ? vocab_type::EPSILON : source[src - 1]);
	const word_type word_target = (! trg ? vocab_type::EPSILON : target[trg - 1]);
	
	if (trg)
	  ttable_counts_source_target[word_source][word_target] += count;
	
	if (src)
	  ttable_counts_target_source[word_target][word_source] += count;
      }
    
    hmm_source_target.accumulate(source, target, atable_counts_source_target);
    hmm_target_source.accumulate(target, source, atable_counts_target_source);    
  }
  
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
    for (size_type src = 0; src <= source_size; ++ src)
      for (size_type trg = 0; trg <= target_size; ++ trg) {
	double count = (! trg ? 1.0 : hmm_source_target.posterior(trg, src)) * (! src ? 1.0 : hmm_target_source.posterior(src, trg));
	if (src && trg)
	  count = utils::mathop::sqrt(count);
	
	const word_type word_source = (! src ? vocab_type::EPSILON : source[src - 1]);
	const word_type word_target = (! trg ? vocab_type::EPSILON : target[trg - 1]);
	
	if (trg)
	  ttable_counts_source_target[word_source][word_target] += count;
	
	if (src)
	  ttable_counts_target_source[word_target][word_source] += count;
      }
    
    hmm_source_target.accumulate(source, target, atable_counts_source_target);
    hmm_target_source.accumulate(target, source, atable_counts_target_source);
  }

  void shrink()
  {
    hmm_source_target.shrink();
    hmm_target_source.shrink();

    LearnBase::shrink();
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

  void operator()(const sentence_type& source, const sentence_type& target, const alignment_type& alignment)
  {
    const size_type source_size = source.size();
    const size_type target_size = target.size();
    
    hmm_source_target.prepare(source, target, alignment, false, ttable_source_target, atable_source_target, classes_source, classes_target);
    hmm_target_source.prepare(target, source, alignment, true,  ttable_target_source, atable_target_source, classes_target, classes_source);
    
    hmm_source_target.forward_backward(source, target);
    hmm_target_source.forward_backward(target, source);
    
    objective_source_target += hmm_source_target.objective() / target_size;
    objective_target_source += hmm_target_source.objective() / source_size;
    
    phi.clear();
    exp_phi.clear();
    exp_phi_old.clear();
    
    phi.reserve(target_size + 1, source_size + 1);
    exp_phi.reserve(target_size + 1, source_size + 1);
    exp_phi_old.reserve(target_size + 1, source_size + 1);
    
    phi.resize(target_size + 1, source_size + 1, 0.0);
    exp_phi.resize(target_size + 1, source_size + 1, 1.0);
    exp_phi_old.resize(target_size + 1, source_size + 1, 1.0);
    
    for (int iter = 0; iter < 5; ++ iter) {
      hmm_source_target.estimate_posterior(source, target);
      hmm_target_source.estimate_posterior(target, source);

      bool updated = false;
      
      // update phi...
      for (size_type src = 1; src <= source_size; ++ src)
	for (size_type trg = 1; trg <= target_size; ++ trg) {
	  const double epsi = hmm_source_target.posterior(trg, src) - hmm_target_source.posterior(src, trg);
	  const double update = - epsi;
	  
	  phi(trg, src) += update;
	  
	  updated |= (phi(trg, src) != 0.0);
	  exp_phi(trg, src) = utils::mathop::exp(phi(trg, src));
	}
      
      if (! updated) break;
      
      // rescale emission table...
      for (size_type trg = 1; trg <= target_size; ++ trg) {
	prob_type* eiter = &(*hmm_source_target.emission.begin(trg)) + 1;
	for (size_type src = 1; src <= source_size; ++ src, ++ eiter)
	  (*eiter) *= exp_phi(trg, src) / exp_phi_old(trg, src);
      }
      
      for (size_type src = 1; src <= source_size; ++ src) {
	prob_type* eiter = &(*hmm_target_source.emission.begin(src)) + 1;
	for (size_type trg = 1; trg <= target_size; ++ trg, ++ eiter)
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
    exp_phi.clear();
    exp_phi_old.clear();

    phi.reserve(target_size + 1, source_size + 1);
    exp_phi.reserve(target_size + 1, source_size + 1);
    exp_phi_old.reserve(target_size + 1, source_size + 1);
    
    phi.resize(target_size + 1, source_size + 1, 0.0);
    exp_phi.resize(target_size + 1, source_size + 1, 1.0);
    exp_phi_old.resize(target_size + 1, source_size + 1, 1.0);
    
    for (int iter = 0; iter < 5; ++ iter) {
      hmm_source_target.estimate_posterior(source, target);
      hmm_target_source.estimate_posterior(target, source);

      bool updated = false;
      
      // update phi...
      for (size_type src = 1; src <= source_size; ++ src)
	for (size_type trg = 1; trg <= target_size; ++ trg) {
	  const double epsi = hmm_source_target.posterior(trg, src) - hmm_target_source.posterior(src, trg);
	  const double update = - epsi;
	  
	  phi(trg, src) += update;
	  
	  updated |= (phi(trg, src) != 0.0);
	  exp_phi(trg, src) = utils::mathop::exp(phi(trg, src));
	}
      
      if (! updated) break;
      
      // rescale emission table...
      for (size_type trg = 1; trg <= target_size; ++ trg) {
	prob_type* eiter = &(*hmm_source_target.emission.begin(trg)) + 1;
	for (size_type src = 1; src <= source_size; ++ src, ++ eiter)
	  (*eiter) *= exp_phi(trg, src) / exp_phi_old(trg, src);
      }
      
      for (size_type src = 1; src <= source_size; ++ src) {
	prob_type* eiter = &(*hmm_target_source.emission.begin(src)) + 1;
	for (size_type trg = 1; trg <= target_size; ++ trg, ++ eiter)
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

  void shrink()
  {
    phi.clear();
    exp_phi.clear();
    exp_phi_old.clear();
    
    phi_set_type(phi).swap(phi);
    phi_set_type(exp_phi).swap(exp_phi);
    phi_set_type(exp_phi_old).swap(exp_phi_old);

    hmm_source_target.shrink();
    hmm_target_source.shrink();

    LearnBase::shrink();
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
    
    forward.clear();
    backptr.clear();
    scale.clear();
    
    forward.clear();
    backptr.clear();
    scale.clear();

    forward.reserve(target_size + 2, (source_size + 2) * 2);
    backptr.reserve(target_size + 2, (source_size + 2) * 2);
    scale.reserve(target_size + 2);
    
    forward.resize(target_size + 2, (source_size + 2) * 2, 0.0);
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
    
    forward(0, 0) = 1.0;
    
    double scale_accumulated = 0.0;
    for (size_type trg = 1; trg != target_size + 2; ++ trg) {
      const word_type target_word = (trg == target_size + 1 ? vocab_type::EOS : target[trg - 1]);
      const double emission_none = ttable(vocab_type::EPSILON, target_word);
      
      // + 1 to exclude BOS
      prob_type* niter = &(*forward.begin(trg)) + 1;
      index_type* biter = &(*backptr.begin(trg)) + 1;
      
      for (size_type next = 1; next != source_size + 2; ++ next, ++ niter, ++ biter) {
	const word_type source_word = (next == static_cast<int>(source_size + 1) ? vocab_type::EOS : source[next - 1]);
	const double emission = ttable(source_word, target_word);
	
	if (emission > 0.0) {
	  const prob_type* piter = &(*forward.begin(trg - 1));
	  
	  for (size_type prev = 0; prev != (source_size + 2) - 1; ++ prev) {
	    const prob_type transition_word = prob_align * atable(source_class[prev], target_class[trg],
								  source_size, target_size,
								  prev - 1, next - 1);
	    
	    const double prob1 = emission * transition_word * piter[prev];
	    const double prob2 = emission * transition_word * piter[prev + source_size + 2];
	    
	    if (prob1 > *niter) {
	      *niter = prob1;
	      *biter = prev;
	    }
	    if (prob2 > *niter) {
	      *niter = prob2;
	      *biter = prev + source_size + 2;
	    }
	  }
	}
      }
      
      prob_type* niter_none = &(*forward.begin(trg)) + source_size + 2;
      index_type* biter_none = &(*backptr.begin(trg)) + source_size + 2;
      
      for (size_type next = 0; next != (source_size + 2) - 1; ++ next, ++ niter_none, ++ biter_none) {
	// alignment into none...
	const int next_none = next + source_size + 2;
	const int prev_none1 = next;
	const int prev_none2 = next + source_size + 2;
	
	const prob_type transition_none = prob_null;
	
	const double prob1 = forward(trg - 1, prev_none1) * emission_none * transition_none;
	const double prob2 = forward(trg - 1, prev_none2) * emission_none * transition_none;
	
	if (prob1 > *niter_none) {
	  *niter_none = prob1;
	  *biter_none = prev_none1;
	}
	
	if (prob2 > *niter_none) {
	  *niter_none = prob2;
	  *biter_none = prev_none2;
	}
      }

      scale[trg] = std::accumulate(forward.begin(trg), forward.end(trg), 0.0);
      scale[trg] = (scale[trg] == 0.0 ? 1.0 : 1.0 / scale[trg]);
      
      if (scale[trg] != 1.0)
	std::transform(forward.begin(trg), forward.end(trg), forward.begin(trg), std::bind2nd(std::multiplies<double>(), scale[trg]));
    }
    
    // traverse-back...
    alignment.clear();
    index_type position = backptr(target_size + 2 - 1 , source_size + 2 - 1);
    for (int trg = target_size; trg >= 1; -- trg) {
      if (position < static_cast<int>(source_size + 2))
	alignment.push_back(std::make_pair(position - 1, trg - 1));
      position = backptr(trg, position);
    }
    
    std::sort(alignment.begin(), alignment.end());
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

  void shrink()
  {
    forward.clear();
    backptr.clear();
    scale.clear();
    
    source_class.clear();
    target_class.clear();
    
    forward_type(forward).swap(forward);
    backptr_type(backptr).swap(backptr);
    scale_type(scale).swap(scale);
    
    sentence_type(source_class).swap(source_class);
    sentence_type(target_class).swap(target_class);

    ViterbiBase::shrink();
  }

  typedef utils::vector2_aligned<prob_type, std::allocator<prob_type> > forward_type;
  typedef std::vector<prob_type, std::allocator<prob_type> >    scale_type;
  
  typedef utils::vector2_aligned<index_type, std::allocator<index_type> > backptr_type;
  
  forward_type  forward;
  backptr_type  backptr;
  scale_type    scale;

  sentence_type source_class;
  sentence_type target_class;
};



struct PosteriorHMM : public ViterbiBase
{
  typedef utils::vector2<double, std::allocator<double> > matrix_type;
  
  PosteriorHMM(const ttable_type& __ttable_source_target,
	       const ttable_type& __ttable_target_source,
	       const atable_type& __atable_source_target,
	       const atable_type& __atable_target_source,
	       const classes_type& __classes_source,
	       const classes_type& __classes_target)
    : ViterbiBase(__ttable_source_target, __ttable_target_source,
		  __atable_source_target, __atable_target_source,
		  __classes_source, __classes_target) {}
  
  typedef LearnHMM::hmm_data_type hmm_data_type;
  
  
  template <typename Matrix>
  void operator()(const sentence_type& source,
		  const sentence_type& target,
		  Matrix& posterior_source_target,
		  Matrix& posterior_target_source)
  {
    const size_type source_size = source.size();
    const size_type target_size = target.size();
    
    hmm_source_target.prepare(source, target, ttable_source_target, atable_source_target, classes_source, classes_target);
    hmm_target_source.prepare(target, source, ttable_target_source, atable_target_source, classes_target, classes_source);
    
    hmm_source_target.forward_backward(source, target);
    hmm_target_source.forward_backward(target, source);

    hmm_source_target.estimate_posterior(source, target);
    hmm_target_source.estimate_posterior(target, source);

    posterior_source_target.reserve(target_size + 1, source_size + 1);
    posterior_target_source.reserve(source_size + 1, target_size + 1);

    posterior_source_target.resize(target_size + 1, source_size + 1);
    posterior_target_source.resize(source_size + 1, target_size + 1);
    
    for (size_type trg = 0; trg <= target_size; ++ trg)
      std::copy(hmm_source_target.posterior.begin(trg), hmm_source_target.posterior.end(trg), posterior_source_target.begin(trg));
    for (size_type src = 0; src <= source_size; ++ src)
      std::copy(hmm_target_source.posterior.begin(src), hmm_target_source.posterior.end(src), posterior_target_source.begin(src));
  }

  void shrink()
  {
    hmm_source_target.shrink();
    hmm_target_source.shrink();

    ViterbiBase::shrink();
  }

  hmm_data_type hmm_source_target;
  hmm_data_type hmm_target_source;
};

struct ViterbiPosteriorHMM : public ViterbiBase
{
  //
  // we will compute both of viterbi and posterior...
  //
  typedef utils::vector2<double, std::allocator<double> > matrix_type;
  
  ViterbiPosteriorHMM(const ttable_type& __ttable_source_target,
		      const ttable_type& __ttable_target_source,
		      const atable_type& __atable_source_target,
		      const atable_type& __atable_target_source,
		      const classes_type& __classes_source,
		      const classes_type& __classes_target)
    : ViterbiBase(__ttable_source_target, __ttable_target_source,
		  __atable_source_target, __atable_target_source,
		  __classes_source, __classes_target) {}
  
  typedef LearnHMM::hmm_data_type hmm_data_type;
  

  template <typename Matrix>
  void viterbi(const sentence_type& source,
	       const sentence_type& target,
	       const ttable_type& ttable,
	       const atable_type& atable,
	       const classes_type& classes_source,
	       const classes_type& classes_target,
	       alignment_type& alignment,
	       Matrix& posterior)
  {
    const size_type source_size = source.size();
    const size_type target_size = target.size();
    
    hmm.prepare(source, target, ttable, atable, classes_source, classes_target);
    
    hmm.forward_backward(source, target);
    
    hmm.estimate_posterior(source, target);

    // posterior...    
    posterior.reserve(target_size + 1, source_size + 1);
    posterior.resize(target_size + 1, source_size + 1);
    
    for (size_type trg = 0; trg <= target_size; ++ trg)
      std::copy(hmm.posterior.begin(trg), hmm.posterior.end(trg), posterior.begin(trg));
    
    // compute viterbi...
    maximum.clear();
    backptr.clear();
    
    maximum.reserve(target_size + 2, (source_size + 2) * 2);
    backptr.reserve(target_size + 2, (source_size + 2) * 2);
    
    maximum.resize(target_size + 2, (source_size + 2) * 2, 0.0);
    backptr.resize(target_size + 2, (source_size + 2) * 2, -1);
    
    maximum(0, 0) = 1.0;

    for (size_type trg = 1; trg != target_size + 2; ++ trg) {
      
      // +1 to exclude BOS
      const prob_type* eiter = &(*hmm.emission.begin(trg)) + 1;
      prob_type*       niter = &(*maximum.begin(trg)) + 1;
      index_type*      biter = &(*backptr.begin(trg)) + 1;
      
      for (size_type next = 1; next != source_size + 2; ++ next, ++ niter, ++ eiter, ++ biter) {
	const double factor = *eiter;
	
	if (factor > 0.0) {
	  const prob_type* piter = &(*maximum.begin(trg - 1));
	  const prob_type* titer = &(*hmm.transition.begin(trg, next));
	  
	  for (size_type prev = 0; prev != (source_size + 2) * 2; ++ prev, ++ piter, ++ titer) {
	    const double prob = factor * (*titer) * (*piter);
	    
	    if (prob >= *niter) {
	      *niter = prob;
	      *biter = prev;
	    }
	  }
	}
      }
      
      prob_type*  niter_none = &(*maximum.begin(trg)) + source_size + 2;
      index_type* biter_none = &(*backptr.begin(trg)) + source_size + 2;
      const prob_type* eiter_none = &(*hmm.emission.begin(trg)) + (source_size + 2);
      const prob_type* piter_none1 = &(*maximum.begin(trg - 1));
      const prob_type* piter_none2 = piter_none1 + (source_size + 2);
      
      for (size_type next = 0; next != (source_size + 2) - 1; ++ next, ++ niter_none, ++ biter_none, ++ eiter_none, ++ piter_none1, ++ piter_none2) {
	// alignment into none...
	const size_type next_none = next + source_size + 2;
	const size_type prev_none1 = next;
	const size_type prev_none2 = next + source_size + 2;
	
	const double prob1 = (*piter_none1) * (*eiter_none) * hmm.transition(trg, next_none, prev_none1);
	const double prob2 = (*piter_none2) * (*eiter_none) * hmm.transition(trg, next_none, prev_none2);
	
	if (prob1 > *niter_none) {
	  *niter_none = prob1;
	  *biter_none = prev_none1;
	}
	
	if (prob2 > *niter_none) {
	  *niter_none = prob2;
	  *biter_none = prev_none2;
	}
      }
      
      // scaling...
      if (hmm.scale[trg] != 1.0)
	std::transform(maximum.begin(trg), maximum.end(trg), maximum.begin(trg), std::bind2nd(std::multiplies<double>(), hmm.scale[trg]));
    }

    // traverse-back...
    alignment.clear();
    index_type position = backptr(target_size + 2 - 1 , source_size + 2 - 1);
    for (int trg = target_size; trg >= 1; -- trg) {
      if (position < static_cast<int>(source_size + 2))
	alignment.push_back(std::make_pair(position - 1, trg - 1));
      position = backptr(trg, position);
    }
    
    std::sort(alignment.begin(), alignment.end());
  }
  
  template <typename Matrix>
  void operator()(const sentence_type& source,
		  const sentence_type& target,
		  alignment_type& alignment_source_target,
		  alignment_type& alignment_target_source,
		  Matrix& posterior_source_target,
		  Matrix& posterior_target_source)
  {
    viterbi(source, target, ttable_source_target, atable_source_target, classes_source, classes_target, alignment_source_target, posterior_source_target);
    viterbi(target, source, ttable_target_source, atable_target_source, classes_target, classes_source, alignment_target_source, posterior_target_source);
  }

  void shrink()
  {
    maximum.clear();
    backptr.clear();
    
    maximum_type(maximum).swap(maximum);
    backptr_type(backptr).swap(backptr);

    hmm.shrink();
    
    ViterbiBase::shrink();
  }
  
  typedef utils::vector2_aligned<prob_type, std::allocator<prob_type> > maximum_type;
  typedef utils::vector2_aligned<index_type, std::allocator<index_type> > backptr_type;
  
  maximum_type  maximum;
  backptr_type  backptr;

  hmm_data_type hmm;
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
    costs.reserve(source_size + 1, target_size + 1);
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

  void shrink()
  {
    costs.clear();
    matrix_type(costs).swap(costs);
    
    hmm_source_target.shrink();
    hmm_target_source.shrink();

    aligner.shrink();

    ViterbiBase::shrink();
  }

  matrix_type costs;
  
  hmm_data_type hmm_source_target;
  hmm_data_type hmm_target_source;
  
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
    costs.reserve(source_size + target_size, target_size + source_size);
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

  void shrink()
  {
    costs.clear();
    matrix_type(costs).swap(costs);    
    
    hmm_source_target.shrink();
    hmm_target_source.shrink();

    ViterbiBase::shrink();
  }
  
  matrix_type costs;

  hmm_data_type hmm_source_target;
  hmm_data_type hmm_target_source;
};

struct DependencyHMM : public ViterbiBase
{
  typedef utils::vector2<double, std::allocator<double> > matrix_type;
  typedef utils::vector2<double, std::allocator<double> > posterior_set_type;
  typedef std::vector<double, std::allocator<double> > prob_set_type;
  
  DependencyHMM(const ttable_type& __ttable_source_target,
		const ttable_type& __ttable_target_source,
		const atable_type& __atable_source_target,
		const atable_type& __atable_target_source,
		const classes_type& __classes_source,
		const classes_type& __classes_target)
    : ViterbiBase(__ttable_source_target, __ttable_target_source,
		  __atable_source_target, __atable_target_source,
		  __classes_source, __classes_target) {}
  
  typedef LearnHMM::hmm_data_type hmm_data_type;
  
  void operator()(const sentence_type& source,
		  const sentence_type& target,
		  const dependency_type& dependency_source,
		  const dependency_type& dependency_target)
  {
    const size_type source_size = source.size();
    const size_type target_size = target.size();
    
    hmm_source_target.prepare(source, target, ttable_source_target, atable_source_target, classes_source, classes_target);
    hmm_target_source.prepare(target, source, ttable_target_source, atable_target_source, classes_target, classes_source);
    
    hmm_source_target.forward_backward(source, target);
    hmm_target_source.forward_backward(target, source);
    
    hmm_source_target.estimate_posterior(source, target);
    hmm_target_source.estimate_posterior(target, source);
    
    static const double lowest = - std::numeric_limits<double>::infinity();
    
    scores.clear();
    scores.reserve(source_size + 1, target_size + 1);
    scores.resize(source_size + 1, target_size + 1, lowest);
    
    for (size_type src = 1; src <= source_size; ++ src)
      for (size_type trg = 1; trg <= target_size; ++ trg)
	scores(src, trg) = 0.5 * (utils::mathop::log(hmm_source_target.posterior(trg, src)) 
				  + utils::mathop::log(hmm_target_source.posterior(src, trg)));
    
    if (! dependency_source.empty()) {
      if (dependency_source.size() != source_size)
	throw std::runtime_error("dependency size do not match");
      
      scores_target.clear();
      scores_target.reserve(target_size + 1, target_size + 1);
      scores_target.resize(target_size + 1, target_size + 1, lowest);
      
      // we will compute the score matrix...
      for (size_type trg_head = 1; trg_head <= target_size; ++ trg_head)
	for (size_type trg_dep = 1; trg_dep <= target_size; ++ trg_dep)
	  if (trg_head != trg_dep)
	    for (size_type src = 0; src != dependency_source.size(); ++ src) 
	      if (dependency_source[src]) {
		const size_type src_head = dependency_source[src];
		const size_type src_dep  = src + 1;
		
		const double score = scores(src_head, trg_head) + scores(src_dep, trg_dep);
		
		scores_target(trg_head, trg_dep) = utils::mathop::logsum(scores_target(trg_head, trg_dep), score);
	      }
      
      // this is for the root...
      for (size_type trg_dep = 1; trg_dep <= target_size; ++ trg_dep)
	for (size_type src = 0; src != dependency_source.size(); ++ src) 
	  if (! dependency_source[src]) {
	    const size_type trg_head = 0;
	    const size_type src_head = dependency_source[src];
	    const size_type src_dep  = src + 1;
	    
	    const double score = scores(src_dep, trg_dep);

	    scores_target(trg_head, trg_dep) = utils::mathop::logsum(scores_target(trg_head, trg_dep), score);
	  }
    }
    
    if (! dependency_target.empty()) {
      if (dependency_target.size() != target_size)
	throw std::runtime_error("dependency size do not match");

      scores_source.clear();
      scores_source.reserve(source_size + 1, source_size + 1);
      scores_source.resize(source_size + 1, source_size + 1, lowest);
      
      // we will compute the score matrix...
      for (size_type src_head = 1; src_head <= source_size; ++ src_head)
	for (size_type src_dep = 1; src_dep <= source_size; ++ src_dep)
	  if (src_head != src_dep)
	    for (size_type trg = 0; trg != dependency_target.size(); ++ trg)
	      if (dependency_target[trg]) {
		const size_type trg_head = dependency_target[trg];
		const size_type trg_dep  = trg + 1;
		
		const double score = scores(src_head, trg_head) + scores(src_dep, trg_dep);
		
		scores_source(src_head, src_dep) = utils::mathop::logsum(scores_source(src_head, src_dep), score);
	      }
      
      // this is for the root.
      for (size_type src_dep = 1; src_dep <= source_size; ++ src_dep)
	for (size_type trg = 0; trg != dependency_target.size(); ++ trg)
	  if (! dependency_target[trg]) {
	    const size_type src_head = 0;
	    const size_type trg_head = dependency_target[trg];
	    const size_type trg_dep  = trg + 1;
	    
	    const double score = scores(src_dep, trg_dep);
	    
	    scores_source(src_head, src_dep) = utils::mathop::logsum(scores_source(src_head, src_dep), score);
	  }
    }
  }

  void shrink()
  {
    scores_source.clear();
    scores_target.clear();
    scores.clear();

    matrix_type(scores_source).swap(scores_source);
    matrix_type(scores_target).swap(scores_target);
    matrix_type(scores).swap(scores);

    hmm_source_target.shrink();
    hmm_target_source.shrink();
    
    ViterbiBase::shrink();
  }
  
  matrix_type scores_source;
  matrix_type scores_target;
  matrix_type scores;
  
  hmm_data_type hmm_source_target;
  hmm_data_type hmm_target_source;
};

template <typename Analyzer>
struct __DependencyHMMBase : public DependencyHMM
{
  
  typedef Analyzer analyzer_type;
  
  __DependencyHMMBase(const ttable_type& __ttable_source_target,
		      const ttable_type& __ttable_target_source,
		      const atable_type& __atable_source_target,
		      const atable_type& __atable_target_source,
		      const classes_type& __classes_source,
		      const classes_type& __classes_target)
    : DependencyHMM(__ttable_source_target, __ttable_target_source,
		    __atable_source_target, __atable_target_source,
		    __classes_source, __classes_target) {}
  
  void operator()(const sentence_type& source,
		  const sentence_type& target,
		  const dependency_type& dependency_source,
		  const dependency_type& dependency_target,
		  dependency_type& projected_source,
		  dependency_type& projected_target)
  {
    DependencyHMM::operator()(source, target, dependency_source, dependency_target);
    
    const size_type source_size = source.size();
    const size_type target_size = target.size();

    projected_source.clear();
    projected_target.clear();
    
    if (! dependency_source.empty()) {
      projected_target.resize(target_size, - 1);
      
      analyzer(scores_target, projected_target);
    }
    
    if (! dependency_target.empty()) {
      projected_source.resize(source_size, - 1);
      
      analyzer(scores_source, projected_source);
    }
  }

  void shink()
  {
    analyzer.shrink();
    DependencyHMM::shrink();
  }
  
  analyzer_type analyzer;
};

typedef __DependencyHMMBase<DependencyHybrid>            DependencyHybridHMM;
typedef __DependencyHMMBase<DependencyHybridSingleRoot>  DependencyHybridSingleRootHMM;
typedef __DependencyHMMBase<DependencyDegree2>           DependencyDegree2HMM;
typedef __DependencyHMMBase<DependencyDegree2SingleRoot> DependencyDegree2SingleRootHMM;
typedef __DependencyHMMBase<DependencyMST>               DependencyMSTHMM;
typedef __DependencyHMMBase<DependencyMSTSingleRoot>     DependencyMSTSingleRootHMM;

struct PermutationHMM : public ViterbiBase
{
  typedef utils::vector2<double, std::allocator<double> > matrix_type;
  typedef utils::vector2<double, std::allocator<double> > posterior_set_type;
  typedef std::vector<double, std::allocator<double> > prob_set_type;

  typedef std::vector<bool, std::allocator<bool > > assigned_type;
  
  PermutationHMM(const ttable_type& __ttable_source_target,
		 const ttable_type& __ttable_target_source,
		 const atable_type& __atable_source_target,
		 const atable_type& __atable_target_source,
		 const classes_type& __classes_source,
		 const classes_type& __classes_target)
    : ViterbiBase(__ttable_source_target, __ttable_target_source,
		  __atable_source_target, __atable_target_source,
		  __classes_source, __classes_target) {}
  
  typedef LearnHMM::hmm_data_type hmm_data_type;
  
  void operator()(const sentence_type& source,
		  const sentence_type& target,
		  const dependency_type& permutation_source,
		  const dependency_type& permutation_target)
  {
    const size_type source_size = source.size();
    const size_type target_size = target.size();
    
    hmm_source_target.prepare(source, target, ttable_source_target, atable_source_target, classes_source, classes_target);
    hmm_target_source.prepare(target, source, ttable_target_source, atable_target_source, classes_target, classes_source);
    
    hmm_source_target.forward_backward(source, target);
    hmm_target_source.forward_backward(target, source);
    
    hmm_source_target.estimate_posterior(source, target);
    hmm_target_source.estimate_posterior(target, source);
    
    static const double lowest = - std::numeric_limits<double>::infinity();
    
    scores.clear();
    scores.reserve(source_size + 1, target_size + 1);
    scores.resize(source_size + 1, target_size + 1, lowest);
    
    for (size_type src = 1; src <= source_size; ++ src)
      for (size_type trg = 1; trg <= target_size; ++ trg)
	scores(src, trg) = 0.5 * (utils::mathop::log(hmm_source_target.posterior(trg, src)) 
				  + utils::mathop::log(hmm_target_source.posterior(src, trg)));
    
    if (! permutation_source.empty()) {
      if (permutation_source.size() != source_size)
	throw std::runtime_error("permutation size do not match");
      
      scores_target.clear();
      scores_target.reserve(target_size + 1, target_size + 1);
      scores_target.resize(target_size + 1, target_size + 1, lowest);
      
      // transform permutation into dependency...
      dependency_source.clear();
      dependency_source.resize(source_size, 0);
      
      const size_type src_leaf = project_permutation(permutation_source, dependency_source);
      if (src_leaf == size_type(-1))
	throw std::runtime_error("no leaf?");
      
      // we will compute the score matrix...
      for (size_type trg_head = 1; trg_head <= target_size; ++ trg_head)
	for (size_type trg_dep = 1; trg_dep <= target_size; ++ trg_dep)
	  if (trg_head != trg_dep)
	    for (size_type src = 0; src != dependency_source.size(); ++ src) 
	      if (dependency_source[src]) {
		const size_type src_head = dependency_source[src];
		const size_type src_dep  = src + 1;
		
		const double score = scores(src_head, trg_head) + scores(src_dep, trg_dep);
		
		scores_target(trg_head, trg_dep) = utils::mathop::logsum(scores_target(trg_head, trg_dep), score);
	      }
      
      // this is for the root...
      for (size_type trg_dep = 1; trg_dep <= target_size; ++ trg_dep)
	for (size_type src = 0; src != dependency_source.size(); ++ src) 
	  if (! dependency_source[src]) {
	    const size_type trg_head = 0;
	    const size_type src_head = dependency_source[src];
	    const size_type src_dep  = src + 1;
	    
	    const double score = scores(src_dep, trg_dep);
	    
	    scores_target(trg_head, trg_dep) = utils::mathop::logsum(scores_target(trg_head, trg_dep), score);
	  }
      
#if 0
      for (size_type trg_head = 1; trg_head <= target_size; ++ trg_head)
	scores_target(trg_head, 0) = scores(src_leaf, trg_head);
#endif
#if 1
      for (size_type trg_head = 1; trg_head <= target_size; ++ trg_head)
	scores_target(trg_head, 0) = 0.0;
#endif
    }
    
    if (! permutation_target.empty()) {
      if (permutation_target.size() != target_size)
	throw std::runtime_error("permutation size do not match");

      scores_source.clear();
      scores_source.reserve(source_size + 1, source_size + 1);
      scores_source.resize(source_size + 1, source_size + 1, lowest);
      
      // transform permutation into dependency...
      dependency_target.clear();
      dependency_target.resize(target_size, 0);
      
      const size_type trg_leaf = project_permutation(permutation_target, dependency_target);
      if (trg_leaf == size_type(-1))
	throw std::runtime_error("no leaf?");
      
      // we will compute the score matrix...
      for (size_type src_head = 1; src_head <= source_size; ++ src_head)
	for (size_type src_dep = 1; src_dep <= source_size; ++ src_dep)
	  if (src_head != src_dep)
	    for (size_type trg = 0; trg != dependency_target.size(); ++ trg)
	      if (dependency_target[trg]) {
		const size_type trg_head = dependency_target[trg];
		const size_type trg_dep  = trg + 1;
		
		const double score = scores(src_head, trg_head) + scores(src_dep, trg_dep);
		
		scores_source(src_head, src_dep) = utils::mathop::logsum(scores_source(src_head, src_dep), score);
	      }
      
      // this is for the root.
      for (size_type src_dep = 1; src_dep <= source_size; ++ src_dep)
	for (size_type trg = 0; trg != dependency_target.size(); ++ trg)
	  if (! dependency_target[trg]) {
	    const size_type src_head = 0;
	    const size_type trg_head = dependency_target[trg];
	    const size_type trg_dep  = trg + 1;
	    
	    const double score = scores(src_dep, trg_dep);
	    
	    scores_source(src_head, src_dep) = utils::mathop::logsum(scores_source(src_head, src_dep), score);
	  }
#if 0
      for (size_type src_head = 1; src_head <= source_size; ++ src_head)
	scores_source(src_head, 0) = scores(src_head, trg_leaf);
#endif
#if 1
      for (size_type src_head = 1; src_head <= source_size; ++ src_head)
	scores_source(src_head, 0) = 0.0;
#endif

    }
  }
  
  template <typename Dependency>
  struct insert_dependency
  {
    Dependency& dependency;
    
    insert_dependency(Dependency& __dependency) : dependency(__dependency) {}

    template <typename Edge>
    insert_dependency& operator=(const Edge& edge)
    {
      if (edge.second)
	dependency[edge.second - 1] = edge.first;
      return *this;
    }
    
    insert_dependency& operator*() { return *this; }
    insert_dependency& operator++() { return *this; }
    insert_dependency operator++(int) { return *this; }
  };

  void operator()(const sentence_type& source,
		  const sentence_type& target,
		  const dependency_type& permutation_source,
		  const dependency_type& permutation_target,
		  dependency_type& projected_source,
		  dependency_type& projected_target)
  {
    //std::cerr << "posterior" << std::endl;
    
    operator()(source, target, permutation_source, permutation_target);
    
    const size_type source_size = source.size();
    const size_type target_size = target.size();

    projected_source.clear();
    projected_target.clear();
    
    if (! permutation_source.empty()) {
      dependency_target.resize(target_size, - 1);
      projected_target.resize(target_size, - 1);
      
      dependency_per.resize(target_size);
      dependency_mst.resize(target_size);
      
      scores_per = scores_target;
      scores_mst = scores_target;
      
      std::cerr << "optimal dependency" << std::endl;
      
      // we will perform dual decomposition!
      // we will minimize the loss...
      static const double inf = std::numeric_limits<double>::infinity();

      double dual = inf;
      size_type epoch = 0;
      
      // 50 iteratins will be fine...?
      bool converged = false;
      for (;;) {
	kuhn_munkres_assignment(scores_per, insert_dependency<dependency_type>(dependency_per));
	mst(scores_mst, dependency_mst);
	
	std::cerr << "permutation: " << dependency_per << std::endl;
	std::cerr << "mst: " << dependency_mst << std::endl;

	double dual_per = 0.0;
	double dual_mst = 0.0;
	double primal_per = 0.0;
	double primal_mst = 0.0;
	double weight_diff_min = std::numeric_limits<double>::infinity();
									    
	for (size_type i = 1; i <= target_size; ++ i) {
	  dual_per += scores_per(dependency_per[i - 1], i);
	  dual_mst += scores_mst(dependency_mst[i - 1], i);
	  primal_per += scores_target(dependency_per[i - 1], i);
	  primal_mst += scores_target(dependency_mst[i - 1], i);
	  
	  if (dependency_per[i - 1] != dependency_mst[i - 1]) {
	    const double diff = std::fabs(scores_target(dependency_per[i - 1], i) - scores_target(dependency_mst[i - 1], i));
	    
	    weight_diff_min = std::min(weight_diff_min, diff);
	  }
	}

	const double dual_curr = dual_per + dual_mst;
	const double primal_curr = primal_per + primal_mst;
	
#if 1
	std::cerr << "dual: " << dual_curr
		  << " per: " << dual_per
		  << " mst: " << dual_mst << std::endl;
	std::cerr << "primal: " << primal_curr
		  << " per: " << primal_per
		  << " mst: " << primal_mst << std::endl;
#endif
	
	//
	// if either dependency_per is sound, we will use the dependency_per
	//

	// if equal, simply quit.
	if (dependency_mst == dependency_per) {
	  dependency_target = dependency_mst;
	  converged = true;
	  break;
	}
	
	const bool sound_per = is_permutation(dependency_per);
	const bool sound_mst = is_permutation(dependency_mst);
	
	if (sound_per || sound_mst) {
	  if (sound_per && sound_mst) {
	    // find the maximum
	    double score_per = 0.0;
	    double score_mst = 0.0;
	    
	    for (size_type i = 1; i <= target_size; ++ i) {
	      score_per += scores_target(dependency_per[i - 1], i);
	      score_mst += scores_target(dependency_mst[i - 1], i);
	    }
	    
	    if (score_per > score_mst)
	      dependency_target = dependency_per;
	    else
	      dependency_target = dependency_mst;
	  } else if (sound_per)
	    dependency_target = dependency_per;
	  else
	    dependency_target = dependency_mst;
	  
	  converged = true;
	  break;
	}
	
	const double alpha = weight_diff_min / (1.0 + epoch);
		
	for (size_type i = 1; i <= target_size; ++ i)
	  for (size_type j = 0; j <= target_size; ++ j) 
	    if (i != j) {
	      const int y = (dependency_mst[i - 1] == static_cast<int>(j));
	      const int z = (dependency_per[i - 1] == static_cast<int>(j));
	      
	      const double update = - alpha * (y - z);
	      
	      scores_mst(i, j) += update;
	      scores_per(i, j) -= update;
	    }
	
	// increase time when dual increased..
	//epoch += (dual_curr > dual);
	dual = dual_curr;
      }
      
      std::cerr << "finished" << std::endl;
      
      
      if (! converged) {
	//
	// we will fall-back to hill-climbing solution, starting from the permutation solution... HOW?
	//
	
      }
      
      
      //std::cerr << "project dependency" << std::endl;

      project_dependency(dependency_target, projected_target);
    }
    
    if (! permutation_target.empty()) {
      dependency_source.resize(source_size, - 1);
      projected_source.resize(source_size, - 1);
      
      kuhn_munkres_assignment(scores_source, insert_dependency<dependency_type>(dependency_source));
      
      project_dependency(dependency_source, projected_source);
    }
  }

  size_type project_permutation(const dependency_type& permutation, dependency_type& dependency)
  {
    const size_type size = permutation.size();

    dependency.resize(size);

    if (size == 1) {
      dependency.front() = 0;
      return 1;
    }
    
    assigned.clear();
    assigned.resize(size, false);

    size_type leaf = size_type(-1);

    for (size_type pos = 0; pos != size; ++ pos) {
      if (permutation[pos] >= static_cast<int>(size))
	throw std::runtime_error("invalid permutation: out of range");
      
      if (assigned[permutation[pos]])
	throw std::runtime_error("invalid permutation: duplicates");
      
      assigned[permutation[pos]] = true;
      
      if (permutation[pos] == static_cast<int>(size - 1))
	leaf = pos + 1;
      
      if (! permutation[pos]) continue;
      
      dependency_type::const_iterator iter = std::find(permutation.begin(), permutation.end(), permutation[pos] - 1);
      if (iter == permutation.end())
	throw std::runtime_error("invalid permutation: no previous index?");
      
      dependency[pos] = (iter - permutation.begin()) + 1;
    }
    
    return leaf;
  }

  bool is_permutation(const dependency_type& dependency)
  {
    const size_type size = dependency.size();
    
    if (size <= 1) return true;
    
    dependency_type::const_iterator diter_begin = dependency.begin();
    dependency_type::const_iterator diter_end   = dependency.end();
    
    size_type pos_head = 0;
    for (size_type i = 0; i != size; ++ i) {
      dependency_type::const_iterator diter = std::find(diter_begin, diter_end, pos_head);
      if (diter == diter_end)
	return false;
      pos_head = (diter - diter_begin) + 1;
    }
    return true;
  }

  void project_dependency(const dependency_type& dependency, dependency_type& permutation)
  {
    const size_type size = dependency.size();
    
    permutation.resize(size);
    
    if (size == 1) {
      permutation.front() = 0;
      return;
    }

    //std::cerr << "dependency: " << dependency << std::endl;
    
    dependency_type::const_iterator diter_begin = dependency.begin();
    dependency_type::const_iterator diter_end   = dependency.end();
    
    size_type pos_head = 0;
    for (size_type i = 0; i != size; ++ i) {
      dependency_type::const_iterator diter = std::find(diter_begin, diter_end, pos_head);
      if (diter == diter_end)
	throw std::runtime_error("no head?");
      
      const size_type pos_dep = (diter - diter_begin) + 1;
      
      //std::cerr << "pos-dep: " << pos_dep << std::endl;

      permutation[pos_dep - 1] = i;
      pos_head = pos_dep;
    }

    //std::cerr << "permutation: " << permutation << std::endl;
    
  }

  void shrink()
  {
    scores_source.clear();
    scores_target.clear();
    scores.clear();

    matrix_type(scores_source).swap(scores_source);
    matrix_type(scores_target).swap(scores_target);
    matrix_type(scores).swap(scores);

    hmm_source_target.shrink();
    hmm_target_source.shrink();

    ViterbiBase::shrink();
  }
  
  matrix_type scores_source;
  matrix_type scores_target;
  matrix_type scores;
  
  hmm_data_type hmm_source_target;
  hmm_data_type hmm_target_source;

  assigned_type   assigned;
  dependency_type dependency_source;
  dependency_type dependency_target;
  
  matrix_type scores_per;
  matrix_type scores_mst;

  dependency_type dependency_mst;
  dependency_type dependency_per;
  
  DependencyMSTSingleRoot mst;
};


#endif
