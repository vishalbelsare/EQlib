#include <pybind11/pybind11.h>
#include <pybind11/eigen.h>
#include <pybind11/functional.h>
#include <pybind11/numpy.h>
#include <pybind11/operators.h>
#include <pybind11/stl.h>
#include <pybind11/stl_bind.h>

#include <EQLib/Dof.h>
#include <EQLib/Parameter.h>

PYBIND11_MODULE(EQLib, m) {
    m.doc() = "EQLib by Thomas Oberbichler";
    m.attr("__author__") = "Thomas Oberbichler";
    m.attr("__copyright__") = "Copyright (c) 2019, Thomas Oberbichler";
    m.attr("__version__") = EQLIB_VERSION;
    m.attr("__email__") = "thomas.oberbichler@gmail.com";
    m.attr("__status__") = "Development";

    namespace py = pybind11;
    using namespace pybind11::literals;

#if defined(EIGEN_USE_BLAS)
    m.attr("USE_BLAS") = true;
#else
    m.attr("USE_BLAS") = false;
#endif // EIGEN_USE_BLAS

    { // Dof
    using Type = EQLib::Dof;

    py::class_<Type>(m, "Dof")
        .def_property("delta", &Type::delta, &Type::set_delta)
        .def_property("residual", &Type::residual, &Type::set_residual)
        .def("__eq__", &Type::operator==)
        .def("__hash__", &Type::hash)
    ;
    }

    { // Parameter
    using Type = EQLib::Parameter;

    py::class_<Type>(m, "Parameter")
        .def(py::init<double, double, double, double, bool>(), "ref_value"_a,
            "act_value"_a, "target"_a, "result"_a, "isfixed"_a)
        .def(py::init<double, double>(), "value"_a, "target"_a)
        .def_property("ref_value", &Type::ref_value, &Type::set_ref_value)
        .def_property("act_value", &Type::act_value, &Type::set_act_value)
        .def_property("target", &Type::target, &Type::set_target)
        .def_property("result", &Type::result, &Type::set_result)
        .def_property("isfixed", &Type::isfixed, &Type::set_isfixed)
        .def_property_readonly("dof", &Type::dof)
        .def(py::pickle([](const Type& self) {
                return py::make_tuple(self.ref_value(), self.act_value(),
                    self.target(), self.result(), self.isfixed());
            }, [](py::tuple tuple) {
                if (tuple.size() != 5) {
                    throw std::runtime_error("Invalid state!");
                }
                
                const auto ref_value = tuple[0].cast<double>();
                const auto act_value = tuple[1].cast<double>();
                const auto target = tuple[2].cast<double>();
                const auto result = tuple[3].cast<double>();
                const auto isfixed = tuple[4].cast<bool>();

                return Type(ref_value, act_value, target, result, isfixed);
            }
        ))
        .def("__copy__", [](const Type& self) { return Type(self); })
        .def("__deepcopy__", [](const Type& self, py::dict& memo) {
            return Type(self); }, "memodict"_a)
    ;
    }
}