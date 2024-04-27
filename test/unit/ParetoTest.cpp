#include <gtest/gtest.h>

#include "util/pareto.h"

using namespace tetris;

struct Point {
  int x;
  int y;

  Point(int x_val, int y_val) : x(x_val), y(y_val) {}

  bool operator==(const Point &other) const = default;
};

class ParetoTest : public ::testing::Test {
protected:
  std::unique_ptr<ParetoFrontFilter<Point>> pareto_filter;

  void SetUp() override {
    std::vector<ParetoFrontFilter<Point>::ObjectiveBetterFunc> objectives{
        [](const Point &a, const Point &b) { return a.x < b.x; },
        [](const Point &a, const Point &b) { return a.y < b.y; }};
    pareto_filter = std::make_unique<ParetoFrontFilter<Point>>(objectives);
  }
};

// Overload the operator<< to print Point objects
std::ostream &operator<<(std::ostream &os, const Point &point) {
  os << "(" << point.x << ", " << point.y << ")";
  return os;
}

TEST_F(ParetoTest, ParetoFrontFilter) {
  std::vector<Point> points{{1, 2}, {2, 1}, {2, 2}};

  auto pareto = pareto_filter->Filter(points);

  EXPECT_EQ(pareto.size(), 2);

  int flag = 0;

  for (const auto &p : pareto) {
    // std::cout << p << "\n";
    if (p == Point{1, 2}) {
      flag |= 1 << 0;
    }
    if (p == Point{2, 1}) {
      flag |= 1 << 1;
    }
  }

  EXPECT_EQ(flag, 3);

  points.emplace_back(1, 1);
  pareto = pareto_filter->Filter(points);
  EXPECT_EQ(pareto.size(), 1);
  EXPECT_EQ(pareto[0], Point(1, 1));
}
