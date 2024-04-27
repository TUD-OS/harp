#ifndef __PARETO_H__
#define __PARETO_H__

#pragma once

#include <functional>
#include <vector>

namespace tetris {

/**
 * \brief A template class for filtering a Pareto front from a set of items
 * based on multiple objectives.
 *
 * This class is designed to determine the Pareto optimal set from a collection
 * of items. Each item is evaluated against multiple criteria (objectives) to
 * establish dominance relationships among them.
 *
 * @tparam T The type of the items being evaluated.
 */
template <typename T> class ParetoFrontFilter {
public:
  /// Type definition for a function that compares two items based on a single
  /// objective
  using ObjectiveBetterFunc = std::function<bool(const T &, const T &)>;

  /**
   * \brief Constructs a Pareto front filter with a set of objective functions.
   *
   * \param objectives A vector of objective functions, each representing a
   * criterion for comparison.
   */
  explicit ParetoFrontFilter(std::vector<ObjectiveBetterFunc> objectives)
      : _objectives(std::move(objectives)) {}

  /**
   * \brief Resets the objectives used for filtering the Pareto front.
   *
   * \param new_objectives A vector of new objective functions to replace the
   * existing ones.
   */
  void Reset(std::vector<ObjectiveBetterFunc> new_objectives) {
    _objectives = std::move(new_objectives);
  }

  /**
   * \brief Filters the provided items to determine the Pareto optimal set.
   *
   * \param items A vector of items to filter.
   * \return A vector containing the Pareto optimal items.
   */
  std::vector<T> Filter(const std::vector<T> &items) const;

private:
  std::vector<ObjectiveBetterFunc> _objectives;

  /**
   * \brief Determines if one item dominates another based on the set
   * objectives.
   *
   * An item 'a' is considered to dominate item 'b' if it is better than or
   * equal to 'b' in all objectives and better in at least one.
   *
   * \param a The item being compared.
   * \param b The item being compared against.
   * \return True if 'a' dominates 'b', false otherwise.
   */
  bool Dominates(const T &a, const T &b) const;
};

template <typename T>
std::vector<T> ParetoFrontFilter<T>::Filter(const std::vector<T> &items) const {
  std::vector<T> pareto;
  for (const auto &item : items) {
    bool dominated = false;
    auto it = pareto.begin();
    while (it != pareto.end()) {
      if (Dominates(*it, item)) {
        dominated = true;
        break;
      } else if (Dominates(item, *it)) {
        it = pareto.erase(it);
      } else {
        ++it;
      }
    }
    if (!dominated) {
      pareto.push_back(item);
    }
  }
  return pareto;
}

template <typename T>
bool ParetoFrontFilter<T>::Dominates(const T &a, const T &b) const {
  bool at_least_one_better = false;
  for (auto &func : _objectives) {
    if (func(a, b)) {
      at_least_one_better = true;
    } else if (func(b, a)) {
      return false;
    }
  }
  return at_least_one_better;
}
} // namespace tetris

#endif
