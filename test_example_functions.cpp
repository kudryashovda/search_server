#include "test_example_functions.h"

using namespace std;

void AssertImpl(bool value, const string& expr_str, const string& file, const string& func, unsigned line,
                const string& hint) {
    if (!value) {
        cerr << file << "("s << line << "): "s << func << ": "s;
        cerr << "ASSERT("s << expr_str << ") failed."s;

        if (!hint.empty()) {
            cerr << " Hint: "s << hint;
        }

        cerr << endl;
        abort();
    }
}

// -------- Начало модульных тестов поисковой системы ----------
void TestExcludeStopWordsFromAddedDocumentContent() {
    constexpr int doc_id = 42;
    string_view content = "cat in the city";
    const std::vector<int> ratings = { 1, 2, 3 };

    {
        SearchServer server(""s);

        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);

        const auto found_docs = server.FindTopDocuments("in"s);
        ASSERT_EQUAL(found_docs.size(), 1u);

        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, doc_id);
    }

    {
        SearchServer server("in the"s);

        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);

        ASSERT_HINT(server.FindTopDocuments("in"s).empty(), "Stop words must be excluded from documents"s);
    }
}

void TestAddDocument() {
    string_view content = "cat in the city";
    const vector<int> ratings = { 1, 2, 3, 6 };

    SearchServer server(""s);

    server.AddDocument(0, content, DocumentStatus::ACTUAL, ratings);

    const auto found_docs0 = server.FindTopDocuments("city"s);
    ASSERT_EQUAL_HINT(found_docs0.size(), 1u, "One Document must show after one AddDocument call"s);

    server.AddDocument(1, content, DocumentStatus::ACTUAL, ratings);

    const auto found_docs1 = server.FindTopDocuments("city"s);
    ASSERT_EQUAL(found_docs1.size(), 2u);
}

void TestStopWordsSupport() {

    string_view content = "eagle in the shadow";
    const vector<int> ratings = { 1, 2, 3, 4 };

    {
        constexpr int doc_id = 42;
        SearchServer server(""s);

        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);

        const auto found_docs0 = server.FindTopDocuments("shadow"s);
        ASSERT_EQUAL_HINT(found_docs0.size(), 1u, "One document on relevant query"s);
    }

    {
        SearchServer server("shadow"s);

        const auto found_docs1 = server.FindTopDocuments("shadow"s);
        ASSERT_HINT(found_docs1.empty(), "shadow is a stop word - no documents must be found"s);
    }
}

void TestMinusWords() {
    const vector<int> ratings = { 1, 2, 3, 6 };

    SearchServer server("in the"s);

    server.AddDocument(0, "black cat in the city"s, DocumentStatus::ACTUAL, ratings);
    server.AddDocument(1, "black cat in the village"s, DocumentStatus::ACTUAL, ratings);

    const auto found_docs0 = server.FindTopDocuments("black cat"s);
    ASSERT_EQUAL_HINT(found_docs0.size(), 2u, "Two documents relevant to the query"s);

    const auto found_docs1 = server.FindTopDocuments("black cat in the -city"s);
    ASSERT_EQUAL_HINT(found_docs1.size(), 1u, "Documents with minus word must be excluded"s);
}

