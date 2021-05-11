#pragma once

#include <cmath>
#include <SFML/Graphics/VertexArray.hpp>


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