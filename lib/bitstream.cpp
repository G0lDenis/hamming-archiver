#include "bitstream.h"

#include <iostream>
#include <fstream>
#include <filesystem>
#include <cmath>


// !!! Haf structure described in function CreateHaf !!!

uint8_t CountAddedBits(uint8_t word) {
    // Control bits depends on word length
    // Calculated with formula 2^added_bits_per_word >= added_bits_per_word + word_length + 1
    switch (word) {
        case 0:
            throw std::logic_error("Word length must be in range 1 ... 255");
        case 1:
            return 2;
        case 2 ... 4:
            return 3;
        case 5 ... 11:
            return 4;
        case 12 ... 26:
            return 5;
        case 27 ... 57:
            return 6;
        case 58 ... 120:
            return 7;
        case 121 ... 247:
            return 8;
        default:
            return 9;
    }
}

// Currently used for only header, maybe useful in future for not only it
void WriteHeader(const std::vector<char>& data, std::ofstream& stream) {
    const auto extra_bits = CountAddedBits(DEFAULT_LENGTH);
    std::vector<bool> bits;
    uint8_t byte = 0;
    uint16_t start_pos = 0;
    while (byte < data.size()) {

        // Adding data for coding
        // need_bytes counted with formula: (bits.size() - start_pos) + need_bytes * CHAR_BIT >= WORD_LENGTH,
        // where (bits.size() - start_pos) is remaining uncoded bits count
        auto need_bytes = std::ceil((double) (DEFAULT_LENGTH - (bits.size() - start_pos)) / CHAR_BIT);
        for (auto i = 0; (i < need_bytes) && (byte < data.size()); i++, byte++) {
            char word = data[byte];
            for (auto j = 0; j < CHAR_BIT; j++) {
                bits.emplace_back(word & 0b10000000);
                word <<= 1;
            }
        }

        // Adding extra bits
        for (uint8_t bit = 0; bit < extra_bits; bit++) {
            auto shift = (uint16_t) pow(2, bit);
            bits.emplace(bits.begin() + start_pos + shift - 1, false);
        }

        // Writing extra bits
        uint16_t end_pos = start_pos + DEFAULT_LENGTH + extra_bits;
        for (uint8_t bit = 0; bit < extra_bits; bit++) {
            auto shift = (uint16_t) pow(2, bit);
            uint8_t extra_bit = '\0';
            for (uint16_t pos = start_pos + shift - 1; pos < end_pos; pos += 2 * shift) {
                for (auto i = 0; (pos + i < end_pos) && (i < shift); i++) {
                    extra_bit ^= bits[pos + i];
                }
            }
            bits[start_pos + shift - 1] = extra_bit;
        }

        // Writing data to stream
        while (end_pos >= CHAR_BIT) {
            end_pos -= CHAR_BIT;
            char word = '\0';
            for (auto i = 0; i < CHAR_BIT; i++) {
                if (bits[0]) word++;
                bits.erase(bits.begin());
                if (i != CHAR_BIT - 1) word <<= 1;
            }
            stream << word;
        }
        start_pos = end_pos;
    }
    // No remaining bits for this data size because of
    // (HEADER_SIZE_WITHOUT_CODING * (DEFAULT_LENGTH + extra_bits)) is divisible by DEFAULT_LENGTH
    // where HEADER_SIZE_WITHOUT_CODING is 11 (described in CreateHaf)
}

