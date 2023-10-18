#ifndef __SCHEDULE_H__
#define __SCHEDULE_H__

#include <sstream>
#pragma once

#include "client.h"
#include "util/operating_point.h"
#include "util/string_util.h"

#include <deque>
#include <map>
#include <optional>
#include <sstream>
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

    std::optional<double> GetClientProgress(int client_id) const {
      if (!_duration.has_value())
        return {};
      auto op = GetOperatingPoint(client_id);
      if (!op.has_value())
        return 0.0;
      double exec_time = op->characteristic("execution_time");
      return *_duration / exec_time;
    }

    CPUThreadSet GetThreadSet() const {
      CPUThreadSet res;
      for (auto &[cid, op] : _ops) {
        res |= op.GetThreadSet();
      }
      return res;
    }

    bool HasOverlaps() const {
      CPUThreadSet total_set;
      for (auto &[cid, op] : _ops) {
        auto op_set = op.GetThreadSet();
        if (total_set.OverlapsWith(op_set))
          return true;
        total_set |= op.GetThreadSet();
      }
      return false;
    }
  };

private:
  ScheduleType _type;
  std::map<Client *, int> _cid; // client indices
  std::deque<std::unique_ptr<Segment>> _segments;
  double _start_time;

  std::optional<int> GetClientId(Client *c) const {
    if (_cid.count(c) > 0) {
      return _cid.at(c);
    }
    return {};
  }

public:
  Schedule(const std::vector<Client *> &clients, double start_time,
           bool multi_segment)
      : _cid{}, _segments{}, _start_time{start_time} {
    for (int i = 0; i < clients.size(); ++i) {
      _cid[clients[i]] = i;
    }
    _type = multi_segment ? ScheduleType::kMultipleSegments
                          : ScheduleType::kSingleSegment;
  }

  bool IsMultiSegment() const {
    return _type == ScheduleType::kMultipleSegments;
  }

  std::vector<Client *> GetClients() const {
    std::vector<Client *> res;
    for (auto &[c, cid] : _cid) {
      res.push_back(c);
    }
    return res;
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
      throw std::out_of_range("Segment index out of range.");
    }

    double start_time = _start_time;
    for (size_t i = 0; i < seg_idx && i < _segments.size(); ++i) {
      start_time += *_segments[i]->GetDuration();
    }
    return start_time;
  }

  std::optional<double> GetSegmentEndTime(size_t seg_idx) const {
    if (seg_idx >= _segments.size()) {
      throw std::out_of_range("Segment index out of range.");
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
      throw std::out_of_range("Segment index out of range.");
    }
    return _segments[seg_idx]->GetDuration();
  }

  size_t GetNumberOfSegments() const { return _segments.size(); }

  void SetOperatingPoint(size_t seg_idx, Client *client,
                         OperatingPointAllocation op) {
    if (seg_idx >= _segments.size()) {
      throw std::out_of_range("Segment index out of range.");
    }
    auto cid = GetClientId(client);
    if (!cid.has_value()) {
      throw std::runtime_error("No such client.");
    }
    _segments[seg_idx]->SetOperatingPoint(*cid, op);
  }

  std::optional<OperatingPointAllocation>
  GetOperatingPoint(size_t seg_idx, Client *client) const {
    if (seg_idx >= _segments.size()) {
      throw std::out_of_range("Segment index out of range.");
    }
    auto cid = GetClientId(client);
    if (!cid.has_value()) {
      throw std::runtime_error("No such client.");
    }
    return _segments[seg_idx]->GetOperatingPoint(*cid);
  }

  CPUThreadSet GetSegmentThreadSet(size_t seg_idx) const {
    if (seg_idx >= _segments.size()) {
      throw std::out_of_range("Segment index out of range.");
    }
    return _segments[seg_idx]->GetThreadSet();
  }

  bool HasSegmentOverlaps(size_t seg_idx) const {
    if (seg_idx >= _segments.size()) {
      throw std::out_of_range("Segment index out of range.");
    }
    return _segments[seg_idx]->HasOverlaps();
  }

  size_t GetSegmentAtTimepoint(double tp) const {
    if (!IsMultiSegment()) {
      return 0;
    }
    double currentTime = _start_time;
    for (size_t i = 0; i < _segments.size(); ++i) {
      if (auto dur = _segments[i]->GetDuration()) {
        if (tp >= currentTime && tp < currentTime + *dur) {
          return i;
        }
        currentTime += *dur;
      }
    }
    throw std::runtime_error("Timepoint not within any segment.");
  }

  std::optional<double> GetSegmentClientProgress(size_t seg_idx,
                                                 Client *client) const {
    if (!IsMultiSegment())
      return {};
    auto cid = GetClientId(client);
    if (!cid.has_value()) {
      throw std::runtime_error("No such client.");
    }
    return _segments[seg_idx]->GetClientProgress(*cid);
  }

  void SplitAtTimepoint(double tp) {
    if (!IsMultiSegment())
      throw std::runtime_error("Cannot split a single-segment schedule.");
    size_t seg_idx = GetSegmentAtTimepoint(tp);
    double dur1 = tp - *GetSegmentStartTime(seg_idx);
    double dur2 = *_segments[seg_idx]->GetDuration() - dur1;

    // Modify the current segment's duration.
    _segments[seg_idx]->SetDuration(dur1);

    // Create a new segment with the remaining duration.
    auto new_segment = std::make_unique<Segment>(dur2);

    for (auto &[c, cid] : _cid) {
      auto op = _segments[seg_idx]->GetOperatingPoint(cid);
      if (op.has_value()) {
        new_segment->SetOperatingPoint(cid, *op);
      }
    }

    _segments.insert(_segments.begin() + seg_idx + 1, std::move(new_segment));
  }

  std::string ToString() const {
    if (IsMultiSegment()) {
      throw std::runtime_error("Not yet implemented");
    }

    std::stringstream ss;
    ss << "Schedule | " << _cid.size() << "  jobs | start_time " << _start_time
       << " | " << _segments.size() << "  segment\n";
    for (const auto &[c, cid] : _cid) {
      ss << "  - '" << c->exec << "' [" << c->pid << "]  progress "
         << c->progress << " | ";
      auto op = _segments[0]->GetOperatingPoint(cid);
      if (op.has_value()) {
        ss << "OP '" << op->base.name << "' exec_time "
           << op->characteristic("execution_time") << " energy "
           << op->characteristic("energy") << " | ";
        auto cores = op->GetThreadSet();
        ss << string_util::join(cores.GetList(), ",") << "\n";
      } else {
        ss << "NONE\n";
      }
    }
    return ss.str();
  }
};

} // namespace tetris

#endif
