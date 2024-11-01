#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <iostream>
#include <cstring>
#include <nlohmann/json.hpp>
#include <cmath>
#include <regex>

using json = nlohmann::json;

bool is_number(const std::string& s) {
    static const std::regex e("^[-+]?[0-9]*\\.?[0-9]+([eE][-+]?[0-9]+)?$");
    return std::regex_match(s, e);
}

void convert_from_csv_to_hty(std::string csv_file_path, std::string hty_file_path) {
    std::ifstream csv_file(csv_file_path);
    std::ofstream hty_file(hty_file_path, std::ios::binary);

    if (!csv_file.is_open() || !hty_file.is_open()) {
        std::cerr << "Error opening files" << std::endl;
        return;
    }

    std::string line;
    std::vector<std::vector<std::string>> data;
    std::vector<std::string> header;

    // Read first line
    if (std::getline(csv_file, line)) {
        std::istringstream iss(line);
        std::string token;
        std::vector<std::string> first_row;
        while (std::getline(iss, token, ',')) {
            first_row.push_back(token);
        }

        // Check if the first row is a header or data
        bool is_header = false;
        for (const auto& item : first_row) {
            if (!is_number(item)) {
                is_header = true;
                break;
            }
        }

        if (is_header) {
            header = first_row;
        } else {
            // Generate column names
            for (size_t i = 0; i < first_row.size(); ++i) {
                header.push_back("column_" + std::to_string(i + 1));
            }
            data.push_back(first_row);
        }
    }

    // Read remaining CSV data
    while (std::getline(csv_file, line)) {
        std::vector<std::string> row;
        std::istringstream iss(line);
        std::string token;
        while (std::getline(iss, token, ',')) {
            row.push_back(token);
        }
        data.push_back(row);
    }

    // Prepare metadata
    json metadata;
    metadata["num_rows"] = data.size();
    metadata["num_groups"] = 1;
    json group;
    group["num_columns"] = header.size();
    group["offset"] = 0;
    json columns;
    for (const auto& col : header) {
        json column;
        column["column_name"] = col;
        column["column_type"] = "float"; // We'll treat all numeric columns as float for simplicity
        columns.push_back(column);
    }
    group["columns"] = columns;
    metadata["groups"].push_back(group);

    // Write raw data
    for (const auto& row : data) {
        for (size_t i = 0; i < header.size(); ++i) {
            const auto& value = (i < row.size()) ? row[i] : "";
            if (is_number(value)) {
                double double_value = std::stod(value); // Use double for higher precision
                float float_value = static_cast<float>(double_value);
                hty_file.write(reinterpret_cast<const char*>(&float_value), sizeof(float));
            } else {
                // For non-numeric values, write a placeholder (e.g., 0.0)
                float placeholder = 0.0;
                hty_file.write(reinterpret_cast<const char*>(&placeholder), sizeof(float));
            }
        }
    }

    // Write metadata
    std::string metadata_str = metadata.dump();
    hty_file.write(metadata_str.c_str(), metadata_str.size());

    // Write metadata size
    int metadata_size = metadata_str.size();
    hty_file.write(reinterpret_cast<const char*>(&metadata_size), sizeof(int));

    csv_file.close();
    hty_file.close();
}

int main() {
    std::string csv_file_path, hty_file_path;

    std::cin >> csv_file_path >> hty_file_path;

    convert_from_csv_to_hty(csv_file_path, hty_file_path);

    return 0;
}