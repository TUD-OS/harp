#include "unit/PlatformFixtures.h"

#include <chrono>
#include <memory>

#include "server/client.h"
#include "server/sched/bruteforce.h"
#include "server/sched/lr.h"
#include "server/schedule.h"
#include "util/mapping_reader.h"

using namespace tetris;

class DISABLED_RaptorLakeScheduleTest : public RaptorLakeTest {
protected:
  std::map<std::string, std::vector<OperatingPoint>> app_ops;

  Client *CreateClient(const std::string &name,
                       const std::vector<OperatingPoint> &ops) {
    ConnectionPtr conn;
    auto c = new Client(conn, nullptr);

    c->exec = name;
    for (auto &op : ops) {
      c->ops.emplace_back(op);
    }
    return c;
  }

  std::string GetMappingPath() {
    return "../examples/raptor-lake-8P16E/mappings";
  }

  void ReadOperatingPoints() {
    // Call the function to read mappings from the test directory
    auto mapping_reader = MappingReader(*platform);
    auto app_mappings = mapping_reader.read_mapping_directory(GetMappingPath());
    for (auto &[app_name, mappings] : app_mappings) {
      std::vector<OperatingPoint> ops;
      for (auto &m : mappings) {
        ops.push_back(m.op());
      }
      app_ops.emplace(app_name, ops);
    }
  }

  std::vector<OperatingPoint> GetOperatingPoints(const std::string &app_name) {
    return app_ops.at(app_name);
  }

  virtual void SetUp() {
    RaptorLakeTest::SetUp();
    ReadOperatingPoints();
  }

  Client *GetClientCG() {
    return CreateClient("CG", GetOperatingPoints("cg.C"));
  }

  Client *GetClientEP() {
    return CreateClient("EP", GetOperatingPoints("ep.C"));
  }

  Client *GetClientFT() {
    return CreateClient("FT", GetOperatingPoints("ft.C"));
  }

  Client *GetClientMG() {
    return CreateClient("MG", GetOperatingPoints("mg.C"));
  }

  Client *GetClientSP() {
    return CreateClient("SP", GetOperatingPoints("sp.B"));
  }
};

TEST_F(DISABLED_RaptorLakeScheduleTest, BF_FourClients) {
  std::vector<Client *> clients;
  clients.push_back(GetClientEP());
  clients.push_back(GetClientCG());
  clients.push_back(GetClientMG());
  clients.push_back(GetClientFT());
  BruteforceMapper mapper(*platform, std::make_unique<EnergyObjective>());

  auto start = std::chrono::high_resolution_clock::now();
  auto s = mapper.GenerateSchedule(clients, 0.0);
  auto end = std::chrono::high_resolution_clock::now();

  std::chrono::duration<double> dur = end - start;
  auto dur_s = dur.count();
  LOGGER->info("Scheduling time: %lfs\n", dur_s);

  LOGGER->info("%s", s->ToString().c_str());
}

TEST_F(DISABLED_RaptorLakeScheduleTest, LR_FourClients_Energy) {
  std::vector<Client *> clients;
  clients.push_back(GetClientEP());
  clients.push_back(GetClientCG());
  clients.push_back(GetClientMG());
  clients.push_back(GetClientFT());
  LagrangianRelaxationMapper mapper(*platform,
                                    std::make_unique<EnergyObjective>(), 500);

  auto start = std::chrono::high_resolution_clock::now();
  auto s = mapper.GenerateSchedule(clients, 0.0);
  auto end = std::chrono::high_resolution_clock::now();

  std::chrono::duration<double> dur = end - start;
  auto dur_s = dur.count();
  LOGGER->info("Scheduling time: %lfs\n", dur_s);

  LOGGER->info("%s", s->ToString().c_str());
}

TEST_F(DISABLED_RaptorLakeScheduleTest, LR_FourClients_EDP) {
  std::vector<Client *> clients;
  clients.push_back(GetClientEP());
  clients.push_back(GetClientCG());
  clients.push_back(GetClientMG());
  clients.push_back(GetClientFT());
  LagrangianRelaxationMapper mapper(*platform,
                                    std::make_unique<BalancedObjective>(), 500);

  auto start = std::chrono::high_resolution_clock::now();
  auto s = mapper.GenerateSchedule(clients, 0.0);
  auto end = std::chrono::high_resolution_clock::now();

  std::chrono::duration<double> dur = end - start;
  auto dur_s = dur.count();
  LOGGER->info("Scheduling time: %lfs\n", dur_s);

  LOGGER->info("%s", s->ToString().c_str());
}
