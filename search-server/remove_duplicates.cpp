#include "remove_duplicates.h"

#include <iostream>

void RemoveDuplicates(SearchServer &search_server) {
    std::map<std::set<std::string>, int> word_to_document_freqs; // Множество слов и ID документа
    std::set<int> id_delete_doc; // Множество ID дубликатов для удаления

    for (const int document_id: search_server) {
        std::set<std::string> words; // Множество слов
        for (auto &[doc, id_and_idf]: search_server.GetWordFrequencies(document_id)) {
            words.insert(doc);
        }
        if (word_to_document_freqs.count(words) == 0) {
            word_to_document_freqs[words] = document_id;
        } else if (document_id > word_to_document_freqs[words]) {
            id_delete_doc.insert(document_id);
        }
    }
    for (auto document: id_delete_doc) {
        std::cout << "Found duplicate document id " << document << std::endl;
        search_server.RemoveDocument(document);
    }
}