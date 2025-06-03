#pragma once

#include <functional>
#include <stdexcept>
#include <iostream>
#include <utility>
#include <vector>
#include <array>

typedef unsigned long long int ulli;  /**< 64-bit unsigned integer alias for bit operations */
typedef unsigned char uc;             /**< 8-bit unsigned integer alias for small values */
typedef unsigned int ui;              /**< 32-bit unsigned integer alias for indices or counters */

/**
 * @enum SimplificationCause
 * @brief Causes for candidate elimination or determination during simplification.
 */
enum SimplificationCause {
    NO_VALUE_POSSIBLE = -1,        /**< No candidate possible for a cell, indicates contradiction */
    ELIMINATION_BY_ROW = 1,        /**< Candidate eliminated because same row has a determined value */
    ELIMINATION_BY_COLUMN = 2,     /**< Candidate eliminated because same column has a determined value */
    ELIMINATION_BY_CHUNK = 3,      /**< Candidate eliminated because same 3¡¿3 chunk has a determined value */
    VALUE_SURE_BY_ROW = 4,         /**< Cell value determined uniquely by row constraint */
    VALUE_SURE_BY_COLUMN = 5,      /**< Cell value determined uniquely by column constraint */
    VALUE_SURE_BY_CHUNK = 6        /**< Cell value determined uniquely by chunk constraint */
};

/**
 * @class Tuple2
 * @brief Simple 2D coordinate pair class.
 *
 * Stores x and y as unsigned chars and provides getters.
 */
class Tuple2 {
private:
    uc x;  /**< X-coordinate (0..8 for Sudoku) */
    uc y;  /**< Y-coordinate (0..8 for Sudoku) */

public:
    /**
     * @brief Constructor initializing x and y coordinates.
     * @param x_val Initial x-coordinate.
     * @param y_val Initial y-coordinate.
     */
    Tuple2(uc x_val, uc y_val);

    /**
     * @brief Get the x-coordinate.
     * @return Current x-coordinate.
     */
    uc getX() const;

    /**
     * @brief Get the y-coordinate.
     * @return Current y-coordinate.
     */
    uc getY() const;
};

/**
 * @class GPos
 * @brief Global position of a cell on the 9¡¿9 Sudoku board.
 *
 * Inherits from Tuple2. X and Y range from 0 to 8.
 */
class GPos : public Tuple2 {
public:
    /**
     * @brief Default constructor, initializes to (0,0).
     */
    GPos();

    /**
     * @brief Constructor with explicit coordinates.
     * @param x X-coordinate (0..8).
     * @param y Y-coordinate (0..8).
     */
    GPos(uc x, uc y);
};

/**
 * @class SudokuBoard
 * @brief Bitset-based representation of a 9¡¿9 Sudoku board supporting
 *        candidate elimination, logical simplification, and DFS solving.
 *
 * Each of the 81 cells has 9 candidate bits (1..9), stored in a total of
 * 12 ulli elements (12¡¿64 = 768 bits) for 729 used bits.
 */
class SudokuBoard {
private:
    std::array<ulli, 12> bitset;  /**< Flat bitset array storing candidate bits for all cells */

private:
    /**
     * @brief Convert a GPos (x,y) to the starting bit index in the flat bitset.
     * @param gpos Global position of the cell.
     * @return Base bit index (0..728) corresponding to candidate '1' of that cell.
     */
    static ui gpos2bitsetIndex(GPos gpos);

    //======== Internal DFS helper ========

    /**
     * @brief Internal recursive DFS solver with listeners for tracking steps.
     *
     * This function applies logical simplification, checks for solution,
     * chooses the next cell by MRV, and branches on each candidate. On failure,
     * the board state is rolled back via copy/assignment.
     *
     * @param board The current board state (passed by reference).
     * @param path Vector of branch indices taken so far (for tracing).
     * @param assigned Boolean array of size 81 indicating which cells are assigned.
     * @param assignListener Callback invoked when a value is assigned to a cell.
     * @param simplifyListener Callback invoked after each simplification step.
     * @param eliminateListener Callback invoked on each candidate elimination.
     * @param isFirst Flag indicating whether this is the first simplification group.
     * @return true if a valid solution is found, false on contradiction or dead end.
     */
    bool dfsSolve(
        SudokuBoard& board,
        std::vector<ui>& path,
        bool assigned[81],
        std::function<void(const std::vector<ui>& path, const bool assigned[81], const GPos& justAssigned)> assignListener,
        std::function<void(const std::vector<ui>& path, const ui& index, const ui& eliminated, const ulli& eliminatedSum, const bool isFirstSimplificationGroup, const bool assigned[81])> simplifyListener,
        std::function<void(const std::vector<ui>& path, const SimplificationCause& cause, const GPos& cell, const uc& value, const uc& by)> eliminateListener,
        bool isFirst
    );

public:
    //======== Constructors & Assignment ========

