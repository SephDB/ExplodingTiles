#pragma once

#include <cmath>
#include <SFML/Graphics/VertexArray.hpp>

#include "bezier.hpp"

sf::VertexArray circArrow(sf::Vector2f center, sf::Color color, float inner, float outer, float pointextra, int num = 50) {
	const float tau = 2 * std::acosf(-1);
	
	sf::VertexArray ret(sf::PrimitiveType::TriangleStrip,num*2+3);

	auto getDir = [&](auto i) {
		float angle = i * tau / (num * 1.3f) - tau / 3;
		return sf::Vector2f{ -std::cos(angle),std::sin(angle) };
	};

	for (int i = 0; i < num; ++i) {
		auto dir = getDir(i);
		ret[i * 2] = { center + dir * inner, color };
		ret[i * 2 + 1] = { center + dir * outer, color };
	}

	auto dir = getDir(num - 1);
	ret[num * 2] = { center + dir * (inner - pointextra), color };
	ret[num * 2 + 1] = { center + dir * (outer + pointextra), color };
	auto extra_dir = sf::Transform().rotate(-90).transformPoint(dir);
	ret[num * 2 + 2] = { center + dir * (inner + outer) * 0.5f + extra_dir*inner*0.7f, color };

	return ret;
}

sf::CircleShape playerShape(int n, sf::Color color, float size = 10) {
	auto ret = sf::CircleShape(size, n);
	ret.setFillColor(color);
	ret.setOrigin(size, size);
	ret.setOutlineThickness(1.f);
	ret.setOutlineColor(sf::Color::Black);
	return ret;
}

class CrossShape : public sf::Drawable, public sf::Transformable {
	sf::RectangleShape line;
public:
	CrossShape(sf::Color c, float size) : line(sf::Vector2f(size, size / 5)) {
		line.setOrigin(line.getSize()/2.f);
		line.setFillColor(c);
	}

	sf::FloatRect getBounds() const {
		auto d = line;
		d.setSize({ line.getSize().x, line.getSize().x });
		d.setOrigin(line.getSize().x / 2, line.getSize().x / 2);
		return getTransform().transformRect(d.getGlobalBounds());
	}

	void setColor(sf::Color c) {
		line.setFillColor(c);
	}

	void draw(sf::RenderTarget& target, sf::RenderStates states) const override {
		states.transform *= getTransform();
		target.draw(line, states);
		states.transform.rotate(90);
		target.draw(line, states);
	}
};

class HumanPlayer : public sf::Drawable, public sf::Transformable {
	sf::RectangleShape body;
	sf::CircleShape head;
	float size;
public:
	HumanPlayer(float size) : body(sf::Vector2f(size * 0.4f, size * 0.7f)), head(size * 0.12f), size(size) {
		body.setPosition(sf::Vector2f(size/2-body.getSize().x/2,size - body.getSize().y));
		body.setFillColor(sf::Color(128, 128, 128));
		head.setPosition(size/2-head.getRadius(),0);
		head.setFillColor(body.getFillColor());
	}

	sf::FloatRect getBounds() const {
		return getTransform().transformRect(sf::FloatRect(body.getPosition().x,0,body.getSize().x,size));
	}

	void draw(sf::RenderTarget& target, sf::RenderStates states) const override {
		states.transform *= getTransform();
		target.draw(head, states);
		target.draw(body, states);
	}
};

class AIPlayerShape : public sf::Drawable, public sf::Transformable {
	sf::RectangleShape body;
	sf::RectangleShape stand;
public:
	AIPlayerShape(float size) : body(sf::Vector2f(size, size*0.7f)), stand(sf::Vector2f(size,size*0.1f)) {
		body.setFillColor(sf::Color::Black);
		body.setOutlineColor(sf::Color::White);
		body.setOutlineThickness(-size * 0.15f);
		stand.setPosition(0,size*0.9f);
		stand.setFillColor(sf::Color::White);
	}

	sf::FloatRect getBounds() const {
		return getTransform().transformRect(sf::FloatRect(body.getPosition().x, 0, body.getSize().x, stand.getPosition().y/0.9f));
	}

	void draw(sf::RenderTarget& target, sf::RenderStates states) const override {
		states.transform *= getTransform();
		target.draw(stand, states);
		target.draw(body, states);
	}
};

class StarShape : public sf::Shape {
	float inner = 0, outer = 0;
	std::size_t num_points = 0;
public:
	StarShape() = default;

	StarShape(float inner_radius, float outer_radius, std::size_t points) : inner(inner_radius), outer(outer_radius), num_points(points) {
		update();
	}

	std::size_t getPointCount() const override {
		return num_points * 2;
	}

	sf::Vector2f getPoint(std::size_t index) const override {
		const float tau = 2 * std::acosf(-1);
		const float increment = tau / (num_points*2);
		const float angle = increment * index - tau/4;
		sf::Vector2f ret{ std::cos(angle),std::sin(angle) };
		ret *= (index % 2 == 0) ? outer : inner;
		return ret;
	}
};

class QuestionMark : public sf::Drawable, public sf::Transformable {
	sf::VertexArray arc;
	sf::Transform arc_offset;
	sf::CircleShape dot;
public:
	QuestionMark(float size) : dot(size / 10) {
		float bottom_height = size / 2;
		float top_height = size / 3;
		Bezier<3> curve{ {sf::Vector2f(0,-bottom_height/2),sf::Vector2f(bottom_height/2,-bottom_height/2),sf::Vector2f(bottom_height/2,-bottom_height)} };
		Bezier<3> curve2{ {sf::Vector2f(0,top_height), sf::Vector2f(bottom_height,top_height), sf::Vector2f(bottom_height,0)} };

		arc = curve_to_strip(PolyBezier(curve).addSpline(curve2), size / 10);

		auto arc_loc = arc.getBounds();

		arc_offset = sf::Transform().translate(-arc_loc.left,-arc_loc.top);

		dot.setPosition(bottom_height/2, arc_loc.height + dot.getRadius() * 1.2f);
	}

	sf::FloatRect getBounds() const {
		auto rect = arc.getBounds();
		rect.height += dot.getRadius() * 3;
		return (getTransform() * arc_offset).transformRect(rect);
	}

	void draw(sf::RenderTarget& target, sf::RenderStates states) const override {
		states.transform *= getTransform();
		target.draw(dot, states);
		states.transform *= arc_offset;
		target.draw(arc, states);
	}
};