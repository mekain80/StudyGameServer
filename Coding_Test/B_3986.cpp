#include <iostream>
#include <string>
#include <vector>
#include <stack>

using namespace std;

int cnt, ans;
int main()
{
    (cin >> cnt).ignore();
    vector<string> strs(cnt);
    for (size_t i = 0; i < cnt; i++)
    {
        getline(cin, strs[i]);
        bool isAOpen = false;
        bool isBOpen = false;
        stack<char> st;
        for (char ch : strs[i])
        {
            if (st.empty() == false && st.top() == ch)
            {
                st.pop();
            }
            else
            {
                st.push(ch);
            }
        }

        if (st.size() == 0)
        {
            ans++;
        }
    }

    cout << ans;
}