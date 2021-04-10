// ExplodingTiles.cpp : Defines the entry point for the application.
//
#include <string_view>
#include <iostream>
#include <cmath>
#include <SFML/Window.hpp>
#include <SFML/Graphics.hpp>

#include "coords.hpp"

std::string_view shader = R"(
const float edge_thickness = 0.06;
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

	sf::Shader s;
	{
		sf::MemoryInputStream stream;
		stream.open(shader.data(), shader.size());
		s.loadFromStream(stream, sf::Shader::Type::Fragment);
	}

	s.setUniform("hex_size", hex_size);
	sf::RenderStates render(&s);

	// run the program as long as the window is open
	while (window.isOpen())
	{
		sf::Event event;
		while (window.pollEvent(event))
		{
			if (event.type == sf::Event::Closed)
				window.close();
		}

		window.clear(sf::Color::Black);

		float v1 = 1 - dot(sf::Vector2f(sf::Mouse::getPosition(window)) - t[0].position, sf::Vector2f(0, 1)) / bary_length;
		float v2 = 1 - dot(sf::Vector2f(sf::Mouse::getPosition(window)) - t[1].position, t[2].position + (t[0].position - t[2].position) / 2.f - t[1].position) / (bary_length * bary_length);
		float v3 = 1 - v1 - v2;

		TriCoord coord(sf::Vector3i(sf::Vector3f(v1,v2,v3)*((hex_size+1)*3.f)) - sf::Vector3i(1,1,1), hex_size);

		s.setUniform("selected", coord.bary(hex_size) + sf::Vector3i(1,1,1));
		window.draw(t, render);

		window.display();
	}

	return 0;
}
