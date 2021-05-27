#pragma once

#include <vector>
#include <memory>
#include "board.hpp"
#include "player.hpp"

class BoardWithPlayers {
	Board board;
	int current_player = 0;
	std::vector<std::unique_ptr<Player>> players{};

	void makeMove(TriCoord c) {
		if (!board.incTile(c, current_player)) return;
		if (!board.needsUpdate())
			nextPlayer();
	}

	void nextPlayer() {
		current_player = (current_player + 1) % players.size();
		players[current_player]->startTurn(board, current_player);
	}

public:
	BoardWithPlayers(int size) : board(size) {}

	void addPlayer(std::unique_ptr<Player> player) {
		players.push_back(std::move(player));
		if (players.size() == 1) {
			players[0]->startTurn(board, 0);
		}
	}

	const Board& getBoard() const { return board; }
	int getCurrentPlayerNum() const { return current_player; }

	Player& getCurrentPlayer() { return *players[current_player]; }
	const Player& getCurrentPlayer() const { return *players[current_player]; }

	std::size_t getPlayerCount() const { return players.size(); }

	void update() {
		if (board.needsUpdate()) {
			board.update_step();
			if (!board.needsUpdate()) nextPlayer();
		}
		else if (auto m = players[current_player]->update(); m) {
			makeMove(*m);
		}
	}

	void reset() {
		board = { board.size() };
		current_player = 0;
		players[current_player]->startTurn(board, current_player);
	}

	std::optional<int> getWinner() const {
		std::size_t total = 0;
		int player = -1;
		bool multiple_players = false;
		board.iterTiles([&](TriCoord c) {
			auto board_player = board[c].player;
			if (board_player != -1) {
				total += board[c].num;
				if (player == -1) player = board_player;
				if (player != board_player) {
					multiple_players = true;
					return false;
				}
			}
			return true;
			});
		if (multiple_players || total <= players.size()) return std::nullopt;
		return player;
	}
};
