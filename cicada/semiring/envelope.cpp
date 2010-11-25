//
//  Copyright(C) 2010 Taro Watanabe <taro.watanabe@nict.go.jp>
//

#include "cicada/semiring/envelope.hpp"

namespace cicada
{
  namespace semiring
  {
    
    const Envelope& Envelope::operator+=(const Envelope& x)
    {
      // Max operation... but we will simply perform addition, then hope for sort happen...
      
      if (! x.is_sorted) const_cast<Envelope&>(x).sort();

      if (lines.empty()) {
	lines = x.lines;
	is_sorted = true;
	return *this;
      }
      
      is_sorted = false;
      
      lines.insert(lines.end(), x.lines.begin(), x.lines.end());
      
      return *this;
    }
    
    const Envelope& Envelope::operator*=(const Envelope& x)
    {
      // Minkowski Sum operation...
      // we will add lines one-at-a-time
#if 0
      if (lines.size() == 1 && lines.front()->m == 0.0 && lines.front()->y == 0.0 && lines.front()->edge == 0) {
	*this = x;
	return *this;
      }
      if (x.lines.size() == 1 && x.lines.front()->m == 0.0 && x.lines.front()->y == 0.0 && x.lines.front()->edge == 0)
	return *this;

      if (x.lines.empty() || lines.empty()) {
	lines.clear();
	return *this;
      }
#endif
      
      if (! is_sorted)   const_cast<Envelope&>(*this).sort();
      if (! x.is_sorted) const_cast<Envelope&>(x).sort();
      
      // we have an object created by weight function...
      if (lines.size() == 1 && lines.front()->edge) {
	line_ptr_type line_edge_ptr(lines.front());
	const line_type& line_edge = *line_edge_ptr;
	
	lines.clear();
	line_ptr_set_type::const_iterator liter_end = x.lines.end();
	for (line_ptr_set_type::const_iterator liter = x.lines.begin(); liter != liter_end; ++ liter) {
	  const line_type& line = *(*liter);
	  
	  // no update to x...
	  const double& x = line_edge.x;
	  const double y  = line_edge.y + line.y;
	  const double m  = line_edge.m + line.m;
	  
	  lines.push_back(line_ptr_type(new line_type(x, m, y, line_edge_ptr, *liter)));
	}
	
      } else {
	static const double infinity = std::numeric_limits<double>::infinity();

	line_ptr_set_type L;
	
	line_ptr_set_type::const_iterator iter1 = lines.begin();
	line_ptr_set_type::const_iterator iter1_end = lines.end();

	line_ptr_set_type::const_iterator iter2 = x.lines.begin();
	line_ptr_set_type::const_iterator iter2_end = x.lines.end();

	double x_curr  = - infinity;
	double x_next1 = (iter1 + 1 < iter1_end ? (*(iter1 + 1))->x : infinity);
	double x_next2 = (iter2 + 1 < iter2_end ? (*(iter2 + 1))->x : infinity);
	
	while (iter1 != iter1_end && iter2 != iter2_end) {
	  const line_type& line1 = *(*iter1);
	  const line_type& line2 = *(*iter2);
	  
	  const double y = line1.y + line2.y;
	  const double m = line1.m + line2.m;
	  
	  L.push_back(line_ptr_type(new line_type(x_curr, m, y, *iter1, *iter2)));
	  
	  if (x_next1 < x_next2) {
	    ++ iter1;
	    x_curr  = x_next1;
	    x_next1 = (iter1 + 1 < iter1_end ? (*(iter1 + 1))->x : infinity);
	  } else if (x_next2 < x_next1) {
	    ++ iter2;
	    x_curr = x_next2;
	    x_next2 = (iter2 + 1 < iter2_end ? (*(iter2 + 1))->x : infinity);
	  } else {
	    ++ iter1;
	    ++ iter2;
	    
	    x_curr = x_next1;
	    
	    x_next1 = (iter1 + 1 < iter1_end ? (*(iter1 + 1))->x : infinity);
	    x_next2 = (iter2 + 1 < iter2_end ? (*(iter2 + 1))->x : infinity);
	  }
	}

	lines.swap(L);
      }
      
      return *this;
    }

    template <typename Line>
    struct compare_slope
    {
      bool operator()(const boost::shared_ptr<Line>& x, const boost::shared_ptr<Line>& y) const
      {
	return x->m < y->m;
      }
    };

    void Envelope::sort()
    {
      if (is_sorted) return;

      std::sort(lines.begin(), lines.end(), compare_slope<line_type>());
      
      int j = 0;
      int K = lines.size();
      
      for (int i = 0; i < K; ++ i) {
	line_type line = *lines[i];
	line.x = - std::numeric_limits<double>::infinity();
	
	if (0 < j) {
	  if (lines[j - 1]->m == line.m) { // parallel line...
	    if (line.y <= lines[j - 1]->y) continue;
	    -- j;
	  }
	  while (0 < j) {
	    line.x = (line.y - lines[j - 1]->y) / (lines[j - 1]->m - line.m);
	    if (lines[j - 1]->x < line.x) break;
	    -- j;
	  }
	  
	  if (0 == j)
	    line.x = - std::numeric_limits<double>::infinity();
	}
	
	*lines[j++] = line;
      }
      
      lines.resize(j);

      is_sorted = true;
    }

    void Envelope::Line::yield(sentence_type& sentence) const
    {
      typedef std::vector<sentence_type, std::allocator<sentence_type> > yield_set_type;
      typedef hypergraph_type::rule_type rule_type;

      yield_set_type yields;
      
      const Line* curr = this;
      while (! curr->edge) {
	yields.push_back(sentence_type());
	
	curr->antecedent->yield(yields.back());
	
	curr = curr->parent.get();
      }
      
      // we will traverse in reverse...
      sentence.clear();

      const rule_type::symbol_set_type& phrase = curr->edge->rule->rhs;
      
      int pos_non_terminal = 0;
      rule_type::symbol_set_type::const_iterator titer_end = phrase.end();
      for (rule_type::symbol_set_type::const_iterator titer = phrase.begin(); titer != titer_end; ++ titer)
	if (titer->is_non_terminal()) {
	  int pos = titer->non_terminal_index() - 1;
	  if (pos < 0)
	    pos = pos_non_terminal;
	  
	  const sentence_type& antecedent = *(yields.rbegin() + pos);
	  
	  sentence.insert(sentence.end(), antecedent.begin(), antecedent.end());
	  
	  ++ pos_non_terminal;
	} else if (*titer != cicada::Vocab::EPSILON)
	  sentence.push_back(*titer);
    }
  };
};
