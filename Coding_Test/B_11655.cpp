#include <iostream>
#include <string>
#include <algorithm>
using namespace std;
string str;
string ROT13(string s)
{
    for (char& ch : s)
    {
        if ('a' <= ch && ch <= 'z')
        {
            ch = static_cast<char>('a' + (ch - 'a' + 13) % 26);
        }
        else if ('A' <= ch && ch <= 'Z')
        {
            ch = static_cast<char>('A' + (ch - 'A' + 13) % 26);
        }
    }
    return s;
}
int main()
{
    getline(cin, str);
    cout << ROT13(str);
}