#include <iostream>
#include <vector>
#include <cstring>
using namespace std;

int mp[50][50];
int T, N, M, K;
int dx[] = {0, 0, -1, 1};
int dy[] = {-1, 1, 0, 0};

void dfs(int y, int x)
{
    if (mp[y][x] == 0) return;
    mp[y][x] = 0;

    for (int i = 0; i < 4; i++)
    {
        int ny = y + dy[i];
        int nx = x + dx[i];
        if (ny < 0 || ny >= N || nx < 0 || nx >= M) continue;
        if (mp[ny][nx] == 1) dfs(ny, nx);
    }
}

int main()
{
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    cin >> T;
    while (T--)
    {
        int ans = 0;
        memset(mp, 0, sizeof(mp));

        cin >> M >> N >> K;
        vector<pair<int,int>> vec;
        for (int j = 0; j < K; j++)
        {
            int x, y;
            cin >> x >> y;
            vec.push_back({y, x});
            mp[y][x] = 1;
        }

        for (auto [y, x] : vec)
        {
            if (mp[y][x] == 1)
            {
                ans++;
                dfs(y, x);
            }
        }

        cout << ans << '\n';
    }
}