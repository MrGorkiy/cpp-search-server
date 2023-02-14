#include "process_queries.h"

std::vector<std::vector<Document>> ProcessQueries(const SearchServer &search_server,
                                                  const std::vector<std::string> &queries) {
    std::vector<std::vector<Document>> result(queries.size());
    std::transform(std::execution::par, queries.begin(), queries.end(), result.begin(),
                   [&search_server](const std::string query) {
                       return search_server.FindTopDocuments(query);
                   });
    return result;
}

std::list<Document> ProcessQueriesJoined(const SearchServer& search_server,
                                         const std::vector<std::string>& queries) {
    std::list<Document> result;
    for (auto& documents : ProcessQueries(search_server, queries)) {
        result.insert(result.end(), std::make_move_iterator(documents.begin()), std::make_move_iterator(documents.end()));
    }
    return result;
}