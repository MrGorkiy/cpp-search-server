#include <algorithm>
#include <cmath>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include <stdexcept>


using namespace std;

const int MAX_RESULT_DOCUMENT_COUNT = 5;
constexpr double comparison_error = 1e-6;

string ReadLine() {
    string s;
    getline(cin, s);
    return s;
}

int ReadLineWithNumber() {
    int result;
    cin >> result;
    ReadLine();
    return result;
}

vector<string> SplitIntoWords(const string &text) {
    vector<string> words;
    string word;
    for (const char c: text) {
        if (c == ' ') {
            if (!word.empty()) {
                words.push_back(word);
                word.clear();
            }
        } else {
            word += c;
        }
    }
    if (!word.empty()) {
        words.push_back(word);
    }

    return words;
}

struct Document {
    Document() = default;

    Document(int id, double relevance, int rating)
            : id(id), relevance(relevance), rating(rating) {
    }

    int id = 0;
    double relevance = 0.0;
    int rating = 0;
};

template<typename StringContainer>
set<string> MakeUniqueNonEmptyStrings(const StringContainer &strings) {
    set<string> non_empty_strings;
    for (const string &str: strings) {
        if (!str.empty()) {
            non_empty_strings.insert(str);
        }
    }
    return non_empty_strings;
}

enum class DocumentStatus {
    ACTUAL,
    IRRELEVANT,
    BANNED,
    REMOVED,
};

class SearchServer {
public:
    template<typename StringContainer>
    explicit SearchServer(const StringContainer &stop_words)
            : stop_words_(MakeUniqueNonEmptyStrings(stop_words)) {
        for (auto word: stop_words) {
            if (!IsValidWord(word)) {
                throw invalid_argument("Stop-words contain invalid characters in create SearchServer");
            }
        }
    }

    explicit SearchServer(const string &stop_words_text)
            : SearchServer(
            SplitIntoWords(stop_words_text)) {
    }

    void AddDocument(int document_id, const string &document, DocumentStatus status, const vector<int> &ratings) {
        if (document_id < 0 || documents_.count(document_id) > 0) {
            throw invalid_argument("A document with a negative ID or the ID of an existing document in AddDocument()");
        }
        if (document.empty()) {
            throw invalid_argument("Can't add empty document"s);
        }
        if (!IsValidWord(document)) {
            throw invalid_argument("Stop-words contain invalid characters in AddDocument()");
        }
        const vector<string> words = SplitIntoWordsNoStop(document);
        const double inv_word_count = 1.0 / words.size();
        for (const string &word: words) {
            word_to_document_freqs_[word][document_id] += inv_word_count;
        }
        documents_.emplace(document_id, DocumentData{ComputeAverageRating(ratings), status});
        documents_id_.push_back(document_id);
    }

