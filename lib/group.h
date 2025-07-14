#ifndef GROUP_H
#define GROUP_H

#include "mobject.h"
#include <vector>

// Groups multiple mobjects into a single one.
inline Mobject group_mobjects(const std::vector<Mobject>& mobjects) {
    if (mobjects.empty()) return {};

    Mobject group = mobjects[0];
    for (size_t i = 1; i < mobjects.size(); ++i) {
        group.path.addPath(mobjects[i].path);
    }
    return group;
}

#endif // GROUP_H
