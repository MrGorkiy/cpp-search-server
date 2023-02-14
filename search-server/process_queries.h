#pragma once

#include <algorithm>
#include <execution>
#include <list>
#include <iterator>

#include "search_server.h"

std::vector<std::vector<Document>> ProcessQueries(
        const SearchServer& search_server,
        const std::vector<std::string>& queries);

std::list<Document> ProcessQueriesJoined(
        const SearchServer& search_server,
        const std::vector<std::string>& queries);