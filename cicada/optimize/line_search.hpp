// -*- mode: c++ -*-
//
//  Copyright(C) 2010-2011 Taro Watanabe <taro.watanabe@nict.go.jp>
//

#ifndef __CICADA__OPTIMIZE__LINE_SEARCH__HPP__
#define __CICADA__OPTIMIZE__LINE_SEARCH__HPP__ 1

#include <vector>
#include <deque>
#include <algorithm>

#include <cicada/hypergraph.hpp>
#include <cicada/weight_vector.hpp>
#include <cicada/dot_product.hpp>

#include <cicada/semiring/envelope.hpp>
#include <cicada/eval/score.hpp>

namespace cicada
{
  namespace optimize
  {
    struct LineSearch
    {
      typedef HyperGraph hypergraph_type;
      
      typedef hypergraph_type::feature_set_type feature_set_type;
      typedef cicada::WeightVector<double>      weight_set_type;
      typedef feature_set_type::feature_type    feature_type;

      typedef semiring::Envelope       envelope_type;
      
      typedef envelope_type::line_type line_type;
      typedef eval::Score              score_type;
      
      typedef boost::shared_ptr<line_type>  line_ptr_type;
      typedef boost::shared_ptr<score_type> score_ptr_type;

      typedef std::vector<score_ptr_type, std::allocator<score_ptr_type> >   score_set_type;
      
      struct segment_type
      {
	double x;
	double m;
	double y;
	
	score_ptr_type score;
	
	segment_type(const std::pair<double, score_ptr_type>& point) : x(point.first), m(0.0), y(0.0), score(point.second) {}
	segment_type(const double& __x,
		     const score_ptr_type& __score) : x(__x), m(0.0), y(0.0), score(__score) {}
	segment_type(const double& __x,
		     const double& __m,
		     const double& __y,
		     const score_ptr_type& __score) : x(__x), m(__m), y(__y), score(__score) {}
	segment_type() : x(0.0), m(0.0), y(0.0), score() {}
      };
      
      typedef std::vector<segment_type, std::allocator<segment_type> >        segment_set_type;
      typedef std::deque<segment_set_type, std::allocator<segment_set_type> > segment_document_type;
      
      static const double interval_min;
      static const double interval_offset_lower;
      static const double interval_offset_upper;
      static double value_min;
      static double value_max;
      
    public:
      struct RegularizeNone
      {
	RegularizeNone(const double& __scale, const weight_set_type& __origin, const weight_set_type& __direction)
	{}
	RegularizeNone(const double& __scale)
	{}
	RegularizeNone()
	{}
	
	double operator()(const weight_set_type& weights) const
	{
	  return 0.0;
	}
	
	double operator()(const double& k) const
	{
	  return 0.0;
	}

	double operator()(const weight_set_type& origin,
			  const weight_set_type& direction,
			  const double& lower,
			  const double& upper) const
	{
	  return 0.0;
	}
      };
      
      struct RegularizeL1
      {
	double scale;
	
	RegularizeL1(const double& __scale, const weight_set_type& __origin, const weight_set_type& __direction)
	  : scale(__scale) {}
	RegularizeL1(const double& __scale)
	  : scale(__scale) {}
	
	double operator()(const weight_set_type& weights) const
	{
	  double total = 0.0;
	  weight_set_type::const_iterator witer_end = weights.end();
	  for (weight_set_type::const_iterator witer = weights.begin(); witer != witer_end; ++ witer)
	    total += std::fabs(*witer);
	  
	  return total * scale;
	}
  
	double operator()(const double& k) const
	{
	  return 0.0;
	}

	double operator()(const weight_set_type& origin,
			  const weight_set_type& direction,
			  const double& lower,
			  const double& upper) const
	{
	  double total = 0.0;
	
	  for (feature_type::id_type id = 0; id < feature_type::allocated(); ++ id)
	    if (! feature_type(id).empty()) {
	      const feature_type feature(id);
	    
	      const double ori = origin[feature];
	      const double dir = direction[feature];
	    
	      const double low = ori + lower * dir;
	      const double upp = ori + upper * dir;
	    
	      const double weight = (ori + (low + upp) * 0.5 * dir) * double(! (low * upp < 0.0));
	      
	      total += std::fabs(weight);
	    }
	  
	  return total * scale;
	}
      };
      