    /**
     * @brief Default constructor. Initializes all 729 bits to 1 (all candidates possible).
     */
    SudokuBoard();

    /**
     * @brief Construct board from existing bitset data.
     * @param data Array of 12 ulli containing candidate bits.
     */
    SudokuBoard(std::array<ulli, 12> data);

    /**
     * @brief Delete copy constructor to prevent accidental copying.
     */
    SudokuBoard(const SudokuBoard& other) = delete;

    /**
     * @brief Delete copy assignment operator to prevent accidental copying.
     */
    SudokuBoard& operator=(const SudokuBoard& other) = delete;

    /**
     * @brief Move constructor; steals bitset data from other.
     * @param other Source board to move from.
     */
    SudokuBoard(SudokuBoard&& other) noexcept;

    /**
     * @brief Move assignment operator; steals bitset data from other.
     * @param other Source board to move from.
     * @return Reference to this board.
     */
    SudokuBoard& operator=(SudokuBoard&& other) noexcept;

    //======== Candidate Management ========

    /**
     * @brief Assign a definite value to a cell by eliminating all other candidates.
     *
     * If force == true, even if the specified value is currently impossible,
     * it will be turned on. Otherwise, only other candidates are turned off.
     *
     * @param gpos Global position of the cell to assign.
     * @param value Value to assign (1..9).
     * @param force If true, force-enable the given candidate even if it was false.
     */
    void makeSureAt(const GPos gpos, const uc value, const bool force);

    /**
     * @brief Check if a particular value is possible at a given cell.
     * @param gpos Global position of the cell.
     * @param value Candidate value to check (1..9).
     * @return true if that bit is set, false otherwise.
     * @throw std::invalid_argument if value is out of range.
     */
    bool isPossibleAt(const GPos gpos, const uc value) const;

    /**
     * @brief Set or clear a candidate bit at a specific cell.
     * @param gpos Global position of the cell.
     * @param value Candidate value to set or clear (1..9).
     * @param isPossible If true, set the bit; if false, clear the bit.
     * @return true if the bit state changed, false if no change was needed.
     * @throw std::invalid_argument if value is out of range.
     */
    bool setPossibleAt(const GPos gpos, const uc value, const bool isPossible);

    /**
     * @brief Get the number of possible candidates at a cell.
     * @param gpos Global position of the cell.
     * @return Number of bits set (0..9).
     */
    uc getPossiblesCountAt(const GPos gpos) const;

    /**
     * @brief If only one candidate remains at the cell, return it; otherwise return 0.
     * @param gpos Global position of the cell.
     * @return The single remaining candidate (1..9) or 0 if multiple or none.
     */
    uc getOnlyPossibleValue(const GPos gpos) const;

    /**
     * @brief Get the only possible candidate and update count reference.
     *
     * Iterates through all 9 candidates, increments count for each set bit,
     * and records the last seen candidate. If exactly one bit is set, returns
     * that candidate value; otherwise returns 0.
     *
     * @param gpos Global position of the cell.
     * @param count Reference to uc that will be set to the number of candidates.
     * @return Single candidate (1..9) if count == 1, else 0.
     */
    uc getCellInfoAt(const GPos gpos, uc& count) const;

    /**
     * @brief Get a vector of all possible candidate values at a cell.
     * @param gpos Global position of the cell.
     * @return Vector of uc containing all values v (1..9) where isPossibleAt(gpos,v) == true.
     */
    std::vector<uc> getCandiatesAt(const GPos gpos) const;

    //======== Simplification ========

    /**
     * @brief Perform a single pass of logical simplification on the board.
     *
     * Applies Naked Single and Hidden Single rules across all cells:
     *   1. If a cell has exactly one candidate, eliminate that value from peers in row, column, chunk.
     *   2. For each remaining candidate in a multi-candidate cell, if no other cell in the same
     *      house (row/column/chunk) can hold that candidate, assign it to this cell.
     *
     * @param eliminations Reference to ui. Number of candidate bits cleared during this pass.
     * @param eliminateListener Callback invoked for each elimination or assignment event. Arguments:
     *        (cause, cellPosition, value, houseIndexOrPeerIndex)
     * @return false if a contradiction (cell with zero candidates) is found; true otherwise.
     */
    bool simplify(
        ui& eliminations,
        std::function<void(const SimplificationCause& cause, const GPos& cell, const uc& value, const uc& by)> eliminateListener
    );