// By analogy of writing data in 11-bit (previous function), but with files and TODO: with arbitrary word length
void WriteFiles(const std::vector<std::string>& files, std::ofstream& stream, const uint8_t word_,
                const std::string& filename_end) {
    const auto extra_bits = CountAddedBits(word_);
    char byte;
    for (const std::string& filename_with_path: files) {
        std::vector<bool> bits;
        uint16_t start_pos = 0;
        auto input = std::ifstream(filename_with_path + filename_end, std::ios::binary);
        if (!input.is_open()) {
            throw std::runtime_error("Failed to open " + filename_with_path += filename_end);
        }

        std::string filename = filename_with_path;
        if (filename_with_path.find_last_of("/\\") != std::string::npos)
            filename = filename_with_path.substr(filename_with_path.find_last_of("/\\") + 1);

        // Creating file header
        std::vector<char> file_header;
        uint8_t filename_size = filename.size();
        uint32_t file_size = std::filesystem::file_size(filename_with_path + filename_end);
        file_header.insert(file_header.end(),
                           (char*) &filename_size,
                           (char*) (&filename_size + sizeof(filename_size)));
        file_header.insert(file_header.end(),
                           (char*) filename.c_str(),
                           (char*) filename.c_str() + filename.size());
        file_header.insert(file_header.end(),
                           (char*) &file_size,
                           (char*) &file_size + sizeof(file_size));

        // Writing data, as Haf-header in 11-bit coding, adding header before each file
        while (!file_header.empty() || input.get(byte)) {
            if (!file_header.empty()) {
                byte = file_header[0];
                file_header.erase(file_header.begin());
            }
            for (auto j = 0; j < CHAR_BIT; j++) {
                bits.emplace_back(byte & 0b10000000);
                byte <<= 1;
            }
            if (bits.size() - start_pos < word_) continue;

            for (uint8_t bit = 0; bit < extra_bits; bit++) {
                auto shift = (uint16_t) pow(2, bit);
                bits.emplace(bits.begin() + start_pos + shift - 1, false);
            }

            uint16_t end_pos = start_pos + word_ + extra_bits;
            for (uint8_t bit = 0; bit < extra_bits; bit++) {
                auto shift = (uint16_t) pow(2, bit);
                uint8_t extra_bit = '\0';
                for (uint16_t pos = start_pos + shift - 1; pos < end_pos; pos += 2 * shift) {
                    for (auto i = 0; (pos + i < end_pos) && (i < shift); i++) {
                        extra_bit ^= bits[pos + i];
                    }
                }
                bits[start_pos + shift - 1] = extra_bit;
            }

            while (end_pos >= CHAR_BIT) {
                end_pos -= CHAR_BIT;
                char word = '\0';
                for (auto i = 0; i < CHAR_BIT; i++) {
                    if (bits[0]) word++;
                    bits.erase(bits.begin());
                    if (i != CHAR_BIT - 1) word <<= 1;
                }
                stream << word;
            }
            start_pos = end_pos;
        }
        // Writing remaining data (if we need)
        if (start_pos < bits.size()) {
            while (bits.size() - start_pos != word_) {
                bits.emplace_back(false);
            }

            for (uint8_t bit = 0; bit < extra_bits; bit++) {
                auto shift = (uint16_t) pow(2, bit);
                bits.emplace(bits.begin() + start_pos + shift - 1, false);
            }

            uint16_t end_pos = start_pos + word_ + extra_bits;
            for (uint8_t bit = 0; bit < extra_bits; bit++) {
                auto shift = (uint16_t) pow(2, bit);
                uint8_t extra_bit = '\0';
                for (uint16_t pos = start_pos + shift - 1; pos < end_pos; pos += 2 * shift) {
                    for (auto i = 0; (pos + i < end_pos) && (i < shift); i++) {
                        extra_bit ^= bits[pos + i];
                    }
                }
                bits[start_pos + shift - 1] = extra_bit;
            }
        }
        while (!bits.empty()) {
            char word = '\0';
            for (auto i = 0; i < CHAR_BIT; i++) {
                if (!bits.empty() && bits[0]) word++;
                if (!bits.empty()) bits.erase(bits.begin());
                if (i != CHAR_BIT - 1) word <<= 1;
            }
            stream << word;
        }
    }
}