      struct RegularizeL2
      {
	double scale;
	double o2;
	double od;
	double d2;
      
	RegularizeL2(const double& __scale, const weight_set_type& origin, const weight_set_type& direction)
	  : scale(__scale)
	{
	  // 0.5 * \lambda * w^2 + k * \lambda * w * d + k^2 * 0.5 * \lambda * d * d
	  
	  o2 = scale * 0.5 * cicada::dot_product(origin, origin);
	  od = scale * cicada::dot_product(origin, direction);
	  d2 = scale * 0.5 * cicada::dot_product(direction, direction);
	}
	
	RegularizeL2(const double& __scale)
	  : scale(__scale), o2(0.0), od(0.0), d2(0.0) {}
	
	double operator()(const double& k) const
	{
	  return o2 + k * od + k * k * d2;
	}

	double operator()(const weight_set_type& weights) const
	{
	  double total = 0.0;
	  weight_set_type::const_iterator witer_end = weights.end();
	  for (weight_set_type::const_iterator witer = weights.begin(); witer != witer_end; ++ witer)
	    total += (*witer) * (*witer);
	
	  return total * scale;
	}
  
	double operator()(const weight_set_type& origin,
			  const weight_set_type& direction,
			  const double& lower,
			  const double& upper) const
	{
	  double total = 0.0;
	
	  for (feature_type::id_type id = 0; id < feature_type::allocated(); ++ id)
	    if (! feature_type(id).empty()) {
	      const feature_type feature(id);
	    
	      const double ori = origin[feature];
	      const double dir = direction[feature];
	    
	      const double low = ori + lower * dir;
	      const double upp = ori + upper * dir;
	    
	      const double weight = (ori + (low + upp) * 0.5 * dir) * double(! (low * upp < 0.0));
	    
	      total += weight * weight;
	    }
	
	  return total * 0.5 * scale;
	}
      };

      struct Result
      {
	double objective;
	double score;
	double lower;
	double upper;
	
	weight_set_type operator()(const weight_set_type& origin,
				   const weight_set_type& direction) const
	{
	  weight_set_type weights;
	  
	  for (feature_type::id_type id = 0; id < feature_type::allocated(); ++ id)
	    if (! feature_type(id).empty()) {
	      const feature_type feature(id);

	      const double weight = origin[feature];
	      const double dir    = direction[feature];
	      
	      const double feature_lower = weight + lower * dir;
	      const double feature_upper = weight + upper * dir;
	      
	      const double average = (lower + upper) * 0.5;
	      
	      // if we cross minus and plus, force zero...
	      
	      weights[feature] = (weight + average * dir) * double(! (feature_lower * feature_upper < 0.0));
	    }
	  return weights;
	}
	
	
	Result() : objective(std::numeric_limits<double>::infinity()), score(0.0), lower(0.0), upper(0.0) {}
	
	Result(const double& _objective,
	       const double& _score,
	       const double& _lower,
	       const double& _upper)
	  : objective(_objective), score(_score), lower(_lower), upper(_upper) {}
      };
      typedef Result value_type;

    private:
      struct Item
      {
	int seg;
	
	segment_set_type::const_iterator first;
	segment_set_type::const_iterator last;

	Item() : seg(), first(), last() {}
	Item(const int& __seg,
	     segment_set_type::const_iterator __first,
	     segment_set_type::const_iterator __last)
	  : seg(__seg), first(__first), last(__last) {}
      };
      typedef Item item_type;
      
      struct item_heap_compare_type
      {
	bool operator()(const item_type& x, const item_type& y) const
	{
	  return x.first->x > y.first->x;
	}
      };
      
      typedef std::vector<item_type, std::allocator<item_type> > heap_type;

      
    public:
      LineSearch(const int __debug = 0)
	: bound_lower(),
 	  bound_upper(),
	  debug(__debug) { initialize_bound(bound_lower, bound_upper); }
      
      LineSearch(const weight_set_type& __bound_lower,
		 const weight_set_type& __bound_upper,
		 const int __debug = 0)
	: bound_lower(__bound_lower),
	  bound_upper(__bound_upper),
	  debug(__debug) { initialize_bound(bound_lower, bound_upper); }

