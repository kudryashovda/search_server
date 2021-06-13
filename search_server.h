#pragma once

#include "concurrent_map.h"
#include "document.h"
#include "log_duration.h"
#include "paginator.h"
#include "string_processing.h"

#include <algorithm>
#include <cmath>
#include <execution>
#include <functional>
#include <future>
#include <list>
#include <map>
#include <set>
#include <stdexcept>
#include <unordered_set>
#include <vector>

using namespace std::string_literals;

constexpr int MAX_RESULT_DOCUMENT_COUNT = 5;

class SearchServer {
public:
    explicit SearchServer(std::string stop_words_text)
        : SearchServer(std::string_view(stop_words_text)) {
    }

    explicit SearchServer(std::string_view stop_words_text)
        : SearchServer(SplitIntoWords(stop_words_text)) {
    }

    template <typename StringContainer>
    explicit SearchServer(StringContainer stop_words)
        : stop_words_(MakeUniqueNonEmptyStrings(stop_words)) {
        if (!std::all_of(stop_words_.begin(), stop_words_.end(), IsValidWord)) {
            throw std::invalid_argument("Some of stop words are invalid"s);
        }
    }

    void AddDocument(int document_id, std::string_view document, DocumentStatus status,
                     const std::vector<int>& ratings);

    int GetDocumentCount() const;

    template <typename DocumentPredicate>
    std::vector<Document> FindTopDocuments(std::string_view raw_query, DocumentPredicate document_predicate) const {
        return FindTopDocuments(std::execution::seq, raw_query, document_predicate);
    }

    template <typename DocumentPredicate>
    std::vector<Document> FindTopDocuments(const std::execution::sequenced_policy&, std::string_view raw_query, DocumentPredicate document_predicate) const {
        const auto query = ParseQuery(raw_query);

        auto matched_documents = FindAllDocuments(std::execution::seq, query, document_predicate);

        std::sort(matched_documents.begin(), matched_documents.end(), [](const Document& lhs, const Document& rhs) {
            constexpr double EPSILON = 1e-6;

            if (std::abs(lhs.relevance - rhs.relevance) < EPSILON) {
                return lhs.rating > rhs.rating;
            } else {
                return lhs.relevance > rhs.relevance;
            }
        });

        if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
            matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
        }

        return matched_documents;
    }

    template <typename DocumentPredicate>
    std::vector<Document> FindTopDocuments(const std::execution::parallel_policy&, std::string_view raw_query, DocumentPredicate document_predicate) const {
        const auto query = ParseQuery(raw_query);

        auto matched_documents = FindAllDocuments(std::execution::par, query, document_predicate);

        std::sort(std::execution::par, matched_documents.begin(), matched_documents.end(), [](const Document& lhs, const Document& rhs) {
            constexpr double EPSILON = 1e-6;

            if (std::abs(lhs.relevance - rhs.relevance) < EPSILON) {
                return lhs.rating > rhs.rating;
            } else {
                return lhs.relevance > rhs.relevance;
            }
        });

        if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
            matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
        }

        return matched_documents;
    }

    std::vector<Document> FindTopDocuments(std::string_view raw_query, DocumentStatus status) const;
    std::vector<Document> FindTopDocuments(const std::execution::sequenced_policy&, std::string_view raw_query, DocumentStatus status) const;
    std::vector<Document> FindTopDocuments(const std::execution::parallel_policy&, std::string_view raw_query, DocumentStatus status) const;

    std::vector<Document> FindTopDocuments(std::string_view raw_query) const;
    std::vector<Document> FindTopDocuments(const std::execution::sequenced_policy&, std::string_view raw_query) const;
    std::vector<Document> FindTopDocuments(const std::execution::parallel_policy&, std::string_view raw_query) const;

    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(std::string_view raw_query,
                                                                            int document_id) const;
    template <typename ExecutionPolicy>
    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(ExecutionPolicy&& policy, std::string_view raw_query,
                                                                            int document_id) const {
        const auto query = ParseQuery(raw_query);

        const auto checker = [this, document_id](std::string_view word) {
            return word_to_document_freqs_.count(word) != 0 && word_to_document_freqs_.at(word).count(document_id);
        };

        std::vector<std::string_view> matched_words;
        matched_words.reserve(query.plus_words.size());

        for_each(policy, query.plus_words.begin(), query.plus_words.end(),
                 [&checker, &matched_words](std::string_view word) {
                     if (checker(word)) {
                         matched_words.push_back(std::move(word));
                     }
                 });

        const auto status = documents_.at(document_id).status;

        if (any_of(policy, query.minus_words.begin(), query.minus_words.end(), [&checker](std::string_view word) { return checker(word); })) {
            return { {}, status };
        }

        return { matched_words, status };
    }

    const std::list<int>::const_iterator begin() const;
    const std::list<int>::const_iterator end() const;

    const std::map<std::string_view, double, std::less<>>& GetWordFrequencies(int document_id) const;

    void RemoveDocument(int document_id);

    template <typename ExecutionPolicy>
    void RemoveDocument(ExecutionPolicy&& policy, int document_id) {
        const auto it_found = find(policy, document_ids_.begin(), document_ids_.end(), document_id);

        if (it_found == document_ids_.end()) {
            return;
        }

        document_ids_.erase(it_found);

        documents_.erase(document_id);

        for_each(policy,
                 word_to_document_freqs_.begin(), word_to_document_freqs_.end(),
                 [&document_id](auto& item) {
                     item.second.erase(document_id);
                 });

        // if exist clear all keys with empty map ids_freqs in word_to_document_freqs_
        std::vector<std::string> keysWithEmptyValues;

        for (const auto& [word, _] : word_to_document_freqs_) {
            if (word_to_document_freqs_.at(word).empty()) {
                keysWithEmptyValues.push_back(std::string(word));
            }
        }

        for (const auto& key : keysWithEmptyValues) {
            word_to_document_freqs_.erase(key);
        }

        documentId_to_word_freqs_.erase(document_id);
    }

    DocumentData GetDocumentById(int id) const;

