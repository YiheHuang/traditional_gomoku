#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/chrono.h>
#include "board.h"
#include "search.h"
#include "game.h"
#include "movegen.h"
#include "pattern.h"

namespace py = pybind11;

PYBIND11_MODULE(_gomoku_core, m) {
    m.doc() = "Gomoku AI - C++ core engine with Alpha-Beta search";

    // ── constants ──────────────────────────────────────────
    m.attr("BOARD_SIZE") = BOARD_SIZE;
    m.attr("EMPTY")      = EMPTY;
    m.attr("BLACK")      = BLACK;
    m.attr("WHITE")      = WHITE;

    // ── Move ──────────────────────────────────────────────
    py::class_<Move>(m, "Move")
        .def(py::init<>())
        .def(py::init<int, int>(), py::arg("row"), py::arg("col"))
        .def_readwrite("row", &Move::row)
        .def_readwrite("col", &Move::col)
        .def("valid",  &Move::valid)
        .def("index",  &Move::index)
        .def("__eq__", [](const Move& a, const Move& b) { return a == b; })
        .def("__repr__", [](const Move& m) {
            return "Move(" + std::to_string(m.row) + ", " + std::to_string(m.col) + ")";
        });

    // ── Board ──────────────────────────────────────────────
    py::class_<Board>(m, "Board")
        .def(py::init<>())
        .def("reset",       &Board::reset)
        .def("makeMove",    &Board::makeMove, py::arg("row"), py::arg("col"))
        .def("undoMove",    &Board::undoMove)
        .def("get",         py::overload_cast<int, int>(&Board::get, py::const_),
             py::arg("row"), py::arg("col"))
        .def("isEmpty",     &Board::isEmpty, py::arg("row"), py::arg("col"))
        .def_static("inBounds", &Board::inBounds, py::arg("row"), py::arg("col"))
        .def_property_readonly("side",       &Board::side)
        .def_property_readonly("opp",        &Board::opp)
        .def_property_readonly("ply",        &Board::ply)
        .def("lastMove",    &Board::lastMove)
        .def("hash",        &Board::hash)
        .def("checkWinner", &Board::checkWinner)
        .def("isFull",      &Board::isFull);

    // ── SearchConfig ──────────────────────────────────────
    py::class_<SearchConfig>(m, "SearchConfig")
        .def(py::init<>())
        .def_readwrite("maxDepth",    &SearchConfig::maxDepth)
        .def_readwrite("timeMs",      &SearchConfig::timeMs)
        .def_readwrite("useTT",       &SearchConfig::useTT)
        .def_readwrite("useKiller",   &SearchConfig::useKiller)
        .def_readwrite("useHistory",  &SearchConfig::useHistory)
        .def_readwrite("useVCT",      &SearchConfig::useVCT)
        .def_readwrite("vctDepth",    &SearchConfig::vctDepth);

    // ── SearchResult ──────────────────────────────────────
    py::class_<SearchResult>(m, "SearchResult")
        .def(py::init<>())
        .def_readwrite("bestMove",   &SearchResult::bestMove)
        .def_readwrite("score",      &SearchResult::score)
        .def_readwrite("depth",      &SearchResult::depth)
        .def_readwrite("nodes",      &SearchResult::nodes)
        .def_readwrite("tbHits",     &SearchResult::tbHits)
        .def_readwrite("tbMisses",   &SearchResult::tbMisses)
        .def_readwrite("timeMs",     &SearchResult::timeMs);

    // ── SearchEngine ──────────────────────────────────────
    py::class_<SearchEngine>(m, "SearchEngine")
        .def(py::init<>())
        .def("search",        &SearchEngine::search, py::arg("board"),
             py::call_guard<py::gil_scoped_release>())
        .def("stop",          &SearchEngine::stop)
        .def("clear",         &SearchEngine::clear)
        .def("getConfig",     [](SearchEngine& e) -> SearchConfig& { return e.getConfig(); },
             py::return_value_policy::reference_internal)
        .def("setConfig",     &SearchEngine::setConfig, py::arg("config"))
        .def("getRootScores", &SearchEngine::getRootScores)
        .def("__repr__",      [](const SearchEngine&) { return "SearchEngine()"; });

    // ── GameResult ────────────────────────────────────────
    py::enum_<GameResult>(m, "GameResult")
        .value("ONGOING",    GameResult::ONGOING)
        .value("BLACK_WIN",  GameResult::BLACK_WIN)
        .value("WHITE_WIN",  GameResult::WHITE_WIN)
        .value("DRAW",       GameResult::DRAW);

    // ── Game ──────────────────────────────────────────────
    py::class_<Game>(m, "Game")
        .def(py::init<>())
        .def("reset",          &Game::reset)
        .def("makePlayerMove", &Game::makePlayerMove, py::arg("row"), py::arg("col"))
        .def("makeAIMove",     &Game::makeAIMove)
        .def("applyMove",      &Game::applyMove, py::arg("move"))
        .def("getBoard",       &Game::getBoard,
             py::return_value_policy::reference_internal)
        .def("result",         &Game::result)
        .def("isOver",         &Game::isOver)
        .def_property("aiIsBlack", &Game::isAIBlack, &Game::setAIBlack)
        .def("engine",         &Game::engine, py::return_value_policy::reference_internal);

    // ── analysis helper ────────────────────────────────────
    // Run ONE full search on `board`, returns root candidate (row, col, score).
    // Scores are from the current player's perspective.
    m.def("analyze_root", [](const Board& board, int timeMs) -> py::list {
        SearchEngine engine;
        SearchConfig cfg = engine.getConfig();
        cfg.timeMs = timeMs;
        engine.setConfig(cfg);

        // release GIL only during the heavy search
        {
            py::gil_scoped_release release;
            engine.search(board);
        }

        // GIL re-acquired — safe to build Python objects
        py::list result;
        for (auto& p : engine.getRootScores()) {
            py::dict e;
            e["row"] = p.first.row;
            e["col"] = p.first.col;
            e["score"] = p.second;
            result.append(e);
        }
        return result;
    }, py::arg("board"), py::arg("time_ms"));
}
