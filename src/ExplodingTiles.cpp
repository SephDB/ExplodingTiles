#include <string_view>
#include <iostream>
#include <optional>
#include <vector>
#include <memory>
#include <cmath>
#include <SFML/Window.hpp>
#include <SFML/Graphics.hpp>

#include "coords.hpp"
#include "board.hpp"
#include "player.hpp"
#include "shapes.hpp"

std::string_view board_shader = R"(
const float edge_thickness = 0.04;
const vec3 edge_color = vec3(1);
const vec3 highlight_color = vec3(1,0,1);

uniform int hex_size;
uniform ivec3 selected;

float min3(vec3 v) {
	return min(min(v.x,v.y),v.z);
}

float max3(vec3 v) {
	return max(max(v.x,v.y),v.z);
}

void main()
{
	vec3 min_bound = vec3(1);
	vec3 max_bound = vec3(hex_size*2+1);
	vec3 bound_edge = vec3(edge_thickness*2);

	vec3 coordinates = (gl_Color.rgb) * (hex_size+1) * 3;
	ivec3 coords = floor(coordinates);
	
	ivec3 larger_selected = selected + ivec3(1);
	
	
	if(all(greaterThan(coordinates,min_bound)) && all(lessThan(coordinates,max_bound))) {
		//Inner edge
		vec3 distance = min(fract(coordinates),ceil(coordinates)-coordinates);
		if(larger_selected == floor(coordinates)) {
			float mix = smoothstep(0,edge_thickness/2,min3(distance));
			gl_FragColor.rgb = edge_color * (1-mix) + mix*highlight_color;
			gl_FragColor.a = 1-smoothstep(edge_thickness/2,edge_thickness*1.5,min3(distance))*0.3;
		} else {
			gl_FragColor.rgb = edge_color;
			gl_FragColor.a = 1-smoothstep(edge_thickness*0.8,edge_thickness,min3(distance));
		}
	} else if(all(greaterThan(coordinates,min_bound-bound_edge)) && all(lessThan(coordinates,max_bound+bound_edge))) {
		//Outer edge
		vec3 distance = max(min_bound - coordinates, coordinates - max_bound);
		gl_FragColor.rgb = edge_color;
		gl_FragColor.a = 1 - smoothstep(edge_thickness*1.5,edge_thickness*2,max3(distance));
	} else {
		gl_FragColor = 0;
	}
}
)";

float dot(sf::Vector2f a, sf::Vector2f b) {
	return a.x * b.x + a.y * b.y;
}

template<typename Vec>
Vec lerp(Vec a, Vec b, float val) {
	return a + (b - a) * val;
}


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

constexpr float explosion_length = 0.5f;

class VisualGame : public sf::Drawable {
	sf::Vertex outer[3];
	sf::Shader shader;

	sf::Vector2f inner[3];
	BoardWithPlayers board;

	sf::CircleShape explode;
	sf::Clock explode_timer;

	sf::VertexArray reset_arrow;

	sf::Transform show_current_player;
	sf::Transform show_winner;
	std::vector<sf::CircleShape> players;
	
	TriCoord mouseToBoard(sf::Vector2f mouse) const {
		float length = inner[1].y - inner[0].y;
		float v1 = 1 - dot(mouse - inner[0], sf::Vector2f(0, 1)) / length;
		float v2 = 1 - dot(mouse - inner[1], inner[2] + (inner[0] - inner[2]) / 2.f - inner[1]) / (length * length);

		sf::Vector3i bary = sf::Vector3i(3.f * board.getBoard().size() * sf::Vector3f(v1, v2, 1 - v1 - v2));

		//ensure out of bounds coordinate when a coordinate < 0, converting to int != floor. Subtract one if a coordinate was below 0
		bary -= sf::Vector3i(v1 < 0, v2 < 0, v1 + v2 > 1);

		return TriCoord(bary, board.getBoard().size());
	}

public:

	VisualGame(sf::Vector2f center, float radius, int board_size) : board(board_size) {
		float r = radius * (board_size + 1) / board_size; //Widen for outer edge
		float sqrt3 = std::sqrt(3.f);
		outer[0] = sf::Vertex({ center.x, center.y - 2 * r }, sf::Color::Red);
		inner[0] = { center.x, center.y - 2 * radius };
		outer[1] = sf::Vertex({ center.x + r * sqrt3, center.y + r }, sf::Color::Green);
		inner[1] = { center.x + radius * sqrt3,center.y + radius };
		outer[2] = sf::Vertex({ center.x - r * sqrt3, center.y + r }, sf::Color::Blue);
		inner[2] = { center.x - radius * sqrt3,center.y + radius };

		{
			sf::MemoryInputStream stream;
			stream.open(board_shader.data(), board_shader.size());
			shader.loadFromStream(stream, sf::Shader::Type::Fragment);
		}

		shader.setUniform("hex_size", board_size);
		shader.setUniform("selected", sf::Vector3i());

		float size = radius / (board_size * 6 + 3);

		explode = sf::CircleShape(size * 3);
		explode.setFillColor(sf::Color::Yellow);
		explode.setOutlineColor(sf::Color::Red);
		explode.setOutlineThickness(-size / 2);
		explode.setOrigin(size * 3, size * 3);

		sf::Transform rot = sf::Transform().rotate(-60);

		sf::Vector2f extra_offset{ 0, radius*1.4f };

		extra_offset = rot.transformPoint(extra_offset);

		show_current_player = sf::Transform().translate(center - extra_offset).scale(sf::Vector2f(1 / size, 1 / size) * (radius / 8));
		show_winner = sf::Transform().translate(center).scale(sf::Vector2f(1, 1) / size * radius);

		extra_offset = rot.transformPoint(extra_offset);

		reset_arrow = circArrow(center - extra_offset, sf::Color::White, 15, 24, 5);
	}

