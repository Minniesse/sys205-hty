/**
 * @brief CSV to HTY File Converter
 * 
 * This program converts CSV files to HTY binary format, which is optimized for 
 * numerical data storage. It handles automatic header detection, type conversion,
 * and metadata generation. The HTY format includes both raw data and metadata
 * describing the structure of the data.
 */

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

// Constants for file processing
#define DEFAULT_COLUMN_PREFIX "column_"
#define DEFAULT_FLOAT_TYPE "float"
#define DEFAULT_FLOAT_VALUE 0.0f

/**
 * @brief Checks if a string represents a valid number
 * @param[in] str String to check
 * @return true if string is a valid number, false otherwise
 */
bool is_number(const std::string& str) {
    static const std::regex number_regex(
        "^[-+]?[0-9]*\\.?[0-9]+([eE][-+]?[0-9]+)?$"
    );
    return std::regex_match(str, number_regex);
}

/**
 * @brief Splits a CSV line into tokens
 * @param[in] line CSV line to split
 * @return Vector of string tokens
 */
std::vector<std::string> split_csv_line(const std::string& line) {
    std::vector<std::string> tokens;
    std::istringstream iss(line);
    std::string token;
    
    while (std::getline(iss, token, ',')) {
        tokens.push_back(token);
    }
    
    return tokens;
}

/**
 * @brief Generates default column names
 * @param[in] count Number of columns to generate names for
 * @return Vector of generated column names
 */
std::vector<std::string> generate_column_names(size_t count) {
    std::vector<std::string> names;
    for (size_t i = 0; i < count; ++i) {
        names.push_back(DEFAULT_COLUMN_PREFIX + std::to_string(i + 1));
    }
    return names;
}

/**
 * @brief Creates metadata JSON object for HTY file
 * @param[in] header Column headers
 * @param[in] data_rows Data content
 * @return JSON object containing metadata
 */
json create_metadata(const std::vector<std::string>& header, 
                    const std::vector<std::vector<std::string>>& data_rows) {
    json metadata;
    metadata["num_rows"] = data_rows.size();
    metadata["num_groups"] = 1;
    
    json group;
    group["num_columns"] = header.size();
    group["offset"] = 0;
    
    json columns;
    for (const auto& col : header) {
        json column;
        column["column_name"] = col;
        column["column_type"] = DEFAULT_FLOAT_TYPE;
        columns.push_back(column);
    }
    
    group["columns"] = columns;
    metadata["groups"].push_back(group);
    
    return metadata;
}

/**
 * @brief Converts CSV file to HTY format
 * @param[in] csv_file_path Path to input CSV file
 * @param[in] hty_file_path Path to output HTY file
 */
void convert_from_csv_to_hty(const std::string& csv_file_path, 
                            const std::string& hty_file_path) {
    std::ifstream csv_file(csv_file_path);
    std::ofstream hty_file(hty_file_path, std::ios::binary);

    if (!csv_file.is_open() || !hty_file.is_open()) {
        std::cerr << "Error: Unable to open input or output file" << std::endl;
        return;
    }

    std::string line;
    std::vector<std::vector<std::string>> data;
    std::vector<std::string> header;

    // Process first line
    if (std::getline(csv_file, line)) {
        std::vector<std::string> first_row = split_csv_line(line);
        
        // Determine if first row is header
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
            header = generate_column_names(first_row.size());
            data.push_back(first_row);
        }
    }

    // Read remaining data
    while (std::getline(csv_file, line)) {
        data.push_back(split_csv_line(line));
    }

    // Create and write metadata
    json metadata = create_metadata(header, data);

    // Write data values
    for (const auto& row : data) {
        for (size_t i = 0; i < header.size(); ++i) {
            const auto& value = (i < row.size()) ? row[i] : "";
            float float_value = DEFAULT_FLOAT_VALUE;
            
            if (is_number(value)) {
                double double_value = std::stod(value);
                float_value = static_cast<float>(double_value);
            }
            
            hty_file.write(reinterpret_cast<const char*>(&float_value), 
                          sizeof(float));
        }
    }

    // Write metadata and its size
    std::string metadata_str = metadata.dump();
    hty_file.write(metadata_str.c_str(), metadata_str.size());

    int metadata_size = metadata_str.size();
    hty_file.write(reinterpret_cast<const char*>(&metadata_size), 
                   sizeof(int));

    csv_file.close();
    hty_file.close();
}

int main() {
    std::string csv_file_path, hty_file_path;
    std::cin >> csv_file_path >> hty_file_path;

    convert_from_csv_to_hty(csv_file_path, hty_file_path);
    return 0;
}