void CreateHaf(const std::string& output_filename, std::vector<std::string>& args, const uint8_t word_,
               const std::string& filename_end) {

    /*
     * Structure of primary Haf consists of 2 parts: header and data
     * Header: [type_code][total_size][n_files][word_length] (total 11 bytes without coding, 15 with 11-bit coding)
     * coding_type - 2B, total_size - 4B, files_number - 4B, word_length - 1B
     * Header coding with 11-bit word length for unique decoding, other code - with arbitrary word length
     * Data: n files of structure [file_name_size][file_name][file_size][file_data] (unknown size)
     * file_name_size - 1B, file_name < 255B, file_size - 32B, file_data - unknown size
     */

    auto output_file = std::ofstream(output_filename, std::ios::binary);
    if (!output_file.is_open()) {
        throw std::runtime_error("Failed to open " + output_filename);
    }

    uint32_t primary_files_size = 0;
    uint32_t total_data_size = 0;
    for (const auto& filename: args) {
        if (!std::filesystem::is_regular_file(filename + filename_end))
            throw std::runtime_error("File [" + filename += filename_end + "] does not exist");

        primary_files_size += std::filesystem::file_size(filename + filename_end);

        uint8_t filename_size = filename.size();
        if (filename.find_last_of("/\\") != std::string::npos) filename_size -= filename.find_last_of("/\\") + 1;
        uint32_t file_size = INCLUDED_FILE_NAME_SIZE + filename_size +
                             INCLUDED_FILE_SIZE + std::filesystem::file_size(filename + filename_end);
        auto words_count = (uint32_t) std::ceil((double) (CHAR_BIT * file_size) / word_);
        total_data_size += std::ceil((double) (words_count * (word_ + CountAddedBits(word_))) / CHAR_BIT);
    }

    uint32_t total_haf_size = HEADER_SIZE + total_data_size;
    uint32_t files_number = args.size();
    std::cout << "Creating Haf \"" << output_filename << "\"\n";
    std::cout << "Primary files size: " << primary_files_size << "B\n";
    std::cout << "Total theoretical size: " << total_haf_size << "B\n";

    std::vector<char> header;
    header.push_back('H');
    header.push_back('A');
    header.insert(header.end(),
                  (char*) &total_haf_size,
                  (char*) &total_haf_size + sizeof(total_haf_size));
    header.insert(header.end(),
                  (char*) &files_number,
                  (char*) &files_number + sizeof(files_number));
    header.insert(header.end(),
                  (char*) &word_,
                  (char*) &word_ + sizeof(word_));

    WriteHeader(header, output_file);
    WriteFiles(args, output_file, word_, filename_end);
    output_file.close();

    std::cout << "Result size: " << std::filesystem::file_size(output_filename) << "B\n";
}

// Checks that file is Haf and returns archive size, number of included files and word length
std::tuple<uint32_t, uint32_t, uint8_t> ReadHeader(std::ifstream& stream) {
    const auto extra_bits = CountAddedBits(DEFAULT_LENGTH);
    std::vector<bool> bits;
    std::vector<char> data;
    char byte = '\0';
    uint16_t start_pos = 0;
    uint16_t bytes_read = 0;
    while (bytes_read < HEADER_SIZE && stream.get(byte)) {
        bytes_read++;

        // Writing necessary bits
        for (auto j = 0; j < CHAR_BIT; j++) {
            bits.emplace_back(byte & 0b10000000);
            byte <<= 1;
        }
        if (bits.size() - start_pos < DEFAULT_LENGTH + extra_bits) continue;

        // Checking errors and removing bits
        uint16_t end_pos = start_pos + DEFAULT_LENGTH + extra_bits;
        uint32_t wrong_bit = 0;
        for (uint8_t bit = 0; bit < extra_bits; bit++) {
            auto shift = (uint16_t) pow(2, bit);
            uint8_t extra_bit = '\0';
            for (uint16_t pos = start_pos + shift - 1 - bit; pos < end_pos - bit; pos += 2 * shift) {
                for (auto i = 0; (pos + i < end_pos) && (i < shift); i++) {
                    extra_bit ^= bits[pos + i];
                }
            }
            if (extra_bit) {
                wrong_bit += shift;
            }
            bits.erase(bits.begin() + start_pos + shift - 1 - bit);
        }
        end_pos -= extra_bits;
        if (wrong_bit) {
            bits[start_pos + wrong_bit - CountAddedBits(wrong_bit)] = !bits[start_pos + wrong_bit -
                                                                            CountAddedBits(wrong_bit)];
        }

        // Writing decoding data
        while (end_pos >= CHAR_BIT) {
            end_pos -= CHAR_BIT;
            char word = '\0';
            for (auto i = 0; i < CHAR_BIT; i++) {
                if (bits[0]) word++;
                bits.erase(bits.begin());
                if (i != CHAR_BIT - 1) word <<= 1;
            }
            data.emplace_back(word);
        }
        start_pos = end_pos;
    }
    // Ignore remaining bits, they are zeros

    // File type - 2B (0 - 1st position in data)
    std::string file_type(data.begin(), data.begin() + 2);
    if (file_type != "HA")
        throw std::logic_error("Trying to open not a Haf");

    // File size - 4B (2 - 5th position in data)
    uint32_t haf_size = *reinterpret_cast<uint32_t*>(&data[2]);
    // Files number - 4B (6 - 9th position in data)
    uint32_t files_number = *reinterpret_cast<uint32_t*>(&data[6]);
    // Word length - 1B (10th position in data)
    uint8_t word_length = *reinterpret_cast<uint8_t*>(&data[10]);

    return {haf_size, files_number, word_length};
}