    template<typename DocumentPredicate>
    vector<Document> FindTopDocuments(const string &raw_query, DocumentPredicate document_predicate) const {
        const Query query = ParseQuery(raw_query);
        auto matched_documents = FindAllDocuments(query, document_predicate);

        sort(matched_documents.begin(), matched_documents.end(),
             [](const Document &lhs, const Document &rhs) {
                 if (abs(lhs.relevance - rhs.relevance) < 1e-6) {
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

    vector<Document> FindTopDocuments(const string &raw_query, DocumentStatus status) const {
        return FindTopDocuments(
                raw_query, [status](int document_id, DocumentStatus document_status, int rating) {
                    return document_status == status;
                });
    }

    vector<Document> FindTopDocuments(const string &raw_query) const {
        return FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
    }

    int GetDocumentCount() const {
        return documents_.size();
    }

    int GetDocumentId(int index) {
        if (index < 0 || documents_id_.size() < index - 1) {
            throw out_of_range("The index of the transmitted document is out of the acceptable range");
        }
        return documents_id_.at(index);
    }

    tuple<vector<string>, DocumentStatus> MatchDocument(const string &raw_query, int document_id) const {
        const Query query = ParseQuery(raw_query);
        vector<string> matched_words;
        for (const string &word: query.plus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            if (word_to_document_freqs_.at(word).count(document_id)) {
                matched_words.push_back(word);
            }
        }
        for (const string &word: query.minus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            if (word_to_document_freqs_.at(word).count(document_id)) {
                matched_words.clear();
                break;
            }
        }
        return {matched_words, documents_.at(document_id).status};
    }

private:
    struct DocumentData {
        int rating;
        DocumentStatus status;
    };
    const set<string> stop_words_; // множество стоп слов
    map<string, map<int, double>> word_to_document_freqs_; // словарь слов : <слово, <document_id, term_freq>>
    map<int, DocumentData> documents_; // словарь документов <document_id, данные>
    vector<int> documents_id_; // множество ids документов на сервере

    bool IsStopWord(const string &word) const {
        return stop_words_.count(word) > 0;
    }

    static bool IsValidWord(const string &word) {
        return none_of(word.begin(), word.end(), [](char c) {
            return c >= '\0' && c < ' ';
        });
    }

    vector<string> SplitIntoWordsNoStop(const string &text) const {
        vector<string> words;
        for (const string &word: SplitIntoWords(text)) {
            if (!IsStopWord(word)) {
                words.push_back(word);
            }
        }
        return words;
    }

    static int ComputeAverageRating(const vector<int> &ratings) {
        if (ratings.empty()) {
            return 0;
        }
        int rating_sum = 0;
        for (const int rating: ratings) {
            rating_sum += rating;
        }
        return rating_sum / static_cast<int>(ratings.size());
    }

    struct QueryWord {
        string data;
        bool is_minus;
        bool is_stop;
    };

    QueryWord ParseQueryWord(string text) const {
        bool is_minus = false;
        if (text.at(0) == '-') {
            text = text.substr(1);
            if (text.empty() || text.at(0) == '-') {
                throw invalid_argument("Error in Stop-words");
            }
            is_minus = true;
        }
        return {text, is_minus, IsStopWord(text)};
    }

    struct Query {
        set<string> plus_words;
        set<string> minus_words;
    };

    Query ParseQuery(const string &text) const {
        if (!IsValidWord(text)) {
            throw invalid_argument("Stop-words contain invalid characters");
        }
        Query query;
        for (const string &word: SplitIntoWords(text)) {
            const QueryWord query_word = ParseQueryWord(word);
            if (!query_word.is_stop) {
                if (query_word.is_minus) {
                    query.minus_words.insert(query_word.data);
                } else {
                    query.plus_words.insert(query_word.data);
                }
            }
        }
        return query;
    }

    double ComputeWordInverseDocumentFreq(const string &word) const {
        return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
    }

    template<typename DocumentPredicate>
    vector<Document> FindAllDocuments(const Query &query,
                                      DocumentPredicate document_predicate) const {
        map<int, double> document_to_relevance;
        for (const string &word: query.plus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
            for (const auto [document_id, term_freq]: word_to_document_freqs_.at(word)) {
                const auto &document_data = documents_.at(document_id);
                if (document_predicate(document_id, document_data.status, document_data.rating)) {
                    document_to_relevance[document_id] += term_freq * inverse_document_freq;
                }
            }
        }

        for (const string &word: query.minus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            for (const auto [document_id, _]: word_to_document_freqs_.at(word)) {
                document_to_relevance.erase(document_id);
            }
        }

        vector<Document> matched_documents;
        for (const auto [document_id, relevance]: document_to_relevance) {
            matched_documents.push_back(
                    {document_id, relevance, documents_.at(document_id).rating});
        }
        return matched_documents;
    }
};

// ==================== для примера =========================

void PrintDocument(const Document &document) {
    cout << "{ "s
         << "document_id = "s << document.id << ", "s
         << "relevance = "s << document.relevance << ", "s
         << "rating = "s << document.rating << " }"s << endl;
}

/*
   Подставьте сюда вашу реализацию макросов
   ASSERT, ASSERT_EQUAL, ASSERT_EQUAL_HINT, ASSERT_HINT и RUN_TEST
*/
template<typename El1, typename El2>
ostream &operator<<(ostream &out, const pair<El1, El2> &container) {
    return out << container.first << ": " << container.second;
}

template<typename Element>
void Print(ostream &out, const Element &container) {
    bool is_first = true;
    for (const auto &element: container) {
        if (!is_first) {
            out << ", "s;
        }
        is_first = false;
        out << element;
    }
}

template<typename Element>
ostream &operator<<(ostream &out, const vector<Element> &container) {
    out << "["s;
    Print(out, container);
    out << "]"s;
    return out;
}

template<typename Element>
ostream &operator<<(ostream &out, const set<Element> &container) {
    out << "{"s;
    Print(out, container);
    out << "}"s;
    return out;
}

template<typename Key, typename Value>
ostream &operator<<(ostream &out, const map<Key, Value> &container) {
    out << "{"s;
    Print(out, container);
    out << "}"s;
    return out;
}

template<typename T, typename U>
void AssertEqualImpl(const T &t, const U &u, const string &t_str, const string &u_str, const string &file,
                     const string &func, unsigned line, const string &hint) {
    if (t != u) {
        cerr << boolalpha;
        cerr << file << "("s << line << "): "s << func << ": "s;
        cerr << "ASSERT_EQUAL("s << t_str << ", "s << u_str << ") failed: "s;
        cerr << t << " != "s << u << "."s;
        if (!hint.empty()) {
            cerr << " Hint: "s << hint;
        }
        cerr << endl;
        abort();
    }
}

#define ASSERT_EQUAL(a, b) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, ""s)

#define ASSERT_EQUAL_HINT(a, b, hint) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, (hint))

void AssertImpl(bool value, const string &expr_str, const string &file, const string &func, unsigned line,
                const string &hint) {
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

#define ASSERT(expr) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, ""s)

#define ASSERT_HINT(expr, hint) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, (hint))

template<typename F>
void RunTestImpl(const F &f, const string &func) {
    f();
    cerr << func << " OK"s << endl;
}

#define RUN_TEST(func) RunTestImpl((func) , #func) // напишите недостающий код
// -------- Начало модульных тестов поисковой системы ----------

// Тест проверяет, что поисковая система исключает стоп-слова при добавлении документов
void TestExcludeStopWordsFromAddedDocumentContent() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = {1, 2, 3};
    {
        SearchServer server(""s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("in"s);
        ASSERT_EQUAL(found_docs.size(), 1u);
        const Document &doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, doc_id);
    }

    {
        SearchServer server("in the"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT_HINT(server.FindTopDocuments("in"s).empty(),
                    "Stop words must be excluded from documents"s);
    }
}

// Тест проверяет добавление документов. Поисковому по запросу, который содержит слова из документа.
void TestAddDocumentAndSearch() {
    const int doc_id_1 = 40;
    const string content_1 = "cat in the city"s;
    const int doc_id_2 = 41;
    const string content_2 = "maxim writes the code"s;
    const vector<int> ratings = {1, 2, 3};

    {
        SearchServer server("in the"s);
        server.AddDocument(doc_id_1, content_1, DocumentStatus::ACTUAL, ratings);
        server.AddDocument(doc_id_2, content_2, DocumentStatus::ACTUAL, ratings);

        const auto found_docs = server.FindTopDocuments("maxim"s);
        ASSERT_EQUAL(found_docs[0].id, doc_id_2);
        ASSERT_EQUAL(found_docs.size(), 1);
    }

    {
        SearchServer server("in the"s);
        server.AddDocument(doc_id_1, content_1, DocumentStatus::ACTUAL, ratings);
        server.AddDocument(doc_id_2, content_2, DocumentStatus::ACTUAL, ratings);

        ASSERT(server.FindTopDocuments("dog"s).empty());
    }
}

// Поддержка минус-слов. Документы, содержащие минус-слова поискового запроса, не должны включаться в результаты поиска.
void TestStopWords() {
    const int doc_id_1 = 40;
    const string content_1 = "maxim create code in unit test"s;
    const int doc_id_2 = 41;
    const string content_2 = "dima writes the test code"s;
    const vector<int> ratings = {1, 2, 3};

    {
        SearchServer server("in the"s);
        server.AddDocument(doc_id_1, content_1, DocumentStatus::ACTUAL, ratings);
        server.AddDocument(doc_id_2, content_2, DocumentStatus::ACTUAL, ratings);

        const auto found_docs = server.FindTopDocuments("-maxim code test"s);
        ASSERT_EQUAL(found_docs[0].id, doc_id_2);
        ASSERT_EQUAL(found_docs.size(), 1);
    }

    {
        SearchServer server("in the"s);
        server.AddDocument(doc_id_1, content_1, DocumentStatus::ACTUAL, ratings);
        server.AddDocument(doc_id_2, content_2, DocumentStatus::ACTUAL, ratings);

        const auto found_docs = server.FindTopDocuments("maxim -code test"s);
        ASSERT(found_docs.empty());
    }
}

// Тест матчинга документов.
void TestMatchingDocuments() {
    const int doc_id_1 = 40;
    const string content_1 = "maxim create code in unit test"s;
    const int doc_id_2 = 41;
    const string content_2 = "dima writes the test code"s;
    const vector<int> ratings = {1, 2, 3};

    {
        SearchServer server("in the"s);
        server.AddDocument(doc_id_1, content_1, DocumentStatus::ACTUAL, ratings);
        server.AddDocument(doc_id_2, content_2, DocumentStatus::ACTUAL, ratings);

        const auto found_docs = server.FindTopDocuments("code test"s);
        ASSERT_EQUAL(found_docs[0].id, doc_id_1);
        ASSERT_EQUAL(found_docs[1].id, doc_id_2);
        ASSERT_EQUAL(found_docs.size(), 2);
    }

    {
        SearchServer server("in the"s);
        server.AddDocument(doc_id_1, content_1, DocumentStatus::ACTUAL, ratings);
        server.AddDocument(doc_id_2, content_2, DocumentStatus::ACTUAL, ratings);

        const auto found_docs = server.FindTopDocuments("maxim -code test"s);
        ASSERT(found_docs.empty());
    }

    {
        SearchServer server("in the"s);
        server.AddDocument(doc_id_1, content_1, DocumentStatus::ACTUAL, ratings);
        server.AddDocument(doc_id_2, content_2, DocumentStatus::ACTUAL, ratings);

        const auto found_docs = server.MatchDocument("-maxim code test"s, 41);
        const vector<string> result = {"code"s, "test"s};
        ASSERT_EQUAL(get<0>(found_docs), result);
        ASSERT_EQUAL(get<0>(found_docs).size(), 2);
    }

    {
        SearchServer server("in the"s);
        server.AddDocument(doc_id_1, content_1, DocumentStatus::ACTUAL, ratings);
        server.AddDocument(doc_id_2, content_2, DocumentStatus::ACTUAL, ratings);

        const auto found_docs = server.FindTopDocuments("-maxim code test"s);
        ASSERT_EQUAL(found_docs[0].id, doc_id_2);
        ASSERT_EQUAL(found_docs.size(), 1);
    }
}

// Сортировка найденных документов по релевантности
void TestSortRelevanceDocuments() {
    const int doc_id_1 = 40;
    const string content_1 = "maxim create code in unit test"s;
    const vector<int> ratings_1 = {1, 2, 3};
    const int doc_id_2 = 41;
    const string content_2 = "dima writes the test code"s;
    const vector<int> ratings_2 = {2, 3, 4};

    {
        SearchServer server("in the"s);
        server.AddDocument(doc_id_1, content_1, DocumentStatus::ACTUAL, ratings_1);
        server.AddDocument(doc_id_2, content_2, DocumentStatus::ACTUAL, ratings_2);

        const auto found_docs = server.FindTopDocuments("maxim code"s);
        const auto relevance = 0.138629;

        ASSERT_EQUAL(found_docs[0].id, doc_id_1);
        ASSERT_EQUAL(found_docs[1].id, doc_id_2);
        ASSERT(found_docs[1].relevance < relevance);
    }

    {
        SearchServer server("in the"s);
        server.AddDocument(doc_id_1, content_1, DocumentStatus::ACTUAL, ratings_1);
        server.AddDocument(doc_id_2, content_2, DocumentStatus::ACTUAL, ratings_2);

        const auto found_docs = server.FindTopDocuments("dima create code"s);
        const auto relevance = 0.173287;

        ASSERT_EQUAL(found_docs[0].id, doc_id_2);
        ASSERT_EQUAL(found_docs[1].id, doc_id_1);
        ASSERT(found_docs[1].relevance < relevance);
    }
}

// Тест вычисления рейтинга документов
void TestRatingDocuments() {
    const int doc_id_1 = 40;
    const string content_1 = "maxim create code in unit test"s;
    const vector<int> ratings_1 = {1};
    const int doc_id_2 = 41;
    const string content_2 = "dima writes the test code"s;
    const vector<int> ratings_2 = {2, 3, 4};
    const int doc_id_3 = 42;
    const string content_3 = "dima writes the test code"s;
    const vector<int> ratings_3 = {5, 8, 9};

    {
        SearchServer server("in the"s);
        server.AddDocument(doc_id_1, content_1, DocumentStatus::ACTUAL, ratings_1);
        server.AddDocument(doc_id_2, content_2, DocumentStatus::ACTUAL, ratings_2);

        const auto found_docs = server.FindTopDocuments("maxim code"s);
        ASSERT_EQUAL(found_docs[0].rating, 1);
        ASSERT(found_docs[1].rating == 3);
    }

    {
        SearchServer server("in the"s);
        server.AddDocument(doc_id_1, content_1, DocumentStatus::ACTUAL, ratings_1);
        server.AddDocument(doc_id_2, content_2, DocumentStatus::ACTUAL, ratings_2);
        server.AddDocument(doc_id_3, content_3, DocumentStatus::ACTUAL, ratings_3);

        const auto found_docs = server.FindTopDocuments("dima create code"s);
        ASSERT(found_docs[0].rating == 1);
        ASSERT_HINT(found_docs[1].rating == 7, "No Rating test accessed"s);
        ASSERT(found_docs[2].rating == 3);
    }
}

// Тест фильтрации результатов поиска с использованием предиката
void TestFiltersDocuments() {
    const int doc_id_1 = 40;
    const string content_1 = "maxim create code in unit test"s;
    const vector<int> ratings_1 = {1};
    const int doc_id_2 = 41;
    const string content_2 = "dima writes the test code"s;
    const vector<int> ratings_2 = {2, 3, 4};
    const int doc_id_3 = 42;
    const string content_3 = "dima writes the test code"s;
    const vector<int> ratings_3 = {5, 8, 9};

    {
        SearchServer server("in the"s);
        server.AddDocument(doc_id_1, content_1, DocumentStatus::ACTUAL, ratings_1);
        server.AddDocument(doc_id_2, content_2, DocumentStatus::ACTUAL, ratings_2);

        const auto found_docs = server.FindTopDocuments("maxim code"s,
                                                        [](int document_id, DocumentStatus status, int rating) {
                                                            return document_id % 2 == 0;
                                                        });
        ASSERT_EQUAL(found_docs[0].id, doc_id_1);
        ASSERT_EQUAL(found_docs.size(), 1);
    }

    {
        SearchServer server("in the"s);
        server.AddDocument(doc_id_1, content_1, DocumentStatus::ACTUAL, ratings_1);
        server.AddDocument(doc_id_2, content_2, DocumentStatus::ACTUAL, ratings_2);
        server.AddDocument(doc_id_3, content_3, DocumentStatus::ACTUAL, ratings_3);

        const auto found_docs = server.FindTopDocuments("maxim code"s,
                                                        [](int document_id, DocumentStatus status, int rating) {
                                                            return rating > 2;
                                                        });

        ASSERT_EQUAL(found_docs[0].id, doc_id_3);
        ASSERT(found_docs[0].rating > 2);

        ASSERT_EQUAL(found_docs[1].id, doc_id_2);
        ASSERT(found_docs[1].rating > 2);

        ASSERT(found_docs.size() == 2);
    }
}

// Тест поиска документов, имеющих заданный статус
void TestSearchDocumentsIsStatus() {
    const int doc_id_1 = 40;
    const string content_1 = "maxim create code in unit test"s;
    const vector<int> ratings_1 = {1};
    const int doc_id_2 = 41;
    const string content_2 = "dima writes the test code"s;
    const vector<int> ratings_2 = {2, 3, 4};
    const int doc_id_3 = 42;
    const string content_3 = "dima writes the test code"s;
    const vector<int> ratings_3 = {5, 8, 9};

    {
        SearchServer server("in the"s);
        server.AddDocument(doc_id_1, content_1, DocumentStatus::ACTUAL, ratings_1);
        server.AddDocument(doc_id_2, content_2, DocumentStatus::BANNED, ratings_2);

        const auto found_docs = server.FindTopDocuments("maxim code"s, DocumentStatus::BANNED);
        ASSERT_EQUAL(found_docs[0].id, doc_id_2);
        ASSERT(found_docs.size() == 1);
    }

    {
        SearchServer server("in the"s);
        server.AddDocument(doc_id_1, content_1, DocumentStatus::IRRELEVANT, ratings_1);
        server.AddDocument(doc_id_2, content_2, DocumentStatus::BANNED, ratings_2);
        server.AddDocument(doc_id_3, content_3, DocumentStatus::IRRELEVANT, ratings_3);

        const auto found_docs = server.FindTopDocuments("maxim code"s, DocumentStatus::IRRELEVANT);
        ASSERT_EQUAL(found_docs[0].id, doc_id_1);
        ASSERT_EQUAL(found_docs[1].id, doc_id_3);
        ASSERT(found_docs.size() == 2);
    }

    {
        SearchServer server("in the"s);
        server.AddDocument(doc_id_1, content_1, DocumentStatus::IRRELEVANT, ratings_1);
        server.AddDocument(doc_id_2, content_2, DocumentStatus::ACTUAL, ratings_2);

        const auto found_docs = server.MatchDocument("code test"s, 41);
        ASSERT(get<1>(found_docs) == DocumentStatus::ACTUAL);
    }

    {
        SearchServer server("in the"s);
        server.AddDocument(doc_id_1, content_1, DocumentStatus::IRRELEVANT, ratings_1);
        server.AddDocument(doc_id_2, content_2, DocumentStatus::ACTUAL, ratings_2);

        const auto found_docs = server.MatchDocument("code test"s, 40);
        ASSERT(get<1>(found_docs) == DocumentStatus::IRRELEVANT);
    }
}

// Тест корректности вычисления релевантности найденных документов
void TestCorrectRelevanceFindDocuments() {
    const int doc_id_1 = 40;
    const string content_1 = "белый кот и модный ошейник"s;
    const vector<int> ratings_1 = {1, 2, 3};
    const int doc_id_2 = 41;
    const string content_2 = "пушистый кот пушистый хвост"s;
    const vector<int> ratings_2 = {2, 3, 4};
    const int doc_id_3 = 42;
    const string content_3 = "ухоженный пёс выразительные глаза"s;
    const vector<int> ratings_3 = {5, 8, 9};

    {
        SearchServer server("и в на"s);
        server.AddDocument(doc_id_1, content_1, DocumentStatus::ACTUAL, ratings_1);
        server.AddDocument(doc_id_2, content_2, DocumentStatus::ACTUAL, ratings_2);
        server.AddDocument(doc_id_3, content_3, DocumentStatus::ACTUAL, ratings_3);

        const auto found_docs = server.FindTopDocuments("пушистый ухоженный кот"s);
        const auto relevance_0 = 0.650672;
        const auto relevance_1 = 0.274653;
        const auto relevance_2 = 0.101366;
        ASSERT(found_docs[0].relevance - relevance_0 < comparison_error);
        ASSERT(found_docs[1].relevance - relevance_1 < comparison_error);
        ASSERT(found_docs[2].relevance - relevance_2 < comparison_error);
    }
}

// Функция TestSearchServer является точкой входа для запуска тестов
void TestSearchServer() {
    RUN_TEST(TestExcludeStopWordsFromAddedDocumentContent);
    RUN_TEST(TestAddDocumentAndSearch);
    RUN_TEST(TestStopWords);
    RUN_TEST(TestAddDocumentAndSearch);
    RUN_TEST(TestAddDocumentAndSearch);
    RUN_TEST(TestMatchingDocuments);
    RUN_TEST(TestSortRelevanceDocuments);
    RUN_TEST(TestRatingDocuments);
    RUN_TEST(TestFiltersDocuments);
    RUN_TEST(TestSearchDocumentsIsStatus);
    RUN_TEST(TestCorrectRelevanceFindDocuments);
}

// --------- Окончание модульных тестов поисковой системы -----------

int main() {
    TestSearchServer();
    cout << "Search server testing finished"s << endl;
}