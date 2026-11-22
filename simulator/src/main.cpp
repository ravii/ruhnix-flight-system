/**
 * rfs_sim: flies a scripted maneuver and writes a CSV trace to stdout for plotting.
 *
 *   build/host/simulator/rfs_sim > trace.csv                      algorithm loop
 *   build/host/simulator/rfs_sim --duration 3 --decimate 1 > trace.csv
 *   build/host/simulator/rfs_sim --udp 127.0.0.1:14560 > trace.csv  SITL firmware
 *
 * The flight controller is either the flight-controller core in-process
 * (default; no RTOS) or the SITL firmware (FreeRTOS POSIX port, bsp/sitl) over
 * UDP. Both produce the same trace: scripts/sitl.sh --check compares them.
 *
 * Script: on the ground, disarmed; armed at 0.5 s and climbing until 1.5 s,
 * then hover throttle and a half-stick doublet (+ then -) on roll, pitch and
 * yaw, so the attitude comes back level in this rate (acro) mode. Hover
 * throttle is computed from the model; there is no altitude hold yet, so the
 * climb only decays through drag.
 */
#include <cerrno>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "rfs/fc/flight_control.hpp"
#include "rfs/sim/imu_sensor_model.hpp"
#include "rfs/sim/in_process_link.hpp"
#include "rfs/sim/quadrotor_model.hpp"
#include "rfs/sim/sim_math.hpp"
#include "rfs/sim/simulation.hpp"
#include "rfs/sim/udp_link.hpp"

namespace
{

using rfs::sim::Simulation;

struct Options
{
    double duration_s = 6.0;
    int decimate = 10;     ///< write every Nth step
    std::string udp_host;  ///< empty: in-process flight controller
    uint16_t udp_port = 0;
    double connect_timeout_s = 10.0;
};

void usage()
{
    std::fprintf(stderr,
        "usage: rfs_sim [--duration SECONDS] [--decimate N] [--udp [HOST:]PORT]\n"
        "               [--connect-timeout SECONDS]\n"
        "Writes a CSV trace of a scripted flight to stdout. Without --udp the\n"
        "flight-controller core runs in-process; with --udp the simulator drives\n"
        "the SITL firmware (default host 127.0.0.1).\n");
}

bool parse(int argc, char** argv, Options& opt)
{
    for (int i = 1; i < argc; ++i)
    {
        const bool has_value = i + 1 < argc;
        if (std::strcmp(argv[i], "--duration") == 0 && has_value)
        {
            opt.duration_s = std::atof(argv[++i]);
        }
        else if (std::strcmp(argv[i], "--decimate") == 0 && has_value)
        {
            opt.decimate = std::atoi(argv[++i]);
        }
        else if (std::strcmp(argv[i], "--udp") == 0 && has_value)
        {
            const std::string arg = argv[++i];
            const std::size_t colon = arg.rfind(':');
            opt.udp_host = colon == std::string::npos ? "127.0.0.1" : arg.substr(0, colon);
            const int port = std::atoi(arg.c_str() + (colon == std::string::npos ? 0 : colon + 1));
            if (port <= 0 || port > 65535)
            {
                return false;
            }
            opt.udp_port = static_cast<uint16_t>(port);
        }
        else if (std::strcmp(argv[i], "--connect-timeout") == 0 && has_value)
        {
            opt.connect_timeout_s = std::atof(argv[++i]);
        }
        else
        {
            return false;
        }
    }
    return opt.duration_s > 0.0 && opt.decimate > 0;
}

/**
 * The rate setpoint the flight controller computes from the sticks (same as
 * FlightControl::debug() with the default config). Computed here so that the
 * trace does not depend on access to the flight controller's internals.
 */
rfs::sim::Vec3d rate_setpoint(const Simulation::RcChannels& rc)
{
    if (rc[rfs::fc::kRcArm] <= 1500)
    {
        return {};
    }
    const rfs::fc::FlightControlConfig config;
    const uint16_t sticks[3] = {rc[rfs::fc::kRcRoll], rc[rfs::fc::kRcPitch], rc[rfs::fc::kRcYaw]};
    rfs::sim::Vec3d sp{};
    for (std::size_t k = 0; k < 3; ++k)
    {
        sp[k] = static_cast<double>(rfs::fc::rc_to_bipolar(sticks[k]) * config.max_rate_rad_s[k]);
    }
    return sp;
}

/** Half-stick doublet: + for 0.3 s from t0, then - for 0.3 s. */
uint16_t doublet(double t, double t0)
{
    if (t >= t0 && t < t0 + 0.3)
    {
        return 1750;
    }
    if (t >= t0 + 0.3 && t < t0 + 0.6)
    {
        return 1250;
    }
    return 1500;
}

/** Pilot input at time t. */
Simulation::RcChannels script(double t, uint16_t hover_us)
{
    Simulation::RcChannels rc{};
    rc.fill(1500);
    rc[rfs::fc::kRcArm] = t >= 0.5 ? 2000 : 1000;
    rc[rfs::fc::kRcThrottle] = t < 0.5 ? 1000 : (t < 1.5 ? 1700 : hover_us);
    rc[rfs::fc::kRcRoll] = doublet(t, 2.0);
    rc[rfs::fc::kRcPitch] = doublet(t, 3.0);
    rc[rfs::fc::kRcYaw] = doublet(t, 4.0);
    return rc;
}

}  // namespace