std::vector<std::pair<std::string, uint32_t>> HafFilesList(const std::string& ha_file) {
    std::vector<std::pair<std::string, uint32_t>> files;
    auto input_stream = std::ifstream(ha_file, std::ios::binary);
    if (!input_stream.is_open()) {
        throw std::runtime_error("Failed to open " + ha_file);
    }
    uint32_t haf_size;
    uint32_t files_number;
    uint8_t word_;
    std::cout << "Reading Haf \"" << ha_file << "\"\n";
    std::tie(haf_size, files_number, word_) = ReadHeader(input_stream);
    std::cout << "Archive size: " << haf_size << "B\n";
    std::cout << "Files number: " << files_number << "\n";
    std::cout << "Coded with word: " << (uint16_t) word_ << "bit\n";

    // Reading such as header, but with skipping bytes with data
    const auto extra_bits = CountAddedBits(word_);
    for (uint32_t file_read = 0; file_read < files_number; file_read++) {
        uint32_t start_pos = 0;
        char byte;
        std::vector<bool> bits;
        std::vector<char> data;
        uint32_t need_bytes = INCLUDED_FILE_NAME_SIZE;
        uint32_t written_bytes = 0;
        while (input_stream.get(byte)) {
            for (auto j = 0; j < CHAR_BIT; j++) {
                bits.emplace_back(byte & 0b10000000);
                byte <<= 1;
            }
            if (bits.size() - start_pos < word_ + extra_bits) continue;

            uint32_t end_pos = start_pos + word_ + extra_bits;
            uint32_t wrong_bit = 0;
            for (uint8_t bit = 0; bit < extra_bits; bit++) {
                auto shift = (uint16_t) pow(2, bit);
                uint8_t extra_bit = '\0';
                for (uint32_t pos = start_pos + shift - 1 - bit; pos < end_pos - bit; pos += 2 * shift) {
                    for (auto i = 0; (pos + i < end_pos) && (i < shift); i++) {
                        extra_bit ^= bits[pos + i];
                    }
                }
                if (extra_bit) {
                    wrong_bit += shift;
                }
                bits.erase(bits.begin() + start_pos + shift - 1 - bit);
            }
            end_pos -= extra_bits;
            if (wrong_bit) {
                bits[start_pos + wrong_bit - CountAddedBits(wrong_bit)] = !bits[start_pos + wrong_bit -
                                                                                CountAddedBits(wrong_bit)];
            }

            while (end_pos >= CHAR_BIT) {
                end_pos -= CHAR_BIT;
                char word = '\0';
                for (auto i = 0; i < CHAR_BIT; i++) {
                    if (bits[0]) word++;
                    bits.erase(bits.begin());
                    if (i != CHAR_BIT - 1) word <<= 1;
                }
                data.emplace_back(word);
                written_bytes++;
                if (written_bytes == need_bytes) break;
            }
            start_pos = end_pos;
            if (data.size() == INCLUDED_FILE_NAME_SIZE) {
                need_bytes += data[0] + INCLUDED_FILE_SIZE;
            } else if (written_bytes == need_bytes) break;
        }

        uint32_t bytes_read = std::ceil(
            std::ceil((double) data.size() * CHAR_BIT / word_) * (word_ + extra_bits) / CHAR_BIT);
        uint32_t data_bytes = *reinterpret_cast<uint32_t*>(&data[INCLUDED_FILE_NAME_SIZE + data[0]]);
        files.emplace_back(
            std::string(data.begin() + INCLUDED_FILE_NAME_SIZE, data.begin() + INCLUDED_FILE_NAME_SIZE + data[0]),
            data_bytes);
        uint32_t skipped_bytes = (uint32_t) std::ceil(
            std::ceil((double) (data.size() + data_bytes) * CHAR_BIT / word_) * (word_ + extra_bits) / CHAR_BIT) -
                                 bytes_read;
        input_stream.seekg(skipped_bytes, std::ios_base::cur);
    }

    return files;
}

