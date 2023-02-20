#pragma once

#include <map>
#include <algorithm>
#include <execution>
#include <set>
#include <vector>

#include "document.h"
#include "string_processing.h"
#include "concurrent_map.h"

inline static constexpr double EPSILON = 1e-6;

class SearchServer {
public:
    // Конструкторы
    template<typename StringContainer>
    explicit SearchServer(const StringContainer &stop_words)
            : stop_words_(MakeUniqueNonEmptyStrings(stop_words)) {
        using std::string_literals::operator ""s;

        if (!(std::all_of(stop_words_.begin(), stop_words_.end(), IsValidWord))) {
            throw std::invalid_argument("Special character detected"s);
        }
    }

    explicit SearchServer(const std::string &stop_words_text)
            : SearchServer(SplitIntoWords(stop_words_text)) {

    }

    // Добавит документ
    void AddDocument(int document_id, std::string_view document,
                     DocumentStatus status, const std::vector<int> &ratings);

    // Удаление документа
    void RemoveDocument(int document_id);

    // Версия метода с возможностью выбора политики выполнения
    template<typename ExecutionPolicy>
    void RemoveDocument(ExecutionPolicy &&policy, int document_id);

    // Итераторы по id-s документов в сервере
    std::set<int>::iterator begin() const;

    std::set<int>::iterator end() const;

    // Поиск документов по запросу
    template<typename DocumentPredicate>
    std::vector<Document> FindTopDocuments(const std::string_view &raw_query,
                                           DocumentPredicate document_predicate) const;

    template<typename DocumentPredicate, typename ExecutionPolicy>
    std::vector<Document> FindTopDocuments(ExecutionPolicy &&policy, const std::string_view &raw_query,
                                           DocumentPredicate document_predicate) const;

    template<typename ExecutionPolicy>
    std::vector<Document>
    FindTopDocuments(ExecutionPolicy &&policy, const std::string_view &raw_query, DocumentStatus status) const;

    std::vector<Document> FindTopDocuments(const std::string_view &raw_query, DocumentStatus status) const;

    template<typename ExecutionPolicy>
    std::vector<Document> FindTopDocuments(ExecutionPolicy &&policy, const std::string_view &raw_query) const;

    std::vector<Document> FindTopDocuments(const std::string_view &raw_query) const;

    int GetDocumentCount() const;

    // Общие слова и статусы документов по ID
    std::tuple<std::vector<std::string_view>, DocumentStatus>
    MatchDocument(const std::string_view raw_query, int document_id) const;

    std::tuple<std::vector<std::string_view>, DocumentStatus>
    MatchDocument(const std::execution::sequenced_policy &,
                  std::string_view raw_query, int document_id) const;

    std::tuple<std::vector<std::string_view>, DocumentStatus>
    MatchDocument(const std::execution::parallel_policy &,
                  std::string_view raw_query, int document_id) const;

    // Метод получения частот слов по id документа.
    const std::map<std::string_view, double> &GetWordFrequencies(int document_id) const;

private:
    // Структура хранения документов
    struct DocumentData {
        int rating;
        DocumentStatus status;
        std::string data;
    };
    const std::set<std::string> stop_words_; // Множество стоп слов.
    std::map<std::string_view, std::map<int, double>> word_to_document_freqs_; // Словарь: Слово - ID, IDF
    std::map<int, std::map<std::string_view, double>> document_to_word_freqs_; // Словарь: ID - Слово, IDF
    std::map<int, DocumentData> documents_; // Словарь ID добавленных документов и структура данных
    std::set<int> document_ids_; // все добавленные ID документов

    static bool IsValidWord(std::string_view word);

    bool IsStopWord(std::string_view word) const;

    std::vector<std::string_view> SplitIntoWordsNoStop(const std::string_view &text) const;

    static int ComputeAverageRating(const std::vector<int> &ratings);

    struct QueryWord {
        std::string_view data;
        bool is_minus;
        bool is_stop;
    };

    QueryWord ParseQueryWord(const std::string_view &text) const;

    struct Query {
        std::vector<std::string_view> plus_words;
        std::vector<std::string_view> minus_words;
    };

    Query ParseQuery(const std::string_view &text, bool= true) const;

    double ComputeWordInverseDocumentFreq(const std::string_view &word) const;

    template<typename DocumentPredicate>
    std::vector<Document> FindAllDocuments(const Query &query,
                                           DocumentPredicate document_predicate) const;

    template<typename DocumentPredicate>
    std::vector<Document> FindAllDocuments(const std::execution::sequenced_policy &policy,
                                           const Query &query,
                                           DocumentPredicate document_predicate) const;

    template<typename DocumentPredicate>
    std::vector<Document> FindAllDocuments(const std::execution::parallel_policy &policy,
                                           const Query &query,
                                           DocumentPredicate document_predicate) const;
};

