#include "SudokuBoard.h"

#include <functional>
#include <stdexcept>
#include <iostream>
#include <utility>
#include <vector>
#include <array>



Tuple2::Tuple2(uc x_val, uc y_val)
    : x(x_val), y(y_val) {}
uc Tuple2::getX() const {
    return x;
}
uc Tuple2::getY() const {
    return y;
}

GPos::GPos() : Tuple2(0, 0) {}
GPos::GPos(uc x, uc y) : Tuple2(x, y) {}

ui SudokuBoard::gpos2bitsetIndex(GPos gpos) {
    // Compute a 0-based cell index from (x,y), then multiply by 9 for candidate bits
    ui cellIndex = (ui)gpos.getX() + (ui)gpos.getY() * 9u;
    return cellIndex * 9u;
}

bool SudokuBoard::dfsSolve(SudokuBoard& board, std::vector<ui>& path, bool assigned[81],
    std::function<void(const std::vector<ui>& path, const bool assigned[81], const GPos& justAssigned)> assignListener,
    std::function<void(const std::vector<ui>& path, const ui& index, const ui& eliminated, const ulli& eliminatedSum, const bool isFirstSimplificationGroup, const bool assigned[81])> simplifyListener,
    std::function<void(const std::vector<ui>& path, const SimplificationCause& cause, const GPos& cell, const uc& value, const uc& by)> eliminateListener, bool isFirst) {
    // First, repeatedly simplify the board using logical rules
    ulli totalEliminations;
    if (!board.simplifyToTheEnd(
        totalEliminations,
        // Wrap the simplifyListener to include path and isFirst flag
        [path, simplifyListener, isFirst, assigned](ui i, ui elim, ui elimSum) {
            simplifyListener(path, i, elim, elimSum, isFirst, assigned);
        },
        // Wrap the eliminateListener for each individual elimination event
        [path, eliminateListener](const SimplificationCause& cause, const GPos& cell, const uc& value, const uc& by) {
            eliminateListener(path, cause, cell, value, by);
        })
        ) {
        // Contradiction found during simplification
        return false;
    }

    // If all cells now have exactly one candidate, puzzle is solved
    if (board.isSolved())
        return true;

    // Choose next cell by MRV (minimum remaining values)
    auto [pos, cnt] = board.findMRVCell();
    if (cnt == 0) {
        // No candidates left for some cell => dead end
        return false;
    }

    uc x = pos.getX();
    uc y = pos.getY();

    // Retrieve list of possible values for this cell
    std::vector<uc> candidates = getCandiatesAt(pos);

    // Mark this cell as assigned in the local boolean array
    assigned[x + 9 * y] = true;
    ui branchIndex = 0;

    // Try each candidate in turn
    for (uc v : candidates) {
        // Save current bitset to history for rollback if needed
        std::array<ulli, 12> history = board.copyData();

        // Force this cell to value v (eliminate other bits)
        board.makeSureAt(pos, v, false);
        // Record which branch we're taking
        path.push_back(branchIndex++);
        // Notify listener that a value was assigned
        assignListener(path, assigned, pos);

        // Recurse
        if (dfsSolve(board, path, assigned, assignListener, simplifyListener, eliminateListener, false))
            return true;

        // If recursion failed, rollback board state and path
        path.pop_back();
        board = SudokuBoard(history);
    }

    // Unmark assignment on backtrack
    assigned[x + 9 * y] = false;
    return false;
}


SudokuBoard::SudokuBoard() {
    // Initialize all bits to 1 (all candidates possible for every cell)
    for (uc i = 0; i < 12; i++) {
        bitset[i] = ~0ULL;
    }
}
SudokuBoard::SudokuBoard(std::array<ulli, 12> data) : bitset(data) {}

// MOVE: Simply steal the bitset contents from other
SudokuBoard::SudokuBoard(SudokuBoard&& other) noexcept {
    std::copy(std::begin(other.bitset), std::end(other.bitset), std::begin(this->bitset));
}
SudokuBoard& SudokuBoard::operator=(SudokuBoard&& other) noexcept {
    if (this != &other) {
        std::copy(std::begin(other.bitset), std::end(other.bitset), std::begin(this->bitset));
    }
    return *this;
}