      template <typename Iterator>
      void operator()(const segment_document_type& segments,
		      const double value_min,
		      const double value_max,
		      Iterator iter)
      {
	// we assume a set of line_ptr and score_ptr pair...
	
	const std::pair<double, double> range(value_min, value_max);

	heap_type heap;
	
	if (debug >= 4)
	  std::cerr << "minimum: " << range.first
		    << " maximum: " << range.second
		    << " segments: " << segments.size()
		    << std::endl;
	
	score_ptr_type stat;
	score_set_type scores(segments.size());
	
	for (size_t seg = 0; seg != segments.size(); ++ seg)
	  if (! segments[seg].empty()) {
	    
	    if (! stat)
	      stat = segments[seg].front().score->zero();
	    
	    scores[seg] = segments[seg].front().score;
	    *stat += *segments[seg].front().score;
	    
	    if (segments[seg].size() > 1)
	      heap.push_back(item_type(seg, segments[seg].begin() + 1, segments[seg].end()));
	  }

	if (heap.empty()) return;
	
	// priority queue...
	std::make_heap(heap.begin(), heap.end(), item_heap_compare_type());
	
	const double score = stat->loss();
	
	double lower = lower_bound(heap.front().first->x, range.first);
	double upper = heap.front().first->x;
	
	double objective = score;
	
	double segment_prev = lower;
	double score_prev   = score;
	
	*iter = value_type(objective, score, lower, upper);
	++ iter;
	
	if (debug >= 4)
	  std::cerr << "lower: " << lower
		    << " upper: " << upper
		    << " score: " << score
		    << " objective: " << objective
		    << std::endl;
	
	while (! heap.empty()) {
	  // next at heap...
	  
	  const double segment_curr = heap.front().first->x;
	  
	  while (! heap.empty() && heap.front().first->x == segment_curr) { 
	    std::pop_heap(heap.begin(), heap.end(), item_heap_compare_type());
	    
	    *stat -= *scores[heap.back().seg];
	    *stat += *heap.back().first->score;
	    scores[heap.back().seg] = heap.back().first->score;
	    
	    // pop and push heap...
	    ++ heap.back().first;
	    if (heap.back().first != heap.back().last)
	      std::push_heap(heap.begin(), heap.end(), item_heap_compare_type());
	    else
	      heap.pop_back();
	  }
	  
	  const double segment_next = (heap.empty() ? upper_bound(segment_curr, range.second) : heap.front().first->x);
	  
	  // we perform merging of ranges if error counts are equal...
	  const double score = stat->loss();
	  if (score != score_prev) {
	    segment_prev = segment_curr;
	    score_prev = score;
	  }
	  
	  const double lower = segment_prev;
	  const double upper = segment_next;
	  const double point = (lower + upper) * 0.5;
	  
	  if (point > range.second) break;   // out of range for upper-bound, quit!
	  if (point < range.first) continue; // out of range for lower-bound...
	  if (std::fabs(point) < interval_min) continue; // interval is very small
	  
	  const double objective = score;
	  
	  if (debug >= 4)
	    std::cerr << "lower: " << lower
		      << " upper: " << upper
		      << " score: " << score
		      << " objective: " << objective
		      << std::endl;
	  
	  *iter = value_type(objective, score, lower, upper);
	  ++ iter;
	}
      }

