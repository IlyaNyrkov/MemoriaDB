//
// Created by Ilya Nyrkov on 19.08.25.
//

#include "memoria/Row.h"

#include <utility>

memoria::Row::Row() = default;

memoria::Row::Row(std::vector<RowValue> data) {
    this->data_ = std::move(data);
}

memoria::Row::Row(std::initializer_list<RowValue> init) {
    this->data_ = std::move(std::vector(init));
}

const memoria::RowValue& memoria::Row::at(std::size_t i) const {
    return data_.at(i);
}

memoria::RowValue& memoria::Row::at(std::size_t i) {
    return data_.at(i);
}

std::size_t memoria::Row::size() const noexcept {
    return data_.size();
}
