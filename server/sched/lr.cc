#include "lr.h"

std::string LambdaToString(const std::map<std::string, double> &lambda) {
  std::vector<std::string> elems;

  for (const auto &[core_type, value] : lambda) {
    std::string elem = core_type + ": " + std::to_string(value);
    elems.push_back(elem);
  }
  return string_util::join(elems, ", ");
}

namespace tetris {

/**
 * Create a schedule object with the current mapping list
 */
std::unique_ptr<Schedule> LagrangianRelaxationMapper::ToSchedule(
    const std::vector<const OperatingPoint *> &ops, CPUCoreSet busy_cores) {
  auto &op_allocator = _platform.GetEquivResAllocator();
  auto schedule = std::make_unique<Schedule>(_clients, _start_time, false);
  schedule->AddSegment();
  for (int i = 0; i < _clients.size(); ++i) {
    if (ops[i] != nullptr) {
      auto opt_opa = op_allocator.FindEquivOP(*ops[i], busy_cores);
      assert(opt_opa.has_value());
      auto opa = *opt_opa;
      schedule->SetOperatingPoint(0, _clients[i], opa);
      busy_cores |= _platform.ToCPUCoreSet(opa.GetThreadSet());
    }
  }
  return schedule;
}
double LagrangianRelaxationMapper::EvaluateOPDual(
    const OperatingPoint &op, const std::map<std::string, double> &lambda,
    double rem_cratio) const {
  double value = _objective->EvaluateOP(op, rem_cratio);

  for (auto &[core_type, core_count] : op.cores_count) {
    value += lambda.at(core_type) * core_count;
  }
  return value;
}

std::tuple<const OperatingPoint *, double>
LagrangianRelaxationMapper::MinimizeDualFunctionClient(
    int c, const std::map<std::string, double> &lambda) const {
  Client &client = *_clients[c];
  double rem_cratio = 1.0 - client.progress;
  double best_value = EvaluateOPDual(_cl_pareto[c][0], lambda, rem_cratio);
  const OperatingPoint *best_op = &_cl_pareto[c][0];
  for (int i = 1; i < _cl_pareto[c].size(); ++i) {
    double op_value = EvaluateOPDual(_cl_pareto[c][i], lambda, rem_cratio);
    if (op_value < best_value) {
      best_value = op_value;
      best_op = &_cl_pareto[c][i];
    }
  }
  return std::make_tuple(best_op, best_value);
}

std::tuple<std::map<std::string, double>, std::vector<const OperatingPoint *>>
LagrangianRelaxationMapper::SolveDualOptimizationProblem() {
  // Initialize lambda
  std::map<std::string, double> lambda{};
  for (auto &[core_type, _] : _platform_cores_count) {
    lambda.emplace(core_type, 0.0);
  }
  std::vector<const OperatingPoint *> min_ops(_clients.size(), nullptr);

  for (int round = 1; round <= _max_rounds; ++round) {
    bool changed = false;

    double total_value = 0.0;

    // Find OPs which minimizes the dual function
    for (int c = 0; c < _clients.size(); ++c) {
      auto [c_op_ptr, c_value] = MinimizeDualFunctionClient(c, lambda);
      min_ops[c] = c_op_ptr;
      total_value += c_value;
    }

    auto lambda_str = LambdaToString(lambda);
    LOGGER->debug("LR Round %d; %s; Total value %lf\n", round,
                  lambda_str.c_str(), total_value);

    // Calculate subgradient of resource cofficients
    // delta_r = \sum_i r(x_i) - R
    std::map<std::string, int> delta_res;
    for (auto &[core_type, core_count] : _platform_cores_count) {
      delta_res.emplace(core_type, -core_count);
    }
    for (const auto &op_ptr : min_ops) {
      for (auto &[core_type, core_count] : op_ptr->cores_count) {
        delta_res[core_type] += core_count;
      }
    }

    // Update rule
    //
    // Note: the alpha coefficient can be still tuned
    double alpha = (total_value * 0.0002) / sqrt(round * 1.0);
    for (auto &[core_type, delta_core] : delta_res) {
      auto new_lambda = lambda[core_type] + alpha * delta_core;
      if (new_lambda < 0)
        new_lambda = 0.0;
      if (abs(new_lambda - lambda[core_type]) > 1e-8) {
        changed = true;
      }
      lambda[core_type] = new_lambda;
    }

    if (!changed)
      break;
  }

  return make_tuple(lambda, min_ops);
}

std::vector<int> LagrangianRelaxationMapper::SortClients(
    const std::map<std::string, double> &lambda,
    const std::vector<const OperatingPoint *> &lr_ops) const {
  // Initialize indices of clients
  std::vector<int> order(_clients.size());
  for (int i = 0; i < order.size(); ++i) {
    order[i] = i;
  }

  // Sort clients according the objective values in non-decreasing order
  std::sort(
      order.begin(), order.end(), [this, &lr_ops, &lambda](int i1, int i2) {
        double rem_cratio1 = 1.0 - this->_clients[i1]->progress;
        double rem_cratio2 = 1.0 - this->_clients[i2]->progress;
        double value1 = this->EvaluateOPDual(*lr_ops[i1], lambda, rem_cratio1);
        double value2 = this->EvaluateOPDual(*lr_ops[i2], lambda, rem_cratio2);
        return value1 < value2;
      });

  std::vector<std::string> order_str;
  for (auto i : order) {
    std::string s = std::to_string(i) + "[" + _clients[i]->exec + "]";
    order_str.push_back(s);
  }
  LOGGER->debug("Order: %s\n", string_util::join(order_str, ", ").c_str());
  return order;
}

const OperatingPoint *LagrangianRelaxationMapper::SelectClientOP(
    const std::map<std::string, int> &cores_count,
    const std::map<std::string, double> &lambda, int c) const {

  // Sort operating points
  std::vector<int> order(_cl_pareto[c].size());
  for (int i = 0; i < order.size(); ++i) {
    order[i] = i;
  }
  std::sort(order.begin(), order.end(), [this, c, &lambda](int i1, int i2) {
    double rem_cratio = 1.0 - this->_clients[c]->progress;
    auto &op1 = this->_cl_pareto[c][i1];
    auto &op2 = this->_cl_pareto[c][i2];
    double value1 = this->EvaluateOPDual(op1, lambda, rem_cratio);
    double value2 = this->EvaluateOPDual(op2, lambda, rem_cratio);
    return value1 < value2;
  });

  // Find the first fitting operating point
  for (auto i : order) {
    bool fit = true;
    for (auto &[core_type, core_count] : _cl_pareto[c][i].cores_count) {
      if (core_count > cores_count.at(core_type)) {
        fit = false;
        break;
      }
    }
    if (fit) {
      return &_cl_pareto[c][i];
    }
  }
  return nullptr;
}

std::vector<const OperatingPoint *> LagrangianRelaxationMapper::SelectOPs(
    const std::map<std::string, double> &lambda,
    const std::vector<const OperatingPoint *> &lr_ops) {
  // Sort clients according to objective value
  std::vector<int> order = SortClients(lambda, lr_ops);

  std::map<std::string, int> cores_count{_platform_cores_count};

  std::vector<const OperatingPoint *> res(_clients.size(), nullptr);
  for (auto i : order) {
    const OperatingPoint *op = SelectClientOP(cores_count, lambda, i);
    res[i] = op;
    for (auto &[core_type, core_count] : op->cores_count) {
      cores_count[core_type] -= core_count;
    }
  }

  return res;
}

/**
 * Selects client mappings considering a list of blocked CPUs.
 *
 * \param clients The list of clients for which mappings need to be selected.
 * \param start_time The start time of the schedule
 * \param blocked_cpus The list of blocked CPUs.
 * \return The selected mappings.
 */
std::unique_ptr<Schedule>
LagrangianRelaxationMapper::GenerateSchedule(std::vector<Client *> clients,
                                             double start_time,
                                             CPUCoreSet blocked_cores) {
  // Initialize internal data structures
  _clients = clients;
  _start_time = start_time;
  _blocked = blocked_cores;
  _cl_pareto.clear();

  // Filter Pareto-front for each client
  //
  // Note: It takes 7ms to filter from 391 to 72 operating points (four jobs).
  // Possible optimization: save pointers instead of copying OperatingPoints
  for (const auto &c : _clients) {
    _cl_pareto.push_back(_objective->FilterParetoFront(_platform, c->ops));
    LOGGER->debug("Filtering operating points for '%s' [%d] from %d to %d.\n",
                  c->exec.c_str(), c->pid, c->ops.size(),
                  _cl_pareto.back().size());
  }

  auto [lambda, lr_ops] = SolveDualOptimizationProblem();

  std::vector<const OperatingPoint *> ops = SelectOPs(lambda, lr_ops);

  return ToSchedule(ops, _blocked);
}
} // namespace tetris
