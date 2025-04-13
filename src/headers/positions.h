#ifndef POSITIONS_H
#define POSITIONS_H

#define DIR_NUM 8

const int Positions [DIR_NUM][2] = {
    { 0, -1 }, // Up
    { 1, -1 }, // Up-right
    { 1,  0 }, // Right
    { 1,  1 }, // Down-right
    { 0,  1 }, // Down
    {-1,  1 }, // Down-left
    {-1,  0 }, // Left
    {-1, -1 }  // Up-left
};

#endif