void TestMatching() {
    const vector<int> ratings = { 1, 2, 3, 6 };

    SearchServer server(""s);

    server.AddDocument(10, "white cat and model colle"s, DocumentStatus::ACTUAL, ratings);
    server.AddDocument(11, "good dog exiting ears"s, DocumentStatus::IRRELEVANT, ratings);
    server.AddDocument(12, "cared dog deep eyes"s, DocumentStatus::REMOVED, ratings);
    server.AddDocument(13, "cared bird grisha"s, DocumentStatus::BANNED, ratings);

    {
        string_view raw_query = "cared cat model colle";

        const auto matched_words_status = server.MatchDocument(raw_query, 10);
        ASSERT_EQUAL_HINT(get<0>(matched_words_status).size(), 3u, "Simple document matching"s);

        for (const auto& word : get<0>(matched_words_status)) {
            ASSERT_HINT(word == "cat"s || word == "colle"s || word == "model"s, "All matched words are exist"s);
        }
    }

    {
        string_view raw_query = "cared cat model -colle";

        const auto matched_words_status = server.MatchDocument(raw_query, 10);
        ASSERT_HINT(get<0>(matched_words_status).empty(), "Minus word was found in matched document"s);
    }

    {
        string_view raw_query = "-cared cat model colle";

        const auto matched_words_status = server.MatchDocument(raw_query, 10);
        ASSERT_EQUAL_HINT(get<0>(matched_words_status).size(), 3u, "Minus word was not found in matched document"s);
    }

    {
        const auto matched_words_status = server.MatchDocument("cat model"s, 10);
        ASSERT(get<DocumentStatus>(matched_words_status) == DocumentStatus::ACTUAL);
        ASSERT(get<DocumentStatus>(matched_words_status) != DocumentStatus::IRRELEVANT);
        ASSERT(get<DocumentStatus>(matched_words_status) != DocumentStatus::REMOVED);
        ASSERT(get<DocumentStatus>(matched_words_status) != DocumentStatus::BANNED);

        const auto matched_words_status2 = server.MatchDocument("good dog"s, 11);
        ASSERT(get<DocumentStatus>(matched_words_status2) != DocumentStatus::ACTUAL);
        ASSERT(get<DocumentStatus>(matched_words_status2) == DocumentStatus::IRRELEVANT);
        ASSERT(get<DocumentStatus>(matched_words_status2) != DocumentStatus::REMOVED);
        ASSERT(get<DocumentStatus>(matched_words_status2) != DocumentStatus::BANNED);

        const auto matched_words_status3 = server.MatchDocument("deep eyes"s, 12);
        ASSERT(get<DocumentStatus>(matched_words_status3) != DocumentStatus::IRRELEVANT);
        ASSERT(get<DocumentStatus>(matched_words_status3) != DocumentStatus::ACTUAL);
        ASSERT(get<DocumentStatus>(matched_words_status3) == DocumentStatus::REMOVED);
        ASSERT(get<DocumentStatus>(matched_words_status3) != DocumentStatus::BANNED);

        const auto matched_words_status4 = server.MatchDocument("grisha bird"s, 13);
        ASSERT(get<DocumentStatus>(matched_words_status4) != DocumentStatus::IRRELEVANT);
        ASSERT(get<DocumentStatus>(matched_words_status4) != DocumentStatus::ACTUAL);
        ASSERT(get<DocumentStatus>(matched_words_status4) != DocumentStatus::REMOVED);
        ASSERT(get<DocumentStatus>(matched_words_status4) == DocumentStatus::BANNED);
    }
}

void TestSortRelevance() {
    const vector<int> ratings = { 1, 2, 3, 6 };

    SearchServer server(""s);

    server.AddDocument(0, "white cat big face"s, DocumentStatus::ACTUAL, ratings);
    server.AddDocument(1, "yellow parrot round cock"s, DocumentStatus::ACTUAL, ratings);
    server.AddDocument(2, "brown cat small nose"s, DocumentStatus::ACTUAL, ratings);
    server.AddDocument(3, "snow white mice"s, DocumentStatus::ACTUAL, ratings);

    const auto found_docs = server.FindTopDocuments("brown cat big cock"s);
    ASSERT_EQUAL_HINT(found_docs.size(), 3u, "Found 3 relevant documents"s);

    vector<int> found_doc_ids;

    for (const auto& it : found_docs) {
        found_doc_ids.push_back(it.id);
    }

    const vector<int> real_found_doc_ids = { 0, 2, 1 };

    ASSERT_EQUAL_HINT(found_doc_ids, real_found_doc_ids, "Found documents ids must be equal to this"s);
}

void TestRatingCalc() {
    SearchServer server(""s);

    const vector<int> ratings0 = { 0, 0, 0 };
    const vector<int> ratings1 = { -1, -2, -3 };
    const vector<int> ratings2 = { 1, 2, 3 };
    const vector<int> ratings3 = { 2, 2, 3 };

    server.AddDocument(0, "yellow"s, DocumentStatus::ACTUAL, ratings0);
    server.AddDocument(1, "parrot"s, DocumentStatus::ACTUAL, ratings1);
    server.AddDocument(2, "round"s, DocumentStatus::ACTUAL, ratings2);
    server.AddDocument(3, "cock"s, DocumentStatus::ACTUAL, ratings3);

    const auto found_docs0 = server.FindTopDocuments("yellow"s);
    const int rating0 = found_docs0[0].rating;
    ASSERT_EQUAL_HINT(rating0, 0, "All zero ratings"s);

    const auto found_docs1 = server.FindTopDocuments("parrot"s);
    const int rating1 = found_docs1[0].rating;
    ASSERT_EQUAL_HINT(rating1, -2, "All negatives ratings"s);

    const auto found_docs2 = server.FindTopDocuments("round"s);
    const int rating2 = found_docs2[0].rating;
    ASSERT_EQUAL_HINT(rating2, 2, "Even ratings summ"s);

    const auto found_docs3 = server.FindTopDocuments("cock"s);
    const int rating3 = found_docs3[0].rating;
    ASSERT_EQUAL_HINT(rating3, 2, "Odd ratings summ"s);
}

