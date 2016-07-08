/**
 * @file openvdb_valgrind.cpp
 * @brief This is used for benchmark testing openvdb
 *
 * @author Levi Armstrong
 * @date Jun 29, 2016
 * @version TODO
 * @bug No known bugs
 *
 * @copyright Copyright (c) 2016, Southwest Research Institute
 *
 * @license Software License Agreement (Apache License)\n
 * \n
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at\n
 * \n
 * http://www.apache.org/licenses/LICENSE-2.0\n
 * \n
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <kdl_parser/kdl_parser.hpp>
#include <ros/ros.h>
#include <ros/package.h>
#include <ros/console.h>
#include <string.h>
#include <moveit/robot_model_loader/robot_model_loader.h>
#include <moveit/robot_model/joint_model_group.h>
#include <moveit/robot_state/conversions.h>
#include <moveit/kinematic_constraints/kinematic_constraint.h>
#include <moveit/kinematic_constraints/utils.h>
#include <moveit/collision_plugin_loader/collision_plugin_loader.h>
#include <constrained_ik/moveit_interface/joint_interpolation_planner.h>
#include <constrained_ik/ConstrainedIKPlannerDynamicReconfigureConfig.h>
#include <fstream>
#include <time.h>
#include "openvdb_distance_field.h"
#include "openvdb/tools/VolumeToSpheres.h"


using namespace ros;
using namespace constrained_ik;
using namespace Eigen;
using namespace moveit::core;
using namespace std;

typedef boost::shared_ptr<collision_detection::CollisionPlugin> CollisionPluginPtr;

int main (int argc, char *argv[])
{
  ros::init(argc, argv, "openvdb_valgrid_name");
  ros::NodeHandle pnh;

  if (ros::console::set_logger_level(ROSCONSOLE_DEFAULT_NAME, ros::console::levels::Debug))
     ros::console::notifyLoggerLevelsChanged();

  sleep(0);
  robot_model_loader::RobotModelLoaderPtr loader;
  robot_model::RobotModelPtr robot_model;
  string urdf_file_path, srdf_file_path;

  urdf_file_path = package::getPath("stomp_test_support") + "/urdf/test_kr210l150_500K.urdf";
  srdf_file_path = package::getPath("stomp_test_kr210_moveit_config") + "/config/test_kr210.srdf";

  ifstream ifs1 (urdf_file_path.c_str());
  string urdf_string((istreambuf_iterator<char>(ifs1)), (istreambuf_iterator<char>()));

  ifstream ifs2 (srdf_file_path.c_str());
  string srdf_string((istreambuf_iterator<char>(ifs2)), (istreambuf_iterator<char>()));

  robot_model_loader::RobotModelLoader::Options opts(urdf_string, srdf_string);
  loader.reset(new robot_model_loader::RobotModelLoader(opts));
  robot_model = loader->getModel();

  if (!robot_model)
  {
    ROS_ERROR_STREAM("Unable to load robot model from urdf and srdf.");
    return false;
  }

  // get robot state
  moveit::core::RobotStatePtr robot_state(new moveit::core::RobotState(robot_model));

  ros::Time start;
  distance_field::OpenVDBDistanceField df(0.02);

  // Add sphere for testing distance
  ROS_ERROR("Adding sphere to distance field.");
  start = ros::Time::now();
  shapes::Sphere *sphere1 = new shapes::Sphere(0.5);
  Eigen::Affine3d pose1 = Eigen::Affine3d::Identity();
  pose1.translation() = Eigen::Vector3d(4, 4, 4);
  df.addShapeToField(sphere1, pose1, 0.5/0.01, 0.5/0.01);
  ROS_ERROR("Time Elapsed: %f (sec)",(ros::Time::now() - start).toSec());

  // Add second sphere for testing distance
  ROS_ERROR("Adding sphere to distance field.");
  start = ros::Time::now();
  shapes::Sphere *sphere2 = new shapes::Sphere(0.5);
  Eigen::Affine3d pose2 = Eigen::Affine3d::Identity();
  pose2.translation() = Eigen::Vector3d(4, 4, 5.5);
  df.addShapeToField(sphere2, pose2, 0.5/0.01, 0.5/0.01);
  ROS_ERROR("Time Elapsed: %f (sec)",(ros::Time::now() - start).toSec());

  double dist, t;
  Eigen::Vector3f pick_point(4.0, 4.0, 4.4), gradient;

  t=0;
  for(int i = 0; i < 10; ++i)
  {
    start = ros::Time::now();
//    dist = df.getDistanceGradient(pick_point, gradient);
    dist = df.getDistance(pick_point);
    t+=(ros::Time::now() - start).toSec();
  }

  ROS_ERROR("Background: %f", df.getGrid()->background());
  ROS_ERROR("Distance: %f", dist);
  ROS_ERROR("Gradient: %f %f %f", gradient(0), gradient(1), gradient(2));
  ROS_ERROR("Average Time Elapsed: %0.8f (sec)",t/10.0);

  // Add Robot to distance field
//  df.addRobotToField(robot_state);

  // Write distance field to file
  df.writeToFile("/home/larmstrong/test.vdb");
  ROS_ERROR("Bytes: %i", int(df.getGrid()->memUsage()));


// This tests the ClosestSurfacePoint which is about 10-30 times slower
// than just quering a distance grid, but the distace can be found when
// requesting a point outside the bandwidth. Though this does not return
// a negative distance when quering a point inside an object.
  openvdb::tools::ClosestSurfacePoint<openvdb::FloatGrid> csp;

  csp.initialize<openvdb::util::NullInterrupter>(*df.getGrid());
  std::vector<openvdb::Vec3R> points;
  std::vector<float> distance;

  points.push_back(openvdb::Vec3R(pick_point[0], pick_point[1], pick_point[2]));

  t=0;
  for(int i=0; i<10; i++)
  {
    distance.clear();
    start = ros::Time::now();
    csp.search(points, distance);
    dist = distance[0];
    t+=(ros::Time::now() - start).toSec();
  }

  ROS_ERROR("Background: %f", df.getGrid()->background());
  ROS_ERROR("Distance: %f", dist);
  ROS_ERROR("Average Time Elapsed: %0.8f (sec)",t/10.0);
  return 0;
}