int main(int argc, char** argv)
{
    Options opt;
    if (!parse(argc, argv, opt))
    {
        usage();
        return 2;
    }

    rfs::sim::QuadrotorModel model;
    rfs::sim::ImuSensorModel sensors;
    rfs::fc::FlightControl control;
    rfs::sim::InProcessLink in_process(control);
    rfs::sim::UdpLink udp;
    const bool sitl = !opt.udp_host.empty();
    if (sitl && !udp.open(opt.udp_host, opt.udp_port))
    {
        std::fprintf(stderr, "rfs_sim: cannot open udp link to %s:%u: %s\n", opt.udp_host.c_str(),
            opt.udp_port, std::strerror(errno));
        return 1;
    }
    rfs::sim::SimulationConfig sim_config;
    if (sitl)
    {
        sim_config.link_timeout = std::chrono::milliseconds(200);
    }
    Simulation sim(
        model, sensors, sitl ? static_cast<rfs::sim::FcLink&>(udp) : in_process, sim_config);

    const double idle = static_cast<double>(rfs::fc::FlightControlConfig{}.idle_thrust);
    const auto hover_us = static_cast<uint16_t>(
        std::lround(1000.0 + 1000.0 * (model.hover_command() - idle) / (1.0 - idle)));

    std::printf(
        "t_s,x_m,y_m,z_m,vx_m_s,vy_m_s,vz_m_s,roll_rad,pitch_rad,yaw_rad,"
        "p_rad_s,q_rad_s,r_rad_s,p_sp_rad_s,q_sp_rad_s,r_sp_rad_s,m0,m1,m2,m3\n");
    const auto steps = static_cast<long>(opt.duration_s * 1000.0 + 0.5);
    for (long i = 0; i < steps; ++i)
    {
        const double t = static_cast<double>(sim.time_us()) * 1e-6;
        const Simulation::RcChannels rc = script(t, hover_us);
        sim.set_rc(rc);
        bool answered = sim.step();
        if (!answered && i == 0 && sitl)
        {
            // The firmware may still be starting: keep offering the first step.
            std::fprintf(stderr, "rfs_sim: waiting for the SITL firmware on %s:%u\n",
                opt.udp_host.c_str(), opt.udp_port);
            const auto tries = static_cast<long>(opt.connect_timeout_s / 0.2);
            for (long k = 0; k < tries && !answered; ++k)
            {
                answered = sim.step();
            }
        }
        if (!answered)
        {
            std::fprintf(stderr, "rfs_sim: flight controller did not answer at t = %.3f s\n", t);
            return 1;
        }
        if (i % opt.decimate != 0)
        {
            continue;
        }
        const rfs::sim::VehicleState& s = sim.state();
        const rfs::sim::Vec3d euler = rfs::sim::quat_to_euler(s.attitude_wxyz);
        const rfs::sim::Vec3d sp = rate_setpoint(rc);
        const rfs::common::SimActuatorsMsg& act = sim.last_actuators();
        std::printf(
            "%.3f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.5f,%.5f,%.5f,%.5f,%.5f,%.5f,%.4f,%.4f,%.4f,"
            "%.4f,%.4f,%.4f,%.4f\n",
            static_cast<double>(sim.time_us()) * 1e-6, s.position_ned_m[0], s.position_ned_m[1],
            s.position_ned_m[2], s.velocity_ned_m_s[0], s.velocity_ned_m_s[1],
            s.velocity_ned_m_s[2], euler[0], euler[1], euler[2], s.angular_velocity_body_rad_s[0],
            s.angular_velocity_body_rad_s[1], s.angular_velocity_body_rad_s[2], sp[0], sp[1], sp[2],
            static_cast<double>(act.motors[0]), static_cast<double>(act.motors[1]),
            static_cast<double>(act.motors[2]), static_cast<double>(act.motors[3]));
    }
    if (sitl && udp.dropped() != 0)
    {
        std::fprintf(stderr, "rfs_sim: dropped %u stale or malformed replies\n", udp.dropped());
    }
    return 0;
}
