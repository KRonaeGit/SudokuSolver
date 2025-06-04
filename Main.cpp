#include "SudokuBoard.h"
#include <iostream>
#include <cstdio>
#include <vector>
#include <chrono>

/**
 * @file
 * @brief Implementation of a console-based Sudoku solver with optional descriptive output.
 *
 * This file defines functions to read a Sudoku puzzle from standard input,
 * print the board state with optional ANSI colors, and solve the puzzle
 * using the SudokuBoard class (bitset-based solver with DFS and logical simplification).
 */

#define PROGRAM_VERSION "SudokuSolver v1.1.4"
#define IS_DESCRIPTED_VERSION          0
#define IS_ANSI_ESCAPE_COLORED_VERSION 0

#if IS_ANSI_ESCAPE_COLORED_VERSION
  #define ANSI_ESCAPE_RESET    "\033[0m"
  #define ANSI_ESCAPE_GRAY     "\033[90m"
  #define ANSI_ESCAPE_RED      "\033[91m"
  #define ANSI_ESCAPE_GREEN    "\033[92m"
  #define ANSI_ESCAPE_YELLOW   "\033[93m"
  #define ANSI_ESCAPE_MAGENTA  "\033[95m"
#else
  #define ANSI_ESCAPE_RESET    ""
  #define ANSI_ESCAPE_GRAY     ""
  #define ANSI_ESCAPE_RED      ""
  #define ANSI_ESCAPE_GREEN    ""
  #define ANSI_ESCAPE_YELLOW   ""
  #define ANSI_ESCAPE_MAGENTA  ""
#endif

/** Global SudokuBoard instance used by the solver. */
SudokuBoard board;

/** Counters for assignments and simplifications, used in output summary. */
static ulli assignments, simplifications;

/** Constant array of 81 falses, used to indicate no highlights when printing. */
static const bool falseArr81[81] = {};

/**
 * @brief Print the current board state to stdout, optionally highlighting certain cells.
 *
 * This function draws the Sudoku grid with row and column separators.
 * Cells with a single determined value are printed as digits; cells with
 * multiple candidates are shown as '-' and empty cells with zero candidates
 * are marked with '!'. Highlighted cells (where highlights[i] == true)
 * are printed in magenta.
 *
 * @param spaces Number of leading spaces to indent each printed line (to indicate recursion depth).
 * @param highlights Boolean array of length 81, where true indicates the cell should be highlighted.
 * @return true if any cell has zero candidates (contradiction), false otherwise.
 */
static bool printBoard(size_t spaces, const bool highlights[81], const char* highlightColor) {
    bool hasContradiction = false;

    for (ui y = 0; y < 9; y++) {
        // Print horizontal separator every 3 rows
        if (y % 3 == 0) {
            for (size_t i = 0; i < spaces; i++) std::cout << ' ';
            std::cout << ANSI_ESCAPE_GRAY << "+-------+-------+-------+" << ANSI_ESCAPE_RESET << std::endl;
        }
        // Indent
        for (size_t i = 0; i < spaces; i++) std::cout << ' ';

        for (ui x = 0; x < 9; x++) {
            // Print vertical separator every 3 columns
            if (x % 3 == 0) std::cout << ANSI_ESCAPE_GRAY << "| " << ANSI_ESCAPE_RESET;

            uc count;
            uc val = board.getCellInfoAt(GPos(x, y), count);
            if (val == 0) {
                // If no single value assigned
                if (count == 0) {
                    // Contradiction: no candidates remain
                    hasContradiction = true;
                    std::cout << ANSI_ESCAPE_RED << "! " << ANSI_ESCAPE_RESET;
                    continue;
                }
                // Multiple candidates remain: print placeholder '-'
                std::cout << ANSI_ESCAPE_GRAY << '-' << ANSI_ESCAPE_RESET << ' ';
            } else {
                // Single value assigned
                bool highlight = highlights[x + 9 * y];
                if (highlight) std::cout << highlightColor;
                std::cout << char('0' + val) << ' ';
                if (highlight) std::cout << ANSI_ESCAPE_RESET;
            }
        }
        // Close row
        std::cout << ANSI_ESCAPE_GRAY << "|" << std::endl;
    }
    // Print final horizontal separator
    for (ui i = 0; i < spaces; i++) std::cout << ' ';
    std::cout << ANSI_ESCAPE_GRAY << "+-------+-------+-------+" << ANSI_ESCAPE_RESET << std::endl;

    return hasContradiction;
}

