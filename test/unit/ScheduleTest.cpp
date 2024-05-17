#include "unit/PlatformFixtures.h"

#include <chrono>
#include <memory>

#include "server/client.h"
#include "server/manager.h"
#include "server/mapper/factory.h"
#include "server/trace_logger.h"

using namespace tetris;

class SmallOdroidClientMappingTest : public SmallOdroidTest {
protected:
  Client *CreateClient(const std::string &name,
                       const std::vector<OperatingPoint> &ops) {
    ConnectionPtr conn;
    auto c = new Client(conn, *manager);

    c->exec = name;
    c->op_table =
        std::make_unique<CustomOperatingPointTable>(manager->GetPlatform());
    for (auto &op : ops) {
      c->op_table->AddOperatingPoint(op);
    }
    return c;
  }

  OperatingPoint CreateOP(const std::string &name, double utility, double power,
                          const CPUThreadSet &thread_set) {
    const Platform &platform = manager->GetPlatform();
    auto core_counts =
        platform.GetCoreCountPerType(platform.ToCPUCoreSet(thread_set));
    OperatingPoint::Configuration config{name, thread_set, core_counts};
    OperatingPoint::Metrics metrics{utility, power};
    return OperatingPoint{config, metrics};
  }

  std::pair<Client *, std::vector<OperatingPoint>> GetClientOP0() {
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

    return std::make_pair(CreateClient("app1", ops), ops);
  }

  std::pair<Client *, std::vector<OperatingPoint>> GetClientOP1() {
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

    return std::make_pair(CreateClient("app2", ops), ops);
  }

  std::pair<Client *, std::vector<OperatingPoint>> GetClientOP2() {
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

    return std::make_pair(CreateClient("app3", ops), ops);
  }

  std::unique_ptr<Manager> manager;
  std::vector<Client *> clients;
  std::vector<std::vector<OperatingPoint>> client_ops;

  virtual void SetUp() {
    SmallOdroidTest::SetUp();
    manager = std::make_unique<Manager>(std::move(platform), nullptr, false);
    // Adding a sample client for use in tests
    auto [client, ops] = GetClientOP0();
    clients.push_back(client);
    client_ops.push_back(ops);

    std::tie(client, ops) = GetClientOP1();
    clients.push_back(client);
    client_ops.push_back(ops);

    std::tie(client, ops) = GetClientOP2();
    clients.push_back(client);
    client_ops.push_back(ops);
  }

  virtual void TearDown() {
    for (Client *client : clients) {
      delete client;
    }
    SmallOdroidTest::TearDown();
  }
};

TEST_F(SmallOdroidClientMappingTest, OperatingPoints) {
  ClientMapping cm;

  OperatingPointAllocation op(client_ops[0][0], {});

  cm.Set(clients[0], op);
  auto retrieved_op = cm.Get(clients[0]);

  EXPECT_NEAR(retrieved_op.utility(), 5.848, 0.001);
  EXPECT_NEAR(retrieved_op.power(), 0.351, 0.001);
}

TEST_F(SmallOdroidClientMappingTest, GetThreadSet) {
  ClientMapping cm1;

  OperatingPointAllocation op0(client_ops[0][0], {});
  OperatingPointAllocation op1(client_ops[1][6], {});

  cm1.Set(clients[0], op0);
  cm1.Set(clients[1], op1);
  EXPECT_EQ(cm1.GetAllThreads(), CPUThreadSet({0, 2, 3}));
  EXPECT_FALSE(cm1.HasOverlaps());

  ClientMapping cm2;
  OperatingPointAllocation op2(client_ops[1][0], {});
  cm2.Set(clients[0], op0);
  cm2.Set(clients[1], op2);
  EXPECT_EQ(cm2.GetAllThreads(), CPUThreadSet({0, 2}));
  EXPECT_TRUE(cm2.HasOverlaps());
}

//
//  Brutefore scheduler
//

