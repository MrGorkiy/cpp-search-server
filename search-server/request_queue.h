#pragma once

#include <string>
#include <vector>
#include <deque>

#include "search_server.h"

class RequestQueue {
public:
    explicit RequestQueue(const SearchServer &search_server);

    template<typename DocumentPredicate>
    std::vector<Document> AddFindRequest(const std::string &raw_query, DocumentPredicate document_predicate);

    std::vector<Document> AddFindRequest(const std::string &raw_query, DocumentStatus status);

    std::vector<Document> AddFindRequest(const std::string &raw_query);

    int GetNoResultRequests() const;

private:
    struct QueryResult {
        bool null_result;
    };
    std::deque<QueryResult> requests_;
    const static int min_in_day_ = 1440;
    int second_ = 0;
    const SearchServer &search_server_;
};

template<typename DocumentPredicate>
std::vector<Document> RequestQueue::AddFindRequest(const std::string &raw_query, DocumentPredicate document_predicate) {
    std::vector<Document> doc = search_server_.FindTopDocuments(raw_query, document_predicate);
    ++second_;
    while(min_in_day_ < second_) {
        requests_.pop_front();
        --second_;
    }
    if (doc.empty()) {
        requests_.push_back({true});
    } else {
        requests_.push_back({false});
    }
    return doc;
}