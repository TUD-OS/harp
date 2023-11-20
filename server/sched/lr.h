#ifndef __SCHED_LR_H__
#define __SCHED_LR_H__

#pragma once

#include "base.h"
#include "util/platform/platform.h"

#include <tuple>

namespace tetris {

/**
 * Bruteforce mapper.
 *
 * This scheduler generates a single-segment schedule (that's why called
 * "mapper") using a bruteforce algorithm.
 */
class LagrangianRelaxationMapper : public BaseScheduler {
private:
  /**
   * Create a schedule object with the given mapping list
   */
  std::unique_ptr<Schedule>
  ToSchedule(const std::vector<const OperatingPoint *> &ops,
             CPUCoreSet busy_cores);
  double EvaluateOPDual(const OperatingPoint &op,
                        const std::map<std::string, double> &lambda,
                        double rem_cratio) const;

  std::tuple<const OperatingPoint *, double>
  MinimizeDualFunctionClient(int c,
                             const std::map<std::string, double> &lambda) const;

  /**
   * Solve dual optimization problem
   *
   * Lagrangian function is
   *  Lagr(x,lambda) =
   *    = \sum_{i=1}^n (e(x_i) + \sum_{k=1}^m lambda_k*(r_k(x_i)-R_k))
   *
   * Dual function is
   *  g(lambda) = min_x Lagr(x,lambda) =
   *    = \sum_i min_{x_i} {e(x_i) + \sum_k lambda_k * r_k(x_i)}
   *      - \sum_k lambda_k * R_k =
   *    = \sum_{i=1}^n min_{x_i} f(x_i, lambda) - sum_k lambda_k * R_k
   *
   * Dual optimization problem:
   *   maximize g(lambda)
   *    subject to lambda >= 0
   *
   * Stefan Wildermann, Michael Glaß, and Jürgen Teich. 2014. Multi-objective
   * distributed run-time resource management for many-cores. In Proceedings of
   * the conference on Design, Automation & Test in Europe (DATE '14).
   */
  std::tuple<std::map<std::string, double>, std::vector<const OperatingPoint *>>
  SolveDualOptimizationProblem();

  std::vector<int>
  SortClients(const std::map<std::string, double> &lambda,
              const std::vector<const OperatingPoint *> &lr_ops) const;

  const OperatingPoint *
  SelectClientOP(const std::map<std::string, int> &cores_count,
                 const std::map<std::string, double> &lambda, int c) const;

  std::vector<const OperatingPoint *>
  SelectOPs(const std::map<std::string, double> &lambda,
            const std::vector<const OperatingPoint *> &lr_ops);

public:
  explicit LagrangianRelaxationMapper(
      const Platform &platform,
      std::unique_ptr<OptimizationObjective> objective, int max_rounds)
      : _platform{platform},
        _platform_cores_count{platform.GetCoreCountPerType()},
        _objective{std::move(objective)}, _max_rounds{max_rounds} {}

  void SetObjective(std::unique_ptr<OptimizationObjective> objective) override {
    _objective = std::move(objective);
  }

  OptimizationObjective *GetObjective() override { return _objective.get(); }

  // Bring all overloads of GenerateSchedule()
  using BaseScheduler::GenerateSchedule;

  /**
   * Selects client mappings considering a list of blocked CPUs.
   *
   * \param clients The list of clients for which mappings need to be selected.
   * \param start_time The start time of the schedule
   * \param blocked_cpus The list of blocked CPUs.
   * \return The selected mappings.
   */
  std::unique_ptr<Schedule> GenerateSchedule(std::vector<Client *> clients,
                                             double start_time,
                                             CPUCoreSet blocked_cores) override;

private:
  const Platform &_platform;
  std::map<std::string, int> _platform_cores_count;
  std::unique_ptr<OptimizationObjective> _objective;
  int _max_rounds;

  // temporary fields (initialized at each invokation of GenerateSchedule())
  std::vector<Client *> _clients;
  double _start_time;
  CPUCoreSet _blocked;

  std::vector<std::vector<OperatingPoint>> _cl_pareto; // Pareto front of ops
};

} // namespace tetris

#endif /* __SCHED_LR_H__ */