// ---------------- Candidate management ----------------

void SudokuBoard::makeSureAt(const GPos gpos, const uc value, const bool force) {
    // To set cell to 'value', eliminate all other candidate bits
    for (uc v = 1; v <= 9; v++) {
        if (v == value) {
            if (force)
                // If forcing, ensure this bit is turned on even if it was off
                setPossibleAt(gpos, v, true);
            // If not forcing, leave the bit as-is (if it was already off, we keep it off)
        } else {
            // Eliminate all other values
            setPossibleAt(gpos, v, false);
        }
    }
}

bool SudokuBoard::isPossibleAt(const GPos gpos, const uc value) const {
    if (!(1 <= value && value <= 9))
        throw std::invalid_argument("SudokuBoard::isPossibleAt: value out of range.");

    // Compute the flat bit index corresponding to (gpos, value)
    ui base_index = gpos2bitsetIndex(gpos);         // cellIndex * 9
    ui bit_index = base_index + (value - 1); // 0..728

    // Determine which 64-bit block and bit offset within that block
    ui bsci = bit_index / 64;
    ui bsii = bit_index % 64;
    // Return true if that bit is set
    return (bitset[bsci] & (1ULL << bsii)) != 0ULL;
}

bool SudokuBoard::setPossibleAt(const GPos gpos, const uc value, const bool isPossible) {
    if (!(1 <= value && value <= 9))
        throw std::invalid_argument("SudokuBoard::setPossibleAt: value out of range.");

    // Compute flat bit index
    ui base_index = gpos2bitsetIndex(gpos);
    ui bit_index = base_index + (value - 1);
    // Locate block and offset
    ui bsci = bit_index / 64;
    ui bsii = bit_index % 64;
    ulli mask = (1ULL << bsii);

    // Check current bit state
    bool currently = (bitset[bsci] & mask) != 0ULL;
    if (currently == isPossible) {
        // No change needed
        return false;
    }

    if (isPossible) {
        // Turn bit on
        bitset[bsci] |= mask;
    } else {
        // Turn bit off
        bitset[bsci] &= ~mask;
    }

    return true;
}

uc SudokuBoard::getCellInfoAt(const GPos gpos, uc& count) const {
    count = 0;
    uc onlyValue = 0;
    // Count how many candidate bits are set, remember last seen value
    for (uc v = 1; v <= 9; v++) {
        if (isPossibleAt(gpos, v)) {
            onlyValue = v;
            count++;
            if (count > 1) {
                // We don't stop early; we want count to reflect >1
                continue;
            }
        }
    }
    // If exactly one candidate, return it; otherwise return 0
    return (count == 1 ? onlyValue : 0);
}

uc SudokuBoard::getOnlyPossibleValue(const GPos gpos) const {
    uc cnt;
    return getCellInfoAt(gpos, cnt);
}

uc SudokuBoard::getPossiblesCountAt(const GPos gpos) const {
    uc cnt;
    getCellInfoAt(gpos, cnt);
    return cnt;
}

std::vector<uc> SudokuBoard::getCandiatesAt(const GPos gpos) const {
    std::vector<uc> candidates;
    // Collect all values v where the bit is set
    for (uc v = 1; v <= 9; v++) {
        if (this->isPossibleAt(gpos, v)) {
            candidates.push_back(v);
        }
    }
    return candidates;
}

