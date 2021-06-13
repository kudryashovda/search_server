#include "string_processing.h"

using namespace std;

vector<string_view> SplitIntoWords(string_view str) {

    vector<string_view> result;
    str.remove_prefix(0);

    const int64_t pos_end = str.npos;

    while (true) {
        int64_t space = str.find(' ');

        result.push_back(str.substr(0, space));

        if (space == pos_end) {
            break;
        } else {
            str.remove_prefix(space + 1);
        }
    }

    return result;
}