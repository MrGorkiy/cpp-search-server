#include "string_processing.h"

std::vector<std::string_view> SplitIntoWords(const std::string_view text) {
    std::vector<std::string_view> result;
    std::string_view word;
    unsigned long pos = 0;

    while (true) {
        unsigned long space_pos = text.find(' ', pos);

        if (space_pos == std::string::npos) {
            word = text.substr(pos);
            result.push_back(word);

            break;
        } else {
            word = text.substr(pos, space_pos - pos);
            pos = space_pos + 1;
            result.push_back(word);
        }
    }
    return result;
}