      value_type operator()(const segment_document_type& segments,
			    const double value_min,
			    const double value_max)
      {
	// we assume a set of line_ptr and score_ptr pair...
	const std::pair<double, double> range(value_min, value_max);

	heap_type heap;
	
	if (debug >= 4)
	  std::cerr << "minimum: " << range.first
		    << " maximum: " << range.second
		    << " segments: " << segments.size()
		    << std::endl;
	
	score_ptr_type stat;
	score_set_type scores(segments.size());
	
	for (size_t seg = 0; seg != segments.size(); ++ seg)
	  if (! segments[seg].empty()) {
	    
	    if (! stat)
	      stat = segments[seg].front().score->zero();
	    
	    scores[seg] = segments[seg].front().score;
	    *stat += *segments[seg].front().score;
	    
	    if (segments[seg].size() > 1)
	      heap.push_back(item_type(seg, segments[seg].begin() + 1, segments[seg].end()));
	  }

	if (heap.empty())
	  return value_type();
	
	// priority queue...
	std::make_heap(heap.begin(), heap.end(), item_heap_compare_type());
	
	const double score = stat->loss();
	
	double optimum_lower = lower_bound(heap.front().first->x, range.first);
	double optimum_upper = heap.front().first->x;
	
	double optimum_objective = score;
	double optimum_score = score;
	
	double segment_prev = optimum_lower;
	double score_prev   = score;

	if (debug >= 4)
	  std::cerr << "lower: " << optimum_lower
		    << " upper: " << optimum_upper
		    << " score: " << score
		    << " objective: " << optimum_objective
		    << std::endl;

	
	while (! heap.empty()) {
	  // next at heap...
	  
	  const double segment_curr = heap.front().first->x;
	  
	  while (! heap.empty() && heap.front().first->x == segment_curr) { 
	    
	    std::pop_heap(heap.begin(), heap.end(), item_heap_compare_type());
	    
	    *stat -= *scores[heap.back().seg];
	    *stat += *heap.back().first->score;
	    scores[heap.back().seg] = heap.back().first->score;
	    
	    // pop and push heap...
	    ++ heap.back().first;
	    if (heap.back().first != heap.back().last)
	      std::push_heap(heap.begin(), heap.end(), item_heap_compare_type());
	    else
	      heap.pop_back();
	  }
	
	  const double segment_next = (heap.empty() ? upper_bound(segment_curr, range.second) : heap.front().first->x);
	  
	  // we perform merging of ranges if error counts are equal...
	  const double score = stat->loss();
	  if (score != score_prev) {
	    segment_prev = segment_curr;
	    score_prev = score;
	  }
	  
	  const double lower = segment_prev;
	  const double upper = segment_next;
	  const double point = (lower + upper) * 0.5;
	  
	  if (point > range.second) break;   // out of range for upper-bound, quit!
	  if (point < range.first) continue; // out of range for lower-bound...
	  if (std::fabs(point) < interval_min) continue; // interval is very small
	  
	  const double objective = score;
	  
	  if (debug >= 4)
	    std::cerr << "lower: " << lower
		      << " upper: " << upper
		      << " score: " << score
		      << " objective: " << objective
		      << std::endl;
	  
	  if (objective < optimum_objective) {
	    optimum_objective = objective;
	    optimum_score = score;
	    optimum_lower = lower;
	    optimum_upper = upper;
	  }
	}
	
	const double point = (optimum_lower + optimum_upper) * 0.5;
	if (point < range.first || range.second < point)
	  return value_type();
	else {
	  if (debug >= 2)
	    std::cerr << "minimum objective: " << optimum_objective
		      << " score: " << optimum_score
		      << " lower: " << optimum_lower
		      << " upper: " << optimum_upper << std::endl;
	  return value_type(optimum_objective, optimum_score, optimum_lower, optimum_upper);
	}
      }

