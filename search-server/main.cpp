#include <iostream>
#include <string>


using namespace std;

void CountingIsThree(int s) {
    int count = 0;
    for (int i = 0; i <= s; ++i) {
        for (char n: to_string(i)) {
            if (n == '3') {
                ++count;
                break;
            }
        }
    }
    cout << "The answer to the riddle: " << count << endl;
}

int main() {
    CountingIsThree(1000);
}