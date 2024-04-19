#include "unit/PlatformFixtures.h"

#include <chrono>
#include <memory>

#include "server/client.h"
#include "server/sched/bruteforce.h"
#include "server/schedule.h"
#include "server/trace_logger.h"

using namespace tetris;

class SmallOdroidScheduleTest : public SmallOdroidTest {
protected:
  Client *CreateClient(const std::string &name,
                       std::vector<OperatingPoint> &ops) {
    ConnectionPtr conn;
    auto c = new Client(conn, nullptr);

    c->exec = name;
    for (auto &op : ops) {
      c->ops.emplace_back(op);
    }
    return c;
  }

  OperatingPoint CreateOP(const std::string &name, double utility, double power,
                          const CPUThreadSet &thread_set) {
    auto core_counts =
        platform->GetCoreCountPerType(platform->ToCPUCoreSet(thread_set));
    OperatingPoint::Configuration config{name, thread_set, core_counts};
    OperatingPoint::Metrics metrics{utility, power};
    return OperatingPoint{config, metrics};
  }

  Client *GetClientOP0() {
    std::vector<OperatingPoint> ops = {
        CreateOP("1L0B", 5.848, 0.351, {0}),
        CreateOP("1L1B", 10.526, 0.758, {0, 2}),
        CreateOP("2L0B", 11.364, 0.852, {0, 1}),
        CreateOP("2L2B", 12.821, 1.026, {0, 1, 2, 3}),
        CreateOP("2L1B", 21.277, 2.234, {0, 1, 2}),
        CreateOP("1L2B", 28.571, 3.429, {0, 2, 3}),
        CreateOP("0L1B", 11.628, 1.500, {2}),
        CreateOP("0L2B", 21.739, 3.087, {2, 3}),
    };

    return CreateClient("app1", ops);
  }

  Client *GetClientOP1() {
    std::vector<OperatingPoint> ops = {
        CreateOP("1L1B", 22.222, 1.333, {0, 2}),
        CreateOP("2L0B", 16.129, 1.065, {0, 1}),
        CreateOP("2L1B", 28.571, 2.000, {0, 1, 2}),
        CreateOP("1L0B", 8.772, 0.640, {0}),
        CreateOP("2L2B", 31.250, 2.344, {0, 1, 2, 3}),
        CreateOP("1L2B", 10.753, 0.828, {0, 2, 3}),
        CreateOP("0L2B", 23.810, 2.619, {2, 3}),
        CreateOP("0L1B", 13.158, 1.474, {2}),
    };

    return CreateClient("app2", ops);
  }

  Client *GetClientOP2() {
    std::vector<OperatingPoint> ops = {
        CreateOP("1L0B", 10.870, 0.435, {0}),
        CreateOP("2L0B", 18.868, 0.849, {0, 1}),
        CreateOP("1L1B", 38.462, 2.038, {0, 2}),
        CreateOP("2L2B", 43.478, 2.348, {0, 1, 2, 3}),
        CreateOP("2L1B", 43.478, 2.522, {0, 1, 2}),
        CreateOP("1L2B", 58.824, 3.765, {0, 2, 3}),
        CreateOP("0L2B", 55.556, 4.167, {2, 3}),
        CreateOP("0L1B", 29.412, 2.382, {2}),
    };

    return CreateClient("app3", ops);
  }

  std::vector<Client *> clients;
  virtual void SetUp() {
    SmallOdroidTest::SetUp();
    // Adding a sample client for use in tests
    clients.push_back(GetClientOP0());
    clients.push_back(GetClientOP1());
    clients.push_back(GetClientOP2());
  }

  virtual void TearDown() {
    for (Client *client : clients) {
      delete client;
    }
    SmallOdroidTest::TearDown();
  }
};

