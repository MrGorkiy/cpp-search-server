#include "request_queue.h"

RequestQueue::RequestQueue(const SearchServer& search_server)
        : search_server_(search_server)
{
}

int RequestQueue::GetNoResultRequests() const {
    return count_if(requests_.begin(), requests_.end(), [](const auto n){return n.null_result == false;});
}

std::vector<Document> RequestQueue::AddFindRequest(const std::string &raw_query, DocumentStatus status) {
    std::vector<Document> doc = search_server_.FindTopDocuments(raw_query, status);
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

std::vector<Document> RequestQueue::AddFindRequest(const std::string &raw_query) {
    std::vector<Document> doc = search_server_.FindTopDocuments(raw_query);
    ++second_;
    while(min_in_day_ < second_) {
        requests_.pop_front();
        --second_;
    }
    if (doc.empty()) {
        requests_.push_back({false});
    } else {
        requests_.push_back({true});
    }
    return doc;
}