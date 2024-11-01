#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cstring>
#include <nlohmann/json.hpp>
#include <iomanip>

using json = nlohmann::json;

json extract_metadata(std::string hty_file_path) {
    std::ifstream file(hty_file_path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        std::cerr << "Error opening file" << std::endl;
        return json();
    }

    std::streamsize size = file.tellg();
    file.seekg(-static_cast<int>(sizeof(int)), std::ios::end);
    int metadata_size;
    file.read(reinterpret_cast<char*>(&metadata_size), sizeof(int));

    file.seekg(-static_cast<int>(sizeof(int)) - metadata_size, std::ios::end);
    std::vector<char> metadata_buffer(metadata_size);
    file.read(metadata_buffer.data(), metadata_size);

    file.close();

    return json::parse(metadata_buffer.begin(), metadata_buffer.end());
}

std::vector<float> project_single_column(json metadata, std::string hty_file_path, std::string projected_column) {
    std::ifstream file(hty_file_path, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Error opening file" << std::endl;
        return std::vector<float>();
    }

    int num_rows = metadata["num_rows"];
    int column_index = -1;
    int group_index = -1;

    for (int i = 0; i < metadata["num_groups"]; ++i) {
        auto& group = metadata["groups"][i];
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
        std::cerr << "Column not found: " << projected_column << std::endl;
        return std::vector<float>();
    }

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

std::string format_large_number(float value) {
    std::ostringstream oss;
    
    if (std::abs(value) >= 1e9) {
        double scaled = value / 1e9;
        oss << std::fixed << std::setprecision(5) << scaled;
        std::string result = oss.str();
        
        // Remove trailing zeros
        result.erase(result.find_last_not_of('0') + 1, std::string::npos);
        if (result.back() == '.') {
            result.pop_back();
        }
        
        result += "e+09";
        return result;
    } else {
        oss << std::fixed << std::setprecision(1) << value;
        return oss.str();
    }
}

void display_column(json metadata, std::string column_name, std::vector<float> data) {
    std::cout << column_name << std::endl;
    for (const auto& value : data) {
        std::cout << format_large_number(value) << std::endl;
    }
}

int main() {
    std::string hty_file_path;
    std::cin >> hty_file_path;

    json metadata = extract_metadata(hty_file_path);

    std::string column_name;
    std::cin >> column_name;

    std::vector<float> column_data = project_single_column(metadata, hty_file_path, column_name);
    display_column(metadata, column_name, column_data);

    return 0;
}