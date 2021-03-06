#pragma once

#include "Define.h"

#include <functional>
#include <set>
#include <vector>

namespace eqlib {

template <typename TScalar = double, typename TIndex = int, bool TRowMajor = false, bool TIndexMap = true>
class SparseStructure {
private: // types
    using Type = SparseStructure<TScalar, TIndex, TRowMajor, TIndexMap>;

private: // variables
    TIndex m_rows;
    TIndex m_cols;
    std::vector<TIndex> m_ia;
    std::vector<TIndex> m_ja;
    std::vector<DenseMap<TIndex, index>> m_indices;

public: // constructors
    SparseStructure()
    {
    }

    SparseStructure(const TIndex rows, const TIndex cols, const std::vector<TIndex>& ia, const std::vector<TIndex>& ja)
        : m_rows(rows)
        , m_cols(cols)
        , m_ia(ia)
        , m_ja(ja)
    {
        const TIndex size_i = TRowMajor ? rows : cols;
        const TIndex size_j = TRowMajor ? cols : rows;

        if (length(ia) != size_i + 1) {
            throw std::invalid_argument("Vector ia has an invalid size");
        }

        if (ja.size() > 0) {
            const TIndex max_j = Map<const Eigen::Matrix<TIndex, 1, Eigen::Dynamic>>(ja.data(), ja.size()).maxCoeff();

            if (max_j >= size_j) {
                throw std::invalid_argument("Vector ja has invalid entries");
            }
        }

        if (TIndexMap) {
            m_indices.resize(size_i);

            for (TIndex i = 0; i < size_i; i++) {
                m_indices[i].set_empty_key(-1);
                m_indices[i].resize(m_ia[i + 1] - m_ia[i]);
                for (TIndex k = m_ia[i]; k < m_ia[i + 1]; k++) {
                    const TIndex j = m_ja[k];
                    m_indices[i][j] = k;
                }
            }
        }
    }

public: // methods
    TIndex rows() const noexcept
    {
        return m_rows;
    }

    TIndex cols() const noexcept
    {
        return m_cols;
    }

    TIndex nb_nonzeros() const noexcept
    {
        return m_ia.back();
    }

    double density() const noexcept
    {
        if (rows() == 0 || cols() == 0) {
            return 0;
        }

        return (double)nb_nonzeros() / rows() / cols();
    }

    const std::vector<TIndex>& ia() const noexcept
    {
        return m_ia;
    }

    const TIndex& ia(TIndex index) const
    {
        return m_ia.at(index);
    }

    const std::vector<TIndex>& ja() const noexcept
    {
        return m_ja;
    }

    const TIndex& ja(TIndex index) const
    {
        return m_ja.at(index);
    }

    EQLIB_INLINE index get_first_index(const index i) const
    {
        return m_ia[i];
    }

    index get_index_bounded(const index j, const index lo, const index hi) const
    {
        const auto ja_begin = m_ja.begin();

        const auto lower = std::next(ja_begin, lo);
        const auto upper = std::next(ja_begin, hi);

        const auto it = std::lower_bound(lower, upper, j);

        if (*it != j || it == upper) {
            return -1;
        }

        const index value_index = std::distance(ja_begin, it);

        assert(value_index < nb_nonzeros());

        return value_index;
    }

    index get_index(const index row, const index col) const
    {
        assert(0 <= row && row < rows());
        assert(0 <= col && col < cols());

        const auto i = static_cast<TIndex>(TRowMajor ? row : col);
        const auto j = static_cast<TIndex>(TRowMajor ? col : row);

        if (TIndexMap) {
            const auto it = m_indices[i].find(j);

            if (it == m_indices[i].end()) {
                return -1;
            }

            return it->second;
        } else {
            const auto ja_begin = m_ja.begin();

            const auto lower = std::next(ja_begin, m_ia[i]);
            const auto upper = std::next(ja_begin, m_ia[i + 1]);

            const auto it = std::lower_bound(lower, upper, j);

            if (*it != j || it == upper) {
                return -1;
            }

            const index value_index = std::distance(ja_begin, it);

            assert(value_index < nb_nonzeros());

            return value_index;
        }
    }

    template <typename TPattern>
    static Type from_pattern(const index rows, const index cols, const TPattern& pattern)
    {
        const index size_i = TRowMajor ? rows : cols;
        const index size_j = TRowMajor ? cols : rows;

        assert(length(pattern) == size_i);

        std::vector<TIndex> ia(size_i + 1);

        ia[0] = 0;

        for (TIndex i = 0; i < size_i; i++) {
            const TIndex n = static_cast<TIndex>(pattern[i].size());

            ia[i + 1] = ia[i] + n;
        }

        const auto nb_nonzeros = ia.back();

        std::vector<TIndex> ja(nb_nonzeros);

        auto ja_it = ja.begin();

        for (TIndex i = 0; i < size_i; i++) {
            const TIndex n = static_cast<TIndex>(pattern[i].size());

            for (const auto j : pattern[i]) {
                assert(j < size_j);
                *ja_it++ = static_cast<TIndex>(j);
            }
        }

        return Type(static_cast<TIndex>(rows), static_cast<TIndex>(cols), ia, ja);
    }