TEST_F(SmallOdroidScheduleTest, AddSegment) {
  Schedule multi_sched(clients, 0.0, true); // multi-segment
  EXPECT_EQ(multi_sched.GetStartTime(), 0.0);
  EXPECT_EQ(multi_sched.GetNumberOfSegments(), 0); // No segment added yet
  multi_sched.AddSegment(10.0);
  EXPECT_EQ(multi_sched.GetNumberOfSegments(), 1);
  EXPECT_EQ(multi_sched.GetTotalDuration().value(), 10.0);
  multi_sched.AddSegment(5.0);
  EXPECT_EQ(multi_sched.GetNumberOfSegments(), 2);
  EXPECT_EQ(multi_sched.GetTotalDuration().value(), 15.0);
  EXPECT_EQ(multi_sched.GetSegmentStartTime(0).value(), 0.0);
  EXPECT_EQ(multi_sched.GetSegmentEndTime(0).value(), 10.0);
  EXPECT_EQ(multi_sched.GetSegmentStartTime(1).value(), 10.0);
  EXPECT_EQ(multi_sched.GetSegmentEndTime(1).value(), 15.0);
  EXPECT_TRUE(multi_sched.IsMultiSegment());

  Schedule single_sched(clients, 0.0, false); // single-segment
  single_sched.AddSegment();
  EXPECT_EQ(single_sched.GetNumberOfSegments(), 1);
  EXPECT_EQ(single_sched.GetSegmentDuration(0).has_value(), false);
  EXPECT_EQ(single_sched.GetSegmentStartTime(0).value(), 0.0);
  EXPECT_EQ(single_sched.GetSegmentEndTime(0).has_value(), false);
  EXPECT_FALSE(single_sched.IsMultiSegment());
}

TEST_F(SmallOdroidScheduleTest, OperatingPoints) {
  Schedule multi_sched(clients, 0.0, true);
  multi_sched.AddSegment(10.0);

  OperatingPointAllocation op(clients[0]->ops[0], {});

  multi_sched.SetOperatingPoint(0, clients[0], op);
  auto retrieved_op = multi_sched.GetOperatingPoint(0, clients[0]);

  EXPECT_TRUE(retrieved_op.has_value());
  EXPECT_NEAR(retrieved_op->utility(), 5.848, 0.001);
  EXPECT_NEAR(retrieved_op->power(), 0.351, 0.001);
}

TEST_F(SmallOdroidScheduleTest, InvalidSegmentIndex) {
  Schedule multi_sched(clients, 0.0, true);
  multi_sched.AddSegment(10.0);

  // Expecting some kind of error or exception handling
  EXPECT_THROW(multi_sched.GetSegmentStartTime(1), std::out_of_range);
  EXPECT_THROW(multi_sched.GetSegmentEndTime(1), std::out_of_range);
}

TEST_F(SmallOdroidScheduleTest, NoOperatingPointSet) {
  Schedule multi_sched(clients, 0.0, true);
  multi_sched.AddSegment(10.0);

  // Not setting any operating point yet
  auto retrieved_op = multi_sched.GetOperatingPoint(0, clients[0]);
  EXPECT_FALSE(retrieved_op.has_value());
}

TEST_F(SmallOdroidScheduleTest, SetOperatingPointWithoutSegment) {
  Schedule multi_sched(clients, 0.0, true);

  OperatingPointAllocation op(clients[0]->ops[0], {});

  // Expecting an error as no segment added yet
  EXPECT_THROW(multi_sched.SetOperatingPoint(0, clients[0], op),
               std::out_of_range);
}

TEST_F(SmallOdroidScheduleTest, SplitSegment) {
  Schedule multi_sched(clients, 0.0, true);
  multi_sched.AddSegment(10.0);

  multi_sched.SplitAtTimepoint(6.0); // Splitting the segment at 6 seconds

  EXPECT_EQ(multi_sched.GetNumberOfSegments(), 2);
  EXPECT_EQ(multi_sched.GetSegmentStartTime(0).value(), 0.0);
  EXPECT_EQ(multi_sched.GetSegmentEndTime(0).value(), 6.0);
  EXPECT_EQ(multi_sched.GetSegmentStartTime(1).value(), 6.0);
  EXPECT_EQ(multi_sched.GetSegmentEndTime(1).value(), 10.0);
}

