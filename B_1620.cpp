#include <iostream>
#include <unordered_map>
#include <vector>
#include <string>

using namespace std;

int N, M;
int main()
{
    ios_base::sync_with_stdio(0);
    cin.tie(0); cout.tie(0);
    
    cin >> N >> M;
    cin.ignore();
    vector<string> encyclopedia(N + 1);
    unordered_map<string, int> reversEncyclopedia(N);

    for (int i = 1; i <= N; i++)
    {
        getline(cin, encyclopedia[i]);
        reversEncyclopedia[encyclopedia[i]] = i;
    }

    string question;
    for (int i = 0; i < M; i++)
    {
        bool isNum = true;
        getline(cin, question);

        for (char ch : question)
        {
            if (isdigit(ch) == false)
            {
                isNum = false;
                break;
            }
        }

        if (isNum)
        {
            int ans = stoi(question);
            cout << encyclopedia[ans] << '\n';
        }
        else
        {
            cout << reversEncyclopedia[question] << '\n';
        }
    }
}