void TestFilterPredicat() {
    SearchServer server(""s);

    server.AddDocument(0, "white cat big face"s, DocumentStatus::ACTUAL, { 8, -3 });
    server.AddDocument(1, "yellow cat round cock"s, DocumentStatus::ACTUAL, { 7, 1, 7 });
    server.AddDocument(2, "brown cat small nose"s, DocumentStatus::ACTUAL, { 5, -8, 2, 1 });
    server.AddDocument(3, "blue cat small nose"s, DocumentStatus::ACTUAL, { 5, -10, 2, 1 });

    const auto found_docs = server.FindTopDocuments(
        "cat"s, [](int document_id, DocumentStatus status, int rating) { return document_id % 2 != 0; });

    const Document& doc0 = found_docs[0];
    const Document& doc1 = found_docs[1];

    ASSERT_EQUAL(doc0.id, 1);
    ASSERT_EQUAL(doc1.id, 3);
}

void TestSearchWitnStatus() {
    SearchServer server(""s);

    server.AddDocument(0, "white cat big face"s, DocumentStatus::ACTUAL, { 8, -3 });
    server.AddDocument(1, "yellow cat round cock"s, DocumentStatus::REMOVED, { 7, 1, 7 });
    server.AddDocument(2, "brown cat small nose"s, DocumentStatus::IRRELEVANT, { 5, -8, 2, 1 });
    server.AddDocument(3, "brown dog small nose"s, DocumentStatus::ACTUAL, { 5, -8, 2, 1 });
    server.AddDocument(4, "blue cat small nose"s, DocumentStatus::ACTUAL, { 5, -10, 2, 1 });

    {
        const auto found_docs = server.FindTopDocuments("brown"s, DocumentStatus::IRRELEVANT);
        ASSERT_EQUAL(found_docs.size(), 1u);

        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, 2);
    }

    {
        const auto found_docs = server.FindTopDocuments("cat"s, DocumentStatus::REMOVED);
        ASSERT_EQUAL(found_docs.size(), 1u);

        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, 1);
    }
}

void TestRelevanceCalc() {
    SearchServer server("in the"s);

    vector<int> ratings = { 1, 2, 3 };

    server.AddDocument(0, "white cat and model colle"s, DocumentStatus::ACTUAL, ratings);
    server.AddDocument(1, "fur cat fur cock"s, DocumentStatus::ACTUAL, ratings);
    server.AddDocument(2, "cared dog exiting eyes"s, DocumentStatus::ACTUAL, ratings);

    const auto found_docs = server.FindTopDocuments("fur cared cat"s);

    const Document& doc0 = found_docs[0];
    const Document& doc1 = found_docs[1];
    const Document& doc2 = found_docs[2];

    constexpr double EPSILON = 1e-6;
    constexpr double doc0_real_relevance = 0.650672;
    constexpr double doc1_real_relevance = 0.274653;
    constexpr double doc2_real_relevance = 0.081093;

    const double error0 = abs(doc0.relevance - doc0_real_relevance);
    const double error1 = abs(doc1.relevance - doc1_real_relevance);
    const double error2 = abs(doc2.relevance - doc2_real_relevance);

    ASSERT_HINT(error0 < EPSILON, "Relevance calculation of first document"s);
    ASSERT_HINT(error1 < EPSILON, "Relevance calculation of second document"s);
    ASSERT_HINT(error2 < EPSILON, "Relevance calculation of third document"s);
}