TEST_F(SmallOdroidScheduleTest, GetThreadSetAndProgress) {
  Schedule multi_sched(clients, 0.0, true);
  multi_sched.AddSegment(10.0);

  OperatingPointAllocation op0(clients[0]->ops[0], {});
  OperatingPointAllocation op1(clients[1]->ops[6], {});

  multi_sched.SetOperatingPoint(0, clients[0], op0);
  multi_sched.SetOperatingPoint(0, clients[1], op1);
  EXPECT_EQ(multi_sched.GetSegmentThreadSet(0), CPUThreadSet({0, 2, 3}));
  EXPECT_FALSE(multi_sched.HasSegmentOverlaps(0));
#if 0
  EXPECT_NEAR(*multi_sched.GetSegmentClientProgress(0, clients[0]),
              10.0 / 171.0, 0.01);
  EXPECT_NEAR(*multi_sched.GetSegmentClientProgress(0, clients[1]), 10.0 / 42.0,
              0.01);
  EXPECT_NEAR(*multi_sched.GetSegmentClientProgress(0, clients[2]), 0.0, 0.01);
#endif

  multi_sched.AddSegment(15.0);
  OperatingPointAllocation op2(clients[1]->ops[0], {});
  multi_sched.SetOperatingPoint(1, clients[0], op0);
  multi_sched.SetOperatingPoint(1, clients[1], op2);
  EXPECT_EQ(multi_sched.GetSegmentThreadSet(1), CPUThreadSet({0, 2}));
  EXPECT_TRUE(multi_sched.HasSegmentOverlaps(1));
#if 0
  EXPECT_NEAR(*multi_sched.GetSegmentClientProgress(1, clients[0]),
              15.0 / 171.0, 0.01);
  EXPECT_NEAR(*multi_sched.GetSegmentClientProgress(1, clients[1]), 15.0 / 45.0,
              0.01);
  EXPECT_NEAR(*multi_sched.GetSegmentClientProgress(1, clients[2]), 0.0, 0.01);
#endif
}

//
//  Brutefore scheduler
//

TEST_F(SmallOdroidScheduleTest, BruteforceMapper_OneJob) {
  BruteforceMapper mapper(*platform.get(), std::make_unique<EnergyObjective>());
  auto obj = mapper.GetObjective();
  auto s1 = mapper.GenerateSchedule({clients[0]}, 0.0);
  LOGGER->debug("%s", s1->ToString().c_str());
  EXPECT_EQ(s1->GetNumberOfSegments(), 1);
  auto s1_op = s1->GetOperatingPoint(0, clients[0]);
  EXPECT_TRUE(s1_op.has_value());
  EXPECT_EQ(s1_op->threads(), CPUThreadSet({0}));
  EXPECT_EQ(s1->GetSegmentThreadSet(0), CPUThreadSet({0}));
  auto s1_result = obj->EvaluateSchedule(*s1);
  EXPECT_EQ(std::get<0>(s1_result), 1);
  EXPECT_NEAR(std::get<1>(s1_result), 0.060, 0.001);

  auto s2 = mapper.GenerateSchedule({clients[0]}, 0.0, CPUCoreSet{0});
  LOGGER->debug("%s", s2->ToString().c_str());
  EXPECT_EQ(s2->GetNumberOfSegments(), 1);
  auto s2_op = s2->GetOperatingPoint(0, clients[0]);
  EXPECT_TRUE(s2_op.has_value());
  EXPECT_EQ(s2_op->threads(), CPUThreadSet({1}));
  EXPECT_EQ(s2->GetSegmentThreadSet(0), CPUThreadSet({1}));
  auto s2_result = obj->EvaluateSchedule(*s2);
  EXPECT_EQ(std::get<0>(s2_result), 1);
  EXPECT_NEAR(std::get<1>(s2_result), 0.060, 0.001);

  auto s3 = mapper.GenerateSchedule({clients[0]}, 0.0, CPUCoreSet{0, 1});
  LOGGER->debug("%s", s3->ToString().c_str());
  EXPECT_EQ(s3->GetNumberOfSegments(), 1);
  auto s3_op = s3->GetOperatingPoint(0, clients[0]);
  EXPECT_TRUE(s3_op.has_value());
  EXPECT_EQ(s3_op->threads(), CPUThreadSet({2}));
  EXPECT_EQ(s3->GetSegmentThreadSet(0), CPUThreadSet({2}));
  auto s3_result = obj->EvaluateSchedule(*s3);
  EXPECT_EQ(std::get<0>(s3_result), 1);
  EXPECT_NEAR(std::get<1>(s3_result), 0.129, 0.001);
}

