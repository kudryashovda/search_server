#include "search_server.h"
#include "test_example_functions.h"

using namespace std;

int main() {

    // TestSearchServer();

    const vector<string_view> stop_words = {};
    SearchServer search_server(stop_words);

    const std::vector<int> ratings = {};

    search_server.AddDocument(0, "first article"s, DocumentStatus::ACTUAL, ratings);
    search_server.AddDocument(1, "second article"s, DocumentStatus::ACTUAL, ratings);
    search_server.AddDocument(2, "third poem"s, DocumentStatus::ACTUAL, ratings);

    const string_view query = "article"s;

    for (const auto& it: search_server.FindTopDocuments(query)) {
        cout << search_server.GetDocumentById(it.id).text << endl;
    }

    return 0;
}
