/**
 * @brief HTY file analyzer that extracts and displays column data from HTY files
 * 
 * This program reads HTY files (a binary format containing numerical data) and 
 * provides functionality to extract metadata, project specific columns, and
 * filter data based on various conditions. It supports large number formatting
 * and handles binary data with proper error checking.
 */

#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cstring>
#include <nlohmann/json.hpp>
#include <iomanip>
#include <cmath>
#include <sstream>
#include <algorithm>
#include <stdexcept>

using json = nlohmann::json;

// Constants for number formatting
#define BILLION 1e9
#define PRECISION_LARGE 5
#define PRECISION_NORMAL 2

/**
 * @brief Filter operations enumeration
 */
enum FilterOperation {
    GREATER_THAN,     // >
    GREATER_EQUAL,    // >=
    LESS_THAN,       // <
    LESS_EQUAL,      // <=
    EQUAL,           // =
    NOT_EQUAL        // !=
};

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

    try {
        // Read metadata size from end of file
        std::streamsize file_size = file.tellg();
        file.seekg(-static_cast<int>(sizeof(int)), std::ios::end);
        int metadata_size;
        file.read(reinterpret_cast<char*>(&metadata_size), sizeof(int));

        // Read metadata content
        file.seekg(-static_cast<int>(sizeof(int)) - metadata_size, std::ios::end);
        std::vector<char> metadata_buffer(metadata_size);
        file.read(metadata_buffer.data(), metadata_size);
        file.close();

        std::string metadata_str(metadata_buffer.begin(), metadata_buffer.end());
        return json::parse(metadata_str);
    } catch (const std::exception& e) {
        std::cerr << "Error parsing metadata: " << e.what() << std::endl;
        return json();
    }
}

/**
 * @brief Gets column information including index and group
 * @param[in] metadata JSON metadata of the HTY file
 * @param[in] column_name Name of the column to find
 * @return pair of group_index and column_index, (-1,-1) if not found
 */
std::pair<int, int> get_column_info(const json& metadata, const std::string& column_name) {
    if (column_name.empty()) {
        std::cerr << "Error: Empty column name" << std::endl;
        return {-1, -1};
    }

    for (int i = 0; i < metadata["num_groups"]; ++i) {
        const auto& group = metadata["groups"][i];
        for (int j = 0; j < group["columns"].size(); ++j) {
            if (group["columns"][j]["column_name"] == column_name) {
                return {i, j};
            }
        }
    }
    
    std::cerr << "Error: Column not found: " << column_name << std::endl;
    return {-1, -1};
}

/**
 * @brief Formats large numbers with appropriate precision
 * @param[in] value Number to format
 * @return Formatted string representation of the number
 */
