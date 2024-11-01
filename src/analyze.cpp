/**
 * @brief HTY file analyzer that extracts and displays column data from HTY files
 * 
 * This program reads HTY files (a binary format containing numerical data) and 
 * provides functionality to extract metadata and project specific columns. It 
 * supports large number formatting and handles binary data with proper error checking.
 */

#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cstring>
#include <nlohmann/json.hpp>
#include <iomanip>

using json = nlohmann::json;

// Constants for number formatting
#define BILLION 1e9
#define PRECISION_LARGE 5
#define PRECISION_NORMAL 1

/**
 * @brief Extracts metadata from an HTY file
 * @param[in] hty_file_path Path to the HTY file to read
 * @return json object containing the file's metadata
 */
json extract_metadata(const std::string& hty_file_path) {
    std::ifstream file(hty_file_path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        std::cerr << "Error: Unable to open file: " << hty_file_path << std::endl;
        return json();
    }

    // Read metadata size from the end of file
    std::streamsize file_size = file.tellg();
    file.seekg(-static_cast<int>(sizeof(int)), std::ios::end);
    int metadata_size;
    file.read(reinterpret_cast<char*>(&metadata_size), sizeof(int));

    // Read metadata content
    file.seekg(-static_cast<int>(sizeof(int)) - metadata_size, std::ios::end);
    std::vector<char> metadata_buffer(metadata_size);
    file.read(metadata_buffer.data(), metadata_size);

    file.close();
    return json::parse(metadata_buffer.begin(), metadata_buffer.end());
}

/**
 * @brief Projects a single column from the HTY file
 * @param[in] metadata JSON metadata of the HTY file
 * @param[in] hty_file_path Path to the HTY file
 * @param[in] projected_column Name of the column to project
 * @return Vector of float values from the specified column
 */
std::vector<float> project_single_column(const json& metadata, 
                                       const std::string& hty_file_path, 
                                       const std::string& projected_column) {
    std::ifstream file(hty_file_path, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Error: Unable to open file: " << hty_file_path << std::endl;
        return std::vector<float>();
    }

    // Find the column index and group
    int num_rows = metadata["num_rows"];
    int column_index = -1;
    int group_index = -1;

    for (int i = 0; i < metadata["num_groups"]; ++i) {
        const auto& group = metadata["groups"][i];
        for (int j = 0; j < group["columns"].size(); ++j) {
            if (group["columns"][j]["column_name"] == projected_column) {
                column_index = j;
                group_index = i;
                break;
            }
        }
        if (column_index != -1) break;
    }

    if (column_index == -1) {
        std::cerr << "Error: Column not found: " << projected_column << std::endl;
        return std::vector<float>();
    }

    // Read column data
    std::vector<float> result(num_rows);
    int offset = metadata["groups"][group_index]["offset"];
    int num_columns = metadata["groups"][group_index]["num_columns"];

    file.seekg(offset);
    for (int i = 0; i < num_rows; ++i) {
        file.seekg(offset + (i * num_columns + column_index) * sizeof(float));
        file.read(reinterpret_cast<char*>(&result[i]), sizeof(float));
    }

    file.close();
    return result;
}

/**
 * @brief Formats large numbers with appropriate precision and notation
 * @param[in] value Number to format
 * @return Formatted string representation of the number
 */
std::string format_large_number(float value) {
    std::ostringstream oss;
    
    if (std::abs(value) >= BILLION) {
        double scaled = value / BILLION;
        oss << std::fixed << std::setprecision(PRECISION_LARGE) << scaled;
        std::string result = oss.str();
        
        // Remove trailing zeros and decimal point if needed
        result.erase(result.find_last_not_of('0') + 1, std::string::npos);
        if (result.back() == '.') {
            result.pop_back();
        }
        
        result += "e+09";
        return result;
    } else {
        oss << std::fixed << std::setprecision(PRECISION_NORMAL) << value;
        return oss.str();
    }
}

/**
 * @brief Displays column data with formatted values
 * @param[in] metadata JSON metadata of the HTY file
 * @param[in] column_name Name of the column to display
 * @param[in] data Vector of column values to display
 */
void display_column(const json& metadata, 
                   const std::string& column_name, 
                   const std::vector<float>& data) {
    std::cout << column_name << std::endl;
    for (const auto& value : data) {
        std::cout << format_large_number(value) << std::endl;
    }
}

int main() {
    std::string hty_file_path;
    std::cin >> hty_file_path;

    // Extract metadata and validate file
    json metadata = extract_metadata(hty_file_path);
    if (metadata.empty()) {
        return 1;
    }

    // Get column name and display data
    std::string column_name;
    std::cin >> column_name;

    std::vector<float> column_data = project_single_column(metadata, hty_file_path, column_name);
    if (column_data.empty()) {
        return 1;
    }

    display_column(metadata, column_name, column_data);
    return 0;
}