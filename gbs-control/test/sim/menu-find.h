// Test helper: find a tree node by its label path ("Input/Sync cleanup"); ref < 0 = first match
#pragma once
#include <string>
#include "menu-tree.h"
#include "menu-strings.h"

// A label may contain '/' ("Clock / ADC"): a child matches when its label is the path or a prefix ending at a '/'
inline bool menuFindIn(const MenuNode* kids, uint8_t count, const std::string& path, int ref, MenuNode& out) {
    for (uint8_t i = 0; i < count; i++) {
        MenuNode n; menuNodeRead(&kids[i], n);
        char l[32]; menuStrCopy(n.label, l, sizeof(l));
        std::string label(l);
        if (path == label) {
            if (ref >= 0 && n.ref != ref) continue;
            out = n;
            return true;
        }
        if (path.compare(0, label.size(), label) != 0 || path.size() <= label.size() || path[label.size()] != '/') continue;
        if (n.type == N_SUB && !(n.flags & NF_FN) && menuFindIn(n.children, n.count, path.substr(label.size() + 1), ref, out))
            return true;
    }
    return false;
}

inline MenuNode menuFind(const std::string& path, int ref = -1) {
    MenuNode root; menuNodeRead(menuRoot(), root);
    MenuNode out = {};
    menuFindIn(root.children, root.count, path, ref, out);
    return out;
}

// Child of the SUB at parentPath with exactly this label (labels may contain '/')
inline MenuNode menuFindChild(const std::string& parentPath, const std::string& label, int ref = -1) {
    MenuNode parent = menuFind(parentPath);
    MenuNode out = {};
    for (uint8_t i = 0; i < parent.count; i++) {
        MenuNode n; menuNodeRead(&parent.children[i], n);
        char l[32]; menuStrCopy(n.label, l, sizeof(l));
        if (label == l && (ref < 0 || n.ref == ref)) return n;
    }
    return out;
}
