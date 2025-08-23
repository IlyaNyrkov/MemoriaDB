//
// Created by Ilya Nyrkov on 22.08.25.
//

#ifndef PREDICATE_H
#define PREDICATE_H

#include <functional>

namespace memoria {
    class Row;

    using Predicate = std::function<bool(const Row&)>;

}


#endif //PREDICATE_H
