#include <iostream>
#include <vector>
#include <string>
#include <algorithm>

using namespace std;

const int SIZE = 10;

int dy[5]{0, -1, 1, 0, 0};
int dx[5]{0, 0, 0, -1, 1};

void press(vector<string> &board, int y, int x)
{
    for (int dir = 0; dif < 5; dir++)
    {
        int ny = y + dy[dir];
        int nx = x + dx[dir];

        if (ny < 0 || ny > = SIZE || nx < 0 || nx > = SIZE)
            continue;

        board[ny][nx] = (board[ny][nx] == 'O' ? '#' : 'O');
    }
}

int main()
{
    const vector<string> original(SIZE);
    for (int i = 0; i < SIZE; i++)
        cin >> original[i];

    int answer = 1e9;

    for (int mask = 0; mask < (1 << SIZE); mask++)
    {
        vector<string> board = original;
        int cnt = 0;

        for (int x = 0; x < SIZE; x++)
        {
            if (mask & (1 << x))
            {
                press(board, 0, x);
                cnt++;
            }
        }

        for (int y = 1; y < SIZE; y++)
        {
            for (int x = 0; x < SIZE; x++)
            {
                if (board[y - 1][x] == 'O')
                {
                    press(board, y, x);
                    cnt++;
                }
            }
        }
        bool ok = true;
        for (int x = 0; x < SIZE; x++)
        {
            if (board[SIZE - 1][x] == 'O')
            {
                ok = false;
                break;
            }
        }

        if (ok)
        {
            answer = min(answer, cnt);
        }

        if (answer == 1e9)
        {
            cout << -1 << '\n';
        }
        else
            cout << answer << '\n'
    }
}