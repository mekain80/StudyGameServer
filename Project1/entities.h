#pragma once

struct Player {
    int hp;
    int x, y;
    bool isVisible = false;
    int movedTick = 0;
    int atkedTick = 0;
    int damagedTick = 0;
};

struct Enemy {
    int hp;
    int x, y;
    int type;
    bool isVisible = false;
    int moveSpeed = 0;
    int atkSpeed = 0;
    int currMovePattern = 0;
    int movedTick = 0;
    int atkedTick = 0;
    char shape;
};

struct Bullet {
    int x, y;
    bool isEnemy;
    bool isVisible = false;
    int speed;
    int movedTick = 0;
};