template<typename ExecutionPolicy>
void SearchServer::RemoveDocument(ExecutionPolicy &&policy, int document_id) {
    if (document_to_word_freqs_.count(document_id) == 0) {
        return;
    }

    const auto &word_freq = document_to_word_freqs_.at(document_id);
    std::vector<std::string_view> words(word_freq.size());

    std::transform(policy, word_freq.begin(), word_freq.end(), words.begin(),
                   [](const std::pair<const std::string_view, double> &el) {
                       return el.first;
                   });

    std::for_each(policy, words.begin(), words.end(),
                  [this, document_id](const std::string_view key) {
                      word_to_document_freqs_.at(key).erase(document_id);
                  });

    document_to_word_freqs_.erase(document_id);
    documents_.erase(document_id);
    document_ids_.erase(document_id);
}

template<typename DocumentPredicate, typename ExecutionPolicy>
std::vector<Document> SearchServer::FindTopDocuments(ExecutionPolicy &&policy, const std::string_view &raw_query,
                                                     DocumentPredicate document_predicate) const {
    const auto query = ParseQuery(raw_query);

    auto matched_documents = FindAllDocuments(policy, query, document_predicate);

    sort(policy,
         matched_documents.begin(), matched_documents.end(),
         [](const Document &lhs, const Document &rhs) {
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

template<typename DocumentPredicate>
std::vector<Document>
SearchServer::FindTopDocuments(const std::string_view &raw_query, DocumentPredicate document_predicate) const {
    return FindTopDocuments(std::execution::seq, raw_query, document_predicate);
}

template<typename ExecutionPolicy>
std::vector<Document>
SearchServer::FindTopDocuments(ExecutionPolicy &&policy, const std::string_view &raw_query, DocumentStatus status) const {
    return FindTopDocuments(policy, raw_query,
                            [&status](int document_id, DocumentStatus new_status, int rating) {
                                return new_status == status;
                            });
}

template<typename ExecutionPolicy>
std::vector<Document> SearchServer::FindTopDocuments(ExecutionPolicy &&policy, const std::string_view &raw_query) const {
    return FindTopDocuments(policy, raw_query, DocumentStatus::ACTUAL);
}

template<typename DocumentPredicate>
std::vector<Document> SearchServer::FindAllDocuments(const std::execution::sequenced_policy &policy,
                                                     const Query &query,
                                                     DocumentPredicate document_predicate) const {
    std::map<int, double> document_to_relevance;
    for (const std::string_view &word: query.plus_words) {
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

    for (const std::string_view &word: query.minus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        for (const auto [document_id, _]: word_to_document_freqs_.at(word)) {
            document_to_relevance.erase(document_id);
        }
    }

    std::vector<Document> matched_documents;
    for (const auto [document_id, relevance]: document_to_relevance) {
        matched_documents.push_back(
                {document_id, relevance, documents_.at(document_id).rating});
    }
    return matched_documents;
}

template<typename DocumentPredicate>
std::vector<Document> SearchServer::FindAllDocuments(const Query &query,
                                                     DocumentPredicate document_predicate) const {
    return FindAllDocuments(std::execution::seq, query, document_predicate);
}

template<typename DocumentPredicate>
std::vector<Document>
SearchServer::FindAllDocuments(const std::execution::parallel_policy &policy,
                               const Query &query,
                               DocumentPredicate document_predicate) const {
    ConcurrentMap<int, double> document_to_relevance(16);

    std::for_each(policy,
                  query.minus_words.begin(), query.minus_words.end(),
                  [this, &document_to_relevance](std::string_view word) {
                      if (word_to_document_freqs_.count(word)) {
                          for (const auto [document_id, _]: word_to_document_freqs_.at(word)) {
                              document_to_relevance.Erase(document_id);
                          }
                      }
                  });

    std::for_each(policy,
                  query.plus_words.begin(), query.plus_words.end(),
                  [this, &document_predicate, &document_to_relevance](std::string_view word) {
                      if (word_to_document_freqs_.count(word)) {
                          const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
                          for (const auto [document_id, term_freq]: word_to_document_freqs_.at(word)) {
                              const auto &document_data = documents_.at(document_id);
                              if (document_predicate(document_id, document_data.status, document_data.rating)) {
                                  document_to_relevance[document_id].ref_to_value += term_freq * inverse_document_freq;
                              }
                          }
                      }
                  });

    std::map<int, double> document_to_relevance_reduced = document_to_relevance.BuildOrdinaryMap();
    std::vector<Document> matched_documents;
    matched_documents.reserve(document_to_relevance_reduced.size());

    for (const auto [document_id, relevance]: document_to_relevance_reduced) {
        matched_documents.emplace_back(document_id, relevance, documents_.at(document_id).rating);
    }
    return matched_documents;
}

// Выводит результаты в консоль
void PrintMatchDocumentResult(int document_id, const std::vector<std::string> &words, DocumentStatus status);

void PrintDocument(const Document &document);