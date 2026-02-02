#include <iostream>
#include <string>
#include <algorithm>
using namespace std;
int main() 
{
    string str;
    getline(cin, str);
    string str2{str};
    reverse(str.begin(), str.end());
    cout << (int)(str == str2);
    return 0;
}