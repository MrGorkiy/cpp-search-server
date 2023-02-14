#include "search_server.h"

#include <cmath>

using std::string_literals::operator""s;

std::set<int>::iterator SearchServer::begin() const {
    return document_ids_.begin();
}

std::set<int>::iterator SearchServer::end() const {
    return document_ids_.end();
}

// Добавление нового документа.
void SearchServer::AddDocument(int document_id, std::string_view document,
                               DocumentStatus status, const std::vector<int> &ratings) {

    // Попытка добавить документ с отрицательным id или с id ранее добавленного
    // документа
    if ((document_id < 0) || (documents_.count(document_id) > 0)) {
        throw std::invalid_argument("Invalid document id"s);
    }

    documents_.emplace(document_id,
                       DocumentData{
                               ComputeAverageRating(ratings),
                               status,
                               std::string(document) // Оригинал строки
                       });

    auto &src_string = documents_.find(document_id)->second.data;
    const std::vector<std::string_view> words = SplitIntoWordsNoStop(src_string);
    const double inv_word_count = 1.0 / words.size();

    for (const std::string_view word : words) {
        word_to_document_freqs_[word][document_id] += inv_word_count;
        document_to_word_freqs_[document_id][word] += inv_word_count;
    }

    document_ids_.insert(document_id);
}

// Удаление документа по ID.
void SearchServer::RemoveDocument(int document_id) {
    return RemoveDocument(std::execution::seq, document_id);
}

// Поиск документов по запросу + статусу.
std::vector<Document> SearchServer::FindTopDocuments(const std::string &raw_query, DocumentStatus status) const {
    return FindTopDocuments(
            raw_query, [status](int document_id, DocumentStatus document_status, int rating) {
                return document_status == status;
            });
}

// Поиск документов по запросу
std::vector<Document> SearchServer::FindTopDocuments(const std::string &raw_query) const {
    return FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
}

// Метод получения частот слов по id документа.
const std::map<std::string_view, double> &SearchServer::GetWordFrequencies(int document_id) const {
    static const std::map<std::string_view, double> empty_map;
    if (document_id < 0 || documents_.count(document_id) == 0) {
        return empty_map;
    }
    return document_to_word_freqs_.at(document_id);
}

// Получение кол-ва документов.
int SearchServer::GetDocumentCount() const {
    return documents_.size();
}

// Получить кортеж из слов и статуса документа по запросу.
std::tuple<std::vector<std::string_view>, DocumentStatus>
SearchServer::MatchDocument(const std::string_view raw_query, int document_id) const {
    return MatchDocument(std::execution::seq, raw_query, document_id);
}

std::tuple<std::vector<std::string_view>, DocumentStatus>
SearchServer::MatchDocument(const std::execution::sequenced_policy&,
                            const std::string_view raw_query, int document_id) const {
    const Query query = ParseQuery(raw_query);
    const auto status = documents_.at(document_id).status;

    for (const std::string_view word : query.minus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        if (word_to_document_freqs_.at(word).count(document_id) > 0) {
            return {std::vector<std::string_view>(), status};
        }
    }

    std::vector<std::string_view> matched_words;
    for (const std::string_view word : query.plus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        if (word_to_document_freqs_.at(word).count(document_id) > 0) {
            matched_words.push_back(word);
        }
    }

    return {matched_words, status};
}

std::tuple<std::vector<std::string_view>, DocumentStatus>
SearchServer::MatchDocument(const std::execution::parallel_policy&,
                            std::string_view raw_query, int document_id) const {

    const auto query = ParseQuery(raw_query, false);
    const auto status = documents_.at(document_id).status;
    const auto word_checker =
            [this, document_id](std::string_view word) {
                const auto it = word_to_document_freqs_.find(word);
                return it != word_to_document_freqs_.end() && it->second.count(document_id);
            };

    if (any_of(std::execution::par, query.minus_words.begin(), query.minus_words.end(), word_checker)) {
        return {std::vector<std::string_view>(), status};
    }

    std::vector<std::string_view> matched_words(query.plus_words.size());
    auto words_end = copy_if(
            std::execution::par,
            query.plus_words.begin(), query.plus_words.end(),
            matched_words.begin(),
            word_checker
    );

    sort(matched_words.begin(), words_end);
    words_end = unique(matched_words.begin(), words_end);
    matched_words.erase(words_end, matched_words.end());

    return {matched_words, status};
}

// возвращает кортеж из общих слов и статуса документа по запросу
bool SearchServer::IsStopWord(const std::string_view word) const {
    return stop_words_.count(std::string(word)) > 0;
}

bool SearchServer::IsValidWord(const std::string_view word) {
    return std::none_of(word.begin(), word.end(), [](char c) {
        return c >= '\0' && c < ' ';
    });
}

std::vector<std::string_view> SearchServer::SplitIntoWordsNoStop(const std::string_view &text) const {
    std::vector<std::string_view> words;
    for (const std::string_view &word: SplitIntoWords(text)) {
        if (!IsValidWord(word)) {
            throw std::invalid_argument("Special character detected");
        }
        if (!IsStopWord(word)) {
            words.push_back(word);
        }
    }
    return words;
}

int SearchServer::ComputeAverageRating(const std::vector<int> &ratings) {
    if (ratings.empty()) {
        return 0;
    }
    int rating_sum = 0;
    for (const int rating: ratings) {
        rating_sum += rating;
    }
    return rating_sum / static_cast<int>(ratings.size());
}

SearchServer::QueryWord SearchServer::ParseQueryWord(const std::string_view &text) const {
    if (text.empty()) {
        throw std::invalid_argument("Query word is empty");
    }
    std::string_view word = text;
    bool is_minus = false;
    if (word[0] == '-') {
        is_minus = true;
        word = word.substr(1);
    }
    if (word.empty() || word[0] == '-' || !IsValidWord(word)) {
        throw std::invalid_argument("Query word is invalid");
    }

    return {word, is_minus, IsStopWord(word)};
}

SearchServer::Query SearchServer::ParseQuery(const std::string_view &text,
                                             bool make_uniq) const {
    Query result;
    for (const std::string_view word: SplitIntoWords(text)) {
        const auto query_word = ParseQueryWord(word);
        if (!query_word.is_stop) {
            if (query_word.is_minus) {
                result.minus_words.push_back(query_word.data);
            } else {
                result.plus_words.push_back(query_word.data);
            }
        }
    }
    if (make_uniq) {
        // Удаление дубликатов из векторов "плюс" и "минус" слов
        for (auto *word : {&result.plus_words, &result.minus_words}) {
            std::sort(word->begin(), word->end());
            auto trash_pos = std::unique(word->begin(), word->end());
            word->erase(trash_pos, word->end());
        }
    }

    return result;
}

// Подсчитывает TF-IDF
double SearchServer::ComputeWordInverseDocumentFreq(const std::string_view &word) const {
    return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
}

// Выводит результаты в консоль
void PrintMatchDocumentResult(int document_id, const std::vector<std::string> &words, DocumentStatus status) {
    std::cout << "{ "
              << "document_id = " << document_id << ", "
              << "status = " << static_cast<int>(status) << ", "
              << "words =";
    for (std::string word: words) {
        std::cout << ' ' << word;
    }
    std::cout << "}" << std::endl;
}

// Выводит результаты в консоль
void PrintDocument(const Document &document) {
    std::cout << "{ "
              << "document_id = " << document.id << ", "
              << "relevance = " << document.relevance << ", "
              << "rating = " << document.rating
              << " }" << std::endl;
}