std::string format_large_number(float value) {
    std::ostringstream oss;
    if (std::abs(value) >= BILLION) {
        oss << std::scientific << std::setprecision(PRECISION_LARGE) << value;
        std::string result = oss.str();
        size_t e_pos = result.find('e');
        if (e_pos != std::string::npos) {
            size_t last_nonzero = result.find_last_not_of('0', e_pos - 1);
            if (last_nonzero != std::string::npos && result[last_nonzero] == '.') {
                last_nonzero--;
            }
            if (last_nonzero != std::string::npos && last_nonzero < e_pos - 1) {
                result.erase(last_nonzero + 1, e_pos - (last_nonzero + 1));
            }
            return result;
        }
        return oss.str();
    } else if (value == static_cast<int>(value)) {
        // For integer values, show one decimal place
        oss << std::fixed << std::setprecision(1) << value;
        return oss.str();
    } else {
        // For non-integer values, show two decimal places
        oss << std::fixed << std::setprecision(2) << value;
        return oss.str();
    }
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
    std::vector<float> result;
    std::ifstream file(hty_file_path, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Error: Unable to open file: " << hty_file_path << std::endl;
        return result;
    }

    // Find column location
    auto [group_index, column_index] = get_column_info(metadata, projected_column);
    if (group_index == -1 || column_index == -1) {
        std::cerr << "Error: Column not found: " << projected_column << std::endl;
        return result;
    }

    // Read data
    const auto& group = metadata["groups"][group_index];
    int num_rows = metadata["num_rows"];
    int offset = group["offset"];
    int num_columns = group["num_columns"];
    
    result.resize(num_rows);
    for (int i = 0; i < num_rows; ++i) {
        file.seekg(offset + (i * num_columns + column_index) * sizeof(float));
        file.read(reinterpret_cast<char*>(&result[i]), sizeof(float));
    }

    file.close();
    return result;
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

/**
 * @brief Applies filter operation to a value
 * @param[in] value Value to compare
 * @param[in] operation Filter operation to apply
 * @param[in] filter_value Value to compare against
 * @return true if value passes filter, false otherwise
 */
bool apply_filter(float value, int operation, float filter_value) {
    const float EPSILON = 1e-6f;
    switch (operation) {
        case GREATER_THAN:
            return value > filter_value;
        case GREATER_EQUAL:
            return value >= filter_value;
        case LESS_THAN:
            return value < filter_value;
        case LESS_EQUAL:
            return value <= filter_value;
        case EQUAL:
            return std::abs(value - filter_value) < EPSILON;
        case NOT_EQUAL:
            return std::abs(value - filter_value) >= EPSILON;
        default:
            return false;
    }
}

/**
 * @brief Filters data based on a condition
 * @param[in] metadata JSON metadata of the HTY file
 * @param[in] hty_file_path Path to the HTY file
 * @param[in] filtered_column Name of the column to filter
 * @param[in] operation Filter operation to apply
 * @param[in] filtered_value Value to filter against
 * @return Vector of filtered values
 */
std::vector<float> filter(const json& metadata,
                         const std::string& hty_file_path,
                         const std::string& filtered_column,
                         int operation,
                         float filtered_value) {
    std::vector<float> result;
    
    // Get all values from the column
    auto column_data = project_single_column(metadata, hty_file_path, filtered_column);
    if (column_data.empty()) {
        return result;
    }
    
    // Apply filter
    for (const auto& value : column_data) {
        if (apply_filter(value, operation, filtered_value)) {
            result.push_back(value);
        }
    }
    
    return result;
}

/**
 * @brief Verifies if all columns are in the same group
 * @param[in] metadata JSON metadata of the HTY file
 * @param[in] columns Vector of column names to check
 * @return Group index if all columns are in same group, -1 otherwise
 */
int verify_same_group(const json& metadata, const std::vector<std::string>& columns) {
    if (columns.empty()) return -1;

    auto [first_group, first_col] = get_column_info(metadata, columns[0]);
    if (first_group == -1) return -1;

    for (size_t i = 1; i < columns.size(); ++i) {
        if (columns[i].empty()) {
            std::cerr << "Error: Empty column name at index " << i << std::endl;
            return -1;
        }
        auto [group_idx, col_idx] = get_column_info(metadata, columns[i]);
        if (group_idx == -1 || group_idx != first_group) {
            return -1;
        }
    }

    return first_group;
}

/**
 * @brief Projects multiple columns from the HTY file
 * @param[in] metadata JSON metadata of the HTY file
 * @param[in] hty_file_path Path to the HTY file
 * @param[in] projected_columns Names of the columns to project
 * @return Vector of vectors containing the projected column values
 */
std::vector<std::vector<float>> project(const json& metadata,
                                      const std::string& hty_file_path,
                                      const std::vector<std::string>& projected_columns) {
    std::vector<std::vector<float>> result;
    
    // Verify all columns are in the same group
    int group_index = verify_same_group(metadata, projected_columns);
    if (group_index == -1) {
        return result;
    }
    
    // Open file
    std::ifstream file(hty_file_path, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Error: Unable to open file: " << hty_file_path << std::endl;
        return result;
    }
    
    const auto& group = metadata["groups"][group_index];
    int num_rows = metadata["num_rows"];
    int offset = group["offset"];
    int num_columns = group["num_columns"];
    
    // Initialize result vectors
    result.resize(projected_columns.size(), std::vector<float>(num_rows));
    
    // Get column indices
    std::vector<int> column_indices;
    for (const auto& col_name : projected_columns) {
        auto [_, col_idx] = get_column_info(metadata, col_name);
        column_indices.push_back(col_idx);
    }
    
    // Read data for each row
    for (int row = 0; row < num_rows; ++row) {
        for (size_t i = 0; i < column_indices.size(); ++i) {
            int col_idx = column_indices[i];
            file.seekg(offset + (row * num_columns + col_idx) * sizeof(float));
            file.read(reinterpret_cast<char*>(&result[i][row]), sizeof(float));
        }
    }
    
    file.close();
    return result;
}

/**
 * @brief Projects columns and applies filter condition
 * @param[in] metadata JSON metadata of the HTY file
 * @param[in] hty_file_path Path to the HTY file
 * @param[in] projected_columns Names of columns to project
 * @param[in] filtered_column Name of column to filter on
 * @param[in] op Filter operation to apply
 * @param[in] value Filter value to compare against
 * @return Vector of vectors containing filtered column values
 */
std::vector<std::vector<float>> project_and_filter(const json& metadata,
                                                 const std::string& hty_file_path,
                                                 const std::vector<std::string>& projected_columns,
                                                 const std::string& filtered_column,
                                                 int op,
                                                 float value) {
    std::vector<std::vector<float>> result;
    
    // Create a combined vector of all columns we need
    std::vector<std::string> all_columns = projected_columns;
    if (std::find(all_columns.begin(), all_columns.end(), filtered_column) == all_columns.end()) {
        all_columns.push_back(filtered_column);
    }
    
    // Verify all columns are in the same group
    int group_index = verify_same_group(metadata, all_columns);
    if (group_index == -1) {
        return result;
    }
    
    // Open file
    std::ifstream file(hty_file_path, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Error: Unable to open file: " << hty_file_path << std::endl;
        return result;
    }
    
    const auto& group = metadata["groups"][group_index];
    int num_rows = metadata["num_rows"];
    int offset = group["offset"];
    int num_columns = group["num_columns"];
    
    // Get column indices
    std::vector<int> proj_indices;
    for (const auto& col_name : projected_columns) {
        auto [_, col_idx] = get_column_info(metadata, col_name);
        proj_indices.push_back(col_idx);
    }
    
    auto [_, filter_col_idx] = get_column_info(metadata, filtered_column);
    
    // Initialize result vectors (will resize as we find matching rows)
    result.resize(projected_columns.size());
    
    // Read and filter data
    float filter_value, proj_value;
    for (int row = 0; row < num_rows; ++row) {
        // Read filter column value
        file.seekg(offset + (row * num_columns + filter_col_idx) * sizeof(float));
        file.read(reinterpret_cast<char*>(&filter_value), sizeof(float));
        
        // Check if row passes filter
        if (apply_filter(filter_value, op, value)) {
            // Read all projected columns for this row
            for (size_t i = 0; i < proj_indices.size(); ++i) {
                file.seekg(offset + (row * num_columns + proj_indices[i]) * sizeof(float));
                file.read(reinterpret_cast<char*>(&proj_value), sizeof(float));
                result[i].push_back(proj_value);
            }
        }
    }
    
    file.close();
    return result;
}

/**
 * @brief Displays multiple columns of data
 * @param[in] metadata JSON metadata of the HTY file
 * @param[in] column_names Names of the columns to display
 * @param[in] result_set Vector of vectors containing the column values
 */
void display_result_set(const json& metadata,
                       const std::vector<std::string>& column_names,
                       const std::vector<std::vector<float>>& result_set) {
    if (result_set.empty() || column_names.empty()) return;
    
    // Print header
    for (size_t i = 0; i < column_names.size(); ++i) {
        std::cout << column_names[i];
        if (i < column_names.size() - 1) std::cout << ",";
    }
    std::cout << std::endl;
    
    // Print data rows
    for (size_t row = 0; row < result_set[0].size(); ++row) {
        for (size_t col = 0; col < result_set.size(); ++col) {
            std::cout << format_large_number(result_set[col][row]);
            if (col < result_set.size() - 1) std::cout << ",";
        }
        std::cout << std::endl;
    }
}

/**
 * @brief Validates row data against metadata requirements
 * @param[in] metadata JSON metadata of the HTY file
 * @param[in] rows Vector of row data to validate
 * @return true if valid, false otherwise
 */
bool validate_rows(const json& metadata, const std::vector<std::vector<float>>& rows) {
    if (rows.empty()) {
        std::cerr << "Error: No rows provided" << std::endl;
        return false;
    }

    int total_columns = 0;
    for (const auto& group : metadata["groups"]) {
        total_columns += group["num_columns"].get<int>();
    }
    
    for (size_t i = 0; i < rows.size(); ++i) {
        if (rows[i].size() != static_cast<size_t>(total_columns)) {
            std::cerr << "Error: Row " << i << " has incorrect number of columns. "
                     << "Expected: " << total_columns 
                     << ", Got: " << rows[i].size() << std::endl;
            return false;
        }
    }
    
    return true;
}

/**
 * @brief Adds new rows to HTY file
 * @param[in] metadata JSON metadata of the HTY file
 * @param[in] hty_file_path Path to the source HTY file
 * @param[in] modified_hty_file_path Path to the destination HTY file
 * @param[in] rows Vector of vectors containing new row data
 */
void add_row(const json& metadata, 
             const std::string& hty_file_path,
             const std::string& modified_hty_file_path,
             const std::vector<std::vector<float>>& rows) {
    // Validate input rows
    if (!validate_rows(metadata, rows)) {
        return;
    }

    // Open input file for reading
    std::ifstream input_file(hty_file_path, std::ios::binary);
    if (!input_file.is_open()) {
        std::cerr << "Error: Unable to open input file: " << hty_file_path << std::endl;
        return;
    }

    // Open output file for writing
    std::ofstream output_file(modified_hty_file_path, std::ios::binary | std::ios::trunc);
    if (!output_file.is_open()) {
        std::cerr << "Error: Unable to open output file: " << modified_hty_file_path << std::endl;
        input_file.close();
        return;
    }

    try {
        // Create new metadata with updated row count
        json new_metadata = metadata;
        new_metadata["num_rows"] = metadata["num_rows"].get<int>() + rows.size();

        // Copy existing data and add new rows, group by group
        int current_offset = 0;
        int total_groups = metadata["num_groups"];

        for (int group_idx = 0; group_idx < total_groups; ++group_idx) {
            const auto& group = metadata["groups"][group_idx];
            int group_columns = group["num_columns"];
            int group_offset = group["offset"];
            
            // Update offset in new metadata
            new_metadata["groups"][group_idx]["offset"] = current_offset;

            // Calculate starting column index for this group
            int start_col = 0;
            for (int i = 0; i < group_idx; ++i) {
                start_col += metadata["groups"][i]["num_columns"].get<int>();
            }

            // Calculate group size
            int group_size = metadata["num_rows"].get<int>() * group_columns * sizeof(float);

            // Copy existing group data
            input_file.seekg(group_offset);
            std::vector<char> buffer(group_size);
            input_file.read(buffer.data(), group_size);
            output_file.write(buffer.data(), group_size);

            // Write new rows for this group
            for (const auto& row : rows) {
                for (int col = 0; col < group_columns; ++col) {
                    float value = row[start_col + col];
                    output_file.write(reinterpret_cast<const char*>(&value), sizeof(float));
                }
            }

            // Update offset for next group
            current_offset += group_size + rows.size() * group_columns * sizeof(float);
        }

        // Write new metadata
        std::string metadata_str = new_metadata.dump();
        output_file.write(metadata_str.c_str(), metadata_str.size());

        // Write metadata size
        int metadata_size = static_cast<int>(metadata_str.size());
        output_file.write(reinterpret_cast<const char*>(&metadata_size), sizeof(int));

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        input_file.close();
        output_file.close();
        return;
    }

    input_file.close();
    output_file.close();
}

/**
 * @brief Main function that handles all HTY file operations
 * @return 0 on success, 1 on error
 */
int main() {
    std::string hty_file_path;
    if (!(std::cin >> hty_file_path)) {
        std::cerr << "Error: Failed to read file path" << std::endl;
        return 1;
    }

    json metadata = extract_metadata(hty_file_path);
    if (metadata.empty()) {
        return 1;
    }

    std::string first_input;
    if (!(std::cin >> first_input)) {
        std::cout << "num_rows: " << metadata["num_rows"].get<int>() << std::endl;
        return 0;
    }

    if (first_input == "add_row") {
        std::string modified_hty_file_path;
        int num_rows;
        std::cin >> modified_hty_file_path >> num_rows;

        std::vector<std::vector<float>> rows;
        int total_columns = 0;
        for (const auto& group : metadata["groups"]) {
            total_columns += group["num_columns"].get<int>();
        }

        for (int i = 0; i < num_rows; ++i) {
            std::vector<float> row;
            for (int j = 0; j < total_columns; ++j) {
                float value;
                if (!(std::cin >> value)) {
                    std::cerr << "Error: Failed to read row data" << std::endl;
                    return 1;
                }
                row.push_back(value);
            }
            rows.push_back(row);
        }

        add_row(metadata, hty_file_path, modified_hty_file_path, rows);
        return 0;
    } else {
        try {
            int num_columns = std::stoi(first_input);
            if (num_columns <= 0) {
                std::cerr << "Error: Invalid number of columns" << std::endl;
                return 1;
            }

            std::vector<std::string> column_names;
            for (int i = 0; i < num_columns; ++i) {
                std::string col_name;
                if (!(std::cin >> col_name)) {
                    std::cerr << "Error: Failed to read column name" << std::endl;
                    return 1;
                }
                column_names.push_back(col_name);
            }

            std::string operation_str;
            if (std::cin >> operation_str) {
                float filter_value;
                std::string filter_column;

                // Read filter_value first
                if (!(std::cin >> filter_value)) {
                    std::cerr << "Error: Failed to read filter value" << std::endl;
                    return 1;
                }

                // Read filter_column with getline to handle potential whitespace
                std::string dummy;
                std::getline(std::cin, dummy); // Clear any remaining characters
                std::getline(std::cin, filter_column);
                if (filter_column.empty()) {
                    filter_column = column_names[0]; // Default to first column if not specified
                }

                int operation = std::stoi(operation_str);
                if (operation < 0 || operation > 5) {
                    std::cerr << "Error: Invalid filter operation" << std::endl;
                    return 1;
                }

                if (column_names.size() == 1 && column_names[0] == filter_column) {
                    auto filtered_data = filter(metadata, hty_file_path, filter_column, 
                                             operation, filter_value);
                    display_column(metadata, filter_column, filtered_data);
                } else {
                    auto result_set = project_and_filter(metadata, hty_file_path, 
                                                       column_names, filter_column, 
                                                       operation, filter_value);
                    display_result_set(metadata, column_names, result_set);
                }
            } else {
                if (column_names.size() == 1) {
                    auto column_data = project_single_column(metadata, hty_file_path, 
                                                          column_names[0]);
                    display_column(metadata, column_names[0], column_data);
                } else {
                    auto result_set = project(metadata, hty_file_path, column_names);
                    display_result_set(metadata, column_names, result_set);
                }
            }
        } catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << std::endl;
            return 1;
        }
    }

    return 0;
}