std::vector<std::string> ExtractHaf(const std::string& ha_file, const std::string& filename_end) {
    std::vector<std::string> files;
    auto input_stream = std::ifstream(ha_file, std::ios::binary);
    if (!input_stream.is_open()) {
        throw std::runtime_error("Failed to open " + ha_file);
    }
    uint32_t haf_size;
    uint32_t files_number;
    uint8_t word_;
    std::cout << "Extracting from Haf \"" << ha_file << "\"\n";
    std::tie(haf_size, files_number, word_) = ReadHeader(input_stream);
    std::cout << "Archive size: " << haf_size << "B\n";
    std::cout << "Files number: " << files_number << "\n";
    std::cout << "Coded with word: " << (uint16_t) word_ << "bit\n";

    // Reading such as header, but with skipping bytes with data
    const auto extra_bits = CountAddedBits(word_);
    std::ofstream output_stream;
    for (uint32_t file_read = 0; file_read < files_number; file_read++) {
        bool reading_header = true;
        uint32_t start_pos = 0;
        char byte;
        std::vector<bool> bits;
        std::vector<char> data;
        uint32_t need_bytes = INCLUDED_FILE_NAME_SIZE;
        uint32_t written_bytes = 0;
        while (input_stream.get(byte)) {
            for (auto j = 0; j < CHAR_BIT; j++) {
                bits.emplace_back(byte & 0b10000000);
                byte <<= 1;
            }
            if (bits.size() - start_pos < word_ + extra_bits) continue;

            uint16_t end_pos = start_pos + word_ + extra_bits;
            uint32_t wrong_bit = 0;
            for (uint8_t bit = 0; bit < extra_bits; bit++) {
                auto shift = (uint16_t) pow(2, bit);
                uint8_t extra_bit = '\0';
                for (uint16_t pos = start_pos + shift - 1 - bit; pos < end_pos - bit; pos += 2 * shift) {
                    for (auto i = 0; (pos + i < end_pos) && (i < shift); i++) {
                        extra_bit ^= bits[pos + i];
                    }
                }
                if (extra_bit) {
                    wrong_bit += shift;
                }
                bits.erase(bits.begin() + start_pos + shift - 1 - bit);
            }
            end_pos -= extra_bits;
            if (wrong_bit) {
                bits[start_pos + wrong_bit - CountAddedBits(wrong_bit)] =
                    !bits[start_pos + wrong_bit - CountAddedBits(wrong_bit)];
            }

            while (end_pos >= CHAR_BIT) {
                end_pos -= CHAR_BIT;
                char word = '\0';
                for (auto i = 0; i < CHAR_BIT; i++) {
                    if (bits[0]) word++;
                    bits.erase(bits.begin());
                    if (i != CHAR_BIT - 1) word <<= 1;
                }
                if (reading_header) data.emplace_back(word);
                else output_stream << word;
                written_bytes++;
                if (written_bytes == need_bytes) break;
            }
            start_pos = end_pos;
            if (data.size() == INCLUDED_FILE_NAME_SIZE) {
                need_bytes += data[0] + INCLUDED_FILE_SIZE;
            } else if (reading_header && !data.empty() &&
                       data.size() == INCLUDED_FILE_NAME_SIZE + data[0] + INCLUDED_FILE_SIZE) {
                need_bytes += *reinterpret_cast<uint32_t*>(&data[INCLUDED_FILE_NAME_SIZE + data[0]]);
                reading_header = false;
                std::string filename(data.begin() + INCLUDED_FILE_NAME_SIZE,
                                     data.begin() + INCLUDED_FILE_NAME_SIZE + data[0]);
                filename += filename_end;
                files.emplace_back(filename);
                if (ha_file.find_last_of("/\\") != std::string::npos)
                    filename = ha_file.substr(0, ha_file.find_last_of("/\\") + 1) += filename;
                output_stream = std::ofstream(filename, std::ios::binary);
            } else if (written_bytes == need_bytes) break;
        }
        output_stream.close();
    }

    return files;
}

