#include <iostream>
#include <vector>
#include <algorithm>

using namespace std;

int main()
{
    int n, K;
    cin >> n >> K;

    vector<int> weight(n), value(n);

    for (int i = 0; i < n; i++)
    {
        cin >> weight[i] >> value[i];
    }

    vector<int> dp(K + 1, 0);

    for (int i = 0; i < n; i++)
    {
        for (int w = K; w >= weight[i]; w--)
        {
            dp[w] = max(dp[w], dp[w - weight[i]] + value[i]);
        }
    }

    cout << dp[K] << '\n';
}