bool SudokuBoard::simplify(ui& eliminations, std::function<void(const SimplificationCause& cause, const GPos& cell, const uc& value, const uc& by)> eventListener) {
    eliminations = 0;
    // Iterate over every cell in row-major order
    for (uc y = 0; y < 9; y++) {
        for (uc x = 0; x < 9; x++) {
            GPos selfPos(x, y);
            uc cnt;
            uc onlyVal = getCellInfoAt(selfPos, cnt);

            if (cnt == 0) {
                // No candidates => contradiction
                eventListener(NO_VALUE_POSSIBLE, selfPos, 0, 0);
                return false;
            }

            // Compute chunk coordinates
            uc chunkX = x / 3;
            uc chunkY = y / 3;
            uc chunkStartX = chunkX * 3;
            uc chunkStartY = chunkY * 3;

            if (cnt == 1) {
                // Naked Single: eliminate this fixed value from peers

                // Eliminate from row
                for (uc cx = 0; cx < 9; cx++) {
                    if (cx == x) continue;
                    GPos peer(cx, y);
                    if (isPossibleAt(peer, onlyVal)) {
                        setPossibleAt(peer, onlyVal, false);
                        eliminations++;
                        eventListener(ELIMINATION_BY_ROW, peer, onlyVal, y);
                    }
                }
                // Eliminate from column
                for (uc cy = 0; cy < 9; cy++) {
                    if (cy == y) continue;
                    GPos peer(x, cy);
                    if (isPossibleAt(peer, onlyVal)) {
                        setPossibleAt(peer, onlyVal, false);
                        eliminations++;
                        eventListener(ELIMINATION_BY_COLUMN, peer, onlyVal, x);
                    }
                }
                // Eliminate from chunk
                for (uc by = chunkStartY; by < chunkStartY + 3; by++) {
                    for (uc bx = chunkStartX; bx < chunkStartX + 3; bx++) {
                        if (bx == x && by == y) continue;
                        GPos peer(bx, by);
                        if (isPossibleAt(peer, onlyVal)) {
                            setPossibleAt(peer, onlyVal, false);
                            eliminations++;
                            // Pass chunk index as chunkX + 3*chunkY
                            eventListener(ELIMINATION_BY_CHUNK, peer, onlyVal, chunkX + 3 * chunkY);
                        }
                    }
                }
            }

            // Re-evaluate candidate count after naked single elimination
            onlyVal = getCellInfoAt(selfPos, cnt);
            if (cnt == 0) {
                eventListener(NO_VALUE_POSSIBLE, selfPos, 0, 0);
                return false;
            }
            if (cnt == 1)
                continue; // Already handled as Naked Single

            // Hidden Single checks: for each candidate v,
            // see if it's unique in row, column, or chunk
            for (uc v = 1; v <= 9; v++) {
                if (!isPossibleAt(selfPos, v)) continue;
                bool success;

                // Check row uniqueness
                success = true;
                for (uc cx = 0; cx < 9; cx++) {
                    if (cx == x) continue;
                    GPos peer(cx, y);
                    if (isPossibleAt(peer, v)) {
                        success = false;
                    }
                }
                if (success) {
                    eliminations += cnt - 1;
                    eventListener(VALUE_SURE_BY_ROW, selfPos, v, y);
                    makeSureAt(selfPos, v, false);
                }

                // Check column uniqueness
                success = true;
                for (uc cy = 0; cy < 9; cy++) {
                    if (cy == y) continue;
                    GPos peer(x, cy);
                    if (isPossibleAt(peer, v)) {
                        success = false;
                    }
                }
                if (success) {
                    eliminations += cnt - 1;
                    eventListener(VALUE_SURE_BY_COLUMN, selfPos, v, x);
                    makeSureAt(selfPos, v, false);
                }

                // Check chunk uniqueness
                success = true;
                for (uc by = chunkStartY; by < chunkStartY + 3; by++) {
                    for (uc bx = chunkStartX; bx < chunkStartX + 3; bx++) {
                        if (bx == x && by == y) continue;
                        GPos peer(bx, by);
                        if (isPossibleAt(peer, v)) {
                            success = false;
                            break;
                        }
                    }
                    if (!success) break;
                }
                if (success) {
                    eliminations += cnt - 1;
                    eventListener(VALUE_SURE_BY_CHUNK, selfPos, v, chunkX + 3 * chunkY);
                    makeSureAt(selfPos, v, false);
                }
            }
        }
    }
    return true;
}

bool SudokuBoard::simplifyToTheEnd(ulli& totalEliminations,
    std::function<void(const ui& index, const ui& eliminated, const ulli& eliminatedSum)> simplifyListener,
    std::function<void(const SimplificationCause& cause, const GPos& cell, const uc& value, const uc& by)> eliminateListener) {
    totalEliminations = 0;
    ui index = 0;

    // Keep applying simplify() until no more eliminations or contradiction
    while (true) {
        ui eliminated;
        if (!this->simplify(eliminated, eliminateListener)) {
            // A contradiction occurred in simplify()
            totalEliminations += eliminated;
            simplifyListener(index++, eliminated, totalEliminations);
            return false;
        }
        if (eliminated == 0) break; // No further changes
        totalEliminations += eliminated;
        simplifyListener(index++, eliminated, totalEliminations);
    }
    return true;
}