TEST_F(SmallOdroidClientMappingTest, BruteforceMapper_OneJob) {
  auto mapper = ClientMapperFactory::Create("BF", manager->GetPlatform());
  std::shared_ptr<OperatingPointEvaluator> evaluator =
      OperatingPointEvaluatorFactory::Create("energy");
  mapper->SetOperatingPointEvaluator(evaluator);

  auto s1 = mapper->GenerateClientMapping({clients[0]});
  LOGGER->debug("%s", s1.ToString().c_str());
  auto s1_op = s1.Get(clients[0]);
  EXPECT_EQ(s1_op.threads(), CPUThreadSet({0}));
  EXPECT_EQ(s1.GetAllThreads(), CPUThreadSet({0}));
  auto s1_result = s1.EvaluateWith(*evaluator);
  EXPECT_EQ(std::get<0>(s1_result), 1);
  EXPECT_NEAR(std::get<1>(s1_result), 0.697, 0.001);

  auto s2 = mapper->GenerateClientMapping({clients[0]}, CPUCoreSet{0});
  LOGGER->debug("%s", s2.ToString().c_str());
  auto s2_op = s2.Get(clients[0]);
  EXPECT_EQ(s2_op.threads(), CPUThreadSet({1}));
  EXPECT_EQ(s2.GetAllThreads(), CPUThreadSet({1}));
  auto s2_result = s2.EvaluateWith(*evaluator);
  EXPECT_EQ(std::get<0>(s2_result), 1);
  EXPECT_NEAR(std::get<1>(s2_result), 0.697, 0.001);

  auto s3 = mapper->GenerateClientMapping({clients[0]}, CPUCoreSet{0, 1});
  LOGGER->debug("%s", s3.ToString().c_str());
  auto s3_op = s3.Get(clients[0]);
  EXPECT_EQ(s3_op.threads(), CPUThreadSet({2}));
  EXPECT_EQ(s3.GetAllThreads(), CPUThreadSet({2}));
  auto s3_result = s3.EvaluateWith(*evaluator);
  EXPECT_EQ(std::get<0>(s3_result), 1);
  EXPECT_NEAR(std::get<1>(s3_result), 1.5, 0.001);
}

TEST_F(SmallOdroidClientMappingTest, BruteforceMapper_TwoJobs) {
  auto mapper = ClientMapperFactory::Create("BF", manager->GetPlatform());
  std::shared_ptr<OperatingPointEvaluator> energy_evaluator =
      OperatingPointEvaluatorFactory::Create("energy");
  mapper->SetOperatingPointEvaluator(energy_evaluator);

  auto s1 = mapper->GenerateClientMapping({clients[0], clients[1]});
  LOGGER->debug("%s", s1.ToString().c_str());
  auto s1_op0 = s1.Get(clients[0]);
  auto s1_op1 = s1.Get(clients[1]);
  EXPECT_EQ(s1_op0.threads(), CPUThreadSet({0}));
  EXPECT_EQ(s1_op1.threads(), CPUThreadSet({1, 2}));
  EXPECT_EQ(s1.GetAllThreads(), CPUThreadSet({0, 1, 2}));
  auto s1_result = s1.EvaluateWith(*energy_evaluator);
  EXPECT_EQ(std::get<0>(s1_result), 2);
  EXPECT_NEAR(std::get<1>(s1_result), 2.126, 0.001);

  // Set another objective
  std::shared_ptr<OperatingPointEvaluator> performance_evaluator =
      OperatingPointEvaluatorFactory::Create("performance");
  mapper->SetOperatingPointEvaluator(performance_evaluator);

  auto s2 = mapper->GenerateClientMapping({clients[0], clients[1]});
  LOGGER->debug("%s", s2.ToString().c_str());
  auto s2_op0 = s2.Get(clients[0]);
  auto s2_op1 = s2.Get(clients[1]);
  EXPECT_EQ(s2_op0.threads(), CPUThreadSet({2, 3}));
  EXPECT_EQ(s2_op1.threads(), CPUThreadSet({0, 1}));
  EXPECT_EQ(s2.GetAllThreads(), CPUThreadSet({0, 1, 2, 3}));
  auto s2_result = s2.EvaluateWith(*performance_evaluator);
  EXPECT_EQ(std::get<0>(s2_result), 2);
  EXPECT_NEAR(std::get<1>(s2_result), 3.252, 0.001);

  // Set another objective
  std::shared_ptr<OperatingPointEvaluator> balanced_evaluator =
      OperatingPointEvaluatorFactory::Create("balanced");
  mapper->SetOperatingPointEvaluator(balanced_evaluator);

  auto s3 = mapper->GenerateClientMapping({clients[0], clients[1]});
  LOGGER->debug("%s", s3.ToString().c_str());
  auto s3_op0 = s3.Get(clients[0]);
  auto s3_op1 = s3.Get(clients[1]);
  EXPECT_EQ(s3_op0.threads(), CPUThreadSet({0, 2}));
  EXPECT_EQ(s3_op1.threads(), CPUThreadSet({1, 3}));
  EXPECT_EQ(s3.GetAllThreads(), CPUThreadSet({0, 1, 2, 3}));
  auto s3_result = s3.EvaluateWith(*balanced_evaluator);
  EXPECT_EQ(std::get<0>(s3_result), 2);
  EXPECT_NEAR(std::get<1>(s3_result), 3.987, 0.001);
}

