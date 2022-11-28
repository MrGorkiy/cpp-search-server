#include "search_server.h"
#include "paginator.h"
#include "request_queue.h"
#include "remove_duplicates.h"
#include "log_duration.h"

#include <execution>
#include <iostream>
#include <random>
#include <string>
#include <vector>

int main() {
    std::vector<std::string> stop_words = {"and", "with"};
    SearchServer search_server(stop_words);
//    RequestQueue request_queue(search_server);
//    search_server.AddDocument(1, "curly cat curly tail", DocumentStatus::ACTUAL, {7, 2, 7});
//    search_server.AddDocument(2, "curly dog and fancy collar", DocumentStatus::ACTUAL, {1, 2, 3});
//    search_server.AddDocument(3, "big cat fancy collar ", DocumentStatus::ACTUAL, {1, 2, 8});
//    search_server.AddDocument(4, "big dog sparrow Eugene", DocumentStatus::ACTUAL, {1, 3, 2});
//    search_server.AddDocument(5, "big dog sparrow Vasiliy", DocumentStatus::ACTUAL, {1, 1, 1});
//    // 1439 запросов с нулевым результатом
//    {
//        for (int i = 0; i < 1439; ++i) {
//            request_queue.AddFindRequest("empty request");
//        }
//        // все еще 1439 запросов с нулевым результатом
//        request_queue.AddFindRequest("curly dog");
//        // новые сутки, первый запрос удален, 1438 запросов с нулевым результатом
//        request_queue.AddFindRequest("big collar");
//        // первый запрос удален, 1437 запросов с нулевым результатом
//        request_queue.AddFindRequest("sparrow");
//        std::cout << "Total empty requests: " << request_queue.GetNoResultRequests() << std::endl;
//    }
//    {
//        std::cout << "[------DOG------]" << std::endl;
//        search_server.RemoveDocument(5);
//        auto find = search_server.FindTopDocuments("dog Eugene");
//        for (auto& n: find) {
//            std::cout << n << std::endl;
//        }
//    }
//    {
//        std::cout << "[------CAT------]" << std::endl;
//        auto find = search_server.FindTopDocuments("cat fancy");
//        for (auto& n: find) {
//            std::cout << n << std::endl;
//        }
//    }
    {
        LOG_DURATION("server");
        search_server.AddDocument(1, "funny pet and nasty rat", DocumentStatus::ACTUAL, {7, 2, 7});
        search_server.AddDocument(2, "funny pet with curly hair", DocumentStatus::ACTUAL, {1, 2});

        // дубликат документа 2, будет удалён
        search_server.AddDocument(3, "funny pet with curly hair", DocumentStatus::ACTUAL, {1, 2});

        // отличие только в стоп-словах, считаем дубликатом
        search_server.AddDocument(4, "funny pet and curly hair", DocumentStatus::ACTUAL, {1, 2});

        // множество слов такое же, считаем дубликатом документа 1
        search_server.AddDocument(5, "funny funny pet and nasty nasty rat", DocumentStatus::ACTUAL, {1, 2});

        // добавились новые слова, дубликатом не является
        search_server.AddDocument(6, "funny pet and not very nasty rat", DocumentStatus::ACTUAL, {1, 2});

        // множество слов такое же, как в id 6, несмотря на другой порядок, считаем дубликатом
        search_server.AddDocument(7, "very nasty rat and not very funny pet", DocumentStatus::ACTUAL, {1, 2});

        // есть не все слова, не является дубликатом
        search_server.AddDocument(8, "pet with rat and rat and rat", DocumentStatus::ACTUAL, {1, 2});

        // слова из разных документов, не является дубликатом
        search_server.AddDocument(9, "nasty rat with curly hair", DocumentStatus::ACTUAL, {1, 2});

        std::cout << "Before duplicates removed: " << search_server.GetDocumentCount() << std::endl;
        RemoveDuplicates(search_server);
        std::cout << "After duplicates removed: " << search_server.GetDocumentCount() << std::endl;
    }
    return 0;
}