#ifndef __CONTAINERS_ITERATOR_HPP__
#define __CONTAINERS_ITERATOR_HPP__

#include <algorithm>
#include <functional>
#include <queue>
#include <boost/optional.hpp>

template <typename T>
struct one_way_iterator_t {
    virtual ~one_way_iterator_t() { }
    virtual typename boost::optional<T> next() = 0; // next() can block until it can read the next value
    virtual void prefetch() = 0;    // Fetch all the necessary data to be able to give the next value without blocking.
                                    // prefetch() is assumed to be asynchronous. Thus if next() is called before the data
                                    // is available, next() can still block.
};

template <typename F, typename S, typename Cmp = std::less<F> >
struct first_not_less {
    typedef F first_argument_type;
    typedef S second_argument_type;
    bool operator()(std::pair<F,S> l, std::pair<F,S> r) {
        return !Cmp()(l.first, r.first);
    }
};

template <typename T, typename Cmp = std::less<T> >
class merge_ordered_data_iterator_t : public one_way_iterator_t<T> {
public:
    typedef one_way_iterator_t<T> mergee_t;
    typedef std::vector<mergee_t*> mergees_t;

    typedef std::pair<T, mergee_t*> heap_elem_t;
    typedef std::vector<heap_elem_t> heap_container_t;


    merge_ordered_data_iterator_t(const mergees_t& mergees) : mergees(mergees), next_to_pop_from(NULL),
        merge_heap(first_not_less<T, mergee_t*, Cmp>(), heap_container_t()) { }

    typename boost::optional<T> next() {
        // if we are getting the first element, we have to request the data from all of the mergees
        if (next_to_pop_from == NULL) {
            prefetch();
            for (typename mergees_t::iterator it = mergees.begin(); it != mergees.end(); ++it) {
                mergee_t *mergee = *it;
                typename boost::optional<T> mergee_next = mergee->next();
                if (mergee_next)
                    merge_heap.push(std::make_pair(mergee_next.get(), mergee));
            }
        } else {
            typename boost::optional<T> next_val = next_to_pop_from->next();
            if (next_val)
                merge_heap.push(std::make_pair(next_val.get(), next_to_pop_from));
        }
        if (merge_heap.size() == 0) {
            return boost::none;
        }

        heap_elem_t top = merge_heap.top();
        merge_heap.pop();
        next_to_pop_from = top.second;

        // issue the async prefetch, so that we don't need to block on next has_next/next call
        next_to_pop_from->prefetch();
        return boost::make_optional(top.first);
    }
    void prefetch() {
        std::for_each(mergees.begin(), mergees.end(), std::mem_fun(&mergee_t::prefetch));
    }
private:
    mergees_t mergees;
    mergee_t *next_to_pop_from;
    typename std::priority_queue<heap_elem_t, heap_container_t, first_not_less<T, mergee_t*, Cmp> > merge_heap;

};

#endif /* __CONTAINERS_ITERATOR_HPP__ */