void AppendFilesToHaf(const std::string& output_filename, std::vector<std::string>& args) {
    std::vector<std::string> files;
    auto input_stream = std::ifstream(output_filename, std::ios::binary);
    if (!input_stream.is_open()) {
        throw std::runtime_error("Failed to open " + output_filename);
    }
    uint32_t haf_first_size;
    uint32_t files_number;
    uint8_t word_;
    std::cout << "Appending files to Haf \"" << output_filename << "\"\n";
    std::tie(haf_first_size, files_number, word_) = ReadHeader(input_stream);
    std::cout << "Archive size before: " << haf_first_size << "B\n";
    std::cout << "Files number before: " << files_number << "\n";
    std::cout << "Coded with word: " << (uint16_t) word_ << "bit\n";

    uint32_t haf_after_size = haf_first_size;

    for (const auto& filename: args) {
        if (!std::filesystem::is_regular_file(filename))
            throw std::runtime_error("File [" + filename + "] does not exist");

        uint8_t filename_size = filename.size();
        if (filename.find_last_of("/\\") != std::string::npos) filename_size -= filename.find_last_of("/\\") + 1;
        uint32_t file_size = INCLUDED_FILE_NAME_SIZE + filename_size +
                             INCLUDED_FILE_SIZE + std::filesystem::file_size(filename);
        auto words_count = (uint32_t) std::ceil((double) (CHAR_BIT * file_size) / word_);
        haf_after_size += std::ceil((double) (words_count * (word_ + CountAddedBits(word_))) / CHAR_BIT);
    }
    files_number += args.size();
    input_stream.close();
    auto output_stream = std::ofstream(output_filename, std::ios::in | std::ios::binary);
    std::vector<char> header;
    header.push_back('H');
    header.push_back('A');
    header.insert(header.end(),
                  (char*) &haf_after_size,
                  (char*) &haf_after_size + sizeof(haf_after_size));
    header.insert(header.end(),
                  (char*) &files_number,
                  (char*) &files_number + sizeof(files_number));
    header.insert(header.end(),
                  (char*) &word_,
                  (char*) &word_ + sizeof(word_));
    WriteHeader(header, output_stream);
    output_stream.seekp(haf_first_size, std::ios_base::beg);
    WriteFiles(args, output_stream, word_, "");

    std::cout << "Final archive size: " << haf_after_size << "B\n";
    std::cout << "Files number after: " << files_number << '\n';
}

