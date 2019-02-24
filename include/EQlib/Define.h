#pragma once

#include <Eigen/Core>
#include <Eigen/PardisoSupport>
#include <Eigen/Sparse>

#include <pybind11/pybind11.h>

using Vector3D = Eigen::Vector3d;

using Matrix = Eigen::Matrix<double, Eigen::Dynamic, Eigen::Dynamic>;
using Vector = Eigen::Matrix<double, Eigen::Dynamic, 1>;
using Sparse = Eigen::SparseMatrix<double, Eigen::ColMajor>;

template <typename T>
using Ref = Eigen::Ref<T>;

namespace py = pybind11;

namespace EQlib {

template <typename T>
T get_or_default(py::dict options, std::string key, T default_value) {
    if (!options.contains(key.c_str())) {
        return default_value;
    }
    return options[key.c_str()].cast<T>();
}

}