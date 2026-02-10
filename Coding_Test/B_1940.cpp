#include <iostream>

using namespace std;

int n, m, temp;
int arr[100'001];
int main()
{
    cin >> n >> m;
    for (int i = 0; i < n; i++)
    {
        cin >> temp;
        arr[temp] = 1;
    }

    int ans = 0;
    for (int i = 1; i <= 100'000; i++)
    {
        if (arr[i] == 1)
        {
            int j = m - i;
            if (1 <= j && j <= 100000 && arr[j] == 1)
                ans++;
        }
    }

    cout << ans / 2;
}