void DeleteFilesFromHaf(const std::string& output_filename, std::vector<std::string>& args) {
    auto input_stream = std::ifstream(output_filename, std::ios::binary);
    if (!input_stream.is_open()) {
        throw std::runtime_error("Failed to open " + output_filename);
    }
    uint32_t haf_first_size;
    uint32_t files_number;
    uint8_t word_;
    std::cout << "Deleting files from Haf \"" << output_filename << "\"\n";
    std::tie(haf_first_size, files_number, word_) = ReadHeader(input_stream);
    std::cout << "Archive size before: " << haf_first_size << "B\n";
    std::cout << "Files number before: " << files_number << "\n";
    std::cout << "Coded with word: " << (uint16_t) word_ << "bit\n";

    uint32_t n_files_deleted = args.size();

    auto output_stream = std::ofstream(output_filename + ".tmp", std::ios::binary);
    WriteHeader(std::vector<char>(HEADER_SIZE_WITHOUT_CODING, '\0'), output_stream);
    const auto extra_bits = CountAddedBits(word_);
    for (uint32_t file_read = 0; file_read < files_number; file_read++) {
        uint32_t start_pos = 0;
        char byte;
        std::vector<bool> bits;
        std::vector<char> data;
        uint32_t need_bytes = INCLUDED_FILE_NAME_SIZE;
        uint32_t written_bytes = 0;
        while (input_stream.get(byte)) {
            for (auto j = 0; j < CHAR_BIT; j++) {
                bits.emplace_back(byte & 0b10000000);
                byte <<= 1;
            }
            if (bits.size() - start_pos < word_ + extra_bits) continue;

            uint32_t end_pos = start_pos + word_ + extra_bits;
            uint32_t wrong_bit = 0;
            for (uint8_t bit = 0; bit < extra_bits; bit++) {
                auto shift = (uint16_t) pow(2, bit);
                uint8_t extra_bit = '\0';
                for (uint32_t pos = start_pos + shift - 1 - bit; pos < end_pos - bit; pos += 2 * shift) {
                    for (auto i = 0; (pos + i < end_pos) && (i < shift); i++) {
                        extra_bit ^= bits[pos + i];
                    }
                }
                if (extra_bit) {
                    wrong_bit += shift;
                }
                bits.erase(bits.begin() + start_pos + shift - 1 - bit);
            }
            end_pos -= extra_bits;
            if (wrong_bit) {
                bits[start_pos + wrong_bit - CountAddedBits(wrong_bit)] =
                    !bits[start_pos + wrong_bit - CountAddedBits(wrong_bit)];
            }

            while (end_pos >= CHAR_BIT) {
                end_pos -= CHAR_BIT;
                char word = '\0';
                for (auto i = 0; i < CHAR_BIT; i++) {
                    if (bits[0]) word++;
                    bits.erase(bits.begin());
                    if (i != CHAR_BIT - 1) word <<= 1;
                }
                data.emplace_back(word);
                written_bytes++;
                if (written_bytes == need_bytes) break;
            }
            start_pos = end_pos;
            if (data.size() == INCLUDED_FILE_NAME_SIZE) {
                need_bytes += data[0] + INCLUDED_FILE_SIZE;
            } else if (written_bytes == need_bytes) break;
        }

        bool file_found = false;
        uint32_t bytes_read = std::ceil(
            std::ceil((double) data.size() * CHAR_BIT / word_) * (word_ + extra_bits) / CHAR_BIT);
        uint32_t data_bytes = *reinterpret_cast<uint32_t*>(&data[INCLUDED_FILE_NAME_SIZE + data[0]]);
        auto total_file_bytes = (uint32_t) std::ceil(
            std::ceil((double) (data.size() + data_bytes) * CHAR_BIT / word_) * (word_ + extra_bits)
            / CHAR_BIT);
        std::string filename(data.begin() + INCLUDED_FILE_NAME_SIZE,
                             data.begin() + INCLUDED_FILE_NAME_SIZE + data[0]);
        for (uint32_t i = 0; i < args.size(); i++) {
            if (filename == args[i]) {
                file_found = true;
                args.erase(args.begin() + i);
                haf_first_size -= total_file_bytes;
                uint32_t skipped_bytes = (uint32_t) std::ceil(
                    std::ceil((double) (data.size() + data_bytes) * CHAR_BIT / word_) * (word_ + extra_bits)
                    / CHAR_BIT) - bytes_read;
                input_stream.seekg(skipped_bytes, std::ios_base::cur);
                break;
            }
        }

        if (!file_found) {
            input_stream.seekg(-(int64_t) bytes_read, std::ios_base::cur);
            for (auto i = 0; i < total_file_bytes; i++) {
                char element;
                input_stream.get(element);
                output_stream << element;
            }
        }
    }

    if (!args.empty()) throw std::runtime_error("File " + args[0] + " was not found in archive");
    else {
        files_number -= n_files_deleted;
        input_stream.close();
        output_stream.seekp(0, std::ios_base::beg);
        std::vector<char> header;
        header.push_back('H');
        header.push_back('A');
        header.insert(header.end(),
                      (char*) &haf_first_size,
                      (char*) &haf_first_size + sizeof(haf_first_size));
        header.insert(header.end(),
                      (char*) &files_number,
                      (char*) &files_number + sizeof(files_number));
        header.insert(header.end(),
                      (char*) &word_,
                      (char*) &word_ + sizeof(word_));
        WriteHeader(header, output_stream);
        output_stream.close();
        remove(output_filename.c_str());
        rename((output_filename + ".tmp").c_str(), output_filename.c_str());
    }
}

void ConcatenateHaf(const std::string& output_filename, std::vector<std::string>& args) {
    std::vector<std::string> filenames_result;
    std::string directory;
    if (output_filename.find_last_of("/\\") != std::string::npos)
        directory = output_filename.substr(0, output_filename.find_last_of("/\\") + 1);
    for (const auto& file: args) {
        for (const auto& inner_file: ExtractHaf(file, ".tmp")) {

            filenames_result.emplace_back(directory + inner_file.substr(0, inner_file.length() - 4));
        }
    }
    CreateHaf(output_filename, filenames_result, DEFAULT_LENGTH, ".tmp");
    for (const auto& file: filenames_result) {
        remove((file + ".tmp").c_str());
    }
}

/*
 *  0   -<muchas gracias aficiónados esto para vosotros. Sííííí!!!!!
 * -|-
 * / \
 *
 */
