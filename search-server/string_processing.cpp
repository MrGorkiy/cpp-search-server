//
// Created by maksi on 17.11.2022.
//

#include "string_processing.h"

std::vector<std::string> SplitIntoWords(const std::string &text) {
    std::vector<std::string> words;
    std::string word;
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