      template <typename Regularizer>
      value_type operator()(const segment_document_type& segments,
			    Regularizer regularizer,
			    const double value_min,
			    const double value_max)
      {
	// we assume a set of line_ptr and score_ptr pair...
	
	const std::pair<double, double> range(value_min, value_max);

	heap_type heap;
	
	if (debug >= 4)
	  std::cerr << "minimum: " << range.first
		    << " maximum: " << range.second
		    << " segments: " << segments.size()
		    << std::endl;
	
	score_ptr_type stat;
	score_set_type scores(segments.size());
	
	for (size_t seg = 0; seg != segments.size(); ++ seg)
	  if (! segments[seg].empty()) {
	    
	    if (! stat)
	      stat = segments[seg].front().score->zero();
	    
	    scores[seg] = segments[seg].front().score;
	    *stat += *segments[seg].front().score;
	    
	    if (segments[seg].size() > 1)
	      heap.push_back(item_type(seg, segments[seg].begin() + 1, segments[seg].end()));
	  }

	if (heap.empty())
	  return value_type();
	
	// priority queue...
	std::make_heap(heap.begin(), heap.end(), item_heap_compare_type());
	
	const double score = stat->loss();
	
	double optimum_lower = lower_bound(heap.front().first->x, range.first);
	double optimum_upper = heap.front().first->x;
	
	double optimum_objective = score + regularizer((optimum_lower + optimum_upper) * 0.5);
	double optimum_score = score;
	
	double segment_prev = optimum_lower;
	double score_prev   = score;

	if (debug >= 4)
	  std::cerr << "lower: " << optimum_lower
		    << " upper: " << optimum_upper
		    << " score: " << score
		    << " objective: " << optimum_objective
		    << std::endl;
	
	while (! heap.empty()) {
	  // next at heap...
	  
	  const double segment_curr = heap.front().first->x;
	  
	  while (! heap.empty() && heap.front().first->x == segment_curr) { 
	    
	    std::pop_heap(heap.begin(), heap.end(), item_heap_compare_type());
	    
	    *stat -= *scores[heap.back().seg];
	    *stat += *heap.back().first->score;
	    scores[heap.back().seg] = heap.back().first->score;
	    
	    // pop and push heap...
	    ++ heap.back().first;
	    if (heap.back().first != heap.back().last)
	      std::push_heap(heap.begin(), heap.end(), item_heap_compare_type());
	    else
	      heap.pop_back();
	  }
	
	  const double segment_next = (heap.empty() ? upper_bound(segment_curr, range.second) : heap.front().first->x);
	  
	  // we perform merging of ranges if error counts are equal...
	  const double score = stat->loss();
	  if (score != score_prev) {
	    segment_prev = segment_curr;
	    score_prev = score;
	  }
	  
	  const double lower = segment_prev;
	  const double upper = segment_next;
	  const double point = (lower + upper) * 0.5;
	  
	  if (point > range.second) break;   // out of range for upper-bound, quit!
	  if (point < range.first) continue; // out of range for lower-bound...
	  if (std::fabs(point) < interval_min) continue; // interval is very small
	  
	  const double objective = score + regularizer(point);
	  
	  if (debug >= 4)
	    std::cerr << "lower: " << lower
		      << " upper: " << upper
		      << " score: " << score
		      << " objective: " << objective
		      << std::endl;
	  
	  if (objective < optimum_objective) {
	    optimum_objective = objective;
	    optimum_score = score;
	    optimum_lower = lower;
	    optimum_upper = upper;
	  }
	}
	
	const double point = (optimum_lower + optimum_upper) * 0.5;
	if (point < range.first || range.second < point)
	  return value_type();
	else {
	  if (debug >= 2)
	    std::cerr << "minimum objective: " << optimum_objective
		      << " score: " << optimum_score
		      << " lower: " << optimum_lower
		      << " upper: " << optimum_upper << std::endl;
	  return value_type(optimum_objective, optimum_score, optimum_lower, optimum_upper);
	}
      }