    /**
     * @brief Repeatedly apply simplify() until no further eliminations occur or contradiction appears.
     *
     * Invokes simplifyListener after each pass with (iterationIndex, eliminatedThisPass, totalEliminatedSoFar).
     * Invokes eliminateListener for each individual elimination or assignment inside simplify().
     *
     * @param totalEliminations Reference to ulli that accumulates total number of eliminated bits.
     * @param simplifyListener Callback invoked after each simplification iteration.
     * @param eliminateListener Callback invoked for each elimination/assignment inside simplify().
     * @return false if a contradiction occurred during any pass; true if board is stable (no more eliminations).
     */
    bool simplifyToTheEnd(
        ulli& totalEliminations,
        std::function<void(const ui& index, const ui& eliminated, const ulli& eliminatedSum)> simplifyListener,
        std::function<void(const SimplificationCause& cause, const GPos& cell, const uc& value, const uc& by)> eliminateListener
    );

    //======== Status Checks ========

    /**
     * @brief Check if the board is completely solved (each cell has exactly one candidate).
     * @return true if solved; false otherwise.
     */
    bool isSolved() const;

    /**
     * @brief Check if any cell has zero candidates, indicating a contradiction.
     * @return true if a contradiction exists; false otherwise.
     */
    bool hasContradiction() const;

    //======== Heuristic Selection ========

    /**
     * @brief Find the cell with Minimum Remaining Values (MRV) heuristic.
     *
     * Scans all cells, tracks the minimum candidate count (>1). If any cell has 0 candidates,
     * immediately returns that cell with count = 0 to indicate contradiction.
     *
     * @return Pair of (GPos, uc) where uc is candidate count. If count is 0, contradiction.
     * @throw std::runtime_error if no multi-candidate cell is found (unexpected).
     */
    std::pair<GPos, uc> findMRVCell() const;

    //======== Data Copy ========

    /**
     * @brief Copy raw bitset data for rollback or inspection.
     * @return std::array<ulli,12> containing current bitset state.
     */
    std::array<ulli, 12> copyData() const;

    //======== Public DFS Overloads ========

    /**
     * @brief Public DFS solver entry point with full tracking.
     *
     * Clears path and initializes first branch index to 0, then calls internal dfsSolve.
     *
     * @param path Vector to record branch decisions.
     * @param assigned Boolean array (size 81) indicating which cells have been assigned.
     * @param assignListener Callback invoked when a candidate is assigned to a cell.
     * @param simplifyListener Callback invoked after each simplifyToTheEnd iteration.
     * @param eliminateListener Callback invoked on each elimination or assignment inside simplify().
     * @return true if a solution is found; false otherwise.
     */
    bool dfsSolve(
        std::vector<ui>& path,
        bool assigned[81],
        std::function<void(const std::vector<ui>& path, const bool assigned[81], const GPos& justAssigned)> assignListener,
        std::function<void(const std::vector<ui>& path, const ui& index, const ui& eliminated, const ulli& eliminatedSum, const bool isFirstSimplificationGroup, const bool assigned[81])> simplifyListener,
        std::function<void(const std::vector<ui>& path, const SimplificationCause& cause, const GPos& cell, const uc& value, const uc& by)> eliminateListener
    );

    /**
     * @brief Public DFS solver entry point without branch path tracking.
     *
     * Initializes a local path vector and calls the full-tracking dfsSolve.
     *
     * @param assigned Boolean array (size 81) indicating which cells have been assigned.
     * @param assignListener Callback invoked when a candidate is assigned.
     * @param simplifyListener Callback invoked after each simplifyToTheEnd iteration.
     * @param eliminateListener Callback invoked on each elimination or assignment inside simplify().
     * @return true if a solution is found; false otherwise.
     */
    bool dfsSolve(
        bool assigned[81],
        std::function<void(const std::vector<ui>& path, const bool assigned[81], const GPos& justAssigned)> assignListener,
        std::function<void(const std::vector<ui>& path, const ui& index, const ui& eliminated, const ulli& eliminatedSum, const bool isFirstSimplificationGroup, const bool assigned[81])> simplifyListener,
        std::function<void(const std::vector<ui>& path, const SimplificationCause& cause, const GPos& cell, const uc& value, const uc& by)> eliminateListener
    );

    /**
     * @brief Simplified DFS solver entry point with no listeners.
     *
     * Calls dfsSolve(path, assigned, emptyListeners).
     *
     * @param path Vector to record branch decisions.
     * @param assigned Boolean array (size 81) indicating which cells have been assigned.
     * @return true if solved; false otherwise.
     */
    bool dfsSolve(std::vector<ui>& path, bool assigned[81]);

    /**
     * @brief Simplest DFS solver entry point with no listeners and no path output.
     *
     * Initializes path vector internally and calls dfsSolve(path, assigned).
     *
     * @param assigned Boolean array (size 81) indicating which cells have been assigned.
     * @return true if solved; false otherwise.
     */
    bool dfsSolve(bool assigned[81]);
};
