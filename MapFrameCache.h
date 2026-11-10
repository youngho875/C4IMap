#pragma once
#include <array>
// Cached composition checkpoints preserve GDI/alpha drawing exactly. A change
// in a lower group invalidates all later checkpoints, but never earlier ones.
#pragma pack(push, 8)
class CMapFrameCache
{
private:
    struct Surface
    {
    public:
        CDC dc;
        CBitmap bitmap;
        HGDIOBJ original = nullptr;
        bool valid = false;
        void Clear();
        ~Surface();
    } surfaces[4];

    int width = 0, height = 0;
    std::array<double, 12> key = {};
    bool keyed = false;

public:
    enum Group
    {
        Elevation = 0,
        Background = 1,
        Sources = 2,
        Shapes = 3
    };
    unsigned long long renders[4] = {}, hits[4] = {};
    void InvalidateFrom(int first = 0);
    void Prepare(int w, int h, const std::array<double, 12> &next);
    bool Restore(int group, CDC *target);
    void Store(int group, CDC *source);
};
#pragma pack(pop)