      template <typename Regularizer>
      value_type operator()(const segment_document_type& segments,
			    const weight_set_type& origin,
			    const weight_set_type& direction,
			    Regularizer regularizer)
      {
	// we assume a set of line_ptr and score_ptr pair...
	
	const std::pair<double, double> range = valid_range(origin, direction);

	heap_type heap;
	
	if (debug >= 4)
	  std::cerr << "minimum: " << range.first
		    << " maximum: " << range.second
		    << " segments: " << segments.size()
		    << std::endl;
	
	score_ptr_type stat;
	score_set_type scores(segments.size());
	
	for (size_t seg = 0; seg != segments.size(); ++ seg)
	  if (! segments[seg].empty()) {
	    
	    if (! stat)
	      stat = segments[seg].front().score->zero();
	    
	    scores[seg] = segments[seg].front().score;
	    *stat += *segments[seg].front().score;
	    
	    if (segments[seg].size() > 1)
	      heap.push_back(item_type(seg, segments[seg].begin() + 1, segments[seg].end()));
	  }

	if (heap.empty())
	  return value_type();
	
	// priority queue...
	std::make_heap(heap.begin(), heap.end(), item_heap_compare_type());
	
	const double score = stat->loss();
	
	double optimum_lower = lower_bound(heap.front().first->x, range.first);
	double optimum_upper = heap.front().first->x;
	
	double optimum_objective = score + regularizer(origin, direction, optimum_lower, optimum_upper);
	double optimum_score = score;
	
	double segment_prev = optimum_lower;
	double score_prev   = score;

	if (debug >= 4)
	  std::cerr << "lower: " << optimum_lower
		    << " upper: " << optimum_upper
		    << " score: " << score
		    << " objective: " << optimum_objective
		    << std::endl;

	
	while (! heap.empty()) {
	  // next at heap...
	  
	  const double segment_curr = heap.front().first->x;
	  
	  while (! heap.empty() && heap.front().first->x == segment_curr) { 
	    
	    std::pop_heap(heap.begin(), heap.end(), item_heap_compare_type());
	    
	    *stat -= *scores[heap.back().seg];
	    *stat += *heap.back().first->score;
	    scores[heap.back().seg] = heap.back().first->score;
	    
	    // pop and push heap...
	    ++ heap.back().first;
	    if (heap.back().first != heap.back().last)
	      std::push_heap(heap.begin(), heap.end(), item_heap_compare_type());
	    else
	      heap.pop_back();
	  }
	
	  const double segment_next = (heap.empty() ? upper_bound(segment_curr, range.second) : heap.front().first->x);
	  
	  // we perform merging of ranges if error counts are equal...
	  const double score = stat->loss();
	  if (score != score_prev) {
	    segment_prev = segment_curr;
	    score_prev = score;
	  }
	  
	  const double lower = segment_prev;
	  const double upper = segment_next;
	  const double point = (lower + upper) * 0.5;
	  
	  if (point > range.second) break;   // out of range for upper-bound, quit!
	  if (point < range.first) continue; // out of range for lower-bound...
	  if (std::fabs(point) < interval_min) continue; // interval is very small
	  
	  const double objective = score + regularizer(origin, direction, lower, upper);
	  
	  if (debug >= 4)
	    std::cerr << "lower: " << lower
		      << " upper: " << upper
		      << " score: " << score
		      << " objective: " << objective
		      << std::endl;
	  
	  if (objective < optimum_objective) {
	    optimum_objective = objective;
	    optimum_score = score;
	    optimum_lower = lower;
	    optimum_upper = upper;
	  }
	}
	
	const double point = (optimum_lower + optimum_upper) * 0.5;
	if (point < range.first || range.second < point)
	  return value_type();
	else {
	  if (debug >= 2)
	    std::cerr << "minimum objective: " << optimum_objective
		      << " score: " << optimum_score
		      << " lower: " << optimum_lower
		      << " upper: " << optimum_upper << std::endl;
	  return value_type(optimum_objective, optimum_score, optimum_lower, optimum_upper);
	}
      }

      static void initialize_bound(weight_set_type& bound_lower,
				   weight_set_type& bound_upper)
      {
	static const feature_type feature_none;
	
	bound_lower.allocate(value_min);
	bound_upper.allocate(value_max);
	
	bound_lower[feature_none] = 0.0;
	bound_upper[feature_none] = 0.0;
	
	// checking...
	for (feature_type::id_type id = 0; id < feature_type::allocated(); ++ id)
	  if (bound_upper[feature_type(id)] < bound_lower[feature_type(id)])
	    throw std::runtime_error("invalid lower-upper bound for feature: " + static_cast<const std::string&>(feature_type(id)));
      }

    private:
      
      std::pair<double, double> valid_range(const weight_set_type& origin,
					    const weight_set_type& direction) const
      {
	double minimum = - std::numeric_limits<double>::infinity();
	double maximum =   std::numeric_limits<double>::infinity();
	
	for (feature_type::id_type id = 0; id < feature_type::allocated(); ++ id)
	  if (! feature_type(id).empty()) {
	    const feature_type feature(id);
	    
	    const double ori = origin[feature];
	    const double dir = direction[feature];
	    const double low = bound_lower[feature];
	    const double upp = bound_upper[feature];
	    
	    if (low != upp) {
	      if (dir > 0.0) {
		maximum = std::min(maximum, (upp - ori) / dir);
		minimum = std::max(minimum, (low - ori) / dir);
	      } else if (dir < 0.0) {
		maximum = std::min(maximum, (low - ori) / dir);
		minimum = std::max(minimum, (upp - ori) / dir);
	      }
	    }
	  }
	
	return std::make_pair(minimum, maximum);
      }
      
      double lower_bound(const double& point, const double& bound)
      {
	return (point <= bound ? point - interval_offset_lower : std::max(bound, point - interval_offset_lower));
      }
      
      double upper_bound(const double& point, const double& bound)
      {
	return (point >= bound ? point + interval_offset_upper : std::min(bound, point + interval_offset_upper));
      }
      
      
    private:
      weight_set_type bound_lower;
      weight_set_type bound_upper;

      const int debug;
    };
  };
};

#endif
