#include <iostream>
#include <vector>

using namespace std;
int n, k;
int maxValue, temp;

int main()
{
    cin >> n >> k;
    vector<int> vec(n);
    vector<int> vecSum(n);
    for (int i = 0; i < n; i++)
    {
        cin >> vec[i];
        if (i < k)
        {
            maxValue += vec[i];
            vecSum[i] = maxValue;
            continue;
        }

        vecSum[i] = vecSum[i - 1] + vec[i] - vec[i - k];
        maxValue = std::max(maxValue, vecSum[i]);
    }

    cout << maxValue;
}