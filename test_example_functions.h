#pragma once

#include "process_queries.h"
#include "remove_duplicates.h"
#include "request_queue.h"
#include "search_server.h"

#include <iomanip>
#include <iostream>
#include <map>
#include <set>
#include <vector>

using std::string_literals::operator""s;

template <typename T>
void Print(std::ostream& out, T container) {
    int size = container.size();

    for (auto element : container) {
        --size;

        if (size > 0) {
            out << element << ", "s;
        } else {
            out << element;
        }
    }
}

// Шаблон для вывода вектора
template <typename T>
std::ostream& operator<<(std::ostream& out, const std::vector<T>& container) {
    out << "["s;
    Print(out, container);
    out << "]"s;

    return out;
}

// Шаблон для вывода множества
template <typename T>
std::ostream& operator<<(std::ostream& out, const std::set<T>& container) {
    out << "{"s;
    Print(out, container);
    out << "}"s;

    return out;
}

// Шаблон для вывода словаря 1.1
template <typename Key, typename Value>
std::ostream& operator<<(std::ostream& out, const std::map<Key, Value>& container) {
    out << "{"s;
    Print(out, container);
    out << "}"s;

    return out;
}

// Шаблон для вывода словаря 1.2
template <typename L, typename R>
std::ostream& operator<<(std::ostream& out, const std::pair<L, R>& mypair) {
    const auto& [l, r] = mypair;

    out << l;
    out << ": "s;
    out << r;

    return out;
}

// Шаблонная функция для проверки равенства двух произвольных элементов
template <typename T, typename U>
void AssertEqualImpl(const T& t, const U& u, const std::string& t_str, const std::string& u_str,
                     const std::string& file, const std::string& func, unsigned line, const std::string& hint) {
    if (t != u) {
        std::cerr << std::boolalpha;
        std::cerr << file << "("s << line << "): "s << func << ": "s;
        std::cerr << "ASSERT_EQUAL("s << t_str << ", "s << u_str << ") failed: "s;
        std::cerr << t << " != "s << u << "."s;

        if (!hint.empty()) {
            std::cerr << " Hint: "s << hint;
        }

        std::cerr << std::endl;

        std::abort();
    }
}

// Функция для проверки истинности логического выражения
void AssertImpl(bool value, const std::string& expr_str, const std::string& file, const std::string& func,
                unsigned line, const std::string& hint);

// Макросы подставляющие функции с параметрами на место ASSERT...
#define ASSERT_EQUAL(a, b) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, ""s)
#define ASSERT_EQUAL_HINT(a, b, hint) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, (hint))

#define ASSERT(expr) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, ""s)
#define ASSERT_HINT(expr, hint) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, (hint))

// Шаблонная функция для вызова теста по имени его функции
template <typename F>
void RunTestImpl(F foo, const std::string& function_name) {
    foo();
    std::cerr << function_name << " OK" << std::endl;
}

#define RUN_TEST(func) RunTestImpl((func), #func)

void TestExcludeStopWordsFromAddedDocumentContent();
void TestAddDocument();
void TestStopWordsSupport();
void TestMinusWords();
void TestMatching();
void TestSortRelevance();
void TestRatingCalc();
void TestFilterPredicat();
void TestSearchWitnStatus();
void TestRelevanceCalc();

// sprint 3 Added Exeptions
void TestExeptionConstructorInvalidChar();
void TestExeptionAddDocumentInvalidChar();
void TestExeptionAddDocumentInvalidDocumentID();
void TestExeptionAddDocumentSameDocumentID();
void TestExeptionFindTopDocumentsIllegalWordQuery();
void TestExeptionFindTopDocumentsAnotherMinus();
void TestExeptionMatchDocumentWrongID();
void TestExeptionMatchDocumentIllegalWordQuery();
void TestExeptionMatchDocumentAnotherMinus();

// sprint 4 Added Paginator and ReguestQueue class
void TestPaginator();
void TestRequestQueue();

// sprint 5 Added RemoveDuplicate function
bool InTheVicinity(const double d1, const double d2, const double delta);
void TestRemoveDocument();

void TestDuplicateDocumentsRemove();

// sprint 8 Added
void TestProcessQueries();
void TestProcessQueriesJoined();

void TestRemoveDocumentMultiTread();
void TestMatchDocumentMultiTread();

void TestFindTopDocumentsMultiTread();

// Entry point
void TestSearchServer();