// Sptint 3 Exeptions
void TestExeptionConstructorInvalidChar() {
    try {
        string_view bad_stop_word = "ca" + char(30);

        SearchServer server(bad_stop_word);
    } catch (const exception& e) {
        ASSERT_EQUAL(e.what(), "Some of stop words are invalid"s);
    }
}

void TestExeptionAddDocumentInvalidChar() {
    string invalid_word = "rab"s + char(30) + "it"s;
    string string_with_invalid_word = "good white "s + invalid_word;

    try {
        string_view string_view_with_invalid_word(string_with_invalid_word);

        SearchServer server(""s);

        server.AddDocument(1, string_view_with_invalid_word, DocumentStatus::ACTUAL, { 1, 2, 3 });
    } catch (const exception& e) {
        ASSERT_EQUAL(e.what(), "Word "s + invalid_word + " is invalid"s);
    }
}

void TestExeptionAddDocumentInvalidDocumentID() {
    SearchServer server(""s);

    server.AddDocument(0, "black dog"s, DocumentStatus::ACTUAL, { 1, 2, 3 });

    try {
        server.AddDocument(-1, "black cat"s, DocumentStatus::ACTUAL, { 1, 2, 3 });
    } catch (const exception& e) {
        ASSERT_EQUAL(e.what(), "Invalid document_id"s);
    }
}

void TestExeptionAddDocumentSameDocumentID() {
    SearchServer server(""s);

    server.AddDocument(0, "black dog"s, DocumentStatus::ACTUAL, { 1, 2, 3 });

    try {
        server.AddDocument(0, "black cat"s, DocumentStatus::ACTUAL, { 1, 2, 3 });
    } catch (const exception& e) {
        ASSERT_EQUAL(e.what(), "Invalid document_id"s);
    }
}

void TestExeptionFindTopDocumentsIllegalWordQuery() {
    string invalid_word = "rab"s + char(30) + "it"s;

    try {
        string string_with_invalid_word = "good white "s + invalid_word;
        string_view string_view_with_invalid_word(string_with_invalid_word);

        SearchServer server(""s);

        server.FindTopDocuments(string_view_with_invalid_word);
    } catch (const exception& e) {
        ASSERT_EQUAL(e.what(), "Query word "s + invalid_word + " is invalid"s);
    }
}

void TestExeptionFindTopDocumentsAnotherMinus() {
    try {
        SearchServer server(""s);
        server.AddDocument(0, "black cat"s, DocumentStatus::ACTUAL, { 1, 2, 3 });

        server.FindTopDocuments("black --cat"s);
    } catch (const exception& e) {
        ASSERT_EQUAL(e.what(), "Query word -cat is invalid"s);
    }
}

void TestExeptionMatchDocumentWrongID() {
    SearchServer server(""s);

    server.AddDocument(0, "black dog"s, DocumentStatus::ACTUAL, { 1, 2, 3 });

    try {
        const auto& [words, status] = server.MatchDocument("Black big dog"s, 3);

    } catch (const exception& e) {
        ASSERT_EQUAL(e.what(), "map::at"s);
    }
}

void TestExeptionMatchDocumentIllegalWordQuery() {
    string invalid_word = "rab"s + char(30) + "it"s;

    SearchServer server(""s);

    server.AddDocument(0, "black dog"s, DocumentStatus::ACTUAL, { 1, 2, 3 });

    try {
        string string_with_invalid_word = "good white "s + invalid_word;
        string_view string_view_with_invalid_word(string_with_invalid_word);

        const auto& [words, status] = server.MatchDocument(string_view_with_invalid_word, 3);

    } catch (const exception& e) {
        ASSERT_EQUAL(e.what(), "Query word "s + invalid_word + " is invalid"s);
    }
}

void TestExeptionMatchDocumentAnotherMinus() {
    SearchServer server(""s);

    server.AddDocument(0, "black dog"s, DocumentStatus::ACTUAL, { 1, 2, 3 });

    try {
        const auto [words, status] = server.MatchDocument("black --dog"s, 0);
    } catch (const exception& e) {
        ASSERT_EQUAL(e.what(), "Query word -dog is invalid"s);
    }
}

