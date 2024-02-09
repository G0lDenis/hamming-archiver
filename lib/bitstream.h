#pragma once

#include <string>
#include <vector>

#define HEADER_SIZE 15
#define HEADER_SIZE_WITHOUT_CODING 11
#define INCLUDED_FILE_NAME_SIZE 1
#define INCLUDED_FILE_SIZE 4
#define DEFAULT_LENGTH 11

uint8_t CountAddedBits(uint8_t word);

void WriteHeader(const std::vector<char>& data, std::ofstream& stream);

void WriteFiles(const std::vector<std::string>& files, std::ofstream& stream, uint8_t word_,
                const std::string& filename_end);

void CreateHaf(const std::string& output_filename, std::vector<std::string>& args, uint8_t word_,
               const std::string& filename_end);

std::tuple<uint32_t, uint32_t, uint8_t> ReadHeader(std::ifstream& stream);

std::vector<std::pair<std::string, uint32_t>> HafFilesList(const std::string& ha_file);

std::vector<std::string> ExtractHaf(const std::string& ha_file, const std::string& filename_end);

void AppendFilesToHaf(const std::string& output_filename, std::vector<std::string>& args);

void DeleteFilesFromHaf(const std::string& output_filename, std::vector<std::string>& args);

void ConcatenateHaf(const std::string& output_filename, std::vector<std::string>& args);