	void addPlayer(int polygon_n, sf::Color color, std::unique_ptr<Player> controller) {
		board.addPlayer(std::move(controller));
		float radius = (inner[1].y - inner[0].y) / 3;
		players.push_back(playerShape(polygon_n,color,radius/(board.getBoard().size() * 6 + 3)));
	}

	void mouseMove(sf::Vector2f mouse) {
		board.getCurrentPlayer().onInput(input_events::MouseMove{ mouseToBoard(mouse) });
	}

	void onClick(sf::Vector2f mouse) {
		if (reset_arrow.getBounds().contains(mouse)) board.reset();
		else board.getCurrentPlayer().onInput(input_events::MouseClick{ mouseToBoard(mouse) });
	}

	void update() {
		if (not board.getWinner() && (not board.getBoard().needsUpdate() || explode_timer.getElapsedTime().asSeconds() > explosion_length)) {
			board.update();
			explode_timer.restart();
		}
		shader.setUniform("selected", board.getCurrentPlayer().selected().bary(board.getBoard().size()));
	}

	void draw(sf::RenderTarget& target, sf::RenderStates states) const override {
		target.draw(outer, 3, sf::PrimitiveType::Triangles, { states.blendMode, states.transform, states.texture, &shader });

		const float explosion_progress = explode_timer.getElapsedTime().asSeconds() / explosion_length;

		auto draw_tile = [this, explosion_progress, &target](TriCoord c, sf::RenderStates states) {
			auto s = board.getBoard()[c];
			if (s.num == 0) return;

			auto [x, y, z] = c.tri_center(board.getBoard().size());

			auto center = x * inner[0] + y * inner[1] + z * inner[2];
			
			states.transform.translate(center);
			
			const sf::CircleShape& circle = players[s.player];

			if (s.num > board.getBoard().allowedPieces(c)) {
				auto explode_state = states;
				float scale = lerp(0.3f, 1.0f, explosion_progress);
				explode_state.transform.scale(scale,scale);
				target.draw(explode, explode_state);
				for (const auto& n : c.neighbors()) {
					if (board.getBoard().inBounds(n)) {
						auto [x2, y2, z2] = n.tri_center(board.getBoard().size());
						auto move_target = x2 * inner[0] + y2 * inner[1] + z2 * inner[2] - center;
						auto move_state = states;
						move_state.transform.translate(lerp(move_target/3.f,move_target,explosion_progress));
						target.draw(circle, move_state);
					}
				}
				return;
			}

			sf::Vector2f offset{};

			if (s.num == 2 && c.R) {
				states.transform.translate(-circle.getOrigin() / 2.f);
			}

			target.draw(circle, states);

			if (s.num == 2) {
				states.transform.translate(circle.getOrigin() * 2.f / 3.f);
				target.draw(circle, states);
			}
		};

		if (auto player = board.getWinner(); player) {
			target.draw(players[*player], { show_current_player });
			target.draw(players[*player], { show_winner });
		}
		else {
			board.getBoard().iterTiles([&](auto c) {draw_tile(c, states); return true; });
			target.draw(players[board.getCurrentPlayerNum()], { show_current_player });
		}

		target.draw(reset_arrow, states);
	}
};

int main()
{
	std::random_device random;

	sf::RenderWindow window(sf::VideoMode(800, 600), "Exploding Tiles");

	window.setFramerateLimit(60);

	VisualGame game({ 400,300 }, 250, 3);
	game.addPlayer(3, sf::Color::Green, std::make_unique<MousePlayer>());
	game.addPlayer(5, sf::Color::Red, std::make_unique<AI::InteractiveAIPlayer>(AI::AIPlayer(AI::randomAI(random))));

	while (window.isOpen())
	{
		sf::Event event;
		while (window.pollEvent(event))
		{
			switch (event.type) {
			case sf::Event::Closed:
				window.close();
				return 0;
			case sf::Event::MouseButtonReleased:
				game.onClick(sf::Vector2f{ sf::Mouse::getPosition(window) });
				break;
			case sf::Event::MouseMoved:
				game.mouseMove(sf::Vector2f{ sf::Mouse::getPosition(window) });
				break;
			default:
				break;
			}
		}

		game.update();

		window.clear(sf::Color(0x4A4AB5FF));

		window.draw(game);

		window.display();
	}

	return 0;
}