TEST_F(SmallOdroidClientMappingTest, BruteforceMapper_ThreeJobs) {
  auto mapper = ClientMapperFactory::Create("BF", manager->GetPlatform());
  std::shared_ptr<OperatingPointEvaluator> energy_evaluator =
      OperatingPointEvaluatorFactory::Create("energy");
  mapper->SetOperatingPointEvaluator(energy_evaluator);

  auto client_mapping = mapper->GenerateClientMapping(clients);
  LOGGER->debug("%s", client_mapping.ToString().c_str());
  auto op0 = client_mapping.Get(clients[0]);
  auto op1 = client_mapping.Get(clients[1]);
  auto op2 = client_mapping.Get(clients[2]);
  EXPECT_EQ(op0.threads(), CPUThreadSet({2}));
  EXPECT_EQ(op1.threads(), CPUThreadSet({0, 3}));
  EXPECT_EQ(op2.threads(), CPUThreadSet({1}));
  EXPECT_EQ(client_mapping.GetAllThreads(), CPUThreadSet({0, 1, 2, 3}));
  auto client_map_result = client_mapping.EvaluateWith(*energy_evaluator);
  EXPECT_EQ(std::get<0>(client_map_result), 3);
  EXPECT_NEAR(std::get<1>(client_map_result), 5.152, 0.001);
}

TEST_F(SmallOdroidClientMappingTest,
       BruteforceMapper_SimulateThreeJobsWithTraceLogger) {
  std::chrono::high_resolution_clock::time_point zero_time;

  TraceLogger logger(zero_time);
  logger.RegisterPlatform(manager->GetPlatform());

  auto mapper = ClientMapperFactory::Create("BF", manager->GetPlatform());
  std::shared_ptr<OperatingPointEvaluator> balanced_evaluator =
      OperatingPointEvaluatorFactory::Create("balanced");
  mapper->SetOperatingPointEvaluator(balanced_evaluator);

  std::vector<Client *> active = clients;

  for (auto &c : active) {
    logger.RegisterClient(c);
  }

  auto s1 = mapper->GenerateClientMapping(active);
  LOGGER->debug("%s", s1.ToString().c_str());

  // assign operating points
  for (auto &c : active) {
    c->active_op = s1.Get(c);
    logger.LogClientMappingBegin(zero_time, c, *c->active_op);
  }

  auto time_34ms = zero_time + std::chrono::milliseconds(34);
  for (auto &c : active) {
    logger.LogClientMappingEnd(time_34ms, c);
    if (c->exec == "app3")
      logger.DeregisterClient(c);
  }

  active.erase(
      std::remove_if(active.begin(), active.end(),
                     [](const Client *c) { return c->exec == "app3"; }),
      active.end());

  auto s2 = mapper->GenerateClientMapping(active);
  LOGGER->debug("%s", s2.ToString().c_str());

  // assign operating points
  for (auto &c : active) {
    c->active_op = s2.Get(c);
    logger.LogClientMappingBegin(time_34ms, c, *c->active_op);
  }

  auto time_63ms = zero_time + std::chrono::milliseconds(63);
  for (auto &c : active) {
    logger.LogClientMappingEnd(time_63ms, c);
    logger.DeregisterClient(c);
  }

  logger.ExportToFile("unit_test.trace.json");
}