/**
 * @brief Convert a vector of GPos into a boolean highlight array (length 81).
 *
 * This helper builds an array of 81 booleans where indices corresponding
 * to the positions in 'highlights' are set to true.
 *
 * @param spaces Unused parameter (kept for overload consistency).
 * @param highlights Vector of GPos positions to highlight.
 * @return Boolean array of length 81 with true at highlighted positions.
 */
static bool printBoard(size_t spaces, const std::vector<GPos>& highlights) {
    bool highlights_[81] = {}; // Initialize all to false
    for (size_t i = 0; i < highlights.size(); i++) {
        GPos gpos = highlights[i];
        highlights_[gpos.getX() + 9 * gpos.getY()] = true;
    }
    return highlights_;
}

/**
 * @brief Listener callback invoked whenever a tentative value is assigned to a cell during DFS.
 *
 * This function increments the 'assignments' counter, and if descriptive mode is enabled,
 * it prints the current recursion path, the assigned cell coordinates, and the board state.
 *
 * @param path Vector of branch indices taken so far (for tracing).
 * @param assigned Boolean array of length 81 indicating which cells are currently assigned.
 * @param justAssigned GPos of the cell that was just assigned a definite value.
 */
static void assignListener(
    const std::vector<ui>& path,
    const bool assigned[81],
    const GPos& justAssigned
) {
    assignments++;
    if (!IS_DESCRIPTED_VERSION) return;

    // Indentation proportional to recursion depth
    size_t spaces = (path.size() - 1) * 2;
    for (size_t i = 0; i < spaces; i++) std::cout << ' ';

    // Print path of branch decisions (1-based for display)
    for (size_t i = 1; i < path.size(); i++) {
        if (i != 0) std::cout << '.';
        std::cout << (path[i] + 1);
    }

    std::cout << ANSI_ESCAPE_GRAY << "(T): " << ANSI_ESCAPE_RESET
        << ANSI_ESCAPE_MAGENTA << "ASSIGN" << ANSI_ESCAPE_RESET
        << ": (" << (int)(justAssigned.getX() + 1)
        << ',' << (int)(justAssigned.getY() + 1) << ") = "
        << ANSI_ESCAPE_MAGENTA
        << ((int)board.getOnlyPossibleValue(justAssigned))
        << ANSI_ESCAPE_RESET << std::endl;

    // Print board state after assignment
    printBoard(spaces, assigned, ANSI_ESCAPE_MAGENTA);

    std::cout << std::endl; // v1.1.4
}

/**
 * @brief Listener callback invoked after each simplification pass in DFS.
 *
 * This function increments the 'simplifications' counter, and if descriptive mode is enabled,
 * it prints the current path, the number of candidates eliminated in this pass,
 * and the total eliminated so far, followed by the board state.
 *
 * @param path Vector of branch indices taken so far.
 * @param index Index of the simplification iteration (0-based).
 * @param eliminated Number of candidate bits eliminated in this pass.
 * @param eliminatedSum Accumulated total elimination count so far.
 * @param isFirstSimplificationGroup True if this is the first simplify call at this branch.
 * @param assigned Boolean array of length 81 indicating which cells are assigned.
 */
static void simplifyListener(
    const std::vector<ui>& path,
    const ui& index,
    const ui& eliminated,
    const ulli& eliminatedSum,
    const bool isFirstSimplificationGroup,
    const bool assigned[81]
) {
    simplifications++;
    if (!IS_DESCRIPTED_VERSION) return;

    // Indentation proportional to recursion depth
    size_t spaces = (path.size()) * 2;
    if (spaces >= 2) {
        for (size_t i = 0; i < spaces - 2; i++) std::cout << ' ';
        std::cout << ANSI_ESCAPE_GRAY << "> " << ANSI_ESCAPE_RESET;
    }

    // Print branch path
    for (size_t i = 1; i < path.size(); i++) {
        if (i != 0) std::cout << '.';
        std::cout << (path[i] + 1);
    }

    std::cout << ANSI_ESCAPE_GRAY << "(S." << (index + 1) << "): " << ANSI_ESCAPE_RESET
        << ANSI_ESCAPE_GREEN << "SIMPLIFY" << ANSI_ESCAPE_RESET
        << ": ELIMINATED = " << eliminated
        << ANSI_ESCAPE_GRAY << "(sum = " << eliminatedSum << ")" << ANSI_ESCAPE_RESET
        << std::endl;

    // Print board state after simplification
    printBoard(spaces, assigned, ANSI_ESCAPE_MAGENTA);

    std::cout << std::endl; // v1.1.3
}

