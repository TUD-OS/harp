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

double LagrangianRelaxationMapper::EvaluateOPDual(
    const OperatingPoint &op,
    const std::map<std::string, double> &lambda) const {
  double value = _evaluator->Evaluate(op);

  for (auto &[core_type, core_count] : op.core_counts()) {
    value += lambda.at(core_type) * core_count;
  }
  return value;
}

std::tuple<const OperatingPoint *, double>
LagrangianRelaxationMapper::MinimizeDualFunctionClient(
    int c, const std::map<std::string, double> &lambda) const {
  Client &client = *_clients[c];
  double best_value = EvaluateOPDual(_client_ops[c][0], lambda);
  const OperatingPoint *best_op = &_client_ops[c][0];
  for (int i = 1; i < _client_ops[c].size(); ++i) {
    double op_value = EvaluateOPDual(_client_ops[c][i], lambda);
    if (op_value < best_value) {
      best_value = op_value;
      best_op = &_client_ops[c][i];
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
      for (auto &[core_type, core_count] : op_ptr->core_counts()) {
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
  std::sort(order.begin(), order.end(),
            [this, &lr_ops, &lambda](int i1, int i2) {
              double value1 = this->EvaluateOPDual(*lr_ops[i1], lambda);
              double value2 = this->EvaluateOPDual(*lr_ops[i2], lambda);
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
  std::vector<int> order(_client_ops[c].size());
  for (int i = 0; i < order.size(); ++i) {
    order[i] = i;
  }
  std::sort(order.begin(), order.end(), [this, c, &lambda](int i1, int i2) {
    auto &op1 = this->_client_ops[c][i1];
    auto &op2 = this->_client_ops[c][i2];
    double value1 = this->EvaluateOPDual(op1, lambda);
    double value2 = this->EvaluateOPDual(op2, lambda);
    return value1 < value2;
  });

  // Find the first fitting operating point
  for (auto i : order) {
    bool fit = true;
    for (auto &[core_type, core_count] : _client_ops[c][i].core_counts()) {
      if (core_count > cores_count.at(core_type)) {
        fit = false;
        break;
      }
    }
    if (fit) {
      return &_client_ops[c][i];
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
    for (auto &[core_type, core_count] : op->core_counts()) {
      cores_count[core_type] -= core_count;
    }
  }

  return res;
}

/**
 * Selects client mappings considering a list of blocked CPUs.
 *
 * \param clients The list of clients for which mappings need to be selected.
 * \param blocked_cpus The list of blocked CPUs.
 * \return The selected mappings.
 */
ClientMapping
LagrangianRelaxationMapper::GenerateClientMapping(std::vector<Client *> clients,
                                                  CPUCoreSet blocked_cores) {
  // Initialize internal data structures
  _clients = clients;
  _blocked = blocked_cores;
  _client_ops.clear();

  LOGGER->debug(
      "Allocating clients to the resource using LagrangianRelaxationMapper\n");
  LOGGER->debug("Current clients:\n");

  // Filter Pareto-front for each client
  for (const auto &c : _clients) {
    _client_ops.push_back(c->op_table->GetParetoFront());
    LOGGER->debug("  - '%s' [%d]: %d operating points.\n", c->exec.c_str(),
                  c->pid, _client_ops.back().size());
  }

  auto [lambda, lr_ops] = SolveDualOptimizationProblem();

  std::vector<const OperatingPoint *> ops = SelectOPs(lambda, lr_ops);

  return ToClientMapping(clients, ops, _blocked);
}
} // namespace tetris