private:
    static bool IsValidWord(std::string_view word);
    int ComputeAverageRating(const std::vector<int>& ratings);

    std::vector<std::string_view> SplitIntoWordsNoStop(std::string_view text) const;
    bool IsStopWord(std::string_view word) const;

    double ComputeWordInverseDocumentFreq(std::string_view word) const;

    struct Query {
        std::set<std::string_view> plus_words;
        std::set<std::string_view> minus_words;
    };

    struct QueryWord {
        std::string_view data;
        bool is_minus;
        bool is_stop;
    };

    QueryWord ParseQueryWord(std::string_view text) const;
    Query ParseQuery(std::string_view text) const;

    template <typename DocumentPredicate>
    std::vector<Document> FindAllDocuments(const Query& query, DocumentPredicate document_predicate) const {
        return FindAllDocuments(std::execution::seq, query, document_predicate);
    }

    template <typename DocumentPredicate>
    std::vector<Document> FindAllDocuments(const std::execution::sequenced_policy&, const Query& query, DocumentPredicate document_predicate) const {
        std::map<int, double> document_to_relevance;

        for (std::string_view word : query.plus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }

            const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);

            for (const auto& [document_id, term_freq] : word_to_document_freqs_.at(word)) {
                const auto& document_data = documents_.at(document_id);

                if (document_predicate(document_id, document_data.status, document_data.rating)) {
                    document_to_relevance[document_id] += term_freq * inverse_document_freq;
                }
            }
        }

        for (std::string_view word : query.minus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }

            for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
                document_to_relevance.erase(document_id);
            }
        }

        std::vector<Document> matched_documents;

        for (const auto [document_id, relevance] : document_to_relevance) {
            matched_documents.push_back({ document_id, relevance, documents_.at(document_id).rating });
        }

        return matched_documents;
    }

    template <typename DocumentPredicate>
    std::vector<Document> FindAllDocuments(const std::execution::parallel_policy&, const Query& query, DocumentPredicate document_predicate) const {
        constexpr size_t THREAD_COUNT = 97;
        ConcurrentMap<int, double> mt_document_to_relevance(THREAD_COUNT);

        for_each(std::execution::par, query.plus_words.begin(), query.plus_words.end(),
                 [this, &document_predicate, &mt_document_to_relevance](std::string_view word) {
                     if (word_to_document_freqs_.count(word) == 0) {
                         return;
                     }

                     const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);

                     for (const auto& [document_id, term_freq] : word_to_document_freqs_.at(word)) {

                         const auto& document_data = documents_.at(document_id);
                         if (document_predicate(document_id, document_data.status, document_data.rating)) {
                             mt_document_to_relevance[document_id].ref_to_value += term_freq * inverse_document_freq;
                         }
                     }
                 });

        std::map<int, double>
            document_to_relevance(mt_document_to_relevance.BuildOrdinaryMap());

        for (std::string_view word : query.minus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }

            for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
                document_to_relevance.erase(document_id);
            }
        }

        std::vector<Document> matched_documents;

        for (const auto [document_id, relevance] : document_to_relevance) {
            matched_documents.push_back({ document_id, relevance, documents_.at(document_id).rating });
        }

        return matched_documents;
    }

private:
    const std::set<std::string, std::less<>> stop_words_;

    std::unordered_set<std::string> all_words;

    std::map<std::string_view, std::map<int, double>> word_to_document_freqs_;

    std::map<int, std::map<std::string_view, double, std::less<>>> documentId_to_word_freqs_;

    std::map<int, DocumentData> documents_;

    std::list<int> document_ids_;
};

void PrintMatchDocumentResult(int document_id, const std::vector<std::string_view>& words, DocumentStatus status);

void AddDocument(SearchServer& search_server, int document_id, std::string_view document, DocumentStatus status,
                 const std::vector<int>& ratings);

void FindTopDocuments(const SearchServer& search_server, std::string_view raw_query);

void MatchDocuments(const SearchServer& search_server, std::string_view query);
