#include "argument_parser/lib/parser.h"
#include "lib/bitstream.h"

#include <iostream>
#include <tuple>
#include <variant>

/* ex. run commands:
 * -f=..\..\result_files\output\out_file1.haf -c ..\..\result_files\input\image.jpg ..\..\result_files\input\in2.txt -x -l -w 11
 * -f=..\..\result_files\output\out_file1.haf -a ..\..\result_files\input\in_file1_1.txt -l
 * -f=..\..\result_files\output\out_file1.haf -d image.jpg -l
 * -f=..\..\result_files\output\out_file2.haf -c ..\..\result_files\input\in_file1_1.txt -l
 * -f=..\..\result_files\output\out_file3.haf -c ..\..\result_files\input\image.jpg -l
 * -f=..\..\result_files\output\out_file4.haf -A ..\..\result_files\output\out_file2.haf ..\..\result_files\output\out_file3.haf -l
 *
 */
struct Arguments {
    supported_variants haf_file = "";
    supported_variants create_command = false;
    supported_variants list_command = false;
    supported_variants extract_command = false;
    supported_variants append_command = false;
    supported_variants delete_command = false;
    supported_variants concatenate_command = false;
    supported_variants word_coding_length = DEFAULT_LENGTH;
    std::vector<std::string> free_args;
};

auto arguments = new Arguments{};

void ParseLocal(int argc, char** argv) {
    std::vector<std::tuple<supported_variants&, std::string, std::string>> parameters =
        {{arguments->haf_file,            "-f", "--file"},
         {arguments->create_command,      "-c", "--create"},
         {arguments->list_command,        "-l", "--list"},
         {arguments->extract_command,     "-x", "--extract"},
         {arguments->append_command,      "-a", "--append"},
         {arguments->delete_command,      "-d", "--delete"},
         {arguments->concatenate_command, "-A", "--concatenate"},
         {arguments->word_coding_length,  "-w", "--word"}};
    Parse(argc, argv, parameters, arguments->free_args);
}

int main(int argc, char** argv) {
    ParseLocal(argc, argv);
    const std::string ha_file = std::get<std::string>(arguments->haf_file);
    if (ha_file.empty()) {
        std::cerr << "No file name was found";
        exit(-1);
    }

    // We can use some commands in combination (ex., create + list or append + delete + list)

    bool create_command = std::get<bool>(arguments->create_command);
    bool list_command = std::get<bool>(arguments->list_command);
    bool extract_command = std::get<bool>(arguments->extract_command);
    bool append_command = std::get<bool>(arguments->append_command);
    bool delete_command = std::get<bool>(arguments->delete_command);
    bool concatenate_command = std::get<bool>(arguments->concatenate_command);
    int word_coding_length = std::get<int>(arguments->word_coding_length);
    std::vector<std::string> free_args;
    for (const auto& arg: arguments->free_args) {
        free_args.push_back(arg);
    }
    std::cout << "-------------\n";
    delete arguments;
    try {
        if (create_command) {
            CreateHaf(ha_file, free_args, word_coding_length, "");
            std::cout << "-------------\n";
        }
        if (extract_command) {
            auto files = ExtractHaf(ha_file, "");
            std::cout << "Written files (in Haf directory):\n";
            for (const std::string& filename: files) {
                std::cout << '\"' << filename << "\"\n";
            }
            std::cout << "-------------\n";
        }
        if (append_command) {
            AppendFilesToHaf(ha_file, free_args);
            std::cout << "-------------\n";
        }
        if (delete_command) {
            DeleteFilesFromHaf(ha_file, free_args);
            std::cout << "-------------\n";
        }
        if (concatenate_command) {
            ConcatenateHaf(ha_file, free_args);
            std::cout << "-------------\n";
        }
        if (list_command) {
            auto files = HafFilesList(ha_file);
            std::cout << "Found files:\n";
            for (const auto& data: files) {
                std::cout << '\"' << data.first << "\" " << data.second << "B\n";
            }
            std::cout << "-------------\n";
        }
    }
    catch (const std::exception& ex) {
        std::cerr << ex.what() << '\n';
    }
}