void TestPaginator() {
    SearchServer server("in the"s);

    server.AddDocument(0, "white cat and model colle"s, DocumentStatus::ACTUAL, { 1, 2, 3 });
    server.AddDocument(1, "fur cat fur cock"s, DocumentStatus::ACTUAL, { 1, 2, 3 });
    server.AddDocument(2, "cared dog exiting eyes"s, DocumentStatus::ACTUAL, { 1, 2, 3 });
    server.AddDocument(3, "carfur cat afur cock"s, DocumentStatus::ACTUAL, { 1, 2, 3 });
    server.AddDocument(4, "scared dog exiting eyes"s, DocumentStatus::ACTUAL, { 1, 2, 3 });

    const auto search_results = server.FindTopDocuments("scared fur cared cat afur"s); // 5 max

    int page_size = 2;

    const auto pages = Paginate(search_results, page_size);

    int pages_count = distance(pages.begin(), pages.end());

    ASSERT_EQUAL(pages_count, 3);
}

void TestRequestQueue() {
    SearchServer search_server("and on at"s);
    RequestQueue request_queue(search_server);

    search_server.AddDocument(1, "fluffy cat fluffy tail"s, DocumentStatus::ACTUAL, { 7, 2, 7 });
    search_server.AddDocument(2, "fluffy dog and fancy collar"s, DocumentStatus::ACTUAL, { 1, 2, 3 });
    search_server.AddDocument(3, "big cat fancy collar "s, DocumentStatus::ACTUAL, { 1, 2, 8 });
    search_server.AddDocument(4, "big dog starling Eugine"s, DocumentStatus::ACTUAL, { 1, 3, 2 });
    search_server.AddDocument(5, "big dog starling Vasya"s, DocumentStatus::ACTUAL, { 1, 1, 1 });

    // 1439 empty requests
    constexpr int null_requests = 1439;

    for (int i = 0; i < null_requests; ++i) {
        request_queue.AddFindRequest("empty request"s);
    }

    // still 1439 empty requests
    request_queue.AddFindRequest("fluffy dog"s);

    // new day, first query was deleted, 1438 empty requests
    request_queue.AddFindRequest("big collar"s);

    // first query was deleted, 1437 empty requests
    request_queue.AddFindRequest("starling"s);

    ASSERT_EQUAL(request_queue.GetNoResultRequests(), 1437);
}

// test from Oleg Tsoi
bool InTheVicinity(const double d1, const double d2, const double delta) {
    return abs(d1 - d2) < delta;
}

