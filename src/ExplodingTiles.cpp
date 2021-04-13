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

class BoardView : public sf::Drawable {
	sf::Vertex outer[3];
	sf::Shader shader;

	sf::Vector2f inner[3];

	const Board& b;

	sf::CircleShape explode;


public:
	sf::Transform show_player;
	sf::CircleShape players[2];
	TriCoord selected{};
	float explosion_progress;

	BoardView(sf::Vector2f center, float radius, const Board& board) : b(board) {
		int hex_size = b.size();
		float r = radius * (hex_size + 1) / hex_size; //Widen for outer edge
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

		shader.setUniform("hex_size", hex_size);
		shader.setUniform("selected", sf::Vector3i());

		float size = radius / (hex_size * 6 + 3);

		players[0] = sf::CircleShape(size, 3);
		players[0].setFillColor(sf::Color::Green);
		players[0].setOrigin(size, size);
		players[0].setOutlineThickness(1.f);
		players[0].setOutlineColor(sf::Color::Black);

		players[1] = players[0];
		players[1].setFillColor(sf::Color::Red);
		players[1].setPointCount(5);

		explode = sf::CircleShape(size * 3);
		explode.setFillColor(sf::Color::Yellow);
		explode.setOutlineColor(sf::Color::Red);
		explode.setOutlineThickness(-size / 2);
		explode.setOrigin(size * 3, size * 3);

		show_player = sf::Transform().translate(center - sf::Vector2f(radius, radius)*(7.f/8)).scale(sf::Vector2f(1 / size, 1 / size) * (radius / 8));
	}

	void updatePos(sf::Vector2f mouse) {
		float length = inner[1].y - inner[0].y;
		float v1 = 1 - dot(mouse - inner[0], sf::Vector2f(0, 1)) / length;
		float v2 = 1 - dot(mouse - inner[1], inner[2] + (inner[0] - inner[2]) / 2.f - inner[1]) / (length * length);

		sf::Vector3i bary = sf::Vector3i(3.f * b.size() * sf::Vector3f(v1, v2, 1 - v1 - v2));

		//ensure out of bounds coordinate when a coordinate < 0, converting to int != floor. Subtract one if a coordinate was below 0
		bary -= sf::Vector3i(v1 < 0, v2 < 0, v1 + v2 > 1);

		selected = TriCoord(bary, b.size());
		shader.setUniform("selected", bary);
	}

	void draw(sf::RenderTarget& target, sf::RenderStates states) const override {
		target.draw(outer, 3, sf::PrimitiveType::Triangles, { states.blendMode, states.transform, states.texture, &shader });

		auto draw_tile = [this, &target](TriCoord c, sf::RenderStates states) {
			auto s = b[c];
			if (s.num == 0) return;

			auto [x, y, z] = c.tri_center(b.size());

			auto center = x * inner[0] + y * inner[1] + z * inner[2];
			
			states.transform.translate(center);
			
			const sf::CircleShape& circle = players[s.player];

			if (s.num > b.allowedPieces(c)) {
				auto explode_state = states;
				float scale = lerp(0.3f, 1.0f, explosion_progress);
				explode_state.transform.scale(scale,scale);
				target.draw(explode, explode_state);
				for (const auto& n : c.neighbors()) {
					if (b.inBounds(n)) {
						auto [x2, y2, z2] = n.tri_center(b.size());
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

		b.iterTiles([&](auto c) {draw_tile(c, states); });
	}
};

constexpr float explosion_length = 0.5f;

class Game : public sf::Drawable {
	Board b;
	BoardView board;
	sf::Clock t;
	int current_player = 0;
	std::vector<std::unique_ptr<Player>> players;

	void makeMove(TriCoord c) {
		if (!b.incTile(c, current_player)) return;
		if (b.needsUpdate())
			t.restart();
		else {
			nextPlayer();
		}
	}

	void nextPlayer() {
		current_player = (current_player + 1) % players.size();
		players[current_player]->startTurn(b,current_player);
	}

public:
	Game(int size, std::vector<std::unique_ptr<Player>> players) : b(size), board({ 400,300 }, 250, b), players(std::move(players)) {}

	void update() {
		if (b.needsUpdate()) {
			if (t.getElapsedTime().asSeconds() > explosion_length) {
				t.restart();
				b.update_step();
				if (!b.needsUpdate()) nextPlayer();
			}
			board.explosion_progress = t.getElapsedTime().asSeconds() / explosion_length;
		}
		else if (!players[current_player]->isMouseControlled()) {
			if (auto m = players[current_player]->update(); m) {
				makeMove(*m);
			}
		}
	}

	void onMouseMove(sf::Vector2f mouse_pos) {
		board.updatePos(mouse_pos);
	}

	void onClick() {
		if (!b.needsUpdate() && players[current_player]->isMouseControlled()) {
			makeMove(board.selected);
		}
	}

	void draw(sf::RenderTarget& target, sf::RenderStates states) const override {
		target.draw(board, states);
		target.draw(board.players[current_player], sf::RenderStates(board.show_player));
	}
};

int main()
{
	std::random_device random;

	sf::RenderWindow window(sf::VideoMode(800, 600), "Exploding Tiles");

	window.setFramerateLimit(60);

	std::vector<std::unique_ptr<Player>> players;
	players.push_back(std::make_unique<MousePlayer>());
	players.push_back(std::make_unique<RandomAI>(random));

	Game g(3, std::move(players));

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
				g.onClick();
				break;
			case sf::Event::MouseMoved:
				g.onMouseMove(sf::Vector2f{ sf::Mouse::getPosition(window) });
				break;
			}
		}

		g.update();

		window.clear(sf::Color::Black);

		window.draw(g);

		window.display();
	}

	return 0;
}
