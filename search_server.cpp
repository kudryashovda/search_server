#include "search_server.h"

using namespace std;

bool SearchServer::IsValidWord(string_view word) {
    // A valid word must not contain special characters
    return none_of(word.begin(), word.end(), [](char c) { return c >= '\0' && c < ' '; });
}

void SearchServer::AddDocument(int document_id, string_view document, DocumentStatus status,
                               const vector<int>& ratings) {
    if ((document_id < 0) || (documents_.count(document_id) > 0)) {
        throw invalid_argument("Invalid document_id"s);
    }

    documents_.emplace(document_id, DocumentData{ ComputeAverageRating(ratings), status, string(document) });

    const auto words = SplitIntoWordsNoStop(document);

    const double inv_word_count = 1.0 / words.size();

    auto& word_freqs = documentId_to_word_freqs_[document_id];

    for (string_view word : words) {
        auto it = all_words.insert(string(word)); // make a word hard copy
        string_view sw = *it.first;

        word_to_document_freqs_[sw][document_id] += inv_word_count;
        word_freqs[sw] += inv_word_count;
    }

    document_ids_.push_back(document_id);
}

DocumentData SearchServer::GetDocumentById(int id) const {
    return documents_.at(id);

}

int SearchServer::ComputeAverageRating(const vector<int>& ratings) {
    if (ratings.empty()) {
        return 0;
    }

    int rating_sum = 0;

    for (const int rating : ratings) {
        rating_sum += rating;
    }

    return rating_sum / static_cast<int>(ratings.size());
}

vector<string_view> SearchServer::SplitIntoWordsNoStop(string_view text) const {
    vector<string_view> words;

    for (string_view word : SplitIntoWords(text)) {
        if (!IsValidWord(word)) {
            throw invalid_argument("Word "s + string(word) + " is invalid"s);
        }

        if (!IsStopWord(word)) {
            words.push_back(word);
        }
    }

    return words;
}

bool SearchServer::IsStopWord(string_view word) const {
    return stop_words_.count(word) > 0;
}

int SearchServer::GetDocumentCount() const {
    return document_ids_.size();
}

double SearchServer::ComputeWordInverseDocumentFreq(string_view word) const {
    return log(double(GetDocumentCount()) / word_to_document_freqs_.at(word).size());
}

SearchServer::QueryWord SearchServer::ParseQueryWord(string_view text) const {
    if (text.empty()) {
        throw invalid_argument("Query word is empty"s);
    }

    bool is_minus = false;

    if (text[0] == '-') {
        is_minus = true;

        // remove '-' char from minus word
        text.remove_prefix(1);
    }

    if (text.empty() || text[0] == '-' || !IsValidWord(text)) {
        throw invalid_argument("Query word "s + string(text) + " is invalid");
    }

    return { text, is_minus, IsStopWord(text) };
}

SearchServer::Query SearchServer::ParseQuery(string_view text) const {
    SearchServer::Query result;

    for (string_view word : SplitIntoWords(text)) {
        const auto query_word = ParseQueryWord(word);

        if (!query_word.is_stop) {
            if (query_word.is_minus) {
                result.minus_words.insert(query_word.data);
            } else {
                result.plus_words.insert(query_word.data);
            }
        }
    }

    return result;
}

vector<Document> SearchServer::FindTopDocuments(string_view raw_query, DocumentStatus status) const {
    return FindTopDocuments(execution::seq, raw_query, status);
}

vector<Document> SearchServer::FindTopDocuments(const std::execution::sequenced_policy&, string_view raw_query, DocumentStatus status) const {
    return FindTopDocuments(execution::seq, raw_query, [status](int document_id, DocumentStatus document_status, int rating) {
        return document_status == status;
    });
}

vector<Document> SearchServer::FindTopDocuments(const std::execution::parallel_policy&, string_view raw_query, DocumentStatus status) const {
    return FindTopDocuments(execution::par, raw_query, [status](int document_id, DocumentStatus document_status, int rating) {
        return document_status == status;
    });
}

vector<Document> SearchServer::FindTopDocuments(string_view raw_query) const {
    return FindTopDocuments(execution::seq, raw_query);
}

vector<Document> SearchServer::FindTopDocuments(const std::execution::sequenced_policy&, string_view raw_query) const {
    return FindTopDocuments(execution::seq, raw_query, DocumentStatus::ACTUAL);
}

vector<Document> SearchServer::FindTopDocuments(const std::execution::parallel_policy&, string_view raw_query) const {
    return FindTopDocuments(execution::par, raw_query, DocumentStatus::ACTUAL);
}

tuple<vector<string_view>, DocumentStatus> SearchServer::MatchDocument(string_view raw_query, int document_id) const {
    return MatchDocument(execution::seq, raw_query, document_id);
}

const list<int>::const_iterator SearchServer::begin() const {
    return document_ids_.begin();
}

const list<int>::const_iterator SearchServer::end() const {
    return document_ids_.end();
}

const map<string_view, double, less<>>& SearchServer::GetWordFrequencies(int document_id) const {
    const static map<string_view, double, std::less<>> empty_map;

    if (documentId_to_word_freqs_.count(document_id) == 0) {
        return empty_map;
    }

    return documentId_to_word_freqs_.at(document_id);
}

void SearchServer::RemoveDocument(int document_id) {
    RemoveDocument(execution::seq, document_id);
}

void PrintMatchDocumentResult(int document_id, const vector<string_view>& words, DocumentStatus status) {
    cout << "{ "s
         << "document_id = "s << document_id << ", "s
         << "status = "s << static_cast<int>(status) << ", "s
         << "words ="s;

    for (string_view word : words) {
        cout << ' ' << word;
    }

    cout << "}"s << endl;
}

void AddDocument(SearchServer& search_server, int document_id, string_view document, DocumentStatus status,
                 const vector<int>& ratings) {
    try {
        search_server.AddDocument(document_id, document, status, ratings);
    } catch (const invalid_argument& e) {
        cout << "Add document error"s << document_id << ": "s << e.what() << endl;
    }
}

void FindTopDocuments(const SearchServer& search_server, string_view raw_query) {
    LOG_DURATION_STREAM("Long task", cout);

    cout << "Search results for the query: "s << string(raw_query) << endl;

    try {
        const auto search_results = search_server.FindTopDocuments(raw_query);
        int page_size = 2;
        const auto pages = Paginate(search_results, page_size);

        // Выводим найденные документы по страницам
        for (auto page = pages.begin(); page != pages.end(); ++page) {
            cout << *page << endl;
            cout << "Page break"s << endl;
        }

    } catch (const invalid_argument& e) {
        cout << "Search error: "s << e.what() << endl;
    }
}

void MatchDocuments(const SearchServer& search_server, string_view query) {
    LOG_DURATION_STREAM("Long task", cout);

    try {
        cout << "Matched documents for the query: "s << string(query) << endl;

        for (const auto& document_id : search_server) {

            const auto [words, status] = search_server.MatchDocument(query, document_id);

            PrintMatchDocumentResult(document_id, words, status);
        }

    } catch (const invalid_argument& e) {
        cout << "Matching error for the query "s << string(query) << ": "s << e.what() << endl;
    }
}