#pragma once

template<typename Iterator>
class IteratorRange {
public:
    IteratorRange(Iterator begin, Iterator end)
            : first_(begin), last_(end), size_(distance(first_, last_)) {
    }

    Iterator begin() const {
        return first_;
    }

    Iterator end() const {
        return last_;
    }

    size_t size() const {
        return size_;
    }

private:
    Iterator first_;
    Iterator last_;
    size_t size_;
};

template<typename Iterator>
std::ostream &operator<<(std::ostream &out, const IteratorRange<Iterator> &range) {
    for (Iterator it = range.begin(); it != range.end(); ++it) {
        out << *it;
    }
    return out;
}

template<typename Iterator>
class Paginator {
public:
    Paginator(Iterator begin, Iterator end, size_t page_size) {
        for (size_t left = distance(begin, end); left > 0;) {
            const size_t search_page_size = std::min(page_size, left);
            const Iterator search_page_end = next(begin, search_page_size);
            page_.push_back({begin, search_page_end});

            left -= search_page_size;
            begin = search_page_end;
        }
    }

    auto begin() const {
        return page_.begin();
    }

    auto end() const {
        return page_.end();
    }

    size_t size() const {
        return page_.size();
    }

private:
    std::vector<IteratorRange<Iterator>> page_;
};

template<typename Container>
auto Paginate(const Container &c, size_t page_size) {
    return Paginator(begin(c), end(c), page_size);
}