TEST_F(SmallOdroidScheduleTest, BruteforceMapper_TwoJobs) {
  BruteforceMapper mapper(*platform.get(), std::make_unique<EnergyObjective>());
  auto obj = mapper.GetObjective();
  auto s1 = mapper.GenerateSchedule({clients[0], clients[1]}, 0.0);
  EXPECT_EQ(s1->GetNumberOfSegments(), 1);
  LOGGER->debug("%s", s1->ToString().c_str());
  auto s1_op0 = s1->GetOperatingPoint(0, clients[0]);
  auto s1_op1 = s1->GetOperatingPoint(0, clients[1]);
  EXPECT_TRUE(s1_op0.has_value());
  EXPECT_TRUE(s1_op1.has_value());
  EXPECT_EQ(s1_op0->threads(), CPUThreadSet({0}));
  EXPECT_EQ(s1_op1->threads(), CPUThreadSet({1, 2}));
  EXPECT_EQ(s1->GetSegmentThreadSet(0), CPUThreadSet({0, 1, 2}));
  auto s1_result = obj->EvaluateSchedule(*s1);
  EXPECT_EQ(std::get<0>(s1_result), 2);
  EXPECT_NEAR(std::get<1>(s1_result), 0.120, 0.001);

  // Set another objective
  mapper.SetObjective(std::make_unique<DelayObjective>());
  obj = mapper.GetObjective();
  auto s2 = mapper.GenerateSchedule({clients[0], clients[1]}, 0.0);
  EXPECT_EQ(s2->GetNumberOfSegments(), 1);
  LOGGER->debug("%s", s2->ToString().c_str());
  auto s2_op0 = s2->GetOperatingPoint(0, clients[0]);
  auto s2_op1 = s2->GetOperatingPoint(0, clients[1]);
  EXPECT_TRUE(s2_op0.has_value());
  EXPECT_TRUE(s2_op1.has_value());
  EXPECT_EQ(s2_op0->threads(), CPUThreadSet({2, 3}));
  EXPECT_EQ(s2_op1->threads(), CPUThreadSet({0, 1}));
  EXPECT_EQ(s2->GetSegmentThreadSet(0), CPUThreadSet({0, 1, 2, 3}));
  auto s2_result = obj->EvaluateSchedule(*s2);
  EXPECT_EQ(std::get<0>(s2_result), 2);
  EXPECT_NEAR(std::get<1>(s2_result), 0.108, 0.001);

  // Set another objective
  mapper.SetObjective(std::make_unique<GEDPObjective>(0.5));
  obj = mapper.GetObjective();
  auto s3 = mapper.GenerateSchedule({clients[0], clients[1]}, 0.0);
  LOGGER->debug("%s", s3->ToString().c_str());
  EXPECT_EQ(s3->GetNumberOfSegments(), 1);
  auto s3_op0 = s3->GetOperatingPoint(0, clients[0]);
  auto s3_op1 = s3->GetOperatingPoint(0, clients[1]);
  EXPECT_TRUE(s3_op0.has_value());
  EXPECT_TRUE(s3_op1.has_value());
  EXPECT_EQ(s3_op0->threads(), CPUThreadSet({0, 2}));
  EXPECT_EQ(s3_op1->threads(), CPUThreadSet({1, 3}));
  EXPECT_EQ(s3->GetSegmentThreadSet(0), CPUThreadSet({0, 1, 2, 3}));
  auto s3_result = obj->EvaluateSchedule(*s3);
  EXPECT_EQ(std::get<0>(s3_result), 2);
  EXPECT_NEAR(std::get<1>(s3_result), 0.135, 0.001);
}

