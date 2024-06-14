
#include <boost/test/unit_test.hpp>

#include "simple-mpc/centroidal-dynamics.hpp"
#include "simple-mpc/fulldynamics.hpp"
#include "simple-mpc/fwd.hpp"
#include "simple-mpc/kinodynamics.hpp"
#include "simple-mpc/mpc.hpp"
#include "simple-mpc/robot-handler.hpp"
#include "test_utils.cpp"

BOOST_AUTO_TEST_SUITE(mpc)

using namespace simple_mpc;

BOOST_AUTO_TEST_CASE(mpc_fulldynamics) {
  RobotHandler handler = getTalosHandler();

  FullDynamicsSettings settings = getFullDynamicsSettings(handler);
  FullDynamicsProblem fdproblem(settings, handler);

  std::vector<ContactMap> initial_contact_sequence;
  std::vector<std::map<std::string, Eigen::VectorXd>> initial_force_sequence;
  std::size_t T = 100;
  Eigen::VectorXd f1(6);
  f1 << 0, 0, 400, 0, 0, 0;
  std::map<std::string, Eigen::VectorXd> force_refs;
  force_refs.insert({"left_sole_link", f1});
  force_refs.insert({"right_sole_link", Eigen::VectorXd::Zero(6)});
  for (std::size_t i = 0; i < T; i++) {
    std::vector<bool> contact_states = {true, true};
    StdVectorEigenAligned<Eigen::Vector3d> contact_poses = {{0, 0.1, 0},
                                                            {0, -0.1, 0}};
    ContactMap cm1(contact_states, contact_poses);
    initial_contact_sequence.push_back(cm1);
    initial_force_sequence.push_back(force_refs);
  }

  fdproblem.create_problem(settings.x0, initial_contact_sequence,
                           initial_force_sequence);

  std::shared_ptr<Problem> problem =
      std::make_shared<FullDynamicsProblem>(fdproblem);

  MPCSettings mpc_settings;
  mpc_settings.nq = handler.get_rmodel().nq;
  mpc_settings.nv = handler.get_rmodel().nv;
  mpc_settings.nu = handler.get_rmodel().nv - 6;
  mpc_settings.totalSteps = 4;
  mpc_settings.T = T;
  mpc_settings.ddpIteration = 1;

  mpc_settings.min_force = 150;
  mpc_settings.support_force = 1000;

  mpc_settings.TOL = 1e-4;
  mpc_settings.mu_init = 1e-8;

  mpc_settings.num_threads = 2;

  MPC mpc = MPC(mpc_settings, handler, problem, settings.x0);

  BOOST_CHECK_EQUAL(mpc.get_xs().size(), T + 1);
  BOOST_CHECK_EQUAL(mpc.get_us().size(), T);

  std::vector<ContactMap> contact_sequence;
  std::vector<std::map<std::string, Eigen::VectorXd>> force_sequence;
  Eigen::VectorXd f2 = Eigen::VectorXd::Zero(6);
  std::map<std::string, Eigen::VectorXd> force_refs2;
  force_refs2.insert({"left_sole_link", f1});
  force_refs2.insert({"right_sole_link", f2});
  std::map<std::string, Eigen::VectorXd> force_refs3;
  force_refs3.insert({"left_sole_link", f2});
  force_refs3.insert({"right_sole_link", f1});
  for (std::size_t i = 0; i < 10; i++) {
    std::vector<bool> contact_states = {true, true};
    StdVectorEigenAligned<Eigen::Vector3d> contact_poses = {{0, 0.1, 0},
                                                            {0, -0.1, 0}};
    ContactMap cm1(contact_states, contact_poses);
    contact_sequence.push_back(cm1);
    force_sequence.push_back(force_refs);
  }
  for (std::size_t i = 0; i < 50; i++) {
    std::vector<bool> contact_states = {true, false};
    StdVectorEigenAligned<Eigen::Vector3d> contact_poses = {{0, 0.1, 0},
                                                            {0, -0.1, 0}};
    ContactMap cm1(contact_states, contact_poses);
    contact_sequence.push_back(cm1);
    force_sequence.push_back(force_refs2);
  }
  for (std::size_t i = 0; i < 10; i++) {
    std::vector<bool> contact_states = {true, true};
    StdVectorEigenAligned<Eigen::Vector3d> contact_poses = {{0, 0.1, 0},
                                                            {0.5, -0.1, 0}};
    ContactMap cm1(contact_states, contact_poses);
    contact_sequence.push_back(cm1);
    force_sequence.push_back(force_refs);
  }
  for (std::size_t i = 0; i < 50; i++) {
    std::vector<bool> contact_states = {false, true};
    StdVectorEigenAligned<Eigen::Vector3d> contact_poses = {{0, 0.1, 0},
                                                            {0.5, -0.1, 0}};
    ContactMap cm1(contact_states, contact_poses);
    contact_sequence.push_back(cm1);
    force_sequence.push_back(force_refs3);
  }
  for (std::size_t i = 0; i < 10; i++) {
    std::vector<bool> contact_states = {true, true};
    StdVectorEigenAligned<Eigen::Vector3d> contact_poses = {{0.5, 0.1, 0},
                                                            {0.5, -0.1, 0}};
    ContactMap cm1(contact_states, contact_poses);
    contact_sequence.push_back(cm1);
    force_sequence.push_back(force_refs);
  }

  mpc.generateFullHorizon(contact_sequence, force_sequence);

  BOOST_CHECK_EQUAL(mpc.get_fullHorizon().size(), 130);
  BOOST_CHECK_EQUAL(mpc.get_fullHorizonData().size(), 130);

  for (std::size_t i = 0; i < 50; i++)
    mpc.recedeWithCycle();

  BOOST_CHECK_EQUAL(
      mpc.get_problem()->get_reference_force(80, "right_sole_link"), f2);
}

BOOST_AUTO_TEST_SUITE_END()