#include <iostream>
#include <iomanip>
#include <string>
#include <filesystem>
#include <chrono>
#include <fstream>
#include <algorithm>

#include <CLI/CLI.hpp>
#include <nlohmann/json.hpp>

#include "board.hpp"
#include "solver.hpp"
#include "solver_backtrack.hpp"
#include "solver_dlx.hpp"
#include "json_handler.hpp"
#include "benchmark.hpp"
#include "types.hpp"
#include "system_info.hpp"

#ifdef HAS_TESSERACT
#include "ocr_processor.hpp"
#endif

#ifdef USE_OPENMP
#include <omp.h>
#endif

#ifdef _WIN32
#include <windows.h>
#endif

using namespace sudoku;

// ANSI color codes for console output
namespace Color {
    const std::string Reset   = "\033[0m";
    const std::string Bold    = "\033[1m";
    const std::string Red     = "\033[31m";
    const std::string Green   = "\033[32m";
    const std::string Yellow  = "\033[33m";
    const std::string Blue    = "\033[34m";
    const std::string Cyan    = "\033[36m";
    const std::string Magenta = "\033[35m";
    const std::string White   = "\033[37m";
}

/**
 * @brief Generate built-in test puzzles for benchmarking
 * Supports: 9x9, 16x16, 25x25
 */
namespace BuiltinPuzzles {

    // 9x9 - Classic hard puzzle (AI Escargot style)
    inline Grid get9x9() {
        return {
            {5, 3, 0, 0, 7, 0, 0, 0, 0},
            {6, 0, 0, 1, 9, 5, 0, 0, 0},
            {0, 9, 8, 0, 0, 0, 0, 6, 0},
            {8, 0, 0, 0, 6, 0, 0, 0, 3},
            {4, 0, 0, 8, 0, 3, 0, 0, 1},
            {7, 0, 0, 0, 2, 0, 0, 0, 6},
            {0, 6, 0, 0, 0, 0, 2, 8, 0},
            {0, 0, 0, 4, 1, 9, 0, 0, 5},
            {0, 0, 0, 0, 8, 0, 0, 7, 9}
        };
    }

    // 16x16 - Hard puzzle (30% clues, unique solution)
    inline Grid get16x16() {
        return {
            { 0,  0,  0,  4,  5,  6,  7,  0,  0, 10,  0,  0,  0,  0, 15,  0},
            { 0,  0,  0,  0,  0, 10,  0,  0,  0, 14,  0, 16,  0,  2,  0,  0},
            { 0, 10,  0,  0,  0,  0,  0,  0,  0,  2,  0,  0,  5,  6,  7,  0},
            { 0, 14, 15,  0,  1,  0,  0,  4,  5,  0,  0,  0,  0,  0,  0,  0},
            { 0,  0,  0,  0,  6,  0,  0,  0, 10,  0,  0,  0,  0,  0, 16,  0},
            { 6,  0,  8,  0,  0,  0,  0,  0,  0,  0,  0, 15,  2,  0,  4,  0},
            { 0,  9,  0,  0,  0,  0,  0,  0,  0,  0,  0,  3,  6,  5,  8,  0},
            { 0, 13, 16,  0,  2,  0,  0,  0,  0,  5,  0,  7,  0,  0,  0, 11},
            { 0,  0,  0,  2,  0,  8,  0,  6, 11,  0,  0,  0,  0,  0,  0,  0},
            { 7,  0,  5,  0, 11, 12,  9, 10,  0,  0,  0,  0,  0,  0,  1,  0},
            {11,  0,  0,  0, 15,  0,  0,  0,  3,  4,  0,  0,  7,  0,  0,  0},
            { 0,  0,  0,  0,  0,  4,  0,  2,  0,  8,  0,  6, 11,  0,  0,  0},
            { 0,  0,  2,  0,  0,  7,  0,  5, 12, 11,  0,  0,  0,  0, 14,  0},
            { 8,  0,  0,  0, 12,  0,  0,  0, 16,  0, 14,  0,  0,  0,  0,  0},
            { 0,  0,  0,  0, 16,  0, 14,  0,  4,  0,  0,  0,  0,  7,  0,  0},
            { 0,  0,  0,  0,  0,  3,  0,  0,  0,  0,  6,  0, 12,  0,  0,  0}
        };
    }

