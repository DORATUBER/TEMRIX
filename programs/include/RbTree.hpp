#pragma once

#include <temrixstd.h>

#define RBTREE_NIL (-1)
#define RBTREE_BLACK 0
#define RBTREE_RED 1

struct RbLinks
{
    int32_t parent = RBTREE_NIL;
    int32_t left = RBTREE_NIL;
    int32_t right = RBTREE_NIL;
    uint8_t color = RBTREE_BLACK;
};

template <typename Policy>
class IntrusiveRbTree
{
public:
    explicit IntrusiveRbTree(Policy &policy) : policy(policy) {}

    void reset() { root = RBTREE_NIL; }
    bool empty() const { return root == RBTREE_NIL; }
    int32_t getRoot() const { return root; }

    void insert(int32_t z)
    {
        int32_t y = RBTREE_NIL;
        int32_t x = root;

        while (x != RBTREE_NIL)
        {
            y = x;
            x = policy.less(z, x) ? L(x).left : L(x).right;
        }

        L(z).parent = y;
        if (y == RBTREE_NIL)
            root = z;
        else if (policy.less(z, y))
            L(y).left = z;
        else
            L(y).right = z;

        L(z).left = RBTREE_NIL;
        L(z).right = RBTREE_NIL;
        L(z).color = RBTREE_RED;
        insertFixup(z);
    }

    void remove(int32_t z)
    {
        int32_t y = z;
        uint8_t yOrigColor = L(y).color;
        int32_t x, xParent;

        if (L(z).left == RBTREE_NIL)
        {
            x = L(z).right;
            xParent = L(z).parent;
            transplant(z, L(z).right);
        }
        else if (L(z).right == RBTREE_NIL)
        {
            x = L(z).left;
            xParent = L(z).parent;
            transplant(z, L(z).left);
        }
        else
        {
            y = minimum(L(z).right);
            yOrigColor = L(y).color;
            x = L(y).right;

            if (L(y).parent == z)
            {
                xParent = y;
            }
            else
            {
                xParent = L(y).parent;
                transplant(y, L(y).right);
                L(y).right = L(z).right;
                L(L(y).right).parent = y;
            }

            transplant(z, y);
            L(y).left = L(z).left;
            L(L(y).left).parent = y;
            L(y).color = L(z).color;
        }

        if (yOrigColor == RBTREE_BLACK)
            removeFixup(x, xParent);
    }

    int32_t minimum(int32_t x) const
    {
        while (L(x).left != RBTREE_NIL)
            x = L(x).left;
        return x;
    }
    int32_t maximum(int32_t x) const
    {
        while (L(x).right != RBTREE_NIL)
            x = L(x).right;
        return x;
    }

    int32_t treeMinimum() const { return root == RBTREE_NIL ? RBTREE_NIL : minimum(root); }
    int32_t treeMaximum() const { return root == RBTREE_NIL ? RBTREE_NIL : maximum(root); }

    template <typename Cmp>
    int32_t lowerBound(Cmp cmp) const
    {
        int32_t x = root, best = RBTREE_NIL;
        while (x != RBTREE_NIL)
        {
            if (!cmp(x))
            {
                best = x;
                x = L(x).left;
            }
            else
                x = L(x).right;
        }
        return best;
    }

    template <typename Cmp>
    int32_t lastWhere(Cmp cmp) const
    {
        int32_t x = root, best = RBTREE_NIL;
        while (x != RBTREE_NIL)
        {
            if (cmp(x))
            {
                best = x;
                x = L(x).right;
            }
            else
                x = L(x).left;
        }
        return best;
    }

    template <typename Visit>
    void inOrder(Visit &visit) const { inOrderFrom(root, visit); }

private:
    Policy &policy;
    int32_t root = RBTREE_NIL;

    RbLinks &L(int32_t idx) { return policy.links(idx); }
    const RbLinks &L(int32_t idx) const { return policy.links(idx); }

    uint8_t colorOf(int32_t x) const { return x == RBTREE_NIL ? RBTREE_BLACK : L(x).color; }

    template <typename Visit>
    void inOrderFrom(int32_t x, Visit &visit) const
    {
        if (x == RBTREE_NIL)
            return;
        inOrderFrom(L(x).left, visit);
        visit(x);
        inOrderFrom(L(x).right, visit);
    }

    void rotateLeft(int32_t x)
    {
        int32_t y = L(x).right;
        L(x).right = L(y).left;
        if (L(y).left != RBTREE_NIL)
            L(L(y).left).parent = x;
        L(y).parent = L(x).parent;
        if (L(x).parent == RBTREE_NIL)
            root = y;
        else if (x == L(L(x).parent).left)
            L(L(x).parent).left = y;
        else
            L(L(x).parent).right = y;
        L(y).left = x;
        L(x).parent = y;
    }

