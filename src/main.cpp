#include <Eigen/Dense>
#include <fstream>
#include <iostream>
#include <memory>
#include <vector>
#include <string_view>

#include "yaml-cpp/yaml.h"

#include "bond.h"
#include "constants.h"
#include "file_io.h"
#include "velocity_verlet.h"

const std::string DEFAULT_CONFIG_FILE{"config.yaml"};

int main(int argc, char** argv) {
    std::string config_filename;
    if (argc > 1) {
        config_filename = argv[1];
    } else {
        config_filename = DEFAULT_CONFIG_FILE;
    }
    YAML::Node config;
    try {
         config = YAML::LoadFile(config_filename);
    } catch (const YAML::BadFile& ex) {
         std::cerr << "Could not open " << config_filename << ". Is it present in this directory?\n";
         throw ex;
    }
    const std::string positionfile = config["positionfile"].as<std::string>();
    const std::string bondfile = config["bondfile"].as<std::string>();
    const std::string outputfile = config["outputfile"]["filename"].as<std::string>();
    const int output_frequency = config["outputfile"]["frequency"].as<int>();
    const double timestep = config["simulation"]["timestep"].as<double>();
    const int number_steps = config["simulation"]["steps"].as<int>();

    constexpr auto unit_type = UnitType::ARBITRARY;
    auto [positions, atom_names] = load_positions(positionfile, unit_type, 3);
    auto bonds = load_bonds(bondfile, positions, atom_names);
    Eigen::VectorXd masses = load_masses(atom_names, unit_type);
    Eigen::MatrixXd velocities = Eigen::MatrixXd::Zero(positions.rows(), positions.cols());
    Eigen::MatrixXd accelerations = Eigen::MatrixXd::Zero(positions.rows(), positions.cols());

    for (const auto& excitement : config["excitements"]) {
        const auto atom_a_name = excitement["atoms"][0].as<std::string>();
        const int atom_a_id = atom_name_to_id(atom_names.begin(), atom_names.end(), atom_a_name);

        const auto atom_b_name = excitement["atoms"][1].as<std::string>();
        const int atom_b_id = atom_name_to_id(atom_names.begin(), atom_names.end(), atom_b_name);

        // Now find the correct bond.
        auto correct_bond = std::find_if(bonds.begin(), bonds.end(), [atom_a_id, atom_b_id](const auto& bond){
            return (bond->atoms[0] == atom_a_id) && (bond->atoms[1] == atom_b_id);
        });
        excite_bond(*correct_bond, positions, excitement["factor"].as<double>());

        std::cout << "Exciting a bond between " << atom_a_name << " and " << atom_b_name << "\n";
    }
    std::ofstream output_file { outputfile };
    std::ofstream bond_excitement_file { "bonds.csv" };
    for (const auto& bond : bonds) {
        bond_excitement_file << atom_names[bond->atoms[0]] << "->" << atom_names[bond->atoms[1]]
                             << ",";
    }
    bond_excitement_file << "\n";
    for (int step = 0; step < number_steps; ++step) {
        velocity_verlet(positions, velocities, accelerations, masses, timestep, bonds);
        if (step % 1000 == 0 ){
            std::cout << "Step " << step << "\n";
        }
        if (step % output_frequency == 0) {
            write_xyz(outputfile, positions, velocities, accelerations, step, atom_names);
            for (const auto& bond : bonds) {
                bond_excitement_file << bond->get_excitement_factor(positions) << ",";
            }
            bond_excitement_file << "\n";
        }
    }
    return 0;
}