void TestRemoveDocument() {
    SearchServer server("and with as"s);

    AddDocument(server, 2, "funny pet with curly hair"s, DocumentStatus::ACTUAL, { 1, 2 });
    AddDocument(server, 4, "kind dog bite fat rat"s, DocumentStatus::ACTUAL, { 1, 2 });
    AddDocument(server, 6, "fluffy snake or cat"s, DocumentStatus::ACTUAL, { 1, 2 });

    AddDocument(server, 1, "funny pet and nasty rat"s, DocumentStatus::ACTUAL, { 7, 2, 7 });
    // nasty tf = 1/4
    AddDocument(server, 3, "angry rat with black hat"s, DocumentStatus::ACTUAL, { 1, 2 });
    // black tf = 1/4
    AddDocument(server, 5, "fat fat cat"s, DocumentStatus::ACTUAL, { 1, 2 });
    // cat tf = 1/3
    AddDocument(server, 7, "sharp as hedgehog"s, DocumentStatus::ACTUAL, { 1, 2 });
    // sharp tf = 1/2

    // kind - doesn't occur
    // nasty - log(4)
    // black - log(4)
    // cat - log(4)
    // sharp - log(4)

    // 7 - 1/2 * log(4) = 0.6931471805599453
    // 5 - 1/3 * log(4) = 0.46209812037329684
    // 1 - 1/4 * log(4) = 0.34657359027997264
    // 3 - 1/4 * log(4) = 0.34657359027997264

    ASSERT_EQUAL_HINT(server.GetDocumentCount(), 7, "Nothing has been removed, yet!"s);
    server.RemoveDocument(0);
    server.RemoveDocument(8);

    ASSERT_EQUAL_HINT(server.GetDocumentCount(), 7, "Nothing has been removed, yet!"s);
    {
        const auto docs = server.FindTopDocuments("kind nasty black sharp cat"s);
        for (const auto doc : docs) {
        	cerr << doc.id << " ";
        } cerr << endl;
        ASSERT_EQUAL_HINT(docs.size(), 5u, "All documents must be found"s); // 5 потому что MAX_DOCUMENT_COUNT = 5
    }

    server.RemoveDocument(2);
    ASSERT_EQUAL(server.GetDocumentCount(), 6);
    {
        const auto docs = server.FindTopDocuments("kind nasty black sharp cat"s);
        for (const auto doc : docs) {
        	cerr << doc.id << " ";
        } cerr << endl;
        ASSERT_EQUAL_HINT(docs.size(), 5u, "All documents must be found"s);
    }

    server.RemoveDocument(4);
    ASSERT_EQUAL(server.GetDocumentCount(), 5);
    {
        const auto docs = server.FindTopDocuments("kind nasty black sharp cat"s);
        for (const auto doc : docs) {
        	cerr << doc.id << " ";
        } cerr << endl;
        ASSERT_EQUAL_HINT(docs.size(), 5u, "All documents must be found"s);
    }

    server.RemoveDocument(6);
    ASSERT_EQUAL_HINT(server.GetDocumentCount(), 4, "3 documents have been removed"s);

    // Check document_data_
    ASSERT_HINT(server.GetWordFrequencies(2).empty(), "Server doesn't has id = 2, result must be empty"s);
    ASSERT_HINT(server.GetWordFrequencies(4).empty(), "Server doesn't has id = 4, result must be empty"s);
    ASSERT_HINT(server.GetWordFrequencies(6).empty(), "Server doesn't has id = 6, result must be empty"s);

    // Check document_ids_
    for (int id : server) {
        ASSERT_HINT(id % 2 == 1, "Only odd ids has been left"s);
    }

    // Check word_to_document_freqs_
    const auto docs = server.FindTopDocuments("kind nasty black sharp cat"s);
    for (const auto doc : docs) {
    	cerr << doc.id << " ";
    } cerr << endl;
    ASSERT_EQUAL_HINT(docs.size(), 4u, "All documents must be found"s);

    const double delta = 1e-6;
    ASSERT_EQUAL_HINT(docs.at(0).id, 7, "Max relevance has doc with id 7"s);
    ASSERT_HINT(InTheVicinity(docs.at(0).relevance, 0.6931471805599453, delta), "Wrong relevance"s);

    ASSERT_EQUAL_HINT(docs.at(1).id, 5, "Second relevance has doc with id 5"s);
    ASSERT_HINT(InTheVicinity(docs.at(1).relevance, 0.46209812037329684, delta), "Wrong relevance"s);

    ASSERT_EQUAL_HINT(docs.at(2).id, 1, "Third relevance has doc with id 1"s);
    ASSERT_HINT(InTheVicinity(docs.at(2).relevance, 0.34657359027997264, delta), "Wrong relevance"s);

    ASSERT_EQUAL_HINT(docs.at(3).id, 3, "Forth relevance has doc with id 3"s);
    ASSERT_HINT(InTheVicinity(docs.at(3).relevance, 0.34657359027997264, delta), "Wrong relevance"s);
}


void TestDuplicateDocumentsRemove() {

    SearchServer search_server("and with"s);

    AddDocument(search_server, 1, "funny pet and nasty rat"s, DocumentStatus::ACTUAL, { 7, 2, 7 });

    AddDocument(search_server, 2, "funny pet with curly hair"s, DocumentStatus::ACTUAL, { 1, 2 });

    // is a duplicate of document 2
    AddDocument(search_server, 3, "funny pet with curly hair"s, DocumentStatus::ACTUAL, { 1, 2 });

    // difference only in stop words => is a duplicate
    AddDocument(search_server, 4, "funny pet and curly hair"s, DocumentStatus::ACTUAL, { 1, 2 });

    // the same words set =>  is a duplicate of document 1
    AddDocument(search_server, 5, "funny funny pet and nasty nasty rat"s, DocumentStatus::ACTUAL, { 1, 2 });

    // added new words => not a duplicate
    AddDocument(search_server, 6, "funny pet and not very nasty rat"s, DocumentStatus::ACTUAL, { 1, 2 });

    // the same words set as in 6 => is a duplicate
    AddDocument(search_server, 7, "very nasty rat and not very funny pet"s, DocumentStatus::ACTUAL, { 1, 2 });

    // not all words => not a duplicate
    AddDocument(search_server, 8, "pet with rat and rat and rat"s, DocumentStatus::ACTUAL, { 1, 2 });

    // words from different documents => not a duplicate
    AddDocument(search_server, 9, "nasty rat with curly hair"s, DocumentStatus::ACTUAL, { 1, 2 });

    ASSERT_EQUAL(search_server.GetDocumentCount(), 9);

    const vector<int> added_document_ids = { 1, 2, 3, 4, 5, 6, 7, 8, 9 };

    vector<int> availible_document_ids;

    for (const auto& document_id : search_server) {
        availible_document_ids.push_back(document_id);
    }

    ASSERT_EQUAL(added_document_ids, availible_document_ids);

    // Temporary switch off cout output
    // get underlying buffer
      streambuf* orig_buf = cout.rdbuf();

    // set null
      cout.rdbuf(NULL);

    RemoveDuplicates(search_server);

    // restore buffer
      cout.rdbuf(orig_buf);

    const vector<int> residual_document_ids = { 1, 2, 6, 8, 9 };

    vector<int> after_deduplication_document_ids;

    for (const auto& document_id : search_server) {
        after_deduplication_document_ids.push_back(document_id);
    }

    ASSERT_EQUAL(residual_document_ids, after_deduplication_document_ids);
}

