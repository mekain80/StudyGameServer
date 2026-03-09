#include <bits/stdc++.h>
using namespace std;

int N;
int ans = -1;

int main()
{
    cin >> N;
    vector<int> arr(N);
    for (size_t i = 0; i < N; i++)
    {
        cin >> arr[i];
    }
    sort(arr.begin(), arr.end());

    for (size_t i = N - 1; i >= 2; i--)
    {
        if (arr[i] < arr[i - 1] + arr[i - 2])
        {
            ans = arr[i] + arr[i - 1] + arr[i - 2];
            break;
        }    
    }
    cout << ans;
}