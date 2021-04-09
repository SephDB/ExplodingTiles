// ExplodingTiles.cpp : Defines the entry point for the application.
//
#include <string_view>
#include <cmath>
#include <SFML/Window.hpp>
#include <SFML/Graphics.hpp>

#include "coords.hpp"

std::string_view shader = R"(
const int hex_size = 5;
const float edge_thickness = 0.06;
const vec3 edge_color = vec3(1);
const vec3 highlight_color = vec3(1,0,1);
uniform vec2 mouse; //first two barycentric coordinates

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

	ivec3 selected = floor(vec3(mouse,(1-mouse.x-mouse.y)) * (hex_size+1)*3);
	vec3 coordinates = (gl_Color.rgb) * (hex_size+1) * 3;
	ivec3 coords = floor(coordinates);
	
	
	if(all(greaterThan(coordinates,min_bound)) && all(lessThan(coordinates,max_bound))) {
		//Inner edge
		vec3 distance = min(fract(coordinates),ceil(coordinates)-coordinates);
		if(selected == floor(coordinates)) {
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

int main()
{
	sf::RenderWindow window(sf::VideoMode(800, 600), "Exploding Tiles");

	window.setFramerateLimit(60);

	sf::VertexArray t(sf::PrimitiveType::Triangles, 0);
	const int size = 900;
	const float bary_length = size / 2 * static_cast<float>(std::sqrt(3));
	const int bottom = 550;
	const int hex_size = 5;
	t.append(sf::Vertex(sf::Vector2f(400, bottom - bary_length), sf::Color::Red));
	t.append(sf::Vertex(sf::Vector2f(400 + size / 2, bottom), sf::Color::Green));
	t.append(sf::Vertex(sf::Vector2f(400 - size / 2, bottom), sf::Color::Blue));

	TriCoord c{ 5,5,false };

	sf::Shader s;
	{
		sf::MemoryInputStream stream;
		stream.open(shader.data(), shader.size());
		s.loadFromStream(stream, sf::Shader::Type::Fragment);
	}
	sf::RenderStates render(&s);

	// run the program as long as the window is open
	while (window.isOpen())
	{
		sf::Event event;
		while (window.pollEvent(event))
		{
			if (event.type == sf::Event::Closed)
				window.close();
			else if (event.type == sf::Event::KeyPressed) {
				auto new_coords = neighbors(c);
				if (event.key.code == sf::Keyboard::Num1)
					c = new_coords[0];
				else if (event.key.code == sf::Keyboard::Num2)
					c = new_coords[1];
				else if (event.key.code == sf::Keyboard::Num3)
					c = new_coords[2];
			}
		}

		window.clear(sf::Color::Black);




		//float v1 = 1 - dot(sf::Vector2f(sf::Mouse::getPosition(window)) - t[0].position, sf::Vector2f(0, 1)) / bary_length;
		//float v2 = 1 - dot(sf::Vector2f(sf::Mouse::getPosition(window)) - t[1].position, t[2].position + (t[0].position - t[2].position) / 2.f - t[1].position) / (bary_length * bary_length);

		s.setUniform("mouse", to_bary<sf::Vector2f>(c)/((hex_size+1)*3.f));
		window.draw(t, render);

		window.display();
	}

	return 0;
}
