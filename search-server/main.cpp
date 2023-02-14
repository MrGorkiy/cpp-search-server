#include "search_server.h"
#include "process_queries.h"
#include "paginator.h"
#include "request_queue.h"
#include "remove_duplicates.h"
#include "log_duration.h"

#include <execution>
#include <iostream>
#include <random>
#include <string>
#include <vector>

using namespace std::string_literals;

std::string GenerateWord(std::mt19937& generator, int max_length) {
    const int length = std::uniform_int_distribution(1, max_length)(generator);
    std::string word;
    word.reserve(length);
    for (int i = 0; i < length; ++i) {
        word.push_back(std::uniform_int_distribution('a', 'z')(generator));
    }
    return word;
}
std::vector<std::string> GenerateDictionary(std::mt19937& generator,
                                            int word_count, int max_length) {
    std::vector<std::string> words;
    words.reserve(word_count);
    for (int i = 0; i < word_count; ++i) {
        words.push_back(GenerateWord(generator, max_length));
    }
    sort(words.begin(), words.end());
    words.erase(unique(words.begin(), words.end()), words.end());
    return words;
}
std::string GenerateQuery(std::mt19937& generator,
                          const std::vector<std::string>& dictionary,
                          int max_word_count) {
    const int word_count = std::uniform_int_distribution(1, max_word_count)(generator);
    std::string query;
    for (int i = 0; i < word_count; ++i) {
        if (!query.empty()) {
            query.push_back(' ');
        }
        query += dictionary[std::uniform_int_distribution<int>(0, dictionary.size() - 1)(generator)];
    }
    return query;
}
std::vector<std::string> GenerateQueries(std::mt19937& generator,
                                         const std::vector<std::string>& dictionary,
                                         int query_count, int max_word_count) {
    std::vector<std::string> queries;
    queries.reserve(query_count);
    for (int i = 0; i < query_count; ++i) {
        queries.push_back(GenerateQuery(generator, dictionary, max_word_count));
    }
    return queries;
}
template <typename QueriesProcessor>
void Test(std::string_view mark,
          QueriesProcessor processor,
          const SearchServer& search_server,
          const std::vector<std::string>& queries) {
    LOG_DURATION(mark);
    const auto documents_lists = processor(search_server, queries);
}
#define TEST(processor) Test(#processor, processor, search_server, queries)


int main() {
    {
        std::mt19937 generator;
        const auto dictionary = GenerateDictionary(generator, 2'000, 25);
        const auto documents = GenerateQueries(generator, dictionary, 20'000, 10);
        SearchServer search_server(dictionary[0]);
        for (size_t i = 0; i < documents.size(); ++i) {
            search_server.AddDocument(i, documents[i], DocumentStatus::ACTUAL, {1, 2, 3});
        }
        const auto queries = GenerateQueries(generator, dictionary, 2'000, 7);
        TEST(ProcessQueries);
    }

    {
        SearchServer search_server("and with"s);
        int id = 0;
        for (
            const std::string &text: {
                "funny pet and nasty rat"s,
                "funny pet with curly hair"s,
                "funny pet and not very nasty rat"s,
                "pet with rat and rat and rat"s,
                "nasty rat with curly hair"s,
        }
                ) {
            search_server.AddDocument(++id, text, DocumentStatus::ACTUAL, {1, 2});
        }
        const std::vector<std::string> queries = {
                "nasty rat -not"s,
                "not very funny nasty pet"s,
                "curly hair"s
        };
        id = 0;
        for (
            const auto &documents: ProcessQueries(search_server, queries)
                ) {
            std::cout << documents.size() << " documents for query ["s << queries[id++] << "]"s << std::endl;
        }
    }

    {
        SearchServer search_server("and with"s);
        int id = 0;
        for (
            const std::string& text : {
                "funny pet and nasty rat"s,
                "funny pet with curly hair"s,
                "funny pet and not very nasty rat"s,
                "pet with rat and rat and rat"s,
                "nasty rat with curly hair"s,
        }
                ) {
            search_server.AddDocument(++id, text, DocumentStatus::ACTUAL, {1, 2});
        }
        const std::vector<std::string> queries = {
                "nasty rat -not"s,
                "not very funny nasty pet"s,
                "curly hair"s
        };
        for (const Document& document : ProcessQueriesJoined(search_server, queries)) {
            std::cout << "Document "s << document.id << " matched with relevance "s << document.relevance << std::endl;
        }
    }

    {
        SearchServer search_server("and with"s);

        int id = 0;
        for (
            const std::string& text : {
                "funny pet and nasty rat"s,
                "funny pet with curly hair"s,
                "funny pet and not very nasty rat"s,
                "pet with rat and rat and rat"s,
                "nasty rat with curly hair"s,
        }
                ) {
            search_server.AddDocument(++id, text, DocumentStatus::ACTUAL, {1, 2});
        }

        const std::string query = "curly and funny"s;

        auto report = [&search_server, &query] {
            std::cout << search_server.GetDocumentCount() << " documents total, "s
                 << search_server.FindTopDocuments(query).size() << " documents for query ["s << query << "]"s << std::endl;
        };

        report();
        // однопоточная версия
        search_server.RemoveDocument(5);
        report();
        // однопоточная версия
        search_server.RemoveDocument(std::execution::seq, 1);
        report();
        // многопоточная версия
        search_server.RemoveDocument(std::execution::par, 2);
        report();
    }

    {
        using namespace std;
        SearchServer search_server("and with"s);

        int id = 0;
        for (
            const string& text : {
                "funny pet and nasty rat"s,
                "funny pet with curly hair"s,
                "funny pet and not very nasty rat"s,
                "pet with rat and rat and rat"s,
                "nasty rat with curly hair"s,
        }
                ) {
            search_server.AddDocument(++id, text, DocumentStatus::ACTUAL, {1, 2});
        }

        const string query = "curly and funny -not"s;

        {
            const auto [words, status] = search_server.MatchDocument(query, 1);
            cout << words.size() << " words for document 1"s << endl;
            // 1 words for document 1
        }

        {
            const auto [words, status] = search_server.MatchDocument(execution::seq, query, 2);
            cout << words.size() << " words for document 2"s << endl;
            // 2 words for document 2
        }

        {
            const auto [words, status] = search_server.MatchDocument(execution::par, query, 3);
            cout << words.size() << " words for document 3"s << endl;
            // 0 words for document 3
        }
    }
}