/**
 * @brief Listener callback invoked whenever a single candidate elimination or determination occurs.
 *
 * This function prints detailed information about each elimination step if descriptive mode is enabled:
 * whether the cause was elimination or hidden single logic, which cell was affected,
 * what value was eliminated or assigned, and by which house (row/column/chunk).
 *
 * @param path Vector of branch indices taken so far.
 * @param cause Enumeration value indicating type of elimination or assignment.
 * @param cell GPos of the cell where elimination/assignment occurred.
 * @param value The candidate value (1..9) that was eliminated or determined.
 * @param by For elimination: the index of row/column/chunk responsible (0-based).
 *           For hidden single: the house index similarly encoded.
 */
static void eliminateListener(
    const std::vector<ui>& path,
    const SimplificationCause& cause,
    const GPos& cell,
    const uc& value,
    const uc& by
) {
    if (!IS_DESCRIPTED_VERSION) return;

    // Indentation proportional to recursion depth
    size_t spaces = (path.size()) * 2;
    for (size_t i = 0; i < spaces; i++) std::cout << ' ';

    bool isElimination = ELIMINATION_BY_ROW <= cause && cause <= ELIMINATION_BY_CHUNK;

    std::cout << ANSI_ESCAPE_GRAY << "-> ";
    if (cause == NO_VALUE_POSSIBLE) {
        // Contradiction detected
        std::cout << ANSI_ESCAPE_MAGENTA << "IMPOSSIBLE" << ANSI_ESCAPE_RESET;
    } else if (isElimination) {
        // Candidate eliminated
        std::cout << ANSI_ESCAPE_RED << "ELIMINATED" << ANSI_ESCAPE_RESET;
    } else {
        // Hidden single assignment
        std::cout << ANSI_ESCAPE_GREEN << "BE DECIDED" << ANSI_ESCAPE_RESET;
    }

    // Print cell coordinates (1-based for display)
    std::cout << ": (" << ((int)cell.getX() + 1) << ", " << ((int)cell.getY() + 1) << ")";

    if (cause == NO_VALUE_POSSIBLE) {
        // Only print "IMPOSSIBLE", no further details
        std::cout << ANSI_ESCAPE_RESET << std::endl;
        return;
    }

    // Print the value eliminated or determined
    std::cout << ' ' << (isElimination ? '!' : '=') << "= " << (int)value;

    // Print which house (row/column/chunk) caused this event
    std::cout << " by ";
    int classification = (cause - 1) % 3;
    if (classification == 0) {
        std::cout << "row";
    } else if (classification == 1) {
        std::cout << "column";
    } else if (classification == 2) {
        std::cout << "chunk";
    }
    std::cout << ' ' << ((int)by + 1);
    if (classification == 2) {
        // For chunk, also display 3×3 coordinates (1-based)
        std::cout << '(' << (int)(by % 3 + 1) << ", " << ((int)(by / 3) + 1) << ')';
    }

    // Print remaining candidates for that cell in gray
    std::cout << ANSI_ESCAPE_GRAY << " {";
    bool isFirst = true;
    uc count = 0;
    for (uc v = 1; v <= 9; v++) {
        if (!board.isPossibleAt(cell, v)) continue;
        if (!isFirst) std::cout << ", ";
        std::cout << (int)v;
        isFirst = false;
        count++;
    }
    std::cout << "}(" << (int)count << ")" << ANSI_ESCAPE_RESET;

    // If exactly one candidate remains after elimination, mark it
    if (board.getOnlyPossibleValue(cell) != 0)
        std::cout << ANSI_ESCAPE_GREEN << " (!)" << ANSI_ESCAPE_RESET;

    std::cout << ANSI_ESCAPE_RESET << std::endl;
}

/**
 * @brief Consume and discard all characters until a newline is encountered.
 *
 * Used to clear the input buffer when expecting the user to press ENTER.
 */
static void skipToNewLine() {
    char c;
    while ((c = std::getchar()) != '\n') {}
}

/**
 * @brief Pause execution until the user presses ENTER.
 *
 * Prints a prompt and waits for a newline. Used between major steps.
 */
static void pause() {
    std::cout << std::endl << "Press ENTER to continue..." << std::endl;
    skipToNewLine();
}

/**
 * @brief Main solver function that reads a Sudoku puzzle, solves it, and prints results.
 *
 * 1. Prompt user to enter 9 lines of 9 characters each (digits 1-9 or blank for empty).
 * 2. Populate the SudokuBoard with given clues.
 * 3. Display the initial board, then wait for user input if descriptive mode is off.
 * 4. Invoke DFS solver with listeners to track progress.
 * 5. Measure elapsed time and count of assignments/simplifications.
 * 6. Print final solved board and performance summary.
 *
 * @return true on normal return; false if input format error occurred.
 */