    std::pair<SparseStructure, std::vector<index>> to_general() const
    {
        assert(m_rows == m_cols);

        const index n = m_rows;

        std::vector<std::vector<TIndex>> pattern(n);
        std::vector<std::vector<TIndex>> indices(n);

        for (TIndex i = 0; i < n; i++) {
            for (TIndex k = m_ia[i]; k < m_ia[i + 1]; k++) {
                const TIndex j = m_ja[k];

                pattern[i].push_back(j);
                indices[i].push_back(k);

                if (i == j) {
                    continue;
                }

                pattern[j].push_back(i);
                indices[j].push_back(k);
            }
        }

        SparseStructure result = from_pattern(m_rows, m_cols, pattern);

        std::vector<index> value_indices;
        value_indices.reserve(nb_nonzeros() * 2 - n);

        for (const auto& indices_row : indices) {
            value_indices.insert(value_indices.end(), indices_row.begin(), indices_row.end());
        }

        return {result, value_indices};
    }

    std::pair<SparseStructure, Vector> to_general(Ref<const Vector> values) const
    {
        assert(m_rows == m_cols);

        const index n = m_rows;

        std::vector<std::vector<TIndex>> pattern(n);
        std::vector<std::vector<TIndex>> indices(n);

        for (TIndex i = 0; i < n; i++) {
            for (TIndex k = m_ia[i]; k < m_ia[i + 1]; k++) {
                const TIndex j = m_ja[k];

                pattern[i].push_back(j);
                indices[i].push_back(k);

                if (i == j) {
                    continue;
                }

                pattern[j].push_back(i);
                indices[j].push_back(k);
            }
        }

        SparseStructure result = from_pattern(m_rows, m_cols, pattern);

        Vector new_values(nb_nonzeros() * 2 - n);
        index i = 0;

        for (const auto& indices_row : indices) {
            for (const auto& index : indices_row) {
                new_values(i++) = values(index);
            }
        }

        return {result, new_values};
    }

    /*
    * https://github.com/scipy/scipy/blob/3b36a574dc657d1ca116f6e230be694f3de31afc/scipy/sparse/sparsetools/csr.h#L380
    */
    static Type convert_from(SparseStructure<TScalar, TIndex, !TRowMajor> other, Ref<Vector> values)
    {
        const auto nb_nonzeros = other.nb_nonzeros();

        const auto n = TRowMajor ? other.cols() : other.rows();
        const auto m = TRowMajor ? other.rows() : other.cols();

        std::vector<TIndex> ia(m + 1);
        std::vector<TIndex> ja(nb_nonzeros);

        std::fill(ia.begin(), ia.end(), 0);

        for (TIndex k = 0; k < nb_nonzeros; k++) {
            ia[other.ja(k)] += 1;
        }

        TIndex cumsum = 0;

        for (TIndex j = 0; j < m; j++) {
            const auto temp = ia[j];
            ia[j] = cumsum;
            cumsum += temp;
        }

        ia[m] = nb_nonzeros;

        Vector a_values = values;

        for (TIndex i = 0; i < n; i++) {
            for (TIndex k = other.ia(i); k < other.ia(i + 1); k++) {
                const auto j = other.ja(k);
                const auto dest = ia[j];

                ja[dest] = i;
                values[dest] = a_values[k];

                ia[j]++;
            }
        }

        TIndex last = 0;

        for (TIndex j = 0; j <= m; j++) {
            const auto temp = ia[j];
            ia[j] = last;
            last = temp;
        }

        return Type(other.rows(), other.cols(), ia, ja);
    }

    void for_each(std::function<void(TIndex, TIndex, TIndex)> action) const
    {
        const TIndex size_i = TRowMajor ? m_rows : m_cols;

        TIndex idx = 0;

        for (TIndex i = 0; i < size_i; i++) {
            for (TIndex k = m_ia[i]; k < m_ia[i + 1]; k++) {
                const TIndex j = m_ja[k];
                if constexpr(TRowMajor) {
                    action(i, j, idx++);
                } else {
                    action(j, i, idx++);
                }
            }
        }
    }

public: // python
    template <typename TModule>
    static void register_python(TModule& m, const std::string& name)
    {
        namespace py = pybind11;
        using namespace pybind11::literals;

        using Holder = Pointer<Type>;

        py::class_<Type, Holder>(m, name.c_str())
            // constructors
            .def(py::init<TIndex, TIndex, std::vector<TIndex>, std::vector<TIndex>>(), "rows"_a, "cols"_a, "ia"_a, "ja"_a)
            // static methods
            .def_static("convert_from", &Type::convert_from, "other"_a, "values"_a)
            .def_static("from_pattern", &Type::from_pattern<std::vector<std::set<TIndex>>>, "rows"_a, "cols"_a, "pattern"_a)
            // methods
            .def("to_general", py::overload_cast<>(&Type::to_general, py::const_))
            .def("to_general", py::overload_cast<Ref<const Vector>>(&Type::to_general, py::const_))
            .def("get_index", &Type::get_index, "i"_a, "j"_a)
            .def("for_each", &Type::for_each, "action"_a)
            // read-only properties
            .def_property_readonly("rows", &Type::rows)
            .def_property_readonly("cols", &Type::cols)
            .def_property_readonly("nb_nonzeros", &Type::nb_nonzeros)
            .def_property_readonly("density", &Type::density)
            .def_property_readonly("ia", py::overload_cast<>(&Type::ia, py::const_))
            .def_property_readonly("ja", py::overload_cast<>(&Type::ja, py::const_));
    }
};

} // namespace eqlib