#ifndef __SCHEDULE_H__
#define __SCHEDULE_H__

#pragma once

#include "client.h"
#include "util/operating_point.h"

#include <deque>
#include <map>
#include <optional>
#include <vector>

namespace tetris {

class Schedule {

  enum class ScheduleType {
    kMultipleSegments,
    kSingleSegment,
  };

  class Segment {
  private:
    std::optional<double> _duration;
    std::map<int, OperatingPointAllocation> _ops;

  public:
    Segment() : _duration{}, _ops{} {}
    explicit Segment(double duration) : _duration{duration}, _ops{} {}

    std::optional<double> GetDuration() const { return _duration; }

    void SetDuration(double duration) { _duration = duration; }

    std::optional<OperatingPointAllocation>
    GetOperatingPoint(int client_id) const {
      auto it = _ops.find(client_id);
      if (it == _ops.end()) {
        return {};
      }
      return it->second;
    }

    void SetOperatingPoint(int client_id, OperatingPointAllocation op) {
      _ops[client_id] = op;
    }
  };

private:
  ScheduleType _type;
  std::map<int, Client *> _clients;
  std::deque<std::unique_ptr<Segment>> _segments;
  double _start_time;

public:
  Schedule(const std::vector<Client *> &clients, double start_time,
           bool multi_segment)
      : _clients{}, _segments{}, _start_time{start_time} {
    for (auto &c : clients) {
      _clients[c->pid] = c;
    }
    _type = multi_segment ? ScheduleType::kMultipleSegments
                          : ScheduleType::kSingleSegment;
  }

  std::optional<double> GetTotalDuration() const {
    if (_type == ScheduleType::kSingleSegment)
      return {};
    double total_duration = 0.0;
    for (const auto &segment : _segments) {
      auto dur = segment->GetDuration();
      assert(dur.has_value());
      total_duration += *dur;
    }
    return total_duration;
  }

  double GetStartTime() const { return _start_time; }

  std::optional<double> GetEndTime() const {
    if (_type == ScheduleType::kSingleSegment)
      return {};
    auto total_duration = GetTotalDuration();
    assert(total_duration.has_value());
    return _start_time + *total_duration;
  }

  std::optional<double> GetSegmentStartTime(size_t seg_idx) const {
    if (seg_idx >= _segments.size()) {
      throw std::runtime_error("Segment index out of range.");
    }

    double start_time = _start_time;
    for (size_t i = 0; i < seg_idx && i < _segments.size(); ++i) {
      start_time += *_segments[i]->GetDuration();
    }
    return start_time;
  }

  std::optional<double> GetSegmentEndTime(size_t seg_idx) const {
    if (seg_idx >= _segments.size()) {
      throw std::runtime_error("Segment index out of range.");
    }

    if (_type == ScheduleType::kSingleSegment)
      return {};
    double end_time = *GetSegmentStartTime(seg_idx);
    end_time += *_segments[seg_idx]->GetDuration();
    return end_time;
  }

  void AddSegment(double duration) {
    if (_type != ScheduleType::kMultipleSegments) {
      throw std::runtime_error(
          "Cannot add a segment with duration to a single-segment schedule.");
    }
    _segments.emplace_back(std::make_unique<Segment>(duration));
  }

  void AddSegment() {
    if (_type != ScheduleType::kSingleSegment || !_segments.empty()) {
      throw std::runtime_error(
          "Cannot add more than one segment to a single-segment schedule or "
          "add a segment without duration to a multi-segment schedule.");
    }
    _segments.emplace_back(std::make_unique<Segment>());
  }

  std::optional<double> GetSegmentDuration(size_t seg_idx) const {
    if (seg_idx >= _segments.size()) {
      throw std::runtime_error("Segment index out of range.");
    }
    return _segments[seg_idx]->GetDuration();
  }

  size_t GetNumberOfSegments() const { return _segments.size(); }

  void SetOperatingPoint(size_t seg_idx, int client_id,
                         OperatingPointAllocation op) {
    if (seg_idx >= _segments.size()) {
      throw std::runtime_error("Segment index out of range.");
    }
    _segments[seg_idx]->SetOperatingPoint(client_id, op);
  }

  std::optional<OperatingPointAllocation>
  GetOperatingPoint(size_t seg_idx, int client_id) const {
    if (seg_idx >= _segments.size()) {
      throw std::runtime_error("Segment index out of range.");
    }
    return _segments[seg_idx]->GetOperatingPoint(client_id);
  }
};

} // namespace tetris

#endif
