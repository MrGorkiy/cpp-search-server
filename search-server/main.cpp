// Решите загадку: Сколько чисел от 1 до 1000 содержат как минимум одну цифру 3?
// Напишите ответ здесь:
#include <iostream>
#include <string>
 
using namespace std;
 
void CountingIsThree(int s) {
    int count = 0;
    for (int i = 0; i <= s; ++i) {
        for (char n: to_string(i)) {
            if (n == '3') {
                ++count;
            }
        }
    }
    cout << count << endl;
}

int main() {
    CountingIsThree(1000);
}
// Закомитьте изменения и отправьте их в свой репозиторий.