// ExplodingTiles.cpp : Defines the entry point for the application.
//
#include <string_view>
#include <iostream>
#include <cmath>
#include <SFML/Window.hpp>
#include <SFML/Graphics.hpp>

#include "coords.hpp"
#include "board.hpp"

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

class BoardView : public sf::Drawable {
	sf::Vertex outer[3];
	sf::Shader shader;

	sf::Vector2f inner[3];

	const Board& b;

	sf::CircleShape players[2];

public:
	TriCoord selected{};

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
	}

	void updatePos(sf::Vector2f mouse) {
		float length = inner[1].y - inner[0].y;
		float v1 = 1 - dot(mouse - inner[0], sf::Vector2f(0,1)) / length;
		float v2 = 1 - dot(mouse - inner[1], inner[2] + (inner[0] - inner[2]) / 2.f - inner[1]) / (length * length);
		
		sf::Vector3i bary = sf::Vector3i(3.f * b.size() * sf::Vector3f(v1, v2, 1-v1-v2));
		
		//ensure out of bounds coordinate when a coordinate < 0, converting to int != floor. Subtract one if a coordinate was below 0
		bary -= sf::Vector3i(v1 < 0, v2 < 0, v1 + v2 > 1);

		selected = TriCoord(bary, b.size());
		shader.setUniform("selected", bary);
	}

	void draw(sf::RenderTarget& target, sf::RenderStates states) const override {
		target.draw(outer, 3, sf::PrimitiveType::Triangles, { states.blendMode, states.transform, states.texture, &shader });
		
		auto draw_tile = [this, &target](TriCoord c, sf::RenderStates states) {
			if (!b.in_bounds(c)) return;
			auto s = b[c];
			if (s.state == TileState::State::NONE) return;

			const sf::CircleShape& circle = players[s.player];
			
			auto [x,y,z] = c.tri_center(b.size());

			sf::Vector2f offset{};

			if (s.state == TileState::State::TWO && c.R) {
				offset = -circle.getOrigin() / 2.f;
			}

			states.transform.translate(x*inner[0] + y*inner[1]+z*inner[2] + offset);
			target.draw(circle, states);

			if (s.state == TileState::State::TWO) {
				states.transform.translate(circle.getOrigin() * 2.f / 3.f);
				target.draw(circle, states);
			}
		};
		
		for (int x = 0; x < b.size() * 2; ++x) {
			for (int y = 0; y < b.size() * 2; ++y) {
				draw_tile({ x,y,false },states);
				draw_tile({ x,y,true },states);
			}
		}
	}
};

int main()
{
	sf::RenderWindow window(sf::VideoMode(800, 600), "Exploding Tiles");

	window.setFramerateLimit(60);

	Board b(5);

	BoardView board({ 400,300 }, 250, b);

	int current_player = 0;

	// run the program as long as the window is open
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
				if (b.incTile(board.selected, current_player)) {
					current_player = 1 - current_player; //switch between 0 and 1
				}
			}
		}

		board.updatePos(sf::Vector2f{ sf::Mouse::getPosition(window) });

		window.clear(sf::Color::Black);

		window.draw(board);

		window.display();
	}

	return 0;
}
