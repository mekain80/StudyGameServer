#include <iostream>
#include <string>
using namespace std;

string str1, str2;

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    int cnt;
    cin >> cnt;
    cin.get();                 // 개행 소비

    getline(cin, str1);        // 패턴
    size_t star = str1.find('*');
    string pre = str1.substr(0, star);
    string suf = str1.substr(star + 1);  // suffix만 따로 보관

    int preSize = (int)pre.size();
    int sufSize = (int)suf.size();

    for (int i = 0; i < cnt; i++) {
        getline(cin, str2);

        // 총 길이 조건: 파일명이 prefix+suffix보다 짧으면 매칭 불가
        if ((int)str2.size() < preSize + sufSize) {
            cout << "NE\n";
            continue;
        }

        // 앞/뒤 매칭
        bool ok = (str2.substr(0, preSize) == pre) &&
            (str2.substr((int)str2.size() - sufSize, sufSize) == suf);

        cout << (ok ? "DA\n" : "NE\n");
    }
}
