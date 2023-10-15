#include "unit/PlatformFixtures.h"

#include <memory>

#include "server/client.h"
#include "server/manager.h"
#include "server/sched/bruteforce.h"
#include "server/schedule.h"

using namespace tetris;

class SmallOdroidScheduleTest : public SmallOdroidTest {
protected:
  Client *CreateClient(std::vector<OperatingPoint> &ops) {
    ConnectionPtr conn;
    auto c = new Client(conn);

    for (auto &op : ops) {
      c->ops.emplace_back(op);
    }
    return c;
  }

  Client *GetClientOP0() {
    std::string extime = "execution_time";
    std::string energy = "energy";
    std::vector<OperatingPoint> ops = {
        {"1L0B", {{extime, 171}, {energy, 60}}, CPUThreadSet{0}},
        {"1L1B", {{extime, 95}, {energy, 72}}, CPUThreadSet{0, 2}},
        {"2L0B", {{extime, 88}, {energy, 75}}, CPUThreadSet{0, 1}},
        {"2L2B", {{extime, 78}, {energy, 80}}, CPUThreadSet{0, 1, 2, 3}},
        {"2L1B", {{extime, 47}, {energy, 105}}, CPUThreadSet{0, 1, 2}},
        {"1L2B", {{extime, 35}, {energy, 120}}, CPUThreadSet{0, 2, 3}},
        {"0L1B", {{extime, 86}, {energy, 129}}, CPUThreadSet{2}},
        {"0L2B", {{extime, 46}, {energy, 142}}, CPUThreadSet{2, 3}}};

    return CreateClient(ops);
  }

  Client *GetClientOP1() {
    std::string extime = "execution_time";
    std::string energy = "energy";
    std::vector<OperatingPoint> ops = {
        {"1L1B", {{extime, 45}, {energy, 60}}, CPUThreadSet{0, 2}},
        {"2L0B", {{extime, 62}, {energy, 66}}, CPUThreadSet{0, 1}},
        {"2L1B", {{extime, 35}, {energy, 70}}, CPUThreadSet{0, 1, 2}},
        {"1L0B", {{extime, 114}, {energy, 73}}, CPUThreadSet{0}},
        {"2L2B", {{extime, 32}, {energy, 75}}, CPUThreadSet{0, 1, 2, 3}},
        {"1L2B", {{extime, 93}, {energy, 77}}, CPUThreadSet{0, 2, 3}},
        {"0L2B", {{extime, 42}, {energy, 110}}, CPUThreadSet{2, 3}},
        {"0L1B", {{extime, 76}, {energy, 112}}, CPUThreadSet{2}}};

    return CreateClient(ops);
  }

