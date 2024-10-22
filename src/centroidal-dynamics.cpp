#include "simple-mpc/centroidal-dynamics.hpp"
#include <pinocchio/spatial/fwd.hpp>
#include <stdexcept>

namespace simple_mpc {
using namespace aligator;

CentroidalProblem::CentroidalProblem(const RobotHandler &handler)
    : Base(handler) {}

CentroidalProblem::CentroidalProblem(const CentroidalSettings &settings,
                                     const RobotHandler &handler)
    : Base(handler) {
  initialize(settings);
}

void CentroidalProblem::initialize(const CentroidalSettings &settings) {
  settings_ = settings;
  nx_ = 9;
  nu_ = (int)handler_.getFeetNames().size() * settings_.force_size;
  if (nu_ != settings_.u0.size()) {
    throw std::runtime_error("settings.u0 does not have the correct size nu");
  }
  control_ref_ = settings_.u0;
}

StageModel CentroidalProblem::createStage(
    const std::map<std::string, bool> &contact_phase,
    const std::map<std::string, pinocchio::SE3> &contact_pose,
    const std::map<std::string, Eigen::VectorXd> &contact_force,
    const std::map<std::string, bool> &land_constraint) {
  auto space = VectorSpace(nx_);
  auto rcost = CostStack(space, nu_);
  std::vector<bool> contact_states;
  StdVectorEigenAligned<Eigen::Vector3d> contact_poses;

  for (auto const &x : contact_phase) {
    contact_states.push_back(x.second);
  }
  for (auto const &x : contact_pose) {
    contact_poses.push_back(x.second.translation());
  }

  computeControlFromForces(contact_force);

  ContactMap contact_map =
      ContactMap(handler_.getFeetNames(), contact_states, contact_poses);

  auto linear_mom = LinearMomentumResidual(nx_, nu_, Eigen::Vector3d::Zero());
  auto angular_mom = AngularMomentumResidual(nx_, nu_, Eigen::Vector3d::Zero());

  auto linear_acc = CentroidalAccelerationResidual(
      space.ndx(), nu_, handler_.getMass(), settings_.gravity, contact_map,
      settings_.force_size);
  auto angular_acc = AngularAccelerationResidual(
      space.ndx(), nu_, handler_.getMass(), settings_.gravity, contact_map,
      settings_.force_size);

  rcost.addCost("control_cost",
                QuadraticControlCost(space, control_ref_, settings_.w_u));
  rcost.addCost(
      "linear_mom_cost",
      QuadraticResidualCost(space, linear_mom, settings_.w_linear_mom));
  rcost.addCost(
      "angular_mom_cost",
      QuadraticResidualCost(space, angular_mom, settings_.w_angular_mom));
  rcost.addCost(
      "linear_acc_cost",
      QuadraticResidualCost(space, linear_acc, settings_.w_linear_acc));
  rcost.addCost(
      "angular_acc_cost",
      QuadraticResidualCost(space, angular_acc, settings_.w_angular_acc));

  CentroidalFwdDynamics ode =
      CentroidalFwdDynamics(space, handler_.getMass(), settings_.gravity,
                            contact_map, settings_.force_size);
  IntegratorEuler dyn_model = IntegratorEuler(ode, settings_.DT);

  StageModel stm = StageModel(rcost, dyn_model);

  int i = 0;
  for (auto const &name : handler_.getFeetNames()) {
    if (contact_phase.at(name)) {
      if (settings_.force_size == 6) {
        CentroidalWrenchConeResidual wrench_residual =
            CentroidalWrenchConeResidual(space.ndx(), nu_, i, settings_.mu,
                                         settings_.Lfoot, settings_.Wfoot);
        stm.addConstraint(wrench_residual, NegativeOrthant());
      } else {
        CentroidalFrictionConeResidual friction_residual =
            CentroidalFrictionConeResidual(space.ndx(), nu_, i, settings_.mu,
                                           1e-4);
        stm.addConstraint(friction_residual, NegativeOrthant());
      }
    }
    i++;
  }
  return stm;
}

void CentroidalProblem::computeControlFromForces(
    const std::map<std::string, Eigen::VectorXd> &force_refs) {
  for (std::size_t i = 0; i < handler_.getFeetNames().size(); i++) {
    if (settings_.force_size != force_refs.at(handler_.getFootName(i)).size()) {
      throw std::runtime_error(
          "force size in settings does not match reference force size");
    }
    control_ref_.segment((long)i * settings_.force_size, settings_.force_size) =
        force_refs.at(handler_.getFootName(i));
  }
}

void CentroidalProblem::setReferencePoses(
    const std::size_t t,
    const std::map<std::string, pinocchio::SE3> &pose_refs) {
  if (t >= problem_->stages_.size()) {
    throw std::runtime_error("Stage index exceeds stage vector size");
  }
  if (pose_refs.size() != handler_.getFeetNames().size()) {
    throw std::runtime_error(
        "pose_refs size does not match number of end effectors");
  }
  CentroidalFwdDynamics *cent_dyn = problem_->stages_[t]
                                        ->getDynamics<IntegratorEuler>()
                                        ->getDynamics<CentroidalFwdDynamics>();

  for (auto const &pose : pose_refs) {
    cent_dyn->contact_map_.setContactPose(pose.first,
                                          pose.second.translation());
  }
  CostStack *cs = getCostStack(t);

  for (auto ee_name : handler_.getFeetNames()) {
    QuadraticResidualCost *qrc1 =
        cs->getComponent<QuadraticResidualCost>("linear_acc_cost");
    QuadraticResidualCost *qrc2 =
        cs->getComponent<QuadraticResidualCost>("angular_acc_cost");
    CentroidalAccelerationResidual *car =
        qrc1->getResidual<CentroidalAccelerationResidual>();
    AngularAccelerationResidual *aar =
        qrc2->getResidual<AngularAccelerationResidual>();
    car->contact_map_.setContactPose(ee_name,
                                     pose_refs.at(ee_name).translation());
    aar->contact_map_.setContactPose(ee_name,
                                     pose_refs.at(ee_name).translation());
  }
}

void CentroidalProblem::setReferencePose(const std::size_t t,
                                         const std::string &ee_name,
                                         const pinocchio::SE3 &pose_ref) {
  if (t >= problem_->stages_.size()) {
    throw std::runtime_error("Stage index exceeds stage vector size");
  }
  CentroidalFwdDynamics *cent_dyn = problem_->stages_[t]
                                        ->getDynamics<IntegratorEuler>()
                                        ->getDynamics<CentroidalFwdDynamics>();
  cent_dyn->contact_map_.setContactPose(ee_name, pose_ref.translation());

  CostStack *cs = getCostStack(t);
  QuadraticResidualCost *qrc1 =
      cs->getComponent<QuadraticResidualCost>("linear_acc_cost");
  QuadraticResidualCost *qrc2 =
      cs->getComponent<QuadraticResidualCost>("angular_acc_cost");
  CentroidalAccelerationResidual *car =
      qrc1->getResidual<CentroidalAccelerationResidual>();
  AngularAccelerationResidual *aar =
      qrc2->getResidual<AngularAccelerationResidual>();
  car->contact_map_.setContactPose(ee_name, pose_ref.translation());
  aar->contact_map_.setContactPose(ee_name, pose_ref.translation());
}

const pinocchio::SE3
CentroidalProblem::getReferencePose(const std::size_t t,
                                    const std::string &ee_name) {
  if (t >= problem_->stages_.size()) {
    throw std::runtime_error("Stage index exceeds stage vector size");
  }
  CentroidalFwdDynamics *cent_dyn = problem_->stages_[t]
                                        ->getDynamics<IntegratorEuler>()
                                        ->getDynamics<CentroidalFwdDynamics>();

  pinocchio::SE3 pose = pinocchio::SE3::Identity();
  pose.translation() = cent_dyn->contact_map_.getContactPose(ee_name);

  return pose;
}

void CentroidalProblem::setReferenceForces(
    const std::size_t t,
    const std::map<std::string, Eigen::VectorXd> &force_refs) {
  computeControlFromForces(force_refs);
  setReferenceControl(t, control_ref_);
}

void CentroidalProblem::setReferenceForce(const std::size_t t,
                                          const std::string &ee_name,
                                          const Eigen::VectorXd &force_ref) {
  std::vector<std::string> hname = handler_.getFeetNames();
  std::vector<std::string>::iterator it =
      std::find(hname.begin(), hname.end(), ee_name);
  long id = it - hname.begin();
  control_ref_.segment(id * settings_.force_size, settings_.force_size) =
      force_ref;
  setReferenceControl(t, control_ref_);
}

const Eigen::VectorXd
CentroidalProblem::getReferenceForce(const std::size_t t,
                                     const std::string &ee_name) {
  std::vector<std::string> hname = handler_.getFeetNames();
  std::vector<std::string>::iterator it =
      std::find(hname.begin(), hname.end(), ee_name);
  long id = it - hname.begin();

  return getReferenceControl(t).segment(id * settings_.force_size,
                                        settings_.force_size);
}

const Eigen::VectorXd CentroidalProblem::getProblemState() {
  return handler_.getCentroidalState();
}

size_t CentroidalProblem::getContactSupport(const std::size_t t) {
  CentroidalFwdDynamics *ode = problem_->stages_[t]
                                   ->getDynamics<IntegratorEuler>()
                                   ->getDynamics<CentroidalFwdDynamics>();

  size_t active_contacts = 0;
  for (auto name : handler_.getFeetNames()) {
    if (ode->contact_map_.getContactState(name)) {
      active_contacts += 1;
    }
  }
  return active_contacts;
}

CostStack CentroidalProblem::createTerminalCost() {
  auto ter_space = VectorSpace(nx_);
  auto term_cost = CostStack(ter_space, nu_);
  auto linear_mom = LinearMomentumResidual(nx_, nu_, Eigen::Vector3d::Zero());
  auto angular_mom = AngularMomentumResidual(nx_, nu_, Eigen::Vector3d::Zero());
  term_cost.addCost(
      "linear_mom_cost",
      QuadraticResidualCost(ter_space, linear_mom, settings_.w_linear_mom));
  term_cost.addCost(
      "angular_mom_cost",
      QuadraticResidualCost(ter_space, angular_mom, settings_.w_angular_mom));

  return term_cost;
}

void CentroidalProblem::createTerminalConstraint() {
  if (!problem_initialized_) {
    throw std::runtime_error("Create problem first!");
  }
  CentroidalCoMResidual com_cstr =
      CentroidalCoMResidual(ndx_, nu_, handler_.getComPosition());

  // problem_->addTerminalConstraint(com_cstr, EqualityConstraint());
  terminal_constraint_ = false;
}

void CentroidalProblem::updateTerminalConstraint(
    const Eigen::Vector3d &com_ref) {
  if (terminal_constraint_) {
    CentroidalCoMResidual *CoMres =
        problem_->term_cstrs_.getConstraint<CentroidalCoMResidual>(0);

    CoMres->setReference(com_ref);
  }
}

} // namespace simple_mpc