bool SudokuBoard::isSolved() const {
    // Check that every cell has exactly one candidate
    for (uc y = 0; y < 9; y++) {
        for (uc x = 0; x < 9; x++) {
            GPos gp(x, y);
            uc cnt;
            getCellInfoAt(gp, cnt);
            if (cnt != 1) return false;
        }
    }
    return true;
}

bool SudokuBoard::hasContradiction() const {
    // Check if any cell has zero candidates
    for (uc y = 0; y < 9; y++) {
        for (uc x = 0; x < 9; x++) {
            uc cnt;
            getCellInfoAt(GPos(x, y), cnt);
            if (cnt == 0) return true;
        }
    }
    return false;
}

std::pair<GPos, uc> SudokuBoard::findMRVCell() const {
    GPos bestPos(0, 0);
    uc bestCount = 10; // Anything >9 is effectively "infinite"

    // Iterate all cells to find the one with 2..9 candidates minimum
    for (uc y = 0; y < 9; y++) {
        for (uc x = 0; x < 9; x++) {
            GPos gp(x, y);
            uc cnt;
            getCellInfoAt(gp, cnt);
            if (cnt == 0) {
                // Contradiction: return immediately with count=0
                return { gp, 0 };
            }
            if (cnt > 1 && cnt < bestCount) {
                bestCount = cnt;
                bestPos = gp;
            }
        }
    }
    if (bestCount == 10) {
        // All cells have exactly 1 candidate: should be solved already
        throw new std::runtime_error("Unexpected state in findMRVCell");
    }
    return { bestPos, bestCount };
}

std::array<ulli, 12> SudokuBoard::copyData() const {
    // Return a copy of the underlying bitset for rollback
    return this->bitset;
}

bool SudokuBoard::dfsSolve(std::vector<ui>& path, bool assigned[81],
    std::function<void(const std::vector<ui>& path, const bool assigned[81], const GPos& justAssigned)> assignListener,
    std::function<void(const std::vector<ui>& path, const ui& index, const ui& eliminated, const ulli& eliminatedSum, const bool isFirstSimplificationGroup, const bool assigned[81])> simplifyListener,
    std::function<void(const std::vector<ui>& path, const SimplificationCause& cause, const GPos& cell, const uc& value, const uc& by)> eliminateListener) {
    // Initialize path with a dummy 0 to simplify recursion logic
    path.clear();
    path.push_back(0);
    return dfsSolve(*this, path, assigned, assignListener, simplifyListener, eliminateListener, true);
}

bool SudokuBoard::dfsSolve(bool assigned[81],
    std::function<void(const std::vector<ui>& path, const bool assigned[81], const GPos& justAssigned)> assignListener,
    std::function<void(const std::vector<ui>& path, const ui& index, const ui& eliminated, const ulli& eliminatedSum, const bool isFirstSimplificationGroup, const bool assigned[81])> simplifyListener,
    std::function<void(const std::vector<ui>& path, const SimplificationCause& cause, const GPos& cell, const uc& value, const uc& by)> eliminateListener) {
    // Create a local path vector and call the main recursive solver
    std::vector<ui> path;
    return dfsSolve(path, assigned, assignListener, simplifyListener, eliminateListener);
}

bool SudokuBoard::dfsSolve(std::vector<ui>& path, bool assigned[81]) {
    // Call DFS with empty listeners (no output)
    return dfsSolve(path, assigned,
        [](const std::vector<ui>& path, const bool assigned[81], const GPos& justAssigned) {},
        [](const std::vector<ui>& path, const ui& index, const ui& eliminated, const ulli& eliminatedSum, const bool isFirstSimplificationGroup, const bool assigned[81]) {},
        [](const std::vector<ui>& path, const SimplificationCause& cause, const GPos& cell, const uc& value, const uc& by) {}
    );
}

bool SudokuBoard::dfsSolve(bool assigned[81]) {
    // Simplest entry point: create path internally
    std::vector<ui> path;
    return dfsSolve(path, assigned);
}