  Client *GetClientOP2() {
    std::string extime = "execution_time";
    std::string energy = "energy";
    std::vector<OperatingPoint> ops = {
        {"1L0B", {{extime, 92}, {energy, 40}}, CPUThreadSet{0}},
        {"2L0B", {{extime, 53}, {energy, 45}}, CPUThreadSet{0, 1}},
        {"1L1B", {{extime, 26}, {energy, 53}}, CPUThreadSet{0, 2}},
        {"2L2B", {{extime, 23}, {energy, 54}}, CPUThreadSet{0, 1, 2, 3}},
        {"2L1B", {{extime, 23}, {energy, 58}}, CPUThreadSet{0, 1, 2}},
        {"1L2B", {{extime, 17}, {energy, 64}}, CPUThreadSet{0, 2, 3}},
        {"0L2B", {{extime, 18}, {energy, 75}}, CPUThreadSet{2, 3}},
        {"0L1B", {{extime, 34}, {energy, 81}}, CPUThreadSet{2}},
    };

    return CreateClient(ops);
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
  EXPECT_EQ(retrieved_op->characteristic("execution_time"), 171);
  EXPECT_EQ(retrieved_op->characteristic("energy"), 60);
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
  EXPECT_NEAR(*multi_sched.GetSegmentClientProgress(0, clients[0]),
              10.0 / 171.0, 0.01);
  EXPECT_NEAR(*multi_sched.GetSegmentClientProgress(0, clients[1]), 10.0 / 42.0,
              0.01);
  EXPECT_NEAR(*multi_sched.GetSegmentClientProgress(0, clients[2]), 0.0, 0.01);

  multi_sched.AddSegment(15.0);
  OperatingPointAllocation op2(clients[1]->ops[0], {});
  multi_sched.SetOperatingPoint(1, clients[0], op0);
  multi_sched.SetOperatingPoint(1, clients[1], op2);
  EXPECT_EQ(multi_sched.GetSegmentThreadSet(1), CPUThreadSet({0, 2}));
  EXPECT_TRUE(multi_sched.HasSegmentOverlaps(1));
  EXPECT_NEAR(*multi_sched.GetSegmentClientProgress(1, clients[0]),
              15.0 / 171.0, 0.01);
  EXPECT_NEAR(*multi_sched.GetSegmentClientProgress(1, clients[1]), 15.0 / 45.0,
              0.01);
  EXPECT_NEAR(*multi_sched.GetSegmentClientProgress(1, clients[2]), 0.0, 0.01);
}

//
//  Brutefore scheduler
//

TEST_F(SmallOdroidScheduleTest, BruteforceMapper_OneJob) {
  BruteforceMapper mapper(*platform.get());
  auto s1 = mapper.GenerateSchedule({clients[0]}, 0.0);
  EXPECT_EQ(s1.GetNumberOfSegments(), 1);
  auto s1_op = s1.GetOperatingPoint(0, clients[0]);
  EXPECT_TRUE(s1_op.has_value());
  EXPECT_EQ(s1_op->GetThreadSet(), CPUThreadSet({0}));
  EXPECT_EQ(s1.GetSegmentThreadSet(0), CPUThreadSet({0}));

  auto s2 = mapper.GenerateSchedule({clients[0]}, 0.0, CPUThreadSet{0});
  EXPECT_EQ(s2.GetNumberOfSegments(), 1);
  auto s2_op = s2.GetOperatingPoint(0, clients[0]);
  EXPECT_TRUE(s2_op.has_value());
  EXPECT_EQ(s2_op->GetThreadSet(), CPUThreadSet({1}));
  EXPECT_EQ(s2.GetSegmentThreadSet(0), CPUThreadSet({1}));

  auto s3 = mapper.GenerateSchedule({clients[0]}, 0.0, CPUThreadSet{0, 1});
  EXPECT_EQ(s3.GetNumberOfSegments(), 1);
  auto s3_op = s3.GetOperatingPoint(0, clients[0]);
  EXPECT_TRUE(s3_op.has_value());
  EXPECT_EQ(s3_op->GetThreadSet(), CPUThreadSet({2}));
  EXPECT_EQ(s3.GetSegmentThreadSet(0), CPUThreadSet({2}));
}

TEST_F(SmallOdroidScheduleTest, BruteforceMapper_TwoJobs) {
  BruteforceMapper mapper(*platform.get());
  auto s1 = mapper.GenerateSchedule({clients[0], clients[1]}, 0.0);
  EXPECT_EQ(s1.GetNumberOfSegments(), 1);
  auto s1_op0 = s1.GetOperatingPoint(0, clients[0]);
  auto s1_op1 = s1.GetOperatingPoint(0, clients[1]);
  EXPECT_TRUE(s1_op0.has_value());
  EXPECT_TRUE(s1_op1.has_value());
  EXPECT_EQ(s1_op0->GetThreadSet(), CPUThreadSet({0}));
  EXPECT_EQ(s1_op1->GetThreadSet(), CPUThreadSet({1, 2}));
  EXPECT_EQ(s1.GetSegmentThreadSet(0), CPUThreadSet({0, 1, 2}));
}

TEST_F(SmallOdroidScheduleTest, BruteforceMapper_ThreeJobs) {
  BruteforceMapper mapper(*platform.get());
  auto s1 = mapper.GenerateSchedule(clients, 0.0);
  EXPECT_EQ(s1.GetNumberOfSegments(), 1);
  auto s1_op0 = s1.GetOperatingPoint(0, clients[0]);
  auto s1_op1 = s1.GetOperatingPoint(0, clients[1]);
  auto s1_op2 = s1.GetOperatingPoint(0, clients[2]);
  EXPECT_TRUE(s1_op0.has_value());
  EXPECT_TRUE(s1_op1.has_value());
  EXPECT_TRUE(s1_op2.has_value());
  EXPECT_EQ(s1_op0->GetThreadSet(), CPUThreadSet({0}));
  EXPECT_EQ(s1_op1->GetThreadSet(), CPUThreadSet({1, 2}));
  EXPECT_EQ(s1_op2->GetThreadSet(), CPUThreadSet({3}));
  EXPECT_EQ(s1.GetSegmentThreadSet(0), CPUThreadSet({0, 1, 2, 3}));
}