    void rotateRight(int32_t x)
    {
        int32_t y = L(x).left;
        L(x).left = L(y).right;
        if (L(y).right != RBTREE_NIL)
            L(L(y).right).parent = x;
        L(y).parent = L(x).parent;
        if (L(x).parent == RBTREE_NIL)
            root = y;
        else if (x == L(L(x).parent).right)
            L(L(x).parent).right = y;
        else
            L(L(x).parent).left = y;
        L(y).right = x;
        L(x).parent = y;
    }

    void insertFixup(int32_t z)
    {
        while (L(z).parent != RBTREE_NIL && colorOf(L(z).parent) == RBTREE_RED)
        {
            int32_t zp = L(z).parent;
            int32_t zpp = L(zp).parent;

            if (zp == L(zpp).left)
            {
                int32_t y = L(zpp).right;
                if (colorOf(y) == RBTREE_RED)
                {
                    L(zp).color = RBTREE_BLACK;
                    L(y).color = RBTREE_BLACK;
                    L(zpp).color = RBTREE_RED;
                    z = zpp;
                }
                else
                {
                    if (z == L(zp).right)
                    {
                        z = zp;
                        rotateLeft(z);
                        zp = L(z).parent;
                        zpp = L(zp).parent;
                    }
                    L(zp).color = RBTREE_BLACK;
                    L(zpp).color = RBTREE_RED;
                    rotateRight(zpp);
                }
            }
            else
            {
                int32_t y = L(zpp).left;
                if (colorOf(y) == RBTREE_RED)
                {
                    L(zp).color = RBTREE_BLACK;
                    L(y).color = RBTREE_BLACK;
                    L(zpp).color = RBTREE_RED;
                    z = zpp;
                }
                else
                {
                    if (z == L(zp).left)
                    {
                        z = zp;
                        rotateRight(z);
                        zp = L(z).parent;
                        zpp = L(zp).parent;
                    }
                    L(zp).color = RBTREE_BLACK;
                    L(zpp).color = RBTREE_RED;
                    rotateLeft(zpp);
                }
            }
        }
        L(root).color = RBTREE_BLACK;
    }

    void transplant(int32_t u, int32_t v)
    {
        int32_t up = L(u).parent;
        if (up == RBTREE_NIL)
            root = v;
        else if (u == L(up).left)
            L(up).left = v;
        else
            L(up).right = v;
        if (v != RBTREE_NIL)
            L(v).parent = up;
    }

    void removeFixup(int32_t x, int32_t xParent)
    {
        while (x != root && colorOf(x) == RBTREE_BLACK)
        {
            if (x == L(xParent).left)
            {
                int32_t w = L(xParent).right;
                if (colorOf(w) == RBTREE_RED)
                {
                    L(w).color = RBTREE_BLACK;
                    L(xParent).color = RBTREE_RED;
                    rotateLeft(xParent);
                    w = L(xParent).right;
                }
                if (colorOf(L(w).left) == RBTREE_BLACK && colorOf(L(w).right) == RBTREE_BLACK)
                {
                    L(w).color = RBTREE_RED;
                    x = xParent;
                    xParent = L(x).parent;
                }
                else
                {
                    if (colorOf(L(w).right) == RBTREE_BLACK)
                    {
                        if (L(w).left != RBTREE_NIL)
                            L(L(w).left).color = RBTREE_BLACK;
                        L(w).color = RBTREE_RED;
                        rotateRight(w);
                        w = L(xParent).right;
                    }
                    L(w).color = L(xParent).color;
                    L(xParent).color = RBTREE_BLACK;
                    if (L(w).right != RBTREE_NIL)
                        L(L(w).right).color = RBTREE_BLACK;
                    rotateLeft(xParent);
                    x = root;
                }
            }
            else
            {
                int32_t w = L(xParent).left;
                if (colorOf(w) == RBTREE_RED)
                {
                    L(w).color = RBTREE_BLACK;
                    L(xParent).color = RBTREE_RED;
                    rotateRight(xParent);
                    w = L(xParent).left;
                }
                if (colorOf(L(w).right) == RBTREE_BLACK && colorOf(L(w).left) == RBTREE_BLACK)
                {
                    L(w).color = RBTREE_RED;
                    x = xParent;
                    xParent = L(x).parent;
                }
                else
                {
                    if (colorOf(L(w).left) == RBTREE_BLACK)
                    {
                        if (L(w).right != RBTREE_NIL)
                            L(L(w).right).color = RBTREE_BLACK;
                        L(w).color = RBTREE_RED;
                        rotateLeft(w);
                        w = L(xParent).left;
                    }
                    L(w).color = L(xParent).color;
                    L(xParent).color = RBTREE_BLACK;
                    if (L(w).left != RBTREE_NIL)
                        L(L(w).left).color = RBTREE_BLACK;
                    rotateRight(xParent);
                    x = root;
                }
            }
        }
        if (x != RBTREE_NIL)
            L(x).color = RBTREE_BLACK;
    }
};