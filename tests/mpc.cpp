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

  size_t T = 100;
  fdproblem.createProblem(handler.getState(), T, 6, -settings.gravity[2]);
  std::shared_ptr<Problem> problem =
      std::make_shared<FullDynamicsProblem>(fdproblem);

  MPCSettings mpc_settings;
  mpc_settings.ddpIteration = 1;

  mpc_settings.support_force = -handler.getMass() * settings.gravity[2];

  mpc_settings.TOL = 1e-6;
  mpc_settings.mu_init = 1e-8;
  mpc_settings.num_threads = 1;

  mpc_settings.swing_apex = 0.1;
  mpc_settings.T_fly = 80;
  mpc_settings.T_contact = 20;
  mpc_settings.T = T;
  mpc_settings.dt = 0.01;

  MPC mpc = MPC(mpc_settings, problem);

  BOOST_CHECK_EQUAL(mpc.xs_.size(), T + 1);
  BOOST_CHECK_EQUAL(mpc.us_.size(), T);

  std::vector<std::map<std::string, bool>> contact_states;
  for (std::size_t i = 0; i < 10; i++) {
    std::map<std::string, bool> contact_state;
    contact_state.insert({handler.getFootName(0), true});
    contact_state.insert({handler.getFootName(1), true});
    contact_states.push_back(contact_state);
  }
  for (std::size_t i = 0; i < 50; i++) {
    std::map<std::string, bool> contact_state;
    contact_state.insert({handler.getFootName(0), true});
    contact_state.insert({handler.getFootName(1), false});
    contact_states.push_back(contact_state);
  }
  for (std::size_t i = 0; i < 10; i++) {
    std::map<std::string, bool> contact_state;
    contact_state.insert({handler.getFootName(0), true});
    contact_state.insert({handler.getFootName(1), true});
    contact_states.push_back(contact_state);
  }
  for (std::size_t i = 0; i < 50; i++) {
    std::map<std::string, bool> contact_state;
    contact_state.insert({handler.getFootName(0), false});
    contact_state.insert({handler.getFootName(1), true});
    contact_states.push_back(contact_state);
  }

  mpc.generateCycleHorizon(contact_states);

  BOOST_CHECK_EQUAL(mpc.foot_takeoff_times_.at("left_sole_link")[0], 170);
  BOOST_CHECK_EQUAL(mpc.foot_takeoff_times_.at("right_sole_link")[0], 110);
  BOOST_CHECK_EQUAL(mpc.foot_land_times_.at("left_sole_link")[0], 219);
  BOOST_CHECK_EQUAL(mpc.foot_land_times_.at("right_sole_link")[0], 160);
  for (std::size_t i = 0; i < 10; i++) {
    mpc.iterate(handler.getState().head(handler.getModel().nq),
                handler.getState().tail(handler.getModel().nv));
  }

  BOOST_CHECK_EQUAL(mpc.foot_takeoff_times_.at("left_sole_link")[0], 160);
  BOOST_CHECK_EQUAL(mpc.foot_takeoff_times_.at("right_sole_link")[0], 100);
  BOOST_CHECK_EQUAL(mpc.foot_land_times_.at("left_sole_link")[0], 209);
  BOOST_CHECK_EQUAL(mpc.foot_land_times_.at("right_sole_link")[0], 150);
}