static bool solver() {
    std::cout << std::endl
        << "> Enter your sudoku board below "
        << ANSI_ESCAPE_GRAY
        << "(digits 1 - 9, others blank) "
        << ANSI_ESCAPE_RESET << std::endl;

    // Read 9 rows of input
    for (uc y = 0; y < 9; y++) {
        std::cout << ANSI_ESCAPE_GRAY << (int)(y + 1) << ": " << ANSI_ESCAPE_RESET;
        for (uc x = 0; x < 9; x++) {
            char c = 0;
            // Read one character (expect digit or blank)
            if (scanf_s("%c", &c, 1) != 1) {
                std::cerr << ANSI_ESCAPE_RED << "{error} input-format-error: too little characters provided in a line" << ANSI_ESCAPE_RESET << std::endl;
                return false;
            }
            // If last column, the next character must be newline
            if (x == 8) {
                char nl = std::getchar();
                if (nl != '\n') {
                    std::cerr << ANSI_ESCAPE_RED << "{error} input-format-error: newline is missing" << ANSI_ESCAPE_RESET << std::endl;
                    skipToNewLine();
                    return false;
                }
            }
            // If character is not a digit, treat as empty
            if (!(c >= '1' && c <= '9')) {
                if (c == '\n') {
                    std::cerr << ANSI_ESCAPE_RED << "{error} input-format-error: unexpected newline provided" << ANSI_ESCAPE_RESET << std::endl;
                    return false;
                }
                continue;
            }
            // Convert digit char to numeric value and enforce clue on board
            board.makeSureAt(GPos(x, y), (uc)(c - '0'), true);
        }
    }

    std::cout << std::endl;
    // Display initial board (no highlights)
    printBoard(0, falseArr81, nullptr);
    pause();

    bool decidedAtStart[81] = {}; // {false, false, ...}
    SudokuBoard before(board.copyData());
    for (uc y = 0; y < 9; y++) {
        for (uc x = 0; x < 9; x++) {
            if (before.getOnlyPossibleValue(GPos(x,y)) == 0) {
                continue;
            }
            decidedAtStart[x + 9 * y] = true;
        }
    }


    // Start timing
    auto start = std::chrono::high_resolution_clock::now();

    // Solve with DFS, collecting highlights array (unused) and listeners
    bool highlights[81] = {};
    bool solved = board.dfsSolve(highlights, assignListener, simplifyListener, eliminateListener);

    // End timing
    auto end = std::chrono::high_resolution_clock::now();
    auto micros = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    double seconds = micros / 1'000'000.0;

    // Print spacing and solution header
    std::cout << std::endl;
    std::cout << ">======== ANSWER ========" << std::endl;

    if (!solved) {
        std::cerr << "> No solution found." << std::endl;
        return true;
    }

    // Display solved board
    printBoard(0, decidedAtStart, ANSI_ESCAPE_GRAY);
    std::cout << ANSI_ESCAPE_GRAY << '>' << ANSI_ESCAPE_RESET << " Solved in "
        << ANSI_ESCAPE_YELLOW << assignments     << ANSI_ESCAPE_RESET << " Tentative Assignments, "
        << ANSI_ESCAPE_YELLOW << simplifications << ANSI_ESCAPE_RESET << " Simplifications, "
        << ANSI_ESCAPE_GREEN  << seconds         << ANSI_ESCAPE_RESET << " seconds." << std::endl;
    return true;
}

/**
 * @brief Program entry point. Repeatedly runs the solver in a loop until terminated.
 *
 * After each solved puzzle (or failure), resets counters and the board,
 * then waits for ENTER before proceeding to next puzzle.
 *
 * @return Exit code (unused).
 */
int main() {
    std::cout << ANSI_ESCAPE_YELLOW << PROGRAM_VERSION << ANSI_ESCAPE_RESET << ' ';
    std::cout << (IS_DESCRIPTED_VERSION          ?    "DESC" :    "PRFM") << ' ';
    std::cout << (IS_ANSI_ESCAPE_COLORED_VERSION ? "COLORED" : "NOCOLOR");
    std::cout << std::endl;

    bool isFirst = true;
    while (true) {
        assignments = 0;
        simplifications = 0;
        if (isFirst) {
            isFirst = false;
        } else {
            board = SudokuBoard();
        }

        if (!solver()) {
            continue;
        }
        pause();
    }
    return 0;
}