TEST_F(SmallOdroidScheduleTest, BruteforceMapper_ThreeJobs) {
  BruteforceMapper mapper(*platform.get(), std::make_unique<EnergyObjective>());
  auto obj = mapper.GetObjective();
  auto s1 = mapper.GenerateSchedule(clients, 0.0);
  LOGGER->debug("%s", s1->ToString().c_str());
  EXPECT_EQ(s1->GetNumberOfSegments(), 1);
  auto s1_op0 = s1->GetOperatingPoint(0, clients[0]);
  auto s1_op1 = s1->GetOperatingPoint(0, clients[1]);
  auto s1_op2 = s1->GetOperatingPoint(0, clients[2]);
  EXPECT_TRUE(s1_op0.has_value());
  EXPECT_TRUE(s1_op1.has_value());
  EXPECT_TRUE(s1_op2.has_value());
  EXPECT_EQ(s1_op0->threads(), CPUThreadSet({0}));
  EXPECT_EQ(s1_op1->threads(), CPUThreadSet({1, 2}));
  EXPECT_EQ(s1_op2->threads(), CPUThreadSet({3}));
  EXPECT_EQ(s1->GetSegmentThreadSet(0), CPUThreadSet({0, 1, 2, 3}));
  auto s1_result = obj->EvaluateSchedule(*s1);
  EXPECT_EQ(std::get<0>(s1_result), 3);
  EXPECT_NEAR(std::get<1>(s1_result), 0.201, 0.001);
}

TEST_F(SmallOdroidScheduleTest,
       BruteforceMapper_SimulateThreeJobsWithTraceLogger) {
  std::chrono::high_resolution_clock::time_point zero_time;

  TraceLogger logger(zero_time);
  logger.RegisterPlatform(*platform);

  BruteforceMapper mapper(*platform.get(),
                          std::make_unique<BalancedObjective>());

  std::vector<Client *> active = clients;

  for (auto &c : active) {
    logger.RegisterClient(c);
  }

  auto s1 = mapper.GenerateSchedule(active, 0.0);
  LOGGER->debug("%s", s1->ToString().c_str());

  // assign operating points
  for (auto &c : active) {
    c->active_op = s1->GetOperatingPoint(0, c);
    logger.LogClientMappingBegin(zero_time, c, *c->active_op);
  }

  auto time_34ms = zero_time + std::chrono::milliseconds(34);
  for (auto &c : active) {
    LOGGER->debug("client_progress: %.3lf\n", c->progress);
    logger.LogClientMappingEnd(time_34ms, c);
    if (c->exec == "app3")
      logger.DeregisterClient(c);
  }

  active.erase(
      std::remove_if(active.begin(), active.end(),
                     [](const Client *c) { return c->exec == "app3"; }),
      active.end());

  auto s2 = mapper.GenerateSchedule(active, 0.0);
  LOGGER->debug("%s", s2->ToString().c_str());

  // assign operating points
  for (auto &c : active) {
    c->active_op = s2->GetOperatingPoint(0, c);
    logger.LogClientMappingBegin(time_34ms, c, *c->active_op);
  }

  auto time_63ms = zero_time + std::chrono::milliseconds(63);
  for (auto &c : active) {
    LOGGER->debug("client_progress: %.3lf\n", c->progress);
    logger.LogClientMappingEnd(time_63ms, c);
    logger.DeregisterClient(c);
  }

  logger.ExportToFile("unit_test.trace.json");
}