BOOST_AUTO_TEST_CASE(mpc_kinodynamics) {
  RobotHandler handler = getTalosHandler();

  KinodynamicsSettings settings = getKinodynamicsSettings(handler);
  KinodynamicsProblem kinoproblem(settings, handler);
  std::size_t T = 100;
  double support_force = -handler.getMass() * settings.gravity[2];
  Eigen::VectorXd f1(6);
  f1 << 0, 0, support_force, 0, 0, 0;

  kinoproblem.createProblem(handler.getState(), T, 6, -settings.gravity[2]);

  std::shared_ptr<Problem> problem =
      std::make_shared<KinodynamicsProblem>(kinoproblem);

  MPCSettings mpc_settings;
  mpc_settings.ddpIteration = 1;

  mpc_settings.support_force = support_force;

  mpc_settings.TOL = 1e-6;
  mpc_settings.mu_init = 1e-8;
  mpc_settings.num_threads = 8;

  mpc_settings.swing_apex = 0.1;
  mpc_settings.T_fly = 80;
  mpc_settings.T_contact = 20;
  mpc_settings.T = T;
  mpc_settings.dt = 0.01;

  MPC mpc = MPC(mpc_settings, problem);

  BOOST_CHECK_EQUAL(mpc.xs_.size(), T + 1);
  BOOST_CHECK_EQUAL(mpc.us_.size(), T);

  std::vector<std::map<std::string, bool>> contact_states;
  // std::vector<std::vector<bool>> contact_states;
  for (std::size_t i = 0; i < 10; i++) {
    std::map<std::string, bool> contact_state;
    contact_state.insert({handler.getFootName(0), true});
    contact_state.insert({handler.getFootName(1), true});
    contact_states.push_back(contact_state);
  }
  for (std::size_t i = 0; i < 50; i++) {
    std::map<std::string, bool> contact_state;
    contact_state.insert({handler.getFootName(0), true});
    contact_state.insert({handler.getFootName(1), false});
    contact_states.push_back(contact_state);
  }
  for (std::size_t i = 0; i < 10; i++) {
    std::map<std::string, bool> contact_state;
    contact_state.insert({handler.getFootName(0), true});
    contact_state.insert({handler.getFootName(1), true});
    contact_states.push_back(contact_state);
  }
  for (std::size_t i = 0; i < 50; i++) {
    std::map<std::string, bool> contact_state;
    contact_state.insert({handler.getFootName(0), false});
    contact_state.insert({handler.getFootName(1), true});
    contact_states.push_back(contact_state);
  }

  mpc.generateCycleHorizon(contact_states);

  for (std::size_t i = 0; i < 10; i++) {
    mpc.iterate(handler.getState().head(handler.getModel().nq),
                handler.getState().tail(handler.getModel().nv));
  }
}

BOOST_AUTO_TEST_CASE(mpc_centroidal) {
  RobotHandler handler = getTalosHandler();

  CentroidalSettings settings = getCentroidalSettings();
  CentroidalProblem centproblem(settings, handler);

  std::vector<std::string> contact_names = {"left_sole_link",
                                            "right_sole_link"};
  double support_force = -handler.getMass() * settings.gravity[2];
  std::size_t T = 100;
  Eigen::VectorXd f1(6);
  f1 << 0, 0, support_force / 2., 0, 0, 0;
  Eigen::VectorXd x_multibody = handler.getState();

  centproblem.createProblem(handler.getCentroidalState(), T, 6,
                            -settings.gravity[2]);
  std::shared_ptr<Problem> problem =
      std::make_shared<CentroidalProblem>(centproblem);

  MPCSettings mpc_settings;
  mpc_settings.ddpIteration = 1;

  mpc_settings.support_force = support_force;

  mpc_settings.TOL = 1e-6;
  mpc_settings.mu_init = 1e-8;
  mpc_settings.num_threads = 8;

  mpc_settings.swing_apex = 0.1;
  mpc_settings.T_fly = 80;
  mpc_settings.T_contact = 20;
  mpc_settings.T = T;
  mpc_settings.dt = 0.01;

  MPC mpc = MPC(mpc_settings, problem);

  BOOST_CHECK_EQUAL(mpc.xs_.size(), T + 1);
  BOOST_CHECK_EQUAL(mpc.us_.size(), T);

  std::vector<std::map<std::string, bool>> contact_states;
  for (std::size_t i = 0; i < 10; i++) {
    std::map<std::string, bool> contact_state;
    contact_state.insert({handler.getFootName(0), true});
    contact_state.insert({handler.getFootName(1), true});
    contact_states.push_back(contact_state);
  }
  for (std::size_t i = 0; i < 50; i++) {
    std::map<std::string, bool> contact_state;
    contact_state.insert({handler.getFootName(0), true});
    contact_state.insert({handler.getFootName(1), false});
    contact_states.push_back(contact_state);
  }
  for (std::size_t i = 0; i < 10; i++) {
    std::map<std::string, bool> contact_state;
    contact_state.insert({handler.getFootName(0), true});
    contact_state.insert({handler.getFootName(1), true});
    contact_states.push_back(contact_state);
  }
  for (std::size_t i = 0; i < 50; i++) {
    std::map<std::string, bool> contact_state;
    contact_state.insert({handler.getFootName(0), false});
    contact_state.insert({handler.getFootName(1), true});
    contact_states.push_back(contact_state);
  }

  mpc.generateCycleHorizon(contact_states);

  for (std::size_t i = 0; i < 10; i++) {
    mpc.iterate(x_multibody.head(handler.getModel().nq),
                x_multibody.tail(handler.getModel().nv));
  }
}

BOOST_AUTO_TEST_SUITE_END()