////// Sprint 8 /////

void TestProcessQueries() {

    SearchServer search_server("and with"s);

    for (
        int id = 0;
        const string& text : {
            "funny pet and nasty rat"s,
            "funny pet with curly hair"s,
            "funny pet and not very nasty rat"s,
            "pet with rat and rat and rat"s,
            "nasty rat with curly hair"s,
        }) {
        search_server.AddDocument(++id, text, DocumentStatus::ACTUAL, { 1, 2 });
    }

    const vector<string> queries = {
        "nasty rat -not"s,
        "not very funny nasty pet"s,
        "curly hair"s
    };

    std::vector<std::vector<Document>> result = ProcessQueries(search_server, queries);
    ASSERT_EQUAL(result[0].size(), 3);
    ASSERT_EQUAL(result[1].size(), 5);
    ASSERT_EQUAL(result[2].size(), 2);
}

void TestProcessQueriesJoined() {
    SearchServer search_server("and with"s);

    for (
        int id = 0;
        const string& text : {
            "funny pet and nasty rat"s,
            "funny pet with curly hair"s,
            "funny pet and not very nasty rat"s,
            "pet with rat and rat and rat"s,
            "nasty rat with curly hair"s,
        }) {
        search_server.AddDocument(++id, text, DocumentStatus::ACTUAL, { 1, 2 });
    }

    const vector<string> queries = {
        "nasty rat -not"s,
        "not very funny nasty pet"s,
        "curly hair"s
    };

    std::vector<Document> result = ProcessQueriesJoined(search_server, queries);
    ASSERT_EQUAL(result[0].id, 1);
    ASSERT_EQUAL(result[1].id, 5);
    ASSERT_EQUAL(result[2].id, 4);
    ASSERT_EQUAL(result[3].id, 3);
    ASSERT_EQUAL(result[4].id, 1);
    ASSERT_EQUAL(result[5].id, 2);
    ASSERT_EQUAL(result[6].id, 5);
    ASSERT_EQUAL(result[7].id, 4);
    ASSERT_EQUAL(result[8].id, 2);
    ASSERT_EQUAL(result[9].id, 5);
}

void TestRemoveDocumentMultiTread() {
    SearchServer search_server("and with"s);

    for (
        int id = 0;
        const string& text : {
            "funny pet and nasty rat"s,
            "funny pet with curly hair"s,
            "funny pet and not very nasty rat"s,
            "pet with rat and rat and rat"s,
            "nasty rat with curly hair"s,
        }) {
        search_server.AddDocument(++id, text, DocumentStatus::ACTUAL, { 1, 2 });
    }

    const string query = "curly and funny"s;

    ASSERT_EQUAL(search_server.GetDocumentCount(), 5);
    ASSERT_EQUAL(search_server.FindTopDocuments(query).size(), 4);
    // single thread version
    search_server.RemoveDocument(5);
    ASSERT_EQUAL(search_server.GetDocumentCount(), 4);
    ASSERT_EQUAL(search_server.FindTopDocuments(query).size(), 3);

    // single thread version
    search_server.RemoveDocument(execution::seq, 1);
    ASSERT_EQUAL(search_server.GetDocumentCount(), 3);
    ASSERT_EQUAL(search_server.FindTopDocuments(query).size(), 2);

    // multi thread version
    search_server.RemoveDocument(execution::par, 2);
    ASSERT_EQUAL(search_server.GetDocumentCount(), 2);
    ASSERT_EQUAL(search_server.FindTopDocuments(query).size(), 1);
}

