#include "process_queries.h"

#include <algorithm>
#include <execution>

using namespace std;

std::vector<std::vector<Document>> ProcessQueries(
    const SearchServer& search_server,
    const std::vector<std::string>& queries) {

    std::vector<std::vector<Document>> foundDocumentsPerQueries(queries.size());

    transform(execution::par, queries.begin(), queries.end(), foundDocumentsPerQueries.begin(), [&](const auto& raw_query) { return search_server.FindTopDocuments(raw_query); });

    return foundDocumentsPerQueries;
}

vector<Document> ProcessQueriesJoined(
    const SearchServer& search_server,
    const vector<string>& queries) {

    std::vector<std::vector<Document>> foundDocumentsPerQueries = ProcessQueries(search_server, queries);

    vector<Document> joinedFoundDocuments;

    for (const auto& documents : foundDocumentsPerQueries) {
        for (const auto& it : documents) {
            joinedFoundDocuments.push_back(std::move(it));
        }
    }

    return joinedFoundDocuments;
}