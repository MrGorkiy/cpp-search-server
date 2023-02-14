#include "search_server.h"

#include <cmath>

SearchServer::SearchServer(const std::string &stop_words_text)
        : SearchServer::SearchServer(SplitIntoWords(stop_words_text)) {
}

std::set<int>::iterator SearchServer::begin() const {
    return document_ids_.begin();
}

std::set<int>::iterator SearchServer::end() const {
    return document_ids_.end();
}

// Добавление нового документа.
void SearchServer::AddDocument(int document_id, const std::string &document, DocumentStatus status,
                               const std::vector<int> &ratings) {
    if ((document_id < 0) || (documents_.count(document_id) > 0)) {
        throw std::invalid_argument("Invalid document_id");
    }
    const auto words = SplitIntoWordsNoStop(document);

    const double inv_word_count = 1.0 / words.size();
    for (const std::string &word: words) {
        word_to_document_freqs_[word][document_id] += inv_word_count;
        document_to_word_freqs_[document_id][word] += inv_word_count;
    }
    documents_.emplace(document_id, DocumentData{ComputeAverageRating(ratings), status});
    document_ids_.insert(document_id);
}

// Удаление документа по ID.
void SearchServer::RemoveDocument(int document_id) {
    if (document_ids_.count(document_id) == 0) {
        return;
    }
    std::map<std::string, double> words_doc = document_to_word_freqs_.at(document_id);
    for (auto &[word, idf]: words_doc) {
        word_to_document_freqs_.at(word).erase(document_id);
    }
    documents_.erase(document_id);
    document_ids_.erase(document_id);
    document_to_word_freqs_.erase(document_id);
}

void SearchServer::RemoveDocument(const std::execution::parallel_policy &, int document_id) {
    if (document_ids_.count(document_id) == 0) {
        return;
    }

    const auto &it = document_to_word_freqs_.at(document_id);
    std::vector<std::string> words(it.size());

    std::transform(std::execution::par, it.begin(), it.end(), words.begin(), [](const auto &element) {
        return element.first;
    });

    std::for_each(std::execution::par, words.begin(), words.end(), [&](const std::string word) {
        word_to_document_freqs_.at(word).erase(document_id);
        return word;
    });

    document_to_word_freqs_.erase(document_id);
    document_ids_.erase(document_id);
    documents_.erase(document_id);

}

void SearchServer::RemoveDocument(const std::execution::sequenced_policy &, int document_id) {
    SearchServer::RemoveDocument(document_id);
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
const std::map<std::string, double> &SearchServer::GetWordFrequencies(int document_id) const {
    static const std::map<std::string, double> empty_map;
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
std::tuple<std::vector<std::string>, DocumentStatus> SearchServer::MatchDocument(const std::string &raw_query,
                                                                                 int document_id) const {
    const auto query = ParseQuery(raw_query);

    std::vector<std::string> matched_words;
    for (const std::string &word: query.plus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        if (word_to_document_freqs_.at(word).count(document_id)) {
            matched_words.push_back(word);
        }
    }
    for (const std::string &word: query.minus_words) {
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

// возвращает кортеж из общих слов и статуса документа по запросу
bool SearchServer::IsStopWord(const std::string &word) const {
    return stop_words_.count(word) > 0;
}

bool SearchServer::IsValidWord(const std::string &word) {
    return none_of(word.begin(), word.end(), [](char c) {
        return c >= '\0' && c < ' ';
    });
}

std::vector<std::string> SearchServer::SplitIntoWordsNoStop(const std::string &text) const {
    std::vector<std::string> words;
    for (const std::string &word: SplitIntoWords(text)) {
        if (!IsValidWord(word)) {
            throw std::invalid_argument("Word " + word + " is invalid");
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

SearchServer::QueryWord SearchServer::ParseQueryWord(const std::string &text) const {
    if (text.empty()) {
        throw std::invalid_argument("Query word is empty");
    }
    std::string word = text;
    bool is_minus = false;
    if (word[0] == '-') {
        is_minus = true;
        word = word.substr(1);
    }
    if (word.empty() || word[0] == '-' || !IsValidWord(word)) {
        throw std::invalid_argument("Query word " + text + " is invalid");
    }

    return {word, is_minus, IsStopWord(word)};
}

SearchServer::Query SearchServer::ParseQuery(const std::string &text) const {
    Query result;
    for (const std::string &word: SplitIntoWords(text)) {
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

// Подсчитывает TF-IDF
double SearchServer::ComputeWordInverseDocumentFreq(const std::string &word) const {
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