void TestMatchDocumentMultiTread() {
    SearchServer search_server("and with"s);

    for (
        int id = 0;
        const string& text : {
            "funny pet and nasty rat"s,
            "funny pet with curly hair"s,
            "funny pet and not very nasty rat"s,
            "pet with rat and rat and rat"s,
            "nasty rat with curly hair"s,
        }) {
        search_server.AddDocument(++id, text, DocumentStatus::ACTUAL, { 1, 2 });
    }

    const string query = "curly and funny -not"s;

    {
        const auto [words, status] = search_server.MatchDocument(query, 1);
        ASSERT_EQUAL(words.size(), 1);
    }

    {
        const auto [words, status] = search_server.MatchDocument(execution::seq, query, 2);
        ASSERT_EQUAL(words.size(), 2);
    }

    {
        const auto [words, status] = search_server.MatchDocument(execution::par, query, 3);
        ASSERT_EQUAL(words.size(), 0);
    }
}

void TestFindTopDocumentsMultiTread() {
    SearchServer search_server("and with"s);

    for (
        int id = 0;
        const string& text : {
            "white cat and yellow hat"s,
            "curly cat curly tail"s,
            "nasty dog with big eyes"s,
            "nasty pigeon john"s,
        }) {
        search_server.AddDocument(++id, text, DocumentStatus::ACTUAL, { 1, 2 });
    }

    // seq version by default
    auto document = search_server.FindTopDocuments("curly nasty cat"s);
    ASSERT_EQUAL(document[0].id, 2);
    ASSERT_EQUAL(document[1].id, 4);
    ASSERT_EQUAL(document[2].id, 1);
    ASSERT_EQUAL(document[3].id, 3);

    // seq version
    document = search_server.FindTopDocuments(execution::seq, "curly nasty cat"s, DocumentStatus::BANNED);
    ASSERT_EQUAL(document.size(), 0);

    // par version
    document = search_server.FindTopDocuments(execution::par, "curly nasty cat"s, [](int document_id, DocumentStatus status, int rating) { return document_id % 2 == 0; });
    ASSERT_EQUAL(document[0].id, 2);
}

// Entry point
void TestSearchServer() {
    RUN_TEST(TestExcludeStopWordsFromAddedDocumentContent);
    RUN_TEST(TestAddDocument);
    RUN_TEST(TestStopWordsSupport);
    RUN_TEST(TestMinusWords);
    RUN_TEST(TestMatching);
    RUN_TEST(TestSortRelevance);
    RUN_TEST(TestRatingCalc);
    RUN_TEST(TestFilterPredicat);
    RUN_TEST(TestSearchWitnStatus);
    RUN_TEST(TestRelevanceCalc);

    RUN_TEST(TestExeptionConstructorInvalidChar);
    RUN_TEST(TestExeptionAddDocumentInvalidChar);
    RUN_TEST(TestExeptionAddDocumentInvalidDocumentID);
    RUN_TEST(TestExeptionAddDocumentSameDocumentID);
    RUN_TEST(TestExeptionFindTopDocumentsIllegalWordQuery);
    RUN_TEST(TestExeptionMatchDocumentWrongID);
    RUN_TEST(TestExeptionMatchDocumentIllegalWordQuery);
    RUN_TEST(TestExeptionFindTopDocumentsAnotherMinus);
    RUN_TEST(TestExeptionMatchDocumentAnotherMinus);

    RUN_TEST(TestPaginator);
    RUN_TEST(TestRequestQueue);

    RUN_TEST(TestRemoveDocument);
    RUN_TEST(TestDuplicateDocumentsRemove);

    RUN_TEST(TestProcessQueries);
    RUN_TEST(TestProcessQueriesJoined);

    RUN_TEST(TestRemoveDocumentMultiTread);
    RUN_TEST(TestMatchDocumentMultiTread);

    RUN_TEST(TestFindTopDocumentsMultiTread);

    cout << endl; // To separate test check and program output
}
