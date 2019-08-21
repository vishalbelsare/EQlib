#pragma once

#include "System.h"

#include <unsupported/Eigen/LevenbergMarquardt>

#include <memory>

namespace EQlib {

class LevenbergMarquardt
{
private:    // members
    Pointer<System<true>> m_system;

    struct Functor
    {
        typedef double Scalar;
        typedef int Index;
        typedef Vector InputType;
        typedef Vector ValueType;
        using JacobianType = Sparse;
        typedef Eigen::SparseQR<JacobianType, Eigen::COLAMDOrdering<int>> QRSolver;

        enum {
            InputsAtCompileTime = Eigen::Dynamic,
            ValuesAtCompileTime = Eigen::Dynamic
        };

        Pointer<System<true>> m_system;

        Functor(Pointer<System<true>> system)
        : m_system(std::move(system))
        { }

        int operator()(const Vector &x, Vector &fvec) const
        {
            m_system->set_x(x);
            m_system->assemble<1>(true);
            fvec = m_system->g();
            Log::info(2, "The value is {}", m_system->f());
            return 0;
        }

        int df(const Vector &x, Sparse &fjac) const
        {
            m_system->set_x(x);
            m_system->assemble<2>(true);
            fjac = m_system->h();
            return 0;
        }

        int values() const { return m_system->nb_dofs(); }

        int inputs() const { return m_system->nb_dofs(); }
    };

private:    // methods
    double linesearch_armijo(Ref<const Vector> x, const Ref<Vector> search_dir,
        const double alpha_init = 1.0)
    {
        const double c = 0.2;
        const double rho = 0.9;
        double alpha = alpha_init;

        m_system->set_x(x + alpha * search_dir);
        m_system->assemble<0>(true);
        double f = m_system->f();

        m_system->set_x(x);
        m_system->assemble<1>(true);
        const double f_in = m_system->f();
        const Vector grad = m_system->g();

        const double cache = c * grad.dot(search_dir);

        while(f > f_in + alpha * cache) {
            alpha *= rho;

            m_system->set_x(x + alpha * search_dir);
            m_system->assemble<0>(true);
            f = m_system->f();
        }

        return alpha;
    }

    double linesearch_morethuente(Ref<const Vector> x,
        Ref<const Vector> search_dir, const double alpha_init = 1.0)
    {
        // assume step width
        double ak = alpha_init;

        m_system->set_x(x);
        m_system->assemble<1>(true);
        double fval = m_system->f();
        Vector g = m_system->g();

        Vector s = search_dir.eval();
        Vector xx = x.eval();

        cvsrch(xx, fval, g, ak, s);

        return ak;
    }

    int cvsrch(Ref<Vector> x, double f, Ref<Vector> g, double &stp,
        Ref<Vector> s)
    {
        // we rewrite this from MIN-LAPACK and some MATLAB code
        int info = 0;
        int infoc = 1;
        const double xtol = 1e-15;
        const double ftol = 1e-4;
        const double gtol = 1e-2;
        const double stpmin = 1e-15;
        const double stpmax = 1e15;
        const double xtrapf = 4;
        const int maxfev = 20;
        int nfev = 0;

        double dginit = g.dot(s);

        if (dginit >= 0.0) {
            // no descent direction
            // TODO: handle this case
            return -1;
        }

        bool brackt      = false;
        bool stage1      = true;

        double finit      = f;
        double dgtest     = ftol * dginit;
        double width      = stpmax - stpmin;
        double width1     = 2 * width;
        Vector wa = x.eval();

        double stx        = 0.0;
        double fx         = finit;
        double dgx        = dginit;
        double sty        = 0.0;
        double fy         = finit;
        double dgy        = dginit;

        double stmin;
        double stmax;

        while (true) {
            // make sure we stay in the interval when setting min/max-step-width
            if (brackt) {
                stmin = std::min(stx, sty);
                stmax = std::max(stx, sty);
            } else {
                stmin = stx;
                stmax = stp + xtrapf * (stp - stx);
            }

            // Force the step to be within the bounds stpmax and stpmin.
            stp = std::max(stp, stpmin);
            stp = std::min(stp, stpmax);

            // Oops, let us return the last reliable values
            if ((brackt && ((stp <= stmin) || (stp >= stmax)))
                || (nfev >= maxfev - 1 ) || (infoc == 0)
                || (brackt && ((stmax - stmin) <= (xtol * stmax)))) {
                stp = stx;
            }

            // test new point
            x = wa + stp * s;

            m_system->set_x(x);
            m_system->assemble<1>(true);
            f = m_system->f();
            g = m_system->g();
            nfev++;

            double dg = g.dot(s);
            double ftest1 = finit + stp * dgtest;

            // all possible convergence tests
            if ((brackt & ((stp <= stmin) | (stp >= stmax))) | (infoc == 0)) {
                info = 6;
            }

            if ((stp == stpmax) & (f <= ftest1) & (dg <= dgtest)) {
                info = 5;
            }

            if ((stp == stpmin) & ((f > ftest1) | (dg >= dgtest))) {
                info = 4;
            }

            if (nfev >= maxfev) {
                info = 3;
            }

            if (brackt & (stmax - stmin <= xtol * stmax)) {
                info = 2;
            }

            if ((f <= ftest1) & (fabs(dg) <= gtol * (-dginit))) {
                info = 1;
            }

            // terminate when convergence reached
            if (info != 0)
                return -1;

            if (stage1 & (f <= ftest1) & (dg >= std::min(ftol, gtol) * dginit))
                stage1 = false;

            if (stage1 & (f <= fx) & (f > ftest1)) {
                double fm = f - stp * dgtest;
                double fxm = fx - stx * dgtest;
                double fym = fy - sty * dgtest;
                double dgm = dg - dgtest;
                double dgxm = dgx - dgtest;
                double dgym = dgy - dgtest;

                cstep(stx, fxm, dgxm, sty, fym, dgym, stp, fm, dgm, brackt,
                    stmin, stmax, infoc);

                fx = fxm + stx * dgtest;
                fy = fym + sty * dgtest;
                dgx = dgxm + dgtest;
                dgy = dgym + dgtest;
            } else {
                // this is ugly and some variables should be moved to the class scope
                cstep(stx, fx, dgx, sty, fy, dgy, stp, f, dg, brackt, stmin,
                    stmax, infoc);
            }

            if (brackt) {
                if (fabs(sty - stx) >= 0.66 * width1) {
                    stp = stx + 0.5 * (sty - stx);
                }
                width1 = width;
                width = fabs(sty - stx);
            }
        }

        return 0;
    }

    int cstep(double& stx, double& fx, double& dx, double& sty, double& fy,
        double& dy, double& stp, double& fp, double& dp, bool& brackt,
        double& stpmin, double& stpmax, int& info)
    {
        info = 0;
        bool bound = false;

        // Check the input parameters for errors.
        if ((brackt & ((stp <= std::min(stx, sty) ) |
            (stp >= std::max(stx, sty)))) |
            (dx * (stp - stx) >= 0.0) | (stpmax < stpmin)) {
            return -1;
        }

        double sgnd = dp * (dx / fabs(dx));

        double stpf = 0;
        double stpc = 0;
        double stpq = 0;

        if (fp > fx) {
            info = 1;
            bound = true;

            double theta = 3.0 * (fx - fp) / (stp - stx) + dx + dp;
            double s = std::max(theta, std::max(dx, dp));
            double gamma = s * sqrt((theta / s) * (theta / s) -
                (dx / s) * (dp / s));

            if (stp < stx) {
                gamma = -gamma;
            }

            double p = (gamma - dx) + theta;
            double q = ((gamma - dx) + gamma) + dp;
            double r = p / q;

            stpc = stx + r * (stp - stx);
            stpq = stx + ((dx / ((fx - fp) / (stp - stx) + dx)) / 2.0) *
                (stp - stx);

            if (fabs(stpc - stx) < fabs(stpq - stx)) {
                stpf = stpc;
            } else {
                stpf = stpc + (stpq - stpc) / 2;
            }

            brackt = true;
        } else if (sgnd < 0.0) {
            info = 2;
            bound = false;

            double theta = 3 * (fx - fp) / (stp - stx) + dx + dp;
            double s = std::max(theta, std::max(dx, dp));
            double gamma = s * sqrt((theta / s) * (theta / s)  - (dx / s) * (dp / s));

            if (stp > stx) {
                gamma = -gamma;
            }

            double p = (gamma - dp) + theta;
            double q = ((gamma - dp) + gamma) + dx;
            double r = p / q;

            stpc = stp + r * (stx - stp);
            stpq = stp + (dp / (dp - dx)) * (stx - stp);

            if (fabs(stpc - stp) > fabs(stpq - stp)) {
                stpf = stpc;
            } else {
                stpf = stpq;
            }

            brackt = true;
        } else if (fabs(dp) < fabs(dx)) {
            info = 3;
            bound = 1;

            double theta = 3 * (fx - fp) / (stp - stx) + dx + dp;
            double s = std::max(theta, std::max( dx, dp));
            double gamma = s * sqrt(std::max(0.0, (theta / s) * (theta / s) -
                (dx / s) * (dp / s)));

            if (stp > stx) {
                gamma = -gamma;
            }

            double p = (gamma - dp) + theta;
            double q = (gamma + (dx - dp)) + gamma;
            double r = p / q;

            if ((r < 0.0) & (gamma != 0.0)) {
                stpc = stp + r * (stx - stp);
            } else if (stp > stx) {
                stpc = stpmax;
            } else {
                stpc = stpmin;
            }

            stpq = stp + (dp / (dp - dx)) * (stx - stp);

            if (brackt) {
                if (fabs(stp - stpc) < fabs(stp - stpq)) {
                    stpf = stpc;
                } else {
                    stpf = stpq;
                }
            } else {
                if (fabs(stp - stpc) > fabs(stp - stpq)) {
                    stpf = stpc;
                } else {
                    stpf = stpq;
                }
            }
        } else {
            info = 4;
            bound = false;

            if (brackt) {
                double theta = 3 * (fp - fy) / (sty - stp) + dy + dp;
                double s = std::max(theta, std::max(dy, dp));
                double gamma = s * sqrt((theta / s) * (theta / s) - (dy / s) * (dp / s));

                if (stp > sty) {
                    gamma = -gamma;
                }

                double p = (gamma - dp) + theta;
                double q = ((gamma - dp) + gamma) + dy;
                double r = p / q;

                stpc = stp + r * (sty - stp);
                stpf = stpc;
            } else if (stp > stx)
                stpf = stpmax;
            else {
                stpf = stpmin;
            }
        }

        if (fp > fx) {
            sty = stp;
            fy = fp;
            dy = dp;
        } else {
            if (sgnd < 0.0) {
                sty = stx;
                fy = fx;
                dy = dx;
            }

            stx = stp;
            fx = fp;
            dx = dp;
        }

        stpf = std::min(stpmax, stpf);
        stpf = std::max(stpmin, stpf);
        stp = stpf;

        if (brackt & bound) {
            if (sty > stx) {
                stp = std::min(stx + 0.66 * (sty - stx), stp);
            } else {
                stp = std::max(stx + 0.66 * (sty - stx), stp);
            }
        }

        return 0;
    }

public:     // constructor
    LevenbergMarquardt(Pointer<System<true>> system)
    : m_system(std::move(system))
    { }

public:     // method
    void minimize(const int maxiter, const double rtol, const double xtol)
    {
        // setup

        Log::info(1, "==> Minimizing nonlinear system...");
        Log::info(2, "Using LM minimizer");

        Timer timer;

        Functor functor(m_system);

        Vector x = m_system->x();
        Eigen::LevenbergMarquardt<Functor> lm(functor);
        lm.minimize(x);

        Log::info(1, "System minimized in {:.3f} sec", timer.ellapsed());
    }

public:     // python
    template <typename TModule>
    static void register_python(TModule& m)
    {
        namespace py = pybind11;
        using namespace pybind11::literals;

        using Type = EQlib::LevenbergMarquardt;

        py::class_<Type>(m, "LevenbergMarquardt")
            .def(py::init<Pointer<EQlib::System<true>>>(), "system"_a)
            .def("minimize", &Type::minimize, "maxiter"_a=100, "rtol"_a=1e-6,
                "xtol"_a=1e-6)
        ;
    }
};

} // namespace EQlib