#include <iostream>
#include <queue>
#include <string>
#include <tuple>
using namespace std;

int Y, X;
int mp[101][101];
int visited[101][101];
int dx[] = {0, 0, 1, -1};
int dy[] = {1, -1, 0, 0};

int PathFind()
{
    queue<tuple<int, int, int>> que; // (y, x, dist)
    que.emplace(0, 0, 1);
    visited[0][0] = 1;

    while (!que.empty())
    {
        auto [y, x, dist] = que.front();
        que.pop();

        if (y == Y - 1 && x == X - 1)
            return dist;

        for (int i = 0; i < 4; i++)
        {
            int ny = y + dy[i];
            int nx = x + dx[i];

            if (ny < 0 || ny >= Y || nx < 0 || nx >= X) continue;
            if (visited[ny][nx]) continue;
            if (mp[ny][nx] == 0) continue;

            visited[ny][nx] = 1;
            que.emplace(ny, nx, dist + 1);
        }
    }
    return -1;
}

int main()
{
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    cin >> Y >> X;

    for (int i = 0; i < Y; i++)
    {
        string str;
        cin >> str;
        for (int k = 0; k < X; k++)
        {
            mp[i][k] = (str[k] == '1') ? 1 : 0;
        }
    }

    cout << PathFind() << '\n';
    return 0;
}