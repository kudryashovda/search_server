#pragma once

#include "search_server.h"

#include <deque>
#include <string>
#include <vector>

class RequestQueue {
public:
    explicit RequestQueue(const SearchServer& search_server);

    template <typename DocumentPredicate>
    std::vector<Document> AddFindRequest(std::string_view raw_query, DocumentPredicate document_predicate) {
        auto results = search_server_.FindTopDocuments(raw_query, document_predicate);

        ++current_time_;

        if (current_time_ > sec_in_day_) {
            // To avoid current_time_ var overflow if numeric_limits<int>::max() reaches
            current_time_ = sec_in_day_;

            if (requests_.front().documents.empty()) {
                --empty_results_requests_;
            }

            requests_.pop_front();
        }

        requests_.push_back({ raw_query, results });

        if (results.empty()) {
            ++empty_results_requests_;
        }

        return results;
    }

    std::vector<Document> AddFindRequest(std::string_view raw_query, DocumentStatus status);

    std::vector<Document> AddFindRequest(std::string_view raw_query);

    int GetNoResultRequests() const;

private:
    const SearchServer& search_server_;

    struct QueryResult {
        std::string_view request_string;
        std::vector<Document> documents;
    };

    std::deque<QueryResult> requests_;
    constexpr static int sec_in_day_ = 1440;
    int current_time_ = 0;
    int empty_results_requests_ = 0;
};