    // 25x25 - Challenging 25x25 puzzle (5x5 boxes)
    inline Grid get25x25() {
        // 25x25 grid with ~85% empty cells for heavy benchmark load
        Grid grid(25, std::vector<int>(25, 0));

        // Seed some valid starting positions (carefully chosen to be valid)
        // Row 0
        grid[0][0] = 1; grid[0][5] = 6; grid[0][10] = 11; grid[0][15] = 16; grid[0][20] = 21;
        // Row 1
        grid[1][1] = 7; grid[1][6] = 12; grid[1][11] = 17; grid[1][16] = 22; grid[1][21] = 2;
        // Row 2
        grid[2][2] = 13; grid[2][7] = 18; grid[2][12] = 23; grid[2][17] = 3; grid[2][22] = 8;
        // Row 3
        grid[3][3] = 19; grid[3][8] = 24; grid[3][13] = 4; grid[3][18] = 9; grid[3][23] = 14;
        // Row 4
        grid[4][4] = 25; grid[4][9] = 5; grid[4][14] = 10; grid[4][19] = 15; grid[4][24] = 20;

        // Row 5
        grid[5][0] = 2; grid[5][5] = 7; grid[5][10] = 12; grid[5][15] = 17; grid[5][20] = 22;
        // Row 6
        grid[6][1] = 8; grid[6][6] = 13; grid[6][11] = 18; grid[6][16] = 23; grid[6][21] = 3;
        // Row 7
        grid[7][2] = 14; grid[7][7] = 19; grid[7][12] = 24; grid[7][17] = 4; grid[7][22] = 9;
        // Row 8
        grid[8][3] = 20; grid[8][8] = 25; grid[8][13] = 5; grid[8][18] = 10; grid[8][23] = 15;
        // Row 9
        grid[9][4] = 1; grid[9][9] = 6; grid[9][14] = 11; grid[9][19] = 16; grid[9][24] = 21;

        // Row 10
        grid[10][0] = 3; grid[10][5] = 8; grid[10][10] = 13; grid[10][15] = 18; grid[10][20] = 23;
        // Row 11
        grid[11][1] = 9; grid[11][6] = 14; grid[11][11] = 19; grid[11][16] = 24; grid[11][21] = 4;
        // Row 12
        grid[12][2] = 15; grid[12][7] = 20; grid[12][12] = 25; grid[12][17] = 5; grid[12][22] = 10;
        // Row 13
        grid[13][3] = 21; grid[13][8] = 1; grid[13][13] = 6; grid[13][18] = 11; grid[13][23] = 16;
        // Row 14
        grid[14][4] = 2; grid[14][9] = 7; grid[14][14] = 12; grid[14][19] = 17; grid[14][24] = 22;

        // Row 15
        grid[15][0] = 4; grid[15][5] = 9; grid[15][10] = 14; grid[15][15] = 19; grid[15][20] = 24;
        // Row 16
        grid[16][1] = 10; grid[16][6] = 15; grid[16][11] = 20; grid[16][16] = 25; grid[16][21] = 5;
        // Row 17
        grid[17][2] = 16; grid[17][7] = 21; grid[17][12] = 1; grid[17][17] = 6; grid[17][22] = 11;
        // Row 18
        grid[18][3] = 22; grid[18][8] = 2; grid[18][13] = 7; grid[18][18] = 12; grid[18][23] = 17;
        // Row 19
        grid[19][4] = 3; grid[19][9] = 8; grid[19][14] = 13; grid[19][19] = 18; grid[19][24] = 23;

        // Row 20
        grid[20][0] = 5; grid[20][5] = 10; grid[20][10] = 15; grid[20][15] = 20; grid[20][20] = 25;
        // Row 21
        grid[21][1] = 11; grid[21][6] = 16; grid[21][11] = 21; grid[21][16] = 1; grid[21][21] = 6;
        // Row 22
        grid[22][2] = 17; grid[22][7] = 22; grid[22][12] = 2; grid[22][17] = 7; grid[22][22] = 12;
        // Row 23
        grid[23][3] = 23; grid[23][8] = 3; grid[23][13] = 8; grid[23][18] = 13; grid[23][23] = 18;
        // Row 24
        grid[24][4] = 4; grid[24][9] = 9; grid[24][14] = 14; grid[24][19] = 19; grid[24][24] = 24;

        return grid;
    }

    // Get puzzle by size
    inline std::pair<Grid, BoardDimension> getBySize(int size) {
        switch (size) {
        case 9:
            return { get9x9(), {9, 3, 3} };
        case 16:
            return { get16x16(), {16, 4, 4} };
        case 25:
            return { get25x25(), {25, 5, 5} };
        default:
            throw std::runtime_error("Unsupported test size: " + std::to_string(size) +
                ". Supported: 9, 16, 25");
        }
    }

    // Get description
    inline std::string getDescription(int size) {
        switch (size) {
        case 9:  return "9x9 Classic (3x3 boxes)";
        case 16: return "16x16 Extended (4x4 boxes) - 77 clues, hard";
        case 25: return "25x25 Mega (5x5 boxes) - Heavy benchmark";
        default: return "Unknown";
        }
    }
}

void printHeader() {
    std::cout << Color::Cyan << Color::Bold;
    std::cout << R"(
  ____            _       _            ____        _
 / ___| _   _  __| | ___ | | ___   _  / ___|  ___ | |_   _____ _ __
 \___ \| | | |/ _` |/ _ \| |/ / | | | \___ \ / _ \| \ \ / / _ \ '__|
  ___) | |_| | (_| | (_) |   <| |_| |  ___) | (_) | |\ V /  __/ |
 |____/ \__,_|\__,_|\___/|_|\_\\__,_| |____/ \___/|_| \_/ \___|_|

)" << Color::Reset;
    std::cout << "  High-Performance Sudoku Solver " << APP_VERSION << " (AllenK, Kwyshell)\n";
    std::cout << "  Using Dancing Links (DLX) & Constraint Propagation\n";

#ifdef USE_OPENMP
    std::cout << "  OpenMP: Enabled (" << omp_get_max_threads() << " threads)\n";
#else
    std::cout << "  OpenMP: Disabled\n";
#endif
    std::cout << "\n";
}

void printSystemInfo() {
    auto info = SystemInfoDetector::detect();

    // Helper to truncate long strings
    auto truncate = [](const std::string& str, size_t maxLen) -> std::string {
        if (str.length() <= maxLen) return str;
        return str.substr(0, maxLen - 3) + "...";
        };

    std::cout << Color::Magenta;
    std::cout << "+-------------------------------------------------------------+\n";
    std::cout << "|" << Color::Bold << "                    System Information                       " << Color::Reset << Color::Magenta << "|\n";
    std::cout << "+-------------------------------------------------------------+\n";
    std::cout << Color::Reset;

    // CPU
    std::cout << Color::Magenta << "|" << Color::Reset;
    std::cout << " CPU: " << Color::White << std::left << std::setw(55)
        << truncate(info.cpu_model, 55) << Color::Reset;
    std::cout << Color::Magenta << "|" << Color::Reset << "\n";

    std::ostringstream coresStr;
    coresStr << info.physical_cores << " cores / " << info.logical_cores << " threads";
    std::cout << Color::Magenta << "|" << Color::Reset;
    std::cout << " Cores: " << Color::White << std::left << std::setw(53)
        << coresStr.str() << Color::Reset;
    std::cout << Color::Magenta << "|" << Color::Reset << "\n";

    std::cout << Color::Magenta << "|" << Color::Reset;
    std::cout << " Clock: " << Color::White << std::left << std::setw(53)
        << truncate(info.cpuClockFormatted(), 53) << Color::Reset;
    std::cout << Color::Magenta << "|" << Color::Reset << "\n";

    std::cout << Color::Magenta << "+-------------------------------------------------------------+" << Color::Reset << "\n";

    // Memory
    std::ostringstream ramStr;
    ramStr << info.totalRamFormatted() << " (Available: " << info.availableRamFormatted() << ")";
    std::cout << Color::Magenta << "|" << Color::Reset;
    std::cout << " RAM: " << Color::White << std::left << std::setw(55)
        << truncate(ramStr.str(), 55) << Color::Reset;
    std::cout << Color::Magenta << "|" << Color::Reset << "\n";

    std::cout << Color::Magenta << "+-------------------------------------------------------------+" << Color::Reset << "\n";

    // OS
    std::ostringstream osStr;
    osStr << info.os_name;
    if (!info.os_version.empty() && info.os_version != "Unknown") {
        osStr << " " << info.os_version;
    }
    std::cout << Color::Magenta << "|" << Color::Reset;
    std::cout << " OS: " << Color::White << std::left << std::setw(56)
        << truncate(osStr.str(), 56) << Color::Reset;
    std::cout << Color::Magenta << "|" << Color::Reset << "\n";

    std::cout << Color::Magenta << "+-------------------------------------------------------------+" << Color::Reset << "\n";

    // Compiler
    std::cout << Color::Magenta << "|" << Color::Reset;
    std::cout << " Compiler: " << Color::White << std::left << std::setw(50)
        << truncate(info.compiler_info, 50) << Color::Reset;
    std::cout << Color::Magenta << "|" << Color::Reset << "\n";

    std::cout << Color::Magenta << "|" << Color::Reset;
    std::cout << " Build: " << Color::White << std::left << std::setw(53)
        << info.build_type << Color::Reset;
    std::cout << Color::Magenta << "|" << Color::Reset << "\n";

    std::cout << Color::Magenta << "+-------------------------------------------------------------+" << Color::Reset << "\n";
    std::cout << "\n";
}

void printBoard(const Board& board, const std::string& title) {
    std::cout << Color::Yellow << title << Color::Reset << "\n";
    std::cout << board.toString();
}

void printResult(const SolveResult& result) {
    std::cout << "\n" << Color::Bold << "=== Solution Result ===" << Color::Reset << "\n";

    if (result.solved) {
        std::cout << Color::Green << "Status: SOLVED" << Color::Reset << "\n";
    } else {
        std::cout << Color::Red << "Status: FAILED" << Color::Reset << "\n";
        if (!result.error_message.empty()) {
            std::cout << "Error: " << result.error_message << "\n";
        }
    }

    std::cout << "Algorithm: " << result.algorithm << "\n";
    std::cout << std::fixed << std::setprecision(6);
    std::cout << "Time: " << result.time_ms << " ms";

    // Also show in microseconds for fast solves
    if (result.time_ms < 1.0) {
        std::cout << " (" << (result.time_ms * 1000.0) << " μs)";
    }
    std::cout << "\n";

    std::cout << "Iterations: " << result.iterations << "\n";
    std::cout << "Backtracks: " << result.backtracks << "\n";
}

Board loadBoard(const std::string& input, bool isImage) {
#ifdef HAS_TESSERACT
    if (isImage) {
        std::cout << "Processing image: " << input << "\n";
        OCRProcessor ocr;
        ocr.setDebugMode(false);

        auto result = ocr.processImage(input);

        if (!result.success) {
            throw std::runtime_error("OCR failed: " + result.error_message);
        }

        if (!result.error_message.empty()) {
            std::cout << Color::Yellow << "Warning: " << result.error_message << Color::Reset << "\n";
        }
        std::cout << "\n";

        return Board(result.grid, result.dimension);
    } else {
        return JSONHandler::loadFromFile(input);
    }
#else
    (void)isImage;
    return JSONHandler::loadFromFile(input);
#endif
}

bool isImageFile(const std::string& path) {
    std::string ext = std::filesystem::path(path).extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    return ext == ".png" || ext == ".jpg" || ext == ".jpeg" ||
           ext == ".bmp" || ext == ".tiff" || ext == ".tif";
}

int main(int argc, char* argv[]) {

#ifdef _WIN32
    // Set console output to UTF-8
    SetConsoleOutputCP(CP_UTF8);

    // Enable ANSI escape sequences for colors (Windows 10+)
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut != INVALID_HANDLE_VALUE) {
        DWORD dwMode = 0;
        if (GetConsoleMode(hOut, &dwMode)) {
            dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
            SetConsoleMode(hOut, dwMode);
        }
    }
#endif

    CLI::App app{"High-Performance Sudoku Solver"};

    app.set_version_flag("-V,--version", APP_VERSION);

    // Input options
    std::string inputFile;
    app.add_option("-i,--input", inputFile, "Input file (JSON or image)")
        ->check(CLI::ExistingFile);

    // Algorithm selection
    std::string algorithm = "dlx";
    app.add_option("-a,--algorithm", algorithm,
                   "Solving algorithm: dlx, backtrack, compare")
        ->default_val("dlx");

    // Benchmark options
    int benchmarkRuns = 0;
    app.add_option("-b,--benchmark", benchmarkRuns,
                   "Run benchmark with N iterations")
        ->default_val(0);

    int testSize = 0;
    app.add_option("-t,--test", testSize,
        "Use built-in test puzzle (9, 16, or 25)")
        ->check(CLI::IsMember({ 0, 9, 16, 25 }));

    int numWorkers = 1;
    app.add_option("-w,--workers", numWorkers,
                   "Number of parallel workers for benchmark (0 = auto)")
        ->default_val(1);

    // Output options
    std::string outputFile;
    app.add_option("-o,--output", outputFile, "Output solution to JSON file");

    bool verbose = false;
    app.add_flag("-v,--verbose", verbose, "Verbose output");

    bool quiet = false;
    app.add_flag("-q,--quiet", quiet, "Minimal output");

    // Help for JSON format
    bool showJsonHelp = false;
    app.add_flag("--json-help", showJsonHelp, "Show JSON input format help");

    // Manual input for testing
    std::string puzzleString;
    app.add_option("-p,--puzzle", puzzleString,
                   "Puzzle as a string (use . or 0 for empty)");

    // Check unique solution
    bool checkUnique = false;
    app.add_flag("-u,--unique", checkUnique, "Check if solution is unique");

    // Solve all solutions
    bool solveAll = false;
    int maxSolutions = 100;  // Default limit for safety
    app.add_flag("--solve-all", solveAll, "Find all solutions (default limit: 25, use --max-solutions 0 for unlimited)");
    app.add_option("--max-solutions", maxSolutions, "Maximum number of solutions to find (0 = unlimited, WARNING: may never finish!)")
        ->default_val(100);

    // System info option
    bool showSysInfo = true;
    app.add_flag("--no-sysinfo", [&](auto) { showSysInfo = false; }, "Disable system information");

    CLI11_PARSE(app, argc, argv);

    // Show JSON help
    if (showJsonHelp) {
        std::cout << JSONHandler::getFormatHelp();
        return 0;
    }

    if (!quiet) {
        printHeader();
        // Show system info if requested or in benchmark/compare mode
        if (showSysInfo || benchmarkRuns > 0 || algorithm == "compare") {
            printSystemInfo();
        }
    }

    try {
        Board board;

        // Load board from input
        if (!inputFile.empty()) {
            bool isImage = isImageFile(inputFile);
#ifndef HAS_TESSERACT
            if (isImage) {
                throw std::runtime_error("OCR support not compiled in. "
                    "Rebuild with Tesseract to enable image input.");
            }
#endif
            board = loadBoard(inputFile, isImage);
        } else if (!puzzleString.empty()) {
            // Parse puzzle string
            nlohmann::json json;
            json["puzzle"] = puzzleString;
            board = JSONHandler::loadFromJSON(json);
        } else if (testSize > 0) {
            // Use built-in test puzzle of specified size
            if (!quiet) {
                std::cout << "Using built-in test puzzle: "
                    << BuiltinPuzzles::getDescription(testSize) << "\n\n";
            }
            auto [grid, dim] = BuiltinPuzzles::getBySize(testSize);
            board = Board(grid, dim);
        } else {
            // Default: use 9x9 test puzzle
            board = Board(BuiltinPuzzles::get9x9());
        }

        // Print input board
        if (!quiet) {
            printBoard(board, "Input Puzzle:");
            
            // Print optional metadata
            if (!board.name().empty()) {
                std::cout << "Name: " << Color::Cyan << board.name() << Color::Reset << "\n";
            }
            if (!board.difficultyLabel().empty()) {
                std::cout << "Difficulty: " << Color::Yellow << board.difficultyLabel() << Color::Reset << "\n";
            }
            
            std::cout << "Size: " << board.size() << "x" << board.size() << "\n";
            std::cout << "Empty cells: " << board.countEmpty() << "\n";
            std::cout << "Fill ratio: " << std::fixed << std::setprecision(1)
                      << (board.fillRatio() * 100) << "%\n\n";
        }

        // Validate input
        if (!board.isValid()) {
            std::cerr << Color::Red << "Error: Input puzzle is invalid!" << Color::Reset << "\n";
            return 1;
        }

        // Compare algorithms
        if (algorithm == "compare") {
            // Auto-detect workers if 0
            int workers = numWorkers;
            if (workers == 0) {
                workers = Benchmark::getHardwareConcurrency();
            }

            Benchmark bench;
            Benchmark::Config config;
            config.runs = std::max(1, benchmarkRuns > 0 ? benchmarkRuns : 10);
            config.warmup_runs = 2;
            config.num_workers = workers;
            config.verbose = verbose;
            bench.setConfig(config);

            if (workers > 1) {
                // Multi-threaded comparison
                if (!quiet) {
                    std::cout << board.toString();

                    std::cout << Color::Blue << "Comparing algorithms (multi-threaded: "
                              << workers << " workers)..." << Color::Reset << "\n\n";
                }

                auto results = bench.compareMultithreaded(board, {
                    SolverAlgorithm::DancingLinks,
                    SolverAlgorithm::Backtracking
                });

                // Print solutions from first worker
                for (const auto& [algo, result] : results) {
                    if (!result.worker_results.empty() &&
                        result.worker_results[0].result.solved) {
                        Board solutionBoard(result.worker_results[0].result.solution,
                                          board.dimension());
                        printBoard(solutionBoard, std::string("Solution: ") + result.algorithm);
                        std::cout << "\n";
                    }
                }

                bench.printMultithreadComparison(results);
            } else {
                // Single-threaded comparison
                if (!quiet) {
                    std::cout << Color::Blue << "Comparing algorithms..." << Color::Reset << "\n\n";
                }

                auto results = bench.compare(board, {
                    SolverAlgorithm::DancingLinks,
                    SolverAlgorithm::Backtracking
                });

                for (const auto& [algo, result] : results) {
                    if (result.result.solved) {
                        Board solutionBoard(result.result.solution, board.dimension());
                        printBoard(solutionBoard, std::string("Solution: ") + result.algorithm);
                        std::cout << "\n";
                    }
                }

                bench.printComparison(results);
            }
            return 0;
        }

        // Create solver
        std::unique_ptr<ISolver> solver;
        if (algorithm == "backtrack") {
            solver = SolverFactory::createBacktracking();
        } else {
            solver = SolverFactory::createDLX();
        }

        // Benchmark mode
        if (benchmarkRuns > 0) {
            // Auto-detect workers if 0
            int workers = numWorkers;
            if (workers == 0) {
                workers = Benchmark::getHardwareConcurrency();
            }

            Benchmark bench;
            Benchmark::Config config;
            config.runs = benchmarkRuns;
            config.warmup_runs = std::min(2, benchmarkRuns / 5);
            config.num_workers = workers;
            config.verbose = verbose;
            bench.setConfig(config);

            if (workers > 1) {
                // Multi-threaded benchmark
                if (!quiet) {
                    std::cout << Color::Blue << "Running multi-threaded benchmark..." << Color::Reset << "\n";
                    std::cout << "  Workers: " << workers << "\n";
                    std::cout << "  Runs per worker: " << benchmarkRuns << "\n";
                    std::cout << "  Total runs: " << (workers * benchmarkRuns) << "\n\n";
                }

                SolverAlgorithm algo = (algorithm == "backtrack") ?
                    SolverAlgorithm::Backtracking : SolverAlgorithm::DancingLinks;
                auto result = bench.runMultithreaded(board, algo);
                bench.printMultithreadResult(result);

                // Additional throughput summary
                if (!quiet) {
                    std::cout << "\n" << Color::Cyan << Color::Bold;
                    std::cout << "=== Performance Summary ===" << Color::Reset << "\n";
                    double total_time_sec = result.wall_time_ms / 1000.0;
                    int total_runs = workers * benchmarkRuns;
                    double throughput = total_runs / total_time_sec;
                    std::cout << std::fixed << std::setprecision(2);
                    std::cout << "  Throughput: " << Color::Green << throughput << " puzzles/sec" << Color::Reset << "\n";
                    std::cout << "  Total Time: " << result.wall_time_ms << " ms\n";
                    std::cout << "  Avg per puzzle: " << (result.wall_time_ms / total_runs) << " ms\n";
                }
            } else {
                // Single-threaded benchmark
                if (!quiet) {
                    std::cout << Color::Blue << "Running benchmark (" << benchmarkRuns << " iterations)..." <<
                        Color::Reset << "\n\n";
                }

                auto result = bench.run(board, *solver);
                bench.printResult(result);

                // Additional throughput summary
                if (!quiet && result.avg_time_ms > 0) {
                    std::cout << "\n" << Color::Cyan << Color::Bold;
                    std::cout << "=== Performance Summary ===" << Color::Reset << "\n";
                    double throughput = 1000.0 / result.avg_time_ms;
                    std::cout << std::fixed << std::setprecision(2);
                    std::cout << "  Throughput: " << Color::Green << throughput << " puzzles/sec" << Color::Reset << "\n";
                    std::cout << "  Avg per puzzle: " << result.avg_time_ms << " ms\n";

                    // Hint for multi-threaded benchmark
                    int hw_threads = Benchmark::getHardwareConcurrency();
                    if (hw_threads > 1) {
                        std::cout << Color::Yellow << "\n  Tip: Use -w 0 for multi-threaded benchmark ("
                                  << hw_threads << " threads available)" << Color::Reset << "\n";
                    }
                }
            }

            return 0;
        }

        // Solve
        if (!quiet) {
            std::cout << "Solving with " << solver->name() << "...\n";
        }

        // Solve all solutions mode
        if (solveAll) {
            // Warning for large puzzles with unlimited search
            if (maxSolutions == 0 && board.size() > 9) {
                std::cout << Color::Yellow << Color::Bold;
                std::cout << "\n⚠️  WARNING: Searching ALL solutions on a " << board.size() << "x" << board.size()
                          << " puzzle with no limit!\n";
                std::cout << "   This may take EXTREMELY long (potentially years).\n";
                std::cout << "   Consider using --max-solutions to set a limit.\n";
                std::cout << "   Press Ctrl+C to abort.\n";
                std::cout << Color::Reset << "\n";
            }

            if (!quiet) {
                std::cout << Color::Blue << "Finding all solutions";
                if (maxSolutions > 0) {
                    std::cout << " (max: " << maxSolutions << ")";
                } else {
                    std::cout << " (UNLIMITED)";
                }
                std::cout << "..." << Color::Reset << "\n";
            }

            auto start = std::chrono::high_resolution_clock::now();
            auto solutions = solver->findAllSolutions(board, maxSolutions);
            auto end = std::chrono::high_resolution_clock::now();
            double elapsed_ms = std::chrono::duration<double, std::milli>(end - start).count();

            std::cout << "\n" << Color::Bold << "=== All Solutions Result ===" << Color::Reset << "\n";
            std::cout << "Solutions found: " << Color::Green << solutions.size() << Color::Reset;
            if (maxSolutions > 0 && static_cast<int>(solutions.size()) >= maxSolutions) {
                std::cout << Color::Yellow << " (limit reached)" << Color::Reset;
            }
            std::cout << "\n";
            std::cout << std::fixed << std::setprecision(2);
            std::cout << "Time: " << elapsed_ms << " ms\n";

            if (!solutions.empty()) {
                // Show first few solutions
                int showCount = std::min(static_cast<int>(solutions.size()), 3);
                for (int i = 0; i < showCount; i++) {
                    std::cout << "\n";
                    printBoard(solutions[i], "Solution #" + std::to_string(i + 1) + ":");
                }

                if (solutions.size() > 3) {
                    std::cout << "\n" << Color::Yellow
                              << "... and " << (solutions.size() - 3) << " more solutions"
                              << Color::Reset << "\n";
                }

                // Save all solutions if output file specified
                if (!outputFile.empty()) {
                    nlohmann::json output;
                    output["puzzle"] = board.grid();
                    output["solution_count"] = solutions.size();
                    nlohmann::json solArray = nlohmann::json::array();
                    for (const auto& sol : solutions) {
                        solArray.push_back(sol.grid());
                    }
                    output["solutions"] = solArray;
                    output["time_ms"] = elapsed_ms;

                    std::ofstream ofs(outputFile);
                    ofs << output.dump(2);
                    std::cout << "\nAll solutions saved to: " << outputFile << "\n";
                }
            }

            return solutions.empty() ? 1 : 0;
        }

        auto result = solver->solve(board);

        // Print result
        if (!quiet) {
            printResult(result);
        }

        if (result.solved) {
            Board solutionBoard(result.solution, board.dimension());

            if (!quiet) {
                std::cout << "\n";
                printBoard(solutionBoard, "Solution:");
            } else {
                // Quiet mode: just print the solution
                solutionBoard.printCompact(std::cout);
            }

            // Check uniqueness
            if (checkUnique) {
                std::cout << "\nChecking uniqueness...\n";
                bool unique = solver->hasUniqueSolution(board);
                if (unique) {
                    std::cout << Color::Green << "Solution is UNIQUE" << Color::Reset << "\n";
                } else {
                    std::cout << Color::Yellow << "Multiple solutions exist" << Color::Reset << "\n";
                }
            }

            // Save output
            if (!outputFile.empty()) {
                JSONHandler::saveSolutionToFile(board, result, outputFile);
                std::cout << "\nSolution saved to: " << outputFile << "\n";
            }
        }

        return result.solved ? 0 : 1;

    } catch (const std::exception& e) {
        std::cerr << Color::Red << "Error: " << e.what() << Color::Reset << "\n